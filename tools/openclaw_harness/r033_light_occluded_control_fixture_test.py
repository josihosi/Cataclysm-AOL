#!/usr/bin/env python3
"""Narrow invariants for the matched R-033 lamp-containment control."""

from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path


HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

from startup_harness import normalize_fixture_save_transforms, resolve_fixture_payload  # noqa: E402


FIXTURE_ROOT = HARNESS_DIR / "fixtures" / "saves" / "live-debug"
EXPOSED = FIXTURE_ROOT / "r033_bandit_light_only_observer_v1" / "manifest.json"
CONTROL = FIXTURE_ROOT / "r033_bandit_light_occluded_control_v1" / "manifest.json"


class R033LightOccludedControlFixtureTest(unittest.TestCase):
    def test_changes_only_the_local_escape_aperture(self) -> None:
        exposed = json.loads(EXPOSED.read_text(encoding="utf-8"))
        control = json.loads(CONTROL.read_text(encoding="utf-8"))
        self.assertEqual(control["source_fixture"], exposed["name"])
        transforms = normalize_fixture_save_transforms(
            control["save_transforms"], manifest_path=CONTROL
        )
        self.assertEqual(len(transforms), 1)
        self.assertEqual(transforms[0]["kind"], "map_terrain_near_player")
        terrain = transforms[0]["terrain"]
        self.assertEqual(terrain[0], {"id": "t_floor", "offset_ms": [10, 0, 0]})
        self.assertEqual({tuple(item["offset_ms"]) for item in terrain[1:]}, {
            (9, -1, 0), (10, -1, 0), (11, -1, 0), (9, 0, 0),
            (11, 0, 0), (9, 1, 0), (10, 1, 0), (11, 1, 0),
        })
        self.assertTrue(all(item["id"] == "t_wall" for item in terrain[1:]))

    def test_inherits_the_exact_lamp_clock_and_two_person_roster(self) -> None:
        resolved = resolve_fixture_payload("r033_bandit_light_occluded_control_v1", "live-debug")
        transforms = resolved["save_transforms"]
        lamp = next(item for item in transforms if item["kind"] == "map_furniture_near_player")
        roster = next(item for item in transforms if item["kind"] == "overmap_npcs_near_player")
        clock = next(item for item in transforms if item["kind"] == "game_turn")
        self.assertEqual(lamp["furniture"], [{"id": "f_exodii_lamp", "offset_ms": [10, 0, 0]}])
        self.assertEqual(roster["offsets_ms"], [[0, -24, -1], [1, -24, -1]])
        self.assertEqual(clock["turn"], 5266800)


if __name__ == "__main__":
    unittest.main()
