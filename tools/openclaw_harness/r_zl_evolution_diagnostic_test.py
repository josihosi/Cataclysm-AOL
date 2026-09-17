#!/usr/bin/env python3
"""Contract for the scenario-gated natural-evolution actor diagnostic."""

from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path

HARNESS_DIR = Path(__file__).resolve().parent
ROOT = HARNESS_DIR.parent.parent
sys.path.insert(0, str(HARNESS_DIR))

from scenario_registry import validate_manifest  # noqa: E402
from startup_harness import metadata_checkpoint_verdict, resolve_fixture_payload  # noqa: E402


class RzlEvolutionDiagnosticTest(unittest.TestCase):
    def test_persisted_natural_evolution_audit_is_green_step_local_metadata(self) -> None:
        self.assertEqual(
            metadata_checkpoint_verdict({"status": "green_native_natural_evolution_persisted"}),
            ("green_step_metadata_required_state_present", []),
        )

    def test_scenario_is_valid_and_keeps_the_actor_identity_explicit(self) -> None:
        path = HARNESS_DIR / "scenarios" / "zombie_rider.live_natural_evolution_mcw.json"
        manifest = json.loads(path.read_text(encoding="utf-8"))
        self.assertEqual(validate_manifest(manifest, path=path)["validation"]["status"], "valid")
        self.assertEqual(
            manifest["artifact_patterns"],
            ["mon_zombie_rider", "ammo_initialization_version"],
        )
        audit = next(step for step in manifest["steps"] if step["kind"] == "audit_saved_active_monsters")
        self.assertEqual(audit["required_monsters"], [{
            "typeid": "mon_zombie_predator",
            "fixture_actor_id": "predator_natural_evolution_1",
            "offset_ms": [10, 4, 0],
            "friendly": 0,
            "dead": False,
        }])

        fixture_path = HARNESS_DIR / "fixtures" / "saves" / "live-debug" / manifest["fixture"] / "manifest.json"
        fixture = json.loads(fixture_path.read_text(encoding="utf-8"))
        actor_transform = next(
            transform for transform in fixture["save_transforms"]
            if transform["kind"] == "active_monsters_near_player"
        )
        self.assertEqual(actor_transform["monsters"], [{
            "typeid": "mon_zombie_predator",
            "fixture_actor_id": "predator_natural_evolution_1",
            "offset_ms": [10, 4, 0],
            "hp": 100,
            "friendly": 0,
            "faction": "zombie",
            "anger": 100,
            "morale": 100,
            "upgrades": True,
            "upgrade_time": 10000,
        }])
        resolved = resolve_fixture_payload(manifest["fixture"], "live-debug")
        normalized_actor = next(
            transform for transform in resolved["save_transforms"]
            if transform["kind"] == "active_monsters_near_player"
        )["monsters"][0]
        self.assertTrue(normalized_actor["upgrades"])
        self.assertEqual(normalized_actor["upgrade_time"], 10000)
        source = (HARNESS_DIR / "startup_harness.py").read_text(encoding="utf-8")
        self.assertIn('"upgrades": bool(raw_monster.get("upgrades", False))', source)
        self.assertIn('"upgrade_time": int(raw_monster.get("upgrade_time", -1))', source)

    def test_emitter_is_read_only_exactly_gated_and_reports_the_required_schema(self) -> None:
        source = (ROOT / "src" / "handle_action.cpp").read_text(encoding="utf-8")
        gate = '"zombie_rider.live_natural_evolution_mcw"'
        self.assertIn("openclaw_harness_diagnostic_natural_evolution_monster", source)
        self.assertEqual(source.count(gate), 2)
        for field in (
            'caol-zombie-rider-natural-evolution-v1',
            'fixture_actor_id',
            '\\"typeid\\"',
            '\\"absolute_ms\\"',
            '\\"can_upgrade\\"',
            '\\"upgrade_time\\"',
            '\\"ammo_initialization_version\\"',
            '\\"tainted_bone_arrow_ammo\\"',
            'read_only_diagnostic_not_player_knowledge',
        ):
            self.assertIn(field, source)
        self.assertIn('critter.get_value( "caol_fixture_actor_id" ).str() !=', source)
        self.assertIn('g->all_monsters()', source)
        self.assertNotIn('critter.allow_upgrade()', source)
        self.assertNotIn('critter.try_upgrade(', source)
        self.assertNotIn('critter.ammo[tainted_bone_arrow] =', source)


if __name__ == "__main__":
    unittest.main()
