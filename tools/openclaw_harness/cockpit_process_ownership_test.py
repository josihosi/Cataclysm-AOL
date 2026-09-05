import json
import os
import subprocess
import sys
import time
from pathlib import Path
import tempfile
import unittest
from unittest.mock import Mock, patch
import startup_harness as harness
from cockpit_file_bridge import FileBackedCockpitBridge

class ProcessOwnershipTest(unittest.TestCase):
    def test_early_identity_is_persisted_and_reclaimed_before_hud(self):
        with tempfile.TemporaryDirectory() as tmp:
            session = Path(tmp) / "session"
            bridge = FileBackedCockpitBridge(session, ["unused"], binding_id="bound")
            bridge.prepare()
            process = Mock(pid=123)
            command = "/test/cataclysm-tiles --userdir /test/disposable"
            with patch.object(harness, "pid_command", return_value=command):
                harness.record_bridge_game_process(process, {
                    "OPENCLAW_COCKPIT_BRIDGE_SESSION_DIR": str(session),
                    "OPENCLAW_COCKPIT_BRIDGE_BINDING_ID": "bound",
                    "OPENCLAW_HARNESS_RUN_ID": "run"})
                record = json.loads((session / "game-process.json").read_text())
                self.assertEqual(record["pid"], 123)
                self.assertEqual(record["run_id"], "run")
                with patch.object(harness, "cleanup_game_process", return_value={"status": "terminated"}) as cleanup:
                    bridge._emergency_cleanup()
                    cleanup.assert_called_once_with(123)
            with patch.object(harness, "pid_command", return_value="unrelated process"), patch.object(harness, "cleanup_game_process") as cleanup:
                bridge._startup_progress = {"pid": 123}
                result = bridge._emergency_cleanup()
                cleanup.assert_not_called()
                self.assertEqual(result["ownership"], "process_exited_or_identity_changed")

    def test_progress_pid_without_owned_identity_is_not_killed(self):
        with tempfile.TemporaryDirectory() as tmp:
            bridge = FileBackedCockpitBridge(Path(tmp) / "session", ["unused"], binding_id="bound")
            bridge.prepare()
            bridge._startup_progress = {"pid": 123}
            with patch.object(harness, "cleanup_game_process") as cleanup:
                result = bridge._emergency_cleanup()
                cleanup.assert_not_called()
                self.assertEqual(result["ownership"], "unconfirmed_missing_process_record")

    @unittest.skipUnless(os.name == "posix", "POSIX wait ownership")
    def test_liveness_does_not_steal_nonzero_child_exit_status(self):
        process = subprocess.Popen([sys.executable, "-c", "import sys; sys.exit(17)"])
        try:
            deadline = time.monotonic() + 5
            while harness.pid_is_alive(process.pid) and time.monotonic() < deadline:
                time.sleep(0.01)
            self.assertFalse(harness.pid_is_alive(process.pid))
            self.assertEqual(process.wait(timeout=2), 17)
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()

    def test_liveness_probe_leaves_running_child_alive_and_preserves_exit(self):
        process = subprocess.Popen([sys.executable, "-u", "-c",
            "import sys; print('ready',flush=True); sys.stdin.readline(); sys.exit(17)"],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
        try:
            self.assertEqual(process.stdout.readline().strip(), "ready")
            self.assertTrue(harness.pid_is_alive(process.pid))
            self.assertIsNone(process.poll())
            process.communicate("finish\n", timeout=5)
            self.assertEqual(process.returncode, 17)
            self.assertFalse(harness.pid_is_alive(process.pid))
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()
            process.stdin.close()
            process.stdout.close()

    def test_exit_receipt_binds_observed_return_code_to_owned_run(self):
        with tempfile.TemporaryDirectory() as tmp:
            harness.record_bridge_game_exit(Mock(pid=123), {
                "OPENCLAW_COCKPIT_BRIDGE_SESSION_DIR": tmp,
                "OPENCLAW_COCKPIT_BRIDGE_BINDING_ID": "bound",
                "OPENCLAW_HARNESS_RUN_ID": "run"}, 0)
            record = json.loads((Path(tmp) / "game-process-exit.json").read_text())
            self.assertEqual((record["binding_id"], record["run_id"], record["pid"], record["exit_code"]),
                             ("bound", "run", 123, 0))
            self.assertTrue(record["exit_observed_at"])

    def test_missing_process_identity_stops_and_reaps_before_startup(self):
        process = Mock(pid=123)
        with tempfile.TemporaryDirectory() as tmp, patch.object(harness, "pid_command", return_value=""):
            with self.assertRaisesRegex(OSError, "process identity unavailable"):
                harness.record_bridge_game_process(process, {
                    "OPENCLAW_COCKPIT_BRIDGE_SESSION_DIR": tmp,
                    "OPENCLAW_COCKPIT_BRIDGE_BINDING_ID": "bound"})
            process.terminate.assert_called_once()
            process.wait.assert_called_once_with(timeout=2)
            self.assertFalse((Path(tmp) / "game-process.json").exists())

    def test_ownership_write_failure_stops_the_new_child(self):
        process = Mock(pid=123)
        with tempfile.TemporaryDirectory() as tmp, patch.object(harness, "pid_command", return_value="game"):
            with self.assertRaises(OSError):
                harness.record_bridge_game_process(process, {
                    "OPENCLAW_COCKPIT_BRIDGE_SESSION_DIR": str(Path(tmp) / "missing"),
                    "OPENCLAW_COCKPIT_BRIDGE_BINDING_ID": "bound"})
            process.terminate.assert_called_once()

if __name__ == "__main__": unittest.main()
