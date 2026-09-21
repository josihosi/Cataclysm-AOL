"""Exercise the public player CLI against retained bridge traffic, without a game."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parent))
from cockpit_archive import Archive
from play_cli import PlayerClient, session_lock

CLI = Path(__file__).with_name("play_cli.py")


class PlayerCliTest(unittest.TestCase):
    def test_short_wait_preserves_existing_request(self):
        pending = self.cli("wait", "5m")
        sent = next(row for row in self.requests() if row["request_id"] == pending["request_id"])
        self.assertEqual(sent["request"], {"action": "game.wait", "wait": {
            "enabled": True, "recipe": ["world.wait", "wait.5m"],
            "target_delta_game_minutes": 5.0, "danger_handling": "ignore_danger_and_interruptions",
            "bound": {"basis": "game_mechanic", "source": "Player requested 5m",
                      "unit": "game_minutes", "maximum": 5.0, "progress_required": True}}})

    def test_plain_observation_preserves_facts_and_action(self):
        from gameplay_display import plain_player_output
        text = plain_player_output({"ok": True, "response": {"current_input": {
            "owner": "world", "actions": [{"id": "world.wait", "enabled": True}],
            "facts_changed": {"terrain": "forest", "weather": "rain"}}}})
        self.assertIn("forest", text)
        self.assertIn("rain", text)
        self.assertIn("play act world.wait", text)
        self.assertNotIn("Start waiting", text)
        self.assertNotIn("{", text)

    def test_short_yes_uses_existing_advertised_target(self):
        pending = self.cli("look")
        self.reply(pending["request_id"], {"ok": True, "result": {
            "observation_id": "prompt-frame", "run_id": "run-a", "surface": {
                "kind": "prompt", "facts": {"text": "Stop waiting?"},
                "actions": [{"id": "prompt.choose", "stable_id": "prompt-option:1",
                             "label": "YES", "enabled": True}]}}})
        self.cli("collect")
        chosen = self.cli("yes")
        sent = next(row for row in self.requests() if row["request_id"] == chosen["request_id"])
        self.assertEqual(sent["request"]["action"], "game.act")
        self.assertEqual(sent["request"]["action_id"], "prompt.choose")
        self.assertEqual(sent["request"]["stable_id"], "prompt-option:1")

    def test_plain_wait_keeps_progress_messages_and_alarm(self):
        from gameplay_display import plain_player_output
        text = plain_player_output({"ok": True, "response": {"outcome": {"chain": {
            "start_game_minutes": 10, "terminal_game_minutes": 15, "stop_reason": "target_reached",
            "model_round_trips": 1}}, "facts_changed": {"messages": {
                "new_events": [{"value": {"time": "12:05", "text": "You finish waiting."}}]}}},
            "turn_assessment": {"alarms": [{"kind": "waiting_slow", "mean_seconds": .02,
                "message": "Tell the coordinator: waiting performance needs attention."}]}})
        self.assertIn("Waited 5 game minutes. target reached.", text)
        self.assertIn("12:05: You finish waiting.", text)
        self.assertIn("Tell the coordinator", text)
        self.assertNotIn("round trips", text)

    def test_plain_quit_keeps_cleanup_failure_visible(self):
        from gameplay_display import plain_player_output
        text = plain_player_output({"ok": True, "result": {"state": "finished",
            "cleanup": {"status": "failed", "pid": 1234}}})
        self.assertIn("Cleanup: failed", text)
        self.assertNotIn("1234", text)

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.session = Path(self.temp.name)
        (self.session / "requests").mkdir()
        (self.session / "responses").mkdir()
        self.write("bridge.manifest.json", {"binding_id": "bound-a"})
        self.write("status.json", {"binding_id": "bound-a", "state": "ready"})

    def write(self, name, value):
        (self.session / name).write_text(json.dumps(value))

    def cli(self, *arguments, ok=True, wait_seconds=0):
        process = subprocess.run([sys.executable, str(CLI), "--diagnostics", "--session", str(self.session),
                                  "--wait-seconds", str(wait_seconds), *arguments], capture_output=True, text=True)
        self.assertEqual(process.returncode, 0 if ok else 1, process.stderr + process.stdout)
        from evidence_display import DEFAULT_BYTES, recover
        self.assertLessEqual(len(process.stdout.encode()), DEFAULT_BYTES)
        shown = json.loads(process.stdout)
        # The supported CLI now presents handles; verify the complete retained
        # result as well as the serialized boundary instead of demanding a dump.
        return recover(shown["presentation"]["full_evidence"]["sha256"])

    def cli_async(self, *arguments, wait_seconds):
        return subprocess.Popen([sys.executable, str(CLI), "--diagnostics", "--session", str(self.session),
                                 "--wait-seconds", str(wait_seconds), *arguments],
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    def requests(self):
        return [json.loads(p.read_text()) for p in (self.session / "requests").glob("*.json")]

    def reply(self, request_id, value, binding="bound-a"):
        raw = json.dumps(value).encode()
        self.reply_raw(request_id, raw, binding=binding)

    def reply_raw(self, request_id, raw, binding="bound-a"):
        path = "responses/" + request_id + ".json"
        (self.session / path).write_bytes(bytes(raw))
        session_generation = json.loads((self.session / "status.json").read_text()).get("session_generation", 0)
        self.write("responses/" + request_id + ".receipt.json", {
            "request_id": request_id, "binding_id": binding,
            "session_generation": session_generation,
            "response_sha256": hashlib.sha256(raw).hexdigest(), "response_artifact": path,
        })

    def observe(self, frame="frame-1"):
        pending = self.cli("look")
        self.reply(pending["request_id"], {"ok": True, "result": {
            "observation_id": frame, "run_id": "run-a",
            "surface": {"kind": "world", "facts": {"last_save_result": "unattempted"},
                        "actions": [{"id": "world.wait", "enabled": True}]},
        }})
        self.cli("collect")
        return pending["request_id"]

    def test_cancel_is_out_of_band_and_preserves_pending_identity(self):
        self.write("play-client.json", {"binding_id": "bound-a", "pending": {
            "request_id": "pending-1", "request": {"action": "game.wait"}}})
        self.write("status.json", {"binding_id": "bound-a", "state": "awaiting_response",
                                    "inflight_request_id": "pending-1"})
        self.write("active-request.json", {"request_id": "pending-1", "binding_id": "bound-a",
                                            "run_id": "run-a"})
        (self.session / "controls").mkdir()
        with session_lock(self.session / "play-client.lock"):
            result = self.cli("cancel", "--reason", "user stopped waiting")
        self.assertTrue(result["ok"])
        self.assertEqual(result["request_id"], "pending-1")
        self.assertEqual(json.loads((self.session / "play-client.json").read_text())["pending"]["request_id"], "pending-1")
        markers = list((self.session / "controls").glob("cancel-*.json"))
        self.assertEqual(len(markers), 1)
        marker = json.loads(markers[0].read_text())
        self.assertEqual(marker["request_id"], "pending-1")
        self.assertEqual(marker["binding_id"], "bound-a")
        self.assertEqual(marker["run_id"], "run-a")

    def test_cancel_collection_revokes_old_frame_and_preserves_unknown_outcome(self):
        self.observe()
        pending = self.cli("act", "world.wait")
        self.reply(pending["request_id"], {"ok": False, "error": "player_cancelled",
            "failure": {"unused_authority": "revoked", "detail": {"action_outcome": "unknown"}}})
        result = self.cli("collect", ok=False)
        self.assertEqual(result["next"], "look")
        state = json.loads((self.session / "play-client.json").read_text())
        self.assertNotIn("pending", state)
        self.assertNotIn("observation_id", state)
        self.assertFalse(state.get("finished", False))
        self.assertEqual(state["cancellation"]["action_outcome"], "unknown")
        self.assertEqual(self.cli("look")["state"], "pending")

    def test_performance_is_readable_under_pending_request_lock(self):
        self.cli("look")
        before = (self.session / "play-client.json").read_bytes()
        self.write("performance.latest.json", {"record_id": "p1", "owner": {"binding_id": "bound-a"}})
        with session_lock(self.session / "play-client.lock"):
            result = self.cli("performance")
        self.assertEqual(result["latest"]["record_id"], "p1")
        self.assertEqual(result["comparison"]["status"], "unavailable")
        self.assertEqual((self.session / "play-client.json").read_bytes(), before)
        self.assertEqual(len(self.requests()), 1)

    def test_performance_samples_actual_owned_process_under_request_lock(self):
        import os
        from r009_technical_witness import sample_child_resources
        initial = sample_child_resources(os.getpid())
        owner = {"pid": os.getpid(), "run_id": "test-process", "binding_id": "bound-a",
                 "process_identity": initial["process_identity"], "platform": initial["platform"]}
        self.write("performance-owner.json", owner)
        self.write("performance-context.json", {"owner": owner, "context_id": "c1",
                   "context": {"phase": "unknown", "source": "test_process_fixture"}})
        self.cli("look")
        before = (self.session / "play-client.json").read_bytes()
        with session_lock(self.session / "play-client.lock"):
            result = self.cli("performance", "--sample-seconds", "0.05")
        resources = result["latest"]["resources"]
        self.assertEqual(resources["cpu_percent"]["status"], "available")
        self.assertGreater(resources["interval_wall_seconds"], 0)
        self.assertGreater(resources["resident_memory"]["value"], 0)
        self.assertEqual(result["latest"]["owner"]["pid"], os.getpid())
        self.assertEqual((self.session / "play-client.json").read_bytes(), before)
        self.assertEqual(len(self.requests()), 1)

    def test_performance_baseline_is_explicit_and_does_not_overwrite(self):
        self.write("performance.latest.json", {"record_id": "p1", "owner": {"binding_id": "bound-a"}})
        baseline = self.session / "baseline.json"
        self.cli("performance", "--save-baseline", str(baseline), ok=False)
        self.cli("performance", "--tag", "camp idle", "--save-baseline", str(baseline))
        self.assertEqual(json.loads(baseline.read_text())["comparison_tag"], "camp idle")
        self.cli("performance", "--tag", "camp idle", "--save-baseline", str(baseline), ok=False)

    def test_controls_preserves_pending_and_observation_authority(self):
        self.observe()
        self.write("play-client.json", {**json.loads((self.session / "play-client.json").read_text()),
                   "operation_availability": {"game.wait": True, "game.move_relative": False}})
        self.cli("act", "world.wait")
        before = (self.session / "play-client.json").read_bytes()
        requests = self.requests()
        result = self.cli("controls")["result"]
        self.assertEqual(result["availability"], {"game.wait": True, "game.move_relative": False})
        self.assertEqual(result["wait"]["manual_start_request"]["action"], "game.act")
        self.assertEqual(result["wait"]["manual_start_request"]["action_id"], "world.wait")
        self.assertEqual((self.session / "play-client.json").read_bytes(), before)
        self.assertEqual(self.requests(), requests)

    def test_controls_discovers_bound_and_shared_logs_without_touching_pending_state(self):
        run_dir = self.session.parent / "actual-run"
        profile = self.session.parent / "profile"
        config = profile / "config"
        config.mkdir(parents=True, exist_ok=True)
        logs = {
            "native_semantic_events": run_dir / "semantic.native.events.jsonl",
            "native_semantic_snapshot": run_dir / "semantic.native.log",
            "transition_events": run_dir / "transition.events.jsonl",
            "profile_diagnostic_debug": config / "debug.log",
        }
        run_dir.mkdir(exist_ok=True)
        record = {"event": "surface_descriptor", "run_id": "run-a", "frame_id": "frame-a"}
        for name, path in logs.items():
            path.write_text(json.dumps(record) + "\n")
        self.write("game-process.json", {"binding_id": "bound-a", "run_id": "run-a",
                                         "command": "cataclysm-tiles --userdir " + str(profile),
                                         "log_paths": {name: {"path": str(path),
                                                              "scope": "run_bound" if name != "profile_diagnostic_debug" else "profile_shared"}
                                                       for name, path in logs.items()}})
        self.write("status.json", {"binding_id": "bound-a", "state": "awaiting_response",
                                    "session_descriptor": {"run_id": "run-a"}})
        before = (self.session / "status.json").read_bytes()
        result = self.cli("controls")
        logs_result = result["evidence_logs"]
        self.assertEqual(logs_result["run_id"], "run-a")
        by_name = {entry["name"]: entry for entry in logs_result["entries"]}
        self.assertEqual(by_name["native_semantic_events"]["scope"], "run_bound")
        self.assertEqual(by_name["native_semantic_events"]["status"], "available")
        self.assertEqual(by_name["transition_events"]["status"], "available")
        self.assertEqual(by_name["profile_diagnostic_debug"]["scope"], "profile_shared")
        self.assertEqual(by_name["profile_diagnostic_debug"]["status"], "available")
        self.assertNotIn("--run-id", by_name["profile_diagnostic_debug"]["query"])
        queried = subprocess.run(by_name["native_semantic_events"]["query"], capture_output=True, text=True, check=True)
        self.assertEqual(json.loads(queried.stdout)["matched"], 1)
        self.assertEqual((self.session / "status.json").read_bytes(), before)
        self.assertFalse(self.requests())

    def test_controls_reports_descriptor_owner_run_mismatch_without_querying(self):
        self.write("game-process.json", {"binding_id": "bound-a", "run_id": "owner-run",
                                         "log_paths": {"native_semantic_events": {
                                             "path": str(self.session / "events.jsonl"), "scope": "run_bound"}}})
        self.write("status.json", {"binding_id": "bound-a", "state": "ready",
                                    "session_descriptor": {"run_id": "descriptor-run"}})
        result = self.cli("controls")
        logs = result["evidence_logs"]
        self.assertEqual(logs["identity_status"], "run_mismatch")
        self.assertTrue(all(entry["query"] is None for entry in logs["entries"]))

    def test_controls_requires_current_bridge_identity_before_querying_logs(self):
        self.write("game-process.json", {"binding_id": "bound-a", "run_id": "run-a"})
        for status in ({}, {"state": "ready"}, {"binding_id": "bound-a", "state": "ready"}):
            with self.subTest(status=status):
                self.write("status.json", status)
                logs = self.cli("controls")["evidence_logs"]
                self.assertEqual(logs["identity_status"], "unavailable")
                self.assertTrue(all(entry["query"] is None for entry in logs["entries"]))
        (self.session / "status.json").unlink()
        logs = self.cli("controls")["evidence_logs"]
        self.assertEqual(logs["identity_status"], "unavailable")
        self.assertTrue(all(entry["query"] is None for entry in logs["entries"]))

    def test_controls_rejects_status_binding_mismatch(self):
        self.write("game-process.json", {"binding_id": "bound-a", "run_id": "run-a"})
        self.write("status.json", {"binding_id": "stale-binding", "state": "ready",
                                    "session_descriptor": {"run_id": "run-a"}})
        logs = self.cli("controls")["evidence_logs"]
        self.assertEqual(logs["identity_status"], "binding_mismatch")
        self.assertTrue(all(entry["query"] is None for entry in logs["entries"]))

    def test_controls_reports_unavailable_and_mismatched_log_identity(self):
        result = self.cli("controls")
        self.assertEqual(result["evidence_logs"]["identity_status"], "unavailable")
        self.assertTrue(all(entry["status"] == "unavailable" for entry in result["evidence_logs"]["entries"]))
        self.write("game-process.json", {"binding_id": "other-binding", "run_id": "run-a",
                                         "command": ["cataclysm-tiles", "--userdir", ".userdata/profile/"]})
        result = self.cli("controls")
        self.assertEqual(result["evidence_logs"]["identity_status"], "binding_mismatch")
        by_name = {entry["name"]: entry for entry in result["evidence_logs"]["entries"]}
        self.assertEqual(by_name["native_semantic_events"]["status"], "binding_mismatch")
        self.assertEqual(by_name["transition_events"]["status"], "binding_mismatch")
        self.assertEqual(by_name["profile_diagnostic_debug"]["status"], "binding_mismatch")

    def test_controls_before_observation_reports_unknown_permission(self):
        result = self.cli("controls")["result"]
        self.assertIsNone(result["availability"]["game.wait"])
        self.assertFalse((self.session / "play-client.json").exists())
        self.assertFalse(self.requests())

    def test_collect_remembers_service_macro_permission_metadata(self):
        pending = self.cli("look")
        availability = {"game.wait": False, "game.move_relative": True}
        self.reply(pending["request_id"], {"ok": True, "result": {"observation_id": "f1"},
                                         "operation_availability": availability})
        self.cli("collect")
        self.assertEqual(self.cli("controls")["result"]["availability"], availability)

    def test_persistent_pending_never_replays_and_action_uses_fresh_successor(self):
        pending = self.cli("look")
        self.assertEqual(pending["state"], "pending")
        self.cli("look", ok=False)
        self.cli("act", "world.wait", ok=False)
        self.cli("collect")
        self.assertEqual(len(self.requests()), 1)
        self.reply(pending["request_id"], {"ok": True, "result": {"observation_id": "frame-1",
            "surface": {"kind": "world", "actions": [{"id": "world.wait"}]}}})
        self.cli("collect")
        action = self.cli("act", "menu.choose", "--target", "choice-a", "--param", "amount=2")
        request = next(r for r in self.requests() if r["request_id"] == action["request_id"])
        self.assertEqual(request["binding_id"], "bound-a")
        self.assertEqual(request["request"], {"action": "game.act", "action_id": "menu.choose",
                         "observation_id": "frame-1", "stable_id": "choice-a", "parameters": {"amount": "2"}})
        self.cli("act", "menu.choose", ok=False)
        self.assertEqual(len(self.requests()), 2)
        self.reply(action["request_id"], {"ok": True, "observation": {"observation_id": "frame-2",
            "surface": {"kind": "world", "actions": [{"id": "world.wait"}]}}})
        self.cli("collect")
        final = self.cli("act", "world.wait")
        request = next(r for r in self.requests() if r["request_id"] == final["request_id"])
        self.assertEqual(request["request"]["observation_id"], "frame-2")

    def test_structured_call_preserves_recipe_and_adopts_terminal_observation(self):
        self.observe()
        request = {"action": "game.wait", "wait": {
            "enabled": True, "target_game_minutes": 101,
            "recipe": ["world.wait"], "danger_handling": "stop_on_interruption",
            "bound": {"run_id": "run-a", "actor_id": "player-a"},
        }}
        path = self.session / "macro.json"
        path.write_text(json.dumps(request))
        pending = self.cli("call", "--request", str(path))
        sent = next(r for r in self.requests() if r["request_id"] == pending["request_id"])
        self.assertEqual(sent["request"], request)
        self.assertEqual(sent["binding_id"], "bound-a")
        self.assertEqual(self.cli("call", "--request", str(path), ok=False)["error"], "request_in_flight")
        self.cli("collect")
        self.assertEqual(len(self.requests()), 2)
        self.reply(pending["request_id"], {"ok": True, "result": {
            "terminal_observation": {"observation_id": "frame-after-wait", "game_minutes": 101,
                "surface": {"kind": "world", "actions": [{"id": "world.inventory"}]}}}})
        result = self.cli("collect")
        self.assertIn("look", result["next"])
        action = self.cli("act", "world.inventory")
        sent = next(r for r in self.requests() if r["request_id"] == action["request_id"])
        self.assertEqual(sent["request"]["observation_id"], "frame-after-wait")
        self.reply(action["request_id"], {"ok": True, "observation": {"observation_id": "inventory-1",
            "surface": {"kind": "inventory", "actions": [{"id": "world.wait"}]}}})
        self.cli("collect")
        self.assertEqual(self.cli("look")["state"], "pending")

    def test_structured_call_keeps_service_authorization_and_lifecycle_guards(self):
        from cockpit import CockpitService
        path = self.session / "macro.json"
        request = {"action": "game.wait", "wait": {"enabled": True}}
        path.write_text(json.dumps(request))
        pending = self.cli("call", "--request", str(path))
        service = CockpitService(allowed_live_operations=set())
        self.reply(pending["request_id"], service.call(request))
        self.assertEqual(self.cli("collect", ok=False)["state"], "rejected")
        for value in ([], {"action": "run.finish"}, {"action": 3}):
            path.write_text(json.dumps(value))
            self.assertEqual(self.cli("call", "--request", str(path), ok=False)["error"],
                             "call_requires_a_structured_game_request")
        path.write_text(json.dumps(request))
        self.write("play-client.json", {"binding_id": "bound-a", "sealed_terminal": {"observation_id": "f"}})
        self.assertIn("journal_is_sealed", self.cli("call", "--request", str(path), ok=False)["error"])
        self.assertEqual(len(self.requests()), 1)

    def test_structured_call_terminal_exit_offers_journal(self):
        path = self.session / "macro.json"
        path.write_text(json.dumps({"action": "game.wait", "wait": {"enabled": True}}))
        pending = self.cli("call", "--request", str(path))
        self.reply(pending["request_id"], {"ok": False, "result": {"terminal_observation": {
            "observation_id": "run-a:process-exit:123", "surface": {
                "kind": "process_exited", "facts": {"exit_code": 1}, "actions": []}}}})
        result = self.cli("collect", ok=False)
        self.assertEqual(result["next"], "journal --reason REASON")
        self.assertEqual(self.cli("call", "--request", str(path), ok=False)["error"], "game_process_exited")
        self.cli("journal", "--reason", "process exited during wait")
        self.assertEqual(len(self.requests()), 2)

    def test_inspect_is_exact_and_does_not_send_actions(self):
        request_id = self.observe()
        result = self.cli("inspect", "result.surface.facts.last_save_result")
        self.assertEqual(result["slice"], "unattempted")
        self.assertEqual(result["request_id"], request_id)
        self.assertEqual(len(self.requests()), 1)
        result = self.cli("inspect", "result.surface.actions", "--limit", "1")
        self.assertEqual(result["slice"][0]["id"], "world.wait")

    def test_inspect_default_output_is_plain_text_and_logged_without_new_input(self):
        self.observe()
        process = subprocess.run([sys.executable, str(CLI), "--session", str(self.session),
                                  "inspect", "result.surface.facts.last_save_result"],
                                 capture_output=True, text=True)
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertEqual(process.stdout.strip(), "unattempted")
        self.assertEqual(len(self.requests()), 1)
        self.assertIn("unattempted", (self.session / "playtest.txt").read_text())

    def test_journal_finish_preserves_sealed_terminal_and_requires_witness(self):
        self.observe()
        witness_path = self.session / "witness.json"
        witness_path.write_text(json.dumps({"verdict": "inconclusive"}))
        self.cli("finish", "--witness", str(witness_path), ok=False)
        pending = self.cli("journal", "--reason", "missing gameplay outcome")
        self.reply(pending["request_id"], {"ok": True, "result": {"evidence_journal": {"entries": []}}})
        self.cli("collect")
        self.cli("look", ok=False)
        self.cli("act", "world.wait", ok=False)
        finish = self.cli("finish", "--witness", str(witness_path))
        request = next(r["request"] for r in self.requests() if r["request_id"] == finish["request_id"])
        self.assertEqual(request, {"action": "run.finish", "observation_id": "frame-1",
                         "stop_reason": "missing gameplay outcome", "unused_authority": "released",
                         "witness": {"verdict": "inconclusive"}})
        self.reply(finish["request_id"], {"ok": True, "result": {"schema": "caol-cockpit-live-final-v1", "state": "finished"}})
        self.cli("collect")
        self.cli("finish", "--witness", str(witness_path), ok=False)
        self.assertEqual(len(self.requests()), 3)

    def test_public_journal_and_finish_serialize_archived_replies(self):
        """The real CLI must cache lazy terminal evidence as references."""
        self.observe()
        archive = Archive(self.session / "cockpit-evidence.sqlite", run_id="run-a", binding_id="bound-a")
        self.addCleanup(archive.close)

        journal_entries = archive.sequence()
        journal_entries.append({"citation_id": "J0001", "kind": "observation",
                                "value": {"game_minutes": 100}})
        journal = self.cli("journal", "--reason", "archived terminal evidence")
        self.reply_raw(journal["request_id"], json.dumps(archive.wire({
            "ok": True,
            "result": {"evidence_journal": {"entries": journal_entries}},
        })).encode())
        journal_result = self.cli("collect")
        self.assertTrue(journal_result["ok"])
        self.assertEqual(journal_result["response"]["result"]["evidence_journal"]["entries"][0]["citation_id"], "J0001")

        witness = self.session / "witness.json"
        witness.write_text(json.dumps({"verdict": "inconclusive"}))
        transcript = archive.sequence()
        transcript.append({"kind": "terminal", "observation_id": "frame-1"})
        finish = self.cli("finish", "--witness", str(witness))
        self.reply_raw(finish["request_id"], json.dumps(archive.wire({
            "ok": True,
            "result": {"schema": "caol-cockpit-live-final-v1", "state": "finished",
                       "action_observation_sequence": transcript},
        })).encode())
        finish_result = self.cli("collect")
        self.assertTrue(finish_result["ok"])
        self.assertEqual(finish_result["state"], "collected")
        self.assertEqual(finish_result["response"]["result"]["action_observation_sequence"][0]["kind"], "terminal")

        cached = json.loads((self.session / "play-client.json").read_text())["last_collected_result"]
        reference = cached["response"]["result"]["action_observation_sequence"]
        self.assertEqual(reference["schema"], "caol-archive-sequence-ref-v1")
        self.assertEqual(reference["count"], 1)

    def test_corrupt_or_wrong_binding_response_remains_pending(self):
        pending = self.cli("look")
        self.reply(pending["request_id"], {"ok": True, "result": {"observation_id": "wrong"}}, "other")
        result = self.cli("collect", ok=False)
        self.assertEqual(result["error"], "response_binding_mismatch")
        self.cli("act", "world.wait", ok=False)
        self.reply(pending["request_id"], {"ok": True, "result": {"observation_id": "right"}})
        (self.session / "responses" / (pending["request_id"] + ".json")).write_text("{}")
        self.assertEqual(self.cli("collect", ok=False)["error"], "response_artifact_hash_mismatch")
        self.assertEqual(len(self.requests()), 1)

    def test_lock_prevents_concurrent_client_submission(self):
        with session_lock(self.session / "play-client.lock"):
            self.assertEqual(self.cli("look", ok=False)["error"], "another_play_client_is_active")
        self.assertEqual(self.requests(), [])
        self.cli("look")
        self.assertEqual(len(self.requests()), 1)

    def test_dead_bridge_returns_failure_without_resubmitting(self):
        pending = self.cli("look")
        for state in ("process_dead", "bridge_failed", "terminalization_failed"):
            self.write("status.json", {"binding_id": "bound-a", "state": state,
                                       "reason": "native owner stopped"})
            started = time.monotonic()
            result = self.cli("collect", ok=False, wait_seconds=10)
            self.assertLess(time.monotonic() - started, 2)
            self.assertEqual(result["error"], "bridge_ended_before_response")
            self.assertEqual(result["reason"], "native owner stopped")
            self.assertEqual(result["request_id"], pending["request_id"])
            self.assertIn("child.stderr.log", result["log_path"])
        self.write("status.json", {"binding_id": "bound-a", "state": "ready", "child_exit_code": 0})
        self.assertEqual(self.cli("collect", ok=False)["state"], "session_ended_without_response")
        self.cli("look", ok=False)
        self.assertEqual(len(self.requests()), 1)

    def test_single_execution_wait_collects_delayed_response_and_retains_receipt(self):
        process = self.cli_async("look", wait_seconds=2)
        deadline = time.monotonic() + 1
        while time.monotonic() < deadline and not self.requests():
            time.sleep(0.01)
        requests = self.requests()
        self.assertEqual(len(requests), 1)
        request_id = requests[0]["request_id"]
        self.reply(request_id, {"ok": True, "result": {"observation_id": "delayed-frame"}})
        stdout, stderr = process.communicate(timeout=2)
        self.assertEqual(process.returncode, 0, stderr)
        from evidence_display import recover
        result = recover(json.loads(stdout)["presentation"]["full_evidence"]["sha256"])
        self.assertEqual(result["request_id"], request_id)
        self.assertEqual(result["state"], "collected")
        self.assertEqual(len(self.requests()), 1)
        receipt = json.loads((self.session / "responses" / (request_id + ".receipt.json")).read_text())
        self.assertEqual(receipt["request_id"], request_id)

    def test_one_collect_returns_alarm_observed_on_an_earlier_internal_poll(self):
        self.write("play-client.json", {"binding_id": "bound-a", "pending": {
            "request_id": "pending-1", "request": {"action": "game.observe"},
            "submitted_unix_seconds": 10.0,
        }})
        client = PlayerClient(self.session)
        missing = {"ok": False, "error": "response_not_available_or_stale"}
        receipt = {"ok": True, "receipt": {"binding_id": "bound-a", "session_generation": 0,
                                                "response_sha256": "ignored"}}
        response = {"ok": True, "response": {"ok": True, "result": {
            "observation_id": "frame-1", "surface": {"kind": "world", "actions": []},
        }}}
        first_assessment = {"status": "measured", "alarms": [{"kind": "spike", "key": "spike:1"}],
                            "recoveries": []}
        final_assessment = {"status": "measured", "alarms": [],
                            "recoveries": [{"kind": "recovery", "recovered_alarm": "spike:1"}]}
        with patch("play_cli.Bridge.response_status", side_effect=[missing, receipt]), \
                patch("play_cli.Bridge.response_artifact", return_value=response), \
                patch("process_performance.collect_turn_assessment",
                      side_effect=[first_assessment, final_assessment]), \
                patch("play_cli.time.sleep"):
            result = client.collect(wait_seconds=1)
        self.assertEqual(result["turn_assessment"]["alarms"], first_assessment["alarms"])
        self.assertEqual(result["turn_assessment"]["recoveries"], final_assessment["recoveries"])

    def test_collect_wakes_on_cancellation_without_resubmitting(self):
        pending = self.cli("look")
        (self.session / "controls").mkdir()
        self.write("active-request.json", {"request_id": pending["request_id"],
                                            "binding_id": "bound-a", "run_id": "run-a"})
        self.write("status.json", {"binding_id": "bound-a", "state": "awaiting_response",
                                    "inflight_request_id": pending["request_id"],
                                    "session_descriptor": {"run_id": "run-a"}})
        cancelled = self.cli("cancel", "--reason", "stop delayed request")
        self.assertEqual(cancelled["request_id"], pending["request_id"])
        markers = list((self.session / "controls").glob("cancel-*.json"))
        self.assertEqual(len(markers), 1)
        marker = json.loads(markers[0].read_text())
        self.assertEqual(marker["request_id"], pending["request_id"])
        self.assertEqual(marker["binding_id"], "bound-a")
        self.assertEqual(marker["schema"], "caol-cockpit-cancel-v1")
        result = self.cli("collect", wait_seconds=10)
        self.assertEqual(result["state"], "cancellation_requested")
        self.assertEqual(result["request_id"], pending["request_id"])
        self.assertEqual(len(self.requests()), 1)

    def test_collect_deadline_preserves_single_pending_request(self):
        pending = self.cli("look", wait_seconds=0.01)
        self.assertEqual(pending["state"], "pending")
        self.assertEqual(pending["request_id"], self.cli("collect")["request_id"])
        self.assertEqual(len(self.requests()), 1)

    def test_duplicate_collect_returns_the_same_result_without_another_submission(self):
        request_id = self.observe()
        first = self.cli("collect")
        repeated = self.cli("collect")
        explicit = self.cli("collect", "--request-id", request_id)
        self.assertEqual(repeated, first)
        self.assertEqual(explicit, first)
        self.assertEqual(len(self.requests()), 1)

    def test_resume_rebuilds_lost_client_state_only_while_the_same_request_is_outstanding(self):
        pending = self.cli("look")
        (self.session / "play-client.json").unlink()
        resumed = self.cli("resume", "--request-id", pending["request_id"])
        self.assertEqual(resumed["state"], "pending")
        self.assertEqual(resumed["recovered_request"], pending["request_id"])
        self.assertEqual(len(self.requests()), 1)
        self.reply(pending["request_id"], {"ok": True, "result": {
            "observation_id": "recovered-frame", "surface": {
                "kind": "world", "actions": [{"id": "world.pause"}]}}})
        self.cli("collect")
        action = self.cli("act", "world.pause")
        sent = next(row for row in self.requests() if row["request_id"] == action["request_id"])
        self.assertEqual(sent["request"]["observation_id"], "recovered-frame")
        self.assertEqual(len(self.requests()), 2)

    def test_older_collected_request_is_read_only_after_newer_collection(self):
        request_a = self.observe("frame-a")
        request_b = self.cli("act", "world.wait")
        self.reply(request_b["request_id"], {"ok": True, "result": {
            "observation_id": "frame-b", "surface": {
                "kind": "world", "facts": {"turn": 2},
                "actions": [{"id": "world.pause", "enabled": True}]}}})
        self.cli("collect")
        before = (self.session / "play-client.json").read_bytes()
        count = len(self.requests())
        collected = self.cli("collect", "--request-id", request_a, ok=False)
        resumed = self.cli("resume", "--request-id", request_a, ok=False)
        self.assertEqual(collected["error"], "request_recovery_superseded")
        self.assertEqual(resumed["error"], "request_recovery_superseded")
        self.assertEqual(collected["retrieval"]["request_result"],
                         f"request-result --request-id {request_a}")
        self.assertEqual((self.session / "play-client.json").read_bytes(), before)
        self.assertEqual(len(self.requests()), count)
        readonly = self.cli("request-result", "--request-id", request_a)
        self.assertTrue(readonly["ok"])
        action = self.cli("act", "world.pause")
        sent = next(row for row in self.requests() if row["request_id"] == action["request_id"])
        self.assertEqual(sent["request"]["observation_id"], "frame-b")

    def test_lost_state_does_not_reclassify_an_already_recorded_response_as_pending(self):
        pending = self.cli("look")
        self.reply(pending["request_id"], {"ok": True, "result": {
            "observation_id": "recorded-frame", "surface": {
                "kind": "world", "actions": [{"id": "world.pause"}]}}})
        (self.session / "play-client.json").unlink()
        result = self.cli("resume", "--request-id", pending["request_id"], ok=False)
        self.assertEqual(result["error"], "request_already_recorded_read_only")
        self.assertFalse((self.session / "play-client.json").exists())
        self.assertEqual(len(self.requests()), 1)

    def test_safe_read_only_retrieval_does_not_wait_for_the_collect_lock(self):
        request_id = self.observe()
        self.write("play-client.json", {**json.loads((self.session / "play-client.json").read_text()),
                   "pending": {"request_id": "still-pending", "request": {"action": "game.wait"}}})
        with session_lock(self.session / "play-client.lock"):
            result = self.cli("inspect", "result.surface.actions", "--request-id", request_id,
                              "--limit", "1")
        self.assertEqual(result["slice"], [{"id": "world.wait", "enabled": True}])
        self.assertEqual(len(self.requests()), 1)

    def test_messages_remain_readable_from_the_last_displayed_frame_while_action_collects(self):
        looked = self.cli("look")
        self.reply(looked["request_id"], {"ok": True, "result": {
            "observation_id": "message-frame", "surface": {"kind": "world", "facts": {
                "messages": json.dumps([{"text": "current retained message", "time": 1}])},
                "actions": [{"id": "world.wait", "enabled": True}]}}})
        self.cli("collect")
        pending = self.cli("act", "world.wait")
        self.assertEqual(pending["state"], "pending")
        with session_lock(self.session / "play-client.lock"):
            messages = self.cli("messages", "--contains", "retained", "--limit", "1")
        self.assertEqual(messages["slice"], [{"text": "current retained message", "time": 1}])
        self.assertEqual(len(self.requests()), 2)

    def test_direct_wait_and_move_commands_preserve_player_chosen_bounds_without_request_files(self):
        wait = self.cli("wait", "--target-delta-game-minutes", "5", "--duration-action", "wait.5m",
                        "--bound-maximum", "5", "--bound-basis", "scheduler_boundary",
                        "--bound-source", "chosen observation window")
        sent_wait = next(row for row in self.requests() if row["request_id"] == wait["request_id"])
        self.assertEqual(sent_wait["request"], {"action": "game.wait", "wait": {
            "enabled": True, "recipe": ["world.wait", "wait.5m"],
            "target_delta_game_minutes": 5.0, "danger_handling": "stop_on_interruption",
            "bound": {"basis": "scheduler_boundary", "source": "chosen observation window",
                      "unit": "game_minutes", "maximum": 5.0, "progress_required": True},
        }})
        self.reply(wait["request_id"], {"ok": False, "error": "interrupted"})
        rejected = self.cli("collect", ok=False)
        self.assertEqual(self.cli("collect", "--request-id", wait["request_id"], ok=False), rejected)
        self.assertEqual(len(self.requests()), 1)
        move = self.cli("move", "--east", "1", "--south", "-1", "--bound-maximum", "2",
                        "--bound-basis", "path_progress", "--bound-source", "chosen diagonal")
        sent_move = next(row for row in self.requests() if row["request_id"] == move["request_id"])
        self.assertEqual(sent_move["request"], {"action": "game.move_relative", "move_relative": {
            "enabled": True, "offset_ms": [1, -1], "danger_handling": "stop_on_interruption",
            "bound": {"basis": "path_progress", "source": "chosen diagonal",
                      "unit": "steps", "maximum": 2},
        }})

    def test_result_frame_reuse_sends_next_action_without_redundant_look(self):
        observe_request_id = self.observe()
        first = self.cli("act", "world.wait")
        self.reply(first["request_id"], {"ok": True, "result": {
            "observation_id": "successor-frame", "surface": {
                "kind": "world", "actions": [{"id": "world.pause"}]}}})
        self.cli("collect")
        second = self.cli("act", "world.pause")
        sent = next(item for item in self.requests() if item["request_id"] == second["request_id"])
        self.assertEqual(sent["request"]["observation_id"], "successor-frame")
        # Request filenames contain random UUIDs, so glob order is not the
        # submission order. Use the authoritative IDs returned by each
        # submission to keep this assertion deterministic.
        request_by_id = {item["request_id"]: item for item in self.requests()}
        self.assertEqual([request_by_id[request_id]["request"]["action"]
                          for request_id in (observe_request_id, first["request_id"], second["request_id"])],
                         ["game.observe", "game.act", "game.act"])

    def test_reuse_matches_redundant_look_outcome_with_one_fewer_observe(self):
        def reset_session():
            for directory in (self.session / "requests", self.session / "responses"):
                for path in directory.glob("*"):
                    path.unlink()
            state = self.session / "play-client.json"
            if state.exists():
                state.unlink()
            self.write("status.json", {"binding_id": "bound-a", "state": "ready",
                                        "session_generation": 0})

        def run(redundant_look):
            self.observe()
            first = self.cli("act", "world.wait")
            successor = {"observation_id": "same-successor", "surface": {
                "kind": "world", "actions": [{"id": "world.pause"}]}}
            self.reply(first["request_id"], {"ok": True, "result": successor})
            self.cli("collect")
            if redundant_look:
                refresh = self.cli("look")
                self.reply(refresh["request_id"], {"ok": True, "result": successor})
                self.cli("collect")
            second = self.cli("act", "world.pause")
            self.reply(second["request_id"], {"ok": True, "result": {
                "observation_id": "final-frame", "surface": {
                    "kind": "world", "actions": []}}})
            final = self.cli("collect")
            return final, len(self.requests())

        reused, reused_requests = run(False)
        reset_session()
        redundant, redundant_requests = run(True)
        self.assertEqual(reused["state"], redundant["state"])
        self.assertEqual(reused["response"]["current_input"], redundant["response"]["current_input"])
        self.assertEqual(reused_requests, 3)
        self.assertEqual(redundant_requests, 4)

    def test_factless_action_result_requires_refresh_before_reuse(self):
        self.observe()
        action = self.cli("act", "world.wait")
        self.reply(action["request_id"], {"ok": True, "result": {"observation_id": "factless"}})
        self.cli("collect")
        self.assertEqual(self.cli("act", "world.pause", ok=False)["error"],
                         "look_required: no current unconsumed observation")
        self.assertEqual(len(self.requests()), 2)

    def test_stale_generation_and_binding_invalidate_result_frame(self):
        self.observe()
        action = self.cli("act", "world.wait")
        self.reply(action["request_id"], {"ok": True, "result": {
            "observation_id": "old-generation", "surface": {
                "kind": "world", "actions": [{"id": "world.pause"}]}}})
        receipt_path = self.session / "responses" / (action["request_id"] + ".receipt.json")
        receipt = json.loads(receipt_path.read_text())
        receipt["session_generation"] = 99
        receipt_path.write_text(json.dumps(receipt))
        self.cli("collect")
        self.assertEqual(self.cli("act", "world.pause", ok=False)["error"],
                         "look_required: no current unconsumed observation")
        self.assertEqual(len(self.requests()), 2)

        self.write("play-client.json", {"binding_id": "bound-a", "observation_id": "bound-frame",
                                         "observation_generation": 0, "observation_binding_id": "other",
                                         "observation_owner": "world"})
        self.assertEqual(self.cli("act", "world.pause", ok=False)["error"],
                         "look_required: current observation authority is stale")

        self.write("status.json", {"binding_id": "other-binding", "state": "ready",
                                    "session_generation": 0})
        self.write("play-client.json", {"binding_id": "bound-a", "observation_id": "bound-frame",
                                         "observation_generation": 0, "observation_binding_id": "bound-a",
                                         "observation_owner": "world"})
        self.assertEqual(self.cli("act", "world.pause", ok=False)["error"],
                         "look_required: current observation authority is stale")

        self.write("status.json", {"binding_id": "bound-a", "state": "ready",
                                    "session_generation": 0})
        self.write("play-client.json", {"binding_id": "bound-a"})
        self.observe()
        action = self.cli("act", "world.wait")
        self.reply(action["request_id"], {"ok": True, "result": {
            "observation_id": "binding-drift-frame", "surface": {
                "kind": "world", "actions": [{"id": "world.pause"}]}}})
        self.write("status.json", {"binding_id": "other-binding", "state": "ready",
                                    "session_generation": 0})
        self.cli("collect")
        self.assertEqual(self.cli("act", "world.pause", ok=False)["error"],
                         "look_required: no current unconsumed observation")

    def test_error_final_never_turns_an_action_failure_into_client_quit(self):
        self.observe()
        pending = self.cli("act", "world.wait")
        self.reply(pending["request_id"], {"ok": False, "error": "owner_lost",
                    "final": {"schema": "caol-cockpit-live-final-v1", "state": "finished"}})
        result = self.cli("collect", ok=False)
        self.assertEqual(result["next"], "look")
        self.assertFalse(json.loads((self.session / "play-client.json").read_text()).get("finished"))
        self.assertEqual(self.cli("look")["state"], "pending")
        self.assertEqual(len(self.requests()), 3)

    def test_explicit_quit_works_without_a_frame_and_after_journal_sealing(self):
        for structured in (False, True):
            with self.subTest(structured=structured):
                self.write("play-client.json", {"binding_id": "bound-a", "sealed_terminal": {"observation_id": "old"}})
                if structured:
                    path = self.session / "quit.json"
                    path.write_text(json.dumps({"action": "run.quit", "stop_reason": "player chooses to stop"}))
                    pending = self.cli("call", "--request", str(path))
                else:
                    pending = self.cli("quit", "--reason", "player chooses to stop")
                self.reply(pending["request_id"], {"ok": True, "result": {
                    "schema": "caol-cockpit-live-final-v1", "state": "finished"}})
                self.assertEqual(self.cli("collect")["next"], "collect")
                self.assertTrue(json.loads((self.session / "play-client.json").read_text())["finished"])

    def test_quit_ack_without_terminal_receipt_keeps_client_recoverable(self):
        self.observe()
        pending = self.cli("quit", "--reason", "stop")
        self.reply(pending["request_id"], {"ok": True})
        result = self.cli("collect", ok=False)
        self.assertEqual(result["state"], "unconfirmed_terminal_response")
        self.assertFalse(json.loads((self.session / "play-client.json").read_text()).get("finished"))
        self.assertEqual(self.cli("look")["state"], "pending")

    def test_declared_reentry_releases_old_grants_before_new_observation(self):
        self.write("play-client.json", {"binding_id": "bound-a",
                   "session_generation": 0, "process_exited": True,
                   "sealed_terminal": {"observation_id": "old"}, "observation_id": "old",
                   "last_request_id": "old-request", "last_collected_result": {"request_id": "old-request"},
                   "display_sha256": "0" * 64, "display_generation": 0})
        self.write("status.json", {"binding_id": "bound-a", "state": "ready", "session_generation": 0})
        self.cli("look", ok=False)
        self.write("status.json", {"binding_id": "bound-a", "state": "ready", "session_generation": 1})
        self.assertEqual(self.cli("collect")["state"], "reentered")
        reentered = json.loads((self.session / "play-client.json").read_text())
        self.assertNotIn("last_request_id", reentered)
        self.assertNotIn("display_sha256", reentered)
        self.assertEqual(self.cli("collect", "--request-id", "old-request", ok=False)["error"],
                         "request_recovery_superseded_by_reentry")
        self.assertIn("look_required", self.cli("act", "world.pause", ok=False)["error"])
        pending = self.cli("look")
        self.reply(pending["request_id"], {"ok": True, "result": {"observation_id": "new-process:frame:1",
            "surface": {"kind": "world", "actions": [{"id": "world.pause"}]}}})
        self.cli("collect")
        action = self.cli("act", "world.pause")
        sent = next(r for r in self.requests() if r["request_id"] == action["request_id"])
        self.assertEqual(sent["request"]["observation_id"], "new-process:frame:1")

    def test_messages_reads_typed_native_text_from_the_displayed_frame_without_input(self):
        for direct in (False, True):
            with self.subTest(direct=direct):
                self.write("play-client.json", {"binding_id": "bound-a"})
                pending = self.cli("look")
                observation = {"observation_id": "reply-frame", "surface": {"kind": "world", "actions": [], "facts": {
                    "messages": json.dumps([{"text": "old fixture"}, {"text": "Earlier rejection"},
                                            {"text": 'Katharina says: "Done."', "time": "16:00"}])}}}
                self.reply(pending["request_id"], {"ok": True, "observation" if direct else "result": observation})
                self.cli("collect")
                before = (self.session / "play-client.json").read_bytes()
                count = len(self.requests())
                result = self.cli("messages", "--contains", "Katharina", "--limit", "1")
                self.assertEqual(result["slice"], [{"text": 'Katharina says: "Done."', "time": "16:00"}])
                self.assertEqual(result["observation_id"], "reply-frame")
                self.assertEqual(result["source_indices"], [2])
                self.assertEqual((self.session / "play-client.json").read_bytes(), before)
                self.assertEqual(len(self.requests()), count)

    def test_terminal_process_view_offers_journal_without_another_native_action(self):
        pending = self.cli("look")
        self.reply(pending["request_id"], {"ok": True, "result": {"observation_id": "run:process-exit:123",
                   "surface": {"kind": "process_exited", "facts": {"exit_code": 0}, "actions": []}}})
        result = self.cli("collect")
        self.assertEqual(result["state"], "process_exited")
        self.assertEqual(result["next"], "journal --reason REASON")
        self.cli("act", "world.move.north", ok=False)
        self.assertEqual(len(self.requests()), 1)
        self.cli("journal", "--reason", "process exited")
        self.assertEqual(len(self.requests()), 2)

    def test_finished_collect_reports_actual_cleanup_instead_of_inventing_termination(self):
        self.write("play-client.json", {"binding_id": "bound-a", "finished": True})
        self.write("status.json", {"binding_id": "bound-a", "state": "terminalizing"})
        before = (self.session / "play-client.json").read_bytes()
        self.assertEqual(self.cli("collect", "--request-id", "old-request", ok=False)["error"],
                         "request_recovery_session_finished")
        self.assertEqual((self.session / "play-client.json").read_bytes(), before)
        self.assertEqual(self.cli("collect")["state"], "finishing")
        self.write("status.json", {"binding_id": "bound-a", "state": "safe_to_cleanup",
                   "cleanup": {"status": "accepted"}, "terminalization": {
                       "cleanup": {"status": "already_exited", "native_exit_credit": True}}})
        result = self.cli("collect")
        self.assertEqual(result["state"], "finished")
        self.assertEqual(result["cleanup"]["status"], "already_exited")
        self.write("status.json", {"binding_id": "bound-a", "state": "terminalization_failed",
                                   "reason": "owned cleanup failed"})
        result = self.cli("collect", ok=False)
        self.assertEqual(result["state"], "cleanup_failed")
        self.assertEqual(result["reason"], "owned cleanup failed")
        self.assertEqual(self.requests(), [])
        self.write("status.json", {"binding_id": "bound-a", "state": "reentry_failed",
                   "reason": "missing_cockpit_session_descriptor",
                   "admission": "closed_until_explicit_cleanup",
                   "reentry_failure": {"replacement": {"status": "retained", "game_process": {"status": "alive", "pid": 96450}}}})
        result = self.cli("collect", ok=False)
        self.assertEqual(result["state"], "cleanup_failed")
        self.assertEqual(result["bridge_state"], "reentry_failed")
        self.assertEqual(result["next"], "inspect retained evidence")
        self.assertEqual(result["reentry_failure"]["replacement"]["status"], "retained")

    def test_rejected_action_without_successor_requires_new_look(self):
        self.observe()
        pending = self.cli("act", "world.wait")
        self.reply(pending["request_id"], {"ok": False, "error": "stale_frame"})
        result = self.cli("collect", ok=False)
        self.assertFalse(result["response"]["ok"])
        self.assertEqual(result["next"], "look")
        self.cli("act", "world.wait", ok=False)
        self.cli("look")
        self.assertEqual(len(self.requests()), 3)


if __name__ == "__main__":
    unittest.main()
