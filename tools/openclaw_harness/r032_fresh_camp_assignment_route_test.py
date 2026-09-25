#!/usr/bin/env python3
"""Focused authority boundary for fresh R-032 camp assignment proof."""

from __future__ import annotations

import sys
import unittest
import json
from pathlib import Path

HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

from scenario_registry import validate_manifest  # noqa: E402
from startup_harness import load_scenario, r014_native_semantic_bootstrap_metadata  # noqa: E402


class R032FreshCampAssignmentRouteTest(unittest.TestCase):
    def setUp(self) -> None:
        self.scenario = load_scenario("r032.fresh_camp_establishment_v001_mcw")

    def test_only_native_assignment_and_reloaded_continuation_are_creditable(self) -> None:
        self.assertTrue(self.scenario["runtime_contract"]["grants_gameplay_proof"])
        self.assertEqual(
            self.scenario["capabilities"]["capabilities.r032.native_assignment_continuity"],
            "native_assignment_at_camp_omt_then_save_quit_reload",
        )
        self.assertEqual(self.scenario["steps"][0]["kind"], "native_semantic_bootstrap")
        self.assertIn("establish_r032_camp", self.scenario["proof_route"]["production_behavior"])
        self.assertEqual(
            self.scenario["proof_route"]["terminal_persistence"],
            [
                "establish_r032_camp",
                "bind_r032_reloaded_camp_world",
                "verify_r032_assigned_camp_continuity",
            ],
        )

    def test_fixture_and_bootstrap_cannot_gain_credit_from_route_promotion(self) -> None:
        metadata = r014_native_semantic_bootstrap_metadata(
            profile="test",
            run_dir=Path("/nonexistent-r032-run"),
            run_id="r032-test",
            start_offset=0,
            press_trace_offset=0,
            required_state="world",
            required_actions=["world.chat"],
            frame={
                "frame_id": "bootstrap-frame",
                "run_id": "r032-test",
                "event": "surface_descriptor",
                "kind": "world",
                "valid_actions": [{"id": "world.chat", "enabled": True}],
                "_event_offset": 0,
            },
        )
        self.assertFalse(metadata["gameplay_credit"])
        self.assertEqual(metadata["artifact_kind"], "r014_native_semantic_bootstrap_frame")
        self.assertTrue(any("setup only" in target.lower() for target in self.scenario["steps"][1]["proof_targets"]))

    def test_promoted_manifest_remains_a_valid_declaration(self) -> None:
        source = HARNESS_DIR / "scenarios" / "r032.fresh_camp_establishment_v001_mcw.json"
        result = validate_manifest(json.loads(source.read_text(encoding="utf-8")), path=source)
        self.assertEqual(result["validation"]["status"], "valid")


if __name__ == "__main__":
    unittest.main()
