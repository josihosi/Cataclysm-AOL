#!/usr/bin/env python3
"""Focused checks for the harness-owned curses PTY bootstrap."""

from __future__ import annotations

import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from curses_terminal_transport import should_use_curses_terminal


class CursesTerminalTransportTest(unittest.TestCase):
    def test_auto_selects_only_non_tiles_binary(self) -> None:
        self.assertTrue(should_use_curses_terminal(Path("cataclysm"), "auto"))
        self.assertFalse(should_use_curses_terminal(Path("cataclysm-tiles"), "auto"))
        self.assertTrue(should_use_curses_terminal(Path("cataclysm-tiles"), "pty"))
        self.assertFalse(should_use_curses_terminal(Path("cataclysm"), "pipes"))

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


if __name__ == "__main__":
    unittest.main()
