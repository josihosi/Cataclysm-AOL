#!/usr/bin/env python3
"""Create crash-recoverable Git checkpoints at quiescent DE67 boundaries."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sqlite3
import subprocess
import time
import uuid
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable, Sequence


class RepositoryCheckpointError(RuntimeError):
    """Raised when a recovery checkpoint cannot be completed honestly."""


@dataclass(frozen=True)
class CheckpointTarget:
    source_ref: str
    target_ref: str
    remote: str
    remote_url: str | None


EventHook = Callable[[str, dict[str, Any]], None]


def _git(
    workspace: Path,
    *arguments: str,
    check: bool = True,
) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(
        ["git", "-C", str(workspace), *arguments],
        capture_output=True,
        text=True,
        encoding="utf-8",
        check=False,
    )
    if check and result.returncode != 0:
        detail = (result.stderr or result.stdout).strip() or "git command failed"
        raise RepositoryCheckpointError(detail)
    return result


def _load_target(workspace: Path) -> CheckpointTarget | None:
    config_path = workspace / ".de67" / "state" / "workspace.json"
    if not config_path.is_file():
        return None
    try:
        value = json.loads(config_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise RepositoryCheckpointError("Workspace checkpoint configuration is invalid") from error
    if not isinstance(value, dict):
        raise RepositoryCheckpointError("Workspace checkpoint configuration must be an object")
    configured_workspace = Path(str(value.get("workspace", ""))).expanduser().resolve()
    if configured_workspace != workspace:
        raise RepositoryCheckpointError("Workspace checkpoint configuration names another workspace")
    source_ref = str(value.get("source_ref", ""))
    targets = value.get("targets")
    if not source_ref.startswith("refs/heads/"):
        raise RepositoryCheckpointError("Checkpoint source_ref must name one local branch")
    if not isinstance(targets, list) or len(targets) != 1:
        raise RepositoryCheckpointError("Checkpointing requires exactly one configured push target")
    target = targets[0]
    if not isinstance(target, dict):
        raise RepositoryCheckpointError("Checkpoint target must be an object")
    remote = str(target.get("remote", "")).strip()
    target_ref = str(target.get("target_ref", ""))
    remote_url = target.get("remote_url")
    if not remote or not target_ref.startswith("refs/heads/"):
        raise RepositoryCheckpointError("Checkpoint target must name a remote branch")
    if remote_url is not None and not isinstance(remote_url, str):
        raise RepositoryCheckpointError("Checkpoint remote_url must be text")
    return CheckpointTarget(source_ref, target_ref, remote, remote_url)


def _validate_git_target(workspace: Path, target: CheckpointTarget) -> None:
    head_ref = _git(workspace, "symbolic-ref", "-q", "HEAD").stdout.strip()
    if head_ref != target.source_ref:
        raise RepositoryCheckpointError(
            f"Checkpoint source branch mismatch: expected {target.source_ref}, found {head_ref or 'detached HEAD'}"
        )
    configured_url = _git(workspace, "remote", "get-url", "--push", target.remote).stdout.strip()
    if target.remote_url is not None and configured_url != target.remote_url:
        raise RepositoryCheckpointError(
            f"Checkpoint remote URL mismatch for {target.remote}"
        )


def _initialize(connection: sqlite3.Connection) -> None:
    connection.execute(
        """
        CREATE TABLE IF NOT EXISTS repository_checkpoints (
            lineage_id TEXT NOT NULL,
            checkpoint_id TEXT NOT NULL,
            allocated_at REAL NOT NULL,
            state_revision TEXT NOT NULL,
            source_ref TEXT NOT NULL,
            target_ref TEXT NOT NULL,
            remote TEXT NOT NULL,
            commit_sha TEXT,
            committed_at REAL,
            pushed_at REAL,
            verified_at REAL,
            status TEXT NOT NULL CHECK (
                status IN ('allocated', 'committed', 'pushed', 'verified', 'no_changes', 'failed')
            ),
            failure_step TEXT,
            failure_detail TEXT,
            failure_count INTEGER NOT NULL DEFAULT 0 CHECK (failure_count >= 0),
            PRIMARY KEY (lineage_id, checkpoint_id)
        )
        """
    )


def _table_exists(connection: sqlite3.Connection, name: str) -> bool:
    return connection.execute(
        "SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = ?", (name,)
    ).fetchone() is not None


def _state_revision(connection: sqlite3.Connection, lineage_id: str) -> str:
    revision: dict[str, Any] = {"lineage_id": lineage_id}
    queries = {
        "tasks": "SELECT COUNT(*), MAX(COALESCE(attempt_terminal_at, started_at)) FROM tasks WHERE lineage_id = ?",
        "worker_claims": "SELECT COUNT(*), MAX(COALESCE(released_at, last_checkpoint_at)) FROM worker_claims WHERE lineage_id = ?",
        "supervisor_attempts": "SELECT COUNT(*), MAX(COALESCE(finished_at, started_at)) FROM supervisor_attempts WHERE lineage_id = ?",
        "restart_requests": "SELECT COUNT(*), MAX(generation) FROM coordinator_restart_requests WHERE lineage_id = ?",
        "acceptances": "SELECT COUNT(*), MAX(accepted_at) FROM claim_acceptances WHERE lineage_id = ?",
    }
    table_for_key = {
        "tasks": "tasks",
        "worker_claims": "worker_claims",
        "supervisor_attempts": "supervisor_attempts",
        "restart_requests": "coordinator_restart_requests",
        "acceptances": "claim_acceptances",
    }
    for key, query in queries.items():
        if _table_exists(connection, table_for_key[key]):
            row = connection.execute(query, (lineage_id,)).fetchone()
            revision[key] = [row[0], row[1]]
    encoded = json.dumps(revision, sort_keys=True, separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest()


def _assert_quiescent(
    connection: sqlite3.Connection,
    lineage_id: str,
    supervisor_owner_id: str | None,
) -> None:
    if _table_exists(connection, "worker_claims"):
        live_claim = connection.execute(
            "SELECT task_id FROM worker_claims WHERE lineage_id = ? AND released_at IS NULL LIMIT 1",
            (lineage_id,),
        ).fetchone()
        if live_claim is not None:
            raise RepositoryCheckpointError(
                f"Checkpoint refused while worker task {live_claim[0]} is live"
            )
    if _table_exists(connection, "supervisor_attempts"):
        columns = {
            str(row[1])
            for row in connection.execute("PRAGMA table_info(supervisor_attempts)")
        }
        owner_clause = ""
        parameters: list[Any] = [lineage_id]
        if supervisor_owner_id is not None and "owner_id" in columns:
            owner_clause = " AND owner_id = ?"
            parameters.append(supervisor_owner_id)
        live_attempt = connection.execute(
            (
                """
            SELECT role, run_id FROM supervisor_attempts
            WHERE lineage_id = ? AND finished_at IS NULL
              AND role IN ('coordinator', 'mutation-reviewer', 'worker')
            """
                + owner_clause
                + " LIMIT 1"
            ),
            parameters,
        ).fetchone()
        if live_attempt is not None:
            raise RepositoryCheckpointError(
                f"Checkpoint refused while {live_attempt[0]} run {live_attempt[1]} is live"
            )


def _trailers(workspace: Path) -> dict[str, str]:
    message = _git(workspace, "log", "-1", "--format=%B").stdout
    parsed: dict[str, str] = {}
    for line in message.splitlines():
        match = re.fullmatch(r"(DE67-[A-Za-z-]+):\s*(\S.*)", line)
        if match:
            parsed[match.group(1)] = match.group(2).strip()
    return parsed


def _record_failure(
    connection: sqlite3.Connection,
    lineage_id: str,
    checkpoint_id: str,
    step: str,
    detail: str,
) -> None:
    connection.execute(
        """
        UPDATE repository_checkpoints
        SET status = 'failed', failure_step = ?, failure_detail = ?,
            failure_count = failure_count + 1
        WHERE lineage_id = ? AND checkpoint_id = ?
        """,
        (step, detail, lineage_id, checkpoint_id),
    )
    connection.commit()


def _receipt(row: sqlite3.Row, *, created: bool) -> dict[str, Any]:
    return {
        "checkpoint_id": row["checkpoint_id"],
        "lineage_id": row["lineage_id"],
        "state_revision": row["state_revision"],
        "status": row["status"],
        "commit_sha": row["commit_sha"],
        "source_ref": row["source_ref"],
        "target_ref": row["target_ref"],
        "remote": row["remote"],
        "failure_count": row["failure_count"],
        "created": created,
    }


def checkpoint_repository(
    workspace: str | Path,
    state_path: str | Path,
    lineage_id: str,
    *,
    now: Callable[[], float] = time.time,
    event_hook: EventHook | None = None,
    supervisor_owner_id: str | None = None,
) -> dict[str, Any]:
    """Checkpoint one configured product repository or return a disabled receipt."""

    workdir = Path(workspace).expanduser().resolve()
    state = Path(state_path).expanduser().resolve()
    lineage = lineage_id.strip()
    if not lineage:
        raise RepositoryCheckpointError("Checkpoint lineage must not be empty")
    target = _load_target(workdir)
    if target is None:
        return {"status": "disabled", "reason": "workspace target is not configured"}
    _validate_git_target(workdir, target)

    with sqlite3.connect(state) as connection:
        connection.row_factory = sqlite3.Row
        _initialize(connection)
        _assert_quiescent(connection, lineage, supervisor_owner_id)
        row = connection.execute(
            """
            SELECT * FROM repository_checkpoints
            WHERE lineage_id = ? AND status NOT IN ('verified', 'no_changes')
            ORDER BY allocated_at DESC LIMIT 1
            """,
            (lineage,),
        ).fetchone()
        created = False
        if row is None:
            checkpoint_id = uuid.uuid4().hex[:16]
            allocated_at = now()
            revision = _state_revision(connection, lineage)
            connection.execute(
                """
                INSERT INTO repository_checkpoints (
                    lineage_id, checkpoint_id, allocated_at, state_revision,
                    source_ref, target_ref, remote, status
                ) VALUES (?, ?, ?, ?, ?, ?, ?, 'allocated')
                """,
                (
                    lineage,
                    checkpoint_id,
                    allocated_at,
                    revision,
                    target.source_ref,
                    target.target_ref,
                    target.remote,
                ),
            )
            connection.commit()
            created = True
            row = connection.execute(
                "SELECT * FROM repository_checkpoints WHERE lineage_id = ? AND checkpoint_id = ?",
                (lineage, checkpoint_id),
            ).fetchone()
        assert row is not None
        checkpoint_id = str(row["checkpoint_id"])
        if (
            row["source_ref"] != target.source_ref
            or row["target_ref"] != target.target_ref
            or row["remote"] != target.remote
        ):
            raise RepositoryCheckpointError("Pending checkpoint target no longer matches configuration")

        commit_sha = row["commit_sha"]
        if commit_sha is None:
            trailers = _trailers(workdir)
            if trailers.get("DE67-Checkpoint") == checkpoint_id:
                commit_sha = _git(workdir, "rev-parse", "HEAD").stdout.strip()
                connection.execute(
                    """
                    UPDATE repository_checkpoints
                    SET commit_sha = ?, committed_at = ?, status = 'committed'
                    WHERE lineage_id = ? AND checkpoint_id = ?
                    """,
                    (commit_sha, now(), lineage, checkpoint_id),
                )
                connection.commit()
            else:
                try:
                    _git(workdir, "add", "-A", "--", ".")
                    staged = _git(workdir, "diff", "--cached", "--quiet", check=False)
                    if staged.returncode == 0:
                        connection.execute(
                            """
                            UPDATE repository_checkpoints SET status = 'no_changes'
                            WHERE lineage_id = ? AND checkpoint_id = ?
                            """,
                            (lineage, checkpoint_id),
                        )
                        connection.commit()
                        final = connection.execute(
                            "SELECT * FROM repository_checkpoints WHERE lineage_id = ? AND checkpoint_id = ?",
                            (lineage, checkpoint_id),
                        ).fetchone()
                        assert final is not None
                        return _receipt(final, created=created)
                    if staged.returncode != 1:
                        raise RepositoryCheckpointError("Unable to inspect staged checkpoint changes")
                    message = (
                        f"DE67 recovery checkpoint {checkpoint_id}\n\n"
                        f"DE67-Checkpoint: {checkpoint_id}\n"
                        f"DE67-Lineage: {lineage}\n"
                        f"DE67-State-Revision: {row['state_revision']}\n"
                        f"DE67-Source-Ref: {target.source_ref}\n"
                        f"DE67-Target-Ref: {target.target_ref}\n"
                    )
                    _git(workdir, "commit", "-m", message)
                    commit_sha = _git(workdir, "rev-parse", "HEAD").stdout.strip()
                    if event_hook is not None:
                        event_hook(
                            "after_commit_before_sqlite",
                            {"checkpoint_id": checkpoint_id, "commit_sha": commit_sha},
                        )
                    connection.execute(
                        """
                        UPDATE repository_checkpoints
                        SET commit_sha = ?, committed_at = ?, status = 'committed'
                        WHERE lineage_id = ? AND checkpoint_id = ?
                        """,
                        (commit_sha, now(), lineage, checkpoint_id),
                    )
                    connection.commit()
                except (OSError, RepositoryCheckpointError) as error:
                    _record_failure(connection, lineage, checkpoint_id, "commit", str(error))
                    raise

        assert commit_sha is not None
        head = _git(workdir, "rev-parse", "HEAD").stdout.strip()
        if head != commit_sha:
            detail = "Checkpoint commit is no longer repository HEAD"
            _record_failure(connection, lineage, checkpoint_id, "push", detail)
            raise RepositoryCheckpointError(detail)
        try:
            _git(
                workdir,
                "push",
                target.remote,
                f"{target.source_ref}:{target.target_ref}",
            )
            connection.execute(
                """
                UPDATE repository_checkpoints SET pushed_at = ?, status = 'pushed'
                WHERE lineage_id = ? AND checkpoint_id = ?
                """,
                (now(), lineage, checkpoint_id),
            )
            connection.commit()
            if event_hook is not None:
                event_hook(
                    "after_push_before_verify",
                    {"checkpoint_id": checkpoint_id, "commit_sha": commit_sha},
                )
            remote = _git(workdir, "ls-remote", "--refs", target.remote, target.target_ref)
            matching = [
                line.split("\t", 1)[0]
                for line in remote.stdout.splitlines()
                if line.endswith(f"\t{target.target_ref}")
            ]
            if matching != [commit_sha]:
                raise RepositoryCheckpointError(
                    "Remote checkpoint verification does not match the local commit"
                )
            connection.execute(
                """
                UPDATE repository_checkpoints SET verified_at = ?, status = 'verified'
                WHERE lineage_id = ? AND checkpoint_id = ?
                """,
                (now(), lineage, checkpoint_id),
            )
            connection.commit()
        except (OSError, RepositoryCheckpointError) as error:
            _record_failure(connection, lineage, checkpoint_id, "push-or-verify", str(error))
            raise

        final = connection.execute(
            "SELECT * FROM repository_checkpoints WHERE lineage_id = ? AND checkpoint_id = ?",
            (lineage, checkpoint_id),
        ).fetchone()
        assert final is not None
        return _receipt(final, created=created)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workspace", required=True)
    parser.add_argument("--state", required=True)
    parser.add_argument("--lineage", required=True)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    arguments = build_parser().parse_args(argv)
    try:
        result = checkpoint_repository(
            arguments.workspace, arguments.state, arguments.lineage
        )
    except (OSError, RepositoryCheckpointError, sqlite3.Error) as error:
        print(f"repository checkpoint: {error}", file=__import__("sys").stderr)
        return 1
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
