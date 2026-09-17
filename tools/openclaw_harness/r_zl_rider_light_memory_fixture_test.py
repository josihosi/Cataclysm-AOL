#!/usr/bin/env python3
"""Focused powered-vs-disconnected rider light fixture contract checks."""

import json
import shutil
import sys
import tempfile
import unittest
from unittest import mock
from pathlib import Path

ROOT = Path(__file__).resolve().parent
FIXTURE = ROOT / "fixtures/saves/live-debug/mcwilliams_live_debug_zombie_rider_light_memory_2026-09-13/manifest.json"
SCENARIO = ROOT / "scenarios/zombie_rider.live_light_memory_continuity_mcw.json"

sys.path.insert(0, str(ROOT))
import startup_harness  # noqa: E402


class RzlRiderLightMemoryFixtureTest(unittest.TestCase):
    def test_fixture_uses_powered_terrain_and_forbids_disconnected_appliance(self):
        manifest = json.loads(FIXTURE.read_text())
        furniture = next(t for t in manifest["save_transforms"] if t["kind"] == "map_furniture_near_player")
        self.assertEqual(furniture["furniture"][0]["id"], "f_caol_powered_exposed_lamp")

    def test_scenario_requires_powered_source_and_native_bash_off(self):
        scenario = json.loads(SCENARIO.read_text())
        audit = next(s for s in scenario["steps"] if s["kind"] == "audit_saved_map_tile_near_player")
        self.assertEqual(audit["required_furniture"], ["f_caol_powered_exposed_lamp"])
        self.assertEqual(audit["forbidden_furniture"], ["f_exodii_lamp"])
        live = next(s for s in scenario["steps"] if s["kind"] == "cockpit_live_session")
        self.assertIn("powered exposed utility light", live["objective"])
        self.assertIn("source-off", " ".join(live["proof_targets"]) + live["objective"])

    def test_scenario_pins_non_contaminating_weather(self):
        scenario = json.loads(SCENARIO.read_text())
        weather_transform = next(t for t in json.loads(FIXTURE.read_text())["save_transforms"]
                                 if t["kind"] == "world_option_overrides")
        self.assertEqual(weather_transform["overrides"].get("ETERNAL_WEATHER"), "clear")
        weather_audit = next(s for s in scenario["steps"] if s["kind"] == "audit_saved_weather_state")
        self.assertEqual(weather_audit["required_weather_id"], "clear")

    def test_post_transform_serialized_map_matches_game_load_contract(self):
        """Inspect the exact zzip payload after overlays, as the game loads it."""
        manifest = json.loads(FIXTURE.read_text())
        contract = manifest["serialized_map_contract"]
        resolved = startup_harness.resolve_fixture_payload(
            manifest["name"], "live-debug"
        )
        with tempfile.TemporaryDirectory(prefix="rzl_fixture_contract_") as temp_dir:
            world = Path(temp_dir) / contract["world"]
            shutil.copytree(resolved["save_src"] / contract["world"], world)
            startup_harness.apply_fixture_save_transforms(world, resolved["save_transforms"])
            audit = startup_harness.audit_map_tiles_near_player(
                world,
                player_save=contract["player_save"],
                offsets=[tuple(contract["offset_ms"])],
                required_furniture=contract["required_furniture"],
                forbidden_furniture=contract["forbidden_furniture"],
            )
        self.assertEqual(audit["status"], "required_state_present")
        tile = audit["tiles"][0]
        for key in ("target_abs_omt", "target_abs_sm", "local_ms"):
            self.assertEqual(tile[key], contract[key])
        self.assertEqual(tile["furniture"], contract["required_furniture"])
        self.assertEqual(audit["observed_forbidden_furniture"], [])

    def test_authorized_source_off_is_verified_and_rejects_noop(self):
        """The custom fixture must be cleared, not merely receipt-acknowledged."""
        source = (ROOT.parent.parent / "src" / "handle_action.cpp").resolve()
        text = source.read_text()
        self.assertIn('f_caol_powered_exposed_lamp', text)
        self.assertIn('openclaw_harness_turn_off_rider_fixture_lamp', text)
        self.assertIn('fixture_lamp_noop', text)
        self.assertIn('here.furn_set( point, furn_str_id::NULL_ID() )', text)
        self.assertIn('after.light_emitted == 0', text)

    def test_post_relaunch_saved_audit_uses_immutable_handoff_snapshot(self):
        with tempfile.TemporaryDirectory(prefix="rzl_snapshot_audit_") as temp_dir:
            root = Path(temp_dir)
            snapshot = root / "replacement_saved_world" / "McWilliams"
            profile_world = root / "mutable_profile" / "McWilliams"
            for world_dir in (snapshot, profile_world):
                world_dir.mkdir(parents=True)
                payload = {
                    "zombie_rider_light_memory": [{
                        "turns_remaining": 12,
                        "source_present": False,
                        "riders": [{"light_intent": {"posture": "investigate"}}],
                    }]
                }
                (world_dir / "dimension_data.gsav").write_text(
                    "# version\n" + json.dumps(payload), encoding="utf-8"
                )
            run_dir = root / "run"
            run_dir.mkdir()
            with mock.patch.object(startup_harness, "save_dir_for_profile", return_value=root / "mutable_profile"):
                reports = startup_harness.execute_probe_steps(
                    0, run_dir, [{
                        "kind": "audit_saved_zombie_rider_light_memory",
                        "label": "immutable_audit",
                        "required_source_present": False,
                        "required_memory_active": True,
                        "required_intent": "investigate",
                    }],
                    profile="dev-harness", world="McWilliams",
                    immutable_saved_world_snapshot=snapshot,
                )
            self.assertEqual(reports[0]["metadata"]["status"], "green_required_state_present")
            self.assertEqual(reports[0]["metadata"]["world_dir"], str(snapshot))
            self.assertEqual(
                reports[0]["metadata"]["dimension_path"],
                str(snapshot / "dimension_data.gsav"),
            )

    def test_continuation_resolves_saved_world_handoff_instead_of_profile_world(self):
        with tempfile.TemporaryDirectory(prefix="rzl_continuation_snapshot_") as temp_dir:
            snapshot = Path(temp_dir) / "replacement_saved_world" / "McWilliams"
            snapshot.mkdir(parents=True)
            args = type("Args", (), {"saved_world_snapshot": str(snapshot)})()
            resolved = startup_harness.continuation_saved_world_snapshot(
                args, post_relaunch_continuation=True,
            )
            self.assertEqual(resolved, snapshot.resolve())
            self.assertIsNone(startup_harness.continuation_saved_world_snapshot(
                args, post_relaunch_continuation=False,
            ))

    def test_continuation_reads_registered_bridge_handoff_pointer(self):
        with tempfile.TemporaryDirectory(prefix="rzl_bridge_snapshot_") as temp_dir:
            root = Path(temp_dir)
            snapshot = root / "replacement_saved_world" / "McWilliams"
            snapshot.mkdir(parents=True)
            session = root / "bridge-session"
            session.mkdir()
            (session / "replacement_saved_world.snapshot.json").write_text(
                json.dumps({"snapshot": str(snapshot)}), encoding="utf-8"
            )
            args = type("Args", (), {"saved_world_snapshot": ""})()
            with mock.patch.dict(
                startup_harness.os.environ,
                {"OPENCLAW_COCKPIT_BRIDGE_SESSION_DIR": str(session)},
                clear=False,
            ):
                resolved = startup_harness.continuation_saved_world_snapshot(
                    args, post_relaunch_continuation=True,
                )
            self.assertEqual(resolved, snapshot.resolve())


if __name__ == "__main__":
    unittest.main()
