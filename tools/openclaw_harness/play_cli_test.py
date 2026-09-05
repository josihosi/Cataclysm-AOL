"""Exercise the public player CLI against retained bridge traffic, without a game."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
from play_cli import session_lock

CLI = Path(__file__).with_name("play_cli.py")


class PlayerCliTest(unittest.TestCase):
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

    def cli(self, *arguments, ok=True):
        process = subprocess.run([sys.executable, str(CLI), "--session", str(self.session),
                                  "--wait-seconds", "0", *arguments], capture_output=True, text=True)
        self.assertEqual(process.returncode, 0 if ok else 1, process.stderr + process.stdout)
        return json.loads(process.stdout)

    def requests(self):
        return [json.loads(p.read_text()) for p in (self.session / "requests").glob("*.json")]

    def reply(self, request_id, value, binding="bound-a"):
        raw = json.dumps(value).encode()
        path = "responses/" + request_id + ".json"
        (self.session / path).write_bytes(raw)
        self.write("responses/" + request_id + ".receipt.json", {
            "request_id": request_id, "binding_id": binding,
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

    def test_persistent_pending_never_replays_and_action_uses_fresh_successor(self):
        pending = self.cli("look")
        self.assertEqual(pending["state"], "pending")
        self.cli("look", ok=False)
        self.cli("act", "world.wait", ok=False)
        self.cli("collect")
        self.assertEqual(len(self.requests()), 1)
        self.reply(pending["request_id"], {"ok": True, "result": {"observation_id": "frame-1"}})
        self.cli("collect")
        action = self.cli("act", "menu.choose", "--target", "choice-a", "--param", "amount=2")
        request = next(r for r in self.requests() if r["request_id"] == action["request_id"])
        self.assertEqual(request["binding_id"], "bound-a")
        self.assertEqual(request["request"], {"action": "game.act", "action_id": "menu.choose",
                         "observation_id": "frame-1", "stable_id": "choice-a", "parameters": {"amount": "2"}})
        self.cli("act", "menu.choose", ok=False)
        self.assertEqual(len(self.requests()), 2)
        self.reply(action["request_id"], {"ok": True, "observation": {"observation_id": "frame-2"}})
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
            "terminal_observation": {"observation_id": "frame-after-wait", "game_minutes": 101}}})
        result = self.cli("collect")
        self.assertIn("look", result["next"])
        action = self.cli("act", "world.inventory")
        sent = next(r for r in self.requests() if r["request_id"] == action["request_id"])
        self.assertEqual(sent["request"]["observation_id"], "frame-after-wait")
        self.reply(action["request_id"], {"ok": True, "observation": {"observation_id": "inventory-1"}})
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
        self.reply(finish["request_id"], {"ok": True, "result": {"state": "finished"}})
        self.cli("collect")
        self.cli("finish", "--witness", str(witness_path), ok=False)
        self.assertEqual(len(self.requests()), 3)

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
            result = self.cli("collect", ok=False)
            self.assertEqual(result["error"], "bridge_ended_before_response")
            self.assertEqual(result["reason"], "native owner stopped")
            self.assertEqual(result["request_id"], pending["request_id"])
            self.assertIn("child.stderr.log", result["log_path"])
        self.write("status.json", {"binding_id": "bound-a", "state": "ready", "child_exit_code": 0})
        self.assertEqual(self.cli("collect", ok=False)["state"], "session_ended_without_response")
        self.cli("look", ok=False)
        self.assertEqual(len(self.requests()), 1)

    def test_native_fail_closed_final_ends_session_even_when_response_is_error(self):
        self.observe()
        pending = self.cli("act", "world.wait")
        self.reply(pending["request_id"], {"ok": False, "error": "owner_lost",
                    "final": {"schema": "caol-cockpit-live-final-v1", "state": "finished"}})
        result = self.cli("collect", ok=False)
        self.assertEqual(result["next"], "collect")
        self.cli("look", ok=False)
        self.cli("act", "world.wait", ok=False)
        self.assertEqual(len(self.requests()), 2)

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
