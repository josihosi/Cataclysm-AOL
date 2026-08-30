#!/usr/bin/env python3
"""Focused controls for the R-005 wait-only qualification successor."""

from __future__ import annotations

import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from playtest_witness import normalize_witness_charter  # noqa: E402
from scenario_registry import validate_manifest  # noqa: E402
from scenario_registry_store import (  # noqa: E402
    RegistryQueryCandidateSnapshot,
    _first_run_certification_route,
)
from startup_harness import (  # noqa: E402
    audit_structured_transition_event,
    apply_bandit_camp_supply_transform,
    apply_fixture_save_transforms,
    derive_terminal_exit_observation_window,
    native_save_quit_receipt,
    observe_bound_process_exit,
    normalize_fixture_save_transforms,
    r005_source_bound_visibility_setup_receipts_from_installed_save,
    wait_for_pid_exit,
)


HARNESS_DIR = Path(__file__).resolve().parent
SCENARIO_PATH = HARNESS_DIR / "scenarios" / "bandit.r005_native_wait_qualification.json"
CHARTER_PATH = HARNESS_DIR / "charters" / "r005-native-wait-qualification-rev1.json"
QUERY_PATH = HARNESS_DIR / "queries" / "r005-native-wait-qualification.json"
FIXTURE_PATH = HARNESS_DIR / "fixtures" / "saves" / "live-debug" / \
    "bandit_r005_natural_hostile_ecology_v0" / "manifest.json"


class R005NativeWaitQualificationTest(unittest.TestCase):
    def load(self) -> dict:
        return json.loads(SCENARIO_PATH.read_text(encoding="utf-8"))

    def test_successor_is_registry_valid_and_wait_only(self) -> None:
        scenario = self.load()
        validation = validate_manifest(scenario, path=SCENARIO_PATH)
        self.assertEqual(validation["validation"]["status"], "valid")
        self.assertEqual(
            scenario["runtime_contract"]["permitted_input"],
            ["long_wait:1h", "press:S", "press:Y", "press:q", "press:left", "press:enter"],
        )
        self.assertIn("ordinary-overmap-route", scenario["runtime_contract"]["forbidden_input"])
        self.assertFalse(scenario["runtime_contract"]["grants_gameplay_proof"])
        self.assertEqual(scenario["fixture"], "bandit_r005_native_wait_visibility_bootstrap_v0")
        self.assertEqual(scenario["binding_contract"]["camp"]["omt"], [140, 51, 0])
        self.assertEqual(scenario["binding_contract"]["lead"]["target_omt"], [138, 52, 0])
        self.assertEqual(scenario["binding_contract"]["ecology_actors"]["member_ids"], list(range(4, 18)))
        self.assertEqual(scenario["binding_contract"]["scheduler"]["cadence_minutes"], 60)
        self.assertEqual(scenario["binding_contract"]["evidence_ceiling"], "zero-credit")

    def test_positive_route_and_fail_closed_controls_are_ordered(self) -> None:
        scenario = self.load()
        labels = [step["label"] for step in scenario["steps"]]
        waits = [step for step in scenario["steps"] if step["kind"] == "long_wait"]
        # The fourth hourly window ends on the exact pair's physical-return
        # interruption.  The production camp-decision owner runs on the next
        # native scheduler boundary, so the fifth wait is required before the
        # decision audit can observe that committed event.
        self.assertEqual(len(waits), 5)
        self.assertTrue(all(step["expected_duration"] == "1h" for step in waits))
        self.assertEqual(labels[:2], ["preflight_idle_lead", "audit_preflight_idle_camp_structural_lead"])
        self.assertLess(labels.index("wait_1_hour_for_production_dispatch"), labels.index("audit_local_crossing_and_actor_outcomes"))
        self.assertLess(labels.index("audit_local_crossing_and_actor_outcomes"), labels.index("wait_1_hour_for_return_report_and_camp_decision"))
        self.assertLess(labels.index("wait_1_hour_for_return_report_and_camp_decision"), labels.index("audit_return_and_camp_decision"))
        self.assertLess(labels.index("audit_return_and_camp_decision"), labels.index("open_native_save_quit_after_wait_lifecycle"))
        quit_step = next(
            step for step in scenario["steps"]
            if step["label"] == "open_native_main_menu_quit_confirmation_after_wait_lifecycle"
        )
        self.assertEqual(quit_step["keys"], ["q"])
        self.assertEqual(
            quit_step["semantic_ui_expectation"],
            {"intent": "main_menu_quit_confirmation", "valid_actions": ["left", "enter"]},
        )
        self.assertTrue(quit_step["abort_on_semantic_ui_failure"])
        self.assertEqual(
            scenario["post_relaunch"]["terminal_save_step_label"],
            "confirm_native_process_exit_after_wait_lifecycle",
        )
        self.assertTrue(all(step.get("abort_on_metadata_failure") for step in scenario["steps"] if step["kind"].startswith("audit_")))
        self.assertIn("player travel", json.dumps(scenario["evidence_contract"]).lower())
        self.assertIn("missing owner transition", scenario["evidence_contract"]["failure_rule"])

    def test_camp_decision_observation_is_exact_and_fail_closed(self) -> None:
        scenario = self.load()
        audit = next(step for step in scenario["steps"]
                     if step["label"] == "audit_return_and_camp_decision")
        self.assertEqual(audit["kind"], "audit_structured_transition_event")
        predicate = audit["predicate"]
        event = {
            "run_id": "fresh-run", "sequence": 6, "domain": "bandit_live_world",
            "transition": "camp_decision", "outcome": "committed",
            "site_id": "overmap_special:bandit_camp@140,51,0",
            "operation_id": "overmap_special:bandit_camp@140,51,0#structural",
            "generation": 1, "simulation_owner": "abstract",
            "previous_state": "idle", "new_state": "report_awaiting_assessment",
        }
        self.assertEqual(
            audit_structured_transition_event(
                [event], run_id="fresh-run", predicate=predicate, require_exactly_one=True,
            )["status"], "required_state_present",
        )
        self.assertEqual(
            audit_structured_transition_event(
                [event], run_id="wrong-run", predicate=predicate, require_exactly_one=True,
            )["status"], "required_state_missing",
        )
        self.assertEqual(
            audit_structured_transition_event(
                [event, event], run_id="fresh-run", predicate=predicate, require_exactly_one=True,
            )["status"], "required_state_missing",
        )

    @unittest.skipUnless(os.name == "posix", "POSIX child reaping is platform-specific")
    def test_native_save_quit_child_exit_is_not_mistaken_for_a_live_zombie(self) -> None:
        process = subprocess.Popen([sys.executable, "-c", "pass"])
        self.assertTrue(wait_for_pid_exit(process.pid, 5.0))
        self.assertIsNotNone(process.poll())

    def test_terminal_window_is_measured_plus_poll_uncertainty_and_ceiling_is_separate(self) -> None:
        result = derive_terminal_exit_observation_window(
            [60.2, 120.4, 180.7],
            scheduling_uncertainty_seconds=0.1,
            safety_ceiling_seconds=240.0,
        )
        self.assertEqual(result["status"], "within_safety_ceiling")
        self.assertAlmostEqual(result["derived_observation_window_seconds"], 180.8)
        exceeded = derive_terminal_exit_observation_window(
            [180.7], scheduling_uncertainty_seconds=0.1, safety_ceiling_seconds=180.0
        )
        self.assertEqual(exceeded["status"], "exceeds_safety_ceiling")

    @unittest.skipUnless(os.name == "posix", "POSIX process identity controls are platform-specific")
    def test_bound_observer_rejects_wrong_command_and_never_calls_cleanup(self) -> None:
        process = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(0.2)"])
        command = " ".join(process.args)
        observed = observe_bound_process_exit(
            process.pid, 2.0, expected_command="definitely-not-the-child"
        )
        self.assertEqual(observed["status"], "wrong_process_identity")
        process.terminate()
        process.wait()

    def test_native_save_quit_receipt_requires_one_same_run_open_return_pair(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            trace = Path(temporary_directory) / "debug.log"
            trace.write_text(
                "07:48:00.000 INFO : openclaw_harness_ui_trace: component=semantic_ui "
                "event=open instance_id=\"save-quit-1\" run_id=\"run-1\" "
                "intent=\"save_quit_confirmation\" valid_actions=[\"Y\"] "
                "postcondition=\"save_quit_confirmation_resolved\"\n"
                "07:48:01.000 INFO : openclaw_harness_ui_trace: component=semantic_ui "
                "event=return instance_id=\"save-quit-1\" run_id=\"run-1\" "
                "intent=\"save_quit_confirmation\" valid_actions=[\"Y\"] "
                "postcondition=\"save_quit_confirmation_resolved\"\n",
                encoding="utf-8",
            )
            receipt = native_save_quit_receipt(trace, 0, run_id="run-1")
        self.assertEqual(receipt["status"], "matched")
        self.assertEqual(receipt["pair"]["return"]["timestamp"], "07:48:01.000")
        self.assertEqual(native_save_quit_receipt(trace, 0, run_id="wrong" )["status"], "missing_or_ambiguous")

    def test_terminal_observer_captures_the_initial_command_before_any_steps(self) -> None:
        source = (HARNESS_DIR / "startup_harness.py").read_text(encoding="utf-8")
        capture = source.index("initial_process_command = pid_command(pid)")
        steps = source.index("step_reports = execute_probe_steps(", capture)
        relaunch = source.index("initial_process_command=initial_process_command", steps)
        self.assertLess(capture, steps)
        self.assertLess(steps, relaunch)

    def test_source_bound_supply_bootstrap_is_exact_and_fail_closed(self) -> None:
        fixture = json.loads(FIXTURE_PATH.read_text(encoding="utf-8"))
        transforms = normalize_fixture_save_transforms(
            fixture["save_transforms"], manifest_path=FIXTURE_PATH
        )
        bootstrap = next(item for item in transforms if item["kind"] == "bandit_camp_supply")
        self.assertEqual(bootstrap["site_id"], "overmap_special:bandit_camp@140,51,0")
        self.assertEqual(bootstrap["supply_units"], 40)
        self.assertEqual(bootstrap["supply_last_update_minutes"], 8100)
        self.assertEqual(bootstrap["supply_accounted_living_total"], 14)
        self.assertEqual(bootstrap["supply_member_minute_remainder"], 0)
        self.assertEqual(
            bootstrap["source_key"],
            "fixture_r005_natural_hostile_ecology_supply_bootstrap",
        )
        with tempfile.TemporaryDirectory() as temporary_directory:
            world_dir = Path(temporary_directory)
            dimension_path = world_dir / "dimension_data.gsav"
            dimension_path.write_text(
                "version\n" + json.dumps({
                    "overmapbuffer": {"bandit_live_world": {"sites": [{
                        "site_id": bootstrap["site_id"],
                        "supply_units": 96,
                        "supply_last_update_minutes": 8100,
                        "supply_accounted_living_total": 14,
                        "supply_member_minute_remainder": 0,
                    }]}},
                }, separators=(",", ":")),
                encoding="utf-8",
            )
            receipt = apply_bandit_camp_supply_transform(world_dir, bootstrap)
        self.assertEqual(receipt["old_supply_units"], 96)
        self.assertEqual(receipt["supply_units"], 40)
        self.assertEqual(receipt["source_key"], bootstrap["source_key"])
        with self.assertRaises(SystemExit):
            normalize_fixture_save_transforms([{
                "kind": "bandit_camp_supply",
                "player_save": "#Wm9yYWlkYSBWaWNr.sav.zzip",
                "site_id": "overmap_special:bandit_camp@140,51,0",
                "supply_units": 257,
                "supply_last_update_minutes": 8100,
                "supply_accounted_living_total": 14,
                "supply_member_minute_remainder": 0,
            }], manifest_path=FIXTURE_PATH)

    def test_visibility_bootstrap_is_exact_and_zero_credit(self) -> None:
        scenario = self.load()
        contract = scenario["setup_contract"]
        self.assertEqual(contract["kind"], "r005_source_bound_visibility_fixture")
        self.assertEqual(contract["camp_site_id"], "overmap_special:bandit_camp@140,51,0")
        self.assertEqual(contract["lead_id"], "overmap_special:bandit_camp@140,51,0#lead:structural_bounty:forest@138,52,0")
        self.assertEqual(contract["target_omt"], [138, 52, 0])
        self.assertEqual(contract["candidate_omt"], [141, 49, 0])
        self.assertEqual(contract["player_source_omt"], [140, 41, 0])
        self.assertEqual(contract["player_observation_omt"], [141, 49, 0])
        self.assertEqual(contract["player_offset_ms"], [24, 192, 0])
        self.assertEqual(contract["candidate_terrain_id"], "communitygarden_east")
        self.assertEqual(contract["intervening_terrain_ids"], ["field", "field"])
        self.assertEqual(contract["candidate_see_cost"], 1)
        self.assertEqual(contract["clear_day_sight_points"], 3)
        self.assertEqual(contract["intervening_see_costs"], [0, 0])
        self.assertEqual(
            contract["candidate_see_cost"] + sum(contract["intervening_see_costs"]),
            1,
        )
        self.assertEqual(contract["evidence_ceiling"], "zero-credit")

    def test_visibility_bootstrap_rejects_exposed_unsafe_identity_or_stale_binding(self) -> None:
        scenario = self.load()
        contract = scenario["setup_contract"]
        raw_fixture = HARNESS_DIR / "fixtures" / "saves" / "live-debug" / \
            "tmp_bandit_live_world_local_contact_raw_2026-04-23"
        child_manifest_path = HARNESS_DIR / "fixtures" / "saves" / "live-debug" / \
            "bandit_r005_native_wait_visibility_bootstrap_v0" / "manifest.json"
        parent_manifest = json.loads(FIXTURE_PATH.read_text(encoding="utf-8"))
        child_manifest = json.loads(child_manifest_path.read_text(encoding="utf-8"))
        with tempfile.TemporaryDirectory() as temporary_directory:
            fixture = Path(temporary_directory) / "fixture"
            shutil.copytree(raw_fixture, fixture)
            world_dir = fixture / "save" / "McWilliams"
            transforms = normalize_fixture_save_transforms(
                parent_manifest["save_transforms"], manifest_path=FIXTURE_PATH,
            ) + normalize_fixture_save_transforms(
                child_manifest["save_transforms"], manifest_path=child_manifest_path,
            )
            applied = apply_fixture_save_transforms(world_dir, transforms)
            cases = {
                "blocked_sight": {"intervening_terrain_ids": ["forest", "field"]},
                "exposed_footing": {"candidate_omt": [141, 50, 0]},
                "unsafe_terrain": {"candidate_terrain_id": "forest_water"},
                "wrong_observation_footing": {"player_observation_omt": [141, 50, 0]},
                "wrong_player_offset": {"player_offset_ms": [0, 0, 0]},
                "identity_drift": {"camp_site_id": "overmap_special:bandit_camp@999,999,0"},
                "lead_drift": {"lead_id": "stale-lead"},
                "stale_binding": {"fixture": "bandit_r005_natural_hostile_ecology_v0"},
            }
            for label, changes in cases.items():
                mutated = json.loads(json.dumps(contract))
                mutated.update(changes)
                receipt = r005_source_bound_visibility_setup_receipts_from_installed_save(
                    world_dir,
                    setup_contract=mutated,
                    fixture_install={
                        "fixture": mutated["fixture"],
                        "manifest": child_manifest,
                        "applied_save_transforms": applied,
                    },
                )
                self.assertEqual(receipt["status"], "unprepared", label)
                self.assertFalse(receipt["interventions"][0]["native_receipt"]["accepted"], label)

    def test_visibility_bootstrap_receipt_is_bound_and_cleanup_safe(self) -> None:
        scenario = self.load()
        contract = scenario["setup_contract"]
        raw_fixture = HARNESS_DIR / "fixtures" / "saves" / "live-debug" / \
            "tmp_bandit_live_world_local_contact_raw_2026-04-23"
        child_manifest_path = HARNESS_DIR / "fixtures" / "saves" / "live-debug" / \
            "bandit_r005_native_wait_visibility_bootstrap_v0" / "manifest.json"
        parent_manifest = json.loads(FIXTURE_PATH.read_text(encoding="utf-8"))
        child_manifest = json.loads(child_manifest_path.read_text(encoding="utf-8"))
        with tempfile.TemporaryDirectory() as temporary_directory:
            fixture = Path(temporary_directory) / "fixture"
            shutil.copytree(raw_fixture, fixture)
            world_dir = fixture / "save" / "McWilliams"
            transforms = normalize_fixture_save_transforms(
                parent_manifest["save_transforms"], manifest_path=FIXTURE_PATH,
            ) + normalize_fixture_save_transforms(
                child_manifest["save_transforms"], manifest_path=child_manifest_path,
            )
            applied = apply_fixture_save_transforms(world_dir, transforms)
            receipt = r005_source_bound_visibility_setup_receipts_from_installed_save(
                world_dir,
                setup_contract=contract,
                fixture_install={
                    "fixture": contract["fixture"],
                    "manifest": child_manifest,
                    "applied_save_transforms": applied,
                },
            )
            self.assertEqual(receipt["status"], "prepared")
            intervention = receipt["interventions"][0]
            self.assertTrue(intervention["native_receipt"]["accepted"])
            self.assertEqual(intervention["after_facts"]["target_terrain_id"], "forest")
            self.assertEqual(intervention["after_facts"]["candidate_terrain_id"], "communitygarden_east")
            self.assertEqual(intervention["after_facts"]["player_abs_omt"], [141, 49, 0])
            self.assertTrue(intervention["after_facts"]["player_position_match"])
            self.assertTrue(intervention["after_facts"]["line_of_sight_within_budget"])
            self.assertFalse(intervention["gameplay_credit"])
            self.assertEqual(intervention["evidence_effect"], "none_for_manufactured_state")

    def test_charter_is_zero_credit_and_rejects_player_control_evidence(self) -> None:
        charter = normalize_witness_charter(json.loads(CHARTER_PATH.read_text(encoding="utf-8")))
        self.assertEqual(charter["requested_evidence_ceiling"], "zero-credit")
        self.assertIn("ordinary native waiting alone", charter["claim"])
        self.assertIn("R-005-closure-056", " ".join(charter["already_accepted_evidence"]))
        self.assertTrue(any("player-travel" in item for item in charter["forbidden_shortcuts"]))
        self.assertTrue(any("binding drift" in item for item in charter["honest_stop_conditions"]))

    def test_registry_first_run_route_is_exact_and_fail_closed(self) -> None:
        scenario = self.load()
        manifest = {
            "name": scenario["name"],
            "source_path": str(SCENARIO_PATH),
            "sha256": "a" * 64,
            "validation": {"status": "valid", "review_required": False},
        }
        snapshot = RegistryQueryCandidateSnapshot(
            scenario_id="r005-wait",
            lifecycle_state="active",
            token_eligible=True,
            facts={"capabilities.bandit.r005.native_wait_qualification": {
                "value": "source_bound_native_wait_only_structural_outing_lifecycle",
            }},
            explanation={"manifest": manifest},
        )
        route = _first_run_certification_route(snapshot)
        self.assertIsNotNone(route)
        self.assertEqual(route["internal_resolution_state"], "first_run")
        self.assertTrue(route["details"]["first_run"])
        rejected = RegistryQueryCandidateSnapshot(
            scenario_id="r005-wait",
            lifecycle_state="active",
            token_eligible=True,
            facts={"capabilities.bandit.r005.native_wait_qualification": {
                "value": "player_travel_route",
            }},
            explanation={"manifest": manifest},
        )
        self.assertIsNone(_first_run_certification_route(rejected))

    def test_typed_selection_requires_wait_only_outcome_and_prepared_footing(self) -> None:
        """The closure query must not silently fall back to a player-travel route."""
        scenario = self.load()
        requirements = {
            key: {"value": value, "evidence_state": "declared", "present": True}
            for key, value in scenario["capabilities"].items()
            if key in {
                "capabilities.bandit.r005.native_wait_qualification",
                "runtime.r005.source_binding",
                "runtime.r005.route_mode",
                "runtime.r005.player_travel",
                "runtime.r005.fixture_footing",
                "runtime.r005.outcome",
                "runtime.evidence_ceiling",
                "world.id",
            }
        }
        request = {
            "requirements": [
                {"key": key, "op": "eq", "value": value["value"], "minimum_evidence": "declared"}
                for key, value in requirements.items()
            ],
            "preferences": [],
        }
        from scenario_registry_store import evaluate_registry_query, parse_registry_query_request
        typed = parse_registry_query_request(request)
        positive = evaluate_registry_query(
            typed,
            [{"scenario_id": "wait-only", "facts": requirements}],
        )
        self.assertEqual(positive.ranked_scenario_ids, ("wait-only",))

        player_travel = dict(requirements)
        player_travel["runtime.r005.route_mode"] = {
            **player_travel["runtime.r005.route_mode"], "value": "ordinary_overmap_route",
        }
        rejected = evaluate_registry_query(
            typed,
            [{"scenario_id": "player-travel", "facts": player_travel}],
        )
        self.assertEqual(rejected.ranked_scenario_ids, ())
        route_result = rejected.candidates[0].hard_results
        route_mode = next(item for item in route_result if item.key == "runtime.r005.route_mode")
        self.assertFalse(route_mode.passed)
        self.assertEqual(route_mode.reason, "equality_mismatch")

    def test_typed_selection_rejects_wrong_fixture_and_missing_binding_facts(self) -> None:
        scenario = self.load()
        expected = scenario["capabilities"]
        query = {
            "requirements": [
                {"key": "capabilities.bandit.r005.native_wait_qualification", "op": "eq",
                 "value": expected["capabilities.bandit.r005.native_wait_qualification"],
                 "minimum_evidence": "declared"},
                {"key": "runtime.r005.fixture_footing", "op": "eq",
                 "value": expected["runtime.r005.fixture_footing"], "minimum_evidence": "declared"},
                {"key": "runtime.r005.source_binding", "op": "eq",
                 "value": expected["runtime.r005.source_binding"], "minimum_evidence": "declared"},
            ],
            "preferences": [],
        }
        from scenario_registry_store import evaluate_registry_query, parse_registry_query_request
        typed = parse_registry_query_request(query)
        wrong_fixture = {
            key: {"value": value, "evidence_state": "declared", "present": True}
            for key, value in expected.items()
            if key in {"capabilities.bandit.r005.native_wait_qualification", "runtime.r005.fixture_footing",
                       "runtime.r005.source_binding"}
        }
        wrong_fixture["runtime.r005.fixture_footing"]["value"] = "bandit_r005_natural_hostile_ecology_v0"
        result = evaluate_registry_query(
            typed, [{"scenario_id": "wrong-footing", "facts": wrong_fixture}],
        )
        self.assertEqual(result.ranked_scenario_ids, ())
        self.assertEqual(
            next(item for item in result.candidates[0].hard_results
                 if item.key == "runtime.r005.fixture_footing").reason,
            "equality_mismatch",
        )
        missing_binding = dict(wrong_fixture)
        missing_binding.pop("runtime.r005.source_binding")
        missing_result = evaluate_registry_query(
            typed, [{"scenario_id": "missing-binding", "facts": missing_binding}],
        )
        self.assertEqual(missing_result.ranked_scenario_ids, ())
        self.assertEqual(
            next(item for item in missing_result.candidates[0].hard_results
                 if item.key == "runtime.r005.source_binding").reason,
            "unknown_fact",
        )
        wrong_executable = dict(wrong_fixture)
        wrong_executable["runtime.r005.source_binding"] = {
            **wrong_executable["runtime.r005.source_binding"],
            "value": "current-worktree:cataclysm-tlg-tiles:dev-harness",
        }
        wrong_executable_result = evaluate_registry_query(
            typed, [{"scenario_id": "wrong-executable", "facts": wrong_executable}],
        )
        self.assertEqual(wrong_executable_result.ranked_scenario_ids, ())
        self.assertEqual(
            next(item for item in wrong_executable_result.candidates[0].hard_results
                 if item.key == "runtime.r005.source_binding").reason,
            "equality_mismatch",
        )

    def test_canonical_query_is_typed_to_the_wait_only_outcome(self) -> None:
        from scenario_registry_store import parse_registry_query_request
        query = json.loads(QUERY_PATH.read_text(encoding="utf-8"))
        request = parse_registry_query_request(query)
        self.assertEqual(request.preferences, ())
        self.assertEqual(
            {predicate.key for predicate in request.requirements},
            {
                "capabilities.bandit.r005.native_wait_qualification",
                "runtime.r005.source_binding",
                "runtime.r005.route_mode",
                "runtime.r005.player_travel",
                "runtime.r005.fixture_footing",
                "runtime.r005.outcome",
                "runtime.evidence_ceiling",
                "world.id",
            },
        )
        scenario = self.load()
        facts = {
            key: {"value": value, "evidence_state": "declared", "present": True}
            for key, value in scenario["capabilities"].items()
        }
        from scenario_registry_store import evaluate_registry_query
        evaluation = evaluate_registry_query(
            request, [{"scenario_id": scenario["name"], "facts": facts}],
        )
        self.assertEqual(evaluation.ranked_scenario_ids, (scenario["name"],))

    def test_canonical_query_selects_only_wait_manifest_from_registry_projection(self) -> None:
        from scenario_registry_store import (
            build_registry_query_candidate_snapshot,
            evaluate_registry_query,
            open_registry,
            parse_registry_query_request,
            rebuild_manifest_projection,
        )
        query = parse_registry_query_request(
            json.loads(QUERY_PATH.read_text(encoding="utf-8"))
        )
        with tempfile.TemporaryDirectory() as temporary_directory:
            registry = open_registry(str(Path(temporary_directory) / "registry.sqlite3"))
            try:
                rebuild_manifest_projection(registry, HARNESS_DIR / "scenarios")
                snapshots = build_registry_query_candidate_snapshot(registry)
                evaluation = evaluate_registry_query(
                    query,
                    tuple({"scenario_id": item.scenario_id, "facts": item.facts}
                          for item in snapshots),
                )
                self.assertEqual(len(evaluation.ranked_scenario_ids), 1)
                selected = next(
                    item for item in snapshots
                    if item.scenario_id == evaluation.ranked_scenario_ids[0]
                )
                self.assertEqual(
                    selected.explanation["manifest"]["name"],
                    "bandit.r005_native_wait_qualification",
                )
                self.assertEqual(
                    selected.explanation["manifest"]["known_footing"]["fixture"],
                    "bandit_r005_native_wait_visibility_bootstrap_v0",
                )
            finally:
                registry.close()


if __name__ == "__main__":
    unittest.main()
