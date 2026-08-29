#!/usr/bin/env python3
"""Contract tests for the zero-credit R-019 off-switch interruption source."""

from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

from startup_harness import (  # noqa: E402
    build_probe_step_ledger,
    debug_spawn_monster_intervention_receipt,
    evaluate_structured_proof_gates,
    load_scenario,
)
import scenario_registry_store as registry_store  # noqa: E402


class R019OffInterruptionBootstrapTest(unittest.TestCase):
    def test_selected_closure_bootstrap_issues_single_use_first_run_authority(self) -> None:
        source = HARNESS_DIR / "scenarios" / "r019.keep_watch_off_interruption_closure057_bootstrap_mcw.json"
        request = registry_store.parse_registry_query_request({
            "requirements": [{
                "key": "capabilities.r019.off_interruption_bootstrap",
                "op": "eq",
                "value": "native_debug_spawned_hostile_exposes_current_activity_interruption",
                "minimum_evidence": "declared",
            }],
            "preferences": [],
        })
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            scenarios = root / "scenarios"
            scenarios.mkdir()
            manifest = scenarios / source.name
            manifest.write_bytes(source.read_bytes())
            connection = registry_store.open_registry(str(root / "registry.sqlite3"))
            try:
                registry_store.rebuild_manifest_projection(connection, scenarios)
                selected = registry_store.execute_registry_query(connection, request, drafts_root=root / "drafts")
                self.assertIsNotNone(selected.token_id)
                self.assertIsNone(selected.draft_path)

                launch = registry_store.reload_selection_token_for_launch(connection, str(selected.token_id))
                self.assertTrue(launch.accepted)
                self.assertEqual(launch.reason, "first_run_bootstrap")
                self.assertEqual(launch.scenario, manifest.stem)

                route = registry_store._current_route_evidence(
                    connection,
                    connection.execute("SELECT manifest_id FROM manifest_current").fetchone()[0],
                )
                stale_route = {**route[0], "evidence_state": "stale"}
                with mock.patch.object(registry_store, "_current_route_evidence", return_value=(stale_route,)):
                    stale = registry_store.reload_selection_token_for_launch(connection, str(selected.token_id))
                self.assertFalse(stale.accepted)
                self.assertEqual(stale.reason, "first_run_route_binding_changed")
            finally:
                connection.close()

    def test_first_run_authority_rejects_ineligible_closure_bootstrap(self) -> None:
        snapshot = registry_store.RegistryQueryCandidateSnapshot(
            scenario_id="r019-closure", lifecycle_state="active", token_eligible=False,
            facts={
                "capabilities.r019.off_interruption_bootstrap": {
                    "value": "native_debug_spawned_hostile_exposes_current_activity_interruption",
                },
                "runtime.r019.source_binding": {"value": "r019_keep_watch_off_positive_progress_v1:r009-m095"},
            },
            explanation={"manifest": {
                "name": "r019.keep_watch_off_interruption_closure057_bootstrap_mcw",
                "source_path": "scenario.json", "sha256": "a" * 64,
                "validation": {"status": "valid", "review_required": False},
            }},
        )
        self.assertIsNone(registry_store._first_run_certification_route(snapshot))

    def test_partial_progress_validation_issues_only_its_current_first_run_authority(self) -> None:
        manifest = {
            "name": "r019.keep_watch_off_interruption_closure059_validation_mcw",
            "source_path": "tools/openclaw_harness/scenarios/"
                           "r019.keep_watch_off_interruption_closure059_validation_mcw.json",
            "sha256": "a" * 64,
            "validation": {"status": "valid", "review_required": False},
        }
        current = registry_store.RegistryQueryCandidateSnapshot(
            scenario_id="r019-partial-progress", lifecycle_state="active", token_eligible=True,
            facts={
                "capabilities.r019.off_interruption_closure059_validation": {
                    "value": "fresh_disabled_master_primitive_wait_stops_at_current_native_hostile_"
                             "interruption_with_source_bound_partial_progress",
                },
                "runtime.r019.source_binding": {
                    "value": "r019_keep_watch_off_positive_progress_v1:r009-m095"
                },
            },
            explanation={"manifest": manifest},
        )
        self.assertIsNotNone(registry_store._first_run_certification_route(current))

        stale = registry_store.RegistryQueryCandidateSnapshot(
            scenario_id="r019-stale", lifecycle_state="active", token_eligible=True,
            facts={
                **current.facts,
                "capabilities.r019.off_interruption_closure059_validation": {
                    "value": "fresh_disabled_master_primitive_wait_stops_at_current_native_hostile_"
                             "interruption",
                },
            },
            explanation={"manifest": manifest},
        )
        self.assertIsNone(registry_store._first_run_certification_route(stale))

    def test_native_spawn_source_is_separate_and_zero_credit(self) -> None:
        scenario = load_scenario("r019.keep_watch_off_interruption_bootstrap_mcw")

        self.assertEqual(scenario["fixture"], "r013_clean_wait_duration_v1")
        self.assertEqual(
            scenario["capabilities"]["capabilities.r019.off_interruption_bootstrap"],
            "native_debug_spawned_hostile_exposes_current_activity_interruption",
        )
        contract = scenario["runtime_contract"]
        self.assertIn("debug:spawn", contract["permitted_input"])
        self.assertIn("cockpit:game.keep_watch", contract["forbidden_input"])
        self.assertIn("semantic:activity.ignore", contract["forbidden_input"])
        self.assertFalse(contract["grants_gameplay_proof"])

        spawn = scenario["steps"][2]
        self.assertEqual(spawn["kind"], "debug_spawn_monster")
        self.assertEqual(spawn["monster_query"], "zombie dog")
        self.assertEqual(spawn["creature_id"], "mon_zombie_dog")
        self.assertEqual(spawn["target_offset"], [6, 0, 0])
        self.assertEqual(spawn["target_keys"], ["right"] * 6)

        trigger = scenario["steps"][3]
        self.assertEqual(trigger["action_chain"], ["world.wait", "wait.duration_menu", "wait.1m"])
        self.assertEqual(trigger["expected_final_action"], "activity.ignore")
        self.assertFalse(trigger["gameplay_credit"])

    def test_exact_setup_and_hostile_interruption_receipts_are_required_together(self) -> None:
        authority = {
            "authority": "registry", "authority_id": "authority-1",
            "binding_id": "binding-1", "source_sha256": "source-1",
        }
        setup = debug_spawn_monster_intervention_receipt(
            creature_id="mon_zombie_dog", target_offset=[6, 0, 0],
            target_keys=["right"] * 6, group_radius=0, friendly=False,
            hallucination=False, run_id="run-1", registry_authority=authority,
            run_dir=Path("/tmp/r019"),
        )
        interruption = {
            "artifact_kind": "native_cockpit_transaction", "run_id": "run-1",
            "gameplay_credit": False,
            "r019_hostile_interruption": {
                "type": "hostile_spotted_near", "advertised_action": "activity.ignore",
                "advertised_action_dispatched": False, "creature_id": "mon_zombie_dog",
                "target_offset": [6, 0, 0],
            },
        }
        reports = [
            {"index": 2, "kind": "debug_spawn_monster", "label": "spawn", "metadata": setup},
            {"index": 3, "kind": "cockpit_act", "label": "interrupt", "metadata": interruption},
        ]
        gates = [{
            "id": "r019_off_interruption_source_bound", "boundary_step": "interrupt",
            "predecessors": [], "expectations": [
                {"kind": "saved_artifact", "predicate": {
                    "artifact_kind": "native_debug_setup_intervention", "creature_id": "mon_zombie_dog",
                    "target_offset": [6, 0, 0], "spawn_count": 1, "gameplay_credit": False,
                    "same_run": True,
                }},
                {"kind": "saved_artifact", "predicate": {
                    "artifact_kind": "native_cockpit_transaction", "gameplay_credit": False,
                    "same_run": True,
                }},
            ],
        }]
        watermarks = {"interrupt": {"run_id": "run-1", "step_index": 3}}
        for index, artifact in enumerate((setup, interruption), start=2):
            artifact["producer_step_index"] = index
        evidence = evaluate_structured_proof_gates(
            gates, events=[], watermarks=watermarks, saved_artifacts=[setup, interruption], run_id="run-1",
        )
        ledger = build_probe_step_ledger(
            reports, wec_authority=authority, report_binding_id="binding-1",
            scenario_source_sha256="source-1", structured_gate_evidence=evidence,
        )
        self.assertEqual([row["verdict"] for row in ledger], [
            "green_step_setup_intervention_receipt", "green_step_r019_hostile_interruption_receipt",
        ])

        interruption["r019_hostile_interruption"]["advertised_action_dispatched"] = True
        rejected = build_probe_step_ledger(
            reports, wec_authority=authority, report_binding_id="binding-1",
            scenario_source_sha256="source-1", structured_gate_evidence=evidence,
        )
        self.assertEqual(rejected[1]["verdict"], "yellow_step_r019_hostile_interruption_receipt_unbound")


if __name__ == "__main__":
    unittest.main()
