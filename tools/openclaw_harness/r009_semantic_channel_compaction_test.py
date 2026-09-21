#!/usr/bin/env python3
"""Regression for bounded R-009 semantic frame handoff."""

from __future__ import annotations

import json
import sys
import tempfile
import tracemalloc
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert( 0, str( Path( __file__ ).resolve().parent ) )

from semantic_state import MAX_EVENT_BYTES, MAX_EVENTS, SEMANTIC_STEP_PREFIX, read_semantic_step_trace
from r008_indoor_channel_observation import (
    R008_CHANNEL_RECORD_FILENAME,
    R008_CHANNEL_SCHEMA,
    R008_CHANNELS,
)
from startup_harness import (
    append_semantic_surface_transition_event,
    compact_cockpit_live_evidence,
    current_semantic_step_frame,
    execute_semantic_surface_rejection_matrix,
    refresh_semantic_step_trace,
    read_scheduler_trace_summary,
    semantic_step_source_trace,
)


class R009SemanticChannelCompactionTest( unittest.TestCase ):
    def test_retained_duration_receipt_does_not_overflow_the_bounded_channel(self) -> None:
        """One preserved correlation receipt replaces a stale suffix event."""
        run_id = "r009-duration-receipt-bound"
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "debug.log"
            run_dir = root / "run"
            run_dir.mkdir()
            receipt = {
                "event": "receipt", "run_id": run_id, "frame_id": "duration:0",
                "action_id": "wait.5m", "accepted": True,
            }
            descriptors = [
                {
                    "event": "surface_descriptor", "schema_version": 1, "run_id": run_id,
                    "surface_id": f"surface:{index}", "frame_id": f"frame:{index}",
                    "kind": "world", "breadcrumbs": ["World"], "payload": {},
                    "valid_actions": [],
                }
                for index in range(MAX_EVENTS)
            ]
            source.write_text(
                "".join("native: " + SEMANTIC_STEP_PREFIX + json.dumps(event) + "\n"
                        for event in [receipt, *descriptors]),
                encoding="utf-8",
            )
            with patch("startup_harness.semantic_step_source_trace", return_value=source):
                _, owned = refresh_semantic_step_trace(
                    profile="r009-m095", run_dir=run_dir, run_id=run_id, start_offset=0,
                )
            events, status = read_semantic_step_trace(owned, run_dir, run_id)

        self.assertEqual(status, "ok")
        self.assertEqual(len(events), MAX_EVENTS)
        self.assertEqual(events[0]["action_id"], "wait.5m")
        self.assertEqual(events[-1]["frame_id"], f"frame:{MAX_EVENTS - 1}")

    def test_long_wait_keeps_its_surface_receipt_and_named_successor(self) -> None:
        """A completed native duration remains correlatable after its render tail."""
        run_id = "r009-wait-surface-cluster"
        request_id = "cockpit:wait-cluster"
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "debug.log"
            run_dir = root / "run"
            run_dir.mkdir()
            successor = {
                "event": "surface_descriptor", "schema_version": 1, "run_id": run_id,
                "surface_id": "surface:activity", "frame_id": "frame:activity",
                "kind": "activity_wait", "breadcrumbs": ["Activity in progress"],
                "payload": {}, "valid_actions": [],
            }
            receipt = {
                "event": "surface_receipt", "run_id": run_id, "request_id": request_id,
                "requested_run_id": run_id, "requested_surface_id": "surface:duration",
                "requested_frame_id": "frame:duration", "consuming_surface_id": "surface:duration",
                "consuming_frame_id": "frame:duration", "action_id": "wait.6h",
                "accepted": True, "rejection_reason": "", "resulting_frame_id": "frame:activity",
            }
            tail = [
                {
                    "event": "surface_descriptor", "schema_version": 1, "run_id": run_id,
                    "surface_id": f"surface:tail:{index}", "frame_id": f"frame:tail:{index}",
                    "kind": "world", "breadcrumbs": ["World"], "payload": {}, "valid_actions": [],
                }
                for index in range(MAX_EVENTS)
            ]
            source.write_text(
                "".join("native: " + SEMANTIC_STEP_PREFIX + json.dumps(event) + "\n"
                        for event in [successor, receipt, *tail]),
                encoding="utf-8",
            )
            with patch("startup_harness.semantic_step_source_trace", return_value=source):
                _, owned = refresh_semantic_step_trace(
                    profile="r009-m095", run_dir=run_dir, run_id=run_id, start_offset=0,
                )
            events, status = read_semantic_step_trace(owned, run_dir, run_id)

        self.assertEqual(status, "ok")
        self.assertLessEqual(len(events), MAX_EVENTS)
        self.assertTrue(any(event.get("request_id") == request_id for event in events))
        self.assertTrue(any(event.get("frame_id") == "frame:activity" for event in events))

    def test_run_owned_source_probe_has_constant_peak_for_sixty_action_history( self ) -> None:
        """Selecting the run-owned trace must not load its whole action chain.

        This is the live wait's first per-action operation.  A 60-frame
        history represents exactly one native ``world.pause`` game minute;
        the peak therefore constrains materialized cardinality rather than
        merely checking that references are released later.
        """
        with tempfile.TemporaryDirectory() as temporary:
            root = Path( temporary )
            source = root / "semantic.native.events.jsonl"
            record = (
                SEMANTIC_STEP_PREFIX.encode()
                + b'{"event":"frame","run_id":"peak","payload":"'
                + b"x" * 131072 + b'"}\n'
            )
            source.write_bytes( record * 60 )
            source_bytes = source.stat().st_size
            tracemalloc.start()
            try:
                tracemalloc.reset_peak()
                for _ in range( 60 ):
                    self.assertEqual( semantic_step_source_trace( "r009-m095", root ), source )
                _, peak = tracemalloc.get_traced_memory()
            finally:
                tracemalloc.stop()
        self.assertGreater( source_bytes, 7_000_000 )
        # The fixed probe is 64 KiB.  A megabyte allows interpreter and test
        # bookkeeping while rejecting a whole 60-record history.
        self.assertLess( peak, 1_000_000 )

    def test_repeated_refreshes_keep_source_offsets_and_do_not_rematerialize_history( self ) -> None:
        """A long live watch consumes only each new descriptor/frame pair.

        The registry child used to rediscover every returned raw frame by
        splitting the entire growing native log, and then reprojected the
        complete run to find its immediately preceding descriptor.  The
        result was quadratic allocation before the cockpit envelope or JSONL
        evidence cursors were involved.
        """
        run_id = "r009-incremental-semantic-history"
        with tempfile.TemporaryDirectory() as temporary:
            root = Path( temporary )
            source = root / "semantic.native.events.jsonl"
            source.write_bytes( b"" )
            history = {}
            cursor = 0
            refresh_starts = []
            original_refresh = refresh_semantic_step_trace

            def counted_refresh( **kwargs ):
                refresh_starts.append( kwargs["start_offset"] )
                return original_refresh( **kwargs )

            with patch( "startup_harness.semantic_step_source_trace", return_value=source ), \
                    patch( "startup_harness.refresh_semantic_step_trace", side_effect=counted_refresh ), \
                    patch( "startup_harness.semantic_step_frame_source_offset", side_effect=AssertionError(
                        "source offset must come from the retained event" ) ), \
                    patch( "startup_harness.read_latest_activity_query_trace", return_value=None ), \
                    patch( "startup_harness.read_active_activity_query_trace", return_value=None ):
                for index in range( 160 ):
                    descriptor = {
                        "event": "surface_descriptor", "schema_version": 1,
                        "run_id": run_id, "surface_id": f"surface:{index}",
                        "frame_id": f"descriptor:{index}", "kind": "world",
                        "breadcrumbs": ["World"], "payload": {"owner": "world"},
                        "valid_actions": [],
                    }
                    frame = {
                        "event": "frame", "run_id": run_id,
                        "frame_id": f"frame:{index}", "state": "world",
                        "observed_turn": index, "game_minutes": index,
                        "valid_actions": [], "action_inputs": {},
                        # A realistic repeated render-sized payload makes a
                        # complete-history materialization plainly different
                        # from consuming the next pair.
                        "padding": "x" * 12_000,
                    }
                    descriptor_line = ( SEMANTIC_STEP_PREFIX + json.dumps( descriptor ) + "\n" ).encode()
                    frame_line = ( SEMANTIC_STEP_PREFIX + json.dumps( frame ) + "\n" ).encode()
                    with source.open( "ab" ) as stream:
                        stream.write( descriptor_line )
                        stream.write( frame_line )
                    observed = current_semantic_step_frame(
                        profile="r009-m095", run_dir=root, run_id=run_id,
                        start_offset=cursor, history=history,
                    )
                    self.assertEqual( observed["frame_id"], f"descriptor:{index}" )
                    cursor = int( observed["_event_offset"] ) + len( frame_line )

            self.assertGreater( source.stat().st_size, 1_000_000 )
            self.assertEqual( refresh_starts[0], 0 )
            self.assertTrue( all( offset > 0 for offset in refresh_starts[1:] ) )
            self.assertEqual( len( refresh_starts ), 160 )

    def test_live_evidence_cursor_scans_large_history_once_then_only_the_delta(self) -> None:
        run_id = "r009-indexed-evidence"
        with tempfile.TemporaryDirectory() as temporary:
            root = Path( temporary )
            receipts = root / "semantic.steps.jsonl"
            transitions = root / "transition.events.jsonl"
            rows = [
                {"run_id": run_id, "action_id": "world.wait", "accepted": index != 0,
                 "frame_id": f"frame:{index}"}
                for index in range(5000)
            ]
            receipts.write_text(
                "".join(json.dumps(row, separators=(",", ":")) + "\n" for row in rows),
                encoding="utf-8",
            )
            transitions.write_text(
                "".join(json.dumps({"run_id": run_id, "sequence": index,
                                    "kind": "native_surface_receipt",
                                    "outcome": "accepted"}) + "\n"
                         for index in range(1, 5001)),
                encoding="utf-8",
            )
            cursor = {}
            first = compact_cockpit_live_evidence(root, run_id, cursor=cursor)
            receipt_reader = cursor["receipts"]
            transition_reader = cursor["transitions"]
            first_scan = receipt_reader.bytes_scanned + transition_reader.bytes_scanned
            first_size = receipts.stat().st_size + transitions.stat().st_size
            self.assertEqual(first["receipt_count"], 5000)
            self.assertEqual(first["first_divergence"]["kind"], "rejected_native_receipt")
            self.assertEqual(first["latest_transition"]["sequence"], 5000)
            self.assertEqual(first_scan, first_size)

            with receipts.open("a", encoding="utf-8") as stream:
                stream.write(
                    json.dumps({"run_id": run_id, "action_id": "world.look", "accepted": True}) + "\n"
                )
            append_semantic_surface_transition_event(
                root, run_id,
                {"request_id": "request:5001", "action_id": "world.look"},
                {"accepted": True, "game_minutes": 12, "resulting_frame_id": "frame:5001"},
            )
            second = compact_cockpit_live_evidence(root, run_id, cursor=cursor)
            second_scan = receipt_reader.bytes_scanned + transition_reader.bytes_scanned
            delta = receipts.stat().st_size + transitions.stat().st_size - first_size
            self.assertEqual(second["receipt_count"], 5001)
            self.assertEqual(second["latest_receipt"]["action_id"], "world.look")
            self.assertEqual(second["latest_transition"]["sequence"], 5001)
            self.assertEqual(second_scan - first_scan, delta)
            self.assertLessEqual(second_scan, first_size + delta)

    def test_scheduler_projection_is_tail_bounded_for_repeated_live_actions(self) -> None:
        """Live observations never copy an unbounded native debug log tail.

        The full debug log stays on disk as the native evidence source.  This
        asserts the per-action public projection is bounded, including after
        the log has grown beyond the read window.
        """
        run_id = "r009-repeated-scheduler-actions"
        record = (
            "openclaw_harness_scheduler_trace: component=global_eoc "
            f"run_id={run_id} eoc=EOC_OPENCLAW_R019_SAFE_POPUP "
            "due_turn={turn} current_turn={turn} decision=due outcome=consumed\n"
        )
        with tempfile.TemporaryDirectory() as temporary:
            profile_dir = Path(temporary)
            log_path = profile_dir / "debug.log"
            log_path.write_text(
                "unrelated-native-output " * 100_000 + "\n" +
                "".join(record.format(turn=index) for index in range(4_000)),
                encoding="utf-8",
            )
            with patch("startup_harness.config_dir_for_profile", return_value=profile_dir):
                summary = read_scheduler_trace_summary("unused", run_id)
                evidence = compact_cockpit_live_evidence(
                    profile_dir, run_id, profile="unused", cursor={}
                )

        self.assertGreater(summary["total"], 32)
        self.assertTrue(summary["truncated"])
        self.assertLessEqual(len(summary["records"]), 32)
        self.assertEqual(evidence["scheduler_trace_total"], summary["total"])
        self.assertTrue(evidence["scheduler_trace_truncated"])
        self.assertLessEqual(len(evidence["scheduler_trace"]), 32)

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

    def test_live_channel_evidence_uses_the_live_bridge_binding( self ) -> None:
        run_id = "r009-live-bridge"
        bridge_binding = "bridge-binding-a"
        with tempfile.TemporaryDirectory() as temporary:
            root = Path( temporary )
            ( root / "runtime.binding.json" ).write_text( json.dumps( {
                "runtime_source_sha256": "source-a", "executable_sha256": "exe-a",
            } ), encoding="utf-8" )
            ( root / "contract.preflight.json" ).write_text(
                json.dumps( {"scenario": "scenario-a"} ), encoding="utf-8"
            )
            rows = [
                {
                    "schema": R008_CHANNEL_SCHEMA, "sequence": sequence, "run_id": run_id,
                    "scan_id": run_id + ":1",
                    "binding": {
                        "runtime_source_sha256": "source-a", "executable_sha256": "exe-a",
                        "scenario_id": "scenario-a", "binding_id": bridge_binding,
                    },
                    "scan": {"game_minutes": 100, "fresh": True, "isolated": True},
                    "channel": channel, "signal_origin": "none",
                    "consumer": "bandit_live_world.signal_scan", "observed": False,
                    "isolated": True,
                }
                for sequence, channel in enumerate( R008_CHANNELS, 1 )
            ]
            ( root / R008_CHANNEL_RECORD_FILENAME ).write_text(
                "".join( json.dumps( row ) + "\n" for row in rows ), encoding="utf-8"
            )

            evidence = compact_cockpit_live_evidence(
                root, run_id, binding_id=bridge_binding,
            )

        self.assertTrue( evidence["production_channel_observation"]["eligible"] )

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

    def test_partial_or_oversized_native_json_is_ignored_without_decoder_stall( self ) -> None:
        run_id = "r009-bounded-json"
        valid = {
            "event": "surface_descriptor",
            "schema_version": 1,
            "run_id": run_id,
            "surface_id": "surface-world",
            "frame_id": "frame-world",
            "kind": "world",
            "breadcrumbs": ["World"],
            "payload": {},
            "valid_actions": [],
        }
        oversized = {
            "event": "frame", "run_id": run_id, "frame_id": "frame-large",
            "state": "world", "valid_actions": [],
            "padding": "x" * 20000,
        }
        partial = (
            SEMANTIC_STEP_PREFIX
            + '{"event":"frame","run_id":"' + run_id
            + '","frame_id":"frame-partial","padding":"'
            + "x" * 20000
        ).encode( "utf-8" )
        with tempfile.TemporaryDirectory() as temporary:
            root = Path( temporary )
            source = root / "debug.log"
            run_dir = root / "run"
            run_dir.mkdir()
            source.write_bytes(
                (SEMANTIC_STEP_PREFIX + json.dumps(oversized) + "\n"
                 + SEMANTIC_STEP_PREFIX + json.dumps(valid) + "\n").encode("utf-8")
                + partial
            )
            with patch( "startup_harness.semantic_step_source_trace", return_value=source ), \
                    patch( "startup_harness.SEMANTIC_STEP_MAX_BYTES", 1024 ):
                _, owned = refresh_semantic_step_trace(
                    profile="r009-m095", run_dir=run_dir, run_id=run_id, start_offset=0,
                )

            events, status = read_semantic_step_trace( owned, run_dir, run_id )
        self.assertEqual( status, "ok" )
        self.assertEqual( [event["frame_id"] for event in events], ["frame-world"] )

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
