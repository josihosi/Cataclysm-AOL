#!/usr/bin/env python3
"""Fail-closed controls for R-008's native six-hour completion boundary."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path


HARNESS_DIR = Path( __file__ ).resolve().parent
sys.path.insert( 0, str( HARNESS_DIR ) )

from startup_harness import (  # noqa: E402
    classify_native_wait_activity_completion,
    classify_run_bound_native_wait,
)


class R008NaturalWaitCompletionTest( unittest.TestCase ):
    run_id = "r008-current-run"
    binding_id = "r008-current-binding"
    receipt_id = "r008-six-hour-wait"

    def events( self ) -> list[dict]:
        return [
            {"event": "receipt", "run_id": self.run_id, "frame_id": "duration",
             "action_id": "wait.6h", "accepted": True},
            {"event": "frame", "run_id": self.run_id, "frame_id": "activity",
             "state": "wait_activity", "valid_actions": [], "action_inputs": {}},
            {"event": "frame", "run_id": self.run_id, "frame_id": "complete",
             "state": "wait_activity_complete", "valid_actions": [], "action_inputs": {}},
        ]

    def receipt( self, **overrides: object ) -> dict:
        result = {
            "run_id": self.run_id,
            "binding_id": self.binding_id,
            "receipt_id": self.receipt_id,
            "start_seconds_since_midnight": 8 * 60 * 60,
            "end_seconds_since_midnight": 14 * 60 * 60,
            "expected_seconds": 6 * 60 * 60,
        }
        result.update( overrides )
        return result

    def completion( self, events: list[dict] | None = None, **overrides: object ) -> dict:
        result = classify_native_wait_activity_completion(
            self.events() if events is None else events,
            run_id=self.run_id, binding_id=self.binding_id,
            expected_action="wait.6h", receipt_id=self.receipt_id,
        )
        result.update( overrides )
        return result

    def test_accepts_exact_bound_native_completion_without_debug_delta( self ) -> None:
        result = classify_run_bound_native_wait(
            self.receipt(), self.completion(), current_run_id=self.run_id,
            current_binding_id=self.binding_id,
        )

        self.assertEqual( result["status"], "completed" )
        self.assertEqual( result["observed_seconds"], 21600 )

    def test_rejects_missing_event_short_long_stale_mixed_and_interrupted_waits( self ) -> None:
        interrupted = self.events()
        interrupted.insert( 2, {"event": "frame", "run_id": self.run_id,
                                "frame_id": "interrupted", "state": "activity_distraction",
                                "valid_actions": [], "action_inputs": {}} )
        controls = {
            "missing_event": self.completion( self.events()[:-1] ),
            "short_wait": self.completion( end_seconds_since_midnight=13 * 60 * 60 ),
            "long_wait": self.completion( end_seconds_since_midnight=15 * 60 * 60 ),
            "stale_event": self.completion( [{**event, "run_id": "stale"}
                                               for event in self.events()] ),
            "mixed_binding": self.completion( binding_id="other-binding" ),
            "interrupted": self.completion( interrupted ),
        }
        for name, evidence in controls.items():
            with self.subTest( name=name ):
                receipt = self.receipt()
                if name == "short_wait":
                    receipt["end_seconds_since_midnight"] = 13 * 60 * 60
                elif name == "long_wait":
                    receipt["end_seconds_since_midnight"] = 15 * 60 * 60
                result = classify_run_bound_native_wait(
                    receipt, evidence, current_run_id=self.run_id,
                    current_binding_id=self.binding_id,
                )
                self.assertNotEqual( result["status"], "completed" )

    def test_rejects_a_native_ui_label_or_receipt_mismatch( self ) -> None:
        result = classify_run_bound_native_wait(
            self.receipt(), self.completion( source="native_wait_ui" ),
            current_run_id=self.run_id, current_binding_id=self.binding_id,
        )
        self.assertEqual( result["status"], "unproved" )
        self.assertIn( "native_wait_termination_unproved", result["failures"] )


if __name__ == "__main__":
    unittest.main()
