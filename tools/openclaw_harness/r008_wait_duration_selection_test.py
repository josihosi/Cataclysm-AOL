#!/usr/bin/env python3
"""Fail-closed controls for R-008's semantic/native duration receipt."""

import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert( 0, str( Path( __file__ ).resolve().parent ) )

import startup_harness


class R008WaitDurationSelectionTest( unittest.TestCase ):
    run_id = "r008-duration-run"

    def semantic_events( self, *, action="wait.6h", accepted=True,
                         state="wait_duration_choice", duplicate=False ):
        frame = {
            "event": "frame", "run_id": self.run_id, "frame_id": "duration-frame",
            "state": state, "valid_actions": ["wait.1h", "wait.6h"],
            "action_inputs": {"wait.1h": "5", "wait.6h": "8"},
        }
        receipt = {
            "event": "receipt", "run_id": self.run_id, "frame_id": "duration-frame",
            "action_id": action, "accepted": accepted,
        }
        result = [frame, receipt]
        if duplicate:
            result.append( dict( receipt ) )
        return result

    def check( self, root, events, selection, *, binding_id, dispatch="wait_dispatched" ):
        trace = root / "debug.log"
        trace.write_text( selection, encoding="utf-8" )
        owned = root / "semantic.steps.native.log"
        owned.write_text( "".join(
            "openclaw_harness_semantic_step: " + json.dumps( event ) + "\n"
            for event in events
        ), encoding="utf-8" )
        with patch.object( startup_harness, "semantic_step_source_trace", return_value=trace ), \
                patch.object( startup_harness, "refresh_semantic_step_trace", return_value=(trace, owned)):
            return startup_harness.validate_native_wait_duration_selection(
                profile="r008", run_dir=root, run_id=self.run_id, start_offset=0,
                selection_trace_log=trace, selection_trace_start_offset=0,
                binding_id=binding_id, expected_action="wait.6h", expected_binding="8",
                wait_dispatch={"status": dispatch}, require_native_selection=True,
            )

    def test_accepts_same_run_advertised_six_hour_selection_without_ocr( self ):
        with tempfile.TemporaryDirectory() as temp:
            root = Path( temp )
            binding = root / startup_harness.TRANSITION_EVENT_BINDING_FILENAME
            binding.write_text( '{"run_id":"r008-duration-run"}', encoding="utf-8" )
            binding_id, _ = startup_harness.sha256_file( binding )
            result = self.check(
                root, self.semantic_events(),
                'openclaw_harness_wait_input_trace: component=wait_menu event=selection '
                'run_id="r008-duration-run" action_id="wait.6h" accepted=yes\n',
                binding_id=binding_id,
            )
        self.assertEqual( result["status"], "matched" )
        self.assertTrue( result["ocr_diagnostic_only"] )

    def test_rejects_missing_dispatch_wrong_menu_wrong_duration_and_duplicate_receipts( self ):
        cases = {
            "missing_dispatch": (self.semantic_events(), "wait.6h", "missing_wait_dispatch"),
            "wrong_menu": (self.semantic_events( state="alarm_duration_choice" ), "wait.6h",
                           "missing_or_duplicate_duration_menu"),
            "wrong_duration": (self.semantic_events( action="wait.1h" ), "wait.1h",
                               "semantic_selection_rejected_or_wrong_duration"),
            "duplicate_semantic": (self.semantic_events( duplicate=True ), "wait.6h",
                                   "missing_or_duplicate_semantic_selection_receipt"),
            "duplicate_native": (self.semantic_events(), "wait.6h\n"
                                 'openclaw_harness_wait_input_trace: component=wait_menu event=selection '
                                 'run_id="r008-duration-run" action_id="wait.6h" accepted=yes',
                                 "missing_or_duplicate_native_selection_receipt"),
        }
        with tempfile.TemporaryDirectory() as temp:
            root = Path( temp )
            binding = root / startup_harness.TRANSITION_EVENT_BINDING_FILENAME
            binding.write_text( '{"run_id":"r008-duration-run"}', encoding="utf-8" )
            binding_id, _ = startup_harness.sha256_file( binding )
            for name, (events, native_actions, reason) in cases.items():
                with self.subTest( name=name ):
                    selections = "".join(
                        'openclaw_harness_wait_input_trace: component=wait_menu event=selection '
                        f'run_id="{self.run_id}" action_id="{action}" accepted=yes\n'
                        for action in native_actions.splitlines()
                    )
                    result = self.check(
                        root, events, selections, binding_id=binding_id,
                        dispatch="missing" if name == "missing_dispatch" else "wait_dispatched",
                    )
                    self.assertEqual( result["reason"], reason )

    def test_rejects_stale_mixed_run_and_changed_binding( self ):
        with tempfile.TemporaryDirectory() as temp:
            root = Path( temp )
            binding = root / startup_harness.TRANSITION_EVENT_BINDING_FILENAME
            binding.write_text( '{"run_id":"r008-duration-run"}', encoding="utf-8" )
            binding_id, _ = startup_harness.sha256_file( binding )
            stale = self.check( root, [], "", binding_id=binding_id )
            mixed = self.check(
                root, self.semantic_events(),
                'openclaw_harness_wait_input_trace: component=wait_menu event=selection '
                'run_id="other-run" action_id="wait.6h" accepted=yes\n', binding_id=binding_id,
            )
            binding.write_text( '{"run_id":"changed"}', encoding="utf-8" )
            changed = self.check( root, self.semantic_events(), "", binding_id=binding_id )
        self.assertEqual( stale["reason"], "missing_or_duplicate_duration_menu" )
        self.assertEqual( mixed["reason"], "mixed_native_selection_run" )
        self.assertEqual( changed["reason"], "binding_changed_or_missing" )


if __name__ == "__main__":
    unittest.main()
