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

    def test_explicit_abort_actual_finalizer_intercepts_signal_without_native_credit(self):
        service, _ = fixtures.LiveSessionTest().service([fixtures.frame(1, 100)])
        generation = {"pid": 123, "alive": True, "birth_identity": "fixture-birth",
                      "command": "/fixture/cataclysm --userdir /fixture/owned"}
        owned = {"pid": 123, "status": "alive", "expected": generation}
        current = dict(generation)
        def intercepted(pid, signal):
            self.assertEqual(pid, 123)
            self.assertEqual(signal, harness.signal.SIGTERM)
            current["alive"] = False
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp)
            service.run_channel._finalize_session = lambda report: harness.finalize_cockpit_live_session(
                directory, 123, report)
            with patch.object(harness, "current_owned_process_generation", return_value=owned), \
                    patch.object(harness, "process_generation_snapshot", side_effect=lambda *a, **k: dict(current)), \
                    patch.object(harness.os, "kill", side_effect=intercepted) as signal:
                result = service.call({"action": "run.quit", "abort": True,
                                       "stop_reason": "explicit test abort, never native save"})
            if harness.os.name == "nt":
                signal.assert_not_called()
                self.assertEqual(result["result"]["cleanup"]["status"],
                                 "graceful_quit_unconfirmed_process_retained")
                self.assertTrue(result["result"]["cleanup"]["observed_process_generation"]["alive"])
            else:
                signal.assert_called_once_with(123, harness.signal.SIGTERM)
                self.assertEqual(result["result"]["cleanup"]["status"], "terminated")
            self.assertTrue(result["ok"])
            report = json.loads((directory / "cockpit.live.final.json").read_text())
            self.assertEqual(report["closure_kind"], "explicit_abort")
            self.assertTrue(report["stop_detail"]["explicit_player_abort"])
            for key in ("native_exit_credit", "native_save_credit", "normal_exit_credit", "declared_reentry_ready"):
                self.assertFalse(report[key])
            self.assertFalse(report["cleanup"]["native_exit_credit"])

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
