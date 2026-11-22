import hashlib
import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from world_inspect import inspect_world, resolve_world
from world_view import render


HERE = Path(__file__).resolve().parent
HARNESS = HERE / "startup_harness.py"


def write_json(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data), encoding="utf-8")


class InspectionTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.world = self.root / "fixture" / "save" / "camp"
        write_json(self.world / "worldoptions.json", [])
        write_json(self.world / "player.sav", {"player": {"location": [-1, 3, 1]},
                                               "levx": 354, "levy": -5, "om_x": -1, "om_y": 0})
        self.region = {"layers": [[["field", 32400]]] * 21, "npcs": [],
                       "overmap_special_placements": []}
        write_json(self.world / "overmaps/o.-1.0", self.region)
        self.tower = {"site_id": "camp", "anchor": [40, 1, 0], "footprint": [[40, 1, 0]],
                      "site_kind": "cannibal_camp", "living_total": 3, "members": [
                          {"npc_id": "2", "state": "outbound"}, {"npc_id": 3, "state": "outbound"}],
                      "active_outing": {"activity_id": "outing1", "phase": "returning",
                                         "member_ids": [2, 3], "route_position": [0, 0, 0]}}
        write_json(self.world / "dimension_data.gsav",
                   {"overmapbuffer": {"bandit_live_world": {"sites": [self.tower]}}})

    def test_radius_zoom_seam_and_exact_terrain(self):
        report = inspect_world(self.world.parents[1], include_map=True)
        self.assertEqual(report["player_omt"], [-1, 0, 1])
        self.assertEqual(report["bounds"], [-41, -40, 39, 40])
        self.assertEqual(report["local_map_omt"], [-3, -3, 2, 2])
        self.assertEqual(report["saved_tiles"], 41 * 41)
        self.assertEqual(report["terrain_rows"][40][40:42], ["field", None])
        self.assertEqual(report["sites"], [])
        self.assertIn("unknown", render(report))
        zoom = inspect_world(self.world, center=(40, 1), radius=20)
        self.assertEqual(zoom["unknown_tiles"], 41 * 41)
        self.assertFalse(zoom["sites"][0]["anchor_in_ai_range"])
        self.assertEqual(zoom["sites"][0]["outing"]["saved_member_range"], "unknown")

    def test_profile_and_save_containers_ignore_custody_sidecars(self):
        container = self.world.parent
        (container / ".save-continuity").mkdir()
        self.assertEqual(resolve_world(container), self.world.resolve())
        self.assertEqual(resolve_world(container.parent), self.world.resolve())
        write_json(container / "second" / "worldoptions.json", [])
        with self.assertRaisesRegex(ValueError, "Select one world"):
            resolve_world(container)

    def test_party_qualifies_by_living_member_outside_home_view(self):
        self.region["npcs"] = [{"id": 2, "name": "scout", "location": [-24 * 41, 0, 0], "dead": False}]
        write_json(self.world / "overmaps/o.-1.0", self.region)
        row, = inspect_world(self.world)["sites"]
        self.assertFalse(row["anchor_in_ai_range"])
        self.assertEqual(row["outing"]["saved_member_range"], "inside")
        self.assertEqual(row["members"][0]["omt"], [-41, 0, 0])
        self.assertIsNone(row["members"][1]["omt"])
        self.region["npcs"][0]["dead"] = True
        write_json(self.world / "overmaps/o.-1.0", self.region)
        self.assertEqual(inspect_world(self.world)["sites"][0]["outing"]["saved_member_range"], "unknown")

    def test_placements_readable_without_population_data(self):
        (self.world / "dimension_data.gsav").unlink()
        self.region["overmap_special_placements"] = [{"special": "cannibal_camp", "placements": [
            {"points": [{"p": [177, 1, 0]}, {"p": [178, 1, 0]}]}]}]
        write_json(self.world / "overmaps/o.-1.0", self.region)
        report = inspect_world(self.world)
        self.assertIsNone(report["sites"][0]["registered"])
        self.assertEqual(report["sites"][0]["edge_distance_omt"], 1)
        self.assertIn("population unavailable", render(report))

    def test_tower_last_report_and_current_population_position_stay_distinct(self):
        ident = "mi-go-tower:(9,0,0)"
        member = {"identity": {"member": "scout"}, "hp": 20, "current_position": [48, 0, 0]}
        op = {"generation": 3, "resolved": False, "members": [
            {"member_id": "scout", "last_position": [96, 0, 0], "settled": False, "returned": False}]}
        write_json(self.world / "dimension_data.gsav", {"overmapbuffer": {
            "migo_populations": {"sites": [{"id": ident, "anchor": [9, 0, 0], "members": [member]}]},
            "migo_towers": {"sites": [{"site_id": ident, "operation": op}]}}})
        result = inspect_world(self.world)
        site, = result["sites"]
        self.assertEqual(site["members"][0]["omt"], [2, 0, 0])
        self.assertEqual(site["operation"]["members"][0]["last_observed_omt"], [4, 0, 0])
        self.assertIn("open tower operation", render(result))

    def test_horde_footing_preserves_level_and_exact_tile_counts(self):
        self.region["horde_map"] = [[-1, 24, 0], {"location": [-1, 24, 0], "typeid": "mon_zombie"},
                                    [-2, 25, 0], {"location": [-2, 25, 0], "typeid": "mon_zombie"},
                                    [-1, 24, 1], {"location": [-1, 24, 1], "typeid": "mon_zombie"}]
        write_json(self.world / "overmaps/o.-1.0", self.region)
        result = inspect_world(self.world, include_map=True)
        self.assertEqual(result["horde_monster_counts"], {"mon_zombie": 3})
        self.assertEqual(result["horde_tiles"][0], {"omt": [-1, 1, 0], "types": {"mon_zombie": 2}})
        self.assertIn("h horde", render(result))

    def test_player_choice_and_invalid_archives(self):
        write_json(self.world / "other.sav", {"player": {"location": [0, 0, 0]}})
        with self.assertRaisesRegex(ValueError, "Choose --player-save"):
            inspect_world(self.world)
        self.assertEqual(inspect_world(self.world, player_save="other.sav")["player_omt"], [0, 0, 0])
        with self.assertRaisesRegex(ValueError, "local"):
            inspect_world(self.world, player_save="../other.sav")
        self.region["layers"][10] = [["field", 999999999]]
        write_json(self.world / "overmaps/o.-1.0", self.region)
        with self.assertRaisesRegex(ValueError, "Invalid saved terrain"):
            inspect_world(self.world, player_save="player.sav")
        with self.assertRaisesRegex(ValueError, "materialized"):
            resolve_world(self.root)

    def test_cli_and_real_compressed_fixture(self):
        fixture = HERE / "fixtures/saves/live-debug/r050_migo_tower_remote_initial_v1"
        if not (fixture / "save").is_dir():
            self.skipTest("R050 materialized fixture unavailable")
        if not (HERE.parents[1] / ("zzip.exe" if os.name == "nt" else "zzip")).exists():
            self.skipTest("native save helper unavailable")
        world = resolve_world(fixture)
        paths = [*world.glob("*.sav*"), *world.glob("o.*"), *(world / "overmaps").iterdir()]
        def fingerprints():
            return {p.name: hashlib.sha256(p.read_bytes()).digest() for p in paths}
        before = fingerprints()
        result = subprocess.run([sys.executable, str(HARNESS), "world", "inspect", str(fixture), "--json"],
                                capture_output=True, check=True)
        report = json.loads(result.stdout)
        self.assertEqual((report["saved_tiles"], report["unknown_tiles"]), (5325, 1236))
        homes = {site["faction"]: site for site in report["sites"]}
        self.assertEqual(len(homes["mi-go"]["members"]), 8)
        self.assertFalse(homes["bandit"]["registered"])
        self.assertNotIn("cannibal", homes)
        zoom = subprocess.run([sys.executable, str(HARNESS), "world", "inspect", str(fixture), "--zoom", "--map",
                               "--json"], capture_output=True, check=True)
        report = json.loads(zoom.stdout)
        self.assertEqual((report["saved_tiles"], report["unknown_tiles"]), (1681, 0))
        self.assertEqual(report["terrain_rows"][20 - 15][20], "mi-go_scout_tower_1_north")
        self.assertEqual(before, fingerprints())
        self.assertFalse(list(world.glob("*.sav")))
        self.assertFalse(list(world.glob("o.*")))


if __name__ == "__main__":
    unittest.main()
