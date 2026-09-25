"""Read-only proof that a claimed native worker has returned from its exact turn.

This is a quiescence check, not a task result or a claim release. Missing native
state, an unclosed turn, or an unaccounted transport keeps the attempt live.
"""
from __future__ import annotations

from contextlib import closing
import json
import os
from pathlib import Path
import sqlite3
import subprocess
from typing import Any


def _process_birth(pid: int) -> str | None:
    """Return the OS start identity, or None only when this PID is absent."""
    result = subprocess.run(
        ["ps", "-p", str(pid), "-o", "lstart="], capture_output=True, text=True,
        check=False,
    )
    if result.returncode == 1 and not result.stdout.strip() and not result.stderr.strip():
        return None
    if result.returncode != 0 or not result.stdout.strip():
        return "unknown"
    return result.stdout.strip()


def _transport_quiet(workspace: Path, coordinator: str) -> bool:
    """The exact CLI transport has exited and no matching run is still open."""
    root = workspace / ".de67/state/runner-runs"
    if not root.is_dir():
        return False
    matched = False
    for status_path in root.glob("*/status.json"):
        try:
            status = json.loads(status_path.read_text(encoding="utf-8"))
        except (OSError, ValueError, TypeError, AttributeError):
            return False
        if not isinstance(status, dict):
            return False
        if status.get("session_id") != coordinator:
            continue
        matched = True
        pid, birth = status.get("transport_pid"), status.get("transport_birth")
        if (status.get("transport_kind") != "cli" or status.get("status") != "done"
                or status.get("exit_code") != 0 or not isinstance(pid, int) or pid <= 0
                or not isinstance(birth, str) or not birth):
            return False
        try:
            with (status_path.parent / "events.jsonl").open(encoding="utf-8") as events:
                started = any(
                    (event := json.loads(line)).get("type") == "thread.started"
                    and event.get("thread_id") == coordinator
                    for line in events
                )
        except (OSError, ValueError, TypeError, AttributeError):
            return False
        observed_birth = _process_birth(pid)
        if not started or observed_birth in {birth, "unknown"}:
            return False
    return matched


def _completed_latest_turn(path: Path, thread_id: str, parent_id: str,
                           workspace: Path, agent_path: str) -> bool:
    """Read persisted native turn events; final answer text has no authority."""
    try:
        before = path.stat()
        with path.open(encoding="utf-8") as source:
            first = json.loads(next(source))
            if first.get("type") != "session_meta":
                return False
            meta = first.get("payload", {})
            spawn = meta.get("source", {}).get("subagent", {}).get("thread_spawn", {})
            if (meta.get("id") != thread_id or meta.get("parent_thread_id") != parent_id
                    or Path(meta.get("cwd", "")).resolve() != workspace
                    or meta.get("agent_path") != agent_path
                    or meta.get("thread_source") != "subagent"
                    or spawn.get("parent_thread_id") != parent_id
                    or spawn.get("agent_path") != agent_path):
                return False
            open_turn: str | None = None
            completed = False
            for line in source:
                event = json.loads(line)
                if event.get("type") != "event_msg":
                    continue
                payload = event.get("payload", {})
                kind, turn_id = payload.get("type"), payload.get("turn_id")
                if kind == "task_started":
                    if not isinstance(turn_id, str) or not turn_id:
                        return False
                    # A new turn supersedes an older turn in this native
                    # thread; only the exact latest turn can establish return.
                    open_turn, completed = turn_id, False
                elif kind == "task_complete":
                    if open_turn != turn_id:
                        return False
                    open_turn, completed = None, True
                elif kind == "turn_aborted":
                    if open_turn != turn_id:
                        return False
                    open_turn, completed = None, False
                elif kind == "task_failed":
                    return False
            after = path.stat()
            return (open_turn is None and completed and before.st_size == after.st_size
                    and before.st_mtime_ns == after.st_mtime_ns)
    except (OSError, StopIteration, ValueError, TypeError, AttributeError):
        return False


def _thread_tree_quiet(db: sqlite3.Connection, workspace: Path,
                       worker_id: str, coordinator: str, task_id: str) -> bool:
    expected_path = "/root/task_" + task_id.encode("utf-8").hex()
    pending = [(worker_id, coordinator, expected_path)]
    visited: set[str] = set()
    while pending:
        thread_id, parent_id, expected = pending.pop()
        if thread_id in visited:
            return False
        visited.add(thread_id)
        row = db.execute(
            "SELECT rollout_path,cwd,agent_path FROM threads WHERE id=?", (thread_id,)
        ).fetchone()
        edge = db.execute(
            "SELECT 1 FROM thread_spawn_edges WHERE parent_thread_id=? AND child_thread_id=?",
            (parent_id, thread_id),
        ).fetchone()
        if (row is None or edge is None or not row[0] or not row[2]
                or Path(row[1]).resolve() != workspace or row[2] != expected):
            return False
        path = Path(row[0])
        if (not path.is_file() or path.suffix != ".jsonl" or thread_id not in path.name
                or not _completed_latest_turn(path, thread_id, parent_id, workspace, expected)):
            return False
        for child, agent_path in db.execute(
            "SELECT e.child_thread_id,t.agent_path FROM thread_spawn_edges e "
            "LEFT JOIN threads t ON t.id=e.child_thread_id WHERE e.parent_thread_id=?",
            (thread_id,),
        ):
            if not isinstance(agent_path, str) or not agent_path.startswith(expected + "/"):
                return False
            pending.append((child, thread_id, agent_path))
    return True


def returned_assignments(workspace: Path, state: Path, lineage: str) -> set[str]:
    """Open native task claims whose worker, descendants, and CLI owner are quiet."""
    workspace, state = Path(workspace).resolve(), Path(state).resolve()
    codex_state = Path(os.environ.get("DE67_CODEX_STATE") or Path.home() / ".codex/state_5.sqlite").expanduser().resolve()
    if not codex_state.is_file():
        return set()
    try:
        with closing(sqlite3.connect(state.as_uri() + "?mode=ro", uri=True)) as claims, \
             closing(sqlite3.connect(codex_state.as_uri() + "?mode=ro", uri=True)) as codex:
            rows = claims.execute(
                "SELECT t.task_id,w.worker_id,w.coordinator_session_id "
                "FROM tasks t JOIN worker_claims w ON w.lineage_id=t.lineage_id "
                "AND w.task_id=t.task_id WHERE t.lineage_id=? "
                "AND t.attempt_terminal_at IS NULL AND w.released_at IS NULL",
                (lineage,),
            ).fetchall()
            result: set[str] = set()
            transport: dict[str, bool] = {}
            for task_id, worker_id, coordinator in rows:
                if not worker_id or not coordinator:
                    continue
                expected = "/root/task_" + task_id.encode("utf-8").hex()
                native_edge = codex.execute(
                    "SELECT 1 FROM thread_spawn_edges e JOIN threads t "
                    "ON t.id=e.child_thread_id WHERE e.parent_thread_id=? "
                    "AND e.child_thread_id=? AND t.agent_path=? AND t.cwd=?",
                    (coordinator, worker_id, expected, str(workspace)),
                ).fetchone()
                if native_edge is None:
                    continue
                if coordinator not in transport:
                    transport[coordinator] = _transport_quiet(workspace, coordinator)
                if (transport[coordinator]
                        and _thread_tree_quiet(codex, workspace, worker_id, coordinator, task_id)):
                    result.add(task_id)
            return result
    except (sqlite3.Error, OSError, ValueError, TypeError, AttributeError):
        return set()


def quiet_assignments(workspace: Path, state: Path, lineage: str) -> set[str]:
    from worker_library import returned_assignments as named_returned
    return named_returned(workspace, state, lineage) | returned_assignments(workspace, state, lineage)
