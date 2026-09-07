#!/usr/bin/env python3
"""Focused guardrails for R-033's zero-credit physical-return staging fixture."""

from __future__ import annotations

import copy
import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path


HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

from startup_harness import (  # noqa: E402
    apply_fixture_save_transforms,
    audit_saved_bandit_live_world_state,
    normalize_fixture_save_transforms,
)


FIXTURE_ROOT = HARNESS_DIR / "fixtures" / "saves" / "live-debug"
SOURCE = FIXTURE_ROOT / "bandit_phase4_ecology_dispatch_observer_v0_2026-08-05" / "save" / "McWilliams"
MANIFEST = FIXTURE_ROOT / "r033_phase4_owner_first_delayed_c4_v1" / "manifest.json"
ABSTRACT_MANIFEST = FIXTURE_ROOT / "r033_phase4_abstract_armed_c4_v1" / "manifest.json"
SITE_ID = "overmap_special:bandit_camp@140,51,0"
ACTIVITY_ID = SITE_ID + "#structural"


def load_dimension(world: Path) -> dict:
    return json.loads((world / "dimension_data.gsav").read_text(encoding="utf-8").split("\n", 1)[1])


def selected_outing(payload: dict) -> dict:
    sites = payload["overmapbuffer"]["bandit_live_world"]["sites"]
    site = next(site for site in sites if site.get("site_id") == SITE_ID)
    return site["active_outing"]


class R033OwnerFirstHomewardFixtureTest(unittest.TestCase):
    def transform(self) -> dict:
        manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
        transform = next(item for item in manifest["save_transforms"]
                         if item["kind"] == "bandit_structural_homeward_reentry")
        return normalize_fixture_save_transforms([transform], manifest_path=MANIFEST)[0]

    def test_transform_is_narrow_and_stages_only_native_return_preconditions(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            world = Path(directory) / "McWilliams"
            shutil.copytree(SOURCE, world)
            before = load_dimension(world)
            before_outing = copy.deepcopy(selected_outing(before))
            receipt = apply_fixture_save_transforms(world, [self.transform()])[0]
            after = load_dimension(world)
            after_outing = selected_outing(after)

            self.assertEqual(after_outing["phase"], "returning_home")
            self.assertEqual(after_outing["local_handoff"]["phase"], "returning_home")
            self.assertEqual(after_outing["member_ids"], [4, 5])
            self.assertEqual(after_outing["generation"], 1)
            self.assertEqual(after_outing["simulation_owner"], "local")
            self.assertEqual(after_outing["handoff_epoch"], 1)
            self.assertFalse(after_outing.get("member_return_receipts", []))
            self.assertEqual(after_outing.get("resolved_member_ids", []), [])
            self.assertEqual(after_outing.get("casualty_ids", []), [])
            self.assertEqual(after_outing.get("observations", []), before_outing.get("observations", []))
            self.assertEqual(after_outing.get("report_application_key"), before_outing.get("report_application_key"))
            self.assertEqual(after_outing.get("return_application_key"), before_outing.get("return_application_key"))
            self.assertTrue(all(receipt["invariant_checks"].values()))

            audit = audit_saved_bandit_live_world_state(
                world, required_site_id_contains=SITE_ID,
                required_active_outing_kind="structural_sortie",
                required_active_outing_generation=1,
                required_active_outing_simulation_owner="local",
                required_active_outing_handoff_epoch=1,
                required_active_outing_phase="returning_home",
                required_active_outing_exact_pair=True,
                required_local_handoff_exact_pair=True,
                required_active_members_found=True,
                required_active_member_abs_omt=[140, 51, 0],
            )
            self.assertEqual(audit["status"], "required_state_present")
            site = audit["matching_sites"][0]
            self.assertTrue(site["active_members_at_required_abs_omt"])

    def test_transform_rejects_an_attempt_to_pregrant_abstract_ownership(self) -> None:
        unsafe = self.transform()
        unsafe["generation"] = 2
        with self.assertRaisesRegex(SystemExit, "canonical local observing pair"):
            with tempfile.TemporaryDirectory() as directory:
                world = Path(directory) / "McWilliams"
                shutil.copytree(SOURCE, world)
                apply_fixture_save_transforms(world, [unsafe])

    def test_post_handoff_bootstrap_is_bound_to_the_native_capture_and_has_no_new_lead(self) -> None:
        manifest = json.loads(ABSTRACT_MANIFEST.read_text(encoding="utf-8"))
        raw = next(item for item in manifest["save_transforms"]
                   if item["kind"] == "bandit_post_handoff_abstract_bootstrap")
        transform = normalize_fixture_save_transforms([raw], manifest_path=ABSTRACT_MANIFEST)[0]
        with tempfile.TemporaryDirectory() as directory:
            world = Path(directory) / "McWilliams"
            shutil.copytree(SOURCE, world)
            before = load_dimension(world)
            before_site = next(site for site in before["overmapbuffer"]["bandit_live_world"]["sites"]
                               if site.get("site_id") == SITE_ID)
            receipt = apply_fixture_save_transforms(world, [transform])[0]
            after = load_dimension(world)
            after_site = next(site for site in after["overmapbuffer"]["bandit_live_world"]["sites"]
                              if site.get("site_id") == SITE_ID)
            outing = after_site["active_outing"]

            self.assertEqual(outing["simulation_owner"], "abstract")
            self.assertEqual(outing["generation"], 1)
            self.assertEqual(outing["member_ids"], [4, 5])
            self.assertEqual(outing["handoff_epoch"], 2)
            self.assertEqual(outing["last_advanced_minutes"], 8284)
            self.assertEqual(outing["resolved_member_ids"], [4, 5])
            self.assertEqual([row["returned_minutes"] for row in outing["member_return_receipts"]], [8284, 8284])
            self.assertEqual(outing["local_handoff"]["members"], [])
            self.assertEqual(outing["observations"], [])
            returned_members = [member for member in after_site["members"]
                                if member.get("npc_id") in {4, 5}]
            self.assertEqual([member["state"] for member in returned_members], ["at_home", "at_home"])
            self.assertEqual([member["last_writeback_summary"] for member in returned_members],
                             ["physical structural return receipt committed"] * 2)
            self.assertEqual(after_site["intelligence_map"]["leads"], before_site["intelligence_map"]["leads"])
            self.assertEqual(receipt["native_handoff_capture"]["run_id"],
                             "6158dd4f735779ccb2a4af5982c2bd80a356c50713add68c86db716b770ac05f")
            self.assertTrue(all(receipt["invariant_checks"].values()))

    def test_post_handoff_bootstrap_rejects_an_unbound_capture(self) -> None:
        manifest = json.loads(ABSTRACT_MANIFEST.read_text(encoding="utf-8"))
        raw = next(item for item in manifest["save_transforms"]
                   if item["kind"] == "bandit_post_handoff_abstract_bootstrap")
        raw = copy.deepcopy(raw)
        raw["native_handoff_capture"]["run_id"] = "not-the-native-run"
        with self.assertRaisesRegex(SystemExit, "bound only to the captured"):
            normalize_fixture_save_transforms([raw], manifest_path=ABSTRACT_MANIFEST)


if __name__ == "__main__":
    unittest.main()
