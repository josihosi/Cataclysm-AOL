#!/usr/bin/env python3
"""Contract checks for the bounded debug-setup / native-city behavior route."""

from __future__ import annotations

import sys
import json
import unittest
from pathlib import Path

HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

from startup_harness import (  # noqa: E402
    debug_spawn_monster_intervention_receipt, evaluate_structured_proof_gates,
    load_scenario, normalize_pre_spawn_world_actions,
)


class StalkerFollowCityContractTest(unittest.TestCase):
    def setUp(self) -> None:
        self.scenario = load_scenario("r_zl_stalker_follow_city_debug_setup_mcw")

    def test_authorizes_exactly_one_zero_credit_stalker_and_no_debug_zombies_or_outcome(self) -> None:
        setup = self.scenario["steps"][2]
        self.assertEqual(setup["kind"], "debug_spawn_monster")
        self.assertEqual(setup["creature_id"], "mon_writhing_stalker")
        self.assertEqual(setup["target_offset"], [0, 5, 0])
        self.assertEqual(setup["target_keys"], ["down"] * 5)
        self.assertEqual(
            setup["pre_spawn_world_actions"],
            ["world.toggle_safemode"] + ["world.move.west"] * 3 + ["world.move.south"] * 5 +
            ["world.move.southwest"] * 2 + ["world.move.southeast"],
        )
        self.assertTrue(setup["semantic_open_debug_menu"])
        self.assertFalse(setup["friendly"])
        self.assertFalse(setup["hallucination"])
        self.assertEqual(setup["group_radius"], 0)
        # The receipt is established from the semantic World descriptor.  A
        # persistent safe-mode *message* after the spawn is not a separate
        # modal input owner, so the generic OCR acknowledgement scanner must
        # not block the handoff before that descriptor reaches the cockpit.
        self.assertFalse(setup["auto_acknowledge_interruptions"])
        contract = self.scenario["runtime_contract"]
        self.assertIn("debug:spawn_writhing_stalker_once", contract["permitted_input"])
        for forbidden in ("debug:spawn_zombie", "debug:inject_attention", "debug:inject_contact", "debug:inject_opportunity", "debug:inject_destination", "debug:inject_attack", "debug:inject_hit"):
            self.assertIn(forbidden, contract["forbidden_input"])
        self.assertNotIn("zombie", setup["monster_query"].lower())
        # Each hybrid launch needs a fresh city save: the setup receipt authorizes
        # one stalker, so an earlier setup world cannot be replayed.
        self.assertEqual(self.scenario["world"], "zl-natural-stalker-city-042-missed")
        self.assertEqual(self.scenario["fixture"], "r_zl_stalker_city042_no_baseline_stalker_20260920")
        self.assertEqual(self.scenario["fixture_profile"], "r-zl-stalker-city042-baseline-cleanup-001")
        self.assertEqual(self.scenario["runtime_contract"]["fixture"], "r_zl_stalker_city042_no_baseline_stalker_20260920")

    def test_setup_receipt_is_zero_credit_and_connected_to_the_bootstrap_run(self) -> None:
        authority = {"authority": "registry", "authority_id": "selection-1", "binding_id": "binding-1", "source_sha256": "source-1"}
        receipt = debug_spawn_monster_intervention_receipt(
            creature_id="mon_writhing_stalker", target_offset=[0, 5, 0], target_keys=["down"] * 5,
            group_radius=0, friendly=False, hallucination=False, run_id="run-1",
            registry_authority=authority, run_dir=Path("/tmp/stalker-follow-city"),
            actor_observation={
                "identity": {"kind": "monster", "id": "process:one"},
                "typeid": "mon_writhing_stalker", "debug_setup_run_id": "run-1",
                "absolute_ms": [17, 20, 0],
            },
        )
        receipt["producer_step_index"] = 3
        self.assertFalse(receipt["gameplay_credit"])
        self.assertEqual(receipt["actor_correlation"], {
            "kind": "native_monster_value_tag",
            "marker": "caol_debug_setup_stalker_run_id",
            "run_id": "run-1",
            "typeid": "mon_writhing_stalker",
            "gameplay_credit": False,
        })
        gates = self.scenario["proof_gates"]
        evidence = evaluate_structured_proof_gates(
            gates, events=[], watermarks={"bootstrap_same_world_city_play": {"run_id": "run-1", "step_index": 4}},
            saved_artifacts=[receipt], run_id="run-1",
        )
        self.assertEqual(evidence["status"], "green")
        self.assertEqual(evidence["gates"][0]["status"], "green")
        self.assertEqual(receipt["actor_observation"]["debug_setup_run_id"], "run-1")
        receipt["run_id"] = "other-run"
        rejected = evaluate_structured_proof_gates(
            gates, events=[], watermarks={"bootstrap_same_world_city_play": {"run_id": "run-1", "step_index": 4}},
            saved_artifacts=[receipt], run_id="run-1",
        )
        self.assertEqual(rejected["status"], "red")

    def test_setup_receipt_refuses_unobserved_or_foreign_actor(self) -> None:
        with self.assertRaises(SystemExit):
            debug_spawn_monster_intervention_receipt(
                creature_id="mon_writhing_stalker", target_offset=[0, 5, 0],
                target_keys=["down"] * 5, group_radius=0, friendly=False,
                hallucination=False, run_id="run-1",
                registry_authority={"authority": "registry", "authority_id": "selection-1"},
                run_dir=Path("/tmp/stalker-follow-city"),
                actor_observation={
                    "identity": {"kind": "monster", "id": "process:wrong"},
                    "typeid": "mon_writhing_stalker", "debug_setup_run_id": "other-run",
                    "absolute_ms": [17, 20, 0],
                },
        )

    def test_pre_spawn_route_is_movement_only(self) -> None:
        self.assertEqual(
            normalize_pre_spawn_world_actions(
                ["world.toggle_safemode", "world.move.west", "world.move.southwest"]
            ),
            ["world.toggle_safemode", "world.move.west", "world.move.southwest"],
        )
        with self.assertRaisesRegex(SystemExit, "only World movement"):
            normalize_pre_spawn_world_actions(["world.debug_menu"])

    def test_live_objective_requires_native_following_pressure_and_attack_not_intent(self) -> None:
        live = self.scenario["steps"][3]
        # Setup completion alone cannot prove behavior.  The live bridge must
        # remain available to carry the same run through ordinary city play.
        self.assertFalse(live["bootstrap_only"])
        text = " ".join(live["proof_targets"] + live["invariants"]).lower()
        for required in ("same run/world/player/stalker", "following", "natural city-zombie", "attack-resolution"):
            self.assertIn(required, text)
        self.assertIn("normal-ai", text)

    def test_matching_registry_brief_and_charter_bind_the_city_hybrid_route(self) -> None:
        brief = json.loads((HARNESS_DIR / "charters" /
                            "r-zl-stalker-follow-city-debug-setup-001-brief.json").read_text())
        charter = json.loads((HARNESS_DIR / "charters" /
                              "r-zl-stalker-follow-city-debug-setup-001-witness-charter.json").read_text())
        self.assertEqual(brief["scenario"], self.scenario["name"])
        requirements = {item["key"]: item["value"] for item in brief["query"]["requirements"]}
        self.assertEqual(requirements, {
            "capabilities.r_zl_playtest.stalker_follow_city":
                "one_zero_credit_debug_stalker_then_native_city_follow_pressure_and_attack",
            "capabilities.r_zl_playtest.city_zombies": "natural_seed042_city_population_only",
        })
        self.assertIn("one explicitly zero-credit native debug spawn", charter["claim"])
        self.assertIn("same actor follows", charter["claim"])
        self.assertIn("attack-resolution", charter["material_proof"])


if __name__ == "__main__":
    unittest.main()
