#!/usr/bin/env python3
"""Focused contracts for scenario_registry's declaration-only behavior."""

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

from scenario_registry import (  # noqa: E402
    ManifestValidationError,
    normalize_relation_contract,
    relation_contract_likely_subsumes,
    validate_manifest,
)


class ScenarioRegistryContractTest(unittest.TestCase):
    def write_manifest(self, root: Path, name: str, manifest: dict) -> Path:
        path = root / name
        path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        return path

    def versioned_manifest(self) -> dict:
        return {
            "manifest_version": 1,
            "name": "neutral.scenario",
            "description": "A declaration with no inferred gameplay facts.",
            "steps": [
                {"label": "setup", "kind": "wait"},
                {"label": "production", "kind": "press"},
                {"label": "terminal", "kind": "audit_log_contains"},
                {"label": "artifact", "kind": "audit_log_contains"},
                {"label": "no_shortcut", "kind": "audit_log_not_contains"},
            ],
            "capabilities": {
                "player.injured": False,
                "player.movement": "walk",
                "local_place.coordinates": {"x": 12, "y": 4, "z": 0},
                "local_place.dialogue": ["Pay", "Fight"],
                "actors.visible": ["bandit", "scout"],
                "actors.combat": False,
                "actors.fight": "available",
                "world.weather": "clear",
                "world.travel": "local",
                "capabilities.pay": True,
                "capabilities.trade": True,
                "capabilities.terminal": "pending",
                "capabilities.persistence": False,
                "runtime.ocr": None,
                "runtime.replay": "supported",
            },
            "runtime_contract": {
                "permitted_input": ["press:f"],
                "forbidden_input": ["debug:spawn"],
                "setup_only_debug": True,
                "disposable_copy": True,
                "helpers": ["Peekaboo"],
                "permissions": ["accessibility", "screen-recording"],
                "platform": ["macos"],
                "profile": "dev-harness",
                "fixture": "mcwilliams",
                "requirements": {
                    "os": "macos",
                    "source": "current-worktree",
                    "executable": "cataclysm-tiles",
                    "profile": "dev-harness",
                    "fixture": "mcwilliams",
                    "helper": "Peekaboo",
                    "peekaboo": True,
                    "input": ["press:f"],
                    "ocr": True,
                    "cleanup": True,
                },
                "grants_gameplay_proof": False,
            },
            "proof_route": {
                "precondition": ["setup"],
                "production_behavior": ["production"],
                "terminal_persistence": ["terminal"],
                "artifact_verdict": ["artifact"],
                "disallowed_shortcuts": ["no_shortcut"],
            },
        }

    def checkpoint_manifest(self) -> dict:
        declaration = self.versioned_manifest()
        declaration["manifest_version"] = 2
        declaration["run_class"] = "non_combat"
        declaration["observer_character"] = True
        declaration["installed_save_player"] = "#player.sav.zzip"
        declaration["proof_gates"] = [
            {
                "id": "handoff",
                "label": "Committed handoff",
                "boundary_step": "production",
                "predecessors": [],
                "expectations": [{
                    "kind": "structured_event",
                    "predicate": {"transition": "handoff", "committed": True},
                }],
                "checkpoint_safe_ui": {"screen_text_contains": ["Move:"]},
            },
            {
                "id": "saved",
                "label": "Saved result",
                "boundary_step": "terminal",
                "predecessors": ["handoff"],
                "expectations": [{
                    "kind": "saved_artifact",
                    "predicate": {"audit": "saved_state", "committed": True},
                }],
                "checkpoint_safe_ui": {"screen_text_contains": ["Move:"]},
            },
        ]
        declaration["proof_route"] = {
            "gates": ["handoff", "saved"],
            "terminal": ["saved"],
            "capability_gates": {
                "capabilities.pay": {"terminal": ["saved"]},
            },
        }
        return declaration

    def test_versioned_declaration_round_trips_typed_values_and_source_binding(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            declaration = self.versioned_manifest()
            path = self.write_manifest(root, "neutral.json", declaration)

            result = validate_manifest(declaration, path=path)

            self.assertEqual(result["declaration"], declaration)
            self.assertIsNot(result["declaration"], declaration)
            self.assertEqual(result["validation"]["status"], "valid")
            self.assertFalse(result["validation"]["review_required"])
            self.assertEqual(result["normalized"]["capabilities"]["value"], declaration["capabilities"])
            self.assertEqual(result["normalized"]["runtime_contract"]["value"], declaration["runtime_contract"])
            self.assertEqual(result["normalized"]["proof_route"]["value"], declaration["proof_route"])
            self.assertEqual(result["source"]["path"], str(path.resolve()))
            self.assertEqual(result["source"]["sha256"], hashlib.sha256(path.read_bytes()).hexdigest())
            self.assertEqual(
                set(result["normalized"]["capabilities"]["value"]),
                set(declaration["capabilities"]),
            )

    def test_checkpoint_contract_requires_ordered_causal_gates_without_reinterpreting_v1(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            declaration = self.checkpoint_manifest()
            path = self.write_manifest(root, "checkpoint.json", declaration)
            result = validate_manifest(declaration, path=path)
            self.assertEqual(result["validation"]["status"], "valid")
            self.assertEqual(result["validation"]["manifest_version"], 2)
            for field in (
                    "run_class", "observer_character", "installed_save_player", "proof_gates"):
                self.assertEqual(result["normalized"][field]["state"], "declared")
                self.assertEqual(result["normalized"][field]["value"], declaration[field])
            relation = normalize_relation_contract(declaration)
            self.assertIsNotNone(relation)
            self.assertEqual(relation["checkpoint_contract"], {
                "manifest_version": 2,
                "run_class": declaration["run_class"],
                "observer_character": declaration["observer_character"],
                "installed_save_player": declaration["installed_save_player"],
                "capabilities": declaration["capabilities"],
                "runtime_contract": declaration["runtime_contract"],
                "proof_gates": declaration["proof_gates"],
                "proof_route": declaration["proof_route"],
            })

            invisible = copy.deepcopy(declaration)
            invisible["observer_safety_mode"] = "invisible"
            invisible_path = self.write_manifest(root, "invisible-checkpoint.json", invisible)
            invisible_result = validate_manifest(invisible, path=invisible_path)
            self.assertEqual(
                invisible_result["normalized"]["observer_safety_mode"]["value"], "invisible"
            )
            self.assertEqual(
                normalize_relation_contract(invisible)["checkpoint_contract"]["observer_safety_mode"],
                "invisible",
            )

            invalid_cases = []

            duplicate_id = copy.deepcopy(declaration)
            duplicate_id["proof_gates"][1]["id"] = "handoff"
            invalid_cases.append(duplicate_id)

            out_of_order_predecessor = copy.deepcopy(declaration)
            out_of_order_predecessor["proof_gates"][1]["predecessors"] = ["saved"]
            invalid_cases.append(out_of_order_predecessor)

            route_gap = copy.deepcopy(declaration)
            route_gap["proof_route"]["gates"] = ["saved"]
            invalid_cases.append(route_gap)

            transport_only = copy.deepcopy(declaration)
            transport_only["proof_gates"][0]["expectations"] = [{
                "kind": "input_delivery", "predicate": {"step": "production"},
            }]
            invalid_cases.append(transport_only)

            missing_safe_ui = copy.deepcopy(declaration)
            missing_safe_ui["proof_gates"][0]["checkpoint_safe_ui"] = {}
            invalid_cases.append(missing_safe_ui)

            missing_terminal_observability = copy.deepcopy(declaration)
            missing_terminal_observability["proof_route"]["terminal"] = ["handoff"]
            invalid_cases.append(missing_terminal_observability)

            for index, invalid in enumerate(invalid_cases):
                with self.subTest(index=index):
                    invalid_path = self.write_manifest(root, f"checkpoint-invalid-{index}.json", invalid)
                    with self.assertRaises(ManifestValidationError):
                        validate_manifest(invalid, path=invalid_path)

    def test_legacy_text_never_becomes_camp_fight_visibility_or_injury_facts(self) -> None:
        legacy = {
            "name": "bandit.camp_fight_visibility_injury",
            "description": "Camp Fight proves a visible injured actor.",
            "steps": [{"label": "ordinary_step", "kind": "wait"}],
        }
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_manifest(Path(temp_dir), "bandit.camp_fight_visibility_injury.json", legacy)
            result = validate_manifest(legacy, path=path)

            self.assertEqual(result["validation"]["status"], "review_required")
            for field in ("manifest_version", "capabilities", "runtime_contract", "proof_route"):
                self.assertEqual(result["normalized"][field]["state"], "unknown")
                self.assertTrue(result["normalized"][field]["review_required"])
                self.assertIsNone(result["normalized"][field]["value"])
            self.assertNotIn("camp", json.dumps(result["normalized"], sort_keys=True).lower())
            self.assertEqual(result["declaration"], legacy)

    def test_versioned_invalid_fields_fail_without_coercing_unknown_or_route_labels(self) -> None:
        cases = []
        invalid_namespace = self.versioned_manifest()
        invalid_namespace["capabilities"] = {"description.camp": True}
        cases.append(invalid_namespace)

        nested_object = self.versioned_manifest()
        nested_object["capabilities"] = {"actors.state": {"detail": {"visible": True}}}
        cases.append(nested_object)

        unknown_route = self.versioned_manifest()
        unknown_route["proof_route"]["production_behavior"] = ["Fight from description"]
        cases.append(unknown_route)

        duplicate_labels = self.versioned_manifest()
        duplicate_labels["steps"][1]["label"] = "setup"
        cases.append(duplicate_labels)

        gameplay_proof = self.versioned_manifest()
        gameplay_proof["runtime_contract"]["grants_gameplay_proof"] = True
        cases.append(gameplay_proof)

        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            for index, declaration in enumerate(cases):
                with self.subTest(index=index):
                    path = self.write_manifest(root, f"invalid-{index}.json", declaration)
                    with self.assertRaises(ManifestValidationError):
                        validate_manifest(declaration, path=path)

    def test_unversioned_declared_field_round_trips_but_requires_review(self) -> None:
        declaration = {
            "name": "legacy.partial",
            "steps": [{"label": "ordinary_step", "kind": "wait"}],
            "capabilities": {"player.injured": "unknown"},
        }
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_manifest(Path(temp_dir), "legacy.partial.json", declaration)
            result = validate_manifest(copy.deepcopy(declaration), path=path)

            capability = result["normalized"]["capabilities"]
            self.assertEqual(capability["state"], "declared_unversioned")
            self.assertTrue(capability["review_required"])
            self.assertEqual(capability["value"], {"player.injured": "unknown"})
            self.assertTrue(result["validation"]["review_required"])

    def test_source_binding_rejects_a_declaration_not_read_from_that_path(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            path = self.write_manifest(root, "bound.json", self.versioned_manifest())
            substituted = self.versioned_manifest()
            substituted["description"] = "not the source declaration"

            with self.assertRaises(ManifestValidationError):
                validate_manifest(substituted, path=path)

    def test_relation_contract_ignores_prose_but_retains_input_steps_and_route(self) -> None:
        first = self.versioned_manifest()
        second = self.versioned_manifest()
        second["name"] = "different.identity"
        second["description"] = "Different prose must not create a distinct relation contract."
        second["recommendation"] = "Run a different manual command."
        second["artifact_patterns"] = ["prose narration only"]
        second["steps"][1]["comment"] = "This comment is not a production requirement."
        self.assertEqual(normalize_relation_contract(first), normalize_relation_contract(second))

        changed_input = copy.deepcopy(second)
        changed_input["runtime_contract"]["permitted_input"] = ["press:p"]
        self.assertNotEqual(normalize_relation_contract(first), normalize_relation_contract(changed_input))

        changed_step = copy.deepcopy(second)
        changed_step["steps"][1]["key"] = "p"
        self.assertNotEqual(normalize_relation_contract(first), normalize_relation_contract(changed_step))

        changed_route = copy.deepcopy(second)
        changed_route["proof_route"]["terminal_persistence"] = ["artifact"]
        self.assertNotEqual(normalize_relation_contract(first), normalize_relation_contract(changed_route))

    def test_relation_contract_likely_subsumption_is_directional_and_rejects_narrower_requirements(self) -> None:
        subject = self.versioned_manifest()
        successor = self.versioned_manifest()
        successor["steps"].insert(2, {"label": "production_extra", "kind": "press", "key": "p"})
        successor["proof_route"]["production_behavior"] = ["production", "production_extra"]
        subject_contract = normalize_relation_contract(subject)
        successor_contract = normalize_relation_contract(successor)
        self.assertIsNotNone(subject_contract)
        self.assertIsNotNone(successor_contract)
        self.assertTrue(relation_contract_likely_subsumes(subject_contract, successor_contract))
        self.assertFalse(relation_contract_likely_subsumes(successor_contract, subject_contract))

        narrower = copy.deepcopy(successor)
        narrower["runtime_contract"]["requirements"]["extra_gate"] = True
        narrower_contract = normalize_relation_contract(narrower)
        self.assertIsNotNone(narrower_contract)
        self.assertFalse(relation_contract_likely_subsumes(subject_contract, narrower_contract))


if __name__ == "__main__":
    unittest.main()
