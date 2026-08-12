#!/usr/bin/env python3
"""Focused tests for the hidden fresh-normal-world harness seam."""

from __future__ import annotations

import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import startup_harness as harness


class HarnessNewWorldTest(unittest.TestCase):
    def test_validate_preserves_decimal_seed_provenance(self) -> None:
        self.assertEqual(
            harness.validate_harness_new_world("natural-r002", "830205018"),
            ("natural-r002", "830205018"),
        )

    def test_validate_rejects_zero_overflow_and_path_names(self) -> None:
        for world, seed, message in (
            ("natural-r002", "0", "harness raw seed must be non-zero"),
            ("natural-r002", "4294967296", "harness raw seed is outside the uint32 range"),
            ("../natural-r002", "830205018", "harness new-world name must be one path component"),
        ):
            with self.subTest(world=world, seed=seed):
                with self.assertRaisesRegex(ValueError, message):
                    harness.validate_harness_new_world(world, seed)

    def test_game_argv_is_exact_and_harness_args_are_suppressed_player_ui(self) -> None:
        command = harness.build_game_command(
            Path("/game/cataclysm-tiles"),
            "r002-m93",
            "natural-r002",
            harness_new_world="natural-r002",
            harness_raw_seed="830205018",
        )
        self.assertEqual(
            command,
            [
                "/game/cataclysm-tiles",
                "--userdir", ".userdata/r002-m93/",
                "--harness-new-world", "natural-r002",
                "--harness-raw-seed", "830205018",
            ],
        )

    def test_harness_startup_skips_permission_focus_and_input_owners(self) -> None:
        self.assertFalse(harness.startup_gui_automation_required("harness_new_world"))
        self.assertTrue(harness.startup_gui_automation_required("play_now_default"))

    def test_exit_success_requires_zero_code_and_newer_marker(self) -> None:
        marker = {"mtime": 20.0}
        self.assertTrue(harness.harness_save_ready(0, marker, 10.0))
        self.assertFalse(harness.harness_save_ready(1, marker, 10.0))
        self.assertFalse(harness.harness_save_ready(0, {"mtime": 10.0}, 10.0))
        self.assertTrue(harness.harness_process_ready(0, marker, 10.0, 0))
        self.assertFalse(harness.harness_process_ready(0, marker, 10.0, 1))

    def test_cleanup_accepts_supported_executable_identities(self) -> None:
        for command in (
            "/game/Cataclysm-AOL --userdir .userdata/p/",
            "/game/Cataclysm-AOL.exe --userdir .userdata/p/",
            "/game/cataclysm-tiles --userdir .userdata/p/",
            "/game/cataclysm-tlg-tiles.exe --userdir .userdata/p/",
        ):
            with self.subTest(command=command), patch.object(harness, "pid_command", return_value=command), patch.object(
                harness, "pid_is_alive", return_value=False
            ), patch.object(harness.os, "kill") as kill:
                result = harness.cleanup_game_process(123, grace_seconds=0.0)
            self.assertEqual(result["status"], "terminated")
            kill.assert_called_once()

    def test_plan_rejects_fixture_world_and_reused_name(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            with patch.object(harness, "repo_root", return_value=root), patch.object(
                harness, "detect_executable", return_value=root / "game"
            ), patch.object(harness, "create_run_dir", return_value=root / "run"):
                with self.assertRaisesRegex(ValueError, "cannot be combined"):
                    harness.build_plan("profile", "", "fixture", "natural-r002", "830205018")

                (root / ".userdata" / "profile" / "save" / "natural-r002").mkdir(parents=True)
                with self.assertRaisesRegex(ValueError, "already exists"):
                    harness.build_plan("profile", "", "", "natural-r002", "830205018")

    def test_plan_propagates_named_world_and_raw_seed(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            with patch.object(harness, "repo_root", return_value=root), patch.object(
                harness, "detect_executable", return_value=root / "game"
            ), patch.object(harness, "create_run_dir", return_value=root / "run"):
                plan = harness.build_plan("profile", "", "", "natural-r002", "830205018")
        self.assertEqual(plan.strategy, "harness_new_world")
        self.assertEqual(plan.target_world, "natural-r002")
        self.assertEqual(plan.harness_new_world, "natural-r002")
        self.assertEqual(plan.harness_raw_seed, "830205018")

    def test_failed_run_cleanup_removes_only_the_named_fresh_world(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            world_dir = root / ".userdata" / "profile" / "save" / "natural-r002"
            other_world = root / ".userdata" / "profile" / "save" / "keep-me"
            world_dir.mkdir(parents=True)
            other_world.mkdir()
            run_dir = root / "run"
            run_dir.mkdir()
            (world_dir / ".openclaw-harness-owner").write_text(str(run_dir.resolve()), encoding="utf-8")
            with patch.object(harness, "repo_root", return_value=root):
                result = harness.cleanup_harness_world("profile", "natural-r002", run_dir)
            self.assertEqual(result["status"], "removed")
            self.assertFalse(world_dir.exists())
            self.assertTrue(other_world.exists())

    def test_failed_run_cleanup_refuses_unowned_world(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            world_dir = root / ".userdata" / "profile" / "save" / "natural-r002"
            world_dir.mkdir(parents=True)
            (world_dir / ".openclaw-harness-owner").write_text("another-run", encoding="utf-8")
            run_dir = root / "run"
            run_dir.mkdir()
            with patch.object(harness, "repo_root", return_value=root):
                result = harness.cleanup_harness_world("profile", "natural-r002", run_dir)
            self.assertEqual(result["status"], "skipped_unowned_world")
            self.assertTrue(world_dir.exists())


if __name__ == "__main__":
    unittest.main()
