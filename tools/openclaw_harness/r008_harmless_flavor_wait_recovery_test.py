#!/usr/bin/env python3
"""Fail-closed controls for the R-008 flavor-popup continuation path."""

import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert( 0, str( Path( __file__ ).resolve().parent ) )

import startup_harness


class R008HarmlessFlavorWaitRecoveryTest( unittest.TestCase ):
    def test_accepts_bound_semantic_popup_then_native_eoc_continue( self ):
        popup = {
            "status": "semantic_recovered",
            "acknowledgements": [{
                "response_key": "space",
                "classification": {
                    "classification": "shadow_warning_wilderness_flavor_popup",
                    "provenance": "semantic_ui_trace",
                },
            }],
        }
        activity = {
            "status": "clear",
            "acknowledgements": [{
                "response_key": "I",
                "classification": {
                    "classification": "activity_distraction_prompt",
                    "structured_activity_query_trace": {"type": "eoc"},
                },
            }],
        }
        with tempfile.TemporaryDirectory() as temp, \
                patch.object( startup_harness, "acknowledge_blocking_interruptions",
                              side_effect=[popup, activity] ) as acknowledge:
            result = startup_harness.recover_harmless_wilderness_flavor_wait(
                123, Path( temp ), "r008", delay_ms=1,
                action_trace_log=Path( temp ) / "native.log",
                action_trace_start_offset=0,
            )

        self.assertEqual( result["status"], "recovered_harmless_flavor_wait_sequence" )
        self.assertEqual( result["response_keys"], ["space", "I"] )
        self.assertEqual( acknowledge.call_count, 2 )

    def test_accepts_native_eoc_continue_then_bound_semantic_popup( self ):
        activity = {
            "status": "clear",
            "acknowledgements": [{
                "response_key": "I",
                "classification": {
                    "classification": "activity_distraction_prompt",
                    "structured_activity_query_trace": {"type": "eoc"},
                },
            }],
        }
        popup = {
            "status": "semantic_recovered",
            "acknowledgements": [{
                "response_key": "space",
                "classification": {
                    "classification": "shadow_warning_wilderness_flavor_popup",
                    "provenance": "semantic_ui_trace",
                },
            }],
        }
        with tempfile.TemporaryDirectory() as temp, \
                patch.object( startup_harness, "acknowledge_blocking_interruptions",
                              side_effect=[activity, popup] ) as acknowledge:
            result = startup_harness.recover_harmless_wilderness_flavor_wait(
                123, Path( temp ), "r008", delay_ms=1,
                action_trace_log=Path( temp ) / "native.log",
                action_trace_start_offset=0,
            )

        self.assertEqual( result["status"], "recovered_harmless_flavor_wait_sequence" )
        self.assertEqual( result["response_keys"], ["I", "space"] )
        self.assertEqual( acknowledge.call_count, 2 )

    def test_accepts_standalone_semantic_eoc_flavor_popup( self ):
        popup = {
            "status": "semantic_recovered",
            "acknowledgements": [{
                "response_key": "space",
                "classification": {
                    "classification": "semantic_ui_recovered",
                    "provenance": "semantic_ui_trace",
                },
            }],
        }
        clear = {"status": "clear", "acknowledgements": []}
        with tempfile.TemporaryDirectory() as temp, \
                patch.object( startup_harness, "acknowledge_blocking_interruptions",
                              side_effect=[popup, clear] ) as acknowledge:
            result = startup_harness.recover_harmless_wilderness_flavor_wait(
                123, Path( temp ), "r008", delay_ms=1,
                action_trace_log=Path( temp ) / "native.log",
                action_trace_start_offset=0,
            )

        self.assertEqual( result["status"], "recovered_harmless_flavor_wait_sequence" )
        self.assertEqual( result["response_keys"], ["space"] )
        self.assertEqual( acknowledge.call_count, 2 )

    def test_rejects_nonsemantic_popup_even_if_it_claims_recovery( self ):
        popup = {
            "status": "semantic_recovered",
            "acknowledgements": [{
                "response_key": "space",
                "classification": {"provenance": "ocr"},
            }],
        }
        with tempfile.TemporaryDirectory() as temp, \
                patch.object( startup_harness, "acknowledge_blocking_interruptions",
                              return_value=popup ) as acknowledge:
            result = startup_harness.recover_harmless_wilderness_flavor_wait(
                123, Path( temp ), "r008", delay_ms=1,
                action_trace_log=Path( temp ) / "native.log",
                action_trace_start_offset=0,
            )

        self.assertEqual( result["status"], "blocked_harmless_flavor_popup_sequence" )
        self.assertEqual( acknowledge.call_count, 1 )

    def test_rejects_two_continue_prompts( self ):
        activity = {
            "status": "clear",
            "acknowledgements": [{
                "response_key": "I",
                "classification": {
                    "classification": "activity_distraction_prompt",
                    "structured_activity_query_trace": {"type": "eoc"},
                },
            }],
        }
        with tempfile.TemporaryDirectory() as temp, \
                patch.object( startup_harness, "acknowledge_blocking_interruptions",
                              side_effect=[activity, activity] ) as acknowledge:
            result = startup_harness.recover_harmless_wilderness_flavor_wait(
                123, Path( temp ), "r008", delay_ms=1,
                action_trace_log=Path( temp ) / "native.log",
                action_trace_start_offset=0,
            )

        self.assertEqual( result["status"], "blocked_harmless_flavor_continue_sequence" )
        self.assertEqual( acknowledge.call_count, 2 )


if __name__ == "__main__":
    unittest.main()
