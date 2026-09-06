#!/usr/bin/env python3
"""Regression contract for R-029's post-relaunch native wait route."""

from __future__ import annotations

import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path


HARNESS = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS))

import startup_harness  # noqa: E402


class R029NaturalRouteRecipeTest(unittest.TestCase):
    def test_post_relaunch_fire_audit_tracks_the_guarded_north_retreat(self) -> None:
        scenario = json.loads((Path(__file__).resolve().parent / "scenarios" /
                               "cannibal.r029_natural_route_roof_mcw.json").read_text())
        persisted_fire_audit = scenario["post_relaunch"]["steps"][0]
        self.assertEqual(persisted_fire_audit["label"], "audit_persisted_r029_player_fire")
        # The brazier is deployed southeast (+1,+1); after the exact two-tile
        # north retreat it is relative to the saved player at (+1,+3).
        self.assertEqual(persisted_fire_audit["offsets"], [[1, 3, 0]])

    def test_post_relaunch_recipe_uses_the_advertised_duration_primitive(self) -> None:
        scenario = json.loads((Path(__file__).resolve().parent / "scenarios" /
                               "cannibal.r029_natural_route_roof_mcw.json").read_text())
        post_session = scenario["post_relaunch"]["steps"][1]
        self.assertEqual(post_session["kind"], "cockpit_live_session")
        self.assertEqual(post_session["keep_watch"]["recipe"], ["world.wait", "wait.1m"])
        self.assertEqual(post_session["keep_watch"]["target_delta_game_minutes"], 9)
        self.assertEqual(post_session["keep_watch"]["bound"]["maximum"], 9)

    def test_in_range_fixture_is_zero_credit_and_noncolliding(self) -> None:
        fixture = json.loads((HARNESS / "fixtures" / "saves" / "live-debug" /
                              "r029_fire_signal_roof_cannibal_in_range_v2" /
                              "manifest.json").read_text())
        self.assertEqual(fixture["source_fixture"], "r008_fire_signal_roof_bandit_v1")
        self.assertEqual([item["kind"] for item in fixture["save_transforms"]], [
            "player_mutations", "seed_overmap_special_near_player",
        ])
        seed = fixture["save_transforms"][1]
        self.assertEqual(seed["special_id"], "cannibal_camp")
        self.assertEqual(seed["offset_omt"], [0, -3, -1])
        eligibility = startup_harness.natural_ecology_fixture_eligibility(
            fixture["name"], "live-debug",
        )
        self.assertEqual(eligibility["status"], "eligible")

    def test_in_range_fixture_installs_the_declared_noncolliding_geometry(self) -> None:
        fixture_name = "r029_fire_signal_roof_cannibal_in_range_v2"
        resolved = startup_harness.resolve_fixture_payload(fixture_name, "live-debug")
        with tempfile.TemporaryDirectory() as directory:
            source_world = next(path for path in resolved["save_src"].iterdir()
                                if path.is_dir())
            world = Path(directory) / source_world.name
            shutil.copytree(source_world, world)
            receipts = startup_harness.apply_fixture_save_transforms(
                world, list(resolved["save_transforms"]),
            )
            seed = next(item for item in receipts
                        if item["kind"] == "seed_overmap_special_near_player")
            self.assertEqual(seed["player_abs_omt"], [140, 41, 1])
            self.assertEqual(seed["target_abs_omt"], [140, 38, 0])
            self.assertEqual(seed["approx_range_omt"], 3)
            placements = startup_harness.load_bandit_special_placements(
                HARNESS.parents[1], world, {"cannibal_camp"},
            )
            self.assertTrue(any(
                placement.special_id == "cannibal_camp" and (140, 38, 0) in placement.points
                for placement in placements
            ))

    def test_bandit_sound_route_requires_post_materialization_observer_binding(self) -> None:
        scenario = json.loads((HARNESS / "scenarios" /
                               "bandit.r029_natural_sound_route_mcw.json").read_text())
        session = scenario["steps"][-1]
        self.assertEqual(session["kind"], "cockpit_live_session")
        self.assertIn("zero-credit structural-maintenance pass", session["objective"])
        self.assertIn("nearer site concrete at-home member/readiness after structural materialization",
                      session["proof_targets"])
        self.assertIn("ordinary structural roster materialization are zero-credit",
                      session["invariants"][0])

    def test_bandit_sound_fixture_clones_only_the_declared_zero_credit_observer(self) -> None:
        fixture = json.loads((HARNESS / "fixtures" / "saves" / "live-debug" /
                              "r029_bandit_gunfire_in_range_v1" / "manifest.json").read_text())
        observer = fixture["save_transforms"][-1]
        self.assertEqual(observer["kind"], "overmap_npcs_near_player")
        self.assertEqual(observer["source_npc_id"], 4)
        self.assertEqual(observer["offsets_ms"], [[0, -24, -1]])
        self.assertEqual(observer["npc_faction"], "hells_raiders")


if __name__ == "__main__":
    unittest.main()
