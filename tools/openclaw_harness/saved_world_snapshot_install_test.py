#!/usr/bin/env python3
"""Saved-world snapshot copies retain bytes while becoming usable profiles."""

from __future__ import annotations

import stat
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

import startup_harness


class SavedWorldSnapshotInstallTest(unittest.TestCase):
    def test_read_only_snapshot_and_destination_install_as_writable_exact_copy(self):
        with tempfile.TemporaryDirectory(prefix="saved_world_snapshot_") as temporary:
            root = Path(temporary)
            source = root / "source" / "TestSetup00"
            source_maps = source / "maps"
            source_maps.mkdir(parents=True)
            source_file = source_maps / "4.4.-3.zzip"
            source_file.write_bytes(b"saved map bytes")
            source_dir_original_mode = stat.S_IMODE(source_maps.stat().st_mode)
            source_file_original_mode = stat.S_IMODE(source_file.stat().st_mode)
            source_maps.chmod(0o500)
            source_file.chmod(0o400)

            profile_save = root / "profile" / "save"
            old_world = profile_save / "TestSetup00"
            old_maps = old_world / "maps"
            old_maps.mkdir(parents=True)
            old_file = old_maps / "obsolete.zzip"
            old_file.write_bytes(b"old disposable profile")
            old_maps.chmod(0o500)
            old_file.chmod(0o400)

            try:
                source_hash, source_error = startup_harness.sha256_tree(source)
                self.assertEqual(source_error, "")
                with mock.patch.object(startup_harness, "save_dir_for_profile",
                                       return_value=profile_save):
                    result = startup_harness.install_saved_world_snapshot(
                        "disposable-profile", "TestSetup00", source, replace=True,
                    )

                installed = profile_save / "TestSetup00"
                installed_file = installed / "maps" / "4.4.-3.zzip"
                installed_hash, installed_error = startup_harness.sha256_tree(installed)
                self.assertEqual(result["status"], "green_saved_world_snapshot_installed")
                self.assertEqual(result["source_sha256"], source_hash)
                self.assertEqual(installed_error, "")
                self.assertEqual(installed_hash, source_hash)
                self.assertEqual(installed_file.read_bytes(), b"saved map bytes")
                self.assertFalse((installed / "maps" / "obsolete.zzip").exists())
                self.assertTrue(stat.S_IMODE((installed / "maps").stat().st_mode) & stat.S_IWUSR)
                self.assertTrue(stat.S_IMODE(installed_file.stat().st_mode) & stat.S_IWUSR)
                marker = installed / "profile-copy-is-writable"
                marker.write_text("ok", encoding="utf-8")
                marker.unlink()
                self.assertEqual(stat.S_IMODE(source_maps.stat().st_mode), 0o500)
                self.assertEqual(stat.S_IMODE(source_file.stat().st_mode), 0o400)
                self.assertEqual(result["profile_copy_mode_policy"],
                                 "owner_writable_files_and_directories")
            finally:
                source_file.chmod(source_file_original_mode)
                source_maps.chmod(source_dir_original_mode)


if __name__ == "__main__":
    unittest.main()
