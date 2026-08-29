#!/usr/bin/env python3
"""Focused fail-closed controls for R-008's semantic startup HUD gate."""

import json
from pathlib import Path
import unittest

from startup_harness import evaluate_bound_startup_gameplay_hud_verdict


class R008StartupSemanticHudGateTest( unittest.TestCase ):
    def inputs( self ) -> tuple[ dict, dict, dict, dict, str, dict, str ]:
        return (
            {"classification": "green_gameplay_hud_present"},
            {"classification": "green_gameplay_hud_present", "gameplay_hud_present": True,
             "visible_error_popup": False, "startup_error_logged": False,
             "blocking_overlay_present": False, "native_run_id": "native-run"},
            {"ok": True, "schema": 1, "executable_sha256": "executable-hash",
             "runtime_source_sha256": "runtime-source-hash"},
            {"runtime_binding_status": "matched", "runtime_binding_observed": {
                "executable_sha256": "executable-hash",
                "runtime_source_sha256": "runtime-source-hash"},
             "capture_process_pid": 2468},
            "native-run",
            {"authority": "registry", "authority_id": "authority-id",
             "binding_id": "executable-hash", "run_id": "registry-run",
             "source_sha256": "scenario-source-hash"},
            "scenario-source-hash",
        )

    def evaluate( self, *, probe=None, binding=None, screen=None, native_run=None,
                  authority=None, source=None, expected_process_pid=2468,
                  previous_process_pid=1357 ) -> dict:
        expected, default_probe, default_binding, default_screen, default_run, default_authority, default_source = self.inputs()
        return evaluate_bound_startup_gameplay_hud_verdict(
            expected, default_probe if probe is None else probe,
            default_binding if binding is None else binding,
            default_screen if screen is None else screen,
            semantic_run_id=default_run if native_run is None else native_run,
            registry_authority=default_authority if authority is None else authority,
            scenario_source_sha256=default_source if source is None else source,
            expected_process_pid=expected_process_pid,
            previous_process_pid=previous_process_pid,
        )

    def test_accepts_green_current_bound_semantic_verdict( self ) -> None:
        self.assertEqual( self.evaluate()["status"], "green" )

    def test_missing_stale_wrong_run_wrong_executable_nongameplay_and_contradiction_fail_closed( self ) -> None:
        _, probe, binding, screen, _, authority, _ = self.inputs()
        cases = (
            ({"probe": {}}, "missing_startup_semantic_verdict"),
            ({"probe": {**probe, "native_run_id": "old-native-run"}}, "wrong_or_missing_native_run_binding"),
            ({"probe": {**probe, "classification": "yellow_gameplay_hud_absent", "gameplay_hud_present": False}}, "nongameplay_or_contradictory_startup_verdict"),
            ({"screen": {**screen, "runtime_binding_status": "mismatch"}}, "stale_or_wrong_executable_binding"),
            ({"screen": {**screen, "capture_process_pid": 1357}}, "wrong_or_missing_captured_process_binding"),
            ({"source": "other-scenario-source"}, "stale_or_wrong_scenario_source_binding"),
            ({"probe": {**probe, "visible_error_popup": True}}, "contradictory_startup_verdict"),
            ({"probe": {**probe, "blocking_overlay_present": True}}, "contradictory_startup_verdict"),
        )
        for arguments, expected_issue in cases:
            with self.subTest( expected_issue=expected_issue ):
                result = self.evaluate( **arguments )
                self.assertEqual( result["status"], "red" )
                self.assertIn( expected_issue, result["issues"] )

    def test_missing_or_reused_process_fails_closed( self ) -> None:
        for arguments, expected_issue in (
            ({"expected_process_pid": 0}, "wrong_or_missing_captured_process_binding"),
            ({"previous_process_pid": 2468}, "post_relaunch_process_was_not_replaced"),
        ):
            with self.subTest( expected_issue=expected_issue ):
                result = self.evaluate( **arguments )
                self.assertEqual( result["status"], "red" )
                self.assertIn( expected_issue, result["issues"] )

    def test_post_relaunch_step_uses_semantic_authority_not_ocr( self ) -> None:
        manifest_path = Path( __file__ ).parent / "scenarios" / \
                        "bandit.r008_natural_safe_watch_validation_mcw.json"
        manifest = json.loads( manifest_path.read_text( encoding="utf-8" ) )
        steps = manifest["post_relaunch"]["steps"]
        step = next( item for item in steps
                     if item["label"] == "post_local_contact_save_reload_hud" )

        self.assertEqual(
            step["startup_semantic_hud_expectation"]["classification"],
            "green_gameplay_hud_present",
        )
        self.assertNotIn( "expected_screen_text_after_contains", step )
        self.assertNotIn( "abort_on_screen_text_expectation_failure", step )

    def test_missing_or_wrong_registry_binding_fails_closed( self ) -> None:
        _, _, _, _, _, authority, _ = self.inputs()
        for changed_authority, expected_issue in (
            ({}, "missing_registry_authority"),
            ({**authority, "binding_id": "other-executable"}, "wrong_registry_executable_binding"),
        ):
            with self.subTest( expected_issue=expected_issue ):
                result = self.evaluate( authority=changed_authority )
                self.assertEqual( result["status"], "red" )
                self.assertIn( expected_issue, result["issues"] )


if __name__ == "__main__":
    unittest.main()
