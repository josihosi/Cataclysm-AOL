#!/usr/bin/env python3
"""Focused controls for warning-only dark Cataclysm feature captures."""

import unittest

from startup_harness import startup_screen_probe_classification


class FeatureCaptureWarningTest( unittest.TestCase ):
    WARNING = "Captured window image appears solid black; target may be occluded, transparent, or non-renderable."

    def classify( self, lines: list[str] ) -> dict:
        return startup_screen_probe_classification(
            ocr_payload={"ok": True, "lines": lines},
            capture_warnings=[self.WARNING],
            debug_delta_text="",
        )

    def test_rendered_hud_pixels_override_bridge_dark_canvas_warning( self ) -> None:
        result = self.classify(["Per: 9", "Activity: None"])

        self.assertTrue(result["raw_black_capture_warning"])
        self.assertTrue(result["captured_hud_evidenced"])
        self.assertFalse(result["black_capture_warning"])
        self.assertEqual(result["classification"], "green_gameplay_hud_present")

    def test_warning_stays_fail_closed_without_hud_pixels( self ) -> None:
        result = self.classify([])

        self.assertTrue(result["raw_black_capture_warning"])
        self.assertFalse(result["captured_hud_evidenced"])
        self.assertTrue(result["black_capture_warning"])
        self.assertEqual(result["classification"], "yellow_gameplay_hud_absent")

    def test_native_render_trace_cannot_override_a_black_capture_warning( self ) -> None:
        result = startup_screen_probe_classification(
            ocr_payload={"ok": True, "lines": []},
            capture_warnings=[self.WARNING],
            debug_delta_text=(
                "openclaw_harness_ui_trace: component=gameplay_hud event=rendered "
                'run_id="run-a" move_widget=move_count_mode_desc wield_widget=wielding_desc\n'
            ),
            gameplay_hud_run_id="run-a",
        )

        self.assertTrue(result["gameplay_hud_present"])
        self.assertFalse(result["captured_hud_evidenced"])
        self.assertTrue(result["black_capture_warning"])


if __name__ == "__main__":
    unittest.main()
