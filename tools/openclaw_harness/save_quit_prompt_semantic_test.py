"""Focused binding checks for the native Save-and-Quit confirmation fallback."""

from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from startup_harness import evaluate_screen_text_or_rendered_hud_expectation


class SaveQuitPromptSemanticTest(unittest.TestCase):
    @staticmethod
    def trace(run_id: str = "current-run", event: str = "open") -> str:
        return (
            "openclaw_harness_ui_trace: component=semantic_ui "
            f'event={event} instance_id="save-quit-7" run_id="{run_id}" '
            'intent="save_quit_confirmation" valid_actions=["Y"] '
            'postcondition="save_quit_confirmation_resolved"\n'
        )

    def evaluate(self, trace: str, run_id: str = "current-run") -> dict:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            log = root / "feature.debug.log"
            log.write_text(trace, encoding="utf-8")
            text = root / "screen.json"
            text.write_text(json.dumps({"lines": ["Save and quit? (Case Sensitiv"]}), encoding="utf-8")
            return evaluate_screen_text_or_rendered_hud_expectation(
                {"json_path": str(text)}, ["Case Sensitive"], None,
                action_trace_log=log, run_id=run_id, screen_summary={},
                startup_overlay_recovery=None,
            )

    def test_current_native_save_confirmation_proves_truncated_ocr_prompt(self) -> None:
        result = self.evaluate(self.trace())
        self.assertEqual(result["status"], "matched")
        self.assertEqual(result["match_provenance"], "native_save_quit_confirmation_trace")

    def test_wrong_run_or_closed_confirmation_stays_missing(self) -> None:
        self.assertEqual(self.evaluate(self.trace("previous-run"))["status"], "missing")
        self.assertEqual(self.evaluate(self.trace(event="return"))["status"], "missing")

    def test_current_confirmation_in_a_capped_trace_remains_bound(self) -> None:
        result = self.evaluate("x" * 300000 + self.trace())
        self.assertEqual(result["status"], "matched")
        self.assertTrue(result["native_trace_tail_bound"])

    def test_native_trace_is_limited_to_the_save_and_quit_query(self) -> None:
        source = (Path(__file__).resolve().parents[2] / "src" / "popup.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn('text == "Save and quit?"', source)
        self.assertIn('intent=\\"save_quit_confirmation\\"', source)
        self.assertIn('valid_actions=[\\"Y\\"]', source)


if __name__ == "__main__":
    unittest.main()
