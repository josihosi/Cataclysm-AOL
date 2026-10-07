import json
import hashlib
from pathlib import Path
import tempfile
import unittest

from harness_log_window import roll_bound_session_logs, roll_complete_lines
from cockpit_evidence import query, record_artifact


class HarnessLogWindowTest(unittest.TestCase):
    def test_complete_line_suffix_keeps_inode_and_future_appends(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "debug.log"
            path.write_bytes(b"old-a\nold-b\nrecent-a\nrecent-b\n")
            inode = path.stat().st_ino
            result = roll_complete_lines(path, maximum=20, keep=18)
            self.assertIsNotNone(result)
            self.assertEqual(path.stat().st_ino, inode)
            self.assertEqual(path.read_bytes(), b"recent-a\nrecent-b\n")
            with path.open("ab") as stream:
                stream.write(b"future\n")
            self.assertTrue(path.read_bytes().endswith(b"recent-b\nfuture\n"))

    def test_only_bound_noisy_logs_roll_and_receipts_survive(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            session = root / "session"
            session.mkdir()
            run = root / "run"
            run.mkdir()
            profile = root / "profile" / "config"
            profile.mkdir(parents=True)
            native = run / "semantic.native.events.jsonl"
            debug = profile / "debug.log"
            receipts = run / "semantic.steps.jsonl"
            for path in (native, debug, receipts):
                path.write_bytes(b"old-a\nold-b\nrecent-a\nrecent-b\n")
            process = {"run_id": "owned", "log_paths": {
                "native_semantic_events": {"path": str(native), "scope": "run_bound"},
                "profile_diagnostic_debug": {"path": str(debug), "scope": "profile_shared"},
            }}
            (session / "game-process.json").write_text(json.dumps(process))
            self.assertEqual(roll_bound_session_logs(session, "other", maximum=20, keep=18), [])
            self.assertEqual(len(roll_bound_session_logs(session, "owned", maximum=20,
                                                         keep=18)), 1)
            self.assertEqual(native.read_bytes(), b"old-a\nold-b\nrecent-a\nrecent-b\n")
            self.assertEqual(debug.read_bytes(), b"recent-a\nrecent-b\n")
            self.assertEqual(receipts.read_bytes(), b"old-a\nold-b\nrecent-a\nrecent-b\n")
            events = (session / "log-window.events.jsonl").read_text().splitlines()
            self.assertEqual(len(events), 1)

    def test_native_trace_keeps_raw_handles_across_responses_and_closeout(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            session = root / "session"
            session.mkdir()
            native = root / "semantic.native.events.jsonl"
            run_id = "run"
            operation = "camp#hostile:2"
            rows = [
                {"event": "raid_trace_scope", "run_id": run_id, "game_turn": 10,
                 "trace_group_id": "selected", "selected_npc_ids": [4]},
                {"event": "raid_actor_action", "run_id": run_id, "game_turn": 11,
                 "operation_id": operation, "generation": 2, "npc_id": 4,
                 "action": "Go to position"},
                {"event": "raid_site_search", "run_id": run_id, "game_turn": 12,
                 "operation_id": operation, "generation": 2,
                 "reason": "waiting_for_movement"},
            ]
            encoded = lambda row: json.dumps(row, separators=(",", ":")).encode() + b"\n"
            native.write_bytes(b"".join(encoded(row) for row in rows) + b"noise\n" * 450)
            (session / "game-process.json").write_text(json.dumps({
                "run_id": run_id, "log_paths": {"native_semantic_events": {
                    "path": str(native), "scope": "run_bound"}}}))
            inode = native.stat().st_ino
            original_bytes = native.read_bytes()
            self.assertEqual(roll_bound_session_logs(session, run_id, maximum=1800, keep=300), [])
            self.assertEqual(native.read_bytes(), original_bytes)
            self.assertEqual(native.stat().st_ino, inode)
            with native.open("ab") as stream:
                stream.write(b"noise\n" * 450)
                stream.write(encoded({"event": "raid_trace_repeat", "run_id": run_id,
                                      "key": operation + "#2:4", "of_event": "raid_actor_action",
                                      "count": 450, "first_turn": 13, "last_turn": 462}))
                stream.write(encoded({"event": "raid_trace_repeat", "run_id": run_id,
                                      "key": operation + "#2:search", "of_event": "raid_site_search",
                                      "count": 449, "first_turn": 14, "last_turn": 462}))
            appended_bytes = native.read_bytes()
            self.assertEqual(roll_bound_session_logs(session, run_id, maximum=1800, keep=300), [])
            self.assertEqual(native.read_bytes(), appended_bytes)
            self.assertEqual(native.stat().st_ino, inode)
            with native.open("ab") as stream:
                stream.write(encoded({"event": "raid_trace_truncated", "run_id": run_id,
                                      "row_budget": 1024, "decisions": 902,
                                      "written_rows": 1024}))
            decision = {"operation_id": operation, "group_id": None, "actor_ids": [4],
                        "from_turn": 462, "to_turn": 462}
            result = query([native], {"run_id": run_id}, [], 0, 20, decision=decision)
            self.assertEqual(result["decision_trace"]["orphan_repeat_bases"], 0)
            self.assertEqual([row["record"]["kind"] for row in result["rows"]],
                             ["repeat", "repeat", "capture_truncated"])
            trace_events = [json.loads(line)["event"] for line in native.read_bytes().splitlines()
                            if line.startswith(b"{\"event\":\"raid_")]
            self.assertEqual(set(trace_events),
                             {"raid_trace_scope", "raid_actor_action", "raid_site_search",
                              "raid_trace_repeat", "raid_trace_truncated"})
            self.assertEqual(len(trace_events), 6)
            for row in result["rows"]:
                handle = row["artifact"]
                self.assertEqual(record_artifact(Path(handle["path"]), handle["offset"],
                                                 handle["length"], handle["sha256"], [])["ok"], True)
                self.assertEqual(hashlib.sha256(native.read_bytes()[handle["offset"]:
                                handle["offset"] + handle["length"]]).hexdigest(), handle["sha256"])
            self.assertTrue(all(row["record"].get("base_artifact") for row in result["rows"][:2]))

    def test_trace_preservation_reports_when_its_byte_budget_cannot_fit_a_row(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "semantic.native.events.jsonl"
            path.write_bytes(json.dumps({"event": "raid_actor_action", "run_id": "run",
                                         "npc_id": 4, "action": "Pause"}).encode() +
                             b"\n" + b"noise\n" * 30)
            result = roll_complete_lines(path, maximum=120, keep=50,
                                         preserve_decisions=True)
            self.assertTrue(result["trace_preservation_incomplete"])
            self.assertEqual(result["unpreserved_trace_rows"], 1)
            self.assertLessEqual(result["after_bytes"], 120)

    def test_forced_rollover_keeps_combat_and_action_base_before_generic_trace(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "semantic.native.events.jsonl"
            encode = lambda row: json.dumps(row, separators=(",", ":")).encode() + b"\n"
            rows = [
                {"event": "raid_trace_scope", "run_id": "run", "game_turn": 10,
                 "trace_group_id": "roof", "selected_npc_ids": [10, 11]},
                {"event": "raid_actor_action", "run_id": "run", "game_turn": 11,
                 "trace_group_id": "roof", "npc_id": 10, "action": "Attack",
                 "position_abs": [3220, 3380, 0], "operation_phase": "committed_contact",
                 "operation_owner": "local", "route_waypoint_index": 4,
                 "target": {"type": "monster", "id": None,
                            "monster_type": "mon_zombie", "position_abs": [3221, 3380, 0]}},
                *({"event": "raid_site_search", "run_id": "run", "game_turn": 12 + index,
                   "reason": "generic-" + "x" * 70 + str(index)} for index in range(18)),
                {"event": "raid_trace_repeat", "run_id": "run", "key": "scenario@roof:10",
                 "of_event": "raid_actor_action", "base_turn": 11,
                 "first_turn": 12, "last_turn": 20, "count": 9},
                {"event": "raid_actor_damage", "run_id": "run", "game_turn": 21,
                 "trace_group_id": "roof", "selected_victim_npc_id": 10,
                 "hp_before": 9, "hp_after": 3, "applied_damage": 6},
                {"event": "raid_actor_death", "run_id": "run", "game_turn": 22,
                 "trace_group_id": "roof", "selected_victim_npc_id": 10,
                 "killer": None, "confirmed": True},
            ]
            path.write_bytes(b"".join(map(encode, rows)) + b"noise\n" * 200)
            result = roll_complete_lines(path, maximum=1500, keep=250,
                                         preserve_decisions=True, trace_run_id="run")
            self.assertTrue(result["trace_preservation_incomplete"])
            self.assertTrue(result["trace_truncation_marker_written"])
            kept = [json.loads(line) for line in path.read_bytes().splitlines()
                    if line.startswith(b"{\"event\":\"raid_")]
            events = {row["event"] for row in kept}
            self.assertTrue({"raid_trace_scope", "raid_actor_action", "raid_trace_repeat",
                             "raid_actor_damage", "raid_actor_death",
                             "raid_trace_truncated"}.issubset(events))
            marker = next(row for row in kept if row["event"] == "raid_trace_truncated")
            self.assertEqual(marker["scope"], "window")
            self.assertGreater(marker["unpreserved_rows"], 0)
            self.assertLessEqual(result["after_bytes"], 1500)
            interval = query([path], {"run_id": "run"}, [], 0, 20,
                             decision={"operation_id": None, "group_id": "roof",
                                       "actor_ids": [10], "from_turn": 11,
                                       "to_turn": 22})
            self.assertEqual(interval["decision_trace"]["orphan_repeat_bases"], 0)
            self.assertTrue({"actor", "repeat", "damage", "death", "capture_truncated"}
                            .issubset({row["record"]["kind"] for row in interval["rows"]}))
            actor = next(row["record"] for row in interval["rows"]
                         if row["record"]["kind"] == "actor")
            self.assertEqual((actor["position_abs"], actor["operation_owner"],
                              actor["route_waypoint_index"]), ([3220, 3380, 0], "local", 4))
            for row in interval["rows"]:
                handle = row["artifact"]
                self.assertTrue(record_artifact(Path(handle["path"]), handle["offset"],
                                                handle["length"], handle["sha256"], [])["ok"])


if __name__ == "__main__":
    unittest.main()
