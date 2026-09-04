"""Harness-owned POSIX terminal transport for an interactive curses child.

The transport deliberately owns only the terminal.  It does not translate a
semantic action into bytes: semantic request delivery remains the native
surface boundary.  Keeping that distinction lets a curses diagnostic have a
real controlling terminal without reviving the retired key-dispatch route.
"""

from __future__ import annotations

from dataclasses import dataclass
import errno
import fcntl
import os
from pathlib import Path
import pty
import termios
import threading
from typing import IO


@dataclass
class CursesTerminalTransport:
    """A controlling PTY and an immutable transcript for one child process."""

    master_fd: int
    transcript_path: Path
    _transcript: IO[bytes]
    _reader: threading.Thread | None = None

    @classmethod
    def open(cls, transcript_path: Path) -> tuple["CursesTerminalTransport", int]:
        """Create a PTY pair; the caller passes the returned slave to Popen."""
        master_fd, slave_fd = pty.openpty()
        transcript_path.parent.mkdir(parents=True, exist_ok=True)
        transport = cls(master_fd, transcript_path, transcript_path.open("wb"))
        return transport, slave_fd

    @staticmethod
    def make_controlling_terminal() -> None:
        """Run in the child after setsid so curses sees its own terminal."""
        fcntl.ioctl(0, termios.TIOCSCTTY, 0)

    def start_reader(self) -> None:
        """Drain terminal output so a verbose curses child cannot block on PTY output."""
        if self._reader is not None:
            return

        def drain() -> None:
            while True:
                try:
                    chunk = os.read(self.master_fd, 65536)
                except OSError as error:
                    if error.errno == errno.EIO:
                        break
                    return
                if not chunk:
                    break
                self._transcript.write(chunk)
                self._transcript.flush()

        self._reader = threading.Thread(target=drain, name="caol-curses-pty", daemon=True)
        self._reader.start()

    def close(self) -> None:
        """Close the harness terminal after its child has stopped."""
        try:
            os.close(self.master_fd)
        except OSError:
            pass
        if self._reader is not None:
            self._reader.join()
        self._transcript.close()


def should_use_curses_terminal(executable: Path, requested: str) -> bool:
    """Select PTY only for an explicit request or a non-tiles Cataclysm binary."""
    selection = requested.strip().lower()
    if selection not in {"", "auto", "pty", "pipes"}:
        raise ValueError("terminal transport must be auto, pty, or pipes")
    if selection == "pty":
        return True
    if selection == "pipes":
        return False
    return "tiles" not in executable.name.lower()
