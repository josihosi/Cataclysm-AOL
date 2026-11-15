#!/usr/bin/env python3
"""Check renderer binding and strict per-case execution in the CI route."""

import os
import io
import contextlib
import shutil
import sys
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import travelling_contact_ci as route


class TravellingContactCITest(unittest.TestCase):
    def test_catalogue_preserves_existing_controls_and_one_guarded_addition(self):
        self.assertEqual(len(route.SELECTIONS), 50)
        self.assertEqual(len(set(route.SELECTIONS)), 50)
        self.assertIn("map reload retires old vehicle cache before saved submaps are evicted", route.SELECTIONS)
        self.assertIn("firelight escapes through a distant transparent opening", route.SELECTIONS)
        self.assertIn("unicode world saves with unicode receipt tree entries", route.SELECTIONS)
        self.assertIn("quicksave_reports_confirmed_turn_and_recovers_after_failed_write", route.SELECTIONS)
        self.assertIn("follow close limits target selection only for player allies", route.SELECTIONS)
        self.assertIn("homeward pair reaches its boundary through ordinary paid movement", route.SELECTIONS)
        self.assertIn("homeward boundary crossing awaits a physically incapacitated survivor", route.SELECTIONS)
        self.assertNotIn(route.CURSES_ONLY_NOOP, route.SELECTIONS[:33])
        self.assertEqual(route.SELECTIONS[33:].count(route.CURSES_ONLY_NOOP), 1)
        self.assertFalse(any("benchmark" in selection.lower() for selection in route.SELECTIONS))

    def test_original_regressions_run_both_reported_seeds(self):
        selections = tuple(route.ORIGINAL_REGRESSION_SEEDS)
        passed = (0, "All tests passed (1 assertion in 1 test case)\n")
        with patch.object(route, "SELECTIONS", selections):
            result, calls = self.run_with(True, route.TILES_MARKER, [passed] * 4)
        self.assertEqual(result, 0)
        self.assertEqual([(call.args[0][1], call.args[0][3]) for call in calls[1:]],
                         [(selection, seed) for selection in selections
                          for seed in ("1794708613", "1794710029")])

    def run_with(self, tiles, marker_output, outcomes):
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / "cata_test"
            binary.touch()
            marker = subprocess.CompletedProcess([], 1, marker_output + "\n")
            results = [marker] + [subprocess.CompletedProcess([], code, text) for code, text in outcomes]
            with patch.object(route, "curses_test_environment", return_value={"TERM": "xterm"}), \
                    patch.object(route.subprocess, "run", side_effect=results) as runner:
                result = route.run_controls(binary, tiles)
            return result, runner.call_args_list

    def test_curses_runs_the_native_owner_case(self):
        selections = ("first", route.CURSES_ONLY_NOOP, "last")
        passed = (0, "All tests passed (1 assertion in 1 test case)\n")
        with patch.object(route, "SELECTIONS", selections):
            result, calls = self.run_with(False, route.CURSES_ONLY_NOOP, [passed] * 3)
        self.assertEqual(result, 0)
        self.assertEqual([call.args[0][1] for call in calls[1:]], list(selections))

    def test_linux_curses_case_receives_validated_terminal_only_for_that_case(self):
        selections = ("first", route.CURSES_ONLY_NOOP, "last")
        passed = (0, "All tests passed (1 assertion in 1 test case)\n")
        with patch.object(route.sys, "platform", "linux"), patch.object(route, "SELECTIONS", selections):
            result, calls = self.run_with(False, route.CURSES_ONLY_NOOP, [passed] * 3)
        self.assertEqual(result, 0)
        self.assertIsNone(calls[1].kwargs["env"])
        self.assertEqual(calls[2].kwargs["env"], {"TERM": "xterm"})
        self.assertIsNone(calls[3].kwargs["env"])

    def test_terminal_fallback_preserves_valid_term_and_other_environment(self):
        cases = (("", [0], "xterm"), ("unknown", [0], "xterm"),
                 ("bad-terminal", [1, 0], "xterm"), ("screen", [0], "screen"))
        for terminal, exit_codes, expected in cases:
            with self.subTest(terminal=terminal), \
                    patch.dict(os.environ, {"TERM": terminal, "OTHER_SETTING": "kept"}, clear=True), \
                    patch.object(route.shutil, "which", return_value="/usr/bin/infocmp"), \
                    patch.object(route.subprocess, "run", side_effect=[
                        subprocess.CompletedProcess([], code) for code in exit_codes
                    ]):
                env = route.curses_test_environment()
                self.assertEqual(env, {"TERM": expected, "OTHER_SETTING": "kept"})

    def test_unavailable_terminfo_fails_closed_and_other_cases_continue(self):
        selections = (route.CURSES_ONLY_NOOP, "later")
        passed = (0, "All tests passed (1 assertion in 1 test case)\n")
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / "cata_test"
            binary.touch()
            responses = [subprocess.CompletedProcess([], 1, route.CURSES_ONLY_NOOP + "\n"),
                         subprocess.CompletedProcess([], *passed)]
            with patch.object(route.sys, "platform", "linux"), \
                    patch.object(route, "SELECTIONS", selections), \
                    patch.object(route, "curses_test_environment", return_value=None), \
                    patch.object(route.subprocess, "run", side_effect=responses) as runner:
                self.assertEqual(route.run_controls(binary, False), 1)
            self.assertEqual([call.args[0][1] for call in runner.call_args_list],
                             [f"{route.CURSES_ONLY_NOOP},{route.TILES_MARKER}", "later"])

    def test_missing_infocmp_or_xterm_entry_fails_closed(self):
        with patch.object(route.shutil, "which", return_value=None):
            self.assertIsNone(route.curses_test_environment())
        with patch.dict(os.environ, {"TERM": "unknown"}, clear=True), \
                patch.object(route.shutil, "which", return_value="/usr/bin/infocmp"), \
                patch.object(route.subprocess, "run", return_value=subprocess.CompletedProcess([], 1)):
            self.assertIsNone(route.curses_test_environment())

    @unittest.skipUnless(os.name == "posix" and shutil.which("infocmp"),
                         "requires a local curses/terminfo installation")
    def test_headless_invalid_terminal_runs_real_curses_setup_with_fallback(self):
        if subprocess.run(["infocmp", "xterm"], stdout=subprocess.DEVNULL,
                          stderr=subprocess.DEVNULL, check=False).returncode != 0:
            self.skipTest("xterm terminfo is unavailable")
        invalid_term = "caol-invalid-terminal-829a"
        if subprocess.run(["infocmp", invalid_term], stdout=subprocess.DEVNULL,
                          stderr=subprocess.DEVNULL, check=False).returncode == 0:
            self.skipTest("test terminal unexpectedly exists")
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / "cata_test"
            binary.write_text(
                f"#!{sys.executable}\n"
                "import curses, sys\n"
                f"marker = {route.CURSES_ONLY_NOOP!r}\n"
                "if '--list-test-names-only' in sys.argv:\n"
                "    print(marker)\n"
                "    raise SystemExit(1)\n"
                "curses.setupterm()\n"
                "print('All tests passed (1 assertion in 1 test case)')\n"
            )
            binary.chmod(0o755)
            env = os.environ.copy()
            env["TERM"] = invalid_term
            original = subprocess.run([str(binary), route.CURSES_ONLY_NOOP], env=env,
                                      stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                      text=True, check=False)
            self.assertNotEqual(original.returncode, 0, original.stdout)
            with patch.dict(os.environ, {"TERM": invalid_term}), \
                    patch.object(route.sys, "platform", "linux"), \
                    patch.object(route, "SELECTIONS", (route.CURSES_ONLY_NOOP,)):
                self.assertEqual(route.run_controls(binary, False), 0)

    def test_tiles_skips_only_the_guarded_case(self):
        selections = ("first", route.CURSES_ONLY_NOOP, "last")
        passed = (0, "All tests passed (2 assertions in 1 test case)\n")
        with patch.object(route, "SELECTIONS", selections):
            result, calls = self.run_with(True, route.TILES_MARKER, [passed] * 2)
        self.assertEqual(result, 0)
        self.assertEqual([call.args[0][1] for call in calls[1:]], ["first", "last"])

    def test_unexpected_missing_case_fails_and_later_cases_run(self):
        selections = ("missing", "later")
        outcomes = [(0, "No test cases matched\n"),
                    (0, "All tests passed (1 assertion in 1 test case)\n")]
        with patch.object(route, "SELECTIONS", selections):
            result, calls = self.run_with(True, route.TILES_MARKER, outcomes)
        self.assertEqual(result, 1)
        self.assertEqual([call.args[0][1] for call in calls[1:]], list(selections))

    def test_timeout_reports_bounded_partial_output_and_continues(self):
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / "cata_test"
            binary.touch()
            partial = (b"discarded-prefix\nR067_ALARM_VARIANT faction=cannibal mode=move\n" +
                       b"x" * 16384 + b"last-variant")
            responses = [subprocess.CompletedProcess([], 1, route.TILES_MARKER + "\n"),
                         subprocess.TimeoutExpired("cata_test", 600, output=partial),
                         subprocess.CompletedProcess([], 0, "All tests passed (1 assertion in 1 test case)\n")]
            stderr = io.StringIO()
            with patch.object(route, "SELECTIONS", ("slow", "later")), \
                    patch.object(route.subprocess, "run", side_effect=responses) as runner, \
                    contextlib.redirect_stderr(stderr):
                self.assertEqual(route.run_controls(binary, True), 1)
            self.assertIn("last-variant", stderr.getvalue())
            self.assertIn("Last observed variant: R067_ALARM_VARIANT faction=cannibal mode=move",
                          stderr.getvalue())
            self.assertNotIn("discarded-prefix", stderr.getvalue())
            self.assertEqual([call.args[0][1] for call in runner.call_args_list[1:]], ["slow", "later"])

    def test_renderer_mismatch_and_missing_markers_fail_closed(self):
        for tiles, marker in ((False, route.TILES_MARKER), (True, route.CURSES_ONLY_NOOP),
                              (False, ""), (True, "")):
            with self.subTest(tiles=tiles, marker=marker):
                result, calls = self.run_with(tiles, marker, [])
                self.assertEqual(result, 1)
                self.assertEqual(len(calls), 1)

    def test_renderer_query_error_and_timeout_fail_closed(self):
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / "cata_test"
            binary.touch()
            for response in (subprocess.CompletedProcess([], 2, route.TILES_MARKER),
                             subprocess.TimeoutExpired("cata_test", 30)):
                with self.subTest(response=response):
                    with patch.object(route.subprocess, "run", side_effect=[response]) as runner:
                        self.assertEqual(route.run_controls(binary, True), 1)
                    self.assertEqual(runner.call_count, 1)

    def test_workflows_bind_real_build_configurations(self):
        matrix = Path(".github/workflows/matrix.yml").read_text()
        windows = Path(".github/workflows/msvc-full-features.yml").read_text()
        self.assertIn("tests/travelling_contact_ci.py tests/cata_test --tiles 0", matrix)
        self.assertIn("tests/travelling_contact_ci.py tests/cata_test --tiles ${{ matrix.tiles }}", matrix)
        self.assertIn(r"tests/travelling_contact_ci.py .\Cataclysm-test-vcpkg-static-Release-x64.exe --tiles 1",
                      windows)
        self.assertEqual(matrix.count("python3 tests/test_travelling_contact_ci.py"), 2)
        self.assertEqual(windows.count("py -3 tests/test_travelling_contact_ci.py"), 1)


if __name__ == "__main__":
    unittest.main()
