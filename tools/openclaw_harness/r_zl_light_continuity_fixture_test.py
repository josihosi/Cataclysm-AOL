#!/usr/bin/env python3
"""Focused contract checks for the isolated physical-light stalker spoke."""

from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from scenario_registry import validate_manifest
from startup_harness import normalize_saved_writhing_stalker_state


HARNESS_DIR = Path(__file__).resolve().parent
FIXTURE_PATH = HARNESS_DIR / "fixtures" / "saves" / "live-debug" / \
    "r033_bandit_light_continuity_stalker_v1" / "manifest.json"
SCENARIO_PATH = HARNESS_DIR / "scenarios" / \
    "writhing_stalker.live_light_continuity_mcw.json"


class RzlLightContinuityFixtureTest(unittest.TestCase):
    def test_fixture_isolates_player_los_and_uses_same_roof_lamp_neighbor(self) -> None:
        fixture = json.loads(FIXTURE_PATH.read_text(encoding="utf-8"))
        furniture = next(item["furniture"] for item in fixture["save_transforms"]
                         if item["kind"] == "map_furniture_near_player")
        self.assertEqual([item["offset_ms"] for item in furniture], [[4, -1, 0], [4, 0, 0], [4, 1, 0]])
        monster = next(item for transform in fixture["save_transforms"]
                       if transform["kind"] == "active_monsters_near_player"
                       for item in transform["monsters"])
        self.assertEqual(monster["offset_ms"], [9, 0, 0])
        self.assertEqual(monster["friendly"], 0)
        self.assertEqual(monster["faction"], "zombie")

    def test_scenario_requires_clean_saved_owner_and_native_route(self) -> None:
        scenario = json.loads(SCENARIO_PATH.read_text(encoding="utf-8"))
        self.assertEqual(validate_manifest(scenario, path=SCENARIO_PATH)["validation"]["status"], "valid")
        audit = next(step for step in scenario["steps"] if step["kind"] == "audit_saved_active_monsters")
        state = audit["required_monsters"][0]["writhing_stalker_state"]
        self.assertEqual(state["evidence_target"], 0)
        self.assertFalse(state["has_last_observed_position"])
        self.assertFalse(state["has_light_observed_position"])
        self.assertEqual(state["light_expires_turn"], -1)
        live = next(step for step in scenario["steps"] if step["kind"] == "cockpit_live_session")
        self.assertEqual(set(live["live_operations"]), {"game.act", "game.wait", "game.raw_wait", "game.move_relative"})
        self.assertIn("direct-target absence", live["objective"])
        self.assertIn("expires", live["objective"])
        self.assertTrue(scenario["runtime_contract"]["forbidden_input"])

    def test_saved_audit_materializes_native_default_memory_fields(self) -> None:
        state = normalize_saved_writhing_stalker_state("mon_writhing_stalker", {})
        self.assertEqual(state["evidence_target"], 0)
        self.assertEqual(state["evidence_turn"], -1)
        self.assertFalse(state["has_last_observed_position"])
        self.assertFalse(state["has_light_observed_position"])
        self.assertEqual(state["light_observed_turn"], -1)
        self.assertEqual(state["light_expires_turn"], -1)
        self.assertEqual(state["light_sample_id"], "")
        self.assertFalse(state["has_committed_waypoint"])


if __name__ == "__main__":
    unittest.main()
