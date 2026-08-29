#!/usr/bin/env python3
"""Focused contracts for the zero-credit R-019 meaningful-event bootstrap."""

from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path


HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

from startup_harness import fixture_source_binding, load_scenario, resolve_fixture_payload  # noqa: E402
import scenario_registry_store as store  # noqa: E402


class R019MeaningfulEventBootstrapTest(unittest.TestCase):
    def setUp(self) -> None:
        self.scenario = load_scenario("r019.keep_watch_meaningful_event_bootstrap_mcw")

    def test_declares_an_exact_disposable_hostile_sighting_scene(self) -> None:
        resolved = resolve_fixture_payload(
            "r019_keep_watch_meaningful_event_bootstrap_v1", "live-debug"
        )
        transforms = resolved["save_transforms"]
        staged = next(item for item in reversed(transforms)
                      if item["kind"] == "active_monsters_near_player")
        self.assertTrue(staged["clear_existing"])
        self.assertEqual([{key: monster[key] for key in (
            "typeid", "offset_ms", "hp", "friendly", "faction", "anger", "morale"
        )} for monster in staged["monsters"]], [{
            "typeid": "mon_zombie_dog", "offset_ms": [6, 0, 0], "hp": 80,
            "friendly": 0, "faction": "zombie", "anger": 100, "morale": 100,
        }])

    def test_binds_the_full_fixture_chain_and_forbids_guarded_dispatch(self) -> None:
        binding = fixture_source_binding(
            "r019_keep_watch_meaningful_event_bootstrap_v1", "live-debug"
        )
        self.assertEqual(
            [entry["name"] for entry in binding["source_chain"]][:2],
            ["r019_keep_watch_meaningful_event_bootstrap_v1", "r013_clean_wait_duration_v1"],
        )
        contract = self.scenario["runtime_contract"]
        self.assertEqual(contract["permitted_input"], ["native:in_game_main_menu.dismiss:escape"])
        self.assertIn("cockpit:game.keep_watch", contract["forbidden_input"])
        self.assertFalse(contract["grants_gameplay_proof"])
        self.assertEqual(self.scenario["steps"][0], {
            "kind": "press", "label": "dismiss_declared_in_game_main_menu", "keys": ["escape"],
        })
        self.assertEqual(self.scenario["steps"][1], {
            "kind": "native_semantic_bootstrap",
            "label": "emit_r019_meaningful_event_bootstrap_frame",
            "expected_visible_fact": "the canonical launcher exposes a current run-bound native world frame before a later separately authorized guarded probe may use the staged hostile sighting",
            "native_semantic_checkpoint": {
                "required_actions": ["world.wait"], "required_state": "world",
            },
        })
        descriptor = self.scenario["steps"][2]
        self.assertEqual(descriptor["kind"], "cockpit_live_session")
        self.assertTrue(descriptor["bootstrap_only"])
        self.assertEqual(descriptor["authority"], [])

    def test_declares_setup_receipt_only_with_no_acceptance_credit(self) -> None:
        gate = self.scenario["proof_gates"][0]
        self.assertEqual(gate["expectations"][0]["predicate"], {
            "artifact_kind": "r014_native_semantic_bootstrap_frame", "gameplay_credit": False,
        })
        self.assertEqual(gate["expectations"][1]["predicate"], {
            "artifact_kind": "native_cockpit_session_descriptor", "gameplay_credit": False,
            "same_run": True,
        })
        contract = self.scenario["evidence_contract"]
        self.assertIn("zero gameplay, guarded-stop, matrix, and R-019 acceptance credit", contract["claim"])
        self.assertIn("failed cleanup", contract["failure_rule"])

    def test_guarded_validation_has_separate_authority_and_consumes_the_staged_fixture(self) -> None:
        validation = load_scenario("r019.keep_watch_meaningful_event_validation_mcw")
        self.assertEqual(validation["fixture"], "r019_keep_watch_meaningful_event_bootstrap_v1")
        self.assertEqual(validation["source_binding"]["bootstrap_report_id"],
                         "d269c9797b8ab1e59e3ec7dc4eab28d6a7ec8a4529e3989222bcac55fcb31965")
        self.assertIn("cockpit:game.keep_watch", validation["runtime_contract"]["permitted_input"])
        self.assertFalse(validation["runtime_contract"]["grants_gameplay_proof"])
        self.assertEqual(validation["setup_contract"], {
            "required_monster": {
                "typeid": "mon_zombie_dog", "offset_ms": [6, 0, 0],
                "last_updated_at_save_turn": True,
            },
            "incidental_absent_typeid": "mon_zombie",
        })
        live = validation["steps"][-1]
        self.assertEqual(live["kind"], "cockpit_live_session")
        self.assertIn("separate authority", live["proof_targets"][-1])
        self.assertIn("before unsafe continuation", validation["evidence_contract"]["claim"])

    def test_guarded_validation_has_a_declaration_bound_first_run_route(self) -> None:
        validation = load_scenario("r019.keep_watch_meaningful_event_validation_mcw")
        manifest_id = "r019-meaningful-validation"
        snapshot = store.RegistryQueryCandidateSnapshot(
            scenario_id=manifest_id, lifecycle_state="active", token_eligible=True,
            facts={key: {"value": value} for key, value in validation["capabilities"].items()},
            explanation={"manifest": {"name": validation["name"], "validation": {"status": "valid", "review_required": False}}},
        )
        route = store._first_run_certification_route(snapshot)
        self.assertIsNotNone(route)
        self.assertEqual(route["internal_resolution_state"], "first_run")


if __name__ == "__main__":
    unittest.main()
