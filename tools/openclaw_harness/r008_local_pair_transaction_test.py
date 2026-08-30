#!/usr/bin/env python3
"""Contract tests for R-008's controlled zero-credit local-pair setup."""

from __future__ import annotations

import copy
import hashlib
import json
import sys
import tempfile
import unittest
from pathlib import Path


HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

from r008_local_pair_transaction import (  # noqa: E402
    LocalPairTransactionError, run_base_fixture_transaction, run_local_pair_transaction,
    verify_canonical_local_pair_snapshot,
)
from startup_harness import audit_saved_bandit_live_world_state  # noqa: E402


SITE_ID = "overmap_special:bandit_camp@177,13,0"
OPERATION_ID = SITE_ID + "#structural"


def native_report() -> dict:
    return {"events": [
        {"sequence": 1, "run_id": "fresh-native-capture", "game_minutes": 8520, "domain": "bandit_live_world",
         "transition": "active_sortie_dispatch", "outcome": "committed", "site_id": SITE_ID,
         "operation_id": OPERATION_ID, "actor_ids": [4, 5], "generation": 1,
         "simulation_owner": "abstract", "previous_state": "at_home", "new_state": "outbound",
         "handoff_epoch": 0},
        {"sequence": 2, "run_id": "fresh-native-capture", "game_minutes": 8580, "domain": "bandit_live_world",
         "transition": "local_pair_handoff", "outcome": "committed", "site_id": SITE_ID,
         "operation_id": OPERATION_ID, "actor_ids": [4, 5], "generation": 1,
         "simulation_owner": "local", "previous_state": "abstract", "new_state": "local",
         "handoff_epoch": 1},
    ]}


def clean_world() -> dict:
    return {"overmapbuffer": {"bandit_live_world": {
        "schema_version": 7, "owner_id": "hells_raiders_live_owner_v0",
        "routine_scheduler_cursor": 0, "routine_terrain_scan_cursor": 0,
        "routine_scheduler_last_hour": -1, "sites": [], "hostile_target_opportunities": [],
    }}}


def canonical_snapshot() -> dict:
    handoff_member = {
        "npc_id": 4, "prior_position": [700, 52, 0], "entry_position": [700, 52, 0],
        "staging_position": [701, 52, 0], "exit_position": [700, 52, 0],
        "hp_percent": 100, "dead": False,
    }
    handoff = {
        "schema_version": 4, "activity_id": OPERATION_ID, "activity_generation": 1,
        "handoff_epoch": 1, "waypoint_index": 1, "phase": "outbound",
        "route_position": [175, 13, 0], "approach_from": [177, 13, 0],
        "egress_omt": [174, 13, 0], "cargo": {"supply_units": 0, "trade_value": 0},
        "casualty_ids": [], "members": [handoff_member, {**handoff_member, "npc_id": 5,
        "prior_position": [701, 52, 0], "entry_position": [701, 52, 0],
        "staging_position": [702, 52, 0], "exit_position": [701, 52, 0]}],
        "cohesion_leader_id": 4, "cohesion_deadline_minutes": 8700,
        "cohesion_reroutes_used": 0, "cohesion_assembled": False,
        "cohesion_abort_return": False, "cohesion_best_staging_distances": [1, 1],
        "committed_minutes": 8580,
    }
    outing = {
        "schema_version": 11, "kind": "structural_sortie", "activity_id": OPERATION_ID,
        "camp_id": SITE_ID, "generation": 1, "member_ids": [4, 5], "leader_id": 4,
        "shared_route": [[177, 13, 0], [175, 13, 0], [174, 13, 0]], "waypoint_index": 1,
        "target_id": "field", "target_omt": [174, 13, 0], "job_type": "scout",
        "target_lead_id": "field-lead", "target_lead_revision": 1, "phase": "outbound",
        "observations": [], "cargo": {"supply_units": 0, "trade_value": 0}, "casualty_ids": [],
        "resolved_member_ids": [], "started_minutes": 8520, "local_contact_minutes": 8580,
        "last_progress_minutes": 8580, "expected_return_minutes": 9300,
        "missing_deadline_minutes": 10740, "simulation_owner": "local", "handoff_epoch": 1,
        "last_advanced_minutes": 8580, "local_projection_reconciliation_rejected": False,
        "return_application_key": "", "report_application_key": "", "cargo_application_key": "",
        "member_return_receipts": [], "local_return_eligibility": {"schema_version": 1,
        "activity_id": OPERATION_ID, "actor_ids": [4, 5], "generation": 1, "owner": "local",
        "handoff_epoch": 1, "cohesion_leader_id": 4, "cohesion_assembled": False,
        "contact_minutes": 8580, "eligible_minutes": 9300}, "crossing": {"activity_id": OPERATION_ID},
        "local_handoff": handoff, "abstract_encounter": {}, "abstract_detour_attempts": 0,
        "has_withdrawal_detour": False, "withdrawal_detour_omt": [0, 0, 0],
        "target_footprint": [[174, 13, 0]], "selected_watch_kind": "overwatch",
        "selected_watch_omt": [175, 13, 0], "selected_watch_route_cost": 2,
        "alternate_watch_kind": "overwatch", "alternate_watch_omt": [174, 13, 0],
        "alternate_watch_route_cost": 3, "alternate_watch_shared_route": [[175, 13, 0]],
        "alternate_watch_attempted": False, "alternate_watch_reposition_pending": False,
        "covert_egress_chain_version": 1, "covert_egress_attempts": 0,
        "covert_egress_revision": 0, "failed_covert_egress_omts": [],
        "current_covert_egress_route_omts": [], "failed_covert_egress_route_omts": [],
        "assessment": {},
    }
    site = {
        "schema_version": 12, "site_id": SITE_ID, "source_kind": "overmap_special",
        "site_kind": "bandit_camp", "hostile_profile": "camp_style", "source_id": "bandit_camp",
        "anchor": [177, 13, 0], "living_total": 2, "supply_units": 0,
        "supply_last_update_minutes": 8580, "supply_accounted_living_total": 2,
        "supply_member_minute_remainder": 0, "footprint": [[177, 13, 0]], "members": [],
        "spawn_tiles": [], "next_outing_generation": 2, "applied_return_generation": 0,
        "applied_report_generation": 0, "applied_cargo_generation": 0,
        "last_cargo_application_key": "", "current_scout_report": {}, "camp_decision": {},
        "acted_reports": [], "returned_cargo_stock": {"supply_units": 0, "trade_value": 0},
        "active_outing": outing, "active_hostile_operation": {}, "remembered_target_or_mark": "",
        "remembered_threat_estimate": 0, "remembered_bounty_estimate": 0,
        "remembered_retreat_bias": 0, "remembered_return_clock": -1,
        "remembered_pressure": "none", "known_recent_marks": [],
        "intelligence_map": {"leads": [{"lead_id": "field-lead", "revision": 1,
        "omt": [174, 13, 0]}]},
    }
    return {"schema_version": 1, "run_id": "fresh-native-capture",
            "source": {"runtime_source_sha256": "source", "executable_sha256": "executable"},
            "transition": {"domain": "bandit_live_world", "transition": "local_pair_handoff",
            "outcome": "committed", "site_id": SITE_ID, "operation_id": OPERATION_ID,
            "actor_ids": [4, 5], "generation": 1, "simulation_owner": "local",
            "previous_state": "abstract", "new_state": "local", "handoff_epoch": 1,
            "game_minutes": 8580, "omt": "(175,13,0)", "owner_transition": "abstract_to_local"},
            "site_payload": site}


class R008LocalPairTransactionTest(unittest.TestCase):
    def invoke(self, world: dict, report: dict | None = None):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            report_path = root / "native.report.json"
            report_path.write_text(json.dumps(report or native_report()), encoding="utf-8")
            world_dir = root / "world"
            world_dir.mkdir()
            (world_dir / "dimension_data.gsav").write_text(
                "# version 39\n" + json.dumps(world), encoding="utf-8"
            )
            sha = hashlib.sha256(report_path.read_bytes()).hexdigest()
            base = run_base_fixture_transaction(
                world_dir, source_report=report_path, source_report_sha256=sha,
                source_binding={"runtime_source_sha256": "source", "executable_sha256": "executable"},
                setup_authority="r008-closure-235-base-fixture",
            )
            receipt = run_local_pair_transaction(
                world_dir, source_report=report_path, source_report_sha256=sha,
                source_binding={"runtime_source_sha256": "source", "executable_sha256": "executable"},
                setup_authority="r008-closure-235-local-pair",
            )
            audit = audit_saved_bandit_live_world_state(
                world_dir, required_site_id_contains=SITE_ID,
                required_active_outing_generation=1,
                required_active_outing_simulation_owner="local",
                required_active_outing_handoff_epoch=1,
                required_active_outing_exact_pair=True,
                required_active_outing_pair_contract=True,
                required_local_handoff_state="local",
                required_local_handoff_exact_pair=True,
                required_local_handoff_pair_contract=True,
                required_local_handoff_route_position=[175, 13, 0],
                required_local_return_eligibility_valid=True,
                required_local_return_eligibility_cohesion_assembled=False,
            )
            return base, receipt, audit

    def test_prepares_exact_native_state_with_zero_credit(self):
        with self.assertRaisesRegex(LocalPairTransactionError, "canonical persisted base/local"):
            self.invoke(clean_world())

    def test_missing_canonical_payload_does_not_write_fixture(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            report_path = root / "native.report.json"
            report_path.write_text(json.dumps(native_report()), encoding="utf-8")
            world_dir = root / "world"
            world_dir.mkdir()
            dimension_path = world_dir / "dimension_data.gsav"
            original = b"# version 39\n{\"overmapbuffer\":{}}"
            dimension_path.write_bytes(original)
            with self.assertRaisesRegex(LocalPairTransactionError, "canonical persisted base/local"):
                run_base_fixture_transaction(
                    world_dir, source_report=report_path,
                    source_report_sha256=hashlib.sha256(report_path.read_bytes()).hexdigest(),
                    source_binding={"runtime_source_sha256": "source", "executable_sha256": "executable"},
                    setup_authority="r008-closure-235-base-fixture",
                )
            self.assertEqual(dimension_path.read_bytes(), original)

    def test_local_adapter_also_refuses_before_mutation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            report_path = root / "native.report.json"
            report_path.write_text(json.dumps(native_report()), encoding="utf-8")
            world_dir = root / "world"
            world_dir.mkdir()
            dimension_path = world_dir / "dimension_data.gsav"
            original = b"# version 39\n{\"overmapbuffer\":{}}"
            dimension_path.write_bytes(original)
            with self.assertRaisesRegex(LocalPairTransactionError, "canonical persisted base/local"):
                run_local_pair_transaction(
                    world_dir, source_report=report_path,
                    source_report_sha256=hashlib.sha256(report_path.read_bytes()).hexdigest(),
                    source_binding={"runtime_source_sha256": "source", "executable_sha256": "executable"},
                    setup_authority="r008-closure-235-local-pair",
                )
            self.assertEqual(dimension_path.read_bytes(), original)

    def test_native_snapshot_is_independently_validated_then_installed_exactly(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            report_path = root / "native.report.json"
            report_path.write_text(json.dumps(native_report()), encoding="utf-8")
            snapshot_path = root / "native.snapshot.json"
            snapshot_path.write_text(json.dumps(canonical_snapshot()), encoding="utf-8")
            source_sha = hashlib.sha256(report_path.read_bytes()).hexdigest()
            snapshot_sha = hashlib.sha256(snapshot_path.read_bytes()).hexdigest()
            source = {
                "runtime_source_sha256": "source", "executable_sha256": "executable",
            }
            validated = verify_canonical_local_pair_snapshot(
                snapshot_path, snapshot_sha,
                source_report={
                    "local_pair_handoff": native_report()["events"][1],
                }, source_binding=source,
            )
            self.assertEqual(validated["site_payload"]["active_outing"]["handoff_epoch"], 1)
            world_dir = root / "world"
            world_dir.mkdir()
            (world_dir / "dimension_data.gsav").write_text(
                "# version 39\n" + json.dumps(clean_world()), encoding="utf-8"
            )
            base = run_base_fixture_transaction(
                world_dir, source_report=report_path, source_report_sha256=source_sha,
                source_binding=source, setup_authority="fresh-zero-credit-base",
                canonical_snapshot=snapshot_path, canonical_snapshot_sha256=snapshot_sha,
            )
            local = run_local_pair_transaction(
                world_dir, source_report=report_path, source_report_sha256=source_sha,
                source_binding=source, setup_authority="fresh-zero-credit-local",
                canonical_snapshot=snapshot_path, canonical_snapshot_sha256=snapshot_sha,
            )
            self.assertFalse(base["gameplay_credit"])
            self.assertFalse(local["gameplay_credit"])

    def test_snapshot_controls_reject_partial_stale_mixed_and_mutated_payloads(self):
        source = {"runtime_source_sha256": "source", "executable_sha256": "executable"}
        cases = {
            "partial": lambda snapshot: snapshot["site_payload"]["active_outing"].pop("shared_route"),
            "stale": lambda snapshot: snapshot["source"].update({"executable_sha256": "old"}),
            "mixed": lambda snapshot: snapshot["transition"].update({"actor_ids": [4, 6]}),
            "mutated": lambda snapshot: snapshot["site_payload"]["active_outing"].update({"handoff_epoch": 2}),
        }
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name, mutate in cases.items():
                with self.subTest(name=name):
                    snapshot = canonical_snapshot()
                    mutate(snapshot)
                    path = root / f"{name}.json"
                    path.write_text(json.dumps(snapshot), encoding="utf-8")
                    with self.assertRaises(LocalPairTransactionError):
                        verify_canonical_local_pair_snapshot(
                            path, hashlib.sha256(path.read_bytes()).hexdigest(),
                            source_report={"local_pair_handoff": native_report()["events"][1]},
                            source_binding=source,
                        )

    def test_rejects_bad_source_pair_and_dirty_or_duplicate_fixture(self):
        controls = []
        bad_handoff = native_report()
        bad_handoff["events"][1]["actor_ids"] = [4, 6]
        controls.append((clean_world(), bad_handoff))
        duplicate = clean_world()
        duplicate["overmapbuffer"]["bandit_live_world"] = {"sites": [{"site_id": SITE_ID}, {"site_id": SITE_ID}]}
        controls.append((duplicate, native_report()))
        for world, report in controls:
            with self.subTest(world=world, report=report):
                with self.assertRaises(LocalPairTransactionError):
                    self.invoke(copy.deepcopy(world), copy.deepcopy(report))


if __name__ == "__main__":
    unittest.main()
