"""Actual public quit -> terminalizer boundary, with intercepted signals only."""
import json
import io
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch, Mock

import cockpit_live_session_test as fixtures
import startup_harness as harness


class GenericQuitTest(unittest.TestCase):
    def test_graceful_request_never_enters_terminalizer_or_signals(self):
        service, finals = fixtures.LiveSessionTest().service([fixtures.frame(1, 100)])
        with patch.object(harness.os, "kill") as signal:
            for request in ({"action": "run.quit", "stop_reason": "graceful close"},
                            {"action": "run.quit", "abort": False},
                            {"action": "run.quit", "abort": "true"}):
                result = service.call(request)
                self.assertFalse(result["ok"])
                self.assertEqual(result["error"], "native_graceful_close_required")
                self.assertFalse(result["termination_requested"])
                self.assertEqual(service.run_channel.status()["state"], "active")
            signal.assert_not_called()
        self.assertEqual(finals, [])
        self.assertTrue(service.call({"action": "game.observe"})["ok"])

    def test_explicit_abort_uses_bound_cleanup_before_terminalization(self):
        service, finals = fixtures.LiveSessionTest().service([fixtures.frame(1, 100)])
        self.assertTrue(service.call({"action": "game.observe"})["ok"])
        generation = {"pid": 123, "alive": True, "birth_identity": "fixture-birth",
                      "command": "/fixture/cataclysm --userdir /fixture/owned"}
        current = dict(generation)
        def intercepted(pid, signal):
            self.assertEqual(pid, 123)
            self.assertEqual(signal, harness.signal.SIGTERM)
            current["alive"] = False
        service.run_channel._abort_owned_process = lambda _reason: harness.cleanup_game_process(
            123, explicit_quit=True, explicit_abort=True, expected_process_generation=generation)
        with patch.object(harness, "_process_generation_snapshot", side_effect=lambda *a, **k: dict(current)), \
                patch.object(harness.os, "kill", side_effect=intercepted) as signal:
            result = service.call({"action": "run.quit", "abort": True})
        self.assertTrue(result["ok"])
        self.assertEqual(result["result"]["state"], "exit_observation_pending")
        if harness.os.name != "nt":
            signal.assert_called_once_with(123, harness.signal.SIGTERM)
            self.assertEqual(result["result"]["cleanup"]["status"], "terminated")
        self.assertFalse(result["result"]["native_exit_credit"])
        self.assertFalse(result["result"]["native_save_credit"])
        self.assertEqual(service.run_channel.status()["state"], "active")
        self.assertEqual(finals, [])

    def test_bridge_explicit_cleanup_forwards_abort_not_graceful_quit(self):
        from cockpit_file_bridge import FileBackedCockpitBridge
        with tempfile.TemporaryDirectory() as temp:
            bridge = FileBackedCockpitBridge(Path(temp) / "session", ["unused"],
                                             binding_id="fixture-binding", require_session_ready=True)
            bridge.prepare()
            bridge._child = Mock(pid=0, stdin=io.StringIO())
            bridge._child.poll.return_value = None
            terminal = {"ok": True, "result": {"schema": "caol-cockpit-live-final-v1", "state": "finished", "closure_kind": "explicit_abort", "native_exit_credit": False}}
            with patch.object(bridge, "_read_complete_response", return_value=json.dumps(terminal)), \
                    patch.object(bridge, "_emergency_cleanup") as cleanup:
                self.assertTrue(bridge._handle_request(json.dumps({"control": "cleanup", "binding_id": "fixture-binding"})))
            request = json.loads(bridge._child.stdin.getvalue())
            self.assertTrue(request["abort"])
            self.assertEqual(request["action"], "run.quit")
            cleanup.assert_not_called()

    def test_bridge_abort_pending_keeps_live_channel(self):
        from cockpit_file_bridge import FileBackedCockpitBridge
        with tempfile.TemporaryDirectory() as temp:
            bridge = FileBackedCockpitBridge(Path(temp) / "session", ["unused"],
                                             binding_id="fixture-binding", require_session_ready=True)
            bridge.prepare()
            bridge._child = Mock(pid=123, stdin=io.StringIO())
            bridge._child.poll.return_value = None
            pending = {"ok": True, "result": {"schema": "caol-cockpit-abort-pending-v1",
                       "state": "exit_observation_pending", "cleanup": {"status": "graceful_quit_unconfirmed_process_retained"}}}
            with patch.object(bridge, "_read_complete_response", return_value=json.dumps(pending)), \
                    patch.object(bridge, "_emergency_cleanup") as cleanup:
                self.assertFalse(bridge._handle_request(json.dumps({"control": "cleanup", "binding_id": "fixture-binding"})))
            cleanup.assert_not_called()
            self.assertEqual(json.loads(bridge.status_path.read_text())["state"], "ready")

    def test_existing_native_reporting_is_not_an_abort(self):
        service, finals = fixtures.LiveSessionTest().service([fixtures.frame(1, 100)])
        with patch.object(harness.os, "kill") as signal:
            refusal = service.call({"action": "run.quit"})
            self.assertFalse(refusal["ok"])
            # Refusal neither consumes observation grants nor ends reporting.
            self.assertTrue(service.call({"action": "game.observe"})["ok"])
            signal.assert_not_called()
        self.assertEqual(finals, [])


if __name__ == "__main__":
    unittest.main()
