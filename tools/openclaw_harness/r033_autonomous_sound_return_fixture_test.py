#!/usr/bin/env python3
"""Guard the R-033 delayed source against an unloaded-map timer regression."""

from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path


HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

from startup_harness import resolve_fixture_payload  # noqa: E402


FIXTURE = "r033_phase4_autonomous_sound_return_v1"
DRIVE500_FIXTURE = "r033_phase4_autonomous_sound_return_drive500_v1"
SCENARIO = HARNESS_DIR / "scenarios" / "r033.phase4_semantic_autonomous_sound_return_rev1_mcw.json"
DRIVE500_SCENARIO = HARNESS_DIR / "scenarios" / "r033.phase4_semantic_autonomous_sound_return_drive500_rev1_mcw.json"


class R033AutonomousSoundReturnFixtureTest(unittest.TestCase):
    def test_drive500_successor_changes_only_the_prelaunch_supply_premise(self) -> None:
        retained = resolve_fixture_payload(FIXTURE, "live-debug")
        successor = resolve_fixture_payload(DRIVE500_FIXTURE, "live-debug")
        self.assertEqual(successor["save_transforms"][:-1], retained["save_transforms"])
        supply = successor["save_transforms"][-1]
        self.assertEqual(supply["kind"], "bandit_camp_supply")
        self.assertEqual(supply["site_id"], "overmap_special:bandit_camp@140,51,0")
        self.assertEqual(supply["supply_units"], 0)
        self.assertEqual(supply["supply_last_update_minutes"], 9241)
        self.assertEqual(supply["supply_accounted_living_total"], 5)
        self.assertEqual(supply["supply_member_minute_remainder"], 0)

    def test_delayed_acoustic_charge_starts_inside_the_declared_loaded_map_cap(self) -> None:
        resolved = resolve_fixture_payload(FIXTURE, "live-debug")
        transforms = resolved["save_transforms"]
        source_index = next(
            index for index, transform in enumerate(transforms)
            if transform["kind"] == "map_items_near_player"
            and transform["items"][0]["typeid"] == "r033_acoustic_charge_act"
        )
        relocation = transforms[source_index - 1]
        source = transforms[source_index]["items"][0]
        scenarios = [json.loads(path.read_text(encoding="utf-8")) for path in (SCENARIO, DRIVE500_SCENARIO)]
        audits = [next(step for step in scenario["steps"] if step["label"] == "audit_delayed_zero_credit_explosion_source")
                  for scenario in scenarios]

        self.assertEqual(relocation["kind"], "player_location_offset_ms")
        self.assertEqual(relocation["offset_ms"], [24, 48, 0])
        self.assertEqual(source["offset_ms"], [0, 48, 0])
        for audit in audits:
            self.assertEqual(audit["offsets"], [[0, 48, 0]])
            self.assertEqual(audit["maximum_abs_offset_ms"], 48)
            self.assertLessEqual(max(abs(value) for value in source["offset_ms"]), audit["maximum_abs_offset_ms"])

        # The witnessed failing configuration was outside this preflight cap.
        self.assertGreater(max(abs(value) for value in [24, 72, 0]), audits[0]["maximum_abs_offset_ms"])

    def test_source_is_at_the_witnessed_local_watch(self) -> None:
        resolved = resolve_fixture_payload(FIXTURE, "live-debug")
        source = next(
            transform["items"][0]
            for transform in resolved["save_transforms"]
            if transform["kind"] == "map_items_near_player"
            and transform["items"][0]["typeid"] == "r033_acoustic_charge_act"
        )
        self.assertEqual(source["offset_ms"], [0, 48, 0])

    def test_source_timer_sits_between_the_witnessed_watch_and_return_boundaries(self) -> None:
        resolved = resolve_fixture_payload(FIXTURE, "live-debug")
        source = next(
            transform["items"][0]
            for transform in resolved["save_transforms"]
            if transform["kind"] == "map_items_near_player"
            and transform["items"][0]["typeid"] == "r033_acoustic_charge_act"
        )
        saved_turn = 5306460
        saved_minute = 9241
        cohesive_local_watch_arrival = 9360
        earliest_physical_return = 9375
        source_minute = saved_minute + source["countdown_offset_turns"] // 60

        self.assertEqual(source["countdown_offset_turns"], 7440)
        self.assertEqual(saved_turn + source["countdown_offset_turns"], 5313900)
        self.assertEqual(source_minute, 9365)
        self.assertGreater(source_minute, cohesive_local_watch_arrival)
        self.assertLess(source_minute, earliest_physical_return)


if __name__ == "__main__":
    unittest.main()
