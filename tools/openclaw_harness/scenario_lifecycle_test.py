#!/usr/bin/env python3
"""Focused R-011 lifecycle and setup-only firewall contracts."""

from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path


HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

from scenario_registry_store import (  # noqa: E402
    BindingAdapters,
    ScenarioRegistryStoreError,
    create_source_bound_scenario,
    execute_registry_query,
    final_gate_eligibility,
    ingest_report_reference,
    open_registry,
    parse_registry_query_request,
    validate_source_bound_scenario,
)
import startup_harness  # noqa: E402


def _manifest(name: str, capability_key: str) -> dict[str, object]:
    label = "setup_only"
    return {
        "manifest_version": 1,
        "name": name,
        "profile": "dev-harness",
        "fixture": "r013_clean_wait_duration_v1",
        "fixture_profile": "live-debug",
        "capabilities": {capability_key: True},
        "runtime_contract": {
            "permitted_input": ["semantic:observe"],
            "forbidden_input": ["debug:inject_report"],
            "setup_only_debug": True,
            "disposable_copy": True,
            "helpers": ["Peekaboo"],
            "permissions": ["accessibility"],
            "platform": ["macos"],
            "profile": "dev-harness",
            "fixture": "r013_clean_wait_duration_v1",
            "requirements": {
                "os": "macos", "source": "current-worktree", "executable": "cataclysm-tiles",
                "profile": "dev-harness", "fixture": "r013_clean_wait_duration_v1",
                "helper": "Peekaboo", "peekaboo": True, "ocr": True,
                "input": ["semantic:observe"], "cleanup": True,
            },
            "grants_gameplay_proof": False,
        },
        "steps": [{"kind": "observe", "label": label}],
        "proof_route": {
            "precondition": [label], "production_behavior": [label],
            "terminal_persistence": [label], "artifact_verdict": [label],
            "disallowed_shortcuts": [label],
        },
    }


class ScenarioLifecycleTest(unittest.TestCase):
    def test_no_fit_create_validate_and_selection_reason_are_append_only(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "scenarios"
            registry = Path(directory) / "registry.sqlite3"
            connection = open_registry(str(registry))
            try:
                existing = create_source_bound_scenario(
                    connection, scenarios_root=root, name="existing",
                    declaration=_manifest("existing", "world.r011.existing"),
                )
                selected = execute_registry_query(connection, parse_registry_query_request({
                    "requirements": [{"key": "world.r011.existing", "op": "eq", "value": True,
                                      "minimum_evidence": "declared"}],
                    "preferences": [],
                }), drafts_root=Path(directory) / "drafts")
                self.assertEqual(selected.evaluation.evaluation.ranked_scenario_ids, (existing["manifest_id"],))
                self.assertIsNotNone(selected.selection_id)
                reason = connection.execute(
                    "SELECT fit_reason FROM scenario_selection_history WHERE selection_id = ?",
                    (selected.selection_id,),
                ).fetchone()
                self.assertIn("world.r011.existing", str(reason["fit_reason"]))

                no_fit = execute_registry_query(connection, parse_registry_query_request({
                    "requirements": [{"key": "actors.zombie_dog", "op": "eq", "value": "required",
                                      "minimum_evidence": "declared"}],
                    "preferences": [],
                }), drafts_root=Path(directory) / "drafts")
                self.assertIsNone(no_fit.token_id)
                self.assertIsNotNone(no_fit.draft_path)
                created = create_source_bound_scenario(
                    connection, scenarios_root=root, name="dog",
                    declaration=_manifest("dog", "actors.zombie_dog"),
                )
                self.assertFalse(created["idempotent"])
                same = create_source_bound_scenario(
                    connection, scenarios_root=root, name="dog",
                    declaration=_manifest("dog", "actors.zombie_dog"),
                )
                self.assertTrue(same["idempotent"])
                changed = _manifest("dog", "actors.zombie_dog")
                changed["capabilities"] = {"actors.zombie_dog": "different"}
                with self.assertRaises(ScenarioRegistryStoreError):
                    create_source_bound_scenario(connection, scenarios_root=root, name="dog", declaration=changed)
                validation = validate_source_bound_scenario(connection, scenario_name="dog", scenarios_root=root)
                self.assertEqual(validation["status"], "valid")
            finally:
                connection.close()

    def test_deterministic_free_tile_and_setup_report_cannot_credit_final_gates(self) -> None:
        audit = {
            "player_location_ms": [0, 0, 0],
            "active_monsters": [{"location_ms": [1, 0, 0]}],
        }
        selected = startup_harness.deterministic_free_monster_tile(audit, [[1, 0, 0], [2, 0, 0]])
        self.assertEqual(selected["location_ms"], [2, 0, 0])
        self.assertIsNone(startup_harness.deterministic_free_monster_tile(audit, [[0, 0, 0], [1, 0, 0]])["location_ms"])

        with tempfile.TemporaryDirectory() as directory:
            registry = Path(directory) / "registry.sqlite3"
            report = Path(directory) / "setup.report.json"
            report.write_text(json.dumps({
                "scenario": "setup", "scenario_manifest": {"source": {
                    "path": str(report), "sha256": "a" * 64,
                }},
                "scenario_interventions": [{
                    "evidence_effect": "none_for_manufactured_state", "gameplay_credit": False,
                }],
            }), encoding="utf-8")
            connection = open_registry(str(registry))
            try:
                result = ingest_report_reference(
                    connection, report,
                    adapters=BindingAdapters(runtime=lambda _expected: {}, fixture=lambda _expected: {}, profile=lambda _expected: {}),
                )
                self.assertEqual(result["classification"], "setup-only")
                self.assertEqual(final_gate_eligibility(connection), {
                    "automated_certification": False,
                    "windows_feel": False,
                    "authoritative_verification_ids": [],
                    "overall_acceptance": False,
                })
            finally:
                connection.close()


if __name__ == "__main__":
    unittest.main()
