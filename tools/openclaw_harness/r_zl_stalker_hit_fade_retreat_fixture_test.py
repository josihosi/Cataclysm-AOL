#!/usr/bin/env python3
"""Focused contract checks for the dedicated native retreat fixture."""

from __future__ import annotations

import json
import unittest
from pathlib import Path

from scenario_registry import validate_manifest
from startup_harness import normalize_fixture_save_transforms


HARNESS_DIR = Path(__file__).resolve().parent
FIXTURE_PATH = HARNESS_DIR / "fixtures" / "saves" / "live-debug" / \
    "mcwilliams_live_debug_hit_fade_retreat_stalker_2026-05-02" / "manifest.json"
SCENARIO_PATH = HARNESS_DIR / "scenarios" / \
    "writhing_stalker.live_hit_fade_retreat_mcw.json"


class WrithingStalkerHitFadeRetreatFixtureTest(unittest.TestCase):
    def test_fixture_and_observer_use_same_passable_offset(self) -> None:
        fixture = json.loads(FIXTURE_PATH.read_text(encoding="utf-8"))
        transforms = normalize_fixture_save_transforms(
            fixture["save_transforms"], manifest_path=FIXTURE_PATH
        )
        monster = next(
            item["monsters"][0] for item in transforms
            if item["kind"] == "active_monsters_near_player"
        )
        self.assertEqual(monster["typeid"], "mon_writhing_stalker")
        self.assertEqual(monster["offset_ms"], [4, 0, 0])

        scenario = json.loads(SCENARIO_PATH.read_text(encoding="utf-8"))
        saved = next(
            step for step in scenario["steps"]
            if step["label"] == "audit_saved_writhing_stalker_before_hit_fade_retreat"
        )
        self.assertEqual(
            saved["required_monsters"][0]["offset_ms"], [4, 0, 0]
        )
        self.assertIn("four tiles east", saved["expected_visible_fact"])

    def test_contract_is_native_and_does_not_inject_retreat(self) -> None:
        scenario = json.loads(SCENARIO_PATH.read_text(encoding="utf-8"))
        self.assertEqual(validate_manifest(scenario, path=SCENARIO_PATH)["validation"]["status"], "valid")
        self.assertEqual(
            scenario["capabilities"]["capabilities.r_zl_stalker.native_attack_retreat"],
            "native_attack_attempt_then_retained_retreat_state",
        )
        forbidden = scenario["runtime_contract"]["forbidden_input"]
        self.assertIn("debug:inject_retreat", forbidden)
        self.assertNotIn("debug:inject_retreat", json.dumps(
            next(step for step in scenario["steps"] if step["kind"] == "cockpit_live_session")
        ))

    def test_post_relaunch_contract_is_native_and_typed(self) -> None:
        scenario = json.loads(SCENARIO_PATH.read_text(encoding="utf-8"))
        terminal = next(
            step for step in scenario["steps"]
            if step["kind"] == "semantic_terminal_action_chain"
        )
        self.assertEqual(
            terminal["required_action_chain"],
            ["world.save_quit", "prompt.yes", "main_menu.quit", "prompt.yes"],
        )
        continuation = scenario["post_relaunch"]
        self.assertEqual(
            continuation["terminal_save_step_label"], terminal["label"]
        )
        audit = next(
            step for step in continuation["steps"]
            if step["kind"] == "audit_saved_active_monsters"
        )
        stalker = audit["required_monsters"][0]
        self.assertEqual(stalker["typeid"], "mon_writhing_stalker")
        self.assertEqual(stalker["writhing_stalker_state"]["phase"], 1)
        self.assertEqual(stalker["writhing_stalker_state"]["attempts_spent"], 0)
        self.assertEqual(
            stalker["writhing_stalker_state"]["retreat_waypoint"], [0, 0, 0]
        )


if __name__ == "__main__":
    unittest.main()
