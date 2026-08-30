#!/usr/bin/env python3
"""Repository charter checks for the agent-owned R-009 macOS witness."""

from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path

sys.path.insert( 0, str( Path( __file__ ).resolve().parent ) )

from playtest_witness import normalize_witness_charter  # noqa: E402
from r009_technical_witness import preflight_contract  # noqa: E402


CHARTER_PATH = Path( __file__ ).resolve().parent / "charters" / "r009-macos-witness-rev2.json"


class R009MacosWitnessCharterTest( unittest.TestCase ):
    def test_charter_matches_the_portable_preflight_without_runtime_credit( self ) -> None:
        charter = json.loads( CHARTER_PATH.read_text( encoding="utf-8" ) )
        normalized = normalize_witness_charter( charter )
        contract = preflight_contract()

        self.assertEqual( contract["semantic_contract"], "r009-integrated-wait-v1" )
        self.assertEqual(
            contract["semantic_wait_request"]["required_action_chain"],
            ["world.wait", "wait.duration_menu", "wait.6h"],
        )
        self.assertEqual(
            contract["resource_field_contract"]["unavailable_representation"],
            {"status": "unavailable", "value": None},
        )
        self.assertEqual(
            contract["supported_platform_routes"]["macos"]["resource_sampler"],
            "ps %cpu and rss",
        )
        self.assertEqual( contract["continuous_final_certification_credit"], 0 )
        self.assertFalse( contract["starts_selected_run"] )
        self.assertEqual( normalized["requested_evidence_ceiling"], "focused" )
        self.assertIn( "r009-m095", normalized["claim"] )
        self.assertIn( "current source-bound", normalized["claim"] )
        self.assertIn( "advancing and a stalled", normalized["material_proof"] )
        self.assertIn( "world.wait, wait.duration_menu, wait.6h", normalized["material_proof"] )
        self.assertIn( ".userdata/r009-m095/harness_runs/<authorized-run-id>", normalized["material_proof"] )
        self.assertIn( "accepts cleanup before the finalizer ingests the report", normalized["material_proof"] )
        self.assertTrue( normalized["honest_stop_conditions"] )
        self.assertTrue( any(
            "continuous-final-certification" in shortcut
            for shortcut in normalized["forbidden_shortcuts"]
        ) )


if __name__ == "__main__":
    unittest.main()
