"""Production evidence query checks, including the retained R023 native trace."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
from cockpit_evidence import query, record_artifact
from evidence_display import DEFAULT_BYTES, recover
from gameplay_display import _plain_decision_output


ROOT = Path(__file__).resolve().parents[2]
CLI = Path(__file__).with_name("play_cli.py")
RETAINED = (ROOT / ".userdata/first-smoke-023-r011-raid-actor-trace-20260928"
            / "harness_runs/20260928_185227_68b23b603b5a488096c46f9cf0d4e509"
            / "semantic.native.events.jsonl")
RUN = "43156219029964be0895d5ca55562399b3e151b1b7a56ef1336b45ae33def7ee"
OPERATION = "overmap_special:cannibal_camp@135,137,0#hostile:2"


class DecisionTraceViewTest(unittest.TestCase):
    def test_selected_combat_sleep_and_death_edges_have_raw_handles(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "combat.jsonl"
            rows = [
                {"event": "raid_actor_damage", "run_id": "run", "game_turn": 40,
                 "trace_group_id": "roof", "selected_source_npc_id": 10,
                 "selected_victim_npc_id": None, "source": {"type": "npc", "id": 10,
                 "position_abs": [1, 2, 0]}, "victim": {"type": "monster", "id": None,
                 "monster_type": "mon_zombie", "position_abs": [2, 2, 0]},
                 "damage_kind": "melee_hit", "hp_before": 20, "hp_after": 12,
                 "applied_damage": 8},
                {"event": "raid_actor_sleep", "run_id": "run", "game_turn": 41,
                 "trace_group_id": "roof", "npc_id": 10, "edge": "fall_asleep",
                 "actor": {"type": "npc", "id": 10, "position_abs": [1, 2, 0]},
                 "cause": None, "narcosis": False, "sleepiness": 800},
                {"event": "raid_actor_damage", "run_id": "run", "game_turn": 42,
                 "trace_group_id": "roof", "selected_source_npc_id": None,
                 "selected_victim_npc_id": 11, "source": None,
                 "victim": {"type": "npc", "id": 11, "position_abs": [3, 2, 0]},
                 "damage_kind": "unspecified_direct", "hp_before": 10, "hp_after": 7,
                 "applied_damage": 3},
                {"event": "raid_actor_death", "run_id": "run", "game_turn": 43,
                 "trace_group_id": "roof", "selected_killer_npc_id": None,
                 "selected_victim_npc_id": 11, "killer": None,
                 "victim": {"type": "npc", "id": 11, "position_abs": [3, 2, 0]},
                 "confirmed": True},
            ]
            path.write_bytes(b"".join(json.dumps(row, separators=(",", ":")).encode() + b"\n"
                                      for row in rows))
            scope = {"operation_id": None, "group_id": "roof", "actor_ids": [10],
                     "from_turn": 40, "to_turn": 43}
            result = query([path], {"run_id": "run"}, [], 0, 10, decision=scope)
            self.assertEqual([row["record"]["kind"] for row in result["rows"]],
                             ["damage", "sleep"])
            self.assertIn("DAMAGE melee_hit", _plain_decision_output(result))
            self.assertIn("cause=unknown", _plain_decision_output(result))
            for row in result["rows"]:
                handle = row["artifact"]
                self.assertTrue(record_artifact(Path(handle["path"]), handle["offset"],
                                                handle["length"], handle["sha256"], [])["ok"])
            other = query([path], {"run_id": "run"}, [], 0, 10,
                          decision={**scope, "actor_ids": [11]})
            self.assertEqual([row["record"]["kind"] for row in other["rows"]],
                             ["damage", "death"])
            self.assertIn("killer=unknown#None", _plain_decision_output(other))

    def test_scenario_group_query_has_scope_real_roles_overlap_and_raw_handles(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "selected.jsonl"
            records = [
                {"event": "raid_trace_scope", "run_id": "run", "game_turn": 1,
                 "trace_group_id": "signal-off-camp", "selected_npc_ids": [2, 5, 7],
                 "capture_from_turn": 10, "capture_to_turn": 20,
                 "selection": "scenario_numeric_ids"},
                {"event": "raid_actor_action", "run_id": "run", "game_turn": 10,
                 "trace_group_id": "signal-off-camp", "npc_id": 2, "npc_name": "Casey",
                 "actor_role": "camp_resident", "action": "Pause", "reason": "execute_action",
                 "attitude": 0, "mission": 11, "assigned_camp": True, "has_job": True,
                 "patrol_priority": 10, "patrol_order": True, "path_next_passable": True},
                {"event": "raid_actor_action", "run_id": "run", "game_turn": 11,
                 "trace_group_id": "signal-off-camp", "npc_id": 5, "npc_name": "Scout",
                 "actor_role": "cannibal_scout", "action": "Go to position",
                 "outing_member": True, "outing_kind": "structural_sortie",
                 "outing_phase": "outbound", "outing_owner": "local",
                 "outing_waypoint_index": 2},
                {"event": "raid_trace_repeat", "run_id": "run", "key": "scenario@signal-off-camp:5",
                 "of_event": "raid_actor_action", "count": 9, "first_turn": 12, "last_turn": 20},
                {"event": "raid_actor_action", "run_id": "run", "game_turn": 13,
                 "trace_group_id": "other-group", "npc_id": 7, "action": "Attack"},
                {"event": "raid_actor_action", "run_id": "run", "game_turn": 14,
                 "trace_group_id": "signal-off-camp", "operation_id": "hostile:2",
                 "generation": 1, "npc_id": 7, "npc_name": "Bandit", "action": "Attack"},
                {"event": "raid_trace_repeat", "run_id": "run", "key": "hostile:2#1:7",
                 "of_event": "raid_actor_action", "count": 2, "first_turn": 15, "last_turn": 16},
                {"event": "raid_actor_action", "run_id": "other-run", "game_turn": 13,
                 "trace_group_id": "signal-off-camp", "npc_id": 5, "action": "Attack"},
                {"event": "raid_trace_truncated", "run_id": "run", "row_budget": 6,
                 "decisions": 22, "written_rows": 6},
            ]
            path.write_bytes(b"".join(json.dumps(row, separators=(",", ":")).encode() + b"\n"
                                      for row in records))
            scope = {"operation_id": None, "group_id": "signal-off-camp", "actor_ids": [5],
                     "from_turn": 15, "to_turn": 15}
            result = query([path], {"run_id": "run"}, [], 0, 20, decision=scope)
            self.assertEqual(result["matched"], 3)  # scope, overlapping repeat, run-wide cap
            self.assertEqual(result["decision_trace"]["selected_npc_ids"], [2, 5, 7])
            self.assertEqual(result["decision_trace"]["selected_capture_window"],
                             {"from_turn": 10, "to_turn": 20})
            self.assertEqual(result["decision_trace"]["actors"],
                             {"2": "Casey", "5": "Scout", "7": "Bandit"})
            shown = _plain_decision_output(result)
            self.assertIn("group=signal-off-camp", shown)
            self.assertIn("Selected NPC IDs: [2, 5, 7]", shown)
            self.assertIn("Capture turns: 10..20", shown)
            self.assertIn("REPEAT 9", shown)
            self.assertIn("CAPTURE TRUNCATED", shown)
            handle = result["rows"][1]["artifact"]
            self.assertEqual(record_artifact(Path(handle["path"]), handle["offset"],
                                             handle["length"], handle["sha256"], [])["record"]["count"], 9)
            session = Path(directory) / "session"
            (session / "responses").mkdir(parents=True)
            (session / "bridge.manifest.json").write_text(json.dumps({"binding_id": "fixture"}))
            (session / "status.json").write_text(json.dumps({
                "binding_id": "fixture", "session_descriptor": {"run_id": "run"}}))
            (session / "game-process.json").write_text(json.dumps({
                "binding_id": "fixture", "run_id": "run",
                "log_paths": {"native_semantic_events": {"path": str(path), "scope": "run_bound"}}}))
            process = subprocess.run([sys.executable, str(CLI), "--session", str(session),
                                      "evidence", "--decisions", "--group-id", "signal-off-camp",
                                      "--actor", "5", "--from-turn", "15", "--to-turn", "15"],
                                     text=True, capture_output=True)
            self.assertEqual(process.returncode, 0, process.stderr)
            self.assertIn("REPEAT 9", process.stdout)
            self.assertIn("@", process.stdout)
            earlier = query([path], {"run_id": "run"}, [], 0, 20,
                            decision={**scope, "actor_ids": [], "from_turn": 10, "to_turn": 11})
            self.assertEqual([row["record"].get("actor_role") for row in earlier["rows"]
                              if row["record"]["kind"] == "actor"],
                             ["camp_resident", "cannibal_scout"])
            self.assertNotIn("#7 Attack", _plain_decision_output(earlier))
            hostile_group = query([path], {"run_id": "run"}, [], 0, 20,
                                  decision={**scope, "actor_ids": [7],
                                            "from_turn": 16, "to_turn": 16})
            self.assertTrue(any(row["record"].get("member") == "7" and
                                row["record"].get("of_event") == "raid_actor_action"
                                for row in hostile_group["rows"]))

    def test_oversized_scope_header_stays_within_plain_output_budget(self):
        result = {"decision_trace": {"run_id": "run", "operation_id": "x" * 9000,
                                     "actor_ids": [], "from_turn": None, "to_turn": None,
                                     "actors": {}, "captured_event_counts": {},
                                     "capture_truncation": []},
                  "page": {"offset": 0, "limit": 20, "next_offset": None},
                  "rows": [], "matched": 0, "snapshot": "a" * 64}
        shown = _plain_decision_output(result)
        self.assertLessEqual(len(shown.encode()), DEFAULT_BYTES)
        self.assertIn("Use --diagnostics", shown)

    def test_repeat_overlap_group_scope_truncation_projection_and_snapshot(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "trace.jsonl"
            records = [
                {"event": "raid_actor_action", "run_id": "run", "game_turn": 10,
                 "operation_id": OPERATION, "generation": 2, "npc_id": 8, "npc_name": "Lloyd",
                 "action": "Go to position", "attitude": 10, "mission": 0,
                 "reason": "execute_action", "path_next_passable": False, "panic": 0},
                {"event": "raid_actor_action", "run_id": "run", "game_turn": 10,
                 "operation_id": OPERATION, "npc_id": 9, "action": "Flee"},
                {"event": "raid_site_search", "run_id": "run", "game_turn": 15,
                 "operation_id": OPERATION, "generation": 2, "reason": "member_has_visible_enemy",
                 "trigger_actor_id": 9, "seen_target": {"type": "avatar", "id": 1},
                 "recipients": []},
                {"event": "raid_trace_repeat", "run_id": "run", "key": OPERATION + "#2:8",
                 "of_event": "raid_actor_action", "count": 10, "first_turn": 11, "last_turn": 20},
                {"event": "raid_trace_repeat", "run_id": "run", "key": OPERATION + "#2:search",
                 "of_event": "raid_site_search", "count": 4, "first_turn": 16, "last_turn": 19},
                {"event": "raid_trace_truncated", "run_id": "run", "row_budget": 5,
                 "decisions": 18, "written_rows": 5},
                {"event": "raid_actor_action", "run_id": "other-run", "game_turn": 15,
                 "operation_id": OPERATION, "npc_id": 8, "action": "Attack"},
            ]
            path.write_bytes(b"".join(json.dumps(row, separators=(",", ":")).encode() + b"\n"
                                      for row in records))
            original = hashlib.sha256(path.read_bytes()).hexdigest()
            decision = {"operation_id": OPERATION, "actor_ids": [8],
                        "from_turn": 15, "to_turn": 15}
            first = query([path], {"run_id": "run"}, [], 0, 2, decision=decision)
            self.assertEqual(first["matched"], 3)  # group, overlapping repeat, run-wide cap
            self.assertEqual(first["page"]["next_offset"], 2)
            self.assertEqual([row["record"]["kind"] for row in first["rows"]],
                             ["search", "repeat"])
            self.assertEqual(first["rows"][0]["record"]["trigger_actor_id"], 9)
            self.assertEqual(first["rows"][1]["record"]["first_turn"], 11)
            self.assertEqual(first["rows"][1]["record"]["of_event"], "raid_actor_action")
            self.assertFalse(first["rows"][1]["record"]["base_missing"])
            self.assertIsNotNone(first["rows"][1]["record"]["base_artifact"])
            self.assertEqual(first["decision_trace"]["capture_truncation"][0]["row_budget"], 5)
            second = query([path], {"run_id": "run"}, [], 2, 2, snapshot=first["snapshot"],
                           decision=decision)
            self.assertEqual(second["rows"][0]["record"]["kind"], "capture_truncated")
            handle = first["rows"][0]["artifact"]
            self.assertTrue(record_artifact(Path(handle["path"]), handle["offset"],
                                            handle["length"], handle["sha256"], [])["ok"])
            selected = query([path], {"run_id": "run"},
                             ["path_next_passable", "reason"], 0, 2,
                             decision={**decision, "from_turn": 10, "to_turn": 10})
            self.assertIs(selected["rows"][0]["record"]["path_next_passable"], False)
            self.assertEqual(selected["rows"][0]["record"]["reason"], "execute_action")
            self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(), original)
            path.write_bytes(path.read_bytes().replace(b"member_has_visible_enemy", b"member_has_visible_foe"))
            self.assertEqual(query([path], {"run_id": "run"}, [], 2, 2,
                                   snapshot=first["snapshot"], decision=decision)["error"],
                             "log_snapshot_source_changed")

    @unittest.skipUnless(RETAINED.is_file(), "retained R023 trace not installed")
    def test_retained_r023_play_evidence_recovers_actor_and_group_without_game_input(self):
        before = hashlib.sha256(RETAINED.read_bytes()).hexdigest()
        with tempfile.TemporaryDirectory() as directory:
            session = Path(directory)
            (session / "responses").mkdir()
            (session / "bridge.manifest.json").write_text(json.dumps({"binding_id": "fixture"}))
            (session / "status.json").write_text(json.dumps({
                "binding_id": "fixture", "session_descriptor": {"run_id": RUN}}))
            (session / "game-process.json").write_text(json.dumps({
                "binding_id": "fixture", "run_id": RUN,
                "log_paths": {"native_semantic_events": {"path": str(RETAINED),
                                                          "scope": "run_bound"}}}))

            def play(*args):
                command = [sys.executable, str(CLI), "--session", str(session), "evidence",
                           "--decisions", "--operation-id", OPERATION, "--actor", "8", *args]
                process = subprocess.run(command, text=True, capture_output=True)
                self.assertEqual(process.returncode, 0, process.stderr + process.stdout)
                self.assertLessEqual(len(process.stdout.encode()), DEFAULT_BYTES)
                return process.stdout

            group = play("--from-turn", "5256996", "--to-turn", "5256996")
            self.assertIn("member_has_visible_enemy", group)
            self.assertIn("trigger=#8", group)
            action = play("--from-turn", "5257010", "--to-turn", "5257019")
            self.assertIn("#8 Go to position", action)
            self.assertIn("#8 Attack", action)
            self.assertIn("panic=0 flee=no", action)
            self.assertIn("passable=yes", action)
            self.assertIn("raid_site_search=20", action)
            self.assertIn("REPEAT 23 × raid_site_search", action)  # span starts before the window
            self.assertIn("Final repeat tail unconfirmed", action)
            # Exact raw JSON and path/offset/length/hash remain retrievable
            # through the same generic diagnostics, not an inferred action.
            diagnostic = play("--from-turn", "5257010", "--to-turn", "5257019",
                              "--diagnostics")
            shown = json.loads(diagnostic)
            if "presentation" in shown:
                shown = recover(shown["presentation"]["full_evidence"]["sha256"])
            attacks = [row for row in shown["rows"]
                       if row["record"].get("actor_id") == 8 and row["record"].get("action") == "Attack"]
            self.assertTrue(attacks)
            self.assertTrue(all(row["record"]["panic"] == 0 for row in attacks))
            self.assertTrue(all(row["record"]["path_next_passable"] is True for row in attacks))
            handle = attacks[0]["artifact"]
            raw = record_artifact(Path(handle["path"]), handle["offset"],
                                  handle["length"], handle["sha256"], [])
            self.assertEqual(raw["record"]["action"], "Attack")
            projected = json.loads(play("--from-turn", "5257010", "--to-turn", "5257019",
                                        "--select", "path_next_passable,panic,of_event,attitude,mission",
                                        "--diagnostics"))
            if "presentation" in projected:
                projected = recover(projected["presentation"]["full_evidence"]["sha256"])
            self.assertTrue(any(row["record"]["path_next_passable"] is True
                                for row in projected["rows"] if row["record"].get("panic") == 0))
            self.assertTrue(any(row["record"].get("of_event") == "raid_site_search"
                                for row in projected["rows"]))
            first_page = json.loads(play("--from-turn", "5257010", "--to-turn", "5257019",
                                         "--limit", "2", "--diagnostics"))
            self.assertEqual(first_page["matched"], 11)
            self.assertEqual(first_page["page"]["next_offset"], 2)
            self.assertFalse(first_page["decision_trace"]["capture_truncation"])
            second_page = json.loads(play("--from-turn", "5257010", "--to-turn", "5257019",
                                          "--limit", "2", "--offset", "2",
                                          "--snapshot", first_page["snapshot"], "--diagnostics"))
            self.assertEqual(second_page["rows"][0]["record"]["turn"], 5257012)
            self.assertEqual(hashlib.sha256(RETAINED.read_bytes()).hexdigest(), before)
            self.assertFalse(list((session / "responses").iterdir()))

    def test_orphan_repeat_reports_lost_base_in_compact_view(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "trace.jsonl"
            path.write_text(json.dumps({"event": "raid_trace_repeat", "run_id": "run",
                                        "key": OPERATION + "#2:4",
                                        "of_event": "raid_actor_action", "count": 11248,
                                        "first_turn": 5256612, "last_turn": 5267859}) + "\n")
            decision = {"operation_id": OPERATION, "group_id": None, "actor_ids": [4],
                        "from_turn": 5267859, "to_turn": 5267859}
            result = query([path], {"run_id": "run"}, [], 0, 20, decision=decision)
            self.assertEqual(result["decision_trace"]["orphan_repeat_bases"], 1)
            self.assertTrue(result["rows"][0]["record"]["base_missing"])
            shown = _plain_decision_output(result)
            self.assertIn("base row missing from snapshot", shown)
            self.assertIn("Orphan repeat bases: 1", shown)


if __name__ == "__main__":
    unittest.main()
