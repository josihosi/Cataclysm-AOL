#!/usr/bin/env python3
"""Focused tests for R-009 integrated-wait observations."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from r009_technical_witness import (
    complete_child_resource_interval,
    observe_integrated_wait,
    preflight_contract,
    technical_witness,
)


def event(sequence: int, game_minutes: int, transition: str = "wait") -> dict:
    return {
        "sequence": sequence, "game_minutes": game_minutes, "domain": "activity",
        "transition": transition, "outcome": "committed",
    }


class R009TechnicalWitnessTest(unittest.TestCase):
    def test_integrated_wait_uses_native_game_time_and_compacts_repetitions(self) -> None:
        before = [event(1, 100)]
        after = [*before, event(2, 110), event(3, 120)]
        observation = observe_integrated_wait(
            label="six_hour_wait", before_events=before, after_events=after,
            resources_before={"pid": 7}, resources_after={"pid": 7},
            wait_completion={"status": "completed", "verdict": "green_wait_step_proven"},
        )
        self.assertEqual(observation["product_game_time"]["status"], "advancing")
        self.assertEqual(observation["product_game_time"]["delta_minutes"], 20)
        self.assertEqual(observation["latest_transition"]["value"]["sequence"], 3)
        self.assertEqual(observation["repeated_transition_summary"][0]["count"], 2)
        self.assertEqual(observation["continuous_final_certification_credit"], 0)

    def test_integrated_wait_marks_unchanged_native_time_stalled(self) -> None:
        events = [event(1, 100)]
        observation = observe_integrated_wait(
            label="stalled_wait", before_events=events, after_events=events,
            resources_before={}, resources_after={}, wait_completion={},
        )
        self.assertEqual(observation["product_game_time"]["status"], "stalled")
        self.assertEqual(observation["latest_transition"]["status"], "unavailable")

    def test_missing_native_time_and_metrics_are_explicitly_unavailable(self) -> None:
        observation = observe_integrated_wait(
            label="missing", before_events=[], after_events=[],
            resources_before={}, resources_after={}, wait_completion={},
        )
        self.assertEqual(observation["product_game_time"]["status"], "unavailable")
        self.assertIsNone(observation["product_game_time"]["delta_minutes"])

    def test_linux_cpu_never_invents_a_zero(self) -> None:
        before = {"platform": "linux", "sampled_monotonic_seconds": 10.0, "cpu_ticks": 40}
        after = {"platform": "linux", "sampled_monotonic_seconds": 12.0, "cpu_ticks": 44}
        completed = complete_child_resource_interval(before, after, clock_ticks_per_second=100)
        self.assertEqual(completed["cpu_percent"]["status"], "available")
        self.assertEqual(completed["cpu_percent"]["value"], 2.0)
        unavailable = complete_child_resource_interval(before, {"platform": "linux", "sampled_monotonic_seconds": 12.0}, clock_ticks_per_second=100)
        self.assertEqual(unavailable["cpu_percent"]["status"], "unavailable")
        self.assertIsNone(unavailable["cpu_percent"]["value"])

    def test_windows_cpu_uses_an_interval_when_the_host_exposes_it(self) -> None:
        before = {"platform": "windows", "sampled_monotonic_seconds": 1.0, "cpu_seconds": 3.0}
        after = {"platform": "windows", "sampled_monotonic_seconds": 3.0, "cpu_seconds": 3.5}
        completed = complete_child_resource_interval(before, after)
        self.assertEqual(completed["cpu_percent"]["status"], "available")
        self.assertEqual(completed["cpu_percent"]["value"], 25.0)

    def test_platform_witness_keeps_a_platform_limitation_and_zero_credit(self) -> None:
        witness = technical_witness(
            platform_name="windows", build_runtime_binding={"executable": "cataclysm-tiles.exe"},
            route={"scenario": "wait"}, direct_result={"status": "observed"},
            limitation="Windows process metrics were unavailable on this host.",
        )
        self.assertEqual(witness["platform"], "windows")
        self.assertEqual(witness["continuous_final_certification_credit"], 0)
        self.assertTrue(witness["platform_limitation"])

    def test_preflight_contract_covers_every_supported_route_without_launch_credit(self) -> None:
        contract = preflight_contract()

        self.assertEqual(contract["schema"], "r009-platform-preflight-v1")
        self.assertEqual(
            contract["semantic_wait_request"]["required_action_chain"],
            ["world.wait", "wait.duration_menu", "wait.6h"],
        )
        self.assertEqual(
            contract["resource_field_contract"]["unavailable_representation"],
            {"status": "unavailable", "value": None},
        )
        self.assertTrue({"macos", "linux", "linux-wsl", "windows"}.issubset(
            contract["supported_platform_routes"]
        ))
        self.assertFalse(contract["starts_selected_run"])
        self.assertEqual(contract["continuous_final_certification_credit"], 0)


if __name__ == "__main__":
    unittest.main()
