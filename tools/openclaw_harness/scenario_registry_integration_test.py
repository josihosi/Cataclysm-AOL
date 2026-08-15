#!/usr/bin/env python3
"""Focused integration contracts for registry-backed scenario loading."""

from __future__ import annotations

import hashlib
import io
import json
import sys
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path
from unittest import mock


HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

from startup_harness import (  # noqa: E402
    finalize_scenario_report,
    list_scenarios,
    load_scenario,
    scenario_contract_dict,
    scenario_manifest_binding,
)


class ScenarioRegistryIntegrationTest(unittest.TestCase):
    def write_manifest(self, root: Path, name: str, payload: dict) -> Path:
        path = root / f"{name}.json"
        path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
        return path

    def test_legacy_load_and_list_preserve_flat_fields_with_review_binding(self) -> None:
        legacy = {
            "name": "legacy.camp_fight",
            "description": "A camp Fight remains unreviewed declaration text.",
            "profile": "dev-harness",
            "fixture": "fixture-a",
            "steps": [{"label": "ordinary_step", "kind": "wait"}],
        }
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            path = self.write_manifest(root, "legacy.camp_fight", legacy)
            with mock.patch("startup_harness.scenarios_root", return_value=root):
                loaded = load_scenario("legacy.camp_fight")
                listed = list_scenarios()

            self.assertEqual(loaded["profile"], "dev-harness")
            self.assertEqual(loaded["fixture"], "fixture-a")
            self.assertEqual(loaded["steps"], legacy["steps"])
            self.assertEqual(loaded["path"], str(path))
            self.assertEqual(loaded["_scenario_registry"]["validation"]["status"], "review_required")
            self.assertEqual(
                scenario_manifest_binding(loaded)["normalized"]["capabilities"],
                {"state": "unknown", "review_required": True, "value": None},
            )
            self.assertEqual(len(listed), 1)
            binding = listed[0]["scenario_manifest"]
            self.assertEqual(binding["validation"]["status"], "review_required")
            self.assertEqual(binding["source"]["path"], str(path.resolve()))
            self.assertEqual(binding["source"]["sha256"], hashlib.sha256(path.read_bytes()).hexdigest())

    def test_invalid_versioned_manifest_fails_load_and_is_explicit_in_listing(self) -> None:
        invalid = {
            "manifest_version": 1,
            "name": "invalid.versioned",
            "steps": [{"label": "one", "kind": "wait"}],
        }
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            self.write_manifest(root, "invalid.versioned", invalid)
            with mock.patch("startup_harness.scenarios_root", return_value=root):
                with self.assertRaises(SystemExit) as raised:
                    load_scenario("invalid.versioned")
                listed = list_scenarios()

            self.assertIn("capabilities is required", str(raised.exception))
            self.assertEqual(len(listed), 1)
            validation = listed[0]["scenario_manifest"]["validation"]
            self.assertEqual(validation["status"], "invalid")
            self.assertIn("capabilities is required", validation["error"])

    def test_run_owned_report_carries_exact_manifest_binding(self) -> None:
        legacy = {
            "name": "legacy.binding",
            "profile": "dev-harness",
            "steps": [{"label": "ordinary_step", "kind": "wait"}],
        }
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            self.write_manifest(root, "legacy.binding", legacy)
            with mock.patch("startup_harness.scenarios_root", return_value=root):
                scenario = load_scenario("legacy.binding")
            binding = scenario_manifest_binding(scenario)
            contract = scenario_contract_dict(
                profile="dev-harness",
                config_profile="dev-harness",
                world="",
                profile_snapshot="",
                profile_snapshot_profile="",
                profile_option_overrides={},
                fixture="",
                fixture_profile="",
                replace_existing_worlds=False,
                advance_count=0,
                settle_seconds=0.0,
                artifact_source="debug.log",
                artifact_patterns=[],
                recommended_test_command="",
                steps=scenario["steps"],
                capture_world_after=False,
                portal_storm_policy={},
                scenario_manifest=binding,
            )
            run_dir = root / "run"
            with redirect_stdout(io.StringIO()):
                finalize_scenario_report(
                    run_dir,
                    {"scenario": scenario["name"], "contract": contract},
                    scenario=scenario,
                )

            report = json.loads((run_dir / "probe.report.json").read_text(encoding="utf-8"))
            self.assertEqual(report["scenario_manifest"], binding)
            self.assertEqual(report["contract"]["scenario_manifest"], binding)
            self.assertEqual(
                (root / "legacy.binding.json").read_text(encoding="utf-8"),
                json.dumps(legacy, indent=2) + "\n",
            )


if __name__ == "__main__":
    unittest.main()
