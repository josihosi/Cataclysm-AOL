import json
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

    def test_terminal_native_descriptor_makes_intentional_no_capture_green(self):
        summary = {
            "capture_success": False,
            "capture_status": "not_requested_native_semantic",
            "native_semantic_startup": {"status": "required_state_present"},
            "surface_identity": {"transport": "terminal_native", "ok": True},
        }
        self.assertEqual(
            harness.startup_screen_capture_verdict(summary),
            "green_native_semantic_terminal_hud",
        )
        result = harness.startup_proof_classification(
            ok=True, screen_summary=summary, native_semantic_startup_ready=True)
        self.assertEqual(result["status"], "green")
        self.assertTrue(result["startup_clean_for_feature_steps"])

    def test_terminal_step_text_uses_only_its_run_bound_transcript(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            (directory / "game.terminal.log").write_bytes(
                b"\x1b[1;1HMove: 0(W)  Wield: fists\n"
            )
            capture = {"screen_summary": {
                "capture_success": False,
                "capture_status": "not_requested_native_semantic",
            }}
            result = harness.capture_step_text_artifact(directory, "terminal-step", capture)
            self.assertTrue(result["ok"])
            self.assertEqual(result["kind"], "terminal_transcript_text_capture")
            self.assertEqual(result["source_terminal_transcript"], str(directory / "game.terminal.log"))
            self.assertIn("Move: 0(W)  Wield: fists", result["tail_lines"])
            self.assertEqual(
                harness.evaluate_screen_text_expectation(result, ["Move:", "Wield:"])["status"],
                "matched",
            )

    def test_terminal_interruption_scan_reads_the_run_transcript_not_its_temp_scan_dir(self):
        with tempfile.TemporaryDirectory() as tmp, \
                patch.dict(harness.TERMINAL_NATIVE_INPUTS, {
                    42: {"endpoint": "/tmp/pty.sock", "run_id": "run-42", "run_dir": tmp},
                }, clear=False), \
                patch.object(harness, "run_screen_ocr", side_effect=AssertionError("OCR must not run")):
            directory = Path(tmp)
            (directory / "game.terminal.log").write_bytes(b"\x1b[1;1HMove: 0(W)  Safe: Off\n")
            result = harness.acknowledge_blocking_interruptions(
                42, directory, "terminal-clear", max_acknowledgements=0, settle_seconds=0,
            )
        self.assertEqual(result["status"], "clear")
        diagnostic = result["final_classification"]["screen_text_diagnostic"] \
            if "screen_text_diagnostic" in result["final_classification"] else None
        self.assertIsNone(diagnostic)

    def test_r029_curses_manifest_uses_terminal_move_text_not_tiles_wield_text(self):
        manifest_path = Path(__file__).parent / "scenarios" / \
            "bandit.extortion_committed_paid_departure_continuation_mcw.json"
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        startup_step = manifest["steps"][0]
        self.assertEqual(manifest["runtime_contract"]["terminal_transport"], "pty")
        self.assertNotIn("expected_screen_text_after_contains", startup_step)
        self.assertIn("semantic descriptor", startup_step["expected_visible_fact"])

    def test_r029_payment_manifest_preflights_zero_credit_candidate_not_authored_operation(self):
        manifest_path = Path(__file__).parent / "scenarios" / \
            "bandit.extortion_committed_paid_departure_continuation_mcw.json"
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        preflight = next(step for step in manifest["steps"]
                         if step["label"] == "preflight_scheduler_response_candidate")
        self.assertTrue(preflight["required_scout_report_present"])
        self.assertEqual(preflight["required_camp_decision_state"], "report_awaiting_assessment")
        self.assertTrue(preflight["required_report_decision_identity_match"])
        self.assertNotIn("required_active_hostile_operation_phase", preflight)

    def test_r029_payment_manifest_uses_bounded_native_duration_and_stops_at_pay(self):
        manifest_path = Path(__file__).parent / "scenarios" / \
            "bandit.extortion_committed_paid_departure_continuation_mcw.json"
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        wait = next(step for step in manifest["steps"]
                    if step["label"] == "advance_to_reopened_pay_gate")
        self.assertEqual(wait["kind"], "semantic_terminal_wait_until")
        self.assertEqual(wait["required_action_chain"],
                         ["world.wait", "wait.duration_menu", "wait.30m"])
        self.assertEqual(wait["stop_when_advertised_action"], "shakedown.pay")
        self.assertEqual(wait["max_windows"], 16)
        self.assertTrue(wait["ordinary_applying_turn_after_duration"])
        self.assertEqual(wait["max_unchanged_operation_windows"], 1)

    def test_live_operation_progress_key_ignores_clock_but_tracks_route_and_member_state(self):
        base = {
            "operation_id": "hostile:2", "phase": "approaching", "owner": "abstract",
            "target_id": "player@146,51,0", "target_omt": [146, 51, 0],
            "route_cursor": {"waypoint_index": 1, "route_length": 7,
                             "projection_omt": [141, 51, 0], "local_contact_minutes": -1},
            "members": [{"id": 18, "state": "outbound", "omt": [141, 51, 0]}],
        }
        later_clock = dict(base, current_game_minutes=8340)
        self.assertEqual(harness.semantic_hostile_operation_progress_key(base),
                         harness.semantic_hostile_operation_progress_key(later_clock))
        advanced = dict(base, route_cursor=dict(base["route_cursor"], waypoint_index=2,
                                                projection_omt=[142, 51, 0]))
        self.assertNotEqual(harness.semantic_hostile_operation_progress_key(base),
                            harness.semantic_hostile_operation_progress_key(advanced))


if __name__ == "__main__":
    unittest.main()
