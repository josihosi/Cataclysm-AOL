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
from startup_harness import compact_cockpit_live_evidence, refresh_semantic_step_trace


class R009SemanticChannelCompactionTest( unittest.TestCase ):
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


if __name__ == "__main__":
    unittest.main()
