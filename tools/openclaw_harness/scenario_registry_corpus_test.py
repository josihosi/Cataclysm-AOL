#!/usr/bin/env python3
"""Corpus accounting for registry-backed harness scenario declarations."""

from __future__ import annotations

import hashlib
import json
import sys
import unittest
from pathlib import Path


HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

from startup_harness import list_scenarios, scenarios_root  # noqa: E402


class ScenarioRegistryCorpusContractTest(unittest.TestCase):
    def test_current_scenario_paths_are_bound_and_legacy_unknowns_are_explicit(self) -> None:
        paths = sorted(scenarios_root().glob("*.json"), key=lambda path: path.name.lower())
        source_before = {path.resolve(): path.read_bytes() for path in paths}

        listed = list_scenarios()

        source_after = {path.resolve(): path.read_bytes() for path in paths}
        self.assertEqual(source_after, source_before, "listing/validation must not rewrite declarations")

        expected_paths = set(source_before)
        entries_by_path = {
            Path(str(entry["scenario_manifest"]["source"]["path"])).resolve(): entry
            for entry in listed
        }
        self.assertEqual(set(entries_by_path), expected_paths)

        for path, source_bytes in source_before.items():
            with self.subTest(path=path.name):
                entry = entries_by_path[path]
                binding = entry["scenario_manifest"]
                raw = json.loads(source_bytes.decode("utf-8"))

                self.assertEqual(binding["source"]["path"], str(path))
                self.assertEqual(binding["source"]["sha256"], hashlib.sha256(source_bytes).hexdigest())

                normalized = binding["normalized"]
                validation = binding["validation"]
                if "manifest_version" in raw:
                    self.assertEqual(validation["status"], "valid")
                    self.assertFalse(validation["review_required"])
                    for field in ("manifest_version", "capabilities", "runtime_contract", "proof_route"):
                        self.assertEqual(normalized[field]["state"], "declared")
                        self.assertEqual(normalized[field]["value"], raw[field])
                    continue

                self.assertEqual(validation["status"], "review_required")
                self.assertTrue(validation["review_required"])
                for field in ("manifest_version", "capabilities", "runtime_contract", "proof_route"):
                    field_binding = normalized[field]
                    self.assertTrue(field_binding["review_required"])
                    if field in raw:
                        self.assertEqual(field_binding["state"], "declared_unversioned")
                        self.assertEqual(field_binding["value"], raw[field])
                    else:
                        self.assertEqual(field_binding["state"], "unknown")
                        self.assertIsNone(field_binding["value"])


if __name__ == "__main__":
    unittest.main()
