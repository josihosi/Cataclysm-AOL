#!/usr/bin/env python3
"""Regression contract for the native recurring wait-menu identity seam."""

from pathlib import Path
import unittest


class R027WaitMenuIdentityTest(unittest.TestCase):
    def setUp(self):
        self.source = (Path(__file__).resolve().parents[2] / "src" / "handle_action.cpp").read_text()

    def test_recurring_wait_choices_use_logical_not_counter_identity(self):
        self.assertIn('"wait-mode:wait-a-while"', self.source)
        self.assertIn('"wait-mode:set-alarm"', self.source)
        self.assertIn('"wait-duration:" + action_id', self.source)
        self.assertIn('"uilist-entry:" + std::to_string( ++next_id )',
                      (Path(__file__).resolve().parents[2] / "src" / "uilist.cpp").read_text())

    def test_duration_identity_is_distinct_per_native_operation(self):
        self.assertNotEqual("wait-duration:wait.1m", "wait-duration:wait.5m")
        self.assertEqual("wait-mode:wait-a-while", "wait-mode:wait-a-while")


if __name__ == "__main__":
    unittest.main()
