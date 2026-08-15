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

from scenario_registry import ManifestValidationError, validate_manifest  # noqa: E402


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


if __name__ == "__main__":
    unittest.main()
