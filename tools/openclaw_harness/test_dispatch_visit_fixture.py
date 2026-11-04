import json
import shutil
import tempfile
import unittest
from pathlib import Path

from startup_harness import (
    apply_fixture_save_transforms,
    cleanup_extracted_overmap,
    extract_overmap_payload,
    normalize_fixture_save_transforms,
    write_overmap_payload,
)


class DispatchFixtureHomeBodiesTest(unittest.TestCase):
    def make_world(self, root: Path, bodies: list) -> Path:
        overmaps = root / "overmaps"
        overmaps.mkdir()
        root.joinpath("worldoptions.json").write_text("[]")
        shutil.copyfile(Path(__file__).resolve().parents[2] / "data/raw/compression/overmaps.dict",
                        root / "overmaps.dict")
        write_overmap_payload(root / "o.0.0", "# version 39", {"npcs": bodies})
        root.joinpath("dimension_data.gsav").write_text("# version 39\n" + json.dumps({
            "overmapbuffer": {"bandit_live_world": {"sites": [
                {"site_id": "old", "members": []},
                {"site_id": "camp", "anchor": [140, 51, 0],
                 "footprint": [[140, 51, 0]], "members": [
                     {"npc_id": 4, "state": "local_contact"},
                     {"npc_id": 5, "state": "at_home"},
                     {"npc_id": 6, "state": "at_home"},
                 ]},
            ]}},
        }))
        return root

    @staticmethod
    def body(npc_id: int, x: int, faction: str = "hells_raiders") -> dict:
        return {"id": npc_id, "location": [x, 1238, 0], "my_fac": faction,
                "name": f"NPC {npc_id}", "inventory": ["preserve"]}

    @staticmethod
    def transform(root: Path) -> list:
        return normalize_fixture_save_transforms([{
            "kind": "bandit_site_roster_shape", "player_save": "player.sav.zzip",
            "site_id": "camp", "living_member_count": 2, "member_start_index": 1,
            "home_body_faction": "cannibal_camp", "expected_home_body_faction": "hells_raiders",
        }], manifest_path=root / "manifest.json")

    def test_existing_home_bodies_and_roster_are_aligned_without_moving_others(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = self.make_world(Path(directory), [self.body(4, 3100), self.body(5, 3374), self.body(6, 3369)])
            result = apply_fixture_save_transforms(root, self.transform(root))
            plain, _, overmap = extract_overmap_payload(root / "overmaps/o.0.0.zzip")
            try:
                bodies = {body["id"]: body for body in overmap["npcs"]}
            finally:
                cleanup_extracted_overmap(plain, keep=False)
            dimension = json.loads(root.joinpath("dimension_data.gsav").read_text().split("\n", 1)[1])
            members = dimension["overmapbuffer"]["bandit_live_world"]["sites"][1]["members"]
            self.assertEqual([member["npc_id"] for member in members], [5, 6])
            self.assertEqual(result[0]["home_body_setup"]["npc_ids"], [5, 6])
            self.assertEqual(bodies[4], self.body(4, 3100))
            self.assertEqual(bodies[5], self.body(5, 3374, "cannibal_camp"))
            self.assertEqual(bodies[6], self.body(6, 3369, "cannibal_camp"))

    def test_missing_duplicate_or_away_body_refuses_before_any_write(self) -> None:
        cases = [
            [self.body(5, 3374)],
            [self.body(5, 3374), self.body(5, 3374), self.body(6, 3369)],
            [self.body(5, 3100), self.body(6, 3369)],
            [self.body(5, 3374, "other"), self.body(6, 3369)],
        ]
        for bodies in cases:
            with self.subTest(bodies=bodies), tempfile.TemporaryDirectory() as directory:
                root = self.make_world(Path(directory), bodies)
                dimension = root.joinpath("dimension_data.gsav").read_bytes()
                archive = root.joinpath("overmaps/o.0.0.zzip").read_bytes()
                with self.assertRaises(SystemExit):
                    apply_fixture_save_transforms(root, self.transform(root))
                self.assertEqual(root.joinpath("dimension_data.gsav").read_bytes(), dimension)
                self.assertEqual(root.joinpath("overmaps/o.0.0.zzip").read_bytes(), archive)

    def test_active_outside_cannot_be_relabelled_as_home_setup(self) -> None:
        with self.assertRaises(SystemExit):
            normalize_fixture_save_transforms([{
                "kind": "bandit_site_roster_shape", "player_save": "player.sav.zzip",
                "site_id": "camp", "living_member_count": 1, "active_outside_member_count": 1,
                "home_body_faction": "cannibal_camp", "expected_home_body_faction": "hells_raiders",
            }], manifest_path=Path("manifest.json"))

    def test_other_site_ownership_is_not_stolen(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = self.make_world(Path(directory), [self.body(5, 3374), self.body(6, 3369)])
            path = root / "dimension_data.gsav"
            header, payload = path.read_text().split("\n", 1)
            state = json.loads(payload)
            state["overmapbuffer"]["bandit_live_world"]["sites"][0]["members"] = [{"npc_id": 5}]
            path.write_text(header + "\n" + json.dumps(state))
            before = path.read_bytes()
            archive = root.joinpath("overmaps/o.0.0.zzip").read_bytes()
            with self.assertRaisesRegex(SystemExit, "owned by another site"):
                apply_fixture_save_transforms(root, self.transform(root))
            self.assertEqual(path.read_bytes(), before)
            self.assertEqual(root.joinpath("overmaps/o.0.0.zzip").read_bytes(), archive)


if __name__ == "__main__":
    unittest.main()
