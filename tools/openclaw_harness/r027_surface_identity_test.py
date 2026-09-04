#!/usr/bin/env python3
"""Focused fail-closed controls for R-027's isolated visible surface."""

from __future__ import annotations

import hashlib
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch


HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))
import startup_harness as harness  # noqa: E402


class R027SurfaceIdentityTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.pid = 8123
        self.title = "C-AOL certification round-82dcb879 deadbeefcafebabe"
        self.digest = hashlib.sha256(b"isolated executable").hexdigest()
        (self.root / "process.json").write_text(json.dumps({"surface_identity": {
            "pid": self.pid, "window_title": self.title,
            "executable_sha256": self.digest, "lease_id": "lease",
            "cleanup_token": "token",
        }}), encoding="utf-8")

    def tearDown(self):
        self.temp.cleanup()

    def test_shared_window_wrong_pid_and_ambiguous_windows_are_rejected(self):
        surface = harness.bound_surface_identity(self.root, self.pid)
        shared = {"title": "Cataclysm: Dark Days Ahead - shared", "window_id": 1}
        matching = {"title": self.title, "window_id": 2, "isOnScreen": True}
        with patch.object(harness, "list_windows_for_pid", return_value=[shared]):
            self.assertEqual(harness.choose_capture_window(self.pid, surface)["rejected"],
                             "missing_or_ambiguous_bound_surface")
        with patch.object(harness, "list_windows_for_pid", return_value=[matching, dict(matching, window_id=3)]):
            self.assertEqual(harness.choose_capture_window(self.pid, surface)["rejected"],
                             "missing_or_ambiguous_bound_surface")
        with patch.object(harness, "list_windows_for_pid", return_value=[matching]):
            self.assertEqual(harness.choose_capture_window(self.pid, surface)["window_id"], 2)
        decorated = dict(matching, title="Cataclysm: Dark Days Ahead - build - " + self.title)
        with patch.object(harness, "list_windows_for_pid", return_value=[decorated]):
            self.assertEqual(harness.choose_capture_window(self.pid, surface)["window_id"], 2)

    def test_stale_surface_identity_is_not_reused_for_another_pid(self):
        self.assertEqual(harness.bound_surface_identity(self.root, self.pid + 1), {})

    def test_invalid_persisted_surface_blocks_capture_before_input(self):
        (self.root / "process.json").write_text(json.dumps({"surface_identity": {
            "pid": self.pid, "window_title": self.title, "executable_sha256": "bad",
        }}), encoding="utf-8")
        surface = harness.bound_surface_identity(self.root, self.pid)
        self.assertEqual(surface["rejected"], "missing_or_invalid_persisted_surface_identity")

    def test_bound_cleanup_uses_lease_for_renamed_game(self):
        lease = {
            "lease_id": "lease", "pid": self.pid, "world_identity": "world",
            "executable_path": "/game/r027-closure-007-tiles",
            "executable_sha256": self.digest, "process_birth_identity": "birth",
            "cleanup_token": "token",
        }
        (self.root / "process.json").write_text(json.dumps({
            "pid": self.pid, "certification_lease": lease,
        }), encoding="utf-8")
        connection = unittest.mock.MagicMock()
        with patch.object(harness, "load_certification_manifest", return_value={"round_id": "round"}), \
                patch.object(harness, "open_registry", return_value=connection), \
                patch.object(harness, "release_certification_lease_handle",
                             return_value={"status": "exit_unobserved", "lease": lease}) as release:
            cleanup = harness.finalize_bound_certification_process(
                self.root, self.pid, registry_path="registry.sqlite", manifest_path="round.json",
            )
        self.assertEqual(cleanup["status"], "exit_unobserved")
        release.assert_called_once()
        self.assertTrue(connection.close.called)
        recorded = json.loads((self.root / "process.json").read_text(encoding="utf-8"))
        self.assertEqual(recorded["cleanup"]["owner"], "certification_process_lease")
        self.assertEqual(recorded["cleanup"]["cleanup_token"], "token")


if __name__ == "__main__":
    unittest.main()
