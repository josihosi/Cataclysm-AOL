#!/usr/bin/env python3
"""Contract controls for the fresh source-route R-008 local-pair bootstrap."""

import json
import unittest
from pathlib import Path


SCENARIO_PATH = Path( __file__ ).resolve().parent / "scenarios" / \
                "bandit.r008_closure_229_source_route_local_pair_bootstrap_mcw.json"
PROFILE_PATH = Path( __file__ ).resolve().parent / "profiles" / "r008-closure-229-bootstrap.json"


class R008Closure229SourceRouteManifestTest( unittest.TestCase ):
    def load_scenario( self ) -> dict:
        return json.loads( SCENARIO_PATH.read_text( encoding="utf-8" ) )

    def test_each_native_wait_declares_its_real_menu_context_and_popup_recovery( self ) -> None:
        scenario = self.load_scenario()
        self.assertEqual( scenario["steps"][0]["label"], "dismiss_inherited_actions_overlay" )
        self.assertEqual( scenario["steps"][0]["keys"], ["escape"] )
        profile = json.loads( PROFILE_PATH.read_text( encoding="utf-8" ) )
        self.assertEqual( profile["startup"]["post_lastworld_continue_keys"], ["escape"] )
        post_load_hud = next( step for step in scenario["steps"] if step["label"] == "post_load_hud" )
        self.assertTrue( post_load_hud["capture_after"] )
        self.assertNotIn( "expected_screen_text_after_contains", post_load_hud )
        waits = [step for step in scenario["steps"] if step["kind"] == "long_wait"]
        self.assertEqual( [step["choice_key"] for step in waits], ["8", "8", "5", "5", "5"] )
        self.assertEqual( [step["expected_duration"] for step in waits], ["6h", "6h", "1h", "1h", "1h"] )
        for step in waits:
            with self.subTest( step=step["label"] ):
                self.assertEqual( step["wait_key"], "|" )
                self.assertEqual( step["pre_menu_choice_key"], "w" )
                self.assertTrue( step["require_native_wait_menu_guards"] )
                self.assertTrue( step["require_wait_input_trace"] )
                self.assertEqual( step["pre_menu_expected_screen_text_contains"], ["Wait a while", "Set an alarm"] )
                self.assertTrue( step["duration_menu_expected_screen_text_contains"] )
                self.assertTrue( step["allow_harmless_flavor_popup_wait_recovery"] )

    def test_exact_dispatch_precedes_the_declared_local_pair_successor( self ) -> None:
        scenario = self.load_scenario()
        gates = scenario["proof_gates"]
        self.assertEqual( [gate["id"] for gate in gates], [
            "native_dispatch", "native_local_pair", "persisted_local_pair",
        ] )
        dispatch = gates[0]["expectations"][0]["predicate"]
        self.assertEqual( dispatch["transition"], "active_sortie_dispatch" )
        self.assertEqual( dispatch["actor_ids"], [4, 5] )
        self.assertEqual( (dispatch["simulation_owner"], dispatch["previous_state"], dispatch["new_state"]),
                          ("abstract", "at_home", "outbound") )
        successor = gates[1]["expectations"][0]["predicate"]
        self.assertEqual( successor["transition"], "local_pair_handoff" )
        self.assertEqual( (successor["actor_ids"], successor["generation"], successor["handoff_epoch"], successor["simulation_owner"]),
                          ([4, 5], 1, 1, "local") )
        self.assertEqual( gates[1]["predecessors"], ["native_dispatch"] )
        local_audit = next(
            step for step in scenario["steps"]
            if step["label"] == "audit_native_local_pair_successor"
        )
        self.assertEqual( local_audit["kind"], "audit_structured_transition_event" )
        self.assertTrue( local_audit["require_exactly_one"] )

    def test_local_pair_waits_one_native_hour_after_dispatch_before_auditing( self ) -> None:
        scenario = self.load_scenario()
        steps = scenario["steps"]
        labels = [step["label"] for step in steps]
        dispatch = labels.index( "resolve_native_active_sortie_dispatch_boundary" )
        boundary_wait = labels.index( "source_route_native_local_pair_boundary_hour" )
        local_audit = labels.index( "audit_native_local_pair_successor" )

        self.assertLess( dispatch, boundary_wait )
        self.assertLess( boundary_wait, local_audit )
        wait = steps[boundary_wait]
        self.assertEqual( wait["kind"], "long_wait" )
        self.assertEqual( wait["expected_duration"], "1h" )
        self.assertEqual( wait["minimum_artifact_elapsed_minutes"], 60 )
        self.assertTrue( wait["require_native_wait_menu_guards"] )
        self.assertTrue( wait["require_wait_input_trace"] )

    def test_local_pair_boundary_is_source_bound_and_zero_credit( self ) -> None:
        boundary = self.load_scenario()["setup_receipt_contract"]["local_pair_boundary"]

        self.assertEqual( boundary["predecessor"]["at_minutes"], 8520 )
        self.assertEqual( boundary["predecessor"]["actor_ids"], [4, 5] )
        self.assertEqual( boundary["contact_boundary"]["at_minutes"], 8580 )
        self.assertEqual( boundary["contact_boundary"]["route_position_omt"], [175, 13, 0] )
        self.assertEqual( boundary["contact_boundary"]["owner"], "local" )
        self.assertEqual( boundary["cleanup_owner"], "cleanup_harness_world" )


if __name__ == "__main__":
    unittest.main()
