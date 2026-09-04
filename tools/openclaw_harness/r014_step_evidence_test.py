#!/usr/bin/env python3
"""Focused R-014 immediate evidence binding contracts."""

from __future__ import annotations

import copy
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


HARNESS_DIR = Path( __file__ ).resolve().parent
sys.path.insert( 0, str( HARNESS_DIR ) )

import startup_harness  # noqa: E402


class R014StepEvidenceTest( unittest.TestCase ):
    def test_bootstrap_requires_fresh_same_run_complete_native_frame( self ) -> None:
        frame = {
            "run_id": "run-a",
            "frame_id": "run-a:2",
            "state": "world",
            "valid_actions": ["world.wait"],
            "producer": "hud_world_ready",
            "initial_world_ready": True,
            "_event_offset": 42,
        }
        with tempfile.TemporaryDirectory() as directory, mock.patch.object(
            startup_harness, "current_semantic_step_frame", return_value=frame,
        ):
            root = Path( directory )
            green = startup_harness.r014_native_semantic_bootstrap_metadata(
                profile="test", run_dir=root, run_id="run-a", start_offset=0,
                press_trace_offset=41, required_state="world", required_actions=["world.wait"],
            )
            self.assertEqual( green["status"], "required_state_present" )
            for changed in (
                {"run_id": "run-b"}, {"_event_offset": 40}, {"valid_actions": []},
                {"producer": "player_input"}, {"initial_world_ready": False},
            ):
                invalid = copy.deepcopy( frame )
                invalid.update( changed )
                with mock.patch.object( startup_harness, "current_semantic_step_frame", return_value=invalid ):
                    yellow = startup_harness.r014_native_semantic_bootstrap_metadata(
                        profile="test", run_dir=root, run_id="run-a", start_offset=0,
                        press_trace_offset=41, required_state="world", required_actions=["world.wait"],
                    )
                self.assertEqual( yellow["status"], "scanned" )
                self.assertTrue( startup_harness.metadata_checkpoint_verdict( yellow )[0].startswith( "yellow" ) )

    def test_launcher_bootstrap_accepts_only_the_same_run_world_frame( self ) -> None:
        frame = {
            "run_id": "run-a",
            "frame_id": "run-a:1",
            "state": "world",
            "valid_actions": ["world.wait"],
            "producer": "hud_world_ready",
            "initial_world_ready": True,
            "_event_offset": 1,
        }
        with tempfile.TemporaryDirectory() as directory, mock.patch.object(
            startup_harness, "current_semantic_step_frame", return_value=frame,
        ):
            metadata = startup_harness.await_r014_native_semantic_bootstrap(
                profile="test", run_dir=Path( directory ), run_id="run-a",
                required_state="world", required_actions=["world.wait"],
                timeout_seconds=0.0, poll_seconds=0.0,
            )
        self.assertEqual( metadata["status"], "required_state_present" )
        self.assertEqual( metadata["bootstrap"], "launcher_first_same_run_semantic_frame" )

    def test_bootstrap_accepts_source_bound_world_surface_descriptor( self ) -> None:
        descriptor = {
            "event": "surface_descriptor",
            "schema_version": 1,
            "run_id": "run-a",
            "surface_id": "run-a:surface:1",
            "frame_id": "run-a:frame:1",
            "kind": "world",
            "breadcrumbs": ["World"],
            "payload": {},
            "valid_actions": [{
                "id": "world.inventory", "stable_id": "", "label": "world.inventory",
                "enabled": True,
            }],
            "_event_offset": 42,
        }
        with tempfile.TemporaryDirectory() as directory, mock.patch.object(
            startup_harness, "current_semantic_step_frame", return_value=descriptor,
        ):
            metadata = startup_harness.r014_native_semantic_bootstrap_metadata(
                profile="test", run_dir=Path( directory ), run_id="run-a", start_offset=0,
                press_trace_offset=41, required_state="world", required_actions=["world.inventory"],
            )
        self.assertEqual( metadata["status"], "required_state_present" )
        self.assertEqual( metadata["frame_event"], "surface_descriptor" )

    def test_launcher_bootstrap_fails_closed_for_wrong_run_or_absent_frame( self ) -> None:
        wrong_run = {
            "run_id": "run-b",
            "frame_id": "run-b:1",
            "state": "world",
            "valid_actions": ["world.wait"],
            "producer": "hud_world_ready",
            "initial_world_ready": True,
            "_event_offset": 1,
        }
        with tempfile.TemporaryDirectory() as directory:
            root = Path( directory )
            for result in ( wrong_run, ValueError( "absent" ) ):
                with mock.patch.object(
                    startup_harness, "current_semantic_step_frame",
                    side_effect=result if isinstance( result, Exception ) else None,
                    return_value=None if isinstance( result, Exception ) else result,
                ):
                    metadata = startup_harness.await_r014_native_semantic_bootstrap(
                        profile="test", run_dir=root, run_id="run-a",
                        required_state="world", required_actions=["world.wait"],
                        timeout_seconds=0.0, poll_seconds=0.0,
                    )
                self.assertEqual( metadata["status"], "scanned" )
                self.assertEqual( metadata["reason"], "first_same_run_semantic_frame_timeout" )

    def test_observation_requires_current_run_and_visible_native_result( self ) -> None:
        observation = {
            "ok": True,
            "result": {
                "run_id": "run-a", "observation_id": "run-a:3",
                "visible_local": [{"handle": "visible:run-a:1", "terrain": "floor"}],
            },
        }
        with tempfile.TemporaryDirectory() as directory:
            artifact = Path( directory ) / "observe.json"
            artifact.write_text( "{}", encoding="utf-8" )
            green = startup_harness.r014_cockpit_observation_metadata(
                observation, artifact_path=artifact, run_id="run-a",
            )
            self.assertEqual( green["status"], "required_state_present" )
            for changed in (
                {"run_id": "run-b"}, {"visible_local": []},
            ):
                invalid = copy.deepcopy( observation )
                invalid["result"].update( changed )
                yellow = startup_harness.r014_cockpit_observation_metadata(
                    invalid, artifact_path=artifact, run_id="run-a",
                )
                self.assertEqual( yellow["status"], "scanned" )
                self.assertTrue( startup_harness.metadata_checkpoint_verdict( yellow )[0].startswith( "yellow" ) )

    def test_live_session_requires_same_run_bound_complete_final( self ) -> None:
        final = {
            "run_id": "run-a", "binding_id": "binding-a", "state": "finished",
            "action_observation_sequence": [
                {"kind": "observation"},
                {"kind": "action", "result": {"ok": True}},
                {"kind": "observation"},
            ],
        }
        with tempfile.TemporaryDirectory() as directory:
            artifact = Path( directory ) / "cockpit.live.final.json"
            artifact.write_text( "{}", encoding="utf-8" )
            green = startup_harness.r014_cockpit_live_session_metadata(
                final, artifact_path=artifact, run_id="run-a", binding_id="binding-a", live_status=0,
            )
            self.assertEqual( green["status"], "required_state_present" )
            for changed in (
                {"run_id": "run-b"}, {"state": "active"},
                {"action_observation_sequence": [{"kind": "observation"}]},
            ):
                invalid = copy.deepcopy( final )
                invalid.update( changed )
                yellow = startup_harness.r014_cockpit_live_session_metadata(
                    invalid, artifact_path=artifact, run_id="run-a", binding_id="binding-a", live_status=0,
                )
                self.assertEqual( yellow["status"], "scanned" )
                self.assertTrue( startup_harness.metadata_checkpoint_verdict( yellow )[0].startswith( "yellow" ) )

    def test_declared_live_steps_own_distinct_terminal_artifacts( self ) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path( directory )
            crossing = startup_harness.cockpit_live_final_path(
                root, "observe_cannibal_roof_fire_lifecycle",
            )
            persisted = startup_harness.cockpit_live_final_path(
                root, "observe_persisted_cannibal_return_lifecycle",
            )
            self.assertNotEqual( crossing, persisted )
            self.assertEqual(
                startup_harness.cockpit_live_final_path( root ).name,
                "cockpit.live.final.json",
            )
            first = startup_harness.finalize_cockpit_live_session(
                root, 1, {"run_id": "run-a", "state": "finished"},
                cleanup_process=False, final_path=crossing,
            )
            second = startup_harness.finalize_cockpit_live_session(
                root, 1, {"run_id": "run-a", "state": "finished"},
                cleanup_process=False, final_path=persisted,
            )
            self.assertEqual( first["final_report_ref"], crossing.name )
            self.assertEqual( second["final_report_ref"], persisted.name )
            self.assertTrue( crossing.is_file() )
            self.assertTrue( persisted.is_file() )


if __name__ == "__main__":
    unittest.main()
