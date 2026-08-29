#!/usr/bin/env python3
"""R-013 route ordering guard for the zero-credit long-wait bootstrap."""

import json
import unittest
from pathlib import Path


SCENARIO_PATH = Path( __file__ ).resolve().parent / "scenarios" / \
                "r013.cockpit_long_wait_activity_interrupt_bootstrap_mcw.json"
FIXTURE_PATH = Path( __file__ ).resolve().parent / "fixtures" / "saves" / "live-debug" / \
               "r013_long_wait_hostile_interrupt_bootstrap_v1" / "manifest.json"


class R013LongWaitPlanOrderTest( unittest.TestCase ):
    def load_scenario( self ) -> dict:
        return json.loads( SCENARIO_PATH.read_text( encoding="utf-8" ) )

    def test_wait_chain_starts_from_the_clean_hud_without_modal_dismissal( self ) -> None:
        scenario = self.load_scenario()
        steps = scenario["steps"]

        self.assertEqual( steps[0]["kind"], "wait" )
        self.assertEqual( steps[1]["kind"], "cockpit_act" )
        self.assertEqual(
            steps[1]["action_chain"],
            ["world.wait", "wait.duration_menu", "wait.6h"],
        )

    def test_escape_is_fail_closed_for_the_clean_hud_route( self ) -> None:
        scenario = self.load_scenario()
        contract = scenario["runtime_contract"]

        self.assertNotIn(
            "native:in_game_main_menu.dismiss:escape",
            contract["permitted_input"],
        )
        self.assertIn( "semantic:activity.ignore", contract["forbidden_input"] )

    def test_fixture_stages_one_hostile_sighting_owner_before_the_wait( self ) -> None:
        fixture = json.loads( FIXTURE_PATH.read_text( encoding="utf-8" ) )
        monsters = next(
            transform for transform in fixture["save_transforms"]
            if transform["kind"] == "active_monsters_near_player"
        )
        self.assertTrue( monsters["clear_existing"] )
        self.assertEqual( monsters["monsters"], [{
            "typeid": "mon_zombie_dog", "offset_ms": [6, 0, 0], "hp": 80,
            "friendly": 0, "faction": "zombie", "anger": 100, "morale": 100,
        }] )


if __name__ == "__main__":
    unittest.main()
