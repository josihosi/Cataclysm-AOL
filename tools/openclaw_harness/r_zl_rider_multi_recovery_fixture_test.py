#!/usr/bin/env python3
"""Contract checks for the multi-rider impact recovery control fixture."""

from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from scenario_registry import validate_manifest  # noqa: E402
from startup_harness import normalize_fixture_save_transforms  # noqa: E402


HARNESS_DIR = Path(__file__).resolve().parent
FIXTURE = HARNESS_DIR / "fixtures/saves/live-debug/mcwilliams_live_debug_zombie_rider_impact_multi_recovery_controls_2026-09-12/manifest.json"
SCENARIO = HARNESS_DIR / "scenarios/zombie_rider.live_native_multi_recovery_controls_mcw.json"


class RiderMultiRecoveryFixtureTest(unittest.TestCase):
    def test_fixture_has_identified_empty_and_stocked_riders(self) -> None:
        fixture = json.loads(FIXTURE.read_text(encoding="utf-8"))
        transforms = normalize_fixture_save_transforms(fixture["save_transforms"], manifest_path=FIXTURE)
        roster = next(item["monsters"] for item in transforms if item["kind"] == "active_monsters_near_player")
        self.assertEqual([(item["fixture_actor_id"], item["ammo"]) for item in roster], [
            ("rider-empty-east", {}),
            ("rider-stocked-south", {"zombie_rider_tainted_bone_arrow": 18}),
        ])
        self.assertTrue(all(item["aggro_character"] for item in roster))

    def test_scenario_exactly_audits_bow_premise_and_native_recovery(self) -> None:
        scenario = json.loads(SCENARIO.read_text(encoding="utf-8"))
        self.assertEqual(validate_manifest(scenario, path=SCENARIO)["validation"]["status"], "valid")
        audit = next(step for step in scenario["steps"] if step["label"] == "audit_saved_identified_empty_and_stocked_rider_footing")
        self.assertEqual(audit["required_monsters"][0]["ammo_exact"], {"zombie_rider_tainted_bone_arrow": 0})
        self.assertEqual(audit["required_monsters"][1]["ammo_exact"], {"zombie_rider_tainted_bone_arrow": 18})
        live = next(step for step in scenario["steps"] if step["kind"] == "cockpit_live_session")
        self.assertIn("debug:inject_player_effect", scenario["runtime_contract"]["forbidden_input"])
        self.assertIn("advertised and accepted", live["invariants"][-2])

if __name__ == "__main__":
    unittest.main()
