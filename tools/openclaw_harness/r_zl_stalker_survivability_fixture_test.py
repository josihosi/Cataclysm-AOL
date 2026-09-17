#!/usr/bin/env python3
"""Focused contract checks for the survivable writhing-stalker fixture."""

from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from scenario_registry import validate_manifest  # noqa: E402
from startup_harness import normalize_fixture_save_transforms  # noqa: E402


HARNESS_DIR = Path(__file__).resolve().parent
FIXTURE_PATH = HARNESS_DIR / "fixtures" / "saves" / "live-debug" / \
    "mcwilliams_live_debug_noon_heavy_zombie_stalker_2026-09-12" / "manifest.json"
SCENARIO_PATH = HARNESS_DIR / "scenarios" / \
    "writhing_stalker.live_daylight_heavy_zombie_pressure_mcw.json"


class WrithingStalkerSurvivabilityFixtureTest(unittest.TestCase):
    def test_full_hp_is_the_only_player_intervention(self) -> None:
        fixture = json.loads(FIXTURE_PATH.read_text(encoding="utf-8"))
        transforms = normalize_fixture_save_transforms(
            fixture["save_transforms"], manifest_path=FIXTURE_PATH
        )
        condition = next(item for item in transforms if item["kind"] == "player_condition")
        self.assertEqual(
            condition,
            {
                "kind": "player_condition",
                "player_save": "#Wm9yYWlkYSBWaWNr.sav.zzip",
                "hp_percent": 100,
            },
        )
        self.assertNotIn("stamina", condition)
        self.assertNotIn("effects", condition)

    def test_hostile_roster_and_native_scenario_contract_are_unchanged(self) -> None:
        fixture = json.loads(FIXTURE_PATH.read_text(encoding="utf-8"))
        monsters = next(
            item["monsters"] for item in fixture["save_transforms"]
            if item["kind"] == "active_monsters_near_player"
        )
        self.assertEqual([item["typeid"] for item in monsters], [
            "mon_writhing_stalker", "mon_zombie", "mon_zombie", "mon_zombie", "mon_zombie",
        ])
        self.assertTrue(all(
            item["friendly"] == 0 and item["faction"] == "zombie" and
            item["anger"] == 100 and item["morale"] == 100 and
            item["aggro_character"] is True
            for item in monsters
        ))

        scenario = json.loads(SCENARIO_PATH.read_text(encoding="utf-8"))
        self.assertEqual(validate_manifest(scenario, path=SCENARIO_PATH)["validation"]["status"], "valid")
        self.assertIn("full-HP", scenario["description"])
        self.assertIn("native stalker AI remain unchanged", scenario["description"])
        self.assertTrue(scenario["runtime_contract"]["grants_gameplay_proof"])


if __name__ == "__main__":
    unittest.main()
