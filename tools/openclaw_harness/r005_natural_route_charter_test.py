#!/usr/bin/env python3
"""Repository charter checks for the agent-owned R-005 route qualification."""

from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path

sys.path.insert( 0, str( Path( __file__ ).resolve().parent ) )

from playtest_witness import normalize_witness_charter  # noqa: E402
from scenario_registry import validate_manifest  # noqa: E402
from startup_harness import build_probe_step_ledger  # noqa: E402


HARNESS_DIR = Path( __file__ ).resolve().parent
CHARTER_PATH = HARNESS_DIR / "charters" / "r005-natural-route-qualification-rev1.json"
SCENARIO_PATH = HARNESS_DIR / "scenarios" / "bandit.r005_continuous_hostile_ecology_certification.json"
QUALIFICATION_SCENARIO_PATH = HARNESS_DIR / "scenarios" / "bandit.r005_natural_route_qualification.json"
OBSERVATION_SCENARIO_PATH = HARNESS_DIR / "scenarios" / "bandit.r005_natural_route_observation.json"
WEST_FLANK_SCENARIO_PATH = HARNESS_DIR / "scenarios" / "bandit.r005_west_flank_corridor_qualification.json"


class R005NaturalRouteCharterTest( unittest.TestCase ):
    def test_charter_preserves_r005_contract_and_stays_zero_credit( self ) -> None:
        charter = normalize_witness_charter(
            json.loads( CHARTER_PATH.read_text( encoding="utf-8" ) )
        )
        scenario = json.loads( SCENARIO_PATH.read_text( encoding="utf-8" ) )
        qualification = json.loads( QUALIFICATION_SCENARIO_PATH.read_text( encoding="utf-8" ) )

        self.assertEqual( charter["requested_evidence_ceiling"], "zero-credit" )
        self.assertIn( "unchanged R-005 world", charter["claim"] )
        self.assertIn( "player/observer interruption", charter["material_proof"] )
        self.assertIn( "production bandit state", charter["material_proof"] )
        self.assertIn( "separate intervention-free run", charter["material_proof"] )
        self.assertNotIn( "[", charter["material_proof"] )
        self.assertTrue( charter["honest_stop_conditions"] )
        self.assertTrue( any(
            "coordinate avoidance" in shortcut
            for shortcut in charter["forbidden_shortcuts"]
        ) )
        self.assertTrue( any(
            "creature zap" in contradiction
            for contradiction in charter["material_contradiction"]
        ) )
        self.assertEqual( scenario["world"], "McWilliams" )
        self.assertEqual( scenario["fixture"], "bandit_r005_native_wait_visibility_bootstrap_v0" )
        self.assertTrue( scenario["replace_existing_worlds"] )
        self.assertEqual( scenario["installed_save_player"], "#Wm9yYWlkYSBWaWNr.sav.zzip" )
        route_step = next(
            step for step in qualification["steps"]
            if step["kind"] == "ordinary_overmap_route_constructor"
        )
        self.assertEqual( route_step["origin_omt"], [140, 41, 0] )
        self.assertEqual( route_step["destination_omt"], [144, 42, 0] )
        self.assertEqual(
            route_step["cursor_keys"],
            ["right", "right", "right", "right", "down"],
        )
        farther_east_northbound = next(
            step for step in qualification["steps"]
            if step["label"] == "observed_x144_northbound"
        )
        self.assertEqual( farther_east_northbound["origin_omt"], [144, 42, 0] )
        self.assertEqual( farther_east_northbound["destination_omt"], [144, 29, 0] )
        self.assertEqual(
            farther_east_northbound["cursor_keys"],
            ["up"] * 13,
        )
        y29_return = next(
            step for step in qualification["steps"]
            if step["label"] == "observed_y29_return"
        )
        self.assertEqual( y29_return["origin_omt"], [144, 29, 0] )
        self.assertEqual( y29_return["destination_omt"], [140, 29, 0] )
        self.assertEqual(
            y29_return["cursor_keys"],
            ["left"] * 4,
        )
        farther_east_destination = next(
            step for step in qualification["steps"]
            if step["label"] == "observed_y29_destination"
        )
        self.assertEqual( farther_east_destination["origin_omt"], [140, 29, 0] )
        self.assertEqual( farther_east_destination["destination_omt"], [140, 31, 0] )
        self.assertEqual( farther_east_destination["cursor_keys"], ["down"] * 2 )
        self.assertFalse( scenario["runtime_contract"]["grants_gameplay_proof"] )
        self.assertEqual( qualification["world"], scenario["world"] )
        self.assertNotEqual( qualification["fixture"], scenario["fixture"] )
        self.assertEqual(
            qualification["capabilities"]["capabilities.bandit.r005.route_qualification"],
            "observed_x144_y29_source_bound_zero_credit_destination_arrival",
        )
        stabilization_steps = [
            step for step in qualification["steps"]
            if "native_travel_stabilization" in step
        ]
        self.assertTrue( stabilization_steps )
        self.assertEqual(
            {
                step["native_travel_stabilization"]["danger_handling"]
                for step in stabilization_steps
            },
            {"ignore_danger_and_interruptions"},
        )
        final_accept = next(
            step for step in qualification["steps"]
            if step["label"] == "accept_observed_y29_destination"
        )
        world_artifact = next(
            step for step in qualification["steps"]
            if step["label"] == "arrival_world_artifact"
        )
        arrival_gate = next(
            gate for gate in qualification["proof_gates"]
            if gate["id"] == "destination_arrival"
        )
        self.assertEqual(
            [gate["id"] for gate in qualification["proof_gates"]],
            [
                "declared_world",
                "x144_departure_terminal",
                "x144_northbound_terminal",
                "y29_return_terminal",
                "destination_arrival",
            ],
        )
        terminal_destinations = [
            next(
                item["predicate"]["destination"]
                for item in gate["expectations"]
                if item["predicate"].get("artifact_kind") == "native_travel_terminal_receipt"
            )
            for gate in qualification["proof_gates"]
            if gate["id"] != "declared_world"
        ]
        self.assertEqual(
            terminal_destinations,
            [[144, 42, 0], [144, 29, 0], [140, 29, 0], [140, 31, 0]],
        )
        self.assertEqual(final_accept["settle_seconds"], 0)
        self.assertEqual(
            final_accept["native_travel_stabilization"]["danger_handling"],
            "ignore_danger_and_interruptions",
        )
        self.assertEqual(world_artifact["required_player_abs_omt"], [140, 31, 0])
        self.assertEqual(
            {item["predicate"].get("artifact_kind") for item in arrival_gate["expectations"]},
            {"native_travel_terminal_receipt", "audit_saved_game_turn"},
        )

    def test_observation_uses_bound_startup_authority_not_incidental_hud_words( self ) -> None:
        observation = json.loads( OBSERVATION_SCENARIO_PATH.read_text( encoding="utf-8" ) )
        startup = observation["steps"][0]
        close_steps = [
            step for step in observation["steps"]
            if step["label"].startswith( "close_farther_east_" )
        ]

        self.assertEqual(
            startup["startup_semantic_hud_expectation"]["classification"],
            "green_gameplay_hud_present",
        )
        self.assertEqual(
            startup["checkpoint_evidence"],
            {"classification": "required_feature_evidence", "authoritative_owner": "bound_startup_semantic_hud"},
        )
        self.assertNotIn( "expected_screen_text_after_contains", startup )
        self.assertNotIn( "abort_on_screen_text_expectation_failure", startup )
        self.assertTrue( close_steps )
        for step in close_steps:
            self.assertEqual(
                step["checkpoint_evidence"],
                {"classification": "incidental_bootstrap_observation", "authoritative_owner": "bound_startup_semantic_hud"},
            )
            self.assertNotIn( "expected_screen_text_after_contains", step )
            self.assertNotIn( "abort_on_screen_text_expectation_failure", step )

        positive = build_probe_step_ledger( [{
            "index": 1,
            "kind": "wait",
            "label": startup["label"],
            "checkpoint_evidence": startup["checkpoint_evidence"],
            "startup_semantic_hud_expectation": {
                "status": "green",
                "provenance": "run_bound_semantic_startup_gameplay_hud_verdict",
            },
        }] )
        missing_owner = build_probe_step_ledger( [{
            "index": 1,
            "kind": "wait",
            "label": startup["label"],
            "checkpoint_evidence": startup["checkpoint_evidence"],
        }] )

        self.assertEqual( positive[0]["verdict"], "green_step_required_bound_semantic_hud" )
        self.assertEqual( missing_owner[0]["verdict"], "red_step_required_semantic_hud_missing" )

    def test_west_flank_bootstrap_is_valid_and_remains_zero_credit( self ) -> None:
        scenario = json.loads( WEST_FLANK_SCENARIO_PATH.read_text( encoding="utf-8" ) )

        validate_manifest( scenario, path=WEST_FLANK_SCENARIO_PATH )
        self.assertFalse( scenario["runtime_contract"]["grants_gameplay_proof"] )
        self.assertEqual(
            [gate["checkpoint_safe_ui"] for gate in scenario["proof_gates"]],
            [{"semantic_state": {"required": True}}] * 3,
        )

if __name__ == "__main__":
    unittest.main()
