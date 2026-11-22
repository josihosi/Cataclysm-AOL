"""Exercise disposable world preparation with the bundled, native R050 save."""
from __future__ import annotations

import json
import shutil
import tempfile
import unittest
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from unittest.mock import patch

import startup_harness as h
import world_prepare as wp


FIXTURES = h.repo_root() / "tools/openclaw_harness/fixtures/saves/live-debug"
SOURCE = FIXTURES / "r050_migo_tower_remote_initial_v1"
WORLD = SOURCE / "save/TestSetup00"
PLAYER = "#VGVzdDAw.sav.zzip"


class LocationsTest(unittest.TestCase):
    def test_absolute_and_forward_relative_and_xy_z(self):
        found, resolve = wp._locations({"approach": {"from": "camp", "offset_omt": [1, 0, 0]},
                                       "camp": {"from": "player", "offset_omt": [20, -9, -1]}}, (-3, 5, 1))
        self.assertEqual(found["approach"], (18, -4, 0))
        self.assertEqual(resolve([-1, 2, -3]), (-1, 2, -3))

    def test_reject_circular_unknown_reserved_and_malformed_location(self):
        for definitions in ({"player": [0, 0, 0]}, {"camp": "absent"}, {"camp": "camp"},
                            {"camp": {"from": "a", "offset_omt": [2, 2, 0]}, "a": "camp"},
                            {"camp": [0, True, 0]}, {"camp": {"from": "player", "offset": [1, 0, 0]}}):
            with self.subTest(definitions=definitions), self.assertRaises(ValueError):
                wp._locations(definitions, (0, 0, 0))


class PrepareTest(unittest.TestCase):
    def setUp(self):
        try:
            h.zzip_binary()
        except SystemExit:
            self.skipTest("native zzip save helper unavailable")
        self.tmp = tempfile.TemporaryDirectory(prefix="world-build-test-")
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.out = self.root / "new"

    def test_saved_geometry_supplies_and_required_world_facts(self):
        before = wp._hash(WORLD)
        recipe = {
            "name": "Mixed homes with travel and brazier supplies",
            "locations": {"camp": {"from": "player", "offset_omt": [20, 0, -1]}},
            "specials": [{"id": "cannibal_camp", "at": "camp"}],
            "terrain": [{"id": "forest", "at": [160, 149, 0], "to": [164, 149, 0]}],
            "items": [{"id": "brazier"}, {"id": "lighter", "charges": 50},
                      {"id": "log", "count": 2}, {"id": "log", "count": 1}],
            "requires": [{"staffed": "cannibal_camp", "at": "camp"},
                         {"staffed": "Mi-Go Scout Tower", "at": [145, 134, 0]},
                         {"staffed": "bandit_cabin", "at": [103, 167, 0]},
                         {"field": "fd_fire", "present": False},
                         {"item": "lighter", "active": False},
                         {"item": "missing_lamp", "active": False},
                         {"field": "fd_fire", "present": False, "offset_ms": [2400, 0, 0]}]}
        r = wp.prepare_world(SOURCE, recipe, self.out)
        self.assertEqual(r, json.loads((self.out / "preparation.json").read_bytes()))
        self.assertEqual(wp._hash(WORLD), before)
        built = Path(r["world_dir"])
        self.assertNotEqual(wp._hash(built), before)
        self.assertFalse(r["requirements_met"])
        self.assertEqual([a["status"] for a in r["requirements"]],
                         ["unmet", "met", "unknown", "met", "met", "unmet", "unknown"])
        self.assertEqual(r["locations"]["camp"], [165, 149, 0])
        self.assertEqual(len(r["geometry"][1]["footprint_omt"]), 8)
        self.assertEqual(h.saved_overmap_terrain_id(built, (166, 150, 1)), "cannibal_camp_4_roof_north")
        plain, _, saved_map = h.extract_overmap_payload(built / "overmaps/o.0.0.zzip")
        try:
            footprint = r["geometry"][1]["footprint_omt"]
            slots = [v for p, v in saved_map["mapgen_arg_index"] if p in footprint]
            self.assertEqual(len(slots), len(footprint))
            self.assertEqual(len(set(slots)), 1)
            self.assertIsNone(saved_map["mapgen_arg_storage"][slots[0]])
        finally:
            h.cleanup_extracted_overmap(plain, keep=False)
        self.assertEqual([h.saved_overmap_terrain_id(built, (x, 149, 0)) for x in range(160, 165)],
                         ["forest"] * 5)
        self.assertEqual({item["id"]: item["added"] for item in r["items"]},
                         {"log": 3, "lighter": 1, "brazier": 1})
        # Prove charges survived native zzip repacking, separately from the recipe.
        _, pos = h.load_player_abs_omt(built, r["player_save"])
        _, _, _, _, _, submap, _, _, local, _ = h.map_submap_for_ms_location(built, pos)
        stacks = [items for x, y, items in h.iter_map_triples(submap["items"]) if (x, y) == local[:2]]
        self.assertEqual(len(stacks), 1, "the native reader keeps only the final x,y group")
        self.assertEqual([i["charges"] for i in stacks[0] if i.get("typeid") == "lighter"], [50])
        self.assertEqual(sum(i.get("typeid") == "log" for i in stacks[0]), 3)

    def test_old_duplicate_groups_preserve_only_native_visible_stack(self):
        r = wp.prepare_world(WORLD, {}, self.out)
        world = Path(r["world_dir"])
        _, location = h.load_player_abs_omt(world, PLAYER)
        _, pack, _, path, data, submap, _, _, local, _ = h.map_submap_for_ms_location(world, location)
        x, y = local[:2]
        submap["items"] = [x, y, [{"typeid": "brazier"}], x, y, [[{"typeid": "log"}, 3]]]
        submap["fields"] = [x, y, ["fd_smoke", 1, 0, {}, "fd_fire", 1, 2, {}]]
        path.write_text(json.dumps(data))
        h.run_zzip(path)
        shutil.rmtree(pack)
        before = wp._item_counts(wp._tile_samples(world, PLAYER, {(0, 0, 0)}))
        self.assertEqual(before, {((0, 0, 0), "log"): 3})
        checks = wp._requirements(world, PLAYER, [{"field": "fd_fire", "present": False}], None)
        self.assertEqual(checks[0]["status"], "unmet")
        wp._items(world, [{"id": "log"}, {"id": "lighter", "charges": 4}], PLAYER)
        after = wp._item_counts(wp._tile_samples(world, PLAYER, {(0, 0, 0)}))
        self.assertEqual(after, {((0, 0, 0), "log"): 4, ((0, 0, 0), "lighter"): 1})

    def test_recheck_uses_original_absolute_positions_after_avatar_moves(self):
        recipe = {"locations": {"tower": {"from": "player", "offset_omt": [0, -15, -1]}},
                  "items": [{"id": "lighter", "charges": 3}],
                  "requires": [{"item": "lighter", "active": False}, {"staffed": "Mi-Go Scout Tower", "at": "tower"}]}
        r = wp.prepare_world(SOURCE, recipe, self.out)
        world = Path(r["world_dir"])
        transform = {"player_save": PLAYER, "special_id": "bandit_camp", "site_index": 1,
                     "offset_omt": [16, 0, 0]}
        h.apply_player_near_overmap_special_transform(world, transform)
        omt, _ = h.load_player_abs_omt(world, PLAYER)
        self.assertNotEqual(list(omt), r["locations"]["player"])
        before = wp._hash(world)
        checked = wp.verify_requirements(world, r)
        self.assertTrue(checked["requirements_met"], checked)
        self.assertEqual(wp._hash(world), before)

    def test_fixture_without_payload_applies_inherited_transforms(self):
        source = FIXTURES / "r050_rural_bandit_remote_v1"
        self.assertFalse((source / "save").is_dir())
        r = wp.prepare_world(source, {}, self.out, player_save=PLAYER)
        self.assertEqual(r["locations"]["player"], [145, 149, 0])
        self.assertTrue(r["requirements_met"])

    def test_unmodified_closed_world_and_prepared_source_copy_byte_for_byte(self):
        native = wp.prepare_world(WORLD, {}, self.out)
        self.assertEqual(native["world_sha256"], native["source_sha256"])
        clone = wp.prepare_world(self.out, {}, self.root / "again")
        self.assertEqual(clone["world_sha256"], native["world_sha256"])
        self.assertEqual(clone["source"], str(self.out.resolve()))

    def test_bad_edits_leave_no_artifact_and_preserve_source(self):
        before = wp._hash(WORLD)
        failures = [
            ({"specials": [{"id": "cannibal_camp", "at": [145, 134, 0]}]}, "belongs to special"),
            ({"specials": [{"id": "cannibal_camp", "at": [145, 149, 0]}]}, "generated local terrain"),
            ({"specials": [{"id": "cannibal_camp", "at": [179, 100, 0]}]}, "seam"),
            ({"terrain": [{"id": "bogus", "at": [160, 149, 0]}]}, "unknown terrain"),
            ({"terrain": [{"id": "forest", "at": [185, 149, 0]}]}, "unsaved"),
            ({"specials": [{"id": "not_a_special", "at": [160, 149, 0]}]}, "unsupported"),
            ({"terrain": [{"id": "forest", "at": [165, 149, 0]}],
              "specials": [{"id": "cannibal_camp", "at": [165, 149, 0]}]}, "overlapping"),
            ({"items": [{"id": "not_a_real_named_item"}]}, "unknown item"),
            ({"requires": [{"field": "fd_fire", "present": 1}]}, "true or false"),
        ]
        for recipe, reason in failures:
            with self.subTest(recipe=recipe), self.assertRaisesRegex(ValueError, reason):
                wp.prepare_world(WORLD, recipe, self.out)
            self.assertEqual(list(self.root.iterdir()), [])
        self.assertEqual(wp._hash(WORLD), before)

    def test_destination_and_player_selection(self):
        with self.assertRaisesRegex(ValueError, "outside the source"):
            wp.prepare_world(WORLD, {}, WORLD / "bad")
        with self.assertRaisesRegex(ValueError, "filename"):
            wp.prepare_world(WORLD, {}, self.out, player_save="../bad.sav.zzip")
        self.out.mkdir()
        keep = self.out / "other-owner"
        keep.write_text("keep")
        with self.assertRaises(FileExistsError):
            wp.prepare_world(WORLD, {}, self.out)
        self.assertEqual(keep.read_text(), "keep")

    def test_native_terrain_variants_absent_from_the_saved_overmap(self):
        vocabulary = wp._defined_terrain(WORLD).keys()
        self.assertTrue({"forest", "open_air", "cannibal_camp_1_south", "road_nesw", "road_end_east"} <= vocabulary)
        self.assertFalse({"forest_north", "road_north", "cannibal_camp_1", "made_up_north"} & vocabulary)
        built = wp.prepare_world(WORLD, {"terrain": [{"id": "cannibal_camp_1_south", "at": [160, 149, 0]}]},
                                 self.out)
        self.assertEqual(h.saved_overmap_terrain_id(Path(built["world_dir"]), (160, 149, 0)), "cannibal_camp_1_south")

    def test_refuse_unsuitable_home_terrain_and_reuse_matching_saved_home(self):
        at = [165, 149, 0]
        camp = {"specials": [{"id": "cannibal_camp", "at": at}]}
        water = wp.prepare_world(WORLD, {"terrain": [{"id": "river_center", "at": at}]}, self.out)
        with self.assertRaisesRegex(ValueError, "unsuitable terrain"):
            wp.prepare_world(Path(water["world_dir"]), camp, self.root / "bad")
        first = wp.prepare_world(WORLD, camp, self.root / "land")
        same = wp.prepare_world(Path(first["world_dir"]), camp, self.root / "reuse")
        self.assertEqual(first["world_sha256"], same["world_sha256"])

    def test_changed_source_and_failed_repack_do_not_publish(self):
        with patch.object(wp, "_hash", side_effect=["before", "before", "changed"]):
            with self.assertRaisesRegex(ValueError, "source changed"):
                wp.prepare_world(WORLD, {}, self.out)
        with patch.object(h, "write_overmap_payload", side_effect=SystemExit("repack failed")):
            with self.assertRaisesRegex(SystemExit, "repack failed"):
                wp.prepare_world(WORLD, {"terrain": [{"id": "forest", "at": [165, 149, 0]}]}, self.out)
        self.assertFalse(self.out.exists())
        self.assertEqual(list(self.root.iterdir()), [])

    def test_source_marked_live_is_refused(self):
        source = self.root / "run/save/Test"
        source.mkdir(parents=True)
        for name in ("worldoptions.json", PLAYER):
            shutil.copyfile(WORLD / name, source / name)
        (source.parent.parent / ".harness-owner.lock").write_text('{"run_id":"live","launcher_state":"running"}')
        with self.assertRaisesRegex(RuntimeError, "owner is not closed"):
            wp.prepare_world(source, {}, self.out)
        self.assertFalse(self.out.exists())

    def test_two_builders_cannot_replace_each_other(self):
        def build(label):
            try:
                return wp.prepare_world(WORLD, {"name": label}, self.out)["recipe"]["name"]
            except FileExistsError:
                return None
        with ThreadPoolExecutor(max_workers=2) as workers:
            results = list(workers.map(build, ["one", "two"]))
        self.assertEqual(results.count(None), 1)
        stored = json.loads((self.out / "preparation.json").read_bytes())
        self.assertIn(stored["recipe"]["name"], results)


if __name__ == "__main__":
    unittest.main()
