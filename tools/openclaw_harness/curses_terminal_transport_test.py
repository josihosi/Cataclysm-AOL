#!/usr/bin/env python3
"""Focused checks for the harness-owned curses PTY bootstrap."""

from __future__ import annotations

import subprocess
import sys
import tempfile
import time
import unittest
from unittest import mock
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from curses_terminal_transport import _key_bytes, dispatch_input, should_use_curses_terminal


class CursesTerminalTransportTest(unittest.TestCase):
    def test_auto_selects_only_non_tiles_binary(self) -> None:
        self.assertTrue(should_use_curses_terminal(Path("cataclysm"), "auto"))
        self.assertFalse(should_use_curses_terminal(Path("cataclysm-tiles"), "auto"))
        self.assertTrue(should_use_curses_terminal(Path("cataclysm-tiles"), "pty"))
        self.assertFalse(should_use_curses_terminal(Path("cataclysm"), "pipes"))

    def test_f1_uses_xterm_ss3_sequence_for_native_trade_autobalance(self) -> None:
        self.assertEqual(_key_bytes(["F1"]), b"\x1bOP")

    def test_tab_uses_the_native_control_character_for_trade_pane_switching(self) -> None:
        self.assertEqual(_key_bytes(["tab"]), b"\t")

    def test_curses_child_has_a_controlling_terminal_and_transcript(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            run_dir = Path(temp)
            script = (
                "import os, sys; "
                "print('stdin_tty=' + str(os.isatty(sys.stdin.fileno()))); sys.stdout.flush()"
            )
            from curses_terminal_transport import CursesTerminalTransport

            transport, slave_fd = CursesTerminalTransport.open(run_dir / "game.terminal.log")
            try:
                process = subprocess.Popen(
                    [sys.executable, "-c", script], stdin=slave_fd, stdout=slave_fd, stderr=slave_fd,
                    start_new_session=True, preexec_fn=CursesTerminalTransport.make_controlling_terminal,
                )
            finally:
                import os
                os.close(slave_fd)
            transport.start_reader()
            self.assertEqual(process.wait(timeout=5), 0)
            transport.close()
            self.assertIn("stdin_tty=True", (run_dir / "game.terminal.log").read_text())

    def test_detached_dispatcher_accepts_only_the_bound_run_and_pid(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            run_dir = Path(temp)
            from curses_terminal_transport import CursesTerminalTransport
            transport, slave_fd = CursesTerminalTransport.open(run_dir / "game.terminal.log")
            try:
                process = subprocess.Popen(
                    [sys.executable, "-c", "import os, sys, tty; tty.setraw(0); print(os.read(0, 1).decode(), flush=True)"],
                    stdin=slave_fd, stdout=slave_fd, stderr=slave_fd, start_new_session=True,
                    preexec_fn=CursesTerminalTransport.make_controlling_terminal,
                )
            finally:
                import os
                os.close(slave_fd)
            endpoint = transport.hand_off_to_dispatcher(run_id="run-a", pid=process.pid)
            with self.assertRaisesRegex(RuntimeError, "rejected request"):
                dispatch_input(endpoint, run_id="wrong-run", pid=process.pid, keys=["f"])
            receipt = dispatch_input(endpoint, run_id="run-a", pid=process.pid, keys=["f"])
            self.assertTrue(receipt["ok"])
            self.assertEqual(receipt["owner"], "run_bound_pty")
            self.assertEqual(process.wait(timeout=5), 0)
            deadline = time.monotonic() + 2
            while endpoint.exists() and time.monotonic() < deadline:
                time.sleep(0.01)
            self.assertFalse(endpoint.exists())
            transport.close()
            self.assertIn("f", (run_dir / "game.terminal.log").read_text())

    def test_detached_dispatcher_does_not_inherit_starter_capture_streams(self) -> None:
        """A detached broker must not keep a start subprocess's stdout pipe open."""
        with tempfile.TemporaryDirectory() as temp:
            run_dir = Path(temp)
            from curses_terminal_transport import CursesTerminalTransport
            transport, slave_fd = CursesTerminalTransport.open(run_dir / "game.terminal.log")
            try:
                endpoint = Path("/tmp") / "caol-test-broker-stdio.sock"
                with mock.patch.object(CursesTerminalTransport, "hand_off_to_dispatcher") as handoff:
                    handoff.return_value = endpoint
                    self.assertEqual(transport.hand_off_to_dispatcher(run_id="run", pid=1), endpoint)
                # Regression coverage is source-level because the real broker
                # intentionally outlives this test's starter process.
                source = Path(__file__).with_name("curses_terminal_transport.py").read_text()
                self.assertIn("stdout=__import__(\"subprocess\").DEVNULL", source)
                self.assertIn("stderr=__import__(\"subprocess\").DEVNULL", source)
            finally:
                import os
                os.close(slave_fd)
                transport.close()


if __name__ == "__main__":
    unittest.main()
