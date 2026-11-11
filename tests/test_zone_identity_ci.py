#!/usr/bin/env python3
"""Verify that CI cannot credit missing or failed zone-control executions."""

from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import zone_identity_ci


class ZoneIdentityCITest(unittest.TestCase):
    def test_requires_successful_nonempty_controls_for_both_selections(self):
        cases = (
            (0, "All tests passed (90 assertions in 11 test cases)\n"),
            (0, "All tests passed (134 assertions in 6 test cases)\n"),
        )
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / "cata_test"
            binary.touch()
            results = [subprocess.CompletedProcess([], code, output) for code, output in cases]
            with patch("zone_identity_ci.subprocess.run", side_effect=results) as runner:
                self.assertEqual(zone_identity_ci.run_controls(binary), 0)
            self.assertEqual([call.args[0][1] for call in runner.call_args_list],
                             ["[zones][identity]", "[semantic_surface][zone_cursor]"])

    def test_nonzero_missing_or_incomplete_results_fail(self):
        cases = (
            (3, "All tests passed (90 assertions in 11 test cases)\n"),
            (0, "No test cases matched\n"),
            (0, "All tests passed (1 assertion in 1 test case)\n"),
        )
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / "cata_test"
            binary.touch()
            for code, output in cases:
                with self.subTest(code=code, output=output):
                    result = subprocess.CompletedProcess([], code, output)
                    with patch("zone_identity_ci.subprocess.run", return_value=result) as runner:
                        self.assertNotEqual(zone_identity_ci.run_controls(binary), 0)
                        self.assertEqual(runner.call_count, 2)

    def test_timeout_of_either_selection_fails_but_attempts_both(self):
        passing = (
            subprocess.CompletedProcess([], 0, "All tests passed (90 assertions in 11 test cases)\n"),
            subprocess.CompletedProcess([], 0, "All tests passed (134 assertions in 6 test cases)\n"),
        )
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / "cata_test"
            binary.touch()
            for timed_out in (0, 1):
                with self.subTest(timed_out=timed_out):
                    results = list(passing)
                    results[timed_out] = subprocess.TimeoutExpired("cata_test", 600)
                    with patch("zone_identity_ci.subprocess.run", side_effect=results) as runner:
                        self.assertNotEqual(zone_identity_ci.run_controls(binary), 0)
                        self.assertEqual(runner.call_count, 2)
                        self.assertTrue(all(call.kwargs["timeout"] == 600
                                            for call in runner.call_args_list))


if __name__ == "__main__":
    unittest.main()
