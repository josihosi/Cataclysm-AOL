#!/usr/bin/env python3
"""R-020 controlled-camp transaction contract."""

from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

from controlled_camp_setup import ControlledCampSetupError, run_controlled_camp_setup  # noqa: E402


class ControlledCampSetupTest(unittest.TestCase):
    @staticmethod
    def fake_zzip(path: Path) -> None:
        if path.suffix == ".zzip":
            path.with_suffix("").write_bytes(path.read_bytes())
        else:
            path.with_suffix(f"{path.suffix}.zzip").write_bytes(path.read_bytes())

    def test_disposable_exact_camp_is_receipted_and_restored(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            world = Path(directory) / "Disposable"
            (world / "overmaps").mkdir(parents=True)
            player = world / "player.sav.zzip"
            player.write_text(json.dumps({"player": {"location": [48, 72, 0], "camps": []}}), encoding="utf-8")
            overmap = world / "overmaps" / "o.0.0.zzip"
            overmap.write_bytes(b"overmap-before")
            original = {player: player.read_bytes(), overmap: overmap.read_bytes()}
            payload = {"camps": []}

            def extract(_path: Path):
                plain = overmap.with_suffix("")
                plain.write_text("plain", encoding="utf-8")
                return plain, "# version 1", payload

            def write(_plain: Path, _version: str, _payload: dict) -> None:
                overmap.write_bytes(json.dumps(_payload, sort_keys=True).encode())

            with (
                mock.patch("startup_harness.run_zzip", side_effect=self.fake_zzip),
                mock.patch("controlled_camp_setup.run_zzip", side_effect=self.fake_zzip),
                mock.patch("startup_harness.extract_overmap_payload", side_effect=extract),
                mock.patch("controlled_camp_setup.extract_overmap_payload", side_effect=extract),
                mock.patch("startup_harness.write_overmap_payload", side_effect=write),
                mock.patch("startup_harness.cleanup_extracted_overmap"),
                mock.patch("controlled_camp_setup.cleanup_extracted_overmap"),
            ):
                receipt = run_controlled_camp_setup(world, {"player_save": player.name, "camp_name": "R020 Camp", "owner": "your_followers", "camp_omt": [2, 3, 0]})

            self.assertEqual(receipt["status"], "cleaned")
            self.assertTrue(receipt["after_facts"]["invariant_satisfied"])
            self.assertTrue(receipt["cleanup_receipt"]["accepted"])
            self.assertEqual(receipt["evidence_effect"], "none_for_manufactured_state")
            self.assertFalse(receipt["gameplay_credit"])
            self.assertEqual({player: player.read_bytes(), overmap: overmap.read_bytes()}, original)

    def test_occupied_or_wrong_coordinate_fails_before_mutation(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            world = Path(directory) / "Disposable"
            (world / "overmaps").mkdir(parents=True)
            player = world / "player.sav.zzip"
            player.write_text(json.dumps({"player": {"location": [48, 72, 0], "camps": [{"pos": [2, 3, 0]}]}}), encoding="utf-8")
            overmap = world / "overmaps" / "o.0.0.zzip"
            overmap.write_bytes(b"before")
            payload = {"camps": []}

            def extract(_path: Path):
                plain = overmap.with_suffix("")
                plain.write_text("plain", encoding="utf-8")
                return plain, "# version 1", payload

            with (
                mock.patch("controlled_camp_setup.run_zzip", side_effect=self.fake_zzip),
                mock.patch("controlled_camp_setup.extract_overmap_payload", side_effect=extract),
                mock.patch("controlled_camp_setup.cleanup_extracted_overmap"),
            ):
                with self.assertRaisesRegex(ControlledCampSetupError, "unsafe placement"):
                    run_controlled_camp_setup(world, {"player_save": player.name, "camp_name": "R020 Camp", "owner": "your_followers", "camp_omt": [2, 3, 0]})

    def test_partial_identity_drift_and_undeclared_mutation_are_rejected_and_restored(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            world = Path(directory) / "Disposable"
            (world / "overmaps").mkdir(parents=True)
            player = world / "player.sav.zzip"
            player.write_text(json.dumps({"player": {"location": [48, 72, 0], "camps": []}}), encoding="utf-8")
            overmap = world / "overmaps" / "o.0.0.zzip"
            overmap.write_bytes(b"before")
            original = {player: player.read_bytes(), overmap: overmap.read_bytes()}
            payload = {"camps": []}

            def extract(_path: Path):
                plain = overmap.with_suffix("")
                plain.write_text("plain", encoding="utf-8")
                return plain, "# version 1", payload

            def partial_owner(_world: Path, _transform: dict) -> dict:
                self.fake_zzip(player)
                extracted = player.with_suffix("")
                saved = json.loads(extracted.read_text(encoding="utf-8"))
                saved["player"]["camps"].append({"pos": [2, 3, 0], "drift": "unexpected"})
                saved["player"]["undeclared_mutation"] = True
                extracted.write_text(json.dumps(saved), encoding="utf-8")
                self.fake_zzip(extracted)
                return {"player_registry_present": False, "camp_added": True}

            with (
                mock.patch("controlled_camp_setup.run_zzip", side_effect=self.fake_zzip),
                mock.patch("controlled_camp_setup.extract_overmap_payload", side_effect=extract),
                mock.patch("controlled_camp_setup.cleanup_extracted_overmap"),
                mock.patch("controlled_camp_setup.apply_player_basecamp_at_omt_transform", side_effect=partial_owner),
            ):
                receipt = run_controlled_camp_setup(world, {"player_save": player.name, "camp_name": "R020 Camp", "owner": "your_followers", "camp_omt": [2, 3, 0]})

            self.assertEqual(receipt["status"], "failed_invariant_cleaned")
            self.assertFalse(receipt["native_receipt"]["accepted"])
            self.assertFalse(receipt["after_facts"]["declared_mutation_only"])
            self.assertTrue(receipt["cleanup_receipt"]["accepted"])
            self.assertEqual({player: player.read_bytes(), overmap: overmap.read_bytes()}, original)


if __name__ == "__main__":
    unittest.main()
