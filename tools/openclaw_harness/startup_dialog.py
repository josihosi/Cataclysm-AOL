"""PID-bound observation and explicit recovery of a pre-World native debug dialog.

This is a player accessibility path, not a startup admission or gameplay proof
path.  A current window or owned terminal frame identifies the UI; a matching profile debug-log line
supplies exact diagnostic text.  Recovery is a separate, current-capture-bound
player command; observation alone never sends input.
"""
from __future__ import annotations

from datetime import datetime, timezone
from difflib import SequenceMatcher
from hashlib import sha256
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import uuid
from typing import Any, Callable, Mapping


PEEKABOO = "/opt/homebrew/bin/peekaboo"
BRIDGE_SOCKET = "/Users/josefhorvath/Library/Application Support/Peekaboo/bridge.sock"
LOG_TAIL_BYTES = 524288
MAX_CAPTURE_BYTES = 20 * 1024 * 1024
MAX_CAPTURE_FILES = 8
MAX_DIAGNOSTIC_LINE_CHARS = 2048
STARTUP_STATES = {"starting", "preparing", "process_dead", "bridge_failed"}
REENTRY_STARTUP_PHASE = "awaiting_declared_reentry_descriptor"
ERROR_LINE = re.compile(r"ERROR\s+:\s+(src/[^:\r\n]+):(\d+)\s+\[[^\]]*\]\s+(.+)")
WORKSPACE_ROOT = Path(__file__).resolve().parents[2]


def pre_world_startup_phase(status: Mapping[str, Any]) -> str | None:
    """Identify the exact predescriptor phase that can own a PID-bound UI view."""
    if status.get("state") == "transitioning" and status.get("phase") == REENTRY_STARTUP_PHASE:
        generation = status.get("session_generation")
        return "declared_reentry" if type(generation) is int and generation >= 0 else None
    if (status.get("state") in STARTUP_STATES and not status.get("session_descriptor")
            and status.get("session_generation", 0) == 0):
        return "initial_startup"
    return None


def _read_object(path: Path, limit: int = 65536) -> dict[str, Any]:
    try:
        if path.stat().st_size > limit:
            return {}
        value = json.loads(path.read_text(encoding="utf-8"))
        return dict(value) if isinstance(value, Mapping) else {}
    except (OSError, UnicodeError, ValueError):
        return {}


def _debug_log_path(owner: Mapping[str, Any], session: Path) -> Path | None:
    """Resolve relative userdir against the game's owned cwd, never its binary."""
    try:
        command = shlex.split(str(owner["command"]))
        userdir = Path(command[command.index("--userdir") + 1])
        if not userdir.is_absolute():
            raw_cwd = str(owner.get("launch_cwd", "")).strip()
            if raw_cwd:
                recorded_cwd = Path(raw_cwd)
                if not recorded_cwd.is_absolute():
                    return None
                cwd = recorded_cwd.resolve()
            else:
                # Older game-process records predate launch_cwd. Their bridge
                # session is still tied to this checkout, whose launcher uses
                # repo_root() as the child's cwd in both native routes.
                sessions = WORKSPACE_ROOT / ".userdata" / "openclaw_harness" / "bridge-sessions"
                if not Path(session).resolve().is_relative_to(sessions.resolve()):
                    return None
                cwd = WORKSPACE_ROOT
            userdir = cwd / userdir
        userdir = userdir.resolve()
        published = owner["log_paths"]["profile_diagnostic_debug"]
        path = Path(str(published["path"]))
        if not path.is_absolute():
            return None
        path = path.resolve()
        if published.get("scope") != "profile_shared" or path != userdir / "config" / "debug.log":
            return None
        return path
    except (KeyError, IndexError, TypeError, ValueError, AttributeError):
        return None


def debug_log_clues(path: Path | None) -> list[dict[str, Any]]:
    """Return recent distinct exact ERROR lines with byte offsets, bounded by a tail read."""
    if path is None:
        return []
    try:
        with path.open("rb") as source:
            size = source.seek(0, os.SEEK_END)
            start = max(0, size - LOG_TAIL_BYTES)
            source.seek(start)
            body = source.read(LOG_TAIL_BYTES)
    except OSError:
        return []
    offset = start
    entries: list[dict[str, Any]] = []
    for index, line in enumerate(body.splitlines(keepends=True)):
        current = offset
        offset += len(line)
        if index == 0 and start > 0:
            continue  # The bounded tail may start in the middle of an old line.
        decoded = line.decode("utf-8", errors="replace").rstrip("\r\n")
        match = ERROR_LINE.search(decoded)
        if not match:
            continue
        if len(decoded) > MAX_DIAGNOSTIC_LINE_CHARS:
            continue  # The byte offset still identifies the complete raw line.
        entry = {"message": match.group(3).strip(), "source_file": match.group(1),
                 "source_line": int(match.group(2)), "log_path": str(path),
                 "log_byte_offset": current, "raw_line": decoded}
        if entry["message"]:
            entries.append(entry)
    distinct: dict[tuple[str, int, str], dict[str, Any]] = {}
    for entry in entries:
        distinct[(entry["source_file"], entry["source_line"], entry["message"])] = entry
    return list(distinct.values())[-8:]


def _normal(value: str) -> str:
    return re.sub(r"\s+", " ", str(value).casefold()).strip(" .:\t\r\n")


def _visible_debug_text(lines: list[str]) -> tuple[bool, str]:
    joined = " ".join(_normal(line) for line in lines)
    header = "an error has occur" in joined
    start = next((i for i, line in enumerate(lines)
                  if re.search(r"\bdebug\s*[:|]", line, re.IGNORECASE)), None)
    if not header or start is None:
        return False, ""
    first = re.split(r"\bDEBUG\s*[:|]\s*", lines[start], maxsplit=1, flags=re.IGNORECASE)[-1]
    parts = [first]
    for line in lines[start + 1:start + 4]:
        if re.search(r"REPORTING\s+FUNCTION|C\+\+\s+SOURCE|^LINE\b|^VERSION\b", line,
                     re.IGNORECASE):
            break
        parts.append(line)
    return True, " ".join(part.strip() for part in parts if part.strip()).strip()


def correlate_visible_dialog(lines: list[str], clues: list[dict[str, Any]]) -> dict[str, Any]:
    """Match current visible DEBUG text to one exact log line without a message allowlist."""
    visible, ocr_message = _visible_debug_text(lines)
    if not visible:
        excerpt = " ".join(lines[:5]).strip()
        state = "other_surface" if excerpt and "loading" not in _normal(excerpt) else "loading_unknown"
        return {"state": state, "ocr_excerpt": excerpt[:500],
                "reason": "no_current_debug_dialog_confirmed"}
    scores = sorted(((SequenceMatcher(None, _normal(ocr_message), _normal(entry["message"])).ratio(),
                      entry) for entry in clues), key=lambda item: item[0], reverse=True)
    result: dict[str, Any] = {"state": "debug_dialog_unmatched", "ocr_excerpt": ocr_message[:500],
                              "reason": "visible_debug_text_has_no_unambiguous_log_match"}
    if scores and scores[0][0] >= 0.72 and (len(scores) == 1 or scores[0][0] - scores[1][0] >= 0.08):
        result.update(state="confirmed_debug_dialog", **scores[0][1],
                      message_source="current_window_ocr_matched_to_exact_profile_debug_log")
        result.pop("reason", None)
    return result


def _command(args: list[str], *, timeout: float,
             runner: Callable[..., Any]) -> dict[str, Any]:
    try:
        completed = runner(args, capture_output=True, text=True, check=False, timeout=timeout)
        try:
            value = json.loads(completed.stdout)
        except (TypeError, ValueError):
            value = {}
        if not isinstance(value, dict):
            value = {}
        if completed.returncode != 0 or (args[0] == PEEKABOO and value.get("success") is not True) or \
                value.get("ok") is False:
            failure = value.get("error")
            code = value.get("code") or (failure.get("code") if isinstance(failure, dict) else failure)
            detail = value.get("message") or (failure.get("message") if isinstance(failure, dict) else "")
            return {"error": str(code or "command_failed"),
                    "detail": str(detail or completed.stderr).strip()[:300]}
        logs = value.get("debug_logs", [])
        if isinstance(logs, list) and any(
                "Runtime host: local" in str(line) or "local (in-process" in str(line)
                for line in logs):
            return {"error": "unauthorized_local_capture_host"}
        return value
    except (OSError, subprocess.TimeoutExpired, ValueError, TypeError) as error:
        return {"error": type(error).__name__, "detail": str(error)[:300]}


def _peekaboo_args(*args: str) -> list[str]:
    return [PEEKABOO, *args, "--bridge-socket", BRIDGE_SOCKET, "--json"]


def _terminal_context(owner: Mapping[str, Any], snapshot: Callable[[int], Mapping[str, Any]]) -> dict[str, Any]:
    """Reconcile the run transcript and both copies of its live broker owner."""
    from startup_harness import process_generation_matches
    from curses_terminal_transport import OWNER_SCHEMA, _host_identity
    try:
        event = owner["log_paths"]["native_semantic_events"]
        event_path = Path(event["path"])
        userdir = Path(owner["userdir"]).resolve()
        if event.get("scope") != "run_bound" or not event_path.is_absolute():
            raise ValueError("unbound_terminal_run")
        run_dir = event_path.resolve().parent
        if not run_dir.is_relative_to(userdir / "harness_runs"):
            raise ValueError("unbound_terminal_run")
        run_owner_path = run_dir / "terminal.owner.json"
        terminal = _read_object(run_owner_path)
        endpoint = Path(terminal["endpoint"])
        private = _read_object(endpoint.parent / "owner.json")
        game = owner["process_generation"]
        broker = terminal["broker_process_generation"]
        command = terminal["broker_command"]
        transcript = run_dir / "game.terminal.log"
        observed_broker = snapshot(int(broker["pid"]))
        observed_game = snapshot(int(owner["pid"]))
        if (not observed_broker.get("alive") or not observed_game.get("alive")
                or terminal != private or terminal.get("schema") != OWNER_SCHEMA
                or terminal.get("host") != _host_identity() or owner.get("host") != _host_identity()
                or terminal.get("run_id") != owner.get("run_id")
                or terminal.get("game_pid") != owner.get("pid")
                or not process_generation_matches(game, terminal["game_process_generation"])
                or terminal.get("run_owner_path") != str(run_owner_path)
                or not endpoint.is_absolute() or not endpoint.exists()
                or not isinstance(command, list) or "--broker" not in command
                or command[command.index("--transcript") + 1] != str(transcript)
                or command[command.index("--endpoint") + 1] != str(endpoint)
                or command[command.index("--run-owner") + 1] != str(run_owner_path)
                or command[command.index("--owner") + 1] != str(endpoint.parent / "owner.json")
                or terminal.get("broker_pid") != broker.get("pid")
                or not process_generation_matches(broker, observed_broker)
                or not process_generation_matches(game, observed_game)):
            raise ValueError("terminal_owner_identity_mismatch")
        # The inspected broker command must identify the recorded argument list;
        # Python may resolve its executable symlink in ps, so compare the tail.
        if shlex.split(str(broker.get("command", "")))[1:] != command[1:]:
            raise ValueError("terminal_broker_command_mismatch")
        return {"run_dir": run_dir, "transcript": transcript, "owner": terminal}
    except (KeyError, IndexError, TypeError, ValueError, OSError, AttributeError) as error:
        raise ValueError("terminal_owner_unavailable_or_changed") from error


def _terminal_permission(context: Mapping[str, Any]) -> dict[str, Any]:
    """Allow startup Ignore only from the exact selected declaration bytes."""
    preflight = _read_object(context["run_dir"] / "contract.preflight.json", 2 * 1024 * 1024)
    try:
        source = preflight["scenario_source"]
        path = Path(source["path"])
        if not path.is_absolute() or path.stat().st_size > 2 * 1024 * 1024:
            raise ValueError("invalid_declaration_path")
        raw = path.read_bytes()
        declaration = json.loads(raw)
        contract = declaration["runtime_contract"]
        permitted = contract["permitted_input"]
        if (declaration.get("name") != preflight.get("scenario")
                or permitted.count("debug:ignore") != 1
                or "debug:ignore" in contract.get("forbidden_input", [])
                or "keyboard" not in contract.get("forbidden_input", [])
                or "keyboard" in permitted or contract.get("setup_only_debug") is not True
                or contract.get("playtest_mode") != "terminal"
                or contract.get("terminal_transport") != "pty"):
            raise ValueError("startup_debug_ignore_not_permitted")
        digest = sha256(raw).hexdigest()
        if digest != source["sha256"]:
            raise ValueError("selected_declaration_hash_changed")
        return {"permission": "debug:ignore", "scope": "startup_only",
                "path": str(path), "sha256": digest,
                "selected_declaration_sha256": source["sha256"]}
    except (KeyError, TypeError, ValueError, OSError, AttributeError) as error:
        raise ValueError("startup_debug_ignore_permission_unavailable") from error


def _observe_terminal_dialog(session: Path, binding_id: str, status: Mapping[str, Any],
                             owner: Mapping[str, Any], result: dict[str, Any],
                             snapshot: Callable[[int], Mapping[str, Any]]) -> dict[str, Any]:
    from startup_harness import render_curses_terminal_screen, process_generation_matches
    try:
        if status.get("state") not in {"starting", "preparing", "transitioning"}:
            raise ValueError("terminal_recovery_requires_live_startup")
        bridge = status.get("bridge_process_generation", {})
        observed_bridge = snapshot(int(bridge.get("pid", 0))) if bridge.get("pid") else {}
        if (status.get("bridge_pid") != bridge.get("pid") or not bridge.get("pid")
                or not bridge.get("alive") or not observed_bridge.get("alive")
                or not process_generation_matches(bridge, observed_bridge)):
            raise ValueError("startup_bridge_identity_unavailable_or_changed")
        context = _terminal_context(owner, snapshot)
        path = context["transcript"]
        # Read the complete append-only stream: a tail can leave a prior modal
        # painted. The renderer retains only current cells.
        with path.open("rb") as stream:
            identity = os.fstat(stream.fileno())
            raw = stream.read()
            current = os.fstat(stream.fileno())
        if (identity.st_dev, identity.st_ino, identity.st_size) != \
                (current.st_dev, current.st_ino, current.st_size) or path.stat().st_ino != identity.st_ino:
            raise ValueError("terminal_transcript_changed_during_capture")
        lines = render_curses_terminal_screen(raw.decode("utf-8", errors="replace"))
        terminal = context["owner"]
        frame = {"binding_id": binding_id, "run_id": owner["run_id"],
                 "process_generation": owner["process_generation"],
                 "session_generation": status.get("session_generation", 0),
                 "startup_phase": result["startup_phase"], "terminal_owner": terminal,
                 "bridge_process_generation": bridge,
                 "transcript_path": str(path), "transcript_sha256": sha256(raw).hexdigest(),
                 "transcript_bytes": len(raw), "lines": lines}
        digest = sha256(json.dumps(frame, sort_keys=True).encode()).hexdigest()
        if (_read_object(session / "game-process.json") != owner
                or _terminal_context(owner, snapshot)["owner"] != terminal):
            raise ValueError("terminal_owner_changed_during_capture")
        current_status = _read_object(session / "status.json")
        if (current_status.get("binding_id") != binding_id
                or pre_world_startup_phase(current_status) != result["startup_phase"]
                or current_status.get("session_generation", 0) != status.get("session_generation", 0)
                or current_status.get("state") != status.get("state")
                or current_status.get("bridge_pid") != status.get("bridge_pid")
                or current_status.get("bridge_process_generation") != bridge
                or not snapshot(int(bridge["pid"])).get("alive")):
            raise ValueError("startup_status_changed_during_capture")
        result.update(correlate_visible_dialog(lines, result["log_clues"]))
        # Terminal text has no OCR uncertainty: demand the exact message as
        # well as the existing unique log correlation.
        if result.get("state") == "confirmed_debug_dialog" and \
                _normal(_visible_debug_text(lines)[1]) != _normal(result["message"]):
            result.update(state="debug_dialog_unmatched", reason="terminal_warning_not_exact_log_match")
        advertised = bool(re.search(r"\bpress i\b[^\n]*\bignore\b", "\n".join(lines), re.I))
        if result.get("state") == "confirmed_debug_dialog" and not advertised:
            result.update(state="debug_dialog_unmatched", reason="current_terminal_ignore_not_advertised")
        captures = session / "startup-dialog-captures"
        captures.mkdir(parents=True, exist_ok=True)
        artifact = captures / (digest[:20] + ".terminal.json")
        if not artifact.exists():
            _record_recovery(artifact, frame)
        result.update(capture_kind="terminal", capture_sha256=digest, capture_path=str(artifact),
                      terminal_frame=frame, ignore_advertised=advertised,
                      message_source="current_terminal_frame_matched_to_exact_profile_debug_log")
        return result
    except (ValueError, OSError) as error:
        return {**result, "state": "unavailable", "reason": str(error)}


def observe_startup_dialog(
    session: Path, binding_id: str, status: Mapping[str, Any], *,
    snapshot: Callable[[int], Mapping[str, Any]] | None = None,
    runner: Callable[..., Any] = subprocess.run,
) -> dict[str, Any]:
    """Capture a fresh window or owned terminal frame for the exact startup PID."""
    result: dict[str, Any] = {"schema": "caol-startup-dialog-observation-v1",
                              "state": "unavailable", "reason": "startup_identity_unavailable",
                              "evidence_class": "startup_ui_only", "binding_id": binding_id,
                              "session": str(Path(session).resolve()), "log_clues": []}
    phase = pre_world_startup_phase(status)
    if status.get("binding_id") != binding_id or phase is None:
        return {**result, "reason": "not_bound_pre_world_startup"}
    result["startup_phase"] = phase
    owner = _read_object(Path(session) / "game-process.json")
    expected = owner.get("process_generation")
    if owner.get("binding_id") != binding_id or not isinstance(expected, Mapping):
        return result
    try:
        pid = int(owner.get("pid", 0))
    except (TypeError, ValueError):
        return result
    birth = str(expected.get("birth_identity", "")).strip()
    command = str(expected.get("command", "")).strip()
    if pid <= 0 or expected.get("pid") != pid or not birth or not command or command != owner.get("command"):
        return result
    result.update(pid=pid, birth_identity=birth, run_id=owner.get("run_id"),
                  command=command)
    log_path = _debug_log_path(owner, Path(session))
    clues = debug_log_clues(log_path)
    result["log_clues"] = clues
    result["log_path"] = str(log_path) if log_path else None
    from startup_harness import process_generation_matches, process_generation_snapshot
    process_snapshot = snapshot or process_generation_snapshot
    observed = process_snapshot(pid)
    if not observed.get("alive"):
        return {**result, "state": "exited", "reason": "owned_game_exited"}
    if not process_generation_matches(expected, observed):
        return {**result, "state": "identity_changed", "reason": "pid_birth_or_command_changed"}
    if owner.get("playtest_mode") == "terminal":
        return _observe_terminal_dialog(Path(session), binding_id, status, owner, result, process_snapshot)
    captures = Path(session) / "startup-dialog-captures"
    if captures.is_dir() and len(list(captures.glob("*.png"))) >= MAX_CAPTURE_FILES:
        return {**result, "reason": "capture_limit_reached"}
    windows = _command(_peekaboo_args("list", "windows", "--pid", str(pid)),
                       timeout=8, runner=runner)
    if "error" in windows:
        return {**result, "reason": "window_list_" + windows["error"],
                "capture_detail": windows.get("detail")}
    data = windows.get("data") if isinstance(windows.get("data"), dict) else {}
    candidates = [window for window in data.get("windows", []) if isinstance(window, dict)
                  and "cataclysm" in str(window.get("title", "")).casefold()
                  and not window.get("isMinimized", False)
                  and (window.get("pid") in (None, pid))]
    if len(candidates) != 1:
        return {**result, "reason": "missing_or_ambiguous_pid_window"}
    window = candidates[0]
    window_id = window.get("window_id", window.get("windowID", window.get("id")))
    try:
        window_id = int(window_id)
    except (TypeError, ValueError):
        return {**result, "reason": "window_id_unavailable"}
    captures.mkdir(parents=True, exist_ok=True)
    temporary = captures / ("pending-" + uuid.uuid4().hex + ".png")
    image = _command(_peekaboo_args("image", "--pid", str(pid), "--window-id", str(window_id),
                                    "--mode", "window", "--path", str(temporary)),
                     timeout=20, runner=runner)
    if "error" in image or not temporary.is_file():
        temporary.unlink(missing_ok=True)
        return {**result, "reason": "window_capture_" + image.get("error", "missing_image"),
                "capture_detail": image.get("detail")}
    try:
        raw = temporary.read_bytes()
        if not raw or len(raw) > MAX_CAPTURE_BYTES:
            raise ValueError("capture_size_out_of_bounds")
        digest = sha256(raw).hexdigest()
        retained = captures / (digest[:20] + ".png")
        if retained.exists():
            temporary.unlink()
        else:
            os.replace(temporary, retained)
    except (OSError, ValueError):
        temporary.unlink(missing_ok=True)
        return {**result, "reason": "image_artifact_unavailable"}
    result.update(window_id=window_id, window_title=window.get("title"),
                  image_path=str(retained), image_sha256=digest)
    swift = shutil.which("swift") or "/usr/bin/swift"
    ocr = _command([swift, str(Path(__file__).with_name("ocr_image.swift")), "--image",
                    str(retained), "--region-of-interest", "0", "0.8", "0.8", "0.2"],
                   timeout=30, runner=runner)
    if "error" in ocr or ocr.get("ok") is not True:
        return {**result, "reason": "window_ocr_unavailable", "capture_detail": ocr.get("detail")}
    lines = [line[:1000] for line in ocr.get("lines", []) if isinstance(line, str)][:40]
    if not process_generation_matches(expected, process_snapshot(pid)):
        return {**result, "state": "identity_changed", "reason": "pid_changed_during_capture"}
    current = _read_object(Path(session) / "status.json")
    current_owner = _read_object(Path(session) / "game-process.json")
    if (current.get("binding_id") != binding_id or pre_world_startup_phase(current) != phase
            or current.get("session_generation", 0) != status.get("session_generation", 0)
            or current_owner.get("process_generation") != expected
            or current_owner.get("run_id") != owner.get("run_id")):
        return {**result, "state": "state_changed", "reason": "startup_status_changed_during_capture"}
    result.update(correlate_visible_dialog(lines, clues))
    return result


def _record_recovery(path: Path, value: Mapping[str, Any]) -> None:
    """Keep the pre-input attempt and eventual delivery as separate run artifacts."""
    with path.open("x", encoding="utf-8") as stream:
        json.dump(value, stream, ensure_ascii=False, sort_keys=True, indent=2)
        stream.write("\n")
        stream.flush()
        os.fsync(stream.fileno())


def recover_startup_debug_dialog(
    session: Path, binding_id: str, capture_sha256: str, note: str = "", *,
    snapshot: Callable[[int], Mapping[str, Any]] | None = None,
    runner: Callable[..., Any] = subprocess.run,
) -> dict[str, Any]:
    """Send one PID-bound Ignore only for the same freshly observed debug UI.

    The capture hash is the handle printed by ``play look``. It binds this
    deliberate input to the current modal without classifying its message by
    source line or keeping a warning allowlist.
    """
    result: dict[str, Any] = {"schema": "caol-startup-debug-recovery-v1", "ok": False,
                              "state": "input_not_sent", "binding_id": binding_id,
                              "next_action": "play look"}
    if not re.fullmatch(r"[0-9a-f]{64}", str(capture_sha256)):
        return {**result, "reason": "capture_sha256_required"}
    note = str(note).strip()
    session = Path(session).resolve()
    status = _read_object(session / "status.json")
    before = observe_startup_dialog(session, binding_id, status, snapshot=snapshot, runner=runner)
    result["before"] = before
    if before.get("state") != "confirmed_debug_dialog":
        return {**result, "reason": "current_exact_debug_dialog_unconfirmed"}
    if before.get("capture_sha256", before.get("image_sha256")) != capture_sha256:
        return {**result, "reason": "current_debug_capture_changed"}
    if not before.get("run_id") or not before.get("log_path"):
        return {**result, "reason": "current_debug_source_or_run_unbound"}
    owner = _read_object(session / "game-process.json")
    expected = owner.get("process_generation")
    phase = before.get("startup_phase")
    generation = status.get("session_generation", 0)
    from startup_harness import process_generation_matches, process_generation_snapshot
    process_snapshot = snapshot or process_generation_snapshot
    pid = before["pid"]
    def still_current() -> bool:
        current_owner = _read_object(session / "game-process.json")
        current_status = _read_object(session / "status.json")
        return (isinstance(expected, Mapping)
                and current_owner.get("binding_id") == binding_id
                and current_owner.get("run_id") == before["run_id"]
                and current_owner.get("pid") == pid
                and current_owner.get("command") == before["command"]
                and current_owner.get("process_generation") == expected
                and current_status.get("binding_id") == binding_id
                and pre_world_startup_phase(current_status) == phase
                and current_status.get("session_generation", 0) == generation
                and process_generation_matches(expected, process_snapshot(pid)))
    if not still_current():
        return {**result, "reason": "identity_or_status_changed_before_input"}
    terminal_context = None
    if before.get("capture_kind") == "terminal":
        try:
            terminal_context = _terminal_context(owner, process_snapshot)
            result["authorization"] = _terminal_permission(terminal_context)
        except ValueError as error:
            return {**result, "reason": str(error)}
    records = session / "startup-dialog-recoveries"
    records.mkdir(parents=True, exist_ok=True)
    recovery_id = uuid.uuid4().hex
    attempt_path = records / (recovery_id + ".attempt.json")
    _record_recovery(attempt_path, {
        "schema": "caol-startup-debug-recovery-attempt-v1", "recovery_id": recovery_id,
        "recorded_at_utc": datetime.now(timezone.utc).isoformat(),
        "session": str(session), "binding_id": binding_id, "run_id": before["run_id"],
        "pid": pid, "birth_identity": before["birth_identity"],
        "note": note, "current_dialog": before,
        "authorization": result.get("authorization"),
        "intended_input": "run_bound_pty debug:ignore i" if terminal_context else
                          "Peekaboo type i --pid (native debug Ignore)",
    })
    result.update(recovery_id=recovery_id, attempt_path=str(attempt_path))
    if not still_current():
        result["reason"] = "identity_or_status_changed_before_input"
        result_path = records / (recovery_id + ".result.json")
        try:
            _record_recovery(result_path, result)
            result["result_path"] = str(result_path)
        except OSError as error:
            result["result_record_error"] = str(error)
        return result
    if terminal_context:
        # Reconcile once more after persisting intent. Never automatically retry
        # a request whose acknowledgement or delivery could be uncertain.
        fresh = observe_startup_dialog(session, binding_id, status, snapshot=process_snapshot, runner=runner)
        try:
            permission_current = _terminal_permission(terminal_context) == result["authorization"]
        except ValueError:
            permission_current = False
        if (not permission_current or fresh.get("state") != "confirmed_debug_dialog"
                or fresh.get("capture_sha256") != capture_sha256 or not still_current()):
            result["reason"] = "terminal_dialog_or_owner_changed_before_input"
        else:
            from curses_terminal_transport import dispatch_input
            try:
                receipt = dispatch_input(Path(terminal_context["owner"]["endpoint"]),
                    run_id=before["run_id"], pid=pid, keys=["i"],
                    process_generation=expected, request_id=recovery_id)
                result["input"] = {"key": "i", "pid": pid, "delivery": "acknowledgement_unconfirmed",
                                   "terminal_receipt": receipt}
                if (receipt.get("ok") is not True or receipt.get("request_id") != recovery_id
                        or receipt.get("run_id") != before["run_id"] or receipt.get("pid") != pid
                        or receipt.get("host") != terminal_context["owner"]["host"]
                        or receipt.get("keys") != ["i"] or receipt.get("owner") != "run_bound_pty"
                        or receipt.get("payload_sha256") != sha256(b"i").hexdigest()
                        or not process_generation_matches(expected, receipt.get("process_generation", {}))):
                    raise ValueError("terminal_dispatch_acknowledgement_mismatch")
                result.update(ok=True, state="input_reported_delivered",
                              input={"key": "i", "pid": pid, "delivery": "reported_delivered",
                                     "terminal_receipt": receipt})
            except (RuntimeError, OSError, ValueError, TypeError, AttributeError) as error:
                result.setdefault("input", {"key": "i", "pid": pid, "delivery": "unknown"})
                result["input"]["error_type"] = type(error).__name__
                result.update(state="input_outcome_unknown", reason=str(error),
                              next_action="Reobserve and reconcile the retained attempt; do not retry automatically")
    else:
        pressed = _command(_peekaboo_args("type", "i", "--pid", str(pid)), timeout=8, runner=runner)
        result["input"] = {"key": "i", "pid": pid, "delivery": "failed" if "error" in pressed else "reported_delivered",
                           "peekaboo": pressed}
        result["ok"] = "error" not in pressed
        result["state"] = "input_reported_delivered" if result["ok"] else "input_delivery_failed"
    after_status = _read_object(session / "status.json")
    if after_status.get("binding_id") == binding_id and pre_world_startup_phase(after_status) == phase:
        result["after"] = observe_startup_dialog(session, binding_id, after_status,
                                                 snapshot=snapshot, runner=runner)
    else:
        result["after"] = {"state": "bridge_status_changed", "status": after_status.get("state"),
                           "binding_id": after_status.get("binding_id")}
    result_path = records / (recovery_id + ".result.json")
    try:
        _record_recovery(result_path, result)
        result["result_path"] = str(result_path)
    except OSError as error:
        result["result_record_error"] = str(error)
    return result
