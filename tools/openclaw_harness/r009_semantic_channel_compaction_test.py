#!/usr/bin/env python3
"""Regression for bounded R-009 semantic frame handoff."""

from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert( 0, str( Path( __file__ ).resolve().parent ) )

from semantic_state import MAX_EVENT_BYTES, SEMANTIC_STEP_PREFIX, read_semantic_step_trace
from startup_harness import (
    compact_cockpit_live_evidence,
    execute_semantic_surface_rejection_matrix,
    refresh_semantic_step_trace,
)


class R009SemanticChannelCompactionTest( unittest.TestCase ):
    def test_actionless_unsupported_surface_stops_without_a_request( self ) -> None:
        descriptor = {
            "event": "surface_descriptor",
            "run_id": "r009-actionless-stop",
            "surface_id": "surface:debug-console",
            "frame_id": "frame:debug-console",
            "kind": "unsupported",
            "breadcrumbs": ["World", "Unsupported input owner: DEBUG_CONSOLE"],
            "payload": {"owner": "DEBUG_CONSOLE"},
            "valid_actions": [],
        }
        with tempfile.TemporaryDirectory() as temporary, \
                patch( "startup_harness.refresh_semantic_step_trace", return_value=( Path( temporary ), Path( temporary ) ) ), \
                patch( "startup_harness.read_semantic_step_trace", return_value=( [descriptor], "ok" ) ), \
                patch( "startup_harness.submit_semantic_surface_probe_request" ) as submit:
            result = execute_semantic_surface_rejection_matrix(
                profile="r009", run_dir=Path( temporary ), run_id="r009-actionless-stop",
                trace_start_offset=0, pid=73, session_id="session", action_id="world.debug_menu",
                timeout_seconds=1.0, poll_seconds=0.01, expect_actionless_stop=True,
            )
        self.assertTrue( result["ok"] )
        self.assertEqual( result["mode"], "actionless_stop" )
        self.assertEqual( result["action_submission"], "not_attempted" )
        self.assertEqual( result["requests_submitted"], 0 )
        submit.assert_not_called()

    def test_live_evidence_keeps_direct_child_resource_fields( self ) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            sample = {
                "pid": 73,
                "platform": "macos",
                "cpu_percent": {"status": "available", "value": 2.5},
                "resident_memory": {"status": "available", "value": 4096},
            }
            with patch( "startup_harness.sample_child_resources", return_value=sample ):
                evidence = compact_cockpit_live_evidence(
                    Path( temporary ), "r009-current-macos", profile="r009-m095", pid=73,
                )
        self.assertEqual( evidence["child_resources"], sample )

    def test_full_native_render_maps_do_not_overflow_the_semantic_action_channel( self ) -> None:
        run_id = "r009-current-macos"
        event = {
            "event": "frame",
            "run_id": run_id,
            "frame_id": f"{run_id}:frame:1",
            "state": "world",
            "observed_turn": 42,
            "game_minutes": 600,
            "producer": "native_world_frame",
            "initial_world_ready": True,
            "keep_watch_safety": {"classification": "safe"},
            "valid_actions": ["wait.duration_menu"],
            "action_inputs": {"wait.duration_menu": "5"},
            "observation": {
                "schema": "caol-avatar-visible-v1",
                "avatar": {"name": "Witness"},
                "visible_local": [{"id": "tile:0:0", "terrain": "t_floor"}],
                "visible_entities": [],
                "visible_zones": [],
                "minimap": {
                    "schema": "caol-native-minimap-v1", "radius": 12,
                    "cells": [{"dx": index, "dy": 0, "terrain": "t_floor"}
                              for index in range( 30000 )],
                },
                "overmap": {
                    "schema": "caol-avatar-overmap-v1", "radius": 180,
                    "cells": [{"dx": index, "dy": 0, "terrain": "field"}
                              for index in range( 30000 )],
                },
            },
        }
        with tempfile.TemporaryDirectory() as temporary:
            root = Path( temporary )
            source = root / "debug.log"
            run_dir = root / "run"
            run_dir.mkdir()
            source.write_text(
                "native: " + SEMANTIC_STEP_PREFIX + json.dumps( event ) + "\n",
                encoding="utf-8",
            )
            with patch( "startup_harness.semantic_step_source_trace", return_value=source ):
                _, owned = refresh_semantic_step_trace(
                    profile="r009-m095", run_dir=run_dir, run_id=run_id, start_offset=0,
                )

            self.assertLessEqual( owned.stat().st_size, MAX_EVENT_BYTES )
            events, status = read_semantic_step_trace( owned, run_dir, run_id )
            self.assertEqual( status, "ok" )
            self.assertEqual( len( events ), 1 )
            observation = events[0]["observation"]
            self.assertEqual( observation["avatar"]["name"], "Witness" )
            self.assertEqual( observation["visible_local"][0]["terrain"], "t_floor" )
            self.assertEqual( observation["minimap"]["radius"], 12 )
            self.assertNotIn( "cells", observation["minimap"] )
            self.assertNotIn( "overmap", observation )

    def test_historical_surface_actions_do_not_block_a_current_child_frame( self ) -> None:
        run_id = "r009-current-child"
        actions = [
            {
                "id": "inventory.item_menu.choose",
                "stable_id": f"action:{index}",
                "label": "derived action " + "x" * 120,
                "enabled": True,
            }
            for index in range( 1000 )
        ]
        descriptors = [
            {
                "event": "surface_descriptor",
                "schema_version": 1,
                "run_id": run_id,
                "surface_id": f"surface:{index}",
                "frame_id": f"frame:{index}",
                "kind": "inventory_item_menu",
                "breadcrumbs": ["World", "rock"],
                "payload": {"title": "rock"},
                "valid_actions": actions,
            }
            for index in range( 3 )
        ]
        with tempfile.TemporaryDirectory() as temporary:
            root = Path( temporary )
            source = root / "debug.log"
            run_dir = root / "run"
            run_dir.mkdir()
            source.write_text(
                "".join(
                    SEMANTIC_STEP_PREFIX + json.dumps( descriptor ) + "\n"
                    for descriptor in descriptors
                ), encoding="utf-8",
            )
            with patch( "startup_harness.semantic_step_source_trace", return_value=source ):
                _, owned = refresh_semantic_step_trace(
                    profile="r009-m095", run_dir=run_dir, run_id=run_id, start_offset=0,
                )

            events, status = read_semantic_step_trace( owned, run_dir, run_id )
            self.assertEqual( status, "ok" )
            self.assertEqual( [len( event["valid_actions"] ) for event in events], [0, 0, 1000] )


if __name__ == "__main__":
    unittest.main()
