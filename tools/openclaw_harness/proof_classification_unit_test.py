#!/usr/bin/env python3
"""Unit tests for OpenClaw harness proof classification.

These tests freeze the report-writer seam that prevents load-only, stale-startup,
wait-ledger, or non-green step-ledger runs from being reported as feature proof
just because a log/artifact pattern matched.
"""

from __future__ import annotations

import sys
import unittest
from pathlib import Path
from typing import Any, Dict, List

HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

from startup_harness import probe_proof_classification, summarize_probe_step_ledger  # noqa: E402


def startup(*, clean: bool = True, status: str = "green") -> Dict[str, Any]:
    return {
        "evidence_class": "startup/load",
        "status": status,
        "verdict": "startup_load_only",
        "feature_proof": False,
        "startup_clean_for_feature_steps": clean,
        "feature_gate": "clean_startup_ready_for_feature_steps" if clean else "startup_not_clean",
    }


def matches() -> List[Dict[str, Any]]:
    return [{"pattern": "claim scoped line", "lines": ["claim scoped line"]}]


class ProbeProofClassificationTest(unittest.TestCase):
    def test_load_only_run_never_becomes_feature_proof(self) -> None:
        result = probe_proof_classification(
            verdict="artifacts_matched",
            startup_classification=startup(clean=True),
            step_reports=[],
            artifact_patterns=["claim scoped line"],
            matches_by_pattern=matches(),
        )

        self.assertEqual(result["status"], "yellow")
        self.assertEqual(result["verdict"], "startup_load_only_no_feature_steps")
        self.assertEqual(result["evidence_class"], "startup/load-or-inconclusive")
        self.assertFalse(result["feature_proof"])

    def test_artifact_match_is_inconclusive_when_startup_gate_is_not_clean(self) -> None:
        result = probe_proof_classification(
            verdict="artifacts_matched",
            startup_classification=startup(clean=False, status="yellow"),
            step_reports=[{"label": "feature_step"}],
            artifact_patterns=["claim scoped line"],
            matches_by_pattern=matches(),
            step_ledger_summary={"status": "green_step_local_proof"},
        )

        self.assertEqual(result["status"], "yellow")
        self.assertEqual(result["verdict"], "startup_gate_not_clean_artifact_match_inconclusive")
        self.assertFalse(result["feature_proof"])

    def test_non_green_step_ledger_overrides_artifact_match(self) -> None:
        yellow_ledger = summarize_probe_step_ledger([
            {"primitive_step": "unguarded_press", "verdict": "yellow_step_expected_fact_missing"}
        ])

        result = probe_proof_classification(
            verdict="artifacts_matched",
            startup_classification=startup(clean=True),
            step_reports=[{"label": "unguarded_press"}],
            artifact_patterns=["claim scoped line"],
            matches_by_pattern=matches(),
            step_ledger_summary=yellow_ledger,
        )

        self.assertEqual(result["status"], "yellow")
        self.assertEqual(result["verdict"], "yellow_step_local_proof_incomplete")
        self.assertFalse(result["feature_proof"])

    def test_wait_ledger_blocker_overrides_artifact_match(self) -> None:
        result = probe_proof_classification(
            verdict="artifacts_matched",
            startup_classification=startup(clean=True),
            step_reports=[{"label": "long_wait"}],
            artifact_patterns=["claim scoped line"],
            matches_by_pattern=matches(),
            wait_step_summary={"status": "blocked_wait_step"},
            step_ledger_summary={"status": "green_step_local_proof"},
        )

        self.assertEqual(result["status"], "red")
        self.assertFalse(result["feature_proof"])

    def test_yellow_wait_ledger_keeps_artifact_match_non_feature_proof(self) -> None:
        result = probe_proof_classification(
            verdict="artifacts_matched",
            startup_classification=startup(clean=True),
            step_reports=[{"label": "long_wait"}],
            artifact_patterns=["claim scoped line"],
            matches_by_pattern=matches(),
            wait_step_summary={"status": "yellow_wait_step_unverified"},
            step_ledger_summary={"status": "green_step_local_proof"},
        )

        self.assertEqual(result["status"], "yellow")
        self.assertFalse(result["feature_proof"])

    def test_feature_proof_requires_clean_startup_green_steps_and_matched_artifact(self) -> None:
        green_ledger = summarize_probe_step_ledger([
            {"primitive_step": "guarded_step", "verdict": "green_step_expected_fact_present"}
        ])

        result = probe_proof_classification(
            verdict="artifacts_matched",
            startup_classification=startup(clean=True),
            step_reports=[{"label": "guarded_step"}],
            artifact_patterns=["claim scoped line"],
            matches_by_pattern=matches(),
            step_ledger_summary=green_ledger,
        )

        self.assertEqual(result["status"], "green")
        self.assertEqual(result["evidence_class"], "feature-path")
        self.assertTrue(result["feature_proof"])


if __name__ == "__main__":
    unittest.main()
