"""Harness-owned POSIX terminal transport for an interactive curses child.

The transport deliberately owns only the terminal.  It does not translate a
semantic action into bytes: semantic request delivery remains the native
surface boundary.  Keeping that distinction lets a curses diagnostic have a
real controlling terminal without reviving the retired key-dispatch route.
"""

from __future__ import annotations

from dataclasses import dataclass
import errno
import hashlib
import json
import os
from pathlib import Path
import select
import socket
import sys
import threading
import time
import uuid
from typing import IO


@dataclass
class CursesTerminalTransport:
    """A controlling PTY and an immutable transcript for one child process."""

    master_fd: int
    transcript_path: Path
    _transcript: IO[bytes]
    _reader: threading.Thread | None = None
    _broker: object | None = None
    _broker_endpoint: Path | None = None

    @classmethod
    def open(cls, transcript_path: Path) -> tuple["CursesTerminalTransport", int]:
        """Create a PTY pair; the caller passes the returned slave to Popen."""
        if os.name != "posix":
            raise OSError("curses PTY transport requires POSIX; use the native tiles route on Windows")
        import pty
        master_fd, slave_fd = pty.openpty()
        transcript_path.parent.mkdir(parents=True, exist_ok=True)
        transport = cls(master_fd, transcript_path, transcript_path.open("wb"))
        return transport, slave_fd

    @staticmethod
    def make_controlling_terminal() -> None:
        """Run in the child after setsid so curses sees its own terminal."""
        import fcntl
        import termios
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
        broker = self._broker
        if broker is not None:
            if getattr(broker, "poll")() is None:
                getattr(broker, "terminate")()
            try:
                getattr(broker, "wait")(timeout=2)
            except __import__("subprocess").TimeoutExpired:
                getattr(broker, "kill")()
                getattr(broker, "wait")(timeout=2)
            self._broker = None
        if self._broker_endpoint is not None:
            self._broker_endpoint.unlink(missing_ok=True)
            self._broker_endpoint = None
        try:
            os.close(self.master_fd)
        except OSError:
            pass
        if self._reader is not None:
            self._reader.join()
        self._transcript.close()

    def hand_off_to_dispatcher(self, *, run_id: str, pid: int) -> Path:
        """Give a detached, run-bound broker ownership of this PTY master.

        A probe process may finish with a deliberately retained game.  Keeping
        the master only in that probe would send the child a terminal hangup.
        The broker is also the sole accepted path for terminal-native input.
        """
        # macOS AF_UNIX paths are short; harness run directories are not.
        endpoint = Path("/tmp") / ("caol-pty-" + hashlib.sha256(
            (str(self.transcript_path) + run_id + str(pid)).encode()
        ).hexdigest()[:24] + ".sock")
        endpoint.unlink(missing_ok=True)
        command = [sys.executable, str(Path(__file__).resolve()), "--broker",
                   "--master-fd", str(self.master_fd), "--transcript", str(self.transcript_path),
                   "--endpoint", str(endpoint), "--run-id", run_id, "--pid", str(pid)]
        # The broker outlives the launcher.  Do not inherit a JSON-launch
        # subprocess's capture pipes: their open write ends would keep
        # communicate() waiting after the starter has successfully returned.
        broker = __import__("subprocess").Popen(
            command, pass_fds=(self.master_fd,), start_new_session=True,
            stdout=__import__("subprocess").DEVNULL,
            stderr=__import__("subprocess").DEVNULL,
        )
        self._broker = broker
        self._broker_endpoint = endpoint
        deadline = time.monotonic() + 2.0
        while time.monotonic() < deadline:
            if endpoint.exists():
                self._transcript.close()
                os.close(self.master_fd)
                self.master_fd = -1
                return endpoint
            if broker.poll() is not None:
                break
            time.sleep(0.01)
        broker.terminate()
        self._broker = None
        self._broker_endpoint = None
        raise RuntimeError("terminal dispatcher did not create its run-bound endpoint")


def dispatch_input(endpoint: Path, *, run_id: str, pid: int, keys: list[str], delay_ms: int = 0) -> dict:
    """Deliver one exact request to the detached terminal owner and await receipt."""
    request = {"request_id": uuid.uuid4().hex, "run_id": run_id, "pid": pid, "keys": keys,
               "delay_ms": max(0, int(delay_ms))}
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as client:
        client.settimeout(2.0 + len(keys) * request["delay_ms"] / 1000)
        client.connect(str(endpoint))
        client.sendall((json.dumps(request, sort_keys=True) + "\n").encode())
        response = b""
        while not response.endswith(b"\n"):
            chunk = client.recv(65536)
            if not chunk:
                break
            response += chunk
    receipt = json.loads(response.decode() or "{}")
    if receipt.get("request_id") != request["request_id"] or not receipt.get("ok"):
        raise RuntimeError("terminal input dispatcher rejected request: " + json.dumps(receipt, sort_keys=True))
    return receipt


def _key_bytes(keys: list[str]) -> bytes:
    named = {"return": b"\r", "enter": b"\r", "tab": b"\t", "escape": b"\x1b", "space": b" ", "F1": b"\x1bOP",
             "up": b"\x1bOA", "down": b"\x1bOB", "right": b"\x1bOC", "left": b"\x1bOD"}
    output = bytearray()
    for key in keys:
        if key in named:
            output.extend(named[key])
        elif len(key) == 1 and key.isprintable():
            output.extend(key.encode("utf-8"))
        else:
            raise ValueError("unsupported terminal-native key: " + repr(key))
    return bytes(output)


def _broker(master_fd: int, transcript_path: Path, endpoint: Path, run_id: str, pid: int) -> int:
    endpoint.unlink(missing_ok=True)
    transcript = transcript_path.open("ab")
    listener = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    listener.bind(str(endpoint))
    listener.listen(4)
    listener.settimeout(0.05)
    try:
        while True:
            try:
                os.kill(pid, 0)
            except ProcessLookupError:
                return 0
            readable, _, _ = select.select([master_fd], [], [], 0.05)
            if readable:
                try:
                    data = os.read(master_fd, 65536)
                    if data:
                        transcript.write(data); transcript.flush()
                except OSError as error:
                    if error.errno == errno.EIO:
                        return 0
            try:
                client, _ = listener.accept()
            except socket.timeout:
                continue
            with client:
                raw = client.recv(65536).split(b"\n", 1)[0]
                try:
                    request = json.loads(raw.decode())
                    if request.get("run_id") != run_id or int(request.get("pid", 0)) != pid:
                        raise ValueError("run_or_pid_mismatch")
                    keys = [str(key) for key in request.get("keys", [])]
                    payload = _key_bytes(keys)
                    delay = max(0, int(request.get("delay_ms", 0))) / 1000
                    if delay and len(keys) > 1:
                        for index, key in enumerate(keys):
                            if index:
                                time.sleep(delay)
                            os.write(master_fd, _key_bytes([key]))
                    else:
                        os.write(master_fd, payload)
                    receipt = {"ok": True, "request_id": request["request_id"], "run_id": run_id,
                               "pid": pid, "keys": keys, "payload_sha256": hashlib.sha256(payload).hexdigest(),
                               "owner": "run_bound_pty"}
                except Exception as error:
                    receipt = {"ok": False, "request_id": request.get("request_id", "") if 'request' in locals() else "",
                               "error": str(error)}
                client.sendall((json.dumps(receipt, sort_keys=True) + "\n").encode())
    finally:
        listener.close(); endpoint.unlink(missing_ok=True); transcript.close()


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--broker", action="store_true")
    parser.add_argument("--master-fd", type=int)
    parser.add_argument("--transcript")
    parser.add_argument("--endpoint")
    parser.add_argument("--run-id")
    parser.add_argument("--pid", type=int)
    args = parser.parse_args()
    if args.broker:
        raise SystemExit(_broker(args.master_fd, Path(args.transcript), Path(args.endpoint), args.run_id, args.pid))


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
