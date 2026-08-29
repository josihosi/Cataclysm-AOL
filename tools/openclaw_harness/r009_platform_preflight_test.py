#!/usr/bin/env python3
"""Focused tests for the authority-free R-009 platform preflight."""

from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent))

import startup_harness


class R009PlatformPreflightTest(unittest.TestCase):
    def test_ready_preflight_binds_runtime_and_leaves_run_unstarted(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            executable = root / "cataclysm-tiles"
            executable.write_bytes(b"fixture executable")
            executable.chmod(0o755)
            artifact_root = root / ".userdata" / "r009" / "harness_runs"

            with mock.patch.object(startup_harness, "detect_executable", return_value=executable), \
                    mock.patch.object(startup_harness, "build_runtime_binding", return_value={
                        "ok": True,
                        "executable_path": str(executable),
                        "executable_sha256": "e" * 64,
                        "runtime_source_sha256": "s" * 64,
                    }), \
                    mock.patch.object(startup_harness, "userdir_for_profile",
                                      return_value=artifact_root.parent), \
                    mock.patch.object(startup_harness, "create_run_dir",
                                      side_effect=AssertionError("preflight must not create a run")):
                result = startup_harness.r009_platform_preflight("r009")

            self.assertEqual(result["status"], "ready")
            self.assertTrue(result["no_process_started"])
            self.assertEqual(result["launch_prerequisites"]["source_and_executable_binding"]["status"], "matched")
            self.assertEqual(result["launch_prerequisites"]["artifact_location"]["root"], str(artifact_root))
            self.assertFalse(artifact_root.exists())
            self.assertEqual(
                result["launch_prerequisites"]["selected_run_authority"]["status"],
                "not_requested",
            )

    def test_missing_binding_blocks_without_claiming_a_witness(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            with mock.patch.object(startup_harness, "detect_executable",
                                   side_effect=SystemExit("missing executable")), \
                    mock.patch.object(startup_harness, "userdir_for_profile", return_value=root):
                result = startup_harness.r009_platform_preflight("r009")

        self.assertEqual(result["status"], "blocked")
        self.assertEqual(result["launch_prerequisites"]["executable"]["status"], "unavailable")
        self.assertTrue(result["no_process_started"])
        self.assertEqual(result["continuous_final_certification_credit"], 0)


if __name__ == "__main__":
    unittest.main()
