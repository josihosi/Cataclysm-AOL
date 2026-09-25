#!/usr/bin/env python3
"""Run DE-67 coordinators under one external, restart-aware parent."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import sqlite3
import subprocess
import sys
import time
import types
import uuid
from contextlib import closing, contextmanager
from dataclasses import dataclass
from pathlib import Path
from typing import BinaryIO, Callable, Iterator, Mapping, Sequence

from blocker_adapter import (
    BlockerAdapterError,
    BlockerReply,
    SubprocessBlockerAdapter,
    parse_adapter_command,
    safe_wait_for_reply,
)
from instruction_context import common_guidance
from agent_mailbox import communication_contract
from deadline_harness import DeadlineError, DeadlineHarness
from policy_kernel import current_owner_contract, workspace_facts
from specification import SpecificationError, resolve
from repository_checkpoint import (
    RepositoryCheckpointError,
    checkpoint_repository,
)


RED_DFS_CLAIM = re.compile(r"^- \[ \] \N{LARGE RED CIRCLE} ", re.MULTILINE)
ACTIVE_LEDGER_ITEM = re.compile(r"^- \[ \] ", re.MULTILINE)
BLOCKED_LEDGER_ITEM = re.compile(r"^- Blocked: ", re.MULTILINE)
BLOCKED_AUDIT_PREFIX = "supervisor audit blocked-only ledger sha256:"
WORKER_OWNER_LOST_REASON = (
    "worker_owner_lost: coordinator process exited while this worker window "
    "remained nonterminal"
)
COORDINATOR_DECISION_OPPORTUNITIES = 3


class SupervisorError(RuntimeError):
    """Raised when coordinator ownership or restart state is inconsistent."""


@dataclass(frozen=True)
class RestartState:
    required: bool
    generation: int | None
    expected_run_id: str | None
    run_id: str | None
    reason: str | None


@dataclass(frozen=True)
class ChildResult:
    run_id: str
    run_dir: Path
    exit_code: int
    launched: bool


@dataclass(frozen=True)
class SupervisionEvent:
    restart: RestartState | None
    signature: str


@dataclass(frozen=True)
class MutationGate:
    kind: str
    identity: str
    selected_lane: str | None


@dataclass(frozen=True)
class MutationSuggestion:
    mode: str
    entry: str


class SupervisorJournal:
    """Persist the one-shot authorization for each semantic supervisor frontier."""

    def __init__(
        self,
        state_path: Path,
        lineage_id: str,
        owner_id: str,
        frontier_namespace: str | None = None,
    ) -> None:
        self.state_path = state_path
        self.lineage_id = lineage_id
        self.owner_id = owner_id
        self.frontier_namespace = frontier_namespace
        with closing(sqlite3.connect(state_path)) as connection, connection:
            connection.execute(
                """
                CREATE TABLE IF NOT EXISTS supervisor_attempts (
                    lineage_id TEXT NOT NULL,
                    role TEXT NOT NULL,
                    frontier TEXT NOT NULL,
                    run_id TEXT NOT NULL,
                    owner_id TEXT NOT NULL,
                    started_at REAL NOT NULL,
                    finished_at REAL,
                    outcome TEXT,
                    detail TEXT,
                    PRIMARY KEY (lineage_id, role, frontier),
                    UNIQUE (lineage_id, run_id)
                )
                """
            )

    def begin(self, role: str, frontier: str, run_id: str) -> None:
        if self.frontier_namespace:
            frontier = f"{self.frontier_namespace}:{frontier}"
        try:
            with closing(sqlite3.connect(self.state_path)) as connection, connection:
                connection.execute(
                    """
                    INSERT INTO supervisor_attempts (
                        lineage_id, role, frontier, run_id, owner_id, started_at
                    ) VALUES (?, ?, ?, ?, ?, ?)
                    """,
                    (self.lineage_id, role, frontier, run_id, self.owner_id, time.time()),
                )
        except sqlite3.IntegrityError as error:
            raise SupervisorError(
                f"Supervisor {role} frontier was already attempted: {frontier}"
            ) from error

    def finish(self, run_id: str, outcome: str, detail: str | None = None) -> None:
        with closing(sqlite3.connect(self.state_path)) as connection, connection:
            cursor = connection.execute(
                """
                UPDATE supervisor_attempts
                SET finished_at = ?, outcome = ?, detail = ?
                WHERE lineage_id = ? AND run_id = ? AND outcome IS NULL
                """,
                (time.time(), outcome, detail, self.lineage_id, run_id),
            )
            if cursor.rowcount != 1:
                raise SupervisorError(
                    f"Supervisor run is missing or already terminal: {run_id}"
                )

    def reject(self, run_id: str, reason: str) -> None:
        """Record a protocol rejection after the child's process exit was recorded."""
        with closing(sqlite3.connect(self.state_path)) as connection, connection:
            cursor = connection.execute(
                """
                UPDATE supervisor_attempts SET outcome = 'failed', detail = ?
                WHERE lineage_id = ? AND run_id = ? AND finished_at IS NOT NULL
                """,
                (reason, self.lineage_id, run_id),
            )
            if cursor.rowcount != 1:
                raise SupervisorError(f"Supervisor child exit is not recorded: {run_id}")


def _write(path: Path, value: str) -> None:
    temporary = path.with_name(f".{path.name}.tmp")
    temporary.write_text(value, encoding="utf-8")
    temporary.replace(path)


def _process_exists(pid: int) -> bool:
    if pid <= 0:
        return False
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        return True
    return True


def reconcile_dead_run_records(run_root: Path) -> tuple[str, ...]:
    """Close stale run records only when their recorded process is absent."""
    if not run_root.is_dir():
        return ()
    interrupted: list[str] = []
    for status_path in sorted(run_root.glob("*/status.txt")):
        if status_path.read_text(encoding="utf-8").strip() not in {
            "STARTING", "RUNNING",
        }:
            continue
        pid_path = status_path.parent / "pid.txt"
        try:
            pid = int(pid_path.read_text(encoding="utf-8").strip())
        except (FileNotFoundError, ValueError):
            pid = 0
        if _process_exists(pid):
            continue
        _write(status_path.parent / "supervisor_error.txt", (
            "Explicit supervisor startup interrupted this stale run because "
            "its recorded coordinator process is absent.\n"
        ))
        _write(status_path, "INTERRUPTED\n")
        interrupted.append(status_path.parent.name)
    return tuple(interrupted)


@contextmanager
def _supervisor_lock(state_path: Path) -> Iterator[None]:
    """Hold one OS-released lock for this deadline database."""
    lock_path = state_path.with_name(f"{state_path.name}.coordinator-supervisor.lock")
    handle: BinaryIO = lock_path.open("a+b")
    handle.seek(0, os.SEEK_END)
    if handle.tell() == 0:
        handle.write(b"\0")
        handle.flush()
    handle.seek(0)
    try:
        if os.name == "nt":
            import msvcrt

            msvcrt.locking(handle.fileno(), msvcrt.LK_NBLCK, 1)
        else:
            import fcntl

            fcntl.flock(handle.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
    except OSError as error:
        handle.close()
        raise SupervisorError(
            "Another coordinator supervisor already owns this deadline state"
        ) from error
    try:
        yield
    finally:
        handle.seek(0)
        if os.name == "nt":
            import msvcrt

            msvcrt.locking(handle.fileno(), msvcrt.LK_UNLCK, 1)
        else:
            import fcntl

            fcntl.flock(handle.fileno(), fcntl.LOCK_UN)
        handle.close()


def _restart_state(summary: Mapping[str, object], lineage_id: str) -> RestartState:
    if summary.get("lineage_id") != lineage_id:
        raise SupervisorError("Deadline state is bound to a different lineage")
    raw = summary.get("coordinator_restart")
    if raw is None:
        return RestartState(False, None, None, None, None)
    if not isinstance(raw, Mapping):
        raise SupervisorError("Coordinator restart state must be an object")

    required = raw.get("required")
    if required is None:
        required = raw.get("pending")
    if not isinstance(required, bool):
        raise SupervisorError("Coordinator restart state must say whether restart is required")

    generation = raw.get("generation")
    if isinstance(generation, bool) or not isinstance(generation, int) or generation <= 0:
        raise SupervisorError("Coordinator restart generation must be a positive integer")
    run_id = raw.get("run_id")
    if run_id is not None and (not isinstance(run_id, str) or not run_id.strip()):
        raise SupervisorError("Acknowledged coordinator run id must be non-empty")
    expected_run_id = raw.get("expected_run_id")
    if expected_run_id is not None and (
        not isinstance(expected_run_id, str) or not expected_run_id.strip()
    ):
        raise SupervisorError("Claimed coordinator run id must be non-empty")
    reason = raw.get("reason")
    if reason is not None and (not isinstance(reason, str) or not reason.strip()):
        raise SupervisorError("Coordinator restart reason must be non-empty")
    return RestartState(required, generation, expected_run_id, run_id, reason)


def read_clock(state_path: Path, lineage_id: str) -> RestartState:
    """Read restart state once; callers decide when another read is warranted."""
    with DeadlineHarness(state_path) as harness:
        return _restart_state(
            harness.coordinator_restart_status(lineage_id), lineage_id
        )


def terminalize_unowned_worker_windows(
    state_path: Path,
    lineage_id: str,
    recoverable_workers: Mapping[str, str] | None = None,
    *,
    include_unclaimed: bool = True,
) -> tuple[str, ...]:
    """Fail closed only for clocks that never acquired a verified worker."""
    recoverable = recoverable_workers or {}
    with DeadlineHarness(state_path) as harness:
        rows = harness.connection.execute(
                """
                SELECT task.task_id, task.claim_id, claim.worker_id,
                       claim.coordinator_session_id, claim.supervisor_id
                FROM tasks AS task
                LEFT JOIN worker_claims AS claim
                  ON claim.lineage_id = task.lineage_id
                 AND claim.task_id = task.task_id
                 AND claim.released_at IS NULL
                WHERE task.lineage_id = ? AND task.attempt_terminal_at IS NULL
                ORDER BY task.started_at
                """,
                (lineage_id,),
            ).fetchall()
        terminal_rows = tuple(
            row for row in rows if (
                (row["worker_id"] is not None or include_unclaimed)
                and recoverable.get(str(row["worker_id"] or ""))
                != str(row["coordinator_session_id"] or "")
            )
        )
        for row in terminal_rows:
            task_id = str(row["task_id"])
            receipt_id = None
            if row["worker_id"] is not None:
                existing_receipts = harness._worker_result_receipts(lineage_id, task_id)
                terminal_receipt = next(
                    (
                        item for item in reversed(existing_receipts)
                        if item["receipt"].get("disposition") != "checkpoint"
                    ),
                    None,
                )
                if terminal_receipt is not None:
                    receipt_id = str(terminal_receipt["receipt_id"])
                    receipt_value = terminal_receipt["receipt"]
                    disposition = str(receipt_value["disposition"])
                    if disposition == "completed":
                        harness.complete_task(
                            lineage_id,
                            task_id,
                            str(receipt_value["summary"]),
                            receipt_id=receipt_id,
                        )
                    elif disposition == "finding":
                        harness.report_worker_finding(
                            lineage_id,
                            task_id,
                            str(receipt_value["finding_kind"]),
                            str(receipt_value["summary"]),
                            receipt_id=receipt_id,
                            short_verdict=str(receipt_value["verdict"]),
                        )
                    else:
                        harness.abandon_attempt(
                            lineage_id,
                            task_id,
                            WORKER_OWNER_LOST_REASON,
                            receipt_id=receipt_id,
                        )
                    continue
                checkpoints = harness.connection.execute(
                    """
                    SELECT kind, evidence FROM worker_checkpoints
                    WHERE lineage_id = ? AND task_id = ?
                      AND kind != 'result-receipt-v1'
                    ORDER BY sequence DESC
                    """,
                    (lineage_id, task_id),
                ).fetchall()
                latest = str(checkpoints[0]["evidence"]) if checkpoints else (
                    "No worker checkpoint was recorded before ownership was lost."
                )
                receipt = harness.record_worker_result_receipt(
                    lineage_id,
                    task_id,
                    str(row["worker_id"]),
                    {
                        "schema": "de67.worker-result-receipt.v1",
                        "lineage_id": lineage_id,
                        "task_id": task_id,
                        "claim_id": str(row["claim_id"]),
                        "worker_id": str(row["worker_id"]),
                        "disposition": "abandoned",
                        "verdict": "worker ownership lost; outcome remains open",
                        "outcome": f"Continue claim {row['claim_id']} from durable evidence.",
                        "summary": latest,
                        "material_changes": [],
                        "tests": [],
                        "live_actions": [],
                        "evidence_ceiling": [
                            "The supervisor receipt preserves only durable checkpoints; it does not infer unreturned worker work."
                        ],
                        "bindings": {
                            "coordinator_session_id": str(row["coordinator_session_id"]),
                            "supervisor_id": str(row["supervisor_id"]),
                        },
                        "journal_entries": [],
                        "artifacts": [],
                        "first_divergence": {
                            "class": "worker-ownership-lost",
                            "summary": "The owning worker was not recoverable from the successor coordinator session.",
                        },
                        "accepted_no_replay": [],
                        "active_work": [
                            "Resume the claim from its latest durable checkpoint and current ledger frontier."
                        ],
                        "first_open_boundary": (
                            "Resume from the latest durable checkpoint; treat unreturned work as unknown."
                        ),
                        "narrow_queries": [f"task_id={task_id}"],
                        "entrypoints": [],
                        "context_metrics": {
                            "durable_checkpoint_count": len(checkpoints)
                        },
                    },
                )
                receipt_id = str(receipt["receipt_id"])
            harness.abandon_attempt(
                lineage_id,
                task_id,
                WORKER_OWNER_LOST_REASON,
                receipt_id=receipt_id,
            )
    return tuple(str(row["task_id"]) for row in terminal_rows)


def active_worker_coordinator_session(
    state_path: Path,
    lineage_id: str,
) -> str | None:
    """Return the one coordinator session that owns all active worker claims."""
    from worker_library import execution_sessions, returned_assignments
    workspace = state_path.parent.parent.parent
    returned = returned_assignments(workspace, state_path, lineage_id)
    controllers = execution_sessions(workspace, state_path, lineage_id)
    with DeadlineHarness(state_path) as harness:
        sessions = sorted({
            controllers.get(row["task_id"], str(row["coordinator_session_id"]))
            for row in harness.connection.execute(
                """SELECT claim.task_id, claim.coordinator_session_id
                FROM worker_claims AS claim JOIN tasks AS task
                  ON task.lineage_id = claim.lineage_id AND task.task_id = claim.task_id
                WHERE claim.lineage_id = ? AND claim.released_at IS NULL
                  AND task.attempt_terminal_at IS NULL""", (lineage_id,))
            if row["task_id"] not in returned
        })
    if len(sessions) > 1:
        raise SupervisorError(
            "Active worker claims belong to multiple coordinator sessions"
        )
    return sessions[0] if sessions else None


def runtime_worker_owners(
    workspace: Path,
    environment: Mapping[str, str] | None = None,
    *, state_path: Path | None = None, lineage_id: str | None = None,
) -> dict[str, str]:
    """Return verified native parent edges and exact named-worker task owners."""
    selected = os.environ.copy()
    if environment is not None:
        selected.update(environment)
    value = selected.get("DE67_CODEX_STATE", "").strip()
    state = Path(value).expanduser().resolve() if value else Path.home() / ".codex/state_5.sqlite"
    from worker_library import worker_owners
    owners = worker_owners(workspace, state_path, lineage_id) if state_path and lineage_id else {}
    if not state.is_file():
        return owners
    with closing(sqlite3.connect(f"file:{state}?mode=ro", uri=True)) as connection:
        rows = connection.execute(
            """
            SELECT edge.child_thread_id, edge.parent_thread_id, child.model
            FROM thread_spawn_edges AS edge
            JOIN threads AS child ON child.id = edge.child_thread_id
            WHERE child.cwd = ?
            """,
            (str(workspace),),
        ).fetchall()
    return {
        str(worker): str(parent)
        for worker, parent, model in rows
        if any(name in str(model or "").lower() for name in ("luna", "terra", "astra"))
    } | owners


def supervision_fingerprint(
    state_path: Path,
    lineage_id: str,
    workspace: Path,
) -> str:
    """Hash durable state that can justify another automatic child launch."""
    with DeadlineHarness(state_path) as harness:
        # DeadlineHarness binds one database to one lineage, so list_tasks is
        # lineage-scoped by the database invariant rather than a query filter.
        summary = harness.list_tasks(now=0.0)
        worker_claims = [
            dict(row)
            for row in harness.connection.execute(
                """
                SELECT task_id, worker_id, coordinator_session_id, supervisor_id,
                       claimed_at, last_checkpoint_at, released_at, release_reason
                FROM worker_claims WHERE lineage_id = ?
                ORDER BY task_id
                """,
                (lineage_id,),
            ).fetchall()
        ]
        worker_checkpoints = [
            dict(row)
            for row in harness.connection.execute(
                """
                SELECT task_id, sequence, worker_id, kind, evidence, recorded_at
                FROM worker_checkpoints WHERE lineage_id = ?
                ORDER BY task_id, sequence
                """,
                (lineage_id,),
            ).fetchall()
        ]
    documents: dict[str, str | None] = {}
    try:
        specification = resolve(workspace / ".de67")
        specification_document = str(specification.path.relative_to(workspace))
    except SpecificationError:
        specification_document = ".de67/FS.md"
    for relative in (
        specification_document,
        ".de67/work-ledger.md",
        ".de67/mutation-suggestions.md",
        ".de67/phase3-policy.d67",
    ):
        path = workspace / relative
        documents[relative] = (
            hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else None
        )
    payload = {
        "lineage_id": lineage_id,
        "clock": summary,
        "worker_claims": worker_claims,
        "worker_checkpoints": worker_checkpoints,
        "documents": documents,
    }
    return hashlib.sha256(
        json.dumps(payload, sort_keys=True, separators=(",", ":")).encode("utf-8")
    ).hexdigest()


def pending_mutation_suggestions(workspace: Path) -> tuple[MutationSuggestion, ...]:
    """Read owner entries, preserving legacy immediate-trigger semantics."""
    ledger = workspace / ".de67" / "mutation-suggestions.md"
    if not ledger.is_file():
        return ()
    pending = ledger.read_text(encoding="utf-8").partition(
        "## Pending suggestions"
    )[2]
    pending = re.split(r"^#{1,2}\s+", pending, maxsplit=1, flags=re.MULTILINE)[0]
    suggestions: list[MutationSuggestion] = []
    for line in pending.splitlines():
        if not line.startswith("- "):
            continue
        entry = line[2:].strip()
        if entry.lower() == "none.":
            continue
        match = re.match(
            r"^(?:Owner-authorized\s+)?\[(trigger|defer)\]:?\s+(.+)$",
            entry,
            re.IGNORECASE,
        )
        if match is None:
            suggestions.append(MutationSuggestion("trigger", entry))
        else:
            suggestions.append(MutationSuggestion(match.group(1).lower(), entry))
    return tuple(suggestions)


def mutation_gate(
    state_path: Path,
    lineage_id: str,
    workspace: Path | None = None,
) -> MutationGate | None:
    """Return the first durable mutation gate only after workers are quiet."""
    with DeadlineHarness(state_path) as harness:
        summary = harness.list_tasks()
    from worker_library import returned_assignments
    returned = returned_assignments(workspace, state_path, lineage_id) if workspace else set()
    if any(task.get("state") == "running" and task.get("task_id") not in returned
           for task in summary["tasks"]):
        return None
    if workspace is not None:
        immediate = [
            suggestion.entry
            for suggestion in pending_mutation_suggestions(workspace)
            if suggestion.mode == "trigger"
        ]
        if immediate:
            encoded = json.dumps(immediate, ensure_ascii=False, separators=(",", ":"))
            digest = hashlib.sha256(encoded.encode("utf-8")).hexdigest()[:12]
            return MutationGate("owner-suggestion", digest, None)
    random_review = summary.get("random_mutation")
    if isinstance(random_review, Mapping) and random_review.get("due") is True:
        return MutationGate(
            "random",
            f"cycle {random_review['cycle_number']}",
            str(random_review["selected_lane"]),
        )
    pending_reviews = summary.get("pending_incident_reviews", [])
    if pending_reviews:
        incident = pending_reviews[0]
        return MutationGate(
            "incident-review",
            str(incident.get("task_id") or incident.get("claim_id") or "pending incident"),
            None,
        )
    for key, kind in (
        ("pending_deadline_mutations", "deadline-mutation"),
        ("pending_integrity_mutations", "integrity-mutation"),
    ):
        pending = summary.get(key, [])
        if pending:
            item = pending[0]
            return MutationGate(
                kind,
                str(item.get("task_id") or item.get("claim_id") or kind),
                None,
            )
    return None


def _gate_signature(
    pending_reviews: Sequence[object],
    pending_mutations: Sequence[object],
) -> str:
    return "gate:" + json.dumps(
        {
            "reviews": pending_reviews,
            "mutations": pending_mutations,
        },
        sort_keys=True,
        separators=(",", ":"),
    )


def wait_for_supervision_event(
    state_path: Path,
    lineage_id: str,
    *,
    now: Callable[[], float] = time.time,
    sleep: Callable[[float], None] = time.sleep,
) -> SupervisionEvent | None:
    """Wait without polling until durable clock state requires a successor."""
    with DeadlineHarness(state_path) as harness:
        summary = harness.list_tasks(now=now())
        restart = _restart_state(summary, lineage_id)
        if restart.required:
            return SupervisionEvent(restart, f"restart:{restart.generation}")

        pending_reviews = summary["pending_incident_reviews"]
        pending_mutations = [
            *summary.get("pending_deadline_mutations", []),
            *summary.get("pending_integrity_mutations", []),
        ]
        if pending_reviews or pending_mutations:
            signature = _gate_signature(pending_reviews, pending_mutations)
            return SupervisionEvent(None, signature)

        deadlines = [
            (float(task["deadline_at"]), str(task["claim_id"]))
            for task in summary["tasks"]
            if task.get("deadline_at") is not None and task.get("state") == "running"
        ]
        if not deadlines:
            return None
        deadline_at, claim_id = min(deadlines)

    sleep(max(0.0, deadline_at - now()))

    with DeadlineHarness(state_path) as harness:
        expired = harness.expire_claim(
            lineage_id,
            claim_id,
            now=max(deadline_at, now()),
        )
        if expired["incident"] is None:
            return None
        summary = harness.list_tasks(now=max(deadline_at, now()))
        pending_reviews = summary["pending_incident_reviews"]
        pending_mutations = [
            *summary.get("pending_deadline_mutations", []),
            *summary.get("pending_integrity_mutations", []),
        ]
        # expire_claim also returns the one durable incident after that incident
        # has been reviewed and resolved. Only live gates justify a successor;
        # the immutable historical deadline must not re-arm itself.
        if not pending_reviews and not pending_mutations:
            return None
        signature = _gate_signature(pending_reviews, pending_mutations)
    return SupervisionEvent(None, signature)


def consume_supervision_event(
    consumed: set[str],
    event: SupervisionEvent,
) -> None:
    """Reject replay before a durable event can launch another child."""
    if event.signature in consumed:
        raise SupervisorError(f"Supervision event replayed: {event.signature}")
    consumed.add(event.signature)


def work_is_complete(
    workspace: Path,
    state_path: Path,
    lineage_id: str,
) -> bool:
    """Derive completion from the FS, current ledger, and live clock gates."""
    ledger = workspace / ".de67" / "work-ledger.md"
    if not ledger.is_file() or not state_path.is_file():
        return False
    try:
        specification = resolve(workspace / ".de67")
    except SpecificationError:
        return False
    if RED_DFS_CLAIM.search(specification.text):
        return False
    if ACTIVE_LEDGER_ITEM.search(ledger.read_text(encoding="utf-8")):
        return False

    with DeadlineHarness(state_path) as harness:
        harness.coordinator_restart_status(lineage_id)
        clock = harness.list_tasks()
    restart = clock["coordinator_restart"]
    unresolved_tasks = [
        task for task in clock["tasks"] if task.get("state") != "completed"
    ]
    if (
        unresolved_tasks
        or clock["pending_incident_reviews"]
        or clock.get("pending_deadline_mutations")
        or clock.get("pending_integrity_mutations")
        or clock.get("claim_clock_migration_conflicts")
        or clock.get("reopened_unaccepted_claims")
        or any(
            gap.get("status") == "open"
            for gap in clock.get("closure_gaps", [])
        )
    ):
        return False
    if restart is not None and restart["pending"]:
        return False

    # The FS is the product contract. A cadence that became due on its final
    # terminal window cannot manufacture work after that contract is all green.
    return True


def ledger_has_active_work(workspace: Path) -> bool:
    """Detect unchecked ledger items; executability also depends on route facts."""
    ledger = workspace / ".de67" / "work-ledger.md"
    return ledger.is_file() and ACTIVE_LEDGER_ITEM.search(
        ledger.read_text(encoding="utf-8")
    ) is not None


def ledger_has_only_blocked_work(workspace: Path) -> bool:
    """Stop when the ledger contains blockers but no executable item."""

    ledger = workspace / ".de67" / "work-ledger.md"
    if not ledger.is_file():
        return False
    text = ledger.read_text(encoding="utf-8")
    return (
        ACTIVE_LEDGER_ITEM.search(text) is None
        and BLOCKED_LEDGER_ITEM.search(text) is not None
    )


def blocked_ledger_audit_reason(workspace: Path) -> str | None:
    """Name one durable fresh-coordinator audit for the exact blocked ledger."""

    ledger = workspace / ".de67" / "work-ledger.md"
    if not ledger_has_only_blocked_work(workspace):
        return None
    digest = hashlib.sha256(ledger.read_bytes()).hexdigest()
    return f"{BLOCKED_AUDIT_PREFIX}{digest}"


def blocked_ledger_was_audited(
    audit_reason: str,
    restart: RestartState,
) -> bool:
    """Stop only after one acknowledged coordinator saw this exact blocker set."""

    return (
        not restart.required
        and restart.run_id is not None
        and restart.reason == audit_reason
    )


def blocked_work_is_quiescent(state_path: Path, lineage_id: str) -> bool:
    """Require a confirmed blocker to leave no ticking task or live gate."""

    with DeadlineHarness(state_path) as harness:
        summary = harness.list_tasks()
    return not any(
        task.get("state") == "running" for task in summary["tasks"]
    ) and not any(
        summary.get(key)
        for key in (
            "pending_incident_reviews",
            "pending_deadline_mutations",
            "pending_integrity_mutations",
            "claim_clock_migration_conflicts",
        )
    )


def dfs_has_open_work(workspace: Path) -> bool:
    """Return whether the functional contract/ledger still has open work."""
    try:
        specification = resolve(workspace / ".de67")
    except SpecificationError:
        return False
    if RED_DFS_CLAIM.search(specification.text):
        return True
    ledger = workspace / ".de67" / "work-ledger.md"
    return ledger.is_file() and ACTIVE_LEDGER_ITEM.search(
        ledger.read_text(encoding="utf-8")
    ) is not None


def ordinary_worker_evidence_contract() -> str:
    return ('Select task context around the assigned outcome: established premises, missing knowledge, '
            'source/scenario/evidence entrypoints, constraints and ownership. Discovery may itself be '
            'the bounded assignment; do not manufacture a scout for a known route. Dispatch prepares '
            'the ledger assignment and current frontier automatically, including a valid zero-bundle '
            'brief. To supply a custom selection use context_library.py --workspace WORKSPACE --task '
            'TASK: put --name NAME --source PATH [--kind skill] [--dependency SOURCE_OR_EVIDENCE]; '
            'reuse --name NAME --revision SHA256; prepare --brief PATH [--bundle NAME ...] '
            '[--handoff PATH]. Dependencies bind the facts behind a summary, not just the summary file. '
            'catalog shows metadata; show --revision SHA256 [--section HEADING] retrieves exact text; '
            'assemble previews selection. Limits are visible in catalog and adjustable from evidence; '
            'replace stale/irrelevant knowledge at capacity rather than truncating constraints. '
            'A changed task assignment invalidates its custom preparation; refresh selection or use '
            'a successor task while preserving independent accepted work. Material premise changes '
            'require explicit correction to affected live workers; issued packets remain immutable. '
            'Judge actual worker input and useful returned work, not bundle counts or ceremony. '
            'Return a material architecture, ownership or independent prerequisite decision to Sol; '
            'ordinary investigation and changed tactics remain within worker autonomy.')


def coordinator_ledger_contract() -> str:
    """Return the coordinator's authority over the active work projection."""
    return (
        "Own the current work-ledger projection. Split or merge independently actionable work, "
        "including simultaneous assignments for one red claim; preserve accepted proof and "
        "recoverable work through existing durable transitions. Closed diagnostic/documentation "
        "gaps do not close unfinished product proof. Trust the agent doing repository work to "
        "repair implementation, tooling, fixtures, scenarios, registry bindings or observation "
        "paths within authority. Trust the agent coordinating the claim to invent a materially "
        "different route; a retry fuse ends a strategy, not recoverable work. "
        "Choose subdivisions for useful execution/progress, not worker counts or gates. Four to "
        "seven plot spokes are a presentation preference, never required work. Preserve nested "
        "`  - Subtasks:` with `    - [STATE] ID :: DESCRIPTION`; STATE is open, active, done or "
        "finding, and ID is stable lowercase hyphenated text. These are not deadline tasks, "
        "closure gaps or acceptance gates. Split a prerequisite that depends on its own output "
        "into a non-credit observation/bootstrap step followed by independent validation. "
        "When settling repairs/exceptions, reconcile active bug intake and ledger with accepted "
        "results; retain originals in evidence. For consequential cross-task obstruction, unclear "
        "responsibility, contradictory specification/evidence or scope/ownership conflict after "
        "local diagnosis, consult the owner's conversational mutator through the project's "
        "authenticated interface context. Send task/run/revision, outcome/divergence, evidence/lessons, "
        "live ownership and decision needed; continue independent work. If unavailable, keep the "
        "exact adapter/setup gap executable. Advice cannot grant owner repair/scope authority, "
        "promote owner queue entries, resolve gates or permit shared edits; method changes retain "
        "exclusive review. Requests/timeouts/replies do not close the originating task."
    )


def worker_handoff_contract() -> str:
    return (
        "Record the outcome and exit condition as `  - Assignment TASK-ID: ...` in the ledger "
        "before opening exploration. Keep independent assignments separate; whole-claim tasks remain valid. "
        "Give workers the outcome, relevant evidence and writable scope; offer a starting approach only "
        "when useful, and let them adapt. Give coupled shared-file edits one owner. Distinguish installed "
        "implementation targets from writable candidate paths: ordinary workers stage method patches; "
        "the exclusive mutator promotes them. Carry selected facts and evidence limits through "
        "context_library.py prepare/selected_context, refreshing changed dependencies. "
        "Choose the model and named worker for the remaining work; reuse useful context when it saves "
        "total effort. Create a worker with explicit model/effort when none fits or independent work needs one. "
        "After opening a task, ask the kernel for its prepared handoff. Assign through worker_library.py "
        "using the returned exact task and hash-bound packet; it records verified ownership and creates "
        "or resumes the conversation. Native spawn_agent remains available with the prepared exact "
        "task_name, fork_turns=none and packet arguments. Use those arguments directly. The full packet is "
        "worker input, not coordinator context. Do not invoke claim-worker yourself. "
        "Independently actionable opened tasks need one distinct worker each before waiting, with "
        "exclusive edit/runtime ownership. A worker's prior task must be terminal before a new assignment; "
        "questions and partial results continue the same task. A /root/<task-name> is not a worker UUID. "
        "Coordinate useful work, wait when no decision remains, and record returns before routing. "
        "Timeout is not completion; do not finish while a worker result is outstanding. "
        "Attempts without verified handoff are abandoned."
    )


def nested_worker_contract() -> str:
    return ('Sol and primary workers may use native Luna helpers with fork_turns="none", '
            'an explicit model and suitable effort. Helpers need no deadline task, FS slice, '
            'ledger entry or claim, and never own coordinator state. If unavailable, use bounded '
            'local retrieval. The caller owns the result and collects or stops helpers before return.')


def worker_result_ingress_contract() -> str:
    """Order a verified worker return before ledger-derived route selection."""
    return (
        "Keep owner execution corrections in the marked current owner-contract section of .de67/WEC.md "
        "as pending instructions until the responsible worker acknowledges them and returns applied "
        "evidence, or an explicit deferral reason. The ledger is a replaceable projection, not their "
        "sole store. Send relevant changes through worker_library.py message for named workers or native messaging for native children; new briefs already "
        "include the marked owner contract. Verify receipt and use, not just file preservation. "
        "Progress messages and questions from a live worker are nonterminal conversation: "
        "respond when useful without demanding a result receipt, pausing the task, or creating a "
        "ledger item for each observation. Use checkpoint-worker only when evidence needs durable "
        "continuation; a message or checkpoint does not settle the task or restart its clock. "
        "A verified ordinary-worker return is durable-state ingress, not a route decision. "
        'Judge both whether the change delivers the assigned behavior and whether it leaves the affected '
        'code understandable and maintainable. Inspect the relevant diff and surrounding callers. Look '
        'for duplicated state, unclear ownership or lifetime, unnecessary branches or abstractions, and '
        'departures from established APIs and native types. Check whether the tests could expose a '
        'mistaken implementation assumption. Resolve material differences against the FS; passing tests '
        'do not erase them. Where a concrete problem warrants correction, prefer simplifying the affected '
        'implementation. Use an independent reviewer when a specific unresolved risk warrants another '
        'perspective, giving them the code and question to investigate. Stop when the material concerns '
        "are settled. Treat the worker's requested disposition as evidence "
        "to judge, not as terminal authority; persist its result separately from that judgment. "
        "Before recording a formal finding, name the assigned-outcome exit that its evidence proves. "
        "A return that only disproves the current strategy is nonterminal even when the worker names "
        "no successor. If the assigned outcome still has an authorized repository repair, rerun, "
        "observation, or materially different implementation route, preserve the returned evidence "
        "and choose the next route. Use checkpoint-worker and keep the same task live. "
        "Resume the bound worker through worker_library.py message, or native followup_task, when its accumulated understanding remains "
        "useful, including questions, partial returns, failed tests, diagnosis, repair, and verification. "
        "A changed tactic alone does not require fresh context. Consider a fresh worker for substantially "
        "different context or concrete evidence that the existing worker cannot continue effectively. "
        "Ending an assignment and interrupting execution are separate decisions: completion, cancellation, "
        "a concrete need to stop ongoing actions, or demonstrated inability can justify stopping; "
        "communication and partial results alone do not. "
        "A returned turn may leave a recoverable unfinished task; resume it with message, including "
        "after a due review under the fresh coordinator. Review may proceed once all worker turns "
        "are returned and no longer editing. Preserve the task, missed deadline and returned evidence. "
        "Context exhaustion alone does not require abandonment or replacement. "
        "When accepting returned work, reconcile all worker/helper game attempts, including failed "
        "startups and replacements, against their PID/birth identity and broker. Arrange graceful "
        "closure through the responsible owner or an explicit retained-session handoff; a finished "
        "report or hidden window is not OS exit. Preserve an exact cleanup blocker without erasing "
        "valid gameplay proof. Before every terminal transition, persist one identity-bound worker result receipt through "
        "record-worker-receipt. It preserves the achieved outcome or first divergence, material "
        "repository/runtime changes, journal entries, tests and live actions, evidence ceilings, "
        "exact continuation bindings and artifacts, accepted no-replay work, first remaining "
        "boundary, and narrow queries. Then cite that receipt in exactly one matching deadline-harness "
        "terminal transition before executing DE67_POLICY_DECIDE_ARGV_JSON again. "
        "A completed attempt settles only that task; "
        "when it is bound to a closure gap, close that gap and preserve its proof while any sibling "
        "gaps remain open. Accept the whole claim only through the separate claim-acceptance "
        "transition after every required gap is closed. This is the only pre-decision transition: "
        "the policy kernel derives worker result facts from that committed state. Do not wait for "
        "the live task to terminalize itself, do not ask the worker to mutate DE67 state, and do "
        "not record a second terminal transition when the task is already terminal. Ordinary test "
        "failure remains inside the worker task unless the returned evidence meets the assigned-outcome "
        "finding boundary."
    )


def recovery_frontier_snapshot(workspace: Path) -> str:
    """Render the small durable frontier a recovery coordinator must resolve."""
    try:
        specification = resolve(workspace / ".de67")
        dfs_text = specification.text
        label = "FS"
    except SpecificationError:
        dfs_text = ""
        label = "FS"
    ledger = workspace / ".de67" / "work-ledger.md"
    red_lamps = (
        [line for line in dfs_text.splitlines()
         if line.startswith("- [ ] 🔴 ")]
    )
    executable_entries = (
        [line for line in ledger.read_text(encoding="utf-8").splitlines()
         if line.startswith("- [ ] ")]
        if ledger.is_file()
        else []
    )
    return (
        label + " red lamps:\n"
        + ("\n".join(red_lamps) if red_lamps else "(none)")
        + "\nLedger executable entries:\n"
        + ("\n".join(executable_entries) if executable_entries else "(none)")
    )


def coordinator_recovery_contract(
    opportunity: int, workspace: Path, reason: str | None = None,
) -> str:
    """Return bounded corrective context after a failed coordinator decision."""
    if opportunity <= 1 or opportunity > COORDINATOR_DECISION_OPPORTUNITIES:
        raise SupervisorError(f"Invalid coordinator decision opportunity: {opportunity}")
    final = (
        " This is the final automatic opportunity; another failed decision stops the supervisor "
        "for attended diagnosis."
        if opportunity == COORDINATOR_DECISION_OPPORTUNITIES
        else ""
    )
    snapshot = recovery_frontier_snapshot(workspace)
    return (
        f"Recovery: this is coordinator decision opportunity {opportunity} of "
        f"{COORDINATOR_DECISION_OPPORTUNITIES}.\n"
        + (f"Previous decision failed: {reason}\n" if reason else "")
        + f"{snapshot}\n"
        "If either list is nonempty, Phase 3 is unfinished. Execute the exact policy decision. "
        "If DE67_COORDINATOR_ACK_ARGV_JSON is present, execute that exact array before any "
        "dispatch or other state transition. "
        "If the returned action cannot advance, use causal judgment to repair authorized repository "
        "implementation, game-harness code, fixtures, SQL schemas, queries, migrations or ledger projection. "
        "For an installed DE67 method obstruction, preserve the reproducer and stage a candidate "
        "through the existing exclusive mutation handoff; that is a valid recovery outcome. "
        "Use deadline-harness APIs for DE67 clock transitions. Then rerun the "
        "policy decision and continue de67 3. Do not open a replacement task merely because an "
        "earlier attempt was abandoned. When policy returns spawn_worker, use its exact injected "
        "task/packet identity through named-worker assign or the concrete native spawn_agent call. "
        "Durably close or block existing work only when evidence proves completion "
        "or a genuinely external blocker. A durable external blocker, proved FS completion, or no red lamp and no "
        "executable ledger entry remains a valid stop."
        + final
    )


def coordinator_context_contract() -> str:
    """Keep policy authority and role-specific evidence choices distinct."""
    return (
        "Follow policy ownership and lifecycle requirements. Named reads are starting points, "
        "not a whitelist; inspect evidence that can change the decision. Reading does not "
        "authorize dispatch or mutation. Worker briefs carry their outcome, constraints, useful "
        "evidence and handoff obligations, not coordinator-only routing instructions."
    )


def named_worker_contract(workspace: Path) -> str:
    cli = [sys.executable, str(Path(__file__).with_name("worker_library.py")),
           "--workspace", str(workspace)]
    return (
        "Named worker library: " + json.dumps(cli) + ". Use list for compact names/jobs and "
        "assignment status; create NAME --job TEXT --model MODEL --effort EFFORT, describe NAME "
        "--job TEXT [--model MODEL --effort EFFORT], assign NAME with the kernel's packet arguments, message NAME --message TEXT, "
        "wait NAME --timeout SECONDS, and retire NAME. Choose a descriptive reusable job and adjust "
        "it when responsibility changes. An idle worker can change model/effort for the next job while "
        "retaining its conversation. Reuse remains a judgment about useful context and competence, "
        "not an exact task-name match, compulsory reuse, worker quota or periodic summary ritual. "
        "Git checkpoints are optional snapshots chosen by the coordinator, not task completion or acceptance. "
        "A failed checkpoint remains visible for repair and does not stop delivery or a due review. "
        "The library runs through the App Server transport and returns compact result references in "
        "the existing coordinator mailbox. Use its message/wait for named workers. For a CLI wait, "
        "keep exec_command and any required write_stdin polling inside one functions.exec invocation "
        "and emit the completed wait result; a shell yield alone needs no model decision. "
        "Bound the whole wait by the next task deadline and at most 60 seconds. Read raw worker "
        "event logs only to resolve a concrete question that status, messages or result artifacts leave open. Native collaboration "
        "tools address native children. After a mutation a fresh Sol reads the current FS, ledger, owner "
        "corrections, relevant accepted evidence and worker catalogue. Write the next brief from that "
        "current frontier: changed outcome/premises, useful retained facts, invalidated assumptions and "
        "first unresolved question. Idle named workers retain their conversations across Sol restarts. "
        "Explicitly deliver changed premises in the assignment; old conversation is knowledge to "
        "reconcile, never current authority. Do not replay earlier successful work or attach the old "
        "coordinator transcript. Combine reusable worker context with selected context_library bundles; "
        "retrieve missing detail only when it can change the work. Named dispatch delivers current "
        "assignment content directly and resends standing instructions only when they changed, "
        "preserving the complete hash-bound packet for inspection. Current owner corrections are "
        "always included; retained context never cancels them."
    )


def live_coordination_contract() -> str:
    return "Use worker_library.py message/wait for named workers; use native send_message/followup_task/wait_agent for native children. Native workers address /root. Sol owns direction and scope. While a worker owns execution, resolve an independent source, interface or acceptance question when its answer can change the current work or successor, and send the useful finding through that worker's message route. Preserve exclusive input/edit ownership and do not duplicate the worker's investigation. At useful coordination boundaries, inspect compact CPU, memory pressure/swap and task-owned process state when it can reveal missed cleanup or a stalled operation. Reconcile games, bridges, helpers and builds with active assignments and explicit retained-session purposes. Sol owns noticing and arranging cleanup when a worker forgets; verified finished or unused task-owned processes may be closed through their supported graceful lifecycle without asking again. Check exact PID/birth identity and OS exit, preserving journals, unsaved state and exclusive input ownership. Low CPU, age or RSS alone does not establish disuse; keep active work, Josef's applications, shared services in use and intentionally retained sessions. For a stalled active operation, diagnose the specific process and coordinate bounded recovery with its worker; report an exact unresolved ownership or recovery gap while continuing independent work. Use no monitoring daemon, fixed polling quota or arbitrary kill threshold. If no such question remains, waiting is correct. When execution reveals a substantial independent tooling problem, decide who owns it without interrupting useful live state or handing off merely for a changed tactic. After hard diagnosis, reassess whether Luna can perform substantial remaining execution at lower total cost. Apply the returned-work judgment in the result ingress contract before settling consequential changes. Carry forward current findings, retiring source-specific advice once regression proof absorbs it. Notice recurring context/tool obstructions in worker evidence and commission a bounded repair through the existing work ledger; validate that it removes the demonstrated repetition. DE67 method edits retain exclusive mutation/guard ownership. Do not convert worker discovery transcripts into coordinator context or request parallel summaries, periodic reports or new receipts. Use current-root token_usage in work_context as feedback with helper/handoff costs; distinguish expected savings from measured use, never quotas. At worker return or a natural adoption event, reassess remaining whole-claim proof against its immutable deadline; schedule the unmet boundary with an honest estimate. When no useful decision remains, use the worker's wait route, waking no later than the item deadline; apply policy at routing transitions."


def coordinator_continuation_prompt() -> str:
    """Resume the same session without replaying its stable role contracts."""
    return (
        "You are the coordinator. Continue the same DE-67 coordinator lifecycle under the role contracts already in this "
        "session. Ordinary worker results and findings are state events, not a reason to stop. "
        "Ingest any pending worker return under the existing receipt contract before routing; "
        "otherwise execute DE67_POLICY_DECIDE_ARGV_JSON without a shell for the next action brief. "
        "Use current state and changed evidence, not a replay of prior instructions or history. "
        "Continue live coordination; wait_agent when no useful decision remains while workers "
        "are outstanding. The external supervisor owns coordinator/reviewer launches and restarts; "
        "you own coordination of task processes, including supported graceful cleanup under the "
        "existing identity, unsaved-state and input-ownership safeguards.\n"
    )


def worker_selection_contract() -> str:
    return (
        "Model choice: use Luna for playtesting, clear execution and ordinary repairs; Terra for "
        "coupled implementation or difficult diagnosis; Astra when stronger implementation judgment "
        "may reduce uncertainty or rework. Sol coordinates and is not an ordinary worker. After a "
        "hard repair, prefer Luna for substantial remaining playtesting when the handoff saves total "
        "work; preserve useful context and live ownership. "
        "Thinking effort: for coding, generally prefer max on Luna/Terra and low on Astra. For other "
        "work, choose effort for the reasoning needed. These are preferences, not escalation gates "
        "or quotas; use judgment. Choose only available pairs from model_choices. Judge completed outcomes and total "
        "work, including helpers, handoffs and retries. Give each worker a focused assignment; an "
        "Astra worker is separate from the persistent mutator."
    )


def coordinator_prompt(
    workspace: Path,
    state_path: Path,
    lineage_id: str,
    run_id: str,
    generation: int | None,
    restart_reason: str | None = None,
) -> str:
    lines = [
        f"You are the coordinator for Phase-3 delivery in {workspace}.",
        common_guidance(workspace),
        communication_contract(workspace, "coordinator", "mutator"),
        worker_result_ingress_contract(),
        "Do not read packaged DE-67 SKILL.md, kernel, role, reference, or guideline prose during delivery.",
        "The hash-bound .de67/phase3-policy.d67 file is the machine-canonical routing policy.",
        "Before each coordinator routing transition, execute the argument array in DE67_POLICY_DECIDE_ARGV_JSON as a subprocess without a shell.",
        current_owner_contract(workspace),
        "The FS (Functional Specification, in .de67/FS.md) defines code behavior; the ledger owns delivery assignments and evidence references. Read the relevant contract and reconcile changed outcomes. Historical acceptance retains its scope; the current owner contract decides fresh proof.",
        coordinator_context_contract(),
        "Current work, independent task results, receipt search, recorded dispatch relationships and runner "
        "metadata and latest-assigned coordinator-tree token usage are available from this argument array: " + json.dumps([
            sys.executable, str(Path(__file__).with_name("work_context.py")),
            "--workspace", str(workspace), "--state", str(state_path), "--lineage", lineage_id])
        + ". Narrow by --claim, --task, --receipt or --contains; --full retains every matching receipt. "
        "Use --usage for token usage only and --usage --full for source coverage and freshness. "
        "The infrastructure indexes existing records automatically. Choose relevant context and use a "
        "local query or Luna helper for a specific unresolved extraction or interpretation question. "
        "Packets preserve independent contributions and exact references; shared claim or recency alone "
        "does not establish predecessor authority. Missing context remains visible and merits targeted discovery.",
        live_coordination_contract(),
        named_worker_contract(workspace),
        "Write every owner-facing text field rendered on the hosted dashboard in simple English. This includes ledger items, latest findings, waiting work, mutation or incident summaries, and any FS summary that the dashboard displays. First explain what happened and why it matters in terms any reader can understand. Then preserve the necessary technical identifiers and evidence, state what remains or happens next, and use one concrete statement per sentence. If the simple explanation exposes a contradiction or a missing causal step, record that problem instead of hiding it behind technical language. Internal machine state and FS detail that the dashboard does not display do not need this rewrite.",
        "Never review, apply, or resolve a mutation. When the compiled policy says retire_for_mutation_review, dispatch no worker, make no guidance change, and exit immediately so the external supervisor can run the exclusive reviewer.",
        "Do not infer policy from workspace guideline prose; those files are legacy differential fixtures on this branch.",
        worker_selection_contract(),
        "For every newly spawned ordinary worker, set fork_turns=\"none\" and provide a self-contained task brief. Never omit model selection or pass coordinator or predecessor history. Reusing an already relevant worker remains allowed.",
        coordinator_ledger_contract(),
        ordinary_worker_evidence_contract(),
        worker_handoff_contract(),
        nested_worker_contract(),
        "Use DE67_DEADLINE_STATE and DE67_LINEAGE as the exact clock and lineage for every state transition; do not infer replacements.",
        "The external supervisor owns coordinator/reviewer launches and restarts. Do not launch your successor.",
    ]
    if generation is not None:
        lines.append(
            "Before dispatching work, execute the argument array in "
            "DE67_COORDINATOR_ACK_ARGV_JSON as a subprocess without a shell; "
            "it acknowledges this exact restart generation."
        )
        lines.append(
            "The mutation retired every earlier claim deadline. Read the current ledger and "
            "remaining FS route, then set one new whole-item deadline you can honestly deliver. "
            "Include worker startup, diagnosis, implementation, repair, build, rerun, evidence "
            "return, coordination, known unknowns, and an uncertainty margin; inherit no prior duration."
        )
        if restart_reason:
            lines.append(
                "Treat this exact owner-authorized restart reason as current input: "
                + restart_reason
            )
    lines.append("Continue from the durable accepted frontier until the next required retirement or FS completion.")
    return "\n".join(lines) + "\n"


def mutation_maintenance_contract() -> str:
    return (
        "Find meaningful problems and friction in how coordinators and workers operate and in the "
        "outcomes they produce. Follow the strongest evidence of a useful improvement.\n\n"
        "Make efficiency a standing responsibility of this normally scheduled review, across all "
        "authorized agent work: implementation, playtests, tools, context, handoffs and DE67 itself. "
        "Optimize total effort per correctly completed outcome, not a lower hourly burn obtained by "
        "stopping useful work, weakening proof or shrinking the owner's outcome. Use available recent "
        "usage, roughly the last eight to ten hours, from available usage summaries and source logs "
        "to locate costly patterns. A fresh weekly pace multiplier above one, supported by sustained "
        "high recent use, raises the priority and depth of investigation; expensive useful work is "
        "not itself waste. Distinguish account-wide allowance from this campaign's consumption, "
        "cached from fresh tokens, and observed history from estimates. A latest multiplier alone "
        "does not prove it stayed high throughout that window; disclose missing or stale accounting. "
        "This guidance adds no burn-triggered review, background optimizer or fixed audit quota.\n\n"
        "During this review, use bounded Luna log investigations when useful to trace costly "
        "patterns through the context the affected agents actually received. Give each helper a "
        "concrete causal question and relevant evidence; obtain findings and original handles, not "
        "another transcript. Astra owns the synthesis. Telescope is optional for difficult semantic "
        "retrieval, never a required expense. Ask what made the expensive work necessary and whether "
        "a better interface, reusable result, clearer handoff or simpler test setup can remove that "
        "need while preserving the agreed proof. These are possibilities, not a mandatory checklist. "
        "Keep asking why and follow the chain upstream: a worker's mistake may originate in coordinator "
        "instructions, and a coordinator's mistake in mutated guidelines. Go beyond describing events "
        "to identify the context that produced the behavior. Distinguish explicit instructions from "
        "downstream interpretations, and do not force an instruction-based explanation when the "
        "evidence points elsewhere.\n\n"
        "Use context engineering to repair the earliest preventable systemic cause. Prefer removing "
        "or simplifying the policy that caused the problem and undoing its downstream consequences "
        "over accumulating rules around symptoms. Improve information delivery or interagent "
        "communication where useful. Give agents clearer context and greater freedom to exercise "
        "judgment. Own authorized context changes directly; commission needed tooling through the "
        "existing ledger for Sol. Preserve necessary evidence and ownership. Refine coordinator and "
        "worker context for useful decisions and effective work, and keep Josef's conversation in "
        "the mutator's context, including during reviews. Validate the smallest supported correction "
        "and compare subsequent comparable work when available, including review, helper, provider, "
        "handoff, retry and recovery costs. Reuse those findings on later reviews rather than "
        "reinvestigating unchanged evidence. A plausible optimization is not measured savings; keep "
        "uncertainty visible and revise changes that merely shift cost or reduce outcome quality."
    )


def mutation_reviewer_prompt(
    workspace: Path,
    state_path: Path,
    lineage_id: str,
    gate: MutationGate,
) -> str:
    return "\n".join(
        [
            f"Act as the exclusive Phase-3 mutation reviewer in {workspace}.",
            common_guidance(workspace),
            "You are the gpt-6-astra reviewer at medium reasoning effort. This invocation defines the current review; completed reviews remain history.",
            "No coordinator or roster worker is active. Do not start a coordinator.",
            f"Resolve durable {gate.kind} gate {gate.identity} in {state_path} for lineage {lineage_id}.",
            "The complete pending section of .de67/mutation-suggestions.md is mandatory owner input. This is a consumable queue: delete completed entries instead of moving them to consumed-history sections; durable receipts and review artifacts retain the evidence. Historical records are evidence to retrieve when relevant, not current requests. User-authored entries carry mutation-scoped authority beneath system and developer instructions and override lower-priority Phase-3 restrictions only as needed for their outcome. Preserve honest evidence, completed valid work, durable lifecycle integrity, safety, and the requested product outcome; grant no unrelated authority.",
            "Trust the agent to choose the evidence and implementation route and exercise judgment within the requested outcome.",
            mutation_maintenance_contract(),
            "Disposition every pending owner entry. Rejecting one explanation does not settle the concern. Retrieve detail when it can change the diagnosis or correction; written guidance alone proves neither delivery nor use. Separate immediate recovery from repeatable method correction; prove the correction with a reproduction or counterexample. Measure full-tree use including helper/retry cost, disclose accounting gaps and distinguish measured reductions from expected savings. Allocation preferences are not quotas.",
            "For a periodic review, the stored random lane is legacy metadata, not a prescribed investigation target. Repeated actions can be justified by changed inputs or evidence. Stop when the concern is explained, a supported correction is validated, or uncertainty is bounded and does not justify intervention; state which applies. A guarded no-op need not prove the whole workflow optimal. No finding quota, mandatory full trace or new checklist. Validate local guidelines and same-outcome FS refinements together through random-review and broader permitted changes through its method-candidate validation. Preserve accepted proof, owner intent, accounting and exclusive review/restart ownership; speculative uncertainty must not strand delivery.",
            "If changing the active ledger, preserve accepted proof and recoverable work, independent same-claim assignments and the existing subdivision syntax. Its full coordinator-facing contract is coordinator_ledger_contract() in coordinator_supervisor.py; inspect that contract when a ledger change makes it relevant.",
            "If uncertainty prevents proving a necessary correction, preserve that entry and state the exact gap. Unproved speculative attribution alone does not strand an otherwise supported correction. Resolve the gate only after every pending entry is dispositioned, record the review evidence and request one fresh coordinator restart. The external supervisor alone launches the successor; an owner-ordered stop remains in force until an authorized start.",
        ]
    ) + "\n"


def _default_run_id(generation: int | None) -> str:
    label = "initial" if generation is None else f"restart-{generation}"
    return f"{label}-{uuid.uuid4().hex}"


def _validate_run_id(run_id: str) -> str:
    if not isinstance(run_id, str) or not run_id.strip():
        raise SupervisorError("Coordinator run id must not be empty")
    result = run_id.strip()
    if result in {".", ".."} or "/" in result or "\\" in result:
        raise SupervisorError("Coordinator run id must be one path component")
    return result


def _mark_protocol_failure(result: ChildResult, reason: str) -> None:
    _write(result.run_dir / "supervisor_error.txt", reason.rstrip() + "\n")
    _write(result.run_dir / "status.txt", "FAILED\n")


def mutation_reviewer_environment(
    gate: MutationGate, extra_env: Mapping[str, str] | None = None,
) -> dict[str, str]:
    """Produce reviewer launch settings alongside the current prompt."""
    reviewer_env = dict(extra_env or {})
    reviewer_env.update(
        {
            "DE67_COORDINATOR_MODEL": "gpt-6-astra",
            "DE67_COORDINATOR_REASONING_EFFORT": "medium",
        }
    )
    return reviewer_env


def run_mutation_reviewer(
    runner_command: Sequence[str],
    workspace: Path,
    state_path: Path,
    lineage_id: str,
    run_root: Path,
    gate: MutationGate,
    *,
    extra_env: Mapping[str, str] | None = None,
    run_id: str | None = None,
) -> ChildResult:
    # Load prompt and machine bindings from the same installed producer. A
    # retained supervisor must not pair a fresh runner with stale launch context.
    prompt_source = _fresh_prompt_module()
    reviewer_env = prompt_source.mutation_reviewer_environment(gate, extra_env)
    return run_child(
        runner_command,
        workspace,
        state_path,
        lineage_id,
        run_root,
        run_id or f"mutation-{uuid.uuid4().hex}",
        None,
        extra_env=reviewer_env,
        prompt_override=prompt_source.mutation_reviewer_prompt(
            workspace, state_path, lineage_id, gate
        ),
        role="mutation-reviewer",
    )


def _complete_mutation_review(
    runner_command: Sequence[str],
    workspace: Path,
    state_path: Path,
    lineage_id: str,
    run_root: Path,
    gate: MutationGate,
    *,
    extra_env: Mapping[str, str] | None,
    reviewed_gates: set[tuple[str, str]] | None = None,
    journal: SupervisorJournal | None = None,
) -> RestartState:
    consumed = reviewed_gates if reviewed_gates is not None else set()
    with DeadlineHarness(state_path) as harness:
        harness.retire_claim_clocks_for_mutation(
            lineage_id,
            f"{gate.kind} {gate.identity}",
        )
    while True:
        gate_key = (gate.kind, gate.identity)
        if gate_key in consumed:
            raise SupervisorError(
                f"Mutation gate repeated without resolution: {gate.kind} {gate.identity}"
            )
        consumed.add(gate_key)
        run_id = f"mutation-{uuid.uuid4().hex}"
        if journal is not None:
            journal.begin(
                "mutation-reviewer",
                f"{gate.kind}:{gate.identity}",
                run_id,
            )
        result = run_mutation_reviewer(
            runner_command,
            workspace,
            state_path,
            lineage_id,
            run_root,
            gate,
            extra_env=extra_env,
            run_id=run_id,
        )
        if journal is not None:
            journal.finish(
                run_id,
                "succeeded" if result.launched and result.exit_code == 0 else "failed",
                None if result.exit_code == 0 else f"exit code {result.exit_code}",
            )
        if not result.launched or result.exit_code != 0:
            raise SupervisorError(
                f"Mutation reviewer failed for {gate.kind} {gate.identity}; ordinary work remains stopped"
            )
        remaining = mutation_gate(state_path, lineage_id, workspace)
        if remaining is not None:
            gate = remaining
            continue
        # The reviewer may have atomically promoted the method while this
        # parent was alive.  Its imported DeadlineHarness must not decide the
        # post-review projection: load the installed source from disk before
        # the one authorized successor is claimed.
        with _fresh_deadline_harness()(state_path) as harness:
            harness.synchronize_dfs_statuses()
        restart = read_clock(state_path, lineage_id)
        if not restart.required or restart.generation is None:
            _mark_protocol_failure(
                result,
                "Mutation reviewer resolved the gate without requesting one fresh coordinator",
            )
            raise SupervisorError("Resolved mutation lacks its fresh-coordinator handoff")
        return restart


def _fresh_prompt_module() -> types.ModuleType:
    """Load prompt producers from current disk, bypassing stale module state."""
    source_path = Path(__file__).resolve()
    try:
        source = source_path.read_text(encoding="utf-8")
        code = compile(source, str(source_path), "exec")
    except (OSError, SyntaxError, UnicodeError) as error:
        raise SupervisorError(
            f"Current supervisor prompt source is unavailable or corrupt: {error}"
        ) from error
    module = types.ModuleType(f"_de67_prompt_source_{uuid.uuid4().hex}")
    module.__file__ = str(source_path)
    module.__package__ = None
    # dataclasses (and other introspecting decorators) resolve annotations via
    # sys.modules while the freshly compiled module is executing.
    sys.modules[module.__name__] = module
    try:
        exec(code, module.__dict__)
    except Exception as error:
        raise SupervisorError(
            f"Current supervisor prompt source could not be loaded: {error}"
        ) from error
    return module


def _fresh_deadline_harness() -> type[DeadlineHarness]:
    """Load the post-review delivery writer from the installed source.

    This mirrors fresh prompt loading.  It keeps a long-lived supervisor from
    using a pre-promotion status writer for the one post-review handoff, while
    retaining the same parent, state database, journal, and child-launch
    authority.
    """
    source_path = Path(__file__).with_name("deadline_harness.py")
    try:
        source = source_path.read_text(encoding="utf-8")
        code = compile(source, str(source_path), "exec")
    except (OSError, SyntaxError, UnicodeError) as error:
        raise SupervisorError(
            f"Current delivery projection source is unavailable or corrupt: {error}"
        ) from error
    module = types.ModuleType(f"_de67_delivery_source_{uuid.uuid4().hex}")
    module.__file__ = str(source_path)
    module.__package__ = None
    sys.modules[module.__name__] = module
    try:
        exec(code, module.__dict__)
    except Exception as error:
        raise SupervisorError(
            f"Current delivery projection source could not be loaded: {error}"
        ) from error
    harness_type = getattr(module, "DeadlineHarness", None)
    if not isinstance(harness_type, type):
        raise SupervisorError("Current delivery projection source has no DeadlineHarness")
    return harness_type


def run_child(
    runner_command: Sequence[str],
    workspace: Path,
    state_path: Path,
    lineage_id: str,
    run_root: Path,
    run_id: str,
    generation: int | None,
    *,
    extra_env: Mapping[str, str] | None = None,
    resume_session_id: str | None = None,
    decision_opportunity: int = 1,
    recovery_reason: str | None = None,
    prompt_override: str | None = None,
    role: str = "coordinator",
) -> ChildResult:
    if not runner_command:
        raise SupervisorError("Runner command must not be empty")
    run_id = _validate_run_id(run_id)
    run_dir = run_root / run_id
    try:
        run_dir.mkdir(parents=False, exist_ok=False)
    except OSError as error:
        raise SupervisorError(f"Cannot create coordinator run directory: {error}") from error

    try:
        prompt_source = None if prompt_override is not None else _fresh_prompt_module()
        if prompt_override is not None:
            prompt = prompt_override
        elif resume_session_id is None:
            restart_reason = None
            if generation is not None:
                with DeadlineHarness(state_path) as harness:
                    restart = harness.coordinator_restart_status(lineage_id).get(
                        "coordinator_restart"
                    )
                if restart and restart.get("generation") == generation:
                    restart_reason = restart.get("reason")
            prompt = prompt_source.coordinator_prompt(
                workspace, state_path, lineage_id, run_id, generation, restart_reason
            )
        else:
            prompt = prompt_source.coordinator_continuation_prompt()
    except SupervisorError as error:
        _mark_protocol_failure(run_result := ChildResult(run_id, run_dir, 1, False), str(error))
        raise
    if decision_opportunity > 1:
        prompt = prompt.rstrip() + "\n" + coordinator_recovery_contract(
            decision_opportunity, workspace, recovery_reason
        ) + "\n"
    _write(run_dir / "status.txt", "STARTING\n")

    environment = os.environ.copy()
    if extra_env is not None:
        environment.update(extra_env)
    environment.update(
        {
            "PYTHONIOENCODING": "utf-8",
            "DE67_COORDINATOR_RUN_ID": run_id,
            "DE67_PROCESS_ROLE": role,
            "DE67_DEADLINE_STATE": str(state_path),
            "DE67_LINEAGE": lineage_id,
            "DE67_WORKSPACE": str(workspace),
            "DE67_SUPERVISOR_PID": str(os.getpid()),
            "DE67_COORDINATOR_SESSION_FILE": str(run_dir / "session_id.txt"),
            "DE67_BLOCKER_ADAPTER_STATE": str(
                workspace / ".de67" / "state" / "blocker-adapter-state.json"
            ),
            "DE67_POLICY_DECIDE_ARGV_JSON": json.dumps(
                [
                    sys.executable,
                    str(Path(__file__).resolve().with_name("policy_kernel.py")),
                    "decide",
                    "--policy",
                    str(workspace / ".de67" / "phase3-policy.d67"),
                    "--workspace",
                    str(workspace),
                    "--state",
                    str(state_path),
                    "--lineage",
                    lineage_id,
                ]
            ),
            "DE67_POLICY_GUARD_ARGV_JSON": json.dumps(
                [
                    sys.executable,
                    str(Path(__file__).resolve().with_name("policy_kernel.py")),
                    "guard",
                    "--candidate",
                    str(workspace / ".de67" / "state" / "phase3-policy.candidate.json"),
                    "--contracts",
                    str(workspace / ".de67" / "phase3-contracts.json"),
                    "--output",
                    str(workspace / ".de67" / "state" / "phase3-policy.candidate.d67"),
                ]
            ),
        }
    )
    # Owner-conversation launch input belongs only to that invocation, even when
    # the owner mutator starts this supervisor through an inherited environment.
    environment.pop("DE67_INITIAL_INPUT_PATH", None)
    if resume_session_id is None:
        environment.pop("DE67_COORDINATOR_RESUME_SESSION", None)
    else:
        environment["DE67_COORDINATOR_RESUME_SESSION"] = resume_session_id
    if generation is None:
        environment.pop("DE67_COORDINATOR_RESTART_GENERATION", None)
        environment.pop("DE67_COORDINATOR_ACK_ARGV_JSON", None)
    else:
        environment["DE67_COORDINATOR_RESTART_GENERATION"] = str(generation)
        deadline_script = Path(__file__).resolve().with_name("deadline_harness.py")
        environment["DE67_COORDINATOR_ACK_ARGV_JSON"] = json.dumps(
            [
                sys.executable,
                str(deadline_script),
                "ack-restart",
                "--state",
                str(state_path),
                "--lineage",
                lineage_id,
                "--generation",
                str(generation),
                "--run-id",
                run_id,
            ]
        )

    # Tool execution may use an app-server process that did not inherit this environment.
    binding_keys = ["DE67_COORDINATOR_RUN_ID", "DE67_PROCESS_ROLE", "DE67_DEADLINE_STATE",
                    "DE67_LINEAGE", "DE67_WORKSPACE", "DE67_SUPERVISOR_PID",
                    "DE67_POLICY_DECIDE_ARGV_JSON", "DE67_COORDINATOR_ACK_ARGV_JSON"]
    if role == "mutation-reviewer":
        binding_keys.append("DE67_POLICY_GUARD_ARGV_JSON")
    bindings = {key: json.loads(environment[key]) if key.endswith("_ARGV_JSON") else environment[key]
                for key in binding_keys if key in environment}
    prompt = prompt.rstrip() + (
        "\nCurrent invocation bindings (use these values directly; tool subprocesses need not "
        "inherit the runner environment). Missing environment variables do not require discovery "
        "or authorize a replacement binding. Execute the supplied argument arrays without a shell.\n"
        "```json\n" + json.dumps(bindings, ensure_ascii=False, sort_keys=True) + "\n```\n"
    )
    _write(run_dir / "prompt.txt", prompt)

    command = [*runner_command, "--cwd", str(workspace)]
    process: subprocess.Popen[str]
    try:
        process = subprocess.Popen(
            command,
            cwd=workspace,
            env=environment,
            stdin=subprocess.PIPE,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            text=True,
            encoding="utf-8",
            close_fds=True,
        )
    except OSError as error:
        _write(
            run_dir / "supervisor_error.txt",
            f"Failed to launch coordinator runner: {error}\n",
        )
        _write(run_dir / "exit_code.txt", "1\n")
        _write(run_dir / "status.txt", "FAILED\n")
        return ChildResult(run_id, run_dir, 1, False)
    exit_code = 1
    try:
        _write(run_dir / "pid.txt", f"{process.pid}\n")
        _write(run_dir / "status.txt", "RUNNING\n")
        if process.stdin is None:
            raise SupervisorError("Runner stdin pipe was not created")
        process.communicate(prompt)
        exit_code = process.returncode
    except BaseException:
        if process.poll() is None:
            process.kill()
            exit_code = process.wait()
        _write(run_dir / "exit_code.txt", f"{exit_code}\n")
        _write(run_dir / "status.txt", "FAILED\n")
        raise

    _write(run_dir / "exit_code.txt", f"{exit_code}\n")
    _write(run_dir / "status.txt", "DONE\n" if exit_code == 0 else "FAILED\n")
    return ChildResult(run_id, run_dir, exit_code, True)


def _run_supervisor_locked(
    state_path: str | Path,
    lineage_id: str,
    workspace: str | Path,
    runner_command: Sequence[str],
    run_root: str | Path,
    *,
    extra_env: Mapping[str, str] | None = None,
    run_id_factory: Callable[[int | None], str] = _default_run_id,
    event_waiter: Callable[[Path, str], SupervisionEvent | None] | None = None,
    blocker_waiter: Callable[[Path, str], BlockerReply | None] | None = None,
) -> int:
    state = Path(state_path).expanduser().resolve()
    workdir = Path(workspace).expanduser().resolve()
    records = Path(run_root).expanduser().resolve()
    if not workdir.is_dir():
        raise SupervisorError(f"Workspace does not exist: {workdir}")
    if not lineage_id.strip():
        raise SupervisorError("Lineage id must not be empty")
    if not runner_command:
        raise SupervisorError("Runner command must not be empty")
    wait_for_event = event_waiter or wait_for_supervision_event

    # Existing run records prove this is a supervisor recovery, not the first
    # bootstrap that may legitimately begin with a seeded task clock.
    recovering_existing_runs = records.is_dir() and any(records.iterdir())
    if recovering_existing_runs:
        reconcile_dead_run_records(records)
    terminalize_unowned_worker_windows(
        state,
        lineage_id,
        runtime_worker_owners(workdir, extra_env, state_path=state, lineage_id=lineage_id),
        include_unclaimed=recovering_existing_runs,
    )

    restart = read_clock(state, lineage_id)
    blocked_audit = blocked_ledger_audit_reason(workdir)
    if blocked_audit is not None:
        if (
            blocked_ledger_was_audited(blocked_audit, restart)
            and blocked_work_is_quiescent(state, lineage_id)
        ):
            if blocker_waiter is None:
                return 0
            reply = blocker_waiter(workspace=workdir, lineage_id=lineage_id)
            if reply is None:
                return 0
            with DeadlineHarness(state) as harness:
                requested = harness.request_coordinator_restart(
                    lineage_id,
                    f"owner replied through blocker adapter {reply.reply_message_id}",
                )["coordinator_restart"]
            restart = _restart_state(
                {"lineage_id": lineage_id, "coordinator_restart": requested},
                lineage_id,
            )
        if not restart.required:
            with DeadlineHarness(state) as harness:
                requested = harness.request_coordinator_restart(
                    lineage_id,
                    blocked_audit,
                )["coordinator_restart"]
            restart = _restart_state(
                {"lineage_id": lineage_id, "coordinator_restart": requested},
                lineage_id,
            )
    elif work_is_complete(workdir, state, lineage_id):
        return 0

    records.mkdir(parents=True, exist_ok=True)
    journal = SupervisorJournal(
        state,
        lineage_id,
        f"supervisor-{os.getpid()}-{uuid.uuid4().hex}",
        os.environ.get("DE67_SUPERVISOR_START_TOKEN"),
    )
    reviewed_gates: set[tuple[str, str]] = set()
    consumed_events: set[str] = set()
    gate = mutation_gate(state, lineage_id, workdir)
    if gate is not None:
        restart = _complete_mutation_review(
            runner_command,
            workdir,
            state,
            lineage_id,
            records,
            gate,
            extra_env=extra_env,
            reviewed_gates=reviewed_gates,
            journal=journal,
        )

    generation = restart.generation if restart.required else None
    # A restarted supervisor resumes the durable owner of any still-live
    # workers. It never launches a fresh coordinator that would have to adopt
    # another session's children or dispatch duplicates.
    resume_session_id = None if restart.required else active_worker_coordinator_session(state, lineage_id)
    attempted_generations: set[int] = set()
    failed_decision_opportunities = 0
    recovery_reason = None
    while True:
        if generation is not None:
            if generation in attempted_generations:
                raise SupervisorError(
                    f"Restart generation {generation} was already attempted"
                )
            attempted_generations.add(generation)

        run_id = _validate_run_id(run_id_factory(generation))
        if generation is not None:
            if restart.expected_run_id is not None:
                raise SupervisorError(
                    f"Restart generation {generation} is already claimed by "
                    f"{restart.expected_run_id}"
                )
            with DeadlineHarness(state) as harness:
                harness.claim_coordinator_restart(
                    lineage_id, generation, run_id
                )

        launch_fingerprint = supervision_fingerprint(state, lineage_id, workdir)
        # A corrective opportunity carries new failure context even when the durable
        # frontier is unchanged. Keep it distinct in the journal's replay guard.
        journal_frontier = launch_fingerprint
        if failed_decision_opportunities:
            journal_frontier += f":recovery-{failed_decision_opportunities + 1}"
        journal.begin("coordinator", journal_frontier, run_id)
        result = run_child(
            runner_command,
            workdir,
            state,
            lineage_id,
            records,
            run_id,
            generation,
            extra_env=extra_env,
            resume_session_id=resume_session_id,
            decision_opportunity=failed_decision_opportunities + 1,
            recovery_reason=recovery_reason,
        )

        # A runner that could not be launched is a concrete environment blocker,
        # not a coordinator result that another coordinator can repair.
        if not result.launched:
            return result.exit_code if result.exit_code > 0 else 1

        # Unclaimed clocks are phantom work and must fail closed. A verified
        # worker claim belongs to this supervised coordinator session, however,
        # and survives a transient CLI process exit so the same session can
        # resume its structured worker-result lifecycle.
        terminalize_unowned_worker_windows(
            state, lineage_id, runtime_worker_owners(workdir, extra_env, state_path=state, lineage_id=lineage_id)
        )
        progressed = supervision_fingerprint(state, lineage_id, workdir)
        journal.finish(
            run_id,
            "progressed" if launch_fingerprint != progressed else (
                "succeeded" if result.exit_code == 0 else "failed"
            ),
            None if result.exit_code == 0 else f"exit code {result.exit_code}",
        )

        # This is the only clock read after this child exits. There is no polling loop.
        after = read_clock(state, lineage_id)

        if generation is not None:
            if after.required and after.generation == generation:
                opportunity = failed_decision_opportunities + 1
                _mark_protocol_failure(
                    result,
                    f"Coordinator did not acknowledge restart generation {generation} "
                    f"on decision opportunity {opportunity} of "
                    f"{COORDINATOR_DECISION_OPPORTUNITIES}",
                )
                if opportunity >= COORDINATOR_DECISION_OPPORTUNITIES:
                    return result.exit_code if result.exit_code != 0 else 1
                with DeadlineHarness(state) as harness:
                    harness.release_coordinator_restart_claim(
                        lineage_id, generation, result.run_id
                    )
                failed_decision_opportunities += 1
                attempted_generations.remove(generation)
                session_path = result.run_dir / "session_id.txt"
                session_id = (
                    session_path.read_text(encoding="utf-8").strip()
                    if session_path.is_file()
                    else ""
                )
                active_owner = active_worker_coordinator_session(state, lineage_id)
                resume_session_id = active_owner or (
                    None
                    if failed_decision_opportunities + 1
                    == COORDINATOR_DECISION_OPPORTUNITIES
                    else session_id or None
                )
                restart = read_clock(state, lineage_id)
                continue
            if (
                not after.required
                and after.generation == generation
                and after.run_id != result.run_id
            ):
                _mark_protocol_failure(
                    result,
                    f"Restart generation {generation} was acknowledged by a different run",
                )
                return 1
            if after.generation is not None and after.generation < generation:
                _mark_protocol_failure(result, "Coordinator restart generation moved backwards")
                return 1

        gate = mutation_gate(state, lineage_id, workdir)
        if gate is not None:
            after = _complete_mutation_review(
                runner_command,
                workdir,
                state,
                lineage_id,
                records,
                gate,
                extra_env=extra_env,
                reviewed_gates=reviewed_gates,
                journal=journal,
            )

        blocked_audit = blocked_ledger_audit_reason(workdir)
        if blocked_audit is not None and blocked_work_is_quiescent(
            state, lineage_id
        ):
            if blocker_waiter is None:
                return 0
            reply = blocker_waiter(workspace=workdir, lineage_id=lineage_id)
            if reply is None:
                return 0
            with DeadlineHarness(state) as harness:
                requested = harness.request_coordinator_restart(
                    lineage_id,
                    f"owner replied through blocker adapter {reply.reply_message_id}",
                )["coordinator_restart"]
            after = _restart_state(
                {"lineage_id": lineage_id, "coordinator_restart": requested},
                lineage_id,
            )
        if work_is_complete(workdir, state, lineage_id):
            return 0
        frontier_facts = workspace_facts(workdir, state, lineage_id, now=time.time())
        actionable = bool(frontier_facts & {
            "executable_route", "live_task", "unbound_task",
            "worker_completed", "worker_finding", "worker_abandoned",
        })
        waiting_on_owner = "owner_wait" in frontier_facts and not actionable
        has_executable_work = (
            ledger_has_active_work(workdir) or dfs_has_open_work(workdir)
        ) and not waiting_on_owner
        if waiting_on_owner and not after.required and result.exit_code == 0:
            _write(result.run_dir / "status.txt", "WAITING_FOR_OWNER\n")
        if not after.required and has_executable_work:
            session_path = result.run_dir / "session_id.txt"
            session_id = (
                session_path.read_text(encoding="utf-8").strip()
                if session_path.is_file()
                else ""
            )
            if session_id:
                no_progress = launch_fingerprint == progressed
                if no_progress or result.exit_code != 0:
                    failure = (
                        "Coordinator returned with executable work but made no durable progress"
                        if result.exit_code == 0
                        else f"Coordinator exited with code {result.exit_code} while executable work remains"
                    )
                    if no_progress and result.exit_code != 0:
                        failure += "; made no durable progress"
                    _mark_protocol_failure(result, failure)
                    journal.reject(run_id, failure)
                    if (
                        failed_decision_opportunities + 1
                        >= COORDINATOR_DECISION_OPPORTUNITIES
                    ):
                        _mark_protocol_failure(
                            result,
                            failure + "; Three coordinator decision opportunities were exhausted; "
                            "explicit supervisor startup is required",
                        )
                        return result.exit_code if result.exit_code != 0 else 1
                    failed_decision_opportunities += 1
                    recovery_reason = f"{failure}. Prior run: {run_id}; evidence: {result.run_dir / 'supervisor_error.txt'}."
                else:
                    failed_decision_opportunities = 0
                    recovery_reason = None
                active_owner = active_worker_coordinator_session(state, lineage_id)
                resume_session_id = active_owner or (
                    None
                    if failed_decision_opportunities + 1
                    == COORDINATOR_DECISION_OPPORTUNITIES
                    else session_id
                )
                generation = None
                continue
            if result.exit_code == 0:
                _mark_protocol_failure(
                    result,
                    "Coordinator returned with executable work but no resumable session id",
                )
                return 1
        if not after.required and result.exit_code != 0 and has_executable_work:
            # Process recovery is not a semantic coordinator restart. Start a
            # clean low coordinator against the same durable frontier without
            # manufacturing a mutation generation.
            recovery_reason = f"Coordinator exited with code {result.exit_code} without a resumable session"
            if launch_fingerprint == progressed:
                recovery_reason += "; made no durable progress"
            journal.reject(run_id, recovery_reason)
            if (
                failed_decision_opportunities + 1
                >= COORDINATOR_DECISION_OPPORTUNITIES
            ):
                _mark_protocol_failure(
                    result,
                    "Three coordinator decision opportunities were exhausted; "
                    "explicit supervisor startup is required",
                )
                return result.exit_code
            failed_decision_opportunities += 1
            resume_session_id = None
            generation = None
            continue
        if not after.required:
            if event_waiter is None:
                event = wait_for_supervision_event(state, lineage_id)
            else:
                event = wait_for_event(state, lineage_id)
            if event is None:
                return result.exit_code if result.exit_code > 0 else 0
            try:
                consume_supervision_event(consumed_events, event)
            except SupervisorError as error:
                _mark_protocol_failure(result, str(error))
                return 1
            if event.restart is None:
                gate = mutation_gate(state, lineage_id, workdir)
                if gate is None:
                    _mark_protocol_failure(
                        result,
                        "Mutation event became active before all worker windows were terminal",
                    )
                    return 1
                after = _complete_mutation_review(
                    runner_command,
                    workdir,
                    state,
                    lineage_id,
                    records,
                    gate,
                    extra_env=extra_env,
                    reviewed_gates=reviewed_gates,
                    journal=journal,
                )
            else:
                after = event.restart
        if after.generation is None:
            _mark_protocol_failure(result, "Required coordinator restart lacks a generation")
            return 1
        if after.generation in attempted_generations:
            _mark_protocol_failure(
                result,
                f"Restart generation {after.generation} remains pending after its only attempt",
            )
            return 1
        # A durable semantic restart (mutation, incident retirement, or owner
        # reply) starts a new coordinator lifecycle. Failed decisions from an
        # earlier lifecycle must not consume this one's bounded opportunities.
        failed_decision_opportunities = 0
        restart = after
        generation = after.generation
        resume_session_id = None if restart.required else active_worker_coordinator_session(state, lineage_id)


def run_supervisor(
    state_path: str | Path,
    lineage_id: str,
    workspace: str | Path,
    runner_command: Sequence[str],
    run_root: str | Path,
    *,
    extra_env: Mapping[str, str] | None = None,
    run_id_factory: Callable[[int | None], str] = _default_run_id,
    event_waiter: Callable[[Path, str], SupervisionEvent | None] | None = None,
    blocker_waiter: Callable[[Path, str], BlockerReply | None] | None = None,
) -> int:
    state = Path(state_path).expanduser().resolve()
    with _supervisor_lock(state):
        return _run_supervisor_locked(
            state,
            lineage_id,
            workspace,
            runner_command,
            run_root,
            extra_env=extra_env,
            run_id_factory=run_id_factory,
            event_waiter=event_waiter,
            blocker_waiter=blocker_waiter,
        )


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--state", required=True)
    parser.add_argument("--lineage", required=True)
    parser.add_argument("--workspace", required=True)
    parser.add_argument("--run-root", required=True)
    parser.add_argument("--coordinator-model", default="gpt-5.6-sol")
    parser.add_argument(
        "--coordinator-reasoning-effort",
        choices=("low", "medium", "high", "xhigh", "max", "ultra"),
        default="low",
    )
    parser.add_argument(
        "--runner",
        nargs=argparse.REMAINDER,
        help="Runner command; place this option last",
    )
    parser.add_argument(
        "--blocker-adapter-command-json",
        help=(
            "Optional shell-independent JSON argument array for an external owner-contact "
            "adapter; the supervisor appends the wait/workspace/lineage arguments"
        ),
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    arguments = build_parser().parse_args(argv)
    try:
        blocker_adapter = None
        if arguments.blocker_adapter_command_json is not None:
            try:
                blocker_adapter = SubprocessBlockerAdapter(
                    parse_adapter_command(arguments.blocker_adapter_command_json)
                )
            except BlockerAdapterError as error:
                print(
                    "coordinator supervisor: optional blocker adapter disabled: "
                    f"{error}",
                    file=sys.stderr,
                )
        return run_supervisor(
            arguments.state,
            arguments.lineage,
            arguments.workspace,
            arguments.runner or (),
            arguments.run_root,
            extra_env={
                "DE67_COORDINATOR_MODEL": arguments.coordinator_model,
                "DE67_COORDINATOR_REASONING_EFFORT": (
                    arguments.coordinator_reasoning_effort
                ),
            },
            blocker_waiter=(
                (
                    lambda *, workspace, lineage_id: safe_wait_for_reply(
                        blocker_adapter,
                        workspace=workspace,
                        lineage_id=lineage_id,
                    )
                )
                if blocker_adapter is not None
                else None
            ),
        )
    except (DeadlineError, SupervisorError, OSError) as error:
        print(f"coordinator supervisor: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
