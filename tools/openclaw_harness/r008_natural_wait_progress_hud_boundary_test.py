#!/usr/bin/env python3
"""Focused controls for the R-008 natural wait post-load HUD boundary."""

import json
import tempfile
import unittest
from pathlib import Path

from startup_harness import evaluate_screen_text_or_rendered_hud_expectation


SCENARIO_PATH = Path( __file__ ).with_name( "scenarios" ) / \
                "bandit.r008_natural_wait_progress_observation_mcw.json"
HUD_TRACE = (
    "openclaw_harness_ui_trace: component=gameplay_hud event=rendered "
    'run_id="current-run" move_widget=move_count_mode_desc wield_widget=wielding_desc\n'
)


class R008NaturalWaitProgressHudBoundaryTest( unittest.TestCase ):
    def setUp( self ) -> None:
        self.scenario = json.loads( SCENARIO_PATH.read_text( encoding="utf-8" ) )
        self.step = next( step for step in self.scenario["steps"]
                          if step["label"] == "post_load_hud" )

    def evaluate( self, trace: str, lines: list[str] ) -> dict:
        with tempfile.TemporaryDirectory() as temp_dir:
            trace_path = Path( temp_dir ) / "debug.log"
            trace_path.write_text( trace, encoding="utf-8" )
            return evaluate_screen_text_or_rendered_hud_expectation(
                {"ok": True, "lines": lines, "text": "\n".join( lines ), "json_path": "screen.json"},
                self.step["expected_screen_text_after_contains"],
                self.step["rendered_hud_identity"],
                action_trace_log=trace_path,
                run_id="current-run",
                screen_summary={"capture_warnings": []},
                startup_overlay_recovery={"status": "declared_overlay_not_present"},
            )

    def test_current_run_hud_trace_accepts_ocr_loss_of_move_and_wield( self ) -> None:
        self.assertEqual( self.step["expected_screen_text_after_contains"], ["Move:", "Wield:"] )
        self.assertEqual( self.step["rendered_hud_identity"], ["Move:", "Wield:"] )

        result = self.evaluate( HUD_TRACE, ["Activitu: None", "Lighting: bright"] )

        self.assertEqual( result["status"], "matched" )
        self.assertEqual( result["match_provenance"], "native_gameplay_hud_render_trace" )

    def test_foreign_run_hud_trace_fails_closed( self ) -> None:
        result = self.evaluate( HUD_TRACE.replace( "current-run", "previous-run" ),
                                ["Activitu: None", "Lighting: bright"] )

        self.assertEqual( result["status"], "missing" )
        self.assertIn( "wrong_run_hud_trace", result["rendered_hud_identity"]["issues"] )


if __name__ == "__main__":
    unittest.main()
