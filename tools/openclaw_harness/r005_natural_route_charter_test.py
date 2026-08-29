#!/usr/bin/env python3
"""Repository charter checks for the authority-gated R-005 route qualification."""

from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path

sys.path.insert( 0, str( Path( __file__ ).resolve().parent ) )

from playtest_witness import normalize_witness_charter  # noqa: E402


HARNESS_DIR = Path( __file__ ).resolve().parent
CHARTER_PATH = HARNESS_DIR / "charters" / "r005-natural-route-qualification-rev1.json"
SCENARIO_PATH = HARNESS_DIR / "scenarios" / "bandit.r005_continuous_hostile_ecology_certification.json"


class R005NaturalRouteCharterTest( unittest.TestCase ):
    def test_charter_preserves_r005_contract_and_stays_zero_credit( self ) -> None:
        charter = normalize_witness_charter(
            json.loads( CHARTER_PATH.read_text( encoding="utf-8" ) )
        )
        scenario = json.loads( SCENARIO_PATH.read_text( encoding="utf-8" ) )

        self.assertEqual( charter["requested_evidence_ceiling"], "zero-credit" )
        self.assertIn( "[140,31,0]", charter["claim"] )
        self.assertIn( "[139,40,0]", charter["material_proof"] )
        self.assertIn( "[139,31,0]", charter["material_proof"] )
        self.assertIn( "explicit selected-run authority", charter["honest_stop_conditions"][0] )
        self.assertTrue( any(
            "final-handoff" in shortcut
            for shortcut in charter["forbidden_shortcuts"]
        ) )
        self.assertEqual( scenario["world"], "McWilliams" )
        self.assertEqual( scenario["fixture"], "bandit_r005_natural_hostile_ecology_v0" )
        self.assertTrue( scenario["replace_existing_worlds"] )
        self.assertEqual( scenario["installed_save_player"], "#Wm9yYWlkYSBWaWNr.sav.zzip" )
        route_step = next(
            step for step in scenario["steps"]
            if step["kind"] == "ordinary_overmap_route_constructor"
        )
        self.assertEqual( route_step["origin_omt"], [140, 41, 0] )
        self.assertEqual( route_step["destination_omt"], [139, 40, 0] )
        self.assertEqual(
            route_step["cursor_keys"],
            ["left", "up"],
        )
        west_flank_northbound = next(
            step for step in scenario["steps"]
            if step["label"] == "west_flank_northbound_route"
        )
        self.assertEqual( west_flank_northbound["origin_omt"], [139, 40, 0] )
        self.assertEqual( west_flank_northbound["destination_omt"], [139, 31, 0] )
        west_flank_destination = next(
            step for step in scenario["steps"]
            if step["label"] == "west_flank_destination_route"
        )
        self.assertEqual( west_flank_destination["origin_omt"], [139, 31, 0] )
        self.assertEqual( west_flank_destination["destination_omt"], [140, 31, 0] )
        self.assertFalse( scenario["runtime_contract"]["grants_gameplay_proof"] )


if __name__ == "__main__":
    unittest.main()
