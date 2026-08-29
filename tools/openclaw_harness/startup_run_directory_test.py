#!/usr/bin/env python3
"""Run-artifact isolation checks for the canonical startup launcher."""

from __future__ import annotations

import tempfile
import sys
import unittest
from pathlib import Path
from unittest import mock

HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

import startup_harness


class StartupRunDirectoryTest(unittest.TestCase):
    def test_same_second_runs_receive_distinct_owned_directories(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            with mock.patch.object(startup_harness, "userdir_for_profile", return_value=root), \
                    mock.patch.object(startup_harness.time, "strftime", return_value="20260828_121922"):
                first = startup_harness.create_run_dir("r009-m095")
                second = startup_harness.create_run_dir("r009-m095")

        self.assertNotEqual(first, second)
        self.assertEqual(first.parent, second.parent)
        self.assertTrue(first.name.startswith("20260828_121922_"))
        self.assertTrue(second.name.startswith("20260828_121922_"))

    def test_interruption_action_uses_the_public_advertised_actions_field(self) -> None:
        observation = {
            "advertised_actions": ["activity.stop", "activity.ignore"],
        }

        self.assertTrue(
            startup_harness.cockpit_observation_advertises_action(
                observation, "activity.ignore",
            )
        )


if __name__ == "__main__":
    unittest.main()
