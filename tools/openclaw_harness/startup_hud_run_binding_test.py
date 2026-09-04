#!/usr/bin/env python3
"""Regression checks for startup's bound native HUD fallback."""

import tempfile
import unittest
from pathlib import Path
from unittest import mock

import startup_harness
from startup_harness import semantic_run_binding_child_environment, startup_screen_probe_classification


def trace( run_id: str ) -> str:
    return (
        "openclaw_harness_ui_trace: component=gameplay_hud event=rendered "
        f'run_id="{run_id}" move_widget=move_count_mode_desc wield_widget=wielding_desc'
    )


class StartupHudRunBindingTest( unittest.TestCase ):
    def probe( self, trace_text: str, run_id: str, lines: list[str] = None ) -> dict:
        return startup_screen_probe_classification(
            ocr_payload={"ok": True, "lines": lines if lines is not None else ["Activitu: None", "Lighting: bright"]},
            capture_warnings=[],
            debug_delta_text=trace_text,
            gameplay_hud_run_id=run_id,
        )

    def test_current_run_native_hud_covers_unreadable_hud_capture( self ) -> None:
        result = self.probe( trace( "current-run" ), "current-run", ["waxing crescent"] )

        self.assertTrue( result["gameplay_hud_present"] )
        self.assertIn( "native_rendered_hud_fallback", result["hud_body_marker_types"] )
        self.assertIn( "native_rendered_hud_fallback", result["hud_status_marker_types"] )

    def test_foreign_run_trace_cannot_cover_lost_body_labels( self ) -> None:
        result = self.probe( trace( "previous-run" ), "current-run" )

        self.assertFalse( result["gameplay_hud_present"] )
        self.assertNotIn( "native_rendered_hud_fallback", result["hud_body_marker_types"] )

    def test_wrong_native_widget_cannot_cover_lost_body_labels( self ) -> None:
        result = self.probe(
            trace( "current-run" ).replace( "move_count_mode_desc", "speed_num" ), "current-run"
        )

        self.assertFalse( result["gameplay_hud_present"] )

    def test_launch_binding_requires_one_nonempty_current_run_identity( self ) -> None:
        self.assertEqual(
            semantic_run_binding_child_environment("current-run"),
            {"OPENCLAW_HARNESS_SEMANTIC_RUN_ID": "current-run"},
        )
        with self.assertRaisesRegex(ValueError, "bound harness run ID"):
            semantic_run_binding_child_environment(" ")

    def test_launch_overwrites_a_stale_semantic_binding_with_its_current_run( self ) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            run_dir = Path(temp_dir)
            with mock.patch.object(startup_harness, "detect_executable", return_value=Path("game")), \
                    mock.patch.object(startup_harness, "build_game_command", return_value=["game"]), \
                    mock.patch.object(startup_harness.subprocess, "Popen") as popen:
                startup_harness.launch_game(
                    "profile", "world", run_dir,
                    child_environment={"OPENCLAW_HARNESS_SEMANTIC_RUN_ID": "prior-run"},
                    transition_event_run_id="current-run",
                )

            child_environment = popen.call_args.kwargs["env"]
            self.assertEqual(child_environment["OPENCLAW_HARNESS_RUN_ID"], "current-run")
            self.assertEqual(child_environment["OPENCLAW_HARNESS_SEMANTIC_RUN_ID"], "current-run")
            self.assertTrue(popen.call_args.kwargs["start_new_session"])

    def test_prepared_certification_context_survives_to_the_run_bound_native_writer( self ) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            run_dir = Path(temp_dir)
            sidecar = run_dir / "transition.events.jsonl"
            prepared = '{"binding_id":"sealed-binding","round_id":"round-a"}'
            with mock.patch.object(startup_harness, "detect_executable", return_value=Path("game")), \
                    mock.patch.object(startup_harness, "build_game_command", return_value=["game"]), \
                    mock.patch.object(startup_harness.subprocess, "Popen") as popen:
                startup_harness.launch_game(
                    "profile", "world", run_dir,
                    child_environment={
                        "OPENCLAW_CERTIFICATION_PREPARED_ROUND": prepared,
                        "OPENCLAW_HARNESS_BINDING_ID": "sealed-binding",
                    },
                    transition_event_run_id="run-bound-sidecar",
                    transition_event_path=str(sidecar),
                )

            child_environment = popen.call_args.kwargs["env"]
            self.assertEqual(child_environment["OPENCLAW_CERTIFICATION_PREPARED_ROUND"], prepared)
            self.assertEqual(child_environment["OPENCLAW_HARNESS_BINDING_ID"], "sealed-binding")
            self.assertEqual(child_environment["OPENCLAW_HARNESS_RUN_ID"], "run-bound-sidecar")
            self.assertEqual(child_environment["OPENCLAW_HARNESS_TRANSITION_EVENT_PATH"], str(sidecar.resolve()))

    def test_native_producer_requires_matching_launch_binding_and_resets_on_change( self ) -> None:
        source = (Path(__file__).resolve().parents[2] / "src" / "handle_action.cpp").read_text()

        self.assertIn('std::getenv( "OPENCLAW_HARNESS_SEMANTIC_RUN_ID" )', source)
        self.assertIn("std::strcmp( active_run_id, bound_run_id ) != 0", source)
        self.assertIn("if( bound_run_id != active_run_id )", source)
        self.assertIn("emitted = false;", source)


if __name__ == "__main__":
    unittest.main()
