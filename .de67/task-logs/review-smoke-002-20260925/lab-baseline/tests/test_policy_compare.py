from __future__ import annotations

import importlib.util
import json
import subprocess
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "policy_compare.py"
POLICY = ROOT / "assets" / "environment" / "phase3-policy.d67"
CONTRACTS = ROOT / "assets" / "environment" / "phase3-contracts.json"
SPEC = importlib.util.spec_from_file_location("de67_policy_compare", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
compare_module = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = compare_module
SPEC.loader.exec_module(compare_module)


class PolicyComparisonTests(unittest.TestCase):
    def test_contract_manifest_is_machine_canonical_and_named(self) -> None:
        value = json.loads(CONTRACTS.read_text(encoding="utf-8"))
        self.assertEqual(value["format"], "de67.phase3.contracts")
        names = [case["name"] for case in value["decision_cases"] + value["trace_cases"]]
        self.assertEqual(len(names), len(set(names)))

    def test_main_and_lab_comparison_has_only_declared_strengthenings(self) -> None:
        report = compare_module.compare(POLICY, CONTRACTS)
        self.assertTrue(report["passed"])
        self.assertNotIn("baseline_policy_bytes", report)
        self.assertGreater(report["compiled_policy_bytes"], 0)
        divergences = {
            case["name"] for case in report["trace_cases"]
            if case["main"] != case["lab"]
        }
        self.assertEqual(divergences, {
            "late-acceptance-before-review",
            "worker-terminal-without-receipt",
            "mutation-ignores-suggestion",
            "proof-owner-replacement-leaves-stale-projection",
        })

    def test_comparison_cli_is_reproducible(self) -> None:
        command = [
            sys.executable, str(SCRIPT), "--policy", str(POLICY),
            "--contracts", str(CONTRACTS),
        ]
        first = subprocess.run(command, text=True, capture_output=True)
        second = subprocess.run(command, text=True, capture_output=True)
        self.assertEqual(first.returncode, 0, first.stderr)
        self.assertEqual(first.stdout, second.stdout)
        self.assertTrue(json.loads(first.stdout)["passed"])

    def test_missing_policy_fails_closed(self) -> None:
        result = subprocess.run(
            [sys.executable, str(SCRIPT), "--policy", str(ROOT / "missing-policy.d67"),
             "--contracts", str(CONTRACTS)],
            text=True, capture_output=True,
        )
        self.assertEqual(result.returncode, 2)
        self.assertIn("error:", result.stderr)


if __name__ == "__main__":
    unittest.main()
