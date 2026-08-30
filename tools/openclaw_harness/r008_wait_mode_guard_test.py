#!/usr/bin/env python3
"""Focused R-008 semantic parent wait-menu guard tests."""

from pathlib import Path
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parent))

import startup_harness


class R008WaitModeGuardTest(unittest.TestCase):
    run_id = "run-080"

    def frame(self, *, run_id=None, state="wait_mode_choice", actions=None, frame_id="frame-new"):
        actions = actions if actions is not None else {
            "wait.duration_menu": "w", "alarm.duration_menu": "a",
        }
        return {
            "event": "frame", "run_id": run_id or self.run_id,
            "frame_id": frame_id, "state": state, "valid_actions": list(actions),
            "action_inputs": actions,
        }

    def check(self, frame, prior="frame-old"):
        with patch.object(startup_harness, "refresh_semantic_step_trace", return_value=(Path("source"), Path("owned"))), \
                patch.object(startup_harness, "read_semantic_step_trace", return_value=([frame], "ok")):
            return startup_harness.validate_native_wait_mode_semantic_frame(
                profile="profile", run_dir=Path("run"), run_id=self.run_id,
                start_offset=0, prior_frame_id=prior,
            )

    def test_accepts_fresh_same_run_parent_frame_with_bound_actions(self):
        result = self.check(self.frame())
        self.assertEqual(result["status"], "matched")
        self.assertEqual(result["reason"], "matched")

    def test_rejects_missing_frame(self):
        with patch.object(startup_harness, "refresh_semantic_step_trace", return_value=(Path("source"), Path("owned"))), \
                patch.object(startup_harness, "read_semantic_step_trace", return_value=([], "ok")):
            result = startup_harness.validate_native_wait_mode_semantic_frame(
                profile="profile", run_dir=Path("run"), run_id=self.run_id,
                start_offset=0, prior_frame_id="frame-old",
            )
        self.assertEqual(result["reason"], "missing_frame")

    def test_rejects_stale_wrong_state_wrong_run_and_incomplete_actions(self):
        cases = [
            (self.frame(frame_id="frame-old"), "stale_frame"),
            (self.frame(state="world"), "wrong_state"),
            (self.frame(run_id="other-run"), "wrong_run"),
            (self.frame(actions={"wait.duration_menu": "w"}), "incomplete_actions"),
            (self.frame(actions={"wait.duration_menu": "w", "alarm.duration_menu": ""}), "incomplete_action_inputs"),
        ]
        for frame, reason in cases:
            with self.subTest(reason=reason):
                self.assertEqual(self.check(frame)["reason"], reason)

    def test_long_wait_uses_semantic_duration_receipts_when_ocr_is_wrong(self):
        semantic_guard = {
            "status": "matched",
            "reason": "matched",
            "frame_id": "frame-new",
        }

        def diagnostic_only_ocr_guard(_screen_text, *, surface, **_kwargs):
            if surface == "parent_wait_chooser":
                self.fail("the old OCR-only parent-menu guard ran after a valid semantic frame")
            return {"status": "blocked", "missing_patterns": ["6 hours"]}

        with patch.object(startup_harness, "current_semantic_step_frame", return_value={"frame_id": "frame-old"}), \
                patch.object(startup_harness, "validate_native_wait_mode_semantic_frame", return_value=semantic_guard), \
                patch.object(startup_harness, "validate_native_wait_duration_selection", side_effect=[
                    {"status": "matched", "reason": "advertised_same_run_duration"},
                    {"status": "matched", "reason": "same_run_semantic_and_native_duration_selection_matched"},
                ]) as duration_selection, \
                patch.object(startup_harness, "capture_screenshot", return_value={"screen_summary": {}}), \
                patch.object(startup_harness, "capture_screen_text_artifact", return_value={"ok": True, "text": ""}), \
                patch.object(startup_harness, "validate_native_wait_menu_surface", side_effect=diagnostic_only_ocr_guard), \
                patch.object(startup_harness, "peekaboo_press_sequence") as press:
            result = startup_harness.execute_long_wait_action(
                42, Path("run"), "semantic_parent", {
                    "pre_menu_choice_key": "w",
                    "choice_key": "3",
                    "require_native_wait_menu_guards": True,
                    "menu_settle_seconds": -1,
                    "pre_menu_settle_seconds": -1,
                    "after_choice_settle_seconds": -1,
                    "completion_wait_seconds": -1,
                    "require_run_bound_wait_classification": False,
                    "auto_acknowledge_interruptions": False,
                },
                semantic_profile="profile", semantic_run_id=self.run_id,
            )

        self.assertEqual(result["parent_wait_menu_guard"]["status"], "matched")
        self.assertTrue(result["parent_wait_menu_guard"]["ocr_guard_bypassed"])
        self.assertEqual(result["duration_wait_menu_guard"]["status"], "diagnostic_only")
        self.assertNotIn("abort", result)
        self.assertEqual(duration_selection.call_count, 2)
        self.assertEqual([call.args[1] for call in press.call_args_list], [["|"], ["w"], ["3"]])


if __name__ == "__main__":
    unittest.main()
