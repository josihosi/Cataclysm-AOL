#!/usr/bin/env python3
"""R-013 clean validation owns no startup input before its semantic wait."""

import json
import unittest
from pathlib import Path

from startup_harness import load_profile_config, resolve_startup_config_profile


SCENARIO_PATH = Path( __file__ ).resolve().parent / "scenarios" / \
                "r013.clean_wait_duration_validation_mcw.json"


class R013CleanWaitValidationInputTest( unittest.TestCase ):
    def test_clean_route_starts_with_the_hud_then_the_bound_wait_transaction( self ) -> None:
        scenario = json.loads( SCENARIO_PATH.read_text( encoding="utf-8" ) )
        steps = scenario["steps"]

        self.assertEqual( steps[0]["kind"], "wait" )
        self.assertIn( "Move", steps[0]["expected_visible_fact"] )
        self.assertIn( "Wield", steps[0]["expected_visible_fact"] )
        self.assertEqual( steps[1]["kind"], "adaptive_semantic_window" )
        self.assertEqual(
            steps[1]["required_action_chain"],
            ["world.wait", "wait.duration_menu", "wait.1m"],
        )

    def test_clean_route_has_no_native_menu_dismissal_authority( self ) -> None:
        scenario = json.loads( SCENARIO_PATH.read_text( encoding="utf-8" ) )
        contract = scenario["runtime_contract"]

        self.assertEqual( scenario["config_profile"], "dev-harness" )
        self.assertEqual( contract["config_profile"], "dev-harness" )
        self.assertEqual( contract["time_baseline"], "current_run_native_semantic_frame" )
        self.assertEqual(
            resolve_startup_config_profile( scenario, scenario["profile"] ),
            "dev-harness",
        )
        self.assertEqual(
            load_profile_config( scenario["config_profile"] )["startup"]
            ["post_lastworld_continue_keys"],
            [],
        )
        self.assertNotIn(
            "native:in_game_main_menu.dismiss:escape",
            contract["permitted_input"],
        )
        self.assertFalse( any( step.get("kind") == "press" for step in scenario["steps"] ) )


if __name__ == "__main__":
    unittest.main()
