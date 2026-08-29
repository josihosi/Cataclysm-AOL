#!/usr/bin/env python3
"""Focused map qualifications for the R-019 positive-progress hostile fixture."""

from __future__ import annotations

import sys
import shutil
import tempfile
import unittest
from pathlib import Path


HARNESS_DIR = Path( __file__ ).resolve().parent
sys.path.insert( 0, str( HARNESS_DIR ) )

from startup_harness import (  # noqa: E402
    apply_fixture_save_transforms,
    native_close_hostile_qualification,
    resolve_fixture_payload,
)


class R019PlacementQualificationTest( unittest.TestCase ):
    player_save = "#Wm9yYWlkYSBWaWNr.sav.zzip"
    world_dir = HARNESS_DIR.parents[1] / ".userdata" / "r009-m095" / "save" / "McWilliams"

    def test_selected_candidate_enters_dangerous_proximity_only_after_progress(self) -> None:
        qualification = native_close_hostile_qualification(
                            self.world_dir, player_save=self.player_save,
                            target_offset=[4, 6, 0], target_was_free=True,
                            primitive_wait_minutes=1, maximum_boundary_entry_steps=1,
                        )
        self.assertEqual( qualification["target_offset_ms"], [4, 6, 0] )
        self.assertTrue( qualification["target_was_free"] )
        self.assertTrue( qualification["visible"] )
        self.assertTrue( qualification["nonempty_route"] )
        self.assertGreater( qualification["route_length"], 0 )
        self.assertFalse( qualification["initial_dangerous_proximity_eligible"] )
        self.assertEqual( qualification["dangerous_proximity"], 5 )
        self.assertEqual( qualification["boundary_entry_steps"], 1 )
        self.assertEqual( qualification["boundary_entry_path"][-1], [3, 5, 0] )

    def test_rejected_east_candidate_cannot_receive_qualification(self) -> None:
        with self.assertRaisesRegex( SystemExit, "opaque_native_sight_tile|blocked_native_route_tile" ):
            native_close_hostile_qualification(
                self.world_dir, player_save=self.player_save,
                target_offset=[12, 0, 0], target_was_free=True,
                primitive_wait_minutes=1, maximum_boundary_entry_steps=1,
            )

    def test_initial_boundary_eligibility_and_late_entry_are_rejected(self) -> None:
        with self.assertRaisesRegex( SystemExit, "initial_dangerous_proximity_eligibility" ):
            native_close_hostile_qualification(
                self.world_dir, player_save=self.player_save,
                target_offset=[5, -1, 0], target_was_free=True,
                primitive_wait_minutes=1, maximum_boundary_entry_steps=1,
            )
        with self.assertRaisesRegex( SystemExit, "dangerous_proximity_entry_exceeds_declared_primitive_wait" ):
            native_close_hostile_qualification(
                self.world_dir, player_save=self.player_save,
                target_offset=[10, -8, 0], target_was_free=True,
                primitive_wait_minutes=1, maximum_boundary_entry_steps=1,
            )

    def test_current_fixture_chain_excludes_the_stale_north_observation_fixture(self) -> None:
        resolved = resolve_fixture_payload(
            "r019_keep_watch_off_positive_progress_v1", "live-debug",
        )

        self.assertEqual(
            resolved["source_chain"],
            [
                ("live-debug", "r019_keep_watch_off_positive_progress_v1"),
                ("live-debug", "bandit_r002_m040_post_abort_recenter_return_v0_2026-08-22"),
            ],
        )
        staged = [
            transform
            for transform in resolved["save_transforms"]
            if transform["kind"] == "active_monsters_near_player"
        ]
        self.assertEqual( staged[-1]["monsters"][0]["offset_ms"], [4, 6, 0] )
        self.assertEqual( staged[-1]["monsters"][0]["qualification_offset_ms"], [4, 6, 0] )
        self.assertTrue( staged[-1]["require_native_close_hostile_qualification"] )
        self.assertEqual( staged[-1]["primitive_wait_minutes"], 1 )
        self.assertEqual( staged[-1]["maximum_boundary_entry_steps"], 1 )
        self.assertNotIn( [0, 12, 0], [
            monster["offset_ms"] for transform in staged for monster in transform.get("monsters", [])
        ] )

    def test_current_fixture_preserves_its_native_close_hostile_qualification(self) -> None:
        resolved = resolve_fixture_payload(
            "r019_keep_watch_off_positive_progress_v1", "live-debug",
        )
        with tempfile.TemporaryDirectory() as directory:
            save_root = Path( directory ) / "save"
            shutil.copytree( resolved["save_src"], save_root )
            world_dir = save_root / "McWilliams"
            transforms = apply_fixture_save_transforms( world_dir, resolved["save_transforms"] )

        hostile_transform = [
            transform for transform in transforms
            if transform["kind"] == "active_monsters_near_player" and transform["placed_monsters"]
        ][-1]
        self.assertEqual( hostile_transform["placed_monsters"][0]["offset_ms"], [4, 6, 0] )
        self.assertEqual( hostile_transform["placed_monsters"][0]["qualification_offset_ms"], [4, 6, 0] )
        self.assertEqual( len( hostile_transform["native_close_hostile_qualifications"] ), 1 )
        self.assertEqual(
            hostile_transform["native_close_hostile_qualifications"][0]["boundary_entry_steps"], 1,
        )

if __name__ == "__main__":
    unittest.main()
