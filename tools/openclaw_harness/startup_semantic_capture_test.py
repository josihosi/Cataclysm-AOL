import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import startup_harness as harness


class NativeOnlyStartupCaptureTest(unittest.TestCase):
    def test_native_startup_and_failure_reports_need_no_capture_backend(self):
        with tempfile.TemporaryDirectory() as tmp, \
                patch.object(harness, "choose_capture_window", side_effect=FileNotFoundError("peekaboo")) as windows, \
                patch.object(harness, "run_screen_ocr", side_effect=FileNotFoundError("ocr")) as ocr, \
                patch.object(harness.subprocess, "run", side_effect=FileNotFoundError("capture backend")) as run:
            directory = Path(tmp)
            for label in ("success", "failure_process_exit", "failure_timeout"):
                with self.subTest(label=label):
                    capture = harness.capture_screenshot(123, directory, label, semantic_only=True)
                    summary = capture["screen_summary"]
                    self.assertFalse(summary["capture_success"])
                    self.assertEqual(summary["capture_process_pid"], 123)
                    summary["startup_screen_probe"] = harness.capture_startup_screen_probe(
                        directory, label, summary, directory / "debug.log", "bound-run")
                    self.assertEqual(summary["startup_screen_probe"]["classification"],
                                     "not_requested_native_semantic")
                    result = harness.startup_proof_classification(
                        ok=label == "success", screen_summary=summary,
                        native_semantic_startup_ready=label == "success",
                        failure_reason="" if label == "success" else label)
                    self.assertEqual(result["status"], "green" if label == "success" else "red")
                    self.assertFalse(result["feature_proof"])
                    if label != "success":
                        self.assertEqual(result["verdict"], label)
            windows.assert_not_called()
            ocr.assert_not_called()
            run.assert_not_called()

    def test_skipping_capture_does_not_substitute_for_native_readiness_or_clean_logs(self):
        summary = {"capture_success": False, "capture_status": "not_requested_native_semantic"}
        absent = harness.startup_proof_classification(ok=True, screen_summary=summary)
        self.assertEqual(absent["status"], "red")
        for errors in ({"debug_errors_recorded": 1}, {"debug_popups_recorded": 1}):
            with self.subTest(errors=errors):
                result = harness.startup_proof_classification(
                    ok=True, screen_summary=summary, native_semantic_startup_ready=True, **errors)
                self.assertEqual(result["status"], "red")
                self.assertFalse(result["startup_clean_for_feature_steps"])
        logged = dict(summary, startup_screen_probe={"startup_error_logged": True})
        result = harness.startup_proof_classification(
            ok=True, screen_summary=logged, native_semantic_startup_ready=True)
        self.assertEqual(result["feature_gate"], "startup_error_logged")


if __name__ == "__main__":
    unittest.main()
