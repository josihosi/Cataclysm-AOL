#!/usr/bin/env python3
"""Behavioral counterexamples for the outcome-sized R-008 fire-signal family."""

from __future__ import annotations

import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HARNESS = ROOT / "tools" / "openclaw_harness"
sys.path.insert(0, str(HARNESS))

import startup_harness  # noqa: E402
from scenario_registry import validate_manifest  # noqa: E402


SCENARIOS = (
    "bandit.r008_fire_signal_roof_lifecycle_mcw",
    "cannibal.r008_fire_signal_roof_lifecycle_mcw",
    "bandit.r008_fire_signal_indoor_closed_mcw",
    "bandit.r008_fire_signal_indoor_open_mcw",
)
FIXTURE_ROOT = HARNESS / "fixtures" / "saves" / "live-debug"
PLAYER = "#Wm9yYWlkYSBWaWNr.sav.zzip"


def load_json(path: Path) -> dict[str, object]:
    return json.loads(path.read_text(encoding="utf-8"))


class R008FireSignalFamilyTest(unittest.TestCase):
    def test_family_manifests_are_selectable_outcomes_not_short_wait_scripts(self) -> None:
        for name in SCENARIOS:
            with self.subTest(name=name):
                path = HARNESS / "scenarios" / f"{name}.json"
                manifest = load_json(path)
                validated = validate_manifest(manifest, path=path)
                self.assertEqual(validated["validation"]["status"], "valid")
                kinds = [step["kind"] for step in manifest["steps"]]
                self.assertNotIn("wait_action", kinds)
                self.assertNotIn("advance_turns", kinds)
                session = next(step for step in manifest["steps"]
                               if step["kind"] == "cockpit_live_session")
                text = json.dumps(session)
                self.assertIn("exactly two", text)
                self.assertIn("ordinary player", text.lower())
                self.assertIn("game.guarded_move_relative", session["live_operations"])
                self.assertIn("game.keep_watch", session["live_operations"])
                self.assertIn("game.raw_wait", session["live_operations"])

    def test_cannibal_persisted_return_session_allows_the_declared_six_hour_primitive(self) -> None:
        manifest = load_json(HARNESS / "scenarios" /
                             "cannibal.r008_fire_signal_roof_lifecycle_mcw.json")
        post_relaunch = manifest["post_relaunch"]
        session = next(step for step in post_relaunch["steps"]
                       if step["kind"] == "cockpit_live_session")
        self.assertEqual(session["label"], "observe_persisted_cannibal_return_lifecycle")
        self.assertGreaterEqual(session["transition_timeout_seconds"], 420.0)

    def test_post_relaunch_preserves_semantic_bootstrap_wait_and_boundary(self) -> None:
        source = (HARNESS / "startup_harness.py").read_text(encoding="utf-8")
        post_relaunch_call = source.index("post_reports = execute_probe_steps(")
        post_relaunch_arguments = source[post_relaunch_call:source.index("            for report in post_reports:", post_relaunch_call)]
        self.assertIn(
            "semantic_step_trace_start_offset=post_semantic_trace_start",
            post_relaunch_arguments,
        )
        self.assertIn(
            "semantic_bootstrap_timeout_seconds=semantic_bootstrap_timeout",
            post_relaunch_arguments,
        )
        self.assertIn(
            "semantic_bootstrap_poll_seconds=semantic_bootstrap_poll",
            post_relaunch_arguments,
        )

    def test_fixture_contract_distributes_fuel_without_manufacturing_response(self) -> None:
        roof = startup_harness.resolve_fixture_payload(
            "r008_fire_signal_roof_bandit_v1", "live-debug",
        )
        common = load_json(FIXTURE_ROOT / "r008_fire_signal_common_v1" / "manifest.json")
        player_item_transforms = [item for item in common["save_transforms"]
                                  if item["kind"] == "player_items"]
        brazier = next(item for item in player_item_transforms
                       if item["items"][0]["typeid"] == "brazier")
        lighter = next(item for item in player_item_transforms
                       if item["items"][0]["typeid"] == "lighter")
        self.assertEqual(brazier["storage"], "live_accessible_wielded")
        self.assertEqual(lighter["storage"], "live_accessible_worn_pocket")
        lighter_contents = lighter["items"][0]["contents"]["contents"]
        self.assertEqual(lighter_contents[0]["pocket_type"], 1)
        self.assertEqual(lighter_contents[0]["contents"][0]["typeid"], "butane")
        self.assertEqual(lighter_contents[0]["contents"][0]["charges"], 100)
        transforms = roof["save_transforms"]
        forbidden = {
            "horde_entity_near_player", "active_monsters_near_player", "map_fields_near_player",
            "bandit_camp_map_lead", "bandit_active_sortie_clock", "bandit_clear_site_evidence",
            "bandit_site_roster_shape", "game_turn", "seed_overmap_special_near_player",
        }
        self.assertFalse(forbidden.intersection(item["kind"] for item in transforms))
        for fixture in (
                "r008_fire_signal_roof_bandit_v1",
                "r008_fire_signal_roof_cannibal_v1",
                "r008_fire_signal_indoor_closed_v1",
                "r008_fire_signal_indoor_open_v1",
        ):
            eligibility = startup_harness.natural_ecology_fixture_eligibility(
                fixture, "live-debug",
            )
            self.assertEqual(eligibility["status"], "eligible")
        item_transform = next(item for item in transforms
                              if item["kind"] == "map_items_near_player")
        log_offsets = {
            tuple(item["offset_ms"])
            for item in item_transform["items"] if item["typeid"] == "log"
        }
        self.assertEqual(log_offsets, {(1, -1, 0), (1, 0, 0), (1, 1, 0)})
        zone = next(item for item in transforms
                    if item["kind"] == "source_firewood_zone_near_player")
        self.assertEqual(zone["start_offset_ms"], [1, -1, 0])
        self.assertEqual(zone["end_offset_ms"], [1, 1, 0])
        relocation = [item for item in transforms
                      if item["kind"] == "player_location_offset_ms"]
        self.assertEqual(relocation, [{
            "kind": "player_location_offset_ms", "player_save": PLAYER,
            "offset_ms": [0, 0, 1],
        }])

    def test_closed_and_open_indoor_footings_differ_only_by_declared_openings(self) -> None:
        closed = load_json(FIXTURE_ROOT / "r008_fire_signal_indoor_closed_v1" / "manifest.json")
        opened = load_json(FIXTURE_ROOT / "r008_fire_signal_indoor_open_v1" / "manifest.json")
        self.assertEqual(opened["source_fixture"], closed["name"])
        self.assertEqual(len(opened["save_transforms"]), 1)
        transform = opened["save_transforms"][0]
        self.assertEqual(transform["kind"], "map_terrain_near_player")
        self.assertEqual({item["id"] for item in transform["terrain"]},
                         {"t_door_o", "t_window_open"})

    def test_fixture_transforms_produce_a_roof_lab_and_matched_open_building(self) -> None:
        for fixture, expected_z, expected_openings in (
            ("r008_fire_signal_roof_bandit_v1", 1, set()),
            ("r008_fire_signal_indoor_open_v1", 0, {"t_door_o", "t_window_open"}),
        ):
            with self.subTest(fixture=fixture), tempfile.TemporaryDirectory() as directory:
                resolved = startup_harness.resolve_fixture_payload(fixture, "live-debug")
                source_world = next(path for path in resolved["save_src"].iterdir()
                                    if path.is_dir())
                world = Path(directory) / source_world.name
                shutil.copytree(source_world, world)
                receipts = startup_harness.apply_fixture_save_transforms(
                    world, list(resolved["save_transforms"]),
                )
                self.assertTrue(receipts)
                _omt, location = startup_harness.load_player_abs_omt(world, PLAYER)
                self.assertEqual(location[2], expected_z)
                tile_audit = startup_harness.audit_map_tiles_near_player(
                    world, player_save=PLAYER, radius=3,
                    offsets=[(0, 0, 0), (1, -1, 0), (1, 0, 0), (1, 1, 0),
                             (3, -3, 0), (3, -1, 0), (3, 0, 0), (3, 2, 0)],
                )
                terrain = {str(tile.get("terrain", "")) for tile in tile_audit["tiles"]}
                if expected_z == 1:
                    self.assertIn("t_tile_flat_roof", terrain)
                else:
                    self.assertTrue(expected_openings.issubset(terrain))
                log_tiles = [
                    tile for tile in tile_audit["tiles"]
                    if "log" in tile.get("items", [])
                ]
                self.assertEqual(len(log_tiles), 3)
                zone = startup_harness.audit_saved_zones_near_player(
                    world, player_save=PLAYER, required_zone_type="SOURCE_FIREWOOD",
                    required_offsets=[(1, -1, 0), (1, 0, 0), (1, 1, 0)],
                )
                self.assertTrue(zone["matching_zones"])

    def test_source_and_focused_tests_expose_the_declared_causal_boundary(self) -> None:
        source = (ROOT / "src" / "do_turn.cpp").read_text(encoding="utf-8")
        start = source.index("live_bandit_structural_signal_reads")
        signal_owner = source[start:start + 20000]
        self.assertIn("structural_sortie", signal_owner)
        self.assertIn("current_omt", signal_owner)
        self.assertIn("forward_omt", signal_owner)
        roof = load_json(HARNESS / "scenarios" /
                         "bandit.r008_fire_signal_roof_lifecycle_mcw.json")
        session = next(step for step in roof["steps"]
                       if step["kind"] == "cockpit_live_session")
        self.assertIn("earlier dispatch cause", session["objective"])
        faction_tests = (ROOT / "tests" / "faction_camp_test.cpp").read_text(encoding="utf-8")
        self.assertIn(
            "camp_patrol_alarm_watches_active_shakedown_contact_without_combat_escalation",
            faction_tests,
        )
        firestarter_tests = (ROOT / "tests" / "firestarter_activity_test.cpp").read_text(
            encoding="utf-8",
        )
        self.assertIn("f_brazier", firestarter_tests)
        self.assertIn("lighter", firestarter_tests)

    def test_normal_player_fire_transaction_requires_a_fresh_native_result(self) -> None:
        source = (ROOT / "src" / "handle_action.cpp").read_text(encoding="utf-8")
        self.assertIn('"hud_world_changed"', source)

    def test_live_journal_uses_the_frozen_runtime_source_not_a_scenario_digest(self) -> None:
        source = "a" * 64
        self.assertEqual(
            startup_harness.live_witness_source_identity({"runtime_source_sha256": source}), source,
        )
        self.assertEqual(startup_harness.live_witness_source_identity({}), "")
        self.assertEqual(
            startup_harness.live_witness_source_identity({"runtime_source_sha256": "scenario-sha"}), "",
        )

    def test_dirty_product_source_requires_a_build_receipt_before_registry_credit(self) -> None:
        source = (HARNESS / "startup_harness.py").read_text(encoding="utf-8")
        builder = (HARNESS / "build_source_bound_macos.py").read_text(encoding="utf-8")
        self.assertIn("receipt_bound_dirty_product_build", source)
        self.assertIn("product_source_sha256", source)
        self.assertIn("PRODUCT_BUILD_RECEIPT_SCHEMA", builder)
        self.assertIn('"make"', builder)
        self.assertIn("version_command", builder)
        self.assertIn('[*command, "version"]', builder)
        harness_source = (HARNESS / "startup_harness.py").read_text(encoding="utf-8")
        self.assertIn('action.get("id") == "world.wait"', harness_source)
        self.assertIn('"advertised_world_wait_unavailable"', harness_source)
        self.assertIn("Deployment puts the real brazier over that ground fuel", harness_source)
        self.assertIn('phase = "activate_pocket_lighter"', harness_source)
        self.assertIn('phase = "verify_charged_lighter"', harness_source)
        self.assertIn("r008_runtime_charged_lighter_preflight", harness_source)
        self.assertIn("r008_latest_native_fire_result", harness_source)
        self.assertIn("paired native ``frame`` event", harness_source)
        self.assertIn('peekaboo_press_sequence(pid, ["right"], delay_ms=200)', harness_source)
        self.assertNotIn('peekaboo_press_sequence(pid, ["right", "enter"]', harness_source)
        self.assertIn('phase = "confirm_firewood_source"', harness_source)
        self.assertIn("r008_firestarter_boundary_snapshot", harness_source)
        self.assertIn("r008_player_fire_setup.source_firewood_confirmation", harness_source)
        self.assertIn("source_firewood_confirmation_not_observed", harness_source)
        self.assertIn("source_firewood_confirmation_not_resolved", harness_source)
        self.assertIn('peekaboo_hotkey(pid, "shift,y", hold_ms=100, transport="bridge")', harness_source)
        self.assertIn('time.sleep(1.0)', harness_source)
        self.assertIn('auto_acknowledge_interruptions=False', harness_source)
        before = {
            "observation": {
                "visible_local": [{"dx": 1, "dy": 0, "fields": []}],
            },
        }
        after = {
            "observation": {
                "visible_local": [{
                    "dx": 1, "dy": 0, "furniture": "f_brazier", "fields": ["fd_fire"],
                }],
            },
        }
        self.assertFalse(startup_harness.r008_native_fire_result(before))
        self.assertTrue(startup_harness.r008_native_fire_result(after))
        self.assertEqual(
            startup_harness.r008_charged_lighter_rows({
                "tail_lines": ["p9 lighter (100/100 butane)"],
            }),
            ["p9 lighter (100/100 butane)"],
        )
        self.assertEqual(
            startup_harness.r008_charged_lighter_rows({
                "tail_lines": ["p9 lighter (0/100 butane)"],
            }),
            [],
        )
        with tempfile.TemporaryDirectory() as directory:
            run_dir = Path(directory)
            original_capture = startup_harness.capture_screenshot
            original_screen_text = startup_harness.capture_screen_text_artifact
            try:
                startup_harness.capture_screenshot = lambda *_args, **_kwargs: {}
                startup_harness.capture_screen_text_artifact = lambda *_args, **_kwargs: {
                    "ok": True,
                    "tail_lines": [
                        "Do you reallu want",
                        "to burn your tirewooo source? lbase sensitıve",
                    ],
                }
                self.assertTrue(
                    startup_harness.r008_firestarter_boundary_snapshot(
                        1, run_dir, "ocr_variant",
                    )["confirmable"],
                )
                startup_harness.capture_screen_text_artifact = lambda *_args, **_kwargs: {
                    "ok": True,
                    "tail_lines": ["Do you want to burn this item?"],
                }
                self.assertFalse(
                    startup_harness.r008_firestarter_boundary_snapshot(
                        1, run_dir, "unrelated_modal",
                    )["confirmable"],
                )
            finally:
                startup_harness.capture_screenshot = original_capture
                startup_harness.capture_screen_text_artifact = original_screen_text

    def test_short_staged_predecessors_are_durable_history_not_active_routes(self) -> None:
        for name in (
            "bandit.roof_fire_horde_player_action_mcw",
            "bandit.roof_fire_horde_nice_roof_fire_mcw",
            "bandit.roof_fire_horde_split_wait_from_player_fire_mcw",
            "bandit.player_lit_fire_signal_wait_mcw",
        ):
            manifest = load_json(HARNESS / "scenarios" / f"{name}.json")
            self.assertEqual(manifest["status"], "blocked")
            self.assertIn("Superseded", manifest["blocked_reason"])


if __name__ == "__main__":
    unittest.main()
