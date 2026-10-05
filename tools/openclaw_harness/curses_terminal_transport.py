"""Harness-owned POSIX terminal transport for an interactive curses child.

The detached broker and every input request are bound to one host, run, and
native process generation. The transport owns only the terminal; semantic
actions continue to use the native surface boundary.
"""

from __future__ import annotations

from dataclasses import dataclass
import errno
import hashlib
import json
import os
from pathlib import Path
import re
import select
import signal
import socket
import stat
import subprocess
import sys
import tempfile
import threading
import time
import uuid
from typing import IO, Any, Mapping


OWNER_SCHEMA = "caol-curses-terminal-owner-v1"
REQUEST_SCHEMA = "caol-curses-terminal-request-v1"


def _host_identity() -> str:
    return socket.gethostname().strip()


def _process_generation(pid: int) -> dict[str, Any]:
    """Return the existing harness process-generation vocabulary for one PID."""
    # This module is also the detached broker entrypoint. Importing the small
    # inspector module avoids importing startup_harness and its application
    # dependencies into that long-lived helper.
    from certification_process_lease import SystemProcessInspector

    observed = SystemProcessInspector().inspect(int(pid))
    # macOS and POSIX kill(pid, 0) report zombies as present until their
    # parent reaps them. The startup harness treats those as exited; mirror
    # that distinction here without importing its much larger module.
    if observed.alive and os.name == "posix":
        try:
            state = subprocess.run(
                ["ps", "-p", str(int(pid)), "-o", "stat="],
                capture_output=True, text=True, check=False, timeout=2.0,
            ).stdout.strip()
            if state.startswith("Z"):
                observed = type(observed)(pid=observed.pid, alive=False,
                    executable_path=observed.executable_path,
                    birth_identity=observed.birth_identity, command=observed.command)
        except (OSError, subprocess.TimeoutExpired):
            pass
    return {
        "schema": "caol-owned-process-generation-v1",
        "pid": int(pid),
        "alive": bool(observed.alive),
        "birth_identity": str(observed.birth_identity or "").strip(),
        "command": str(observed.command or "").strip(),
        "executable_path": str(observed.executable_path or "").strip(),
    }


def _same_generation(expected: Mapping[str, Any], observed: Mapping[str, Any]) -> bool:
    try:
        pid_matches = int(expected.get("pid", 0)) == int(observed.get("pid", 0)) > 0
    except (TypeError, ValueError):
        return False
    birth = str(expected.get("birth_identity", "")).strip()
    command = str(expected.get("command", "")).strip()
    return bool(
        pid_matches and birth and command
        and birth == str(observed.get("birth_identity", "")).strip()
        and command == str(observed.get("command", "")).strip()
    )


def _stable_generation(pid: int, *, timeout: float = 2.0) -> dict[str, Any]:
    """Wait for exec and the inspector to report the same full identity twice."""
    deadline = time.monotonic() + timeout
    previous: dict[str, Any] | None = None
    last = _process_generation(pid)
    while last.get("alive") and time.monotonic() < deadline:
        command = str(last.get("command", "")).strip()
        executable = str(last.get("executable_path", "")).strip()
        if command and executable and not re.fullmatch(r"\([^)]*\)", command):
            if previous is not None and _same_generation(previous, last) and \
                    previous.get("executable_path") == last.get("executable_path"):
                return last
            previous = last
        else:
            previous = None
        time.sleep(0.03)
        last = _process_generation(pid)
    return last


def _read_owner(endpoint: Path, owner_path: Path | None = None) -> dict[str, Any]:
    try:
        path = owner_path or (endpoint.parent / "owner.json")
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise RuntimeError("terminal dispatcher owner record is unavailable") from error
    if not isinstance(value, dict) or value.get("schema") != OWNER_SCHEMA:
        raise RuntimeError("terminal dispatcher owner record is invalid")
    return value


def _write_json_atomic(path: Path, value: Mapping[str, Any]) -> None:
    temporary = path.with_name(path.name + ".tmp-" + uuid.uuid4().hex)
    temporary.write_text(json.dumps(dict(value), ensure_ascii=False, sort_keys=True), encoding="utf-8")
    os.chmod(temporary, 0o600)
    os.replace(temporary, path)


def _create_json_exclusive(path: Path, value: Mapping[str, Any]) -> tuple[int, int]:
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    info = os.fstat(fd)
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as sink:
            json.dump(dict(value), sink, ensure_ascii=False, sort_keys=True)
            sink.flush()
            os.fsync(sink.fileno())
    except BaseException:
        try:
            current = path.lstat()
            if (current.st_dev, current.st_ino) == (info.st_dev, info.st_ino):
                path.unlink()
        except OSError:
            pass
        raise
    return info.st_dev, info.st_ino


@dataclass
class CursesTerminalTransport:
    """A controlling PTY and an immutable transcript for one child process."""

    master_fd: int
    transcript_path: Path
    _transcript: IO[bytes]
    _reader: threading.Thread | None = None
    _broker: subprocess.Popen[bytes] | None = None
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
        """Close this launcher's handles; never terminate a detached live owner."""
        if self._broker is not None and self._broker.poll() is None:
            # The detached broker owns the PTY until its exact game generation
            # exits. A caller closing its launcher object does not own that
            # process and therefore cannot terminate it.
            self._broker = None
        if self.master_fd >= 0:
            try:
                os.close(self.master_fd)
            except OSError:
                pass
            self.master_fd = -1
        if self._reader is not None:
            self._reader.join(timeout=2)
        if not self._transcript.closed:
            self._transcript.close()

    def hand_off_to_dispatcher(
        self, *, run_id: str, pid: int,
        process_generation: Mapping[str, Any] | None = None,
        lease_fd: int | None = None,
        lease_fds: tuple[int, ...] = (),
    ) -> Path:
        """Give a detached, run-bound broker ownership of this PTY master.

        ``lease_fds`` inherits the existing writable-profile file and directory
        locks; ``lease_fd`` retains compatibility with single-descriptor callers.
        The game must inherit the same descriptors at its own launch boundary.
        """
        if self.master_fd < 0:
            raise RuntimeError("terminal master is already handed off")
        if not run_id.strip() or pid <= 0:
            raise ValueError("terminal dispatcher needs a nonempty run ID and positive PID")
        observed_game = _stable_generation(pid)
        expected_game = dict(process_generation or observed_game)
        if not _same_generation(expected_game, observed_game):
            raise RuntimeError("terminal game process generation changed before handoff")
        host = _host_identity()
        inherited_leases = tuple(dict.fromkeys((*lease_fds, *((lease_fd,) if lease_fd is not None else ()))))
        if any(not isinstance(fd, int) or fd < 0 for fd in inherited_leases):
            raise ValueError("lease descriptors must be open file descriptors")
        for fd in inherited_leases:
            os.fstat(fd)
        private_dir = Path(tempfile.mkdtemp(prefix="caol-pty-", dir="/tmp"))
        os.chmod(private_dir, 0o700)
        endpoint = private_dir / "input.sock"
        private_owner = private_dir / "owner.json"
        run_owner = self.transcript_path.parent / "terminal.owner.json"
        initial_owner = {
            "schema": OWNER_SCHEMA, "host": host, "run_id": run_id,
            "game_pid": pid, "game_process_generation": expected_game,
            "endpoint": str(endpoint), "run_owner_path": str(run_owner),
            "lease_fd_inherited": bool(inherited_leases), "lease_fd_count": len(inherited_leases),
        }
        try:
            run_owner_identity = _create_json_exclusive(run_owner, initial_owner)
        except FileExistsError as error:
            private_dir.rmdir()
            raise RuntimeError("terminal owner record already exists for this run") from error
        try:
            _write_json_atomic(private_owner, initial_owner)
        except BaseException:
            try:
                current = run_owner.lstat()
                if (current.st_dev, current.st_ino) == run_owner_identity:
                    run_owner.unlink()
            except OSError:
                pass
            private_dir.rmdir()
            raise
        command = [
            sys.executable, str(Path(__file__).resolve()), "--broker",
            "--master-fd", str(self.master_fd), "--transcript", str(self.transcript_path),
            "--endpoint", str(endpoint), "--owner", str(private_owner),
            "--run-owner", str(run_owner),
        ]
        for fd in inherited_leases:
            command.extend(["--lease-fd", str(fd)])
        pass_fds = (self.master_fd, *inherited_leases)
        try:
            broker = subprocess.Popen(
                command, pass_fds=pass_fds, start_new_session=True,
                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
            )
        except BaseException:
            private_owner.unlink(missing_ok=True)
            private_dir.rmdir()
            try:
                current = run_owner.lstat()
                if (current.st_dev, current.st_ino) == run_owner_identity:
                    run_owner.unlink()
            except OSError:
                pass
            raise
        self._broker = broker
        self._broker_endpoint = endpoint
        deadline = time.monotonic() + 3.0
        while time.monotonic() < deadline:
            if endpoint.exists():
                broker_generation = _process_generation(broker.pid)
                if not broker_generation.get("alive") or not broker_generation.get("birth_identity") or not broker_generation.get("command"):
                    raise RuntimeError("terminal dispatcher process identity is unavailable")
                owner = {
                    "schema": OWNER_SCHEMA,
                    "host": host,
                    "run_id": run_id,
                    "game_pid": pid,
                    "game_process_generation": expected_game,
                    "broker_pid": broker.pid,
                    "broker_process_generation": broker_generation,
                    "broker_command": command,
                    "endpoint": str(endpoint),
                    "run_owner_path": str(run_owner),
                    "lease_fd_inherited": bool(inherited_leases), "lease_fd_count": len(inherited_leases),
                }
                _write_json_atomic(private_owner, owner)
                _write_json_atomic(run_owner, owner)
                self._transcript.close()
                os.close(self.master_fd)
                self.master_fd = -1
                return endpoint
            if broker.poll() is not None:
                break
            time.sleep(0.01)
        # The game may have exited during detached startup. Leave cleanup to
        # the broker and report the unavailable endpoint without killing it.
        raise RuntimeError("terminal dispatcher did not create its run-bound endpoint")


def dispatch_input(
    endpoint: Path, *, run_id: str, pid: int, keys: list[str], delay_ms: int = 0,
    host: str | None = None, process_generation: Mapping[str, Any] | None = None,
    request_id: str | None = None,
) -> dict[str, Any]:
    """Deliver one exact request after reconciling its persisted process owner."""
    if os.name == "nt":
        from windows_curses_terminal_transport import dispatch_input as native_dispatch
        return native_dispatch(endpoint, run_id=run_id, pid=pid, keys=keys, delay_ms=delay_ms,
            host=host, process_generation=process_generation, request_id=request_id)
    endpoint = Path(endpoint)
    owner = _read_owner(endpoint)
    actual_host = _host_identity() if host is None else str(host)
    expected_game = owner.get("game_process_generation")
    if not isinstance(expected_game, Mapping):
        raise RuntimeError("terminal dispatcher lacks a game process generation")
    observed = _process_generation(int(owner.get("game_pid", 0) or 0))
    if owner.get("host") != actual_host or str(owner.get("run_id", "")) != run_id or int(owner.get("game_pid", 0) or 0) != pid:
        raise RuntimeError("terminal dispatcher rejected host, run, or PID identity")
    if not observed.get("alive") or not _same_generation(expected_game, observed):
        raise RuntimeError("terminal game process generation is stale or unavailable")
    if process_generation is not None and not _same_generation(process_generation, expected_game):
        raise RuntimeError("terminal dispatcher rejected expected process generation")
    request_identity = str(request_id or uuid.uuid4().hex).strip()
    if not request_identity:
        raise ValueError("terminal request ID must not be empty")
    request = {
        "schema": REQUEST_SCHEMA, "request_id": request_identity,
        "host": actual_host, "run_id": run_id, "pid": pid,
        "process_generation": dict(expected_game), "keys": keys,
        "delay_ms": max(0, int(delay_ms)),
    }
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


def dispatcher_status(endpoint: Path, *, owner_path: Path | None = None) -> dict[str, Any]:
    """Inspect persisted game and broker identities without mutating either."""
    if os.name == "nt":
        from windows_curses_terminal_transport import dispatcher_status as native_status
        return native_status(endpoint, owner_path=owner_path)
    endpoint = Path(endpoint)
    owner = _read_owner(endpoint, owner_path)
    if str(owner.get("endpoint", "")) != str(endpoint):
        return {"schema": OWNER_SCHEMA, "status": "endpoint_identity_mismatch", "endpoint": str(endpoint)}
    game = _process_generation(int(owner.get("game_pid", 0) or 0))
    broker = _process_generation(int(owner.get("broker_pid", 0) or 0))
    return {
        "schema": OWNER_SCHEMA,
        "host": owner.get("host"),
        "run_id": owner.get("run_id"),
        "endpoint": str(endpoint),
        "endpoint_present": endpoint.exists(),
        "game_status": "alive" if game.get("alive") and _same_generation(owner.get("game_process_generation", {}), game)
        else "identity_changed_or_exited",
        "broker_status": "alive" if broker.get("alive") and _same_generation(owner.get("broker_process_generation", {}), broker)
        else "identity_changed_or_exited",
        "owner": owner,
    }


def cleanup_dispatcher(
    endpoint: Path, *, timeout: float = 2.0, owner_path: Path | None = None,
) -> dict[str, Any]:
    """Wait for natural exact-child cleanup; retain live or ambiguous owners."""
    if os.name == "nt":
        from windows_curses_terminal_transport import cleanup_dispatcher as native_cleanup
        return native_cleanup(endpoint, timeout=timeout, owner_path=owner_path)
    endpoint = Path(endpoint)
    try:
        owner = _read_owner(endpoint, owner_path)
    except RuntimeError:
        return {"status": "endpoint_absent_owner_unavailable", "endpoint": str(endpoint)}
    if str(owner.get("endpoint", "")) != str(endpoint):
        return {"status": "retained_endpoint_identity_mismatch", "endpoint": str(endpoint)}
    if owner.get("host") != _host_identity():
        return {"status": "retained_wrong_host", "endpoint": str(endpoint)}
    game = _process_generation(int(owner.get("game_pid", 0) or 0))
    expected_game = owner.get("game_process_generation", {})
    if game.get("alive"):
        if _same_generation(expected_game, game):
            return {"status": "retained_live_game", "endpoint": str(endpoint), "pid": game["pid"]}
        return {"status": "retained_ambiguous_game_identity", "endpoint": str(endpoint), "pid": game["pid"]}
    broker_pid = int(owner.get("broker_pid", 0) or 0)
    expected_broker = owner.get("broker_process_generation", {})
    deadline = time.monotonic() + max(0.0, timeout)
    while time.monotonic() < deadline:
        broker = _process_generation(broker_pid)
        if not broker.get("alive"):
            return _cleanup_result(endpoint, broker_pid)
        if not _same_generation(expected_broker, broker):
            return {"status": "retained_ambiguous_broker_identity", "endpoint": str(endpoint), "pid": broker_pid}
        time.sleep(0.02)
    # Request graceful broker shutdown only after the exact game PID is gone.
    # Its handler still checks game identity and will retain a live owner.
    try:
        os.kill(broker_pid, signal.SIGTERM)
    except ProcessLookupError:
        return _cleanup_result(endpoint, broker_pid)
    deadline = time.monotonic() + max(0.0, timeout)
    while time.monotonic() < deadline:
        broker = _process_generation(broker_pid)
        if not broker.get("alive"):
            return _cleanup_result(endpoint, broker_pid)
        if not _same_generation(expected_broker, broker):
            return {"status": "retained_ambiguous_broker_identity", "endpoint": str(endpoint), "pid": broker_pid}
        time.sleep(0.02)
    return {"status": "broker_exit_unobserved", "endpoint": str(endpoint), "pid": broker_pid}


def _cleanup_result(endpoint: Path, broker_pid: int) -> dict[str, Any]:
    """Report natural broker cleanup, including any exact private residue."""
    private_dir = endpoint.parent
    if private_dir.exists():
        try:
            retained = sorted(str(path) for path in private_dir.iterdir())
        except OSError as error:
            retained = [f"{private_dir} (listing failed: {error})"]
        return {
            "status": "private_directory_retained_after_game_exit",
            "endpoint": str(endpoint), "endpoint_present": endpoint.exists(),
            "private_directory": str(private_dir), "retained_paths": retained,
            "pid": broker_pid,
        }
    return {"status": "cleaned_after_game_exit", "endpoint_present": endpoint.exists(),
            "private_directory": str(private_dir), "pid": broker_pid}


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


def _broker(
    master_fd: int, transcript_path: Path, endpoint: Path, owner_path: Path,
    run_owner_path: Path, lease_fds: list[int],
) -> int:
    owner = _read_owner(endpoint, owner_path)
    run_id = str(owner.get("run_id", ""))
    host = str(owner.get("host", ""))
    pid = int(owner.get("game_pid", 0) or 0)
    game_generation = owner.get("game_process_generation", {})
    if not isinstance(game_generation, Mapping):
        raise RuntimeError("terminal broker owner lacks the game generation")
    transcript = transcript_path.open("ab")
    listener = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    bound_identity: tuple[int, int] | None = None
    processed_requests: dict[str, dict[str, Any]] = {}
    processed_fingerprints: dict[str, str] = {}
    shutdown_requested = False

    def request_shutdown(_signum: int, _frame: Any) -> None:
        nonlocal shutdown_requested
        shutdown_requested = True

    try:
        signal.signal(signal.SIGTERM, request_shutdown)
        listener.bind(str(endpoint))
        os.chmod(endpoint, 0o600)
        listener.listen(4)
        listener.settimeout(0.05)
        info = endpoint.lstat()
        bound_identity = (info.st_dev, info.st_ino)
        child_exit_drain_deadline: float | None = None
        while True:
            observed = _process_generation(pid)
            if not observed.get("alive") and child_exit_drain_deadline is None:
                child_exit_drain_deadline = time.monotonic() + 1.0
            child_is_exact = bool(observed.get("alive") and _same_generation(game_generation, observed))
            if observed.get("alive") and not child_is_exact:
                # A live PID with a different generation is ambiguous. Keep
                # the broker and endpoint for explicit owner reconciliation.
                time.sleep(0.05)
                continue
            readable, _, _ = select.select([master_fd, listener], [], [], 0.05)
            if master_fd in readable:
                try:
                    data = os.read(master_fd, 65536)
                    if data:
                        transcript.write(data)
                        transcript.flush()
                    elif not observed.get("alive"):
                        return 0
                except OSError as error:
                    if error.errno == errno.EIO:
                        if not observed.get("alive"):
                            return 0
                        time.sleep(0.05)
                        continue
            elif not observed.get("alive"):
                if time.monotonic() >= (child_exit_drain_deadline or 0):
                    return 0
                time.sleep(0.02)
                continue
            if not observed.get("alive"):
                # Once the exact child has exited, drain buffered terminal
                # output through PTY EOF before removing our private endpoint.
                continue
            if not child_is_exact:
                continue
            if listener not in readable:
                continue
            client, _ = listener.accept()
            with client:
                raw_buffer = bytearray()
                while b"\n" not in raw_buffer and len(raw_buffer) <= 65536:
                    chunk = client.recv(4096)
                    if not chunk:
                        break
                    raw_buffer.extend(chunk)
                raw = bytes(raw_buffer).split(b"\n", 1)[0]
                try:
                    request = json.loads(raw.decode())
                    if request.get("schema") != REQUEST_SCHEMA:
                        raise ValueError("request_schema_mismatch")
                    if request.get("host") != host or request.get("run_id") != run_id or int(request.get("pid", 0)) != pid:
                        raise ValueError("host_run_or_pid_mismatch")
                    if not _same_generation(game_generation, request.get("process_generation", {})):
                        raise ValueError("process_generation_mismatch")
                    current = _process_generation(pid)
                    if not current.get("alive") or not _same_generation(game_generation, current):
                        raise ValueError("game_process_generation_stale")
                    request_id = str(request.get("request_id", "")).strip()
                    if not request_id:
                        raise ValueError("request_id_missing")
                    previous = processed_requests.get(request_id)
                    if previous is not None:
                        fingerprint = hashlib.sha256(json.dumps(
                            {"keys": request.get("keys", []),
                             "delay_ms": max(0, int(request.get("delay_ms", 0)))},
                            sort_keys=True, separators=(",", ":"),
                        ).encode()).hexdigest()
                        if fingerprint != processed_fingerprints.get(request_id):
                            receipt = {"ok": False, "request_id": request_id,
                                       "error": "duplicate_request_payload_mismatch"}
                            try:
                                client.sendall((json.dumps(receipt, sort_keys=True) + "\n").encode())
                            except OSError:
                                pass
                            continue
                        receipt = dict(previous, duplicate_request=True)
                        try:
                            client.sendall((json.dumps(receipt, sort_keys=True) + "\n").encode())
                        except OSError:
                            pass
                        continue
                    keys = [str(key) for key in request.get("keys", [])]
                    payload = _key_bytes(keys)
                    delay = max(0, int(request.get("delay_ms", 0))) / 1000
                    # The broker accepts and completes one client at a time;
                    # this loop is the per-run input serialization boundary.
                    if delay and len(keys) > 1:
                        for index, key in enumerate(keys):
                            if index:
                                time.sleep(delay)
                            os.write(master_fd, _key_bytes([key]))
                    else:
                        os.write(master_fd, payload)
                    receipt = {"ok": True, "request_id": request_id, "host": host,
                               "run_id": run_id, "pid": pid,
                               "process_generation": dict(game_generation), "keys": keys,
                               "payload_sha256": hashlib.sha256(payload).hexdigest(),
                               "owner": "run_bound_pty"}
                    processed_requests[request_id] = receipt
                    processed_fingerprints[request_id] = hashlib.sha256(json.dumps(
                        {"keys": keys, "delay_ms": max(0, int(request.get("delay_ms", 0)))},
                        sort_keys=True, separators=(",", ":"),
                    ).encode()).hexdigest()
                except Exception as error:
                    receipt = {"ok": False, "request_id": request.get("request_id", "") if 'request' in locals() else "",
                               "error": str(error)}
                try:
                    client.sendall((json.dumps(receipt, sort_keys=True) + "\n").encode())
                except OSError:
                    # The caller can reconnect with this request ID and
                    # recover its receipt without replaying terminal bytes.
                    continue
    finally:
        listener.close()
        transcript.close()
        try:
            os.close(master_fd)
        except OSError:
            pass
        try:
            info = endpoint.lstat()
            if bound_identity == (info.st_dev, info.st_ino) and stat.S_ISSOCK(info.st_mode):
                endpoint.unlink()
        except OSError:
            pass
        # The private owner copy may be removed only when its persisted run,
        # game, endpoint and broker generations identify this exact broker.
        try:
            private_info = owner_path.lstat()
            current_owner = _read_owner(endpoint, owner_path)
            broker_generation = current_owner.get("broker_process_generation", {})
            current_broker = _process_generation(os.getpid())
            exact_owner = (
                stat.S_ISREG(private_info.st_mode)
                and current_owner.get("endpoint") == str(endpoint)
                and current_owner.get("host") == host
                and current_owner.get("run_id") == run_id
                and int(current_owner.get("game_pid", 0) or 0) == pid
                and _same_generation(game_generation, current_owner.get("game_process_generation", {}))
                and int(current_owner.get("broker_pid", 0) or 0) == os.getpid()
                and _same_generation(broker_generation, current_broker)
            )
            if exact_owner:
                owner_path.unlink()
        except (OSError, RuntimeError, TypeError, ValueError):
            pass
        try:
            if (endpoint.name == "input.sock" and endpoint.parent.name.startswith("caol-pty-")
                    and not owner_path.exists()):
                endpoint.parent.rmdir()
        except OSError:
            pass
        for fd in lease_fds:
            try:
                os.close(fd)
            except OSError:
                pass


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


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("--broker", action="store_true")
    parser.add_argument("--master-fd", type=int)
    parser.add_argument("--transcript")
    parser.add_argument("--endpoint")
    parser.add_argument("--owner")
    parser.add_argument("--run-owner")
    parser.add_argument("--lease-fd", type=int, action="append", default=[])
    args = parser.parse_args()
    if args.broker:
        raise SystemExit(_broker(
            args.master_fd, Path(args.transcript), Path(args.endpoint), Path(args.owner),
            Path(args.run_owner), args.lease_fd,
        ))
