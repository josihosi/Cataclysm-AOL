#!/usr/bin/env python3
"""R-008 source-bound setup receipt firewall contracts."""

from __future__ import annotations

import copy
import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path


HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

import scenario_registry_store as store  # noqa: E402
from startup_harness import (  # noqa: E402
    apply_fixture_save_transforms,
    audit_saved_bandit_live_world_state,
    audit_saved_game_turn,
    load_player_abs_omt,
    load_scenario,
    r008_source_bound_setup_receipts_from_installed_save,
    scenario_manifest_binding,
)


class R008SetupReceiptTest(unittest.TestCase):
    def setUp(self) -> None:
        self.root = HARNESS_DIR.parent.parent
        self.scenario = load_scenario("bandit.r008_closure_046_source_bound_m095_mcw")
        self.fixture = HARNESS_DIR / "fixtures/saves/dev-harness/r008_closure_046_source_bound_m095"

    def receipt(self) -> dict:
        with tempfile.TemporaryDirectory() as directory:
            copied = Path(directory) / "fixture"
            shutil.copytree(self.fixture, copied)
            fixture_manifest = json.loads((copied / "manifest.json").read_text(encoding="utf-8"))
            transforms = apply_fixture_save_transforms(
                copied / "save" / self.scenario["world"], fixture_manifest["save_transforms"],
            )
            return r008_source_bound_setup_receipts_from_installed_save(
                copied / "save" / self.scenario["world"],
                setup_contract=self.scenario["setup_receipt_contract"],
                fixture_install={
                    "fixture": self.scenario["fixture"],
                    "manifest": fixture_manifest,
                    "applied_save_transforms": transforms,
                },
            )

    def test_receipt_is_nonempty_and_exactly_source_bound(self) -> None:
        receipt = self.receipt()
        intervention = receipt["interventions"][0]
        self.assertEqual(receipt["status"], "prepared")
        self.assertTrue(intervention["native_receipt"]["accepted"])
        self.assertEqual(
            intervention["after_facts"]["master_gsav_sha256"],
            "cdec3aed809cd5ded71618fc75a0ae05c007ce70e0d688aea8ff31a5a36000e0",
        )

    def test_six_hour_wait_observes_the_declared_full_duration(self) -> None:
        wait = next(
            step for step in self.scenario["steps"]
            if step["label"] == "ordinary_first_6_hours"
        )
        self.assertEqual(wait["expected_duration"], "6h")
        self.assertEqual(wait["minimum_artifact_elapsed_minutes"], 360)
        self.assertGreater(wait["completion_artifact_timeout_seconds"], 0)

    def test_safe_watch_validation_binds_its_fixture_only_setup(self) -> None:
        scenario = load_scenario("bandit.r008_natural_safe_watch_validation_mcw")
        contract = scenario["setup_receipt_contract"]
        self.assertEqual(contract["kind"], "r008_source_bound_fixture")
        self.assertEqual(contract["world"], scenario["world"])
        self.assertEqual(contract["player_save"], scenario["installed_save_player"])
        self.assertEqual(
            contract["stabilizer_traits"], scenario["required_stabilizer_traits"],
        )
        self.assertEqual(contract["source_binding"]["raw_seed"], "830205385")
        step = next(item for item in scenario["steps"] if item["kind"] == "adaptive_semantic_window")
        self.assertEqual(step["required_action_chain"][-1], "wait.6h")
        self.assertEqual(step["adaptive_interrupt_actions"], ["activity.ignore"])
        self.assertEqual(step["required_interrupt_action_chain"], [])

    def test_safe_watch_validation_audits_only_the_post_contact_native_save(self) -> None:
        scenario = load_scenario("bandit.r008_natural_safe_watch_validation_mcw")
        steps = list(scenario["steps"])
        labels = [str(step["label"]) for step in steps]
        dispatch = labels.index("resolve_force_due_dispatch_at_minute_boundary")
        local_contact = labels.index("worker_owned_local_contact_and_cohesion")
        open_save = labels.index("open_native_full_save_after_local_contact")
        confirm_save = labels.index("confirm_native_full_save_after_local_contact")
        fresh_turn = labels.index("audit_saved_turn_after_local_contact_save")
        persisted = labels.index("audit_persisted_local_contact_eligibility")

        self.assertLess(dispatch, local_contact)
        self.assertLess(local_contact, open_save)
        self.assertLess(open_save, confirm_save)
        self.assertLess(confirm_save, fresh_turn)
        self.assertLess(fresh_turn, persisted)
        self.assertNotIn("audit_persisted_force_due_dispatch", labels)
        self.assertEqual(
            steps[confirm_save]["proof_deferred_to_label"],
            "audit_saved_turn_after_local_contact_save",
        )
        self.assertEqual(
            steps[fresh_turn]["baseline_label"],
            "audit_installed_safe_watch_turn_baseline",
        )
        self.assertEqual(steps[fresh_turn]["required_min_delta_turns"], 54000)
        self.assertTrue(steps[fresh_turn]["abort_on_metadata_failure"])
        self.assertEqual(
            steps[fresh_turn]["proof_deferred_to_label"],
            "audit_persisted_local_contact_eligibility",
        )
        self.assertNotIn("required_local_handoff_cohesion_assembled", steps[persisted])
        self.assertTrue(steps[persisted]["required_local_return_eligibility_valid"])
        self.assertFalse(
            steps[persisted]["required_local_return_eligibility_cohesion_assembled"]
        )
        local_contact_step = steps[local_contact]
        self.assertEqual(local_contact_step["kind"], "cockpit_live_session")
        self.assertNotIn("observation_timeout_seconds", local_contact_step)
        self.assertIn("advertised native cockpit actions", local_contact_step["authority"])
        self.assertTrue(any("derive every continuation bound" in item
                            for item in local_contact_step["invariants"]))
        self.assertEqual(
            scenario["proof_route"]["capability_gates"]
            ["capabilities.bandit.natural_structural_scout_dispatch"]["terminal"],
            ["natural_structural_scout_dispatched"],
        )

    def test_natural_return_route_declares_only_the_later_owner_boundary(self) -> None:
        scenario = load_scenario("bandit.r008_natural_return_validation_mcw")
        self.assertEqual(scenario["fixture"], "bandit_r008_natural_safe_watch_footing_v0")
        self.assertEqual(scenario["fixture_profile"], "r008-closure-039-natural")
        self.assertFalse(scenario["runtime_contract"]["grants_gameplay_proof"])
        self.assertFalse(scenario["cockpit_pre_descriptor_bootstrap"]["gameplay_credit"])

        return_step = next(
            step for step in scenario["steps"]
            if step["label"] == "worker_owned_natural_return_through_eligibility"
        )
        self.assertEqual(return_step["kind"], "cockpit_live_session")
        self.assertTrue(any("zero-credit setup" in target for target in return_step["proof_targets"]))
        self.assertTrue(any("minute 9300" in target for target in return_step["proof_targets"]))
        self.assertTrue(any("reused" in invariant for invariant in return_step["invariants"]))

        crossing, relaunch = scenario["proof_gates"]
        predicate = crossing["expectations"][0]["predicate"]
        self.assertEqual(crossing["boundary_step"], return_step["label"])
        self.assertEqual(predicate["transition"], "local_pair_dematerialization")
        self.assertEqual(predicate["actor_ids"], [4, 5])
        self.assertEqual(predicate["generation"], 1)
        self.assertEqual(predicate["handoff_epoch"], 1)
        self.assertEqual(predicate["at_minutes"], 9300)
        self.assertEqual(predicate["previous_state"], "local")
        self.assertEqual(predicate["new_state"], "abstract")

        self.assertEqual(relaunch["boundary_step"], "audit_persisted_natural_return_after_relaunch")
        audit = scenario["post_relaunch"]["steps"][1]
        self.assertEqual(audit["required_active_outing_simulation_owner"], "abstract")
        self.assertEqual(audit["required_active_outing_handoff_epoch"], 2)
        self.assertEqual(audit["required_active_outing_generation"], 1)
        self.assertTrue(audit["required_active_outing_exact_pair"])
        self.assertEqual(audit["required_local_handoff_state"], "abstract_resume")

    def test_pre_cohesion_eligibility_is_not_the_assembled_local_handoff(self) -> None:
        outing = {
            "schema_version": 11,
            "kind": "structural_sortie",
            "activity_id": "site#structural",
            "generation": 1,
            "member_ids": [4, 5],
            "leader_id": 4,
            "simulation_owner": "local",
            "handoff_epoch": 1,
            "local_contact_minutes": 8580,
            "local_return_eligibility": {
                "schema_version": 1,
                "activity_id": "site#structural",
                "actor_ids": [4, 5],
                "generation": 1,
                "owner": "local",
                "handoff_epoch": 1,
                "cohesion_leader_id": 4,
                "cohesion_assembled": False,
                "contact_minutes": 8580,
                "eligible_minutes": 9300,
            },
            "local_handoff": {
                "schema_version": 4,
                "activity_id": "site#structural",
                "activity_generation": 1,
                "handoff_epoch": 1,
                "waypoint_index": 1,
                "phase": "observing",
                "cohesion_leader_id": 4,
                "cohesion_assembled": True,
                "cohesion_abort_return": False,
                "route_position": [175, 13, 0],
                "committed_minutes": 8580,
                "member_ids": [4, 5],
                "members": [],
            },
        }
        payload = {"overmapbuffer": {"bandit_live_world": {"sites": [{
            "site_id": "site", "active_outing": outing,
        }]}}}
        with tempfile.TemporaryDirectory() as directory:
            world = Path(directory)
            (world / "dimension_data.gsav").write_text(
                "1\n" + json.dumps(payload), encoding="utf-8"
            )
            accepted = audit_saved_bandit_live_world_state(
                world,
                required_site_id_contains="site",
                required_local_return_eligibility_valid=True,
                required_local_return_eligibility_cohesion_assembled=False,
            )
            rejected = audit_saved_bandit_live_world_state(
                world,
                required_site_id_contains="site",
                required_local_return_eligibility_valid=True,
                required_local_return_eligibility_cohesion_assembled=True,
            )
        self.assertEqual(accepted["status"], "required_state_present")
        self.assertEqual(rejected["status"], "required_state_missing")

    def test_saved_turn_audit_fails_closed_for_the_installed_pre_save_state(self) -> None:
        scenario = load_scenario("bandit.r008_natural_safe_watch_validation_mcw")
        fixture = HARNESS_DIR / "fixtures/saves/r008-closure-039-natural/bandit_r008_natural_safe_watch_footing_v0"
        with tempfile.TemporaryDirectory() as directory:
            copied = Path(directory) / "fixture"
            shutil.copytree(fixture, copied)
            manifest = json.loads((copied / "manifest.json").read_text(encoding="utf-8"))
            world_dir = copied / "save" / scenario["world"]
            apply_fixture_save_transforms(world_dir, manifest["save_transforms"])
            baseline = audit_saved_game_turn(
                world_dir,
                player_save=scenario["installed_save_player"],
                record_baseline=True,
            )
            stale = audit_saved_game_turn(
                world_dir,
                player_save=scenario["installed_save_player"],
                baseline_metadata=baseline,
                required_min_delta_turns=54000,
            )

        self.assertEqual(baseline["status"], "baseline_recorded")
        self.assertEqual(stale["observed_delta_turns"], 0)
        self.assertTrue(stale["missing_required_min_delta_turns"])
        self.assertEqual(stale["status"], "required_state_missing")

    def test_natural_routes_allow_fixture_install_but_forbid_later_transforms(self) -> None:
        for name in (
            "bandit.r008_natural_safe_watch_footing_mcw",
            "bandit.r008_natural_safe_watch_validation_mcw",
            "bandit.r008_natural_return_validation_mcw",
            "bandit.r008_natural_wait_progress_observation_mcw",
        ):
            with self.subTest(name=name):
                forbidden = load_scenario(name)["runtime_contract"]["forbidden_input"]
                self.assertIn("fixture-save-transform-after-install", forbidden)
                self.assertNotIn("fixture-save-transform", forbidden)

    def test_safe_watch_fixture_installs_the_declared_watch_position(self) -> None:
        scenario = load_scenario("bandit.r008_natural_safe_watch_validation_mcw")
        fixture = HARNESS_DIR / "fixtures/saves/r008-closure-039-natural/bandit_r008_natural_safe_watch_footing_v0"
        with tempfile.TemporaryDirectory() as directory:
            copied = Path(directory) / "fixture"
            shutil.copytree(fixture, copied)
            manifest = json.loads((copied / "manifest.json").read_text(encoding="utf-8"))
            world_dir = copied / "save" / scenario["world"]
            transforms = apply_fixture_save_transforms(world_dir, manifest["save_transforms"])
            self.assertEqual(transforms[0]["target_omt"], [174, 13, 0])
            self.assertEqual(
                load_player_abs_omt(world_dir, scenario["installed_save_player"])[0],
                (174, 13, 0),
            )

    def test_registry_rejects_empty_stale_mismatched_and_fabricated_receipts(self) -> None:
        report = {
            "scenario_manifest": scenario_manifest_binding(self.scenario),
            "scenario_setup": self.receipt(),
        }
        facts = store._extract_report_facts(report)
        with tempfile.TemporaryDirectory() as directory:
            connection = store.open_registry(str(Path(directory) / "registry.sqlite3"))
            try:
                store.rebuild_manifest_projection(connection, HARNESS_DIR / "scenarios")
                store._validate_required_r008_setup_receipt(connection, facts=facts, report=report)
                cases = {
                    "empty": lambda value: value.update({"scenario_setup": {}}),
                    "stale": lambda value: value["scenario_setup"]["interventions"][0]["after_facts"].update(
                        {"master_gsav_sha256": "0" * 64}),
                    "mismatched": lambda value: value["scenario_setup"]["interventions"][0]["arguments"].update(
                        {"world": "OtherWorld"}),
                    "fabricated": lambda value: value["scenario_setup"]["interventions"][0]["native_receipt"].update(
                        {"accepted": False}),
                }
                for name, mutate in cases.items():
                    candidate = copy.deepcopy(report)
                    mutate(candidate)
                    with self.assertRaisesRegex(store.ScenarioRegistryStoreError, "R-008 setup receipt"):
                        store._validate_required_r008_setup_receipt(connection, facts=facts, report=candidate)
            finally:
                connection.close()


if __name__ == "__main__":
    unittest.main()
