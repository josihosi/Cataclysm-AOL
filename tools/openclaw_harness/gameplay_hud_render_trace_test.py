#!/usr/bin/env python3
"""Focused controls for the native Move/Wield HUD identity route."""

import tempfile
import unittest
from pathlib import Path

from startup_harness import evaluate_screen_text_or_rendered_hud_expectation


HUD_LINES = ["R ARM", "Per.", "Activity:", "None"]


def screen_text(*lines: str) -> dict:
    return {"ok": True, "lines": list(lines), "text": "\n".join(lines), "json_path": "screen.json"}


def hud_trace(run_id: str, move_widget: str = "move_count_mode_desc",
              wield_widget: str = "wielding_desc") -> str:
    return (
        "openclaw_harness_ui_trace: component=gameplay_hud event=rendered "
        f'run_id="{run_id}" move_widget={move_widget} wield_widget={wield_widget}\n'
    )


class GameplayHudRenderTraceTest(unittest.TestCase):
    def evaluate(self, trace: str, report: dict, trace_start_offset: int = 0) -> dict:
        with tempfile.TemporaryDirectory() as temp_dir:
            trace_path = Path(temp_dir) / "debug.log"
            trace_path.write_text(trace, encoding="utf-8")
            return evaluate_screen_text_or_rendered_hud_expectation(
                report,
                ["Move:", "Wield:"],
                ["Move:", "Wield:"],
                action_trace_log=trace_path,
                run_id="current-run",
                screen_summary={"capture_warnings": []},
                startup_overlay_recovery={"status": "declared_overlay_not_present"},
                trace_start_offset=trace_start_offset,
            )

    def test_native_render_trace_proves_ocr_missing_move_and_wield(self) -> None:
        result = self.evaluate(hud_trace("current-run"), screen_text(*HUD_LINES))

        self.assertEqual(result["status"], "matched")
        self.assertEqual(result["ocr_missing"], ["Move:", "Wield:"])
        self.assertEqual(result["match_provenance"], "native_gameplay_hud_render_trace")

    def test_native_render_trace_handles_an_unreadable_followup_capture(self) -> None:
        result = self.evaluate(hud_trace("current-run"), screen_text())

        self.assertEqual(result["status"], "matched")
        self.assertEqual(result["match_provenance"], "native_gameplay_hud_render_trace")

    def test_absent_trace_cannot_replace_missing_labels(self) -> None:
        result = self.evaluate("", screen_text(*HUD_LINES))

        self.assertEqual(result["status"], "missing")
        self.assertIn("hud_trace_missing", result["rendered_hud_identity"]["issues"])

    def test_stale_or_wrong_run_trace_is_rejected(self) -> None:
        result = self.evaluate(hud_trace("previous-run"), screen_text(*HUD_LINES))

        self.assertEqual(result["status"], "missing")
        self.assertIn("wrong_run_hud_trace", result["rendered_hud_identity"]["issues"])

    def test_wrong_widget_identity_is_rejected(self) -> None:
        result = self.evaluate(
            hud_trace("current-run", move_widget="speed_num"), screen_text(*HUD_LINES)
        )

        self.assertEqual(result["status"], "missing")
        self.assertIn("wrong_move_widget", result["rendered_hud_identity"]["issues"])

    def test_overlay_obscured_hud_is_rejected(self) -> None:
        result = self.evaluate(
            hud_trace("current-run"), screen_text(*HUD_LINES, "Actions")
        )

        self.assertEqual(result["status"], "missing")
        self.assertIn("blocking_overlay_present", result["rendered_hud_identity"]["issues"])

    def test_current_run_trace_outside_former_tail_window_is_read_from_run_boundary(self) -> None:
        prefix = "old debug output\n" * 20000
        result = self.evaluate(
            prefix + hud_trace("current-run"),
            screen_text(*HUD_LINES),
            trace_start_offset=len(prefix.encode("utf-8")),
        )

        self.assertEqual(result["status"], "matched")
        self.assertNotIn("hud_trace_read_truncated", result["rendered_hud_identity"]["issues"])
        self.assertEqual(
            result["rendered_hud_identity"]["observed"]["event_offset"],
            len(prefix.encode("utf-8")),
        )

    def test_trace_before_current_run_boundary_is_not_current_run_evidence(self) -> None:
        prefix = hud_trace("current-run") + ("old debug output\n" * 20000)
        result = self.evaluate(
            prefix,
            screen_text(*HUD_LINES),
            trace_start_offset=len(prefix.encode("utf-8")),
        )

        self.assertEqual(result["status"], "missing")
        self.assertIn("hud_trace_missing", result["rendered_hud_identity"]["issues"])

    def test_current_run_trace_before_feature_action_boundary_remains_usable(self) -> None:
        process_prefix = "old debug output\n" * 20000
        trace = process_prefix + hud_trace("current-run") + "feature action baseline\n"
        result = self.evaluate(
            trace,
            screen_text(*HUD_LINES),
            trace_start_offset=len(process_prefix.encode("utf-8")),
        )

        self.assertEqual(result["status"], "matched")
        self.assertEqual(
            result["rendered_hud_identity"]["observed"]["event_offset"],
            len(process_prefix.encode("utf-8")),
        )

    def test_malformed_or_non_hud_trace_fails_closed(self) -> None:
        result = self.evaluate(
            "malformed gameplay_hud event\n"
            "openclaw_harness_ui_trace: component=other event=rendered "
            'run_id="current-run" move_widget=move_count_mode_desc wield_widget=wielding_desc\n',
            screen_text(*HUD_LINES),
        )

        self.assertEqual(result["status"], "missing")
        self.assertIn("hud_trace_missing", result["rendered_hud_identity"]["issues"])


if __name__ == "__main__":
    unittest.main()
