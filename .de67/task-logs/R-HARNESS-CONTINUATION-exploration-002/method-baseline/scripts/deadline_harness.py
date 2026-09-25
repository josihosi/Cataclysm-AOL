#!/usr/bin/env python3
"""Small SQLite-backed deadline tracker for DE-67 task coordination."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import re
import secrets
import sqlite3
import subprocess
import sys
import time
from pathlib import Path
from typing import Any

SCRIPT_ROOT = str(Path(__file__).resolve().parent)
if SCRIPT_ROOT not in sys.path:
    sys.path.insert(0, SCRIPT_ROOT)
from specification import SpecificationError, resolve

SCRIPT_ROOT = str(Path(__file__).resolve().parent)
if SCRIPT_ROOT not in sys.path:
    sys.path.insert(0, SCRIPT_ROOT)
from worker_receipt import (
    WorkerReceiptError,
    compact_worker_receipt,
    normalize_worker_receipt,
    receipt_envelope,
)


RANDOM_INTERVAL_MIN = 20
RANDOM_INTERVAL_MAX = 50
CADENCE_VERSION = 3
UNIVERSAL_RANDOM_INTERVAL = 30
RANDOM_MUTATION_LANES = (
    "test-and-task-guidelines.md",
    "DFS.md",
)
RECENT_FAILURE_VERDICT_LIMIT = 10
INCIDENT_KINDS = ("deadline_miss", "integrity_breach")
METHOD_IGNORED_PARTS = {".git", "__pycache__", ".pytest_cache"}
NORMAL_METHOD_PROTECTED_FILES = (
    "scripts/deadline_harness.py",
    "scripts/mutation_guard.py",
    "tests/test_deadline_harness.py",
    "tests/test_mutation_guard.py",
)
WORKSPACE_METHOD_GUIDELINE_FILES = (
    "test-and-task-guidelines.md",
)
ACTIVE_SKILL_ROOT = Path(__file__).resolve().parents[1]


def _workspace_for_state(state_path: Path | None) -> Path | None:
    if (
        state_path is None
        or state_path.parent.name != "state"
        or state_path.parent.parent.name != ".de67"
    ):
        return None
    return state_path.parent.parent.parent


def _ledger_claim_block(ledger: str, claim_id: str) -> str | None:
    pattern = re.compile(
        r"(?ms)^- \[[ x]\] " + re.escape(claim_id)
        + r"(?=[ \t]+—|[ \t]*$).*?(?=^- \[[ x]\] R-|^## |\Z)"
    )
    match = pattern.search(ledger)
    return match.group(0).rstrip() if match else None


def _replace_ledger_claim_block(ledger: str, claim_id: str, replacement: str) -> str:
    current = _ledger_claim_block(ledger, claim_id)
    if current is None:
        raise DeadlineError(f"Claim {claim_id} has no work-ledger projection")
    return ledger.replace(current, replacement.rstrip(), 1)


def _open_ledger_baseline(block: str) -> str:
    """Recover a reopenable open projection without treating FS prose as evidence."""
    block = re.sub(r"(?m)^  - Durable acceptance:.*\n?", "", block)
    return re.sub(r"\A- \[x\] ", "- [ ] ", block, count=1)


def _method_files(root: Path) -> dict[str, bytes]:
    """Read one complete method tree using the mutation guard's digest surface."""

    result: dict[str, bytes] = {}
    for path in sorted(root.rglob("*")):
        relative = path.relative_to(root)
        if any(part in METHOD_IGNORED_PARTS for part in relative.parts):
            continue
        if path.is_symlink():
            raise DeadlineError(
                f"Active method tree cannot contain symlinks: {relative.as_posix()}"
            )
        if path.is_file():
            result[relative.as_posix()] = path.read_bytes()
    return result


def _files_digest(files: dict[str, bytes]) -> str:
    digest = hashlib.sha256()
    for relative, content in sorted(files.items()):
        encoded = relative.encode("utf-8")
        digest.update(len(encoded).to_bytes(8, "big"))
        digest.update(encoded)
        digest.update(len(content).to_bytes(8, "big"))
        digest.update(content)
    return digest.hexdigest()


def method_tree_digest(root: Path = ACTIVE_SKILL_ROOT) -> str:
    return _files_digest(_method_files(root))


def protected_method_digest(root: Path = ACTIVE_SKILL_ROOT) -> str:
    files = _method_files(root)
    protected = {
        relative: files[relative]
        for relative in NORMAL_METHOD_PROTECTED_FILES
        if relative in files
    }
    if len(protected) != len(NORMAL_METHOD_PROTECTED_FILES):
        missing = sorted(set(NORMAL_METHOD_PROTECTED_FILES) - set(protected))
        raise DeadlineError(
            "Active method tree is missing protected files: " + ", ".join(missing)
        )
    return _files_digest(protected)


def workspace_method_digest(
    state_path: Path | None, root: Path = ACTIVE_SKILL_ROOT
) -> str | None:
    """Project the active workspace guidelines onto the installed method tree."""

    if (
        state_path is None
        or state_path.parent.name != "state"
        or state_path.parent.parent.name != ".de67"
    ):
        return None
    environment = state_path.parent.parent
    guideline_paths = {
        name: environment / name for name in WORKSPACE_METHOD_GUIDELINE_FILES
    }
    if any(not path.is_file() for path in guideline_paths.values()):
        return None
    files = _method_files(root)
    for name, path in guideline_paths.items():
        files[f"assets/environment/{name}"] = path.read_bytes()
    return _files_digest(files)


class DeadlineError(RuntimeError):
    """Raised when a deadline operation would violate the task record."""


class DeadlineHarness:
    """Track immutable claim clocks, worker attempts, and failure incidents."""

    def __init__(self, state_path: str | Path) -> None:
        raw_path = str(state_path)
        if not raw_path.strip():
            raise DeadlineError("State path must not be empty")
        self.state_path: Path | None = None
        if raw_path != ":memory:":
            path = Path(raw_path).expanduser()
            path.parent.mkdir(parents=True, exist_ok=True)
            self.state_path = path.resolve()
            raw_path = str(self.state_path)
        self.connection = sqlite3.connect(raw_path, timeout=30)
        self.connection.row_factory = sqlite3.Row
        self.connection.execute("PRAGMA foreign_keys = ON")
        self.connection.executescript(
            """
            CREATE TABLE IF NOT EXISTS tasks (
                lineage_id TEXT NOT NULL,
                task_id TEXT NOT NULL,
                claim_id TEXT NOT NULL,
                estimate_seconds REAL NOT NULL CHECK (estimate_seconds > 0),
                started_at REAL NOT NULL,
                deadline_at REAL NOT NULL,
                phase_at_dispatch TEXT NOT NULL DEFAULT 'exploration',
                phase_sequence_at_dispatch INTEGER,
                completed_at REAL,
                completion_evidence TEXT,
                integrity_breached_at REAL,
                integrity_reason TEXT,
                terminal_at REAL,
                attempt_terminal_at REAL,
                attempt_terminal_kind TEXT,
                abandoned_at REAL,
                abandonment_reason TEXT,
                closure_gap_id TEXT,
                closure_gap_revision INTEGER,
                supervisor_epoch_generation INTEGER NOT NULL DEFAULT 0,
                PRIMARY KEY (lineage_id, task_id)
            );

            CREATE TABLE IF NOT EXISTS incidents (
                incident_id INTEGER PRIMARY KEY AUTOINCREMENT,
                lineage_id TEXT NOT NULL,
                task_id TEXT NOT NULL,
                kind TEXT NOT NULL CHECK (kind IN ('deadline_miss', 'integrity_breach')),
                recorded_at REAL NOT NULL,
                reason TEXT,
                short_verdict TEXT NOT NULL,
                long_detail TEXT NOT NULL,
                reviewed_at REAL,
                restart_generation INTEGER,
                units INTEGER NOT NULL CHECK (units IN (1, 3)),
                cumulative_before INTEGER NOT NULL,
                cumulative_after INTEGER NOT NULL,
                cadence_threshold INTEGER,
                UNIQUE (lineage_id, task_id, kind),
                FOREIGN KEY (lineage_id, task_id)
                    REFERENCES tasks(lineage_id, task_id)
            );

            CREATE TABLE IF NOT EXISTS worker_findings (
                lineage_id TEXT NOT NULL,
                task_id TEXT NOT NULL,
                kind TEXT NOT NULL CHECK (kind IN ('blocker', 'unexpected')),
                reported_at REAL NOT NULL,
                short_verdict TEXT NOT NULL,
                evidence TEXT NOT NULL,
                PRIMARY KEY (lineage_id, task_id),
                FOREIGN KEY (lineage_id, task_id)
                    REFERENCES tasks(lineage_id, task_id)
            );

            CREATE TABLE IF NOT EXISTS worker_claims (
                lineage_id TEXT NOT NULL,
                task_id TEXT NOT NULL,
                worker_id TEXT NOT NULL,
                coordinator_session_id TEXT NOT NULL,
                supervisor_id TEXT NOT NULL,
                claimed_at REAL NOT NULL,
                last_checkpoint_at REAL NOT NULL,
                released_at REAL,
                release_reason TEXT,
                PRIMARY KEY (lineage_id, task_id),
                FOREIGN KEY (lineage_id, task_id)
                    REFERENCES tasks(lineage_id, task_id)
            );

            CREATE TABLE IF NOT EXISTS worker_checkpoints (
                lineage_id TEXT NOT NULL,
                task_id TEXT NOT NULL,
                sequence INTEGER NOT NULL CHECK (sequence > 0),
                worker_id TEXT NOT NULL,
                kind TEXT NOT NULL,
                evidence TEXT NOT NULL,
                recorded_at REAL NOT NULL,
                PRIMARY KEY (lineage_id, task_id, sequence),
                FOREIGN KEY (lineage_id, task_id)
                    REFERENCES tasks(lineage_id, task_id)
            );

            CREATE TABLE IF NOT EXISTS lineage_binding (
                singleton INTEGER PRIMARY KEY CHECK (singleton = 1),
                lineage_id TEXT NOT NULL
            );

            CREATE TABLE IF NOT EXISTS random_mutation_cycles (
                lineage_id TEXT NOT NULL,
                cycle_number INTEGER NOT NULL CHECK (cycle_number > 0),
                interval_windows INTEGER NOT NULL CHECK (
                    interval_windows BETWEEN 10 AND 50
                ),
                cadence_version INTEGER NOT NULL DEFAULT 3 CHECK (
                    cadence_version IN (1, 2, 3)
                ),
                due_after_terminal_windows INTEGER NOT NULL CHECK (
                    due_after_terminal_windows >= interval_windows
                ),
                selected_lane TEXT NOT NULL CHECK (
                    selected_lane IN (
                        'test-and-task-guidelines.md',
                        'orchestrator-guidelines.md',
                        'DFS.md'
                    )
                ),
                due_task_id TEXT,
                resolution_evidence TEXT,
                ordinary_resolution_evidence TEXT,
                universal_required INTEGER NOT NULL DEFAULT 0 CHECK (
                    universal_required IN (0, 1)
                ),
                universal_resolution_evidence TEXT,
                universal_receipt_id TEXT,
                universal_capability_status TEXT CHECK (
                    universal_capability_status IN (
                        'available', 'unavailable', 'legacy-resolved'
                    )
                ),
                universal_capability_reason TEXT,
                universal_capability_checked_at REAL,
                universal_capability_roster_digest TEXT,
                restart_generation INTEGER,
                PRIMARY KEY (lineage_id, cycle_number)
            );

            CREATE TABLE IF NOT EXISTS coordinator_restart_requests (
                lineage_id TEXT NOT NULL,
                generation INTEGER NOT NULL CHECK (generation > 0),
                requested_at REAL NOT NULL,
                reason TEXT NOT NULL,
                claimed_at REAL,
                expected_run_id TEXT,
                acknowledged_at REAL,
                run_id TEXT,
                PRIMARY KEY (lineage_id, generation),
                CHECK (
                    (claimed_at IS NULL AND expected_run_id IS NULL) OR
                    (claimed_at IS NOT NULL AND expected_run_id IS NOT NULL)
                ),
                CHECK (
                    (acknowledged_at IS NULL AND run_id IS NULL) OR
                    (acknowledged_at IS NOT NULL AND run_id IS NOT NULL)
                )
            );

            CREATE TABLE IF NOT EXISTS external_supervisor_epochs (
                lineage_id TEXT NOT NULL,
                generation INTEGER NOT NULL CHECK (generation > 0),
                normalized_at REAL NOT NULL,
                PRIMARY KEY (lineage_id, generation)
            );

            CREATE TABLE IF NOT EXISTS claim_clocks (
                lineage_id TEXT NOT NULL,
                claim_id TEXT NOT NULL,
                estimate_seconds REAL NOT NULL CHECK (estimate_seconds > 0),
                started_at REAL NOT NULL,
                deadline_at REAL NOT NULL,
                phase TEXT NOT NULL CHECK (phase IN ('exploration', 'closure')),
                migrated_from_task_id TEXT,
                migration_note TEXT,
                PRIMARY KEY (lineage_id, claim_id)
            );

            CREATE TABLE IF NOT EXISTS claim_deadline_generations (
                lineage_id TEXT NOT NULL,
                claim_id TEXT NOT NULL,
                generation INTEGER NOT NULL CHECK (generation > 0),
                estimate_seconds REAL NOT NULL CHECK (estimate_seconds > 0),
                started_at REAL NOT NULL,
                deadline_at REAL NOT NULL,
                armed_by_restart_generation INTEGER,
                retired_at REAL,
                retirement_reason TEXT,
                PRIMARY KEY (lineage_id, claim_id, generation),
                FOREIGN KEY (lineage_id, claim_id)
                    REFERENCES claim_clocks(lineage_id, claim_id)
            );

            CREATE TABLE IF NOT EXISTS claim_deadline_generation_incidents (
                lineage_id TEXT NOT NULL,
                claim_id TEXT NOT NULL,
                generation INTEGER NOT NULL,
                source_task_id TEXT NOT NULL,
                recorded_at REAL NOT NULL,
                short_verdict TEXT NOT NULL,
                long_detail TEXT NOT NULL,
                reviewed_at REAL,
                restart_generation INTEGER,
                PRIMARY KEY (lineage_id, claim_id, generation),
                FOREIGN KEY (lineage_id, claim_id, generation)
                    REFERENCES claim_deadline_generations(
                        lineage_id, claim_id, generation
                    ),
                FOREIGN KEY (lineage_id, source_task_id)
                    REFERENCES tasks(lineage_id, task_id)
            );

            CREATE TABLE IF NOT EXISTS deadline_generation_mutation_components (
                lineage_id TEXT NOT NULL,
                claim_id TEXT NOT NULL,
                generation INTEGER NOT NULL,
                component TEXT NOT NULL CHECK (component IN ('micro', 'macro')),
                resolved_at REAL NOT NULL,
                evidence TEXT NOT NULL,
                receipt_id TEXT,
                no_change_required INTEGER NOT NULL DEFAULT 0
                    CHECK (no_change_required IN (0, 1)),
                PRIMARY KEY (lineage_id, claim_id, generation, component),
                FOREIGN KEY (lineage_id, claim_id, generation)
                    REFERENCES claim_deadline_generation_incidents(
                        lineage_id, claim_id, generation
                    )
            );

            CREATE TABLE IF NOT EXISTS claim_clock_migration_conflicts (
                lineage_id TEXT NOT NULL,
                claim_id TEXT NOT NULL,
                detected_at REAL NOT NULL,
                legacy_clock_options TEXT NOT NULL,
                resolved_at REAL,
                resolution_kind TEXT,
                source_task_id TEXT,
                estimate_seconds REAL,
                started_at REAL,
                deadline_at REAL,
                reason TEXT,
                PRIMARY KEY (lineage_id, claim_id)
            );

            CREATE TABLE IF NOT EXISTS claim_phase_events (
                lineage_id TEXT NOT NULL,
                claim_id TEXT NOT NULL,
                sequence INTEGER NOT NULL CHECK (sequence > 0),
                phase TEXT NOT NULL CHECK (phase IN ('exploration', 'closure')),
                recorded_at REAL NOT NULL,
                basis_task_id TEXT NOT NULL,
                closure_outcome TEXT,
                closure_evidence TEXT,
                closure_remaining_gap TEXT,
                contradicted_premise TEXT,
                PRIMARY KEY (lineage_id, claim_id, sequence),
                FOREIGN KEY (lineage_id, claim_id)
                    REFERENCES claim_clocks(lineage_id, claim_id)
            );

            CREATE TABLE IF NOT EXISTS claim_acceptances (
                lineage_id TEXT NOT NULL,
                claim_id TEXT NOT NULL,
                acceptance_number INTEGER NOT NULL CHECK (acceptance_number > 0),
                task_id TEXT NOT NULL,
                closure_sequence INTEGER,
                accepted_at REAL NOT NULL,
                evidence TEXT NOT NULL,
                invalidated_at REAL,
                invalidation_reason TEXT,
                PRIMARY KEY (lineage_id, claim_id, acceptance_number),
                FOREIGN KEY (lineage_id, claim_id)
                    REFERENCES claim_clocks(lineage_id, claim_id),
                FOREIGN KEY (lineage_id, task_id)
                    REFERENCES tasks(lineage_id, task_id)
            );

            CREATE TABLE IF NOT EXISTS closure_gaps (
                lineage_id TEXT NOT NULL,
                claim_id TEXT NOT NULL,
                closure_sequence INTEGER NOT NULL CHECK (closure_sequence > 0),
                gap_id TEXT NOT NULL,
                opened_at REAL NOT NULL,
                basis_task_id TEXT NOT NULL,
                successor_of_gap_id TEXT,
                successor_of_revision INTEGER,
                reopen_reason TEXT,
                closed_at REAL,
                closed_by_task_id TEXT,
                closure_evidence TEXT,
                PRIMARY KEY (lineage_id, claim_id, closure_sequence, gap_id),
                FOREIGN KEY (lineage_id, claim_id)
                    REFERENCES claim_clocks(lineage_id, claim_id),
                CHECK (
                    (closed_at IS NULL AND closed_by_task_id IS NULL
                     AND closure_evidence IS NULL) OR
                    (closed_at IS NOT NULL AND closed_by_task_id IS NOT NULL
                     AND closure_evidence IS NOT NULL)
                )
            );

            CREATE TABLE IF NOT EXISTS closure_gap_revisions (
                lineage_id TEXT NOT NULL,
                claim_id TEXT NOT NULL,
                closure_sequence INTEGER NOT NULL CHECK (closure_sequence > 0),
                gap_id TEXT NOT NULL,
                revision INTEGER NOT NULL CHECK (revision > 0),
                recorded_at REAL NOT NULL,
                basis_task_id TEXT NOT NULL,
                description TEXT NOT NULL,
                proof_route TEXT NOT NULL,
                PRIMARY KEY (
                    lineage_id, claim_id, closure_sequence, gap_id, revision
                ),
                FOREIGN KEY (lineage_id, claim_id, closure_sequence, gap_id)
                    REFERENCES closure_gaps(
                        lineage_id, claim_id, closure_sequence, gap_id
                    )
            );

            CREATE TABLE IF NOT EXISTS claim_deadline_incidents (
                lineage_id TEXT NOT NULL,
                claim_id TEXT NOT NULL,
                source_task_id TEXT NOT NULL,
                recorded_at REAL NOT NULL,
                short_verdict TEXT NOT NULL,
                long_detail TEXT NOT NULL,
                reviewed_at REAL,
                restart_generation INTEGER,
                PRIMARY KEY (lineage_id, claim_id),
                FOREIGN KEY (lineage_id, claim_id)
                    REFERENCES claim_clocks(lineage_id, claim_id),
                FOREIGN KEY (lineage_id, source_task_id)
                    REFERENCES tasks(lineage_id, task_id)
            );

            CREATE TABLE IF NOT EXISTS deadline_mutation_components (
                lineage_id TEXT NOT NULL,
                claim_id TEXT NOT NULL,
                component TEXT NOT NULL CHECK (component IN ('micro', 'macro')),
                resolved_at REAL NOT NULL,
                evidence TEXT NOT NULL,
                receipt_id TEXT,
                PRIMARY KEY (lineage_id, claim_id, component),
                FOREIGN KEY (lineage_id, claim_id)
                    REFERENCES claim_deadline_incidents(lineage_id, claim_id)
            );

            CREATE TABLE IF NOT EXISTS integrity_mutation_components (
                lineage_id TEXT NOT NULL,
                task_id TEXT NOT NULL,
                component TEXT NOT NULL CHECK (component IN ('micro', 'macro')),
                resolved_at REAL NOT NULL,
                evidence TEXT NOT NULL,
                receipt_id TEXT,
                PRIMARY KEY (lineage_id, task_id, component),
                FOREIGN KEY (lineage_id, task_id)
                    REFERENCES tasks(lineage_id, task_id)
            );

            CREATE TABLE IF NOT EXISTS normal_method_receipts (
                receipt_id TEXT PRIMARY KEY,
                lineage_id TEXT NOT NULL,
                task_id TEXT NOT NULL,
                claim_id TEXT NOT NULL,
                incident_kind TEXT NOT NULL CHECK (
                    incident_kind IN ('deadline_miss', 'integrity_breach')
                ),
                validated_at REAL NOT NULL,
                candidate_digest TEXT NOT NULL,
                changed_paths TEXT NOT NULL,
                protected_baseline_digest TEXT NOT NULL,
                live_tree_digest TEXT NOT NULL,
                UNIQUE (lineage_id, task_id, incident_kind, receipt_id),
                FOREIGN KEY (lineage_id, task_id)
                    REFERENCES tasks(lineage_id, task_id)
            );

            CREATE TABLE IF NOT EXISTS universal_review_receipts (
                receipt_id TEXT PRIMARY KEY,
                lineage_id TEXT NOT NULL,
                cycle_number INTEGER NOT NULL CHECK (cycle_number > 0),
                validated_at REAL NOT NULL,
                candidate_digest TEXT NOT NULL,
                changed_paths TEXT NOT NULL,
                interval_windows INTEGER NOT NULL CHECK (interval_windows = 30),
                selected_lane TEXT NOT NULL CHECK (selected_lane = 'DFS.md'),
                reviewer_model TEXT NOT NULL CHECK (reviewer_model = 'gpt-5.6-sol'),
                reviewer_effort TEXT NOT NULL CHECK (reviewer_effort = 'ultra'),
                capability_roster_digest TEXT,
                UNIQUE (lineage_id, cycle_number, receipt_id),
                FOREIGN KEY (lineage_id, cycle_number)
                    REFERENCES random_mutation_cycles(lineage_id, cycle_number)
            );

            CREATE TRIGGER IF NOT EXISTS claim_clock_identity_is_immutable
            BEFORE UPDATE ON claim_clocks
            WHEN NEW.lineage_id IS NOT OLD.lineage_id
              OR NEW.claim_id IS NOT OLD.claim_id
              OR NEW.estimate_seconds IS NOT OLD.estimate_seconds
              OR NEW.started_at IS NOT OLD.started_at
              OR NEW.deadline_at IS NOT OLD.deadline_at
            BEGIN
                SELECT RAISE(ABORT, 'claim clock identity and deadline are immutable');
            END;

            CREATE TRIGGER IF NOT EXISTS claim_clock_cannot_be_deleted
            BEFORE DELETE ON claim_clocks
            BEGIN
                SELECT RAISE(ABORT, 'claim clocks are append-only');
            END;

            CREATE TRIGGER IF NOT EXISTS claim_phase_events_cannot_change
            BEFORE UPDATE ON claim_phase_events
            BEGIN
                SELECT RAISE(ABORT, 'claim phase events are append-only');
            END;

            CREATE TRIGGER IF NOT EXISTS claim_phase_events_cannot_be_deleted
            BEFORE DELETE ON claim_phase_events
            BEGIN
                SELECT RAISE(ABORT, 'claim phase events are append-only');
            END;

            CREATE TRIGGER IF NOT EXISTS claim_acceptance_core_is_immutable
            BEFORE UPDATE ON claim_acceptances
            WHEN NEW.lineage_id IS NOT OLD.lineage_id
              OR NEW.claim_id IS NOT OLD.claim_id
              OR NEW.acceptance_number IS NOT OLD.acceptance_number
              OR NEW.task_id IS NOT OLD.task_id
              OR NEW.accepted_at IS NOT OLD.accepted_at
              OR NEW.evidence IS NOT OLD.evidence
            BEGIN
                SELECT RAISE(ABORT, 'claim acceptance evidence is immutable');
            END;

            CREATE TRIGGER IF NOT EXISTS closure_gap_core_is_immutable
            BEFORE UPDATE ON closure_gaps
            WHEN NEW.lineage_id IS NOT OLD.lineage_id
              OR NEW.claim_id IS NOT OLD.claim_id
              OR NEW.closure_sequence IS NOT OLD.closure_sequence
              OR NEW.gap_id IS NOT OLD.gap_id
              OR NEW.opened_at IS NOT OLD.opened_at
              OR NEW.basis_task_id IS NOT OLD.basis_task_id
              OR OLD.closed_at IS NOT NULL
              OR NEW.closed_at IS NULL
              OR NEW.closed_by_task_id IS NULL
              OR NEW.closure_evidence IS NULL
            BEGIN
                SELECT RAISE(ABORT, 'closure gap identity and disposition are immutable');
            END;

            CREATE TRIGGER IF NOT EXISTS closure_gaps_cannot_be_deleted
            BEFORE DELETE ON closure_gaps
            BEGIN
                SELECT RAISE(ABORT, 'closure gaps are append-only');
            END;

            CREATE TRIGGER IF NOT EXISTS closure_gap_revisions_cannot_change
            BEFORE UPDATE ON closure_gap_revisions
            BEGIN
                SELECT RAISE(ABORT, 'closure gap revisions are append-only');
            END;

            CREATE TRIGGER IF NOT EXISTS closure_gap_revisions_cannot_be_deleted
            BEFORE DELETE ON closure_gap_revisions
            BEGIN
                SELECT RAISE(ABORT, 'closure gap revisions are append-only');
            END;

            CREATE TRIGGER IF NOT EXISTS integrity_mutation_components_cannot_change
            BEFORE UPDATE ON integrity_mutation_components
            BEGIN
                SELECT RAISE(ABORT, 'integrity mutation components are append-only');
            END;

            CREATE TRIGGER IF NOT EXISTS integrity_mutation_components_cannot_be_deleted
            BEFORE DELETE ON integrity_mutation_components
            BEGIN
                SELECT RAISE(ABORT, 'integrity mutation components are append-only');
            END;

            CREATE TRIGGER IF NOT EXISTS universal_review_receipts_cannot_change
            BEFORE UPDATE ON universal_review_receipts
            BEGIN
                SELECT RAISE(ABORT, 'universal review receipts are append-only');
            END;

            CREATE TRIGGER IF NOT EXISTS universal_review_receipts_cannot_be_deleted
            BEFORE DELETE ON universal_review_receipts
            BEGIN
                SELECT RAISE(ABORT, 'universal review receipts are append-only');
            END;
            """
        )
        task_columns = {
            row["name"]
            for row in self.connection.execute("PRAGMA table_info(tasks)").fetchall()
        }
        needs_terminal_backfill = "terminal_at" not in task_columns
        if needs_terminal_backfill:
            self.connection.execute("ALTER TABLE tasks ADD COLUMN terminal_at REAL")
            self.connection.executescript(
                """
                UPDATE tasks AS current
                SET terminal_at = (
                    SELECT MIN(event_at) FROM (
                        SELECT current.completed_at AS event_at
                        UNION ALL SELECT current.integrity_breached_at
                        UNION ALL SELECT reported_at FROM worker_findings
                            WHERE lineage_id = current.lineage_id
                              AND task_id = current.task_id
                        UNION ALL SELECT recorded_at FROM incidents
                            WHERE lineage_id = current.lineage_id
                              AND task_id = current.task_id
                    )
                )
                WHERE current.terminal_at IS NULL;
                """
            )
        if "phase_at_dispatch" not in task_columns:
            self.connection.execute(
                "ALTER TABLE tasks ADD COLUMN phase_at_dispatch TEXT NOT NULL DEFAULT 'exploration'"
            )
        if "phase_sequence_at_dispatch" not in task_columns:
            self.connection.execute(
                "ALTER TABLE tasks ADD COLUMN phase_sequence_at_dispatch INTEGER"
            )
        if "attempt_terminal_at" not in task_columns:
            self.connection.execute("ALTER TABLE tasks ADD COLUMN attempt_terminal_at REAL")
        if "attempt_terminal_kind" not in task_columns:
            self.connection.execute("ALTER TABLE tasks ADD COLUMN attempt_terminal_kind TEXT")
        if "abandoned_at" not in task_columns:
            self.connection.execute("ALTER TABLE tasks ADD COLUMN abandoned_at REAL")
        if "abandonment_reason" not in task_columns:
            self.connection.execute("ALTER TABLE tasks ADD COLUMN abandonment_reason TEXT")
        if "closure_gap_id" not in task_columns:
            self.connection.execute("ALTER TABLE tasks ADD COLUMN closure_gap_id TEXT")
        if "closure_gap_revision" not in task_columns:
            self.connection.execute(
                "ALTER TABLE tasks ADD COLUMN closure_gap_revision INTEGER"
            )
        if "supervisor_epoch_generation" not in task_columns:
            self.connection.execute(
                "ALTER TABLE tasks ADD COLUMN supervisor_epoch_generation "
                "INTEGER NOT NULL DEFAULT 0"
            )
        if "deadline_generation" not in task_columns:
            self.connection.execute(
                "ALTER TABLE tasks ADD COLUMN deadline_generation INTEGER NOT NULL DEFAULT 1"
            )
        generation_columns = {
            row[1]
            for row in self.connection.execute(
                "PRAGMA table_info(claim_deadline_generations)"
            ).fetchall()
        }
        if "retired_at" not in generation_columns:
            self.connection.execute(
                "ALTER TABLE claim_deadline_generations ADD COLUMN retired_at REAL"
            )
        if "retirement_reason" not in generation_columns:
            self.connection.execute(
                "ALTER TABLE claim_deadline_generations ADD COLUMN retirement_reason TEXT"
            )
        self.connection.execute(
            """
            INSERT OR IGNORE INTO claim_deadline_generations (
                lineage_id, claim_id, generation, estimate_seconds,
                started_at, deadline_at
            )
            SELECT lineage_id, claim_id, 1, estimate_seconds, started_at, deadline_at
            FROM claim_clocks
            """
        )
        gap_columns = {
            row["name"]
            for row in self.connection.execute(
                "PRAGMA table_info(closure_gaps)"
            ).fetchall()
        }
        if "successor_of_gap_id" not in gap_columns:
            self.connection.execute(
                "ALTER TABLE closure_gaps ADD COLUMN successor_of_gap_id TEXT"
            )
        if "successor_of_revision" not in gap_columns:
            self.connection.execute(
                "ALTER TABLE closure_gaps ADD COLUMN successor_of_revision INTEGER"
            )
        if "reopen_reason" not in gap_columns:
            self.connection.execute(
                "ALTER TABLE closure_gaps ADD COLUMN reopen_reason TEXT"
            )
        self.connection.executescript(
            """
            CREATE TRIGGER IF NOT EXISTS closure_gap_successor_is_immutable
            BEFORE UPDATE ON closure_gaps
            WHEN NEW.successor_of_gap_id IS NOT OLD.successor_of_gap_id
              OR NEW.successor_of_revision IS NOT OLD.successor_of_revision
              OR NEW.reopen_reason IS NOT OLD.reopen_reason
            BEGIN
                SELECT RAISE(ABORT, 'closure gap successor provenance is immutable');
            END;
            """
        )
        restart_columns = {
            row["name"]
            for row in self.connection.execute(
                "PRAGMA table_info(coordinator_restart_requests)"
            ).fetchall()
        }
        if "claimed_at" not in restart_columns:
            self.connection.execute(
                "ALTER TABLE coordinator_restart_requests ADD COLUMN claimed_at REAL"
            )
        if "expected_run_id" not in restart_columns:
            self.connection.execute(
                "ALTER TABLE coordinator_restart_requests ADD COLUMN expected_run_id TEXT"
            )
        incident_columns = {
            row["name"]
            for row in self.connection.execute("PRAGMA table_info(incidents)").fetchall()
        }
        if "short_verdict" not in incident_columns:
            self.connection.execute("ALTER TABLE incidents ADD COLUMN short_verdict TEXT")
        if "long_detail" not in incident_columns:
            self.connection.execute("ALTER TABLE incidents ADD COLUMN long_detail TEXT")
        if "reviewed_at" not in incident_columns:
            self.connection.execute("ALTER TABLE incidents ADD COLUMN reviewed_at REAL")
        if "restart_generation" not in incident_columns:
            self.connection.execute(
                "ALTER TABLE incidents ADD COLUMN restart_generation INTEGER"
            )
        deadline_component_columns = {
            row["name"]
            for row in self.connection.execute(
                "PRAGMA table_info(deadline_mutation_components)"
            ).fetchall()
        }
        if "receipt_id" not in deadline_component_columns:
            self.connection.execute(
                "ALTER TABLE deadline_mutation_components ADD COLUMN receipt_id TEXT"
            )
        generation_component_columns = {
            row["name"]
            for row in self.connection.execute(
                "PRAGMA table_info(deadline_generation_mutation_components)"
            ).fetchall()
        }
        if "no_change_required" not in generation_component_columns:
            self.connection.execute(
                "ALTER TABLE deadline_generation_mutation_components "
                "ADD COLUMN no_change_required INTEGER NOT NULL DEFAULT 0"
            )
        self.connection.execute(
            """
            INSERT OR IGNORE INTO claim_deadline_generation_incidents (
                lineage_id, claim_id, generation, source_task_id, recorded_at,
                short_verdict, long_detail, reviewed_at, restart_generation
            )
            SELECT lineage_id, claim_id, 1, source_task_id, recorded_at,
                   short_verdict, long_detail, reviewed_at, restart_generation
            FROM claim_deadline_incidents AS legacy
            WHERE NOT EXISTS (
                SELECT 1 FROM claim_deadline_generation_incidents AS current
                WHERE current.lineage_id = legacy.lineage_id
                  AND current.claim_id = legacy.claim_id
                  AND current.generation > 1
                  AND current.source_task_id = legacy.source_task_id
                  AND current.recorded_at = legacy.recorded_at
            )
            """
        )
        self.connection.execute(
            """
            INSERT OR IGNORE INTO deadline_generation_mutation_components (
                lineage_id, claim_id, generation, component, resolved_at,
                evidence, receipt_id
            )
            SELECT legacy.lineage_id, legacy.claim_id, 1, legacy.component,
                   legacy.resolved_at, legacy.evidence, legacy.receipt_id
            FROM deadline_mutation_components AS legacy
            JOIN claim_deadline_generation_incidents AS incident
              ON incident.lineage_id = legacy.lineage_id
             AND incident.claim_id = legacy.claim_id
             AND incident.generation = 1
            """
        )
        integrity_component_columns = {
            row["name"]
            for row in self.connection.execute(
                "PRAGMA table_info(integrity_mutation_components)"
            ).fetchall()
        }
        if "receipt_id" not in integrity_component_columns:
            self.connection.execute(
                "ALTER TABLE integrity_mutation_components ADD COLUMN receipt_id TEXT"
            )
        self.connection.execute(
            """
            UPDATE incidents
            SET short_verdict = replace(kind, '_', ' ')
            WHERE short_verdict IS NULL OR trim(short_verdict) = ''
            """
        )
        self.connection.execute(
            """
            UPDATE incidents
            SET long_detail = CASE
                WHEN reason IS NOT NULL AND trim(reason) != '' THEN reason
                WHEN kind = 'deadline_miss' THEN
                    'The immutable deadline passed without an on-time terminal result.'
                ELSE 'An integrity breach invalidated the task result.'
            END
            WHERE long_detail IS NULL OR trim(long_detail) = ''
            """
        )
        finding_columns = {
            row["name"]
            for row in self.connection.execute(
                "PRAGMA table_info(worker_findings)"
            ).fetchall()
        }
        if "short_verdict" not in finding_columns:
            self.connection.execute(
                "ALTER TABLE worker_findings ADD COLUMN short_verdict TEXT"
            )
        self.connection.execute(
            """
            UPDATE worker_findings
            SET short_verdict = replace(kind, '_', ' ')
            WHERE short_verdict IS NULL OR trim(short_verdict) = ''
            """
        )
        random_columns = {
            row["name"]
            for row in self.connection.execute(
                "PRAGMA table_info(random_mutation_cycles)"
            ).fetchall()
        }
        if "ordinary_resolution_evidence" not in random_columns:
            self.connection.execute(
                "ALTER TABLE random_mutation_cycles ADD COLUMN ordinary_resolution_evidence TEXT"
            )
        if "universal_required" not in random_columns:
            self.connection.execute(
                "ALTER TABLE random_mutation_cycles ADD COLUMN universal_required INTEGER NOT NULL DEFAULT 0"
            )
        if "universal_resolution_evidence" not in random_columns:
            self.connection.execute(
                "ALTER TABLE random_mutation_cycles ADD COLUMN universal_resolution_evidence TEXT"
            )
        if "universal_receipt_id" not in random_columns:
            self.connection.execute(
                "ALTER TABLE random_mutation_cycles ADD COLUMN universal_receipt_id TEXT"
            )
        if "universal_capability_status" not in random_columns:
            self.connection.execute(
                "ALTER TABLE random_mutation_cycles ADD COLUMN universal_capability_status TEXT"
            )
        if "universal_capability_reason" not in random_columns:
            self.connection.execute(
                "ALTER TABLE random_mutation_cycles ADD COLUMN universal_capability_reason TEXT"
            )
        if "universal_capability_checked_at" not in random_columns:
            self.connection.execute(
                "ALTER TABLE random_mutation_cycles ADD COLUMN universal_capability_checked_at REAL"
            )
        if "universal_capability_roster_digest" not in random_columns:
            self.connection.execute(
                "ALTER TABLE random_mutation_cycles ADD COLUMN universal_capability_roster_digest TEXT"
            )
        if "restart_generation" not in random_columns:
            self.connection.execute(
                "ALTER TABLE random_mutation_cycles ADD COLUMN restart_generation INTEGER"
            )
        if "cadence_version" not in random_columns:
            self.connection.execute(
                "ALTER TABLE random_mutation_cycles "
                "ADD COLUMN cadence_version INTEGER NOT NULL DEFAULT 1"
            )
        self._migrate_random_interval_constraint()
        receipt_columns = {
            row["name"]
            for row in self.connection.execute(
                "PRAGMA table_info(universal_review_receipts)"
            ).fetchall()
        }
        if "capability_roster_digest" not in receipt_columns:
            self.connection.execute(
                "ALTER TABLE universal_review_receipts ADD COLUMN capability_roster_digest TEXT"
            )
        acceptance_columns = {
            row["name"]
            for row in self.connection.execute(
                "PRAGMA table_info(claim_acceptances)"
            ).fetchall()
        }
        if "closure_sequence" not in acceptance_columns:
            self.connection.execute(
                "ALTER TABLE claim_acceptances ADD COLUMN closure_sequence INTEGER"
            )
        self.connection.execute(
            "DROP TRIGGER IF EXISTS claim_acceptance_closure_sequence_is_immutable"
        )
        self.connection.execute(
            """
            UPDATE random_mutation_cycles
            SET ordinary_resolution_evidence = resolution_evidence
            WHERE resolution_evidence IS NOT NULL
              AND ordinary_resolution_evidence IS NULL
            """
        )
        self.connection.execute(
            """
            UPDATE random_mutation_cycles
            SET universal_required = 0
            WHERE resolution_evidence IS NULL
              AND (
                due_task_id IS NULL
                OR interval_windows != 30
                OR selected_lane != 'DFS.md'
              )
            """
        )
        self.connection.execute(
            """
            UPDATE random_mutation_cycles
            SET universal_capability_status = CASE
                    WHEN universal_receipt_id IS NOT NULL THEN 'available'
                    ELSE 'legacy-resolved'
                END,
                universal_capability_reason = CASE
                    WHEN universal_receipt_id IS NOT NULL THEN
                        'historical cycle has a validated Sol/ultra receipt'
                    ELSE 'historical cycle resolved before capability snapshots existed'
                END
            WHERE resolution_evidence IS NOT NULL
              AND interval_windows = 30
              AND selected_lane = 'DFS.md'
              AND universal_capability_status IS NULL
            """
        )
        self.connection.execute(
            """
            UPDATE random_mutation_cycles
            SET universal_resolution_evidence =
                'v1 cycle resolved before universal review components existed'
            WHERE resolution_evidence IS NOT NULL
              AND universal_required = 1
              AND universal_resolution_evidence IS NULL
            """
        )
        self._migrate_v1_state()
        self.connection.execute(
            """
            UPDATE tasks AS current
            SET phase_sequence_at_dispatch = (
                SELECT MAX(event.sequence)
                FROM claim_phase_events AS event
                WHERE event.lineage_id = current.lineage_id
                  AND event.claim_id = current.claim_id
                  AND event.phase = current.phase_at_dispatch
                  AND event.recorded_at <= current.started_at
            )
            WHERE current.phase_sequence_at_dispatch IS NULL
            """
        )
        self.connection.execute(
            """
            UPDATE claim_acceptances AS accepted
            SET closure_sequence = (
                SELECT task.phase_sequence_at_dispatch
                FROM tasks AS task
                WHERE task.lineage_id = accepted.lineage_id
                  AND task.task_id = accepted.task_id
            )
            WHERE accepted.closure_sequence IS NULL
            """
        )
        self.connection.executescript(
            """
            CREATE TRIGGER claim_acceptance_closure_sequence_is_immutable
            BEFORE UPDATE ON claim_acceptances
            WHEN NEW.closure_sequence IS NOT OLD.closure_sequence
            BEGIN
                SELECT RAISE(ABORT, 'claim acceptance closure epoch is immutable');
            END;

            CREATE UNIQUE INDEX IF NOT EXISTS random_cycle_universal_receipt_is_unique
            ON random_mutation_cycles(universal_receipt_id)
            WHERE universal_receipt_id IS NOT NULL;

            CREATE TRIGGER IF NOT EXISTS random_cycle_universal_receipt_is_immutable
            BEFORE UPDATE ON random_mutation_cycles
            WHEN OLD.universal_receipt_id IS NOT NULL
             AND NEW.universal_receipt_id IS NOT OLD.universal_receipt_id
            BEGIN
                SELECT RAISE(ABORT, 'universal receipt use is immutable');
            END;

            CREATE TRIGGER IF NOT EXISTS random_cycle_capability_snapshot_is_immutable
            BEFORE UPDATE ON random_mutation_cycles
            WHEN OLD.universal_capability_status IS NOT NULL
             AND (
                NEW.universal_capability_status IS NOT OLD.universal_capability_status
                OR NEW.universal_capability_reason IS NOT OLD.universal_capability_reason
                OR NEW.universal_capability_checked_at IS NOT OLD.universal_capability_checked_at
                OR NEW.universal_capability_roster_digest IS NOT OLD.universal_capability_roster_digest
                OR NEW.universal_required IS NOT OLD.universal_required
             )
            BEGIN
                SELECT RAISE(ABORT, 'universal capability snapshot is immutable');
            END;

            CREATE TRIGGER IF NOT EXISTS normal_method_receipts_cannot_change
            BEFORE UPDATE ON normal_method_receipts
            BEGIN
                SELECT RAISE(ABORT, 'normal method receipts are append-only');
            END;

            CREATE TRIGGER IF NOT EXISTS normal_method_receipts_cannot_be_deleted
            BEFORE DELETE ON normal_method_receipts
            BEGIN
                SELECT RAISE(ABORT, 'normal method receipts are append-only');
            END;

            CREATE TRIGGER IF NOT EXISTS deadline_mutation_components_cannot_change
            BEFORE UPDATE ON deadline_mutation_components
            BEGIN
                SELECT RAISE(ABORT, 'deadline mutation components are append-only');
            END;

            CREATE TRIGGER IF NOT EXISTS deadline_mutation_components_cannot_be_deleted
            BEFORE DELETE ON deadline_mutation_components
            BEGIN
                SELECT RAISE(ABORT, 'deadline mutation components are append-only');
            END;

            CREATE UNIQUE INDEX IF NOT EXISTS deadline_macro_receipt_is_unique
            ON deadline_mutation_components(receipt_id)
            WHERE receipt_id IS NOT NULL;

            CREATE UNIQUE INDEX IF NOT EXISTS integrity_macro_receipt_is_unique
            ON integrity_mutation_components(receipt_id)
            WHERE receipt_id IS NOT NULL;

            CREATE TRIGGER IF NOT EXISTS task_dispatch_phase_is_immutable
            BEFORE UPDATE ON tasks
            WHEN NEW.phase_at_dispatch IS NOT OLD.phase_at_dispatch
              OR (
                  OLD.phase_sequence_at_dispatch IS NOT NULL
                  AND NEW.phase_sequence_at_dispatch IS NOT OLD.phase_sequence_at_dispatch
              )
            BEGIN
                SELECT RAISE(ABORT, 'task dispatch phase is immutable');
            END;
            """
        )
        self._migrate_task_terminal_kind_triggers()
        self._migrate_v2_closure_gaps()
        self._migrate_random_cadence()
        revision_columns = {
            row[1] for row in self.connection.execute("PRAGMA table_info(closure_gap_revisions)")
        }
        if "route_kind" not in revision_columns:
            self.connection.execute(
                "ALTER TABLE closure_gap_revisions ADD COLUMN route_kind TEXT NOT NULL "
                "DEFAULT 'executable' CHECK (route_kind IN ('executable', 'owner_wait'))"
            )
        self.connection.execute("PRAGMA user_version = 5")
        self.connection.commit()

    def _migrate_task_terminal_kind_triggers(self) -> None:
        """Keep persisted terminal validation aligned with harness lifecycle states."""
        self.connection.executescript(
            """
            DROP TRIGGER IF EXISTS task_terminal_kind_is_valid_on_insert;
            DROP TRIGGER IF EXISTS task_terminal_kind_is_valid_on_update;

            CREATE TRIGGER task_terminal_kind_is_valid_on_insert
            BEFORE INSERT ON tasks
            WHEN NEW.attempt_terminal_kind IS NOT NULL
             AND NEW.attempt_terminal_kind NOT IN (
                 'completed', 'abandoned', 'finding', 'integrity_breach',
                 'restart_normalized'
             )
            BEGIN
                SELECT RAISE(ABORT, 'unsupported attempt terminal kind');
            END;

            CREATE TRIGGER task_terminal_kind_is_valid_on_update
            BEFORE UPDATE OF attempt_terminal_kind ON tasks
            WHEN NEW.attempt_terminal_kind IS NOT NULL
             AND NEW.attempt_terminal_kind NOT IN (
                 'completed', 'abandoned', 'finding', 'integrity_breach',
                 'restart_normalized'
             )
            BEGIN
                SELECT RAISE(ABORT, 'unsupported attempt terminal kind');
            END;
            """
        )

    def _migrate_random_interval_constraint(self) -> None:
        """Widen legacy cadence storage without redrawing persisted cycles."""

        row = self.connection.execute(
            "SELECT sql FROM sqlite_master WHERE type = 'table' AND name = 'random_mutation_cycles'"
        ).fetchone()
        schema = str(row["sql"] if row is not None else "").lower()
        if ("interval_windows between 10 and 50" in " ".join(schema.split())
                and re.search(r"cadence_version\s+in\s*\(\s*1\s*,\s*2\s*,\s*3\s*\)", schema)):
            return
        self.connection.commit()
        self.connection.execute("PRAGMA foreign_keys = OFF")
        try:
            self.connection.execute("BEGIN IMMEDIATE")
            self.connection.execute(
                """
                CREATE TABLE random_mutation_cycles_wider (
                    lineage_id TEXT NOT NULL,
                    cycle_number INTEGER NOT NULL CHECK (cycle_number > 0),
                    interval_windows INTEGER NOT NULL CHECK (
                        interval_windows BETWEEN 10 AND 50
                    ),
                    cadence_version INTEGER NOT NULL DEFAULT 1 CHECK (
                        cadence_version IN (1, 2, 3)
                    ),
                    due_after_terminal_windows INTEGER NOT NULL CHECK (
                        due_after_terminal_windows >= interval_windows
                    ),
                    selected_lane TEXT NOT NULL CHECK (
                        selected_lane IN (
                            'test-and-task-guidelines.md',
                            'orchestrator-guidelines.md',
                            'DFS.md'
                        )
                    ),
                    due_task_id TEXT,
                    resolution_evidence TEXT,
                    ordinary_resolution_evidence TEXT,
                    universal_required INTEGER NOT NULL DEFAULT 0 CHECK (
                        universal_required IN (0, 1)
                    ),
                    universal_resolution_evidence TEXT,
                    universal_receipt_id TEXT,
                    universal_capability_status TEXT CHECK (
                        universal_capability_status IN (
                            'available', 'unavailable', 'legacy-resolved'
                        )
                    ),
                    universal_capability_reason TEXT,
                    universal_capability_checked_at REAL,
                    universal_capability_roster_digest TEXT,
                    restart_generation INTEGER,
                    PRIMARY KEY (lineage_id, cycle_number)
                )
                """
            )
            columns = (
                "lineage_id, cycle_number, interval_windows, cadence_version, due_after_terminal_windows, "
                "selected_lane, due_task_id, resolution_evidence, ordinary_resolution_evidence, "
                "universal_required, universal_resolution_evidence, universal_receipt_id, "
                "universal_capability_status, universal_capability_reason, "
                "universal_capability_checked_at, universal_capability_roster_digest, "
                "restart_generation"
            )
            self.connection.execute(
                f"INSERT INTO random_mutation_cycles_wider ({columns}) "
                f"SELECT lineage_id, cycle_number, interval_windows, cadence_version, "
                f"due_after_terminal_windows, selected_lane, due_task_id, "
                f"resolution_evidence, ordinary_resolution_evidence, universal_required, "
                f"universal_resolution_evidence, universal_receipt_id, "
                f"universal_capability_status, universal_capability_reason, "
                f"universal_capability_checked_at, universal_capability_roster_digest, "
                f"restart_generation FROM random_mutation_cycles"
            )
            self.connection.execute("DROP TABLE random_mutation_cycles")
            self.connection.execute(
                "ALTER TABLE random_mutation_cycles_wider RENAME TO random_mutation_cycles"
            )
            self.connection.commit()
        except Exception:
            self.connection.rollback()
            raise
        finally:
            self.connection.execute("PRAGMA foreign_keys = ON")
        violations = self.connection.execute("PRAGMA foreign_key_check").fetchall()
        if violations:
            raise DeadlineError("Random cadence migration broke persisted foreign keys")

    def _migrate_random_cadence(self) -> None:
        """Adopt current bounds without resetting progress or postponing due reviews."""
        rows = self.connection.execute(
            "SELECT * FROM random_mutation_cycles "
            "WHERE resolution_evidence IS NULL AND cadence_version < ?",
            (CADENCE_VERSION,),
        ).fetchall()
        for row in rows:
            if (row["due_task_id"] is not None or
                    self._terminal_window_count(row["lineage_id"]) >=
                    row["due_after_terminal_windows"]):
                continue
            interval = min(RANDOM_INTERVAL_MAX,
                           max(RANDOM_INTERVAL_MIN, row["interval_windows"]))
            cycle_start = row["due_after_terminal_windows"] - row["interval_windows"]
            self.connection.execute(
                "UPDATE random_mutation_cycles SET interval_windows = ?, "
                "due_after_terminal_windows = ?, cadence_version = ? "
                "WHERE lineage_id = ? AND cycle_number = ?",
                (interval, cycle_start + interval, CADENCE_VERSION,
                 row["lineage_id"], row["cycle_number"]),
            )

    def _migrate_v1_state(self) -> None:
        """Project v1 task clocks into v2 claim state without rewriting v1 rows."""

        attempt_rows = self.connection.execute(
            """
            SELECT tasks.lineage_id, tasks.task_id, tasks.completed_at,
                   tasks.integrity_breached_at, tasks.attempt_terminal_at,
                   worker_findings.reported_at AS finding_at
            FROM tasks
            LEFT JOIN worker_findings USING (lineage_id, task_id)
            """
        ).fetchall()
        for row in attempt_rows:
            if row["attempt_terminal_at"] is not None:
                continue
            candidates = [
                (float(value), kind)
                for value, kind in (
                    (row["completed_at"], "completed"),
                    (row["integrity_breached_at"], "integrity_breach"),
                    (row["finding_at"], "finding"),
                )
                if value is not None
            ]
            if not candidates:
                continue
            terminal_at, terminal_kind = min(candidates, key=lambda item: item[0])
            self.connection.execute(
                """
                UPDATE tasks
                SET attempt_terminal_at = ?, attempt_terminal_kind = ?
                WHERE lineage_id = ? AND task_id = ?
                  AND attempt_terminal_at IS NULL
                """,
                (
                    terminal_at,
                    terminal_kind,
                    row["lineage_id"],
                    row["task_id"],
                ),
            )

        task_rows = self.connection.execute(
            """
            SELECT * FROM tasks
            ORDER BY lineage_id, claim_id, started_at, task_id
            """
        ).fetchall()
        tasks_by_claim: dict[tuple[str, str], list[sqlite3.Row]] = {}
        for row in task_rows:
            tasks_by_claim.setdefault(
                (str(row["lineage_id"]), str(row["claim_id"])), []
            ).append(row)
        for (lineage_id, claim_id), rows in tasks_by_claim.items():
            if self.connection.execute(
                """
                SELECT 1 FROM claim_clocks
                WHERE lineage_id = ? AND claim_id = ?
                """,
                (lineage_id, claim_id),
            ).fetchone() is not None:
                continue
            first = rows[0]
            clock_shapes = {
                (
                    float(row["estimate_seconds"]),
                    float(row["started_at"]),
                    float(row["deadline_at"]),
                )
                for row in rows
            }
            if len(clock_shapes) > 1:
                options = [
                    {
                        "task_id": row["task_id"],
                        "estimate_seconds": row["estimate_seconds"],
                        "started_at": row["started_at"],
                        "deadline_at": row["deadline_at"],
                    }
                    for row in rows
                ]
                self.connection.execute(
                    """
                    INSERT OR IGNORE INTO claim_clock_migration_conflicts (
                        lineage_id, claim_id, detected_at, legacy_clock_options
                    ) VALUES (?, ?, ?, ?)
                    """,
                    (
                        lineage_id,
                        claim_id,
                        time.time(),
                        json.dumps(options, sort_keys=True),
                    ),
                )
                continue
            self.connection.execute(
                """
                INSERT OR IGNORE INTO claim_clocks (
                    lineage_id, claim_id, estimate_seconds, started_at,
                    deadline_at, phase, migrated_from_task_id, migration_note
                ) VALUES (?, ?, ?, ?, ?, 'exploration', ?, ?)
                """,
                (
                    lineage_id,
                    claim_id,
                    first["estimate_seconds"],
                    first["started_at"],
                    first["deadline_at"],
                    first["task_id"],
                    "v1 task clock projected without rewriting its source row",
                ),
            )
            phase_exists = self.connection.execute(
                """
                SELECT 1 FROM claim_phase_events
                WHERE lineage_id = ? AND claim_id = ?
                LIMIT 1
                """,
                (lineage_id, claim_id),
            ).fetchone()
            if phase_exists is None:
                self.connection.execute(
                    """
                    INSERT INTO claim_phase_events (
                        lineage_id, claim_id, sequence, phase,
                        recorded_at, basis_task_id
                    ) VALUES (?, ?, 1, 'exploration', ?, ?)
                    """,
                    (
                        lineage_id,
                        claim_id,
                        first["started_at"],
                        first["task_id"],
                    ),
                )

        deadline_rows = self.connection.execute(
            """
            SELECT incidents.*, tasks.claim_id
            FROM incidents
            JOIN tasks USING (lineage_id, task_id)
            WHERE incidents.kind = 'deadline_miss'
            ORDER BY incidents.recorded_at, incidents.incident_id
            """
        ).fetchall()
        seen_claims: set[tuple[str, str]] = set()
        for row in deadline_rows:
            key = (str(row["lineage_id"]), str(row["claim_id"]))
            if key in seen_claims:
                continue
            seen_claims.add(key)
            if self.connection.execute(
                """
                SELECT 1 FROM claim_clocks
                WHERE lineage_id = ? AND claim_id = ?
                """,
                key,
            ).fetchone() is None:
                continue
            self.connection.execute(
                """
                INSERT OR IGNORE INTO claim_deadline_incidents (
                    lineage_id, claim_id, source_task_id, recorded_at,
                    short_verdict, long_detail, reviewed_at
                ) VALUES (?, ?, ?, ?, ?, ?, ?)
                """,
                (
                    row["lineage_id"],
                    row["claim_id"],
                    row["task_id"],
                    row["recorded_at"],
                    row["short_verdict"],
                    row["long_detail"],
                    row["reviewed_at"],
                ),
            )

    def _migrate_v2_closure_gaps(self) -> None:
        """Add one explicit gap to each pre-v3 closure epoch, append-only."""

        closures = self.connection.execute(
            """
            SELECT * FROM claim_phase_events
            WHERE phase = 'closure'
            ORDER BY lineage_id, claim_id, sequence
            """
        ).fetchall()
        for closure in closures:
            key = (
                closure["lineage_id"],
                closure["claim_id"],
                closure["sequence"],
            )
            if self.connection.execute(
                """
                SELECT 1 FROM closure_gaps
                WHERE lineage_id = ? AND claim_id = ? AND closure_sequence = ?
                LIMIT 1
                """,
                key,
            ).fetchone() is not None:
                continue
            description = str(
                closure["closure_remaining_gap"]
                or "Legacy closure gap retained without reinterpretation."
            )
            proof_route = str(
                closure["closure_evidence"]
                or "Legacy closure evidence route retained without reinterpretation."
            )
            self.connection.execute(
                """
                INSERT INTO closure_gaps (
                    lineage_id, claim_id, closure_sequence, gap_id,
                    opened_at, basis_task_id
                ) VALUES (?, ?, ?, 'G-001', ?, ?)
                """,
                (*key, closure["recorded_at"], closure["basis_task_id"]),
            )
            self.connection.execute(
                """
                INSERT INTO closure_gap_revisions (
                    lineage_id, claim_id, closure_sequence, gap_id,
                    revision, recorded_at, basis_task_id, description, proof_route
                ) VALUES (?, ?, ?, 'G-001', 1, ?, ?, ?, ?)
                """,
                (
                    *key,
                    closure["recorded_at"],
                    closure["basis_task_id"],
                    description,
                    proof_route,
                ),
            )
            accepted = self.connection.execute(
                """
                SELECT * FROM claim_acceptances
                WHERE lineage_id = ? AND claim_id = ?
                  AND closure_sequence = ? AND invalidated_at IS NULL
                ORDER BY acceptance_number DESC LIMIT 1
                """,
                key,
            ).fetchone()
            if accepted is not None:
                self.connection.execute(
                    """
                    UPDATE closure_gaps
                    SET closed_at = ?, closed_by_task_id = ?, closure_evidence = ?
                    WHERE lineage_id = ? AND claim_id = ?
                      AND closure_sequence = ? AND gap_id = 'G-001'
                    """,
                    (
                        accepted["accepted_at"],
                        accepted["task_id"],
                        accepted["evidence"],
                        *key,
                    ),
                )

    def synchronize_delivery_statuses(self, *, persist: bool = True) -> tuple[str, ...]:
        """Project durable acceptance into the ledger without changing FS.md."""
        workspace = _workspace_for_state(self.state_path)
        if workspace is None:
            return ()
        try:
            resolve(workspace / ".de67")
        except SpecificationError as error:
            raise DeadlineError(str(error)) from error
        ledger_path = workspace / ".de67" / "work-ledger.md"
        if not ledger_path.is_file():
            raise DeadlineError("FS status projection requires work-ledger.md")
        ledger = ledger_path.read_text(encoding="utf-8")
        rows = self.connection.execute(
            """
            SELECT accepted.* FROM claim_acceptances AS accepted
            JOIN (
              SELECT lineage_id, claim_id, MAX(acceptance_number) AS acceptance_number
              FROM claim_acceptances
              WHERE lineage_id = ?
              GROUP BY lineage_id, claim_id
            ) AS latest ON latest.lineage_id = accepted.lineage_id
             AND latest.claim_id = accepted.claim_id
              AND latest.acceptance_number = accepted.acceptance_number
            WHERE accepted.lineage_id = ? ORDER BY accepted.claim_id
            """,
            (self._bound_lineage_id(), self._bound_lineage_id()),
        ).fetchall()
        changed: list[str] = []
        for acceptance in rows:
            claim_id = str(acceptance["claim_id"])
            current = _ledger_claim_block(ledger, claim_id)
            if current is None:
                raise DeadlineError(f"Accepted claim {claim_id} lacks a work-ledger projection")
            receipt = (
                f"  - Durable acceptance: #{acceptance['acceptance_number']} via "
                f"`{acceptance['task_id']}`; SQLite evidence is authoritative."
            )
            if acceptance["invalidated_at"] is None:
                if not current.startswith(f"- [x] {claim_id}"):
                    raise DeadlineError(
                        f"Accepted claim {claim_id} lacks a checked work-ledger projection"
                    )
                projected = current if receipt in current else current + "\n" + receipt
            else:
                projected = _open_ledger_baseline(current)
            if projected != current:
                ledger = _replace_ledger_claim_block(ledger, claim_id, projected)
                changed.append(claim_id)
        if persist and changed:
            temporary = ledger_path.with_name(f".{ledger_path.name}.{os.getpid()}.tmp")
            temporary.write_text(ledger, encoding="utf-8")
            os.replace(temporary, ledger_path)
        return tuple(changed)

    def synchronize_dfs_statuses(self, *, persist: bool = True) -> tuple[str, ...]:
        """Historical API name; delivery status always projects into the ledger."""
        return self.synchronize_delivery_statuses(persist=persist)

    def _bound_lineage_id(self) -> str:
        rows = self.connection.execute("SELECT lineage_id FROM lineage_binding").fetchall()
        if len(rows) != 1:
            raise DeadlineError("Delivery status projection requires one bound lineage")
        return str(rows[0][0])

    def close(self) -> None:
        self.connection.close()

    def __enter__(self) -> "DeadlineHarness":
        return self

    def __exit__(self, *_: object) -> None:
        self.close()

    @staticmethod
    def _now(value: float | None) -> float:
        result = time.time() if value is None else value
        if isinstance(result, bool) or not isinstance(result, (int, float)):
            raise DeadlineError("Time must be a finite number")
        result = float(result)
        if not math.isfinite(result):
            raise DeadlineError("Time must be a finite number")
        return result

    @staticmethod
    def _identity(value: str, label: str) -> str:
        if not isinstance(value, str) or not value.strip():
            raise DeadlineError(f"{label} must not be empty")
        return value.strip()

    @staticmethod
    def _positive_estimate(value: float) -> float:
        if isinstance(value, bool) or not isinstance(value, (int, float)):
            raise DeadlineError("Estimate must be a positive finite number")
        result = float(value)
        if not math.isfinite(result) or result <= 0:
            raise DeadlineError("Estimate must be a positive finite number")
        return result

    @staticmethod
    def _nonempty_text(value: str, label: str) -> str:
        if not isinstance(value, str) or not value.strip():
            raise DeadlineError(f"{label} must not be empty")
        return value.strip()

    @staticmethod
    def _default_incident_detail(kind: str, reason: str | None) -> str:
        if reason is not None and reason.strip():
            return reason.strip()
        if kind == "deadline_miss":
            return "The immutable deadline passed without an on-time terminal result."
        return "An integrity breach invalidated the task result."

    def _begin(self) -> None:
        self.connection.execute("BEGIN IMMEDIATE")

    def _bind_lineage(self, lineage_id: str) -> None:
        binding = self.connection.execute(
            "SELECT lineage_id FROM lineage_binding WHERE singleton = 1"
        ).fetchone()
        if binding is None:
            existing = self.connection.execute(
                "SELECT DISTINCT lineage_id FROM tasks"
            ).fetchall()
            if len(existing) > 1:
                raise DeadlineError("State database already contains multiple lineages")
            bound_lineage = existing[0]["lineage_id"] if existing else lineage_id
            self.connection.execute(
                "INSERT INTO lineage_binding (singleton, lineage_id) VALUES (1, ?)",
                (bound_lineage,),
            )
        else:
            bound_lineage = binding["lineage_id"]
        if bound_lineage != lineage_id:
            raise DeadlineError(
                f"State database is bound to lineage {bound_lineage}; refusing {lineage_id}"
            )

    def coordinator_restart_status(self, lineage_id: str) -> dict[str, Any]:
        """Bind a fresh clock if needed and return only its restart baton."""
        lineage_id = self._identity(lineage_id, "Lineage id")
        self._begin()
        try:
            self._bind_lineage(lineage_id)
            result = {
                "lineage_id": lineage_id,
                "coordinator_restart": self._coordinator_restart_status(lineage_id),
            }
            self.connection.commit()
            return result
        except Exception:
            self.connection.rollback()
            raise

    def retire_claim_clocks_for_mutation(
        self,
        lineage_id: str,
        reason: str,
        *,
        now: float | None = None,
    ) -> int:
        """Retire every active claim deadline before an exclusive mutation review."""

        lineage_id = self._identity(lineage_id, "Lineage id")
        reason = self._nonempty_text(reason, "Mutation clock retirement reason")
        retired_at = self._now(now)
        self._begin()
        try:
            self._bind_lineage(lineage_id)
            running = self.connection.execute(
                """
                SELECT task_id FROM tasks
                WHERE lineage_id = ? AND attempt_terminal_at IS NULL
                ORDER BY started_at, task_id
                """,
                (lineage_id,),
            ).fetchall()
            from worker_library import returned_assignments
            workspace = _workspace_for_state(self.state_path)
            returned = returned_assignments(workspace, self.state_path, lineage_id) if workspace else set()
            running = [row for row in running if row["task_id"] not in returned]
            if running:
                raise DeadlineError(
                    "Mutation cannot retire clocks while a worker attempt is running: "
                    + str(running[0]["task_id"])
                )
            cursor = self.connection.execute(
                """
                UPDATE claim_deadline_generations AS generation
                SET retired_at = ?, retirement_reason = ?
                WHERE generation.lineage_id = ?
                  AND generation.retired_at IS NULL
                  AND generation.generation = (
                    SELECT MAX(latest.generation)
                    FROM claim_deadline_generations AS latest
                    WHERE latest.lineage_id = generation.lineage_id
                      AND latest.claim_id = generation.claim_id
                  )
                """,
                (retired_at, reason, lineage_id),
            )
            self.connection.commit()
            return cursor.rowcount
        except Exception:
            self.connection.rollback()
            raise

    def _task(self, lineage_id: str, task_id: str) -> sqlite3.Row:
        row = self.connection.execute(
            "SELECT * FROM tasks WHERE lineage_id = ? AND task_id = ?",
            (lineage_id, task_id),
        ).fetchone()
        if row is None:
            raise DeadlineError(f"Unknown task: {lineage_id}/{task_id}")
        return row

    def _claim(self, lineage_id: str, claim_id: str) -> sqlite3.Row:
        row = self.connection.execute(
            """
            SELECT clock.lineage_id, clock.claim_id,
                   generation.estimate_seconds, generation.started_at,
                   generation.deadline_at, clock.phase,
                   clock.migrated_from_task_id, clock.migration_note,
                   generation.generation AS deadline_generation,
                   generation.armed_by_restart_generation,
                   generation.retired_at, generation.retirement_reason
            FROM claim_clocks AS clock
            JOIN claim_deadline_generations AS generation
              ON generation.lineage_id = clock.lineage_id
             AND generation.claim_id = clock.claim_id
            WHERE clock.lineage_id = ? AND clock.claim_id = ?
            ORDER BY generation.generation DESC
            LIMIT 1
            """,
            (lineage_id, claim_id),
        ).fetchone()
        if row is None:
            raise DeadlineError(f"Unknown claim: {lineage_id}/{claim_id}")
        return row

    def _migration_conflict(
        self, lineage_id: str, claim_id: str
    ) -> sqlite3.Row | None:
        return self.connection.execute(
            """
            SELECT * FROM claim_clock_migration_conflicts
            WHERE lineage_id = ? AND claim_id = ?
            """,
            (lineage_id, claim_id),
        ).fetchone()

    def _earliest_legacy_deadline_miss(
        self, lineage_id: str, claim_id: str
    ) -> sqlite3.Row | None:
        """Return the first exact v1 miss whose clock must survive migration."""

        return self.connection.execute(
            """
            SELECT incidents.*, tasks.claim_id,
                   tasks.estimate_seconds, tasks.started_at, tasks.deadline_at
            FROM incidents
            JOIN tasks USING (lineage_id, task_id)
            WHERE incidents.lineage_id = ?
              AND tasks.claim_id = ?
              AND incidents.kind = 'deadline_miss'
            ORDER BY incidents.recorded_at, incidents.incident_id
            LIMIT 1
            """,
            (lineage_id, claim_id),
        ).fetchone()

    def claim_clock_migration_details(
        self, lineage_id: str, claim_id: str
    ) -> dict[str, Any]:
        """Expose one unresolved conflict's immutable legacy choices in full."""

        lineage_id = self._identity(lineage_id, "Lineage id")
        claim_id = self._identity(claim_id, "Claim id")
        self._begin()
        try:
            self._bind_lineage(lineage_id)
            conflict = self._migration_conflict(lineage_id, claim_id)
            if conflict is None:
                raise DeadlineError(
                    f"No claim clock migration conflict: {lineage_id}/{claim_id}"
                )
            if conflict["resolved_at"] is not None:
                raise DeadlineError("Claim clock migration conflict is already resolved")
            earliest_miss = self._earliest_legacy_deadline_miss(lineage_id, claim_id)
            result = {
                "lineage_id": lineage_id,
                "claim_id": claim_id,
                "detected_at": conflict["detected_at"],
                "legacy_clock_options": json.loads(conflict["legacy_clock_options"]),
                "earliest_legacy_deadline_miss": (
                    {
                        "task_id": earliest_miss["task_id"],
                        "recorded_at": earliest_miss["recorded_at"],
                        "estimate_seconds": earliest_miss["estimate_seconds"],
                        "started_at": earliest_miss["started_at"],
                        "deadline_at": earliest_miss["deadline_at"],
                    }
                    if earliest_miss is not None
                    else None
                ),
                "required_source_task_id": (
                    earliest_miss["task_id"] if earliest_miss is not None else None
                ),
            }
            self.connection.commit()
            return result
        except Exception:
            self.connection.rollback()
            raise

    def resolve_claim_clock_migration(
        self,
        lineage_id: str,
        claim_id: str,
        reason: str,
        *,
        source_task_id: str | None = None,
        estimate_seconds: float | None = None,
        now: float | None = None,
    ) -> dict[str, Any]:
        """Bind one ambiguous v1 claim to an exact source clock or a new clock."""

        lineage_id = self._identity(lineage_id, "Lineage id")
        claim_id = self._identity(claim_id, "Claim id")
        reason = self._nonempty_text(reason, "Migration resolution reason")
        resolved_at = self._now(now)
        if (source_task_id is None) == (estimate_seconds is None):
            raise DeadlineError(
                "Choose exactly one legacy source task or one new item-level estimate"
            )
        self._begin()
        try:
            self._bind_lineage(lineage_id)
            conflict = self._migration_conflict(lineage_id, claim_id)
            if conflict is None:
                raise DeadlineError(
                    f"No claim clock migration conflict: {lineage_id}/{claim_id}"
                )
            if conflict["resolved_at"] is not None:
                raise DeadlineError("Claim clock migration conflict is already resolved")
            if self.connection.execute(
                """
                SELECT 1 FROM claim_clocks
                WHERE lineage_id = ? AND claim_id = ?
                """,
                (lineage_id, claim_id),
            ).fetchone() is not None:
                raise DeadlineError("Claim clock already exists and is immutable")
            legacy_deadline_miss = self._earliest_legacy_deadline_miss(
                lineage_id, claim_id
            )
            if source_task_id is None and legacy_deadline_miss is not None:
                raise DeadlineError(
                    "A legacy deadline miss exists; choose an exact legacy source "
                    "clock so the miss remains visible at claim level"
                )
            if source_task_id is not None:
                source_task_id = self._identity(source_task_id, "Source task id")
                task = self._task(lineage_id, source_task_id)
                if task["claim_id"] != claim_id:
                    raise DeadlineError("Legacy source task belongs to another claim")
                if (
                    legacy_deadline_miss is not None
                    and source_task_id != legacy_deadline_miss["task_id"]
                ):
                    raise DeadlineError(
                        "Legacy source task must own the earliest exact deadline miss: "
                        f"{legacy_deadline_miss['task_id']}"
                    )
                estimate = float(task["estimate_seconds"])
                started_at = float(task["started_at"])
                deadline_at = float(task["deadline_at"])
                resolution_kind = "legacy_source"
            else:
                estimate = self._positive_estimate(estimate_seconds)
                started_at = resolved_at
                deadline_at = started_at + estimate
                resolution_kind = "new_item_clock"
            self.connection.execute(
                """
                INSERT INTO claim_clocks (
                    lineage_id, claim_id, estimate_seconds, started_at,
                    deadline_at, phase, migrated_from_task_id, migration_note
                ) VALUES (?, ?, ?, ?, ?, 'exploration', ?, ?)
                """,
                (
                    lineage_id,
                    claim_id,
                    estimate,
                    started_at,
                    deadline_at,
                    source_task_id,
                    reason,
                ),
            )
            self.connection.execute(
                """
                INSERT INTO claim_deadline_generations (
                    lineage_id, claim_id, generation, estimate_seconds,
                    started_at, deadline_at
                ) VALUES (?, ?, 1, ?, ?, ?)
                """,
                (lineage_id, claim_id, estimate, started_at, deadline_at),
            )
            basis_task_id = (
                source_task_id
                if source_task_id is not None
                else str(
                    self.connection.execute(
                        """
                        SELECT task_id FROM tasks
                        WHERE lineage_id = ? AND claim_id = ?
                        ORDER BY started_at, task_id LIMIT 1
                        """,
                        (lineage_id, claim_id),
                    ).fetchone()["task_id"]
                )
            )
            self.connection.execute(
                """
                INSERT INTO claim_phase_events (
                    lineage_id, claim_id, sequence, phase,
                    recorded_at, basis_task_id
                ) VALUES (?, ?, 1, 'exploration', ?, ?)
                """,
                (lineage_id, claim_id, resolved_at, basis_task_id),
            )
            self.connection.execute(
                """
                UPDATE tasks SET phase_sequence_at_dispatch = 1
                WHERE lineage_id = ? AND claim_id = ?
                  AND phase_sequence_at_dispatch IS NULL
                """,
                (lineage_id, claim_id),
            )
            self.connection.execute(
                """
                UPDATE claim_clock_migration_conflicts
                SET resolved_at = ?, resolution_kind = ?, source_task_id = ?,
                    estimate_seconds = ?, started_at = ?, deadline_at = ?, reason = ?
                WHERE lineage_id = ? AND claim_id = ?
                """,
                (
                    resolved_at,
                    resolution_kind,
                    source_task_id,
                    estimate,
                    started_at,
                    deadline_at,
                    reason,
                    lineage_id,
                    claim_id,
                ),
            )
            if source_task_id is not None and legacy_deadline_miss is not None:
                if legacy_deadline_miss["task_id"] != source_task_id:
                    raise DeadlineError(
                        "Resolved source no longer owns the earliest exact legacy miss"
                    )
                legacy_incident = legacy_deadline_miss
                if legacy_incident is not None:
                    self.connection.execute(
                        """
                        INSERT OR IGNORE INTO claim_deadline_incidents (
                            lineage_id, claim_id, source_task_id, recorded_at,
                            short_verdict, long_detail, reviewed_at
                        ) VALUES (?, ?, ?, ?, ?, ?, ?)
                        """,
                        (
                            lineage_id,
                            claim_id,
                            legacy_incident["task_id"],
                            legacy_incident["recorded_at"],
                            legacy_incident["short_verdict"],
                            legacy_incident["long_detail"],
                            legacy_incident["reviewed_at"],
                        ),
                    )
            result = dict(self._claim(lineage_id, claim_id))
            result["resolution_kind"] = resolution_kind
            self.connection.commit()
            return result
        except Exception:
            self.connection.rollback()
            raise

    def _task_claim(self, lineage_id: str, task_id: str) -> sqlite3.Row:
        task = self._task(lineage_id, task_id)
        row = self.connection.execute(
            """
            SELECT clock.lineage_id, clock.claim_id,
                   generation.estimate_seconds, generation.started_at,
                   generation.deadline_at, clock.phase,
                   clock.migrated_from_task_id, clock.migration_note,
                   generation.generation AS deadline_generation,
                   generation.armed_by_restart_generation,
                   generation.retired_at, generation.retirement_reason
            FROM claim_clocks AS clock
            JOIN claim_deadline_generations AS generation
              ON generation.lineage_id = clock.lineage_id
             AND generation.claim_id = clock.claim_id
            WHERE clock.lineage_id = ? AND clock.claim_id = ?
              AND generation.generation = ?
            """,
            (lineage_id, task["claim_id"], task["deadline_generation"]),
        ).fetchone()
        if row is None:
            # Divergent v1 clocks intentionally have no claim clock until an
            # authoritative legacy source is selected.
            legacy_clock = self.connection.execute(
                "SELECT * FROM claim_clocks WHERE lineage_id = ? AND claim_id = ?",
                (lineage_id, task["claim_id"]),
            ).fetchone()
            if legacy_clock is None:
                return task
            self.connection.execute(
                """
                INSERT OR IGNORE INTO claim_deadline_generations (
                    lineage_id, claim_id, generation, estimate_seconds,
                    started_at, deadline_at
                ) VALUES (?, ?, 1, ?, ?, ?)
                """,
                (
                    lineage_id, task["claim_id"], legacy_clock["estimate_seconds"],
                    legacy_clock["started_at"], legacy_clock["deadline_at"],
                ),
            )
            return self._task_claim(lineage_id, task_id)
        return row

    def _latest_valid_acceptance(
        self, lineage_id: str, claim_id: str
    ) -> sqlite3.Row | None:
        return self.connection.execute(
            """
            SELECT * FROM claim_acceptances
            WHERE lineage_id = ? AND claim_id = ? AND invalidated_at IS NULL
            ORDER BY acceptance_number DESC
            LIMIT 1
            """,
            (lineage_id, claim_id),
        ).fetchone()

    def _latest_acceptance(
        self, lineage_id: str, claim_id: str
    ) -> sqlite3.Row | None:
        return self.connection.execute(
            """
            SELECT * FROM claim_acceptances
            WHERE lineage_id = ? AND claim_id = ?
            ORDER BY acceptance_number DESC
            LIMIT 1
            """,
            (lineage_id, claim_id),
        ).fetchone()

    def _active_closure(self, lineage_id: str, claim_id: str) -> sqlite3.Row:
        row = self.connection.execute(
            """
            SELECT * FROM claim_phase_events
            WHERE lineage_id = ? AND claim_id = ? AND phase = 'closure'
            ORDER BY sequence DESC LIMIT 1
            """,
            (lineage_id, claim_id),
        ).fetchone()
        if row is None:
            raise DeadlineError("Claim has no closure epoch")
        return row

    def _closure_gaps(
        self,
        lineage_id: str,
        claim_id: str,
        closure_sequence: int,
        *,
        include_closed: bool = False,
    ) -> list[sqlite3.Row]:
        closed_filter = "" if include_closed else "AND closed_at IS NULL"
        return self.connection.execute(
            f"""
            SELECT * FROM closure_gaps
            WHERE lineage_id = ? AND claim_id = ? AND closure_sequence = ?
              {closed_filter}
            ORDER BY gap_id
            """,
            (lineage_id, claim_id, closure_sequence),
        ).fetchall()

    def _closure_gap(
        self,
        lineage_id: str,
        claim_id: str,
        closure_sequence: int,
        gap_id: str,
    ) -> sqlite3.Row:
        row = self.connection.execute(
            """
            SELECT * FROM closure_gaps
            WHERE lineage_id = ? AND claim_id = ?
              AND closure_sequence = ? AND gap_id = ?
            """,
            (lineage_id, claim_id, closure_sequence, gap_id),
        ).fetchone()
        if row is None:
            raise DeadlineError(f"Unknown closure gap: {gap_id}")
        return row

    def _latest_gap_revision(
        self,
        lineage_id: str,
        claim_id: str,
        closure_sequence: int,
        gap_id: str,
    ) -> sqlite3.Row:
        row = self.connection.execute(
            """
            SELECT * FROM closure_gap_revisions
            WHERE lineage_id = ? AND claim_id = ?
              AND closure_sequence = ? AND gap_id = ?
            ORDER BY revision DESC LIMIT 1
            """,
            (lineage_id, claim_id, closure_sequence, gap_id),
        ).fetchone()
        if row is None:
            raise DeadlineError(f"Closure gap has no revision: {gap_id}")
        return row

    def _active_gap_summaries(self, lineage_id: str) -> list[dict[str, Any]]:
        rows = self.connection.execute(
            """
            SELECT gaps.* FROM closure_gaps AS gaps
            JOIN claim_clocks AS clock
              ON clock.lineage_id = gaps.lineage_id
             AND clock.claim_id = gaps.claim_id
            WHERE gaps.lineage_id = ?
              AND clock.phase = 'closure'
              AND gaps.closure_sequence = (
                SELECT MAX(event.sequence)
                FROM claim_phase_events AS event
                WHERE event.lineage_id = gaps.lineage_id
                  AND event.claim_id = gaps.claim_id
                  AND event.phase = 'closure'
              )
            ORDER BY gaps.claim_id, gaps.gap_id
            """,
            (lineage_id,),
        ).fetchall()
        result: list[dict[str, Any]] = []
        for gap in rows:
            revision = self._latest_gap_revision(
                lineage_id,
                str(gap["claim_id"]),
                int(gap["closure_sequence"]),
                str(gap["gap_id"]),
            )
            result.append(
                {
                    "claim_id": gap["claim_id"],
                    "closure_sequence": gap["closure_sequence"],
                    "gap_id": gap["gap_id"],
                    "revision": revision["revision"],
                    "description": revision["description"],
                    "proof_route": revision["proof_route"],
                    "route_kind": revision["route_kind"],
                    "status": "closed" if gap["closed_at"] is not None else "open",
                    "successor_of_gap_id": gap["successor_of_gap_id"],
                    "successor_of_revision": gap["successor_of_revision"],
                }
            )
        return result

    def _claim_invalidation_details(
        self, lineage_id: str, claim_id: str
    ) -> dict[str, Any] | None:
        """Describe the durable evidence that invalidated the latest acceptance."""

        acceptance = self._latest_acceptance(lineage_id, claim_id)
        if (
            acceptance is None
            or acceptance["invalidated_at"] is None
            or self._latest_valid_acceptance(lineage_id, claim_id) is not None
        ):
            return None

        trigger: dict[str, Any] | None = None
        integrity = self.connection.execute(
            """
            SELECT incidents.task_id, incidents.recorded_at, incidents.reason,
                   incidents.short_verdict, incidents.long_detail,
                   incidents.reviewed_at
            FROM incidents
            JOIN tasks
              ON tasks.lineage_id = incidents.lineage_id
             AND tasks.task_id = incidents.task_id
            WHERE incidents.lineage_id = ?
              AND tasks.claim_id = ?
              AND incidents.kind = 'integrity_breach'
              AND incidents.recorded_at = ?
              AND (
                incidents.task_id = ?
                OR EXISTS (
                    SELECT 1 FROM closure_gaps AS gap
                    WHERE gap.lineage_id = incidents.lineage_id
                      AND gap.claim_id = tasks.claim_id
                      AND gap.closure_sequence = ?
                      AND gap.closed_by_task_id = incidents.task_id
                )
              )
            ORDER BY incidents.incident_id DESC
            LIMIT 1
            """,
            (
                lineage_id,
                claim_id,
                acceptance["invalidated_at"],
                acceptance["task_id"],
                acceptance["closure_sequence"],
            ),
        ).fetchone()
        if integrity is not None:
            trigger = {
                "kind": "integrity_breach",
                "task_id": integrity["task_id"],
                "recorded_at": integrity["recorded_at"],
                "reason": integrity["reason"],
                "short_verdict": integrity["short_verdict"],
                "long_detail": integrity["long_detail"],
                "reviewed_at": integrity["reviewed_at"],
            }
        else:
            reopened = self.connection.execute(
                """
                SELECT event.basis_task_id, event.recorded_at,
                       event.contradicted_premise, finding.kind,
                       finding.short_verdict, finding.evidence
                FROM claim_phase_events AS event
                JOIN worker_findings AS finding
                  ON finding.lineage_id = event.lineage_id
                 AND finding.task_id = event.basis_task_id
                WHERE event.lineage_id = ?
                  AND event.claim_id = ?
                  AND event.phase = 'exploration'
                  AND event.sequence > COALESCE(?, 0)
                  AND event.recorded_at = ?
                ORDER BY event.sequence DESC
                LIMIT 1
                """,
                (
                    lineage_id,
                    claim_id,
                    acceptance["closure_sequence"],
                    acceptance["invalidated_at"],
                ),
            ).fetchone()
            if reopened is not None:
                trigger = {
                    "kind": "closure_reopen",
                    "task_id": reopened["basis_task_id"],
                    "recorded_at": reopened["recorded_at"],
                    "contradicted_premise": reopened["contradicted_premise"],
                    "finding_kind": reopened["kind"],
                    "short_verdict": reopened["short_verdict"],
                    "finding_evidence": reopened["evidence"],
                }

        if trigger is None:
            owner = self.connection.execute(
                "SELECT basis_task_id, recorded_at, closure_evidence FROM claim_phase_events "
                "WHERE lineage_id = ? AND claim_id = ? AND phase = 'exploration' "
                "AND sequence > COALESCE(?, 0) AND recorded_at = ? "
                "AND basis_task_id = ? AND contradicted_premise IS NULL "
                "AND ? = 'owner requested reassessment: ' || closure_evidence "
                "ORDER BY sequence DESC LIMIT 1",
                (lineage_id, claim_id, acceptance["closure_sequence"],
                 acceptance["invalidated_at"], acceptance["task_id"],
                 acceptance["invalidation_reason"]),
            ).fetchone()
            if owner is not None:
                trigger = {"kind": "owner_reopen", "task_id": owner["basis_task_id"],
                           "recorded_at": owner["recorded_at"],
                           "owner_request": owner["closure_evidence"]}
        if trigger is None:
            return None
        return {
            "lineage_id": lineage_id,
            "claim_id": claim_id,
            "acceptance_number": acceptance["acceptance_number"],
            "accepted_task_id": acceptance["task_id"],
            "closure_sequence": acceptance["closure_sequence"],
            "accepted_at": acceptance["accepted_at"],
            "acceptance_evidence": acceptance["evidence"],
            "invalidated_at": acceptance["invalidated_at"],
            "invalidation_reason": acceptance["invalidation_reason"],
            "trigger": trigger,
        }

    def claim_invalidation_details(
        self, lineage_id: str, claim_id: str
    ) -> dict[str, Any]:
        """Expose one exact, currently invalidated acceptance and its trigger."""

        lineage_id = self._identity(lineage_id, "Lineage id")
        claim_id = self._identity(claim_id, "Claim id")
        self._begin()
        try:
            self._bind_lineage(lineage_id)
            details = self._claim_invalidation_details(lineage_id, claim_id)
            if details is None:
                raise DeadlineError(
                    "Claim has no currently invalidated acceptance with durable evidence"
                )
            self.connection.commit()
            return details
        except Exception:
            self.connection.rollback()
            raise

    def _invalidated_unaccepted_claims(
        self, lineage_id: str
    ) -> list[dict[str, Any]]:
        rows = self.connection.execute(
            """
            SELECT DISTINCT claim_id FROM claim_acceptances
            WHERE lineage_id = ? AND invalidated_at IS NOT NULL
            ORDER BY claim_id
            """,
            (lineage_id,),
        ).fetchall()
        result: list[dict[str, Any]] = []
        for row in rows:
            details = self._claim_invalidation_details(
                lineage_id, str(row["claim_id"])
            )
            if details is None:
                continue
            result.append(
                {
                    "claim_id": details["claim_id"],
                    "accepted_task_id": details["accepted_task_id"],
                    "invalidated_at": details["invalidated_at"],
                    "invalidation_reason": details["invalidation_reason"],
                    "trigger_kind": details["trigger"]["kind"],
                    "trigger_task_id": details["trigger"]["task_id"],
                }
            )
        return result

    def _reopened_unaccepted_claims(
        self, lineage_id: str
    ) -> list[dict[str, Any]]:
        """Expose claims whose accepted closure was reopened but not reaccepted."""

        rows = self.connection.execute(
            """
            SELECT clock.claim_id, clock.phase, accepted.invalidated_at,
                   accepted.invalidation_reason
            FROM claim_clocks AS clock
            JOIN claim_acceptances AS accepted
              ON accepted.lineage_id = clock.lineage_id
             AND accepted.claim_id = clock.claim_id
            WHERE clock.lineage_id = ?
              AND accepted.invalidated_at IS NOT NULL
              AND accepted.acceptance_number = (
                SELECT MAX(previous.acceptance_number)
                FROM claim_acceptances AS previous
                WHERE previous.lineage_id = accepted.lineage_id
                  AND previous.claim_id = accepted.claim_id
              )
              AND NOT EXISTS (
                SELECT 1 FROM claim_acceptances AS valid
                WHERE valid.lineage_id = clock.lineage_id
                  AND valid.claim_id = clock.claim_id
                  AND valid.invalidated_at IS NULL
              )
              AND EXISTS (
                SELECT 1 FROM claim_phase_events AS reopened
                WHERE reopened.lineage_id = clock.lineage_id
                  AND reopened.claim_id = clock.claim_id
                  AND reopened.phase = 'exploration'
                  AND reopened.sequence > COALESCE(accepted.closure_sequence, 0)
              )
            ORDER BY accepted.invalidated_at, clock.claim_id
            """,
            (lineage_id,),
        ).fetchall()
        return [dict(row) for row in rows]

    def _claim_deadline_incident(
        self, lineage_id: str, claim_id: str, generation: int | None = None
    ) -> sqlite3.Row | None:
        generation_clause = "AND generation = ?" if generation is not None else ""
        parameters: tuple[object, ...] = (
            (lineage_id, claim_id, generation)
            if generation is not None
            else (lineage_id, claim_id)
        )
        row = self.connection.execute(
            f"""
            SELECT * FROM claim_deadline_generation_incidents
            WHERE lineage_id = ? AND claim_id = ?
              {generation_clause}
            ORDER BY generation DESC LIMIT 1
            """,
            parameters,
        ).fetchone()
        if row is None and (generation is None or generation == 1):
            legacy = self.connection.execute(
                """
                SELECT * FROM claim_deadline_incidents AS legacy
                WHERE lineage_id = ? AND claim_id = ?
                  AND NOT EXISTS (
                      SELECT 1 FROM claim_deadline_generation_incidents AS current
                      WHERE current.lineage_id = legacy.lineage_id
                        AND current.claim_id = legacy.claim_id
                        AND current.generation > 1
                        AND current.source_task_id = legacy.source_task_id
                        AND current.recorded_at = legacy.recorded_at
                  )
                """,
                (lineage_id, claim_id),
            ).fetchone()
            if legacy is not None:
                self.connection.execute(
                    """
                    INSERT OR IGNORE INTO claim_deadline_generation_incidents (
                        lineage_id, claim_id, generation, source_task_id,
                        recorded_at, short_verdict, long_detail, reviewed_at,
                        restart_generation
                    ) VALUES (?, ?, 1, ?, ?, ?, ?, ?, ?)
                    """,
                    (
                        lineage_id, claim_id, legacy["source_task_id"],
                        legacy["recorded_at"], legacy["short_verdict"],
                        legacy["long_detail"], legacy["reviewed_at"],
                        legacy["restart_generation"],
                    ),
                )
                return self._claim_deadline_incident(
                    lineage_id, claim_id, 1 if generation is not None else None
                )
        return row

    def _deadline_mutation_components(
        self, lineage_id: str, claim_id: str, generation: int | None = None
    ) -> dict[str, sqlite3.Row]:
        if generation is None:
            incident = self._claim_deadline_incident(lineage_id, claim_id)
            if incident is None:
                return {}
            generation = int(incident["generation"])
        rows = self.connection.execute(
            """
            SELECT * FROM deadline_generation_mutation_components
            WHERE lineage_id = ? AND claim_id = ? AND generation = ?
            """,
            (lineage_id, claim_id, generation),
        ).fetchall()
        if generation == 1:
            legacy = self.connection.execute(
                """
                SELECT * FROM deadline_mutation_components
                WHERE lineage_id = ? AND claim_id = ?
                """,
                (lineage_id, claim_id),
            ).fetchall()
            rows = [*rows, *legacy]
        return {str(row["component"]): row for row in rows}

    def _deadline_mutation_pending(self, lineage_id: str, claim_id: str) -> bool:
        incident = self._claim_deadline_incident(lineage_id, claim_id)
        if incident is None:
            return False
        return set(self._deadline_mutation_components(
            lineage_id, claim_id, int(incident["generation"])
        )) != {
            "micro",
            "macro",
        }

    def _pending_deadline_mutations(self, lineage_id: str) -> list[dict[str, Any]]:
        rows = self.connection.execute(
            """
            SELECT incident.claim_id, incident.generation, incident.source_task_id,
                   incident.recorded_at, incident.reviewed_at, incident.restart_generation
            FROM claim_deadline_generation_incidents AS incident
            WHERE incident.lineage_id = ?
              AND NOT (incident.generation = 1 AND EXISTS (
                  SELECT 1 FROM claim_deadline_generation_incidents AS current
                  WHERE current.lineage_id = incident.lineage_id
                    AND current.claim_id = incident.claim_id
                    AND current.generation > 1
                    AND current.source_task_id = incident.source_task_id
                    AND current.recorded_at = incident.recorded_at
              ))
            ORDER BY incident.recorded_at, incident.claim_id
            """,
            (lineage_id,),
        ).fetchall()
        result: list[dict[str, Any]] = []
        for row in rows:
            components = self._deadline_mutation_components(
                lineage_id, str(row["claim_id"]), int(row["generation"])
            )
            pending = [
                component
                for component in ("micro", "macro")
                if component not in components
            ]
            if not pending:
                continue
            result.append(
                {
                    "claim_id": row["claim_id"],
                    "deadline_generation": row["generation"],
                    "source_task_id": row["source_task_id"],
                    "recorded_at": row["recorded_at"],
                    "reviewed": row["reviewed_at"] is not None,
                    "pending_components": pending,
                    "restart_generation": row["restart_generation"],
                }
            )
        return result

    def _integrity_incident(
        self, lineage_id: str, task_id: str
    ) -> sqlite3.Row | None:
        return self.connection.execute(
            """
            SELECT * FROM incidents
            WHERE lineage_id = ? AND task_id = ? AND kind = 'integrity_breach'
            """,
            (lineage_id, task_id),
        ).fetchone()

    def _integrity_mutation_components(
        self, lineage_id: str, task_id: str
    ) -> dict[str, sqlite3.Row]:
        rows = self.connection.execute(
            """
            SELECT * FROM integrity_mutation_components
            WHERE lineage_id = ? AND task_id = ?
            """,
            (lineage_id, task_id),
        ).fetchall()
        return {str(row["component"]): row for row in rows}

    def _integrity_mutation_pending(self, lineage_id: str, task_id: str) -> bool:
        if self._integrity_incident(lineage_id, task_id) is None:
            return False
        return set(self._integrity_mutation_components(lineage_id, task_id)) != {
            "micro",
            "macro",
        }

    def _pending_integrity_mutations(
        self, lineage_id: str
    ) -> list[dict[str, Any]]:
        rows = self.connection.execute(
            """
            SELECT incidents.task_id, tasks.claim_id, incidents.recorded_at,
                   incidents.reviewed_at, incidents.restart_generation
            FROM incidents
            JOIN tasks USING (lineage_id, task_id)
            WHERE incidents.lineage_id = ?
              AND incidents.kind = 'integrity_breach'
            ORDER BY incidents.recorded_at, incidents.incident_id
            """,
            (lineage_id,),
        ).fetchall()
        result: list[dict[str, Any]] = []
        for row in rows:
            components = self._integrity_mutation_components(
                lineage_id, str(row["task_id"])
            )
            pending = [
                component
                for component in ("micro", "macro")
                if component not in components
            ]
            if not pending:
                continue
            result.append(
                {
                    "task_id": row["task_id"],
                    "claim_id": row["claim_id"],
                    "recorded_at": row["recorded_at"],
                    "reviewed": row["reviewed_at"] is not None,
                    "pending_components": pending,
                    "restart_generation": row["restart_generation"],
                }
            )
        return result

    def _pending_migration_conflicts(self, lineage_id: str) -> list[dict[str, Any]]:
        rows = self.connection.execute(
            """
            SELECT claim_id, detected_at, legacy_clock_options
            FROM claim_clock_migration_conflicts
            WHERE lineage_id = ? AND resolved_at IS NULL
            ORDER BY detected_at, claim_id
            """,
            (lineage_id,),
        ).fetchall()
        return [
            {
                "claim_id": row["claim_id"],
                "detected_at": row["detected_at"],
                "legacy_clock_options": json.loads(row["legacy_clock_options"]),
            }
            for row in rows
        ]

    def _cumulative_units(self, lineage_id: str) -> int:
        row = self.connection.execute(
            "SELECT COALESCE(SUM(units), 0) AS total FROM incidents WHERE lineage_id = ?",
            (lineage_id,),
        ).fetchone()
        return int(row["total"])

    def _terminal_window_count(self, lineage_id: str) -> int:
        row = self.connection.execute(
            """
            SELECT COUNT(*) AS total FROM tasks
            WHERE lineage_id = ? AND attempt_terminal_at IS NOT NULL
              AND NOT (
                attempt_terminal_kind = 'restart_normalized'
                OR (
                    attempt_terminal_kind = 'abandoned'
                    AND abandonment_reason = 'external_supervisor_restart_normalization'
                )
              )
            """,
            (lineage_id,),
        ).fetchone()
        return int(row["total"])

    @staticmethod
    def _draw_random_cycle() -> tuple[int, str]:
        interval_width = RANDOM_INTERVAL_MAX - RANDOM_INTERVAL_MIN + 1
        interval = RANDOM_INTERVAL_MIN + secrets.randbelow(interval_width)
        lane = RANDOM_MUTATION_LANES[secrets.randbelow(len(RANDOM_MUTATION_LANES))]
        return interval, lane

    @staticmethod
    def _universal_signature_seen(interval: int, lane: str) -> bool:
        return interval == UNIVERSAL_RANDOM_INTERVAL and lane == "DFS.md"

    def _sol_ultra_capability_snapshot(self) -> tuple[bool, str, str | None]:
        """Read the standard persisted roster once when a rare cycle becomes due."""

        if self.state_path is None:
            return (
                False,
                "in-memory deadline state has no persisted workspace roster",
                None,
            )
        workspace_config = self.state_path.parent / "workspace.json"
        if not workspace_config.is_file():
            return False, f"workspace roster is missing at {workspace_config.name}", None
        try:
            roster_bytes = workspace_config.read_bytes()
            roster_digest = hashlib.sha256(roster_bytes).hexdigest()
            payload = json.loads(roster_bytes.decode("utf-8"))
        except (OSError, UnicodeError, json.JSONDecodeError) as error:
            return (
                False,
                f"workspace roster is unreadable: {type(error).__name__}",
                None,
            )
        if not isinstance(payload, dict) or payload.get("version") != 1:
            return False, "workspace roster has an unsupported version", roster_digest
        capabilities = payload.get("worker_capabilities")
        if not isinstance(capabilities, list):
            return (
                False,
                "workspace roster has no valid worker_capabilities list",
                roster_digest,
            )
        proved = any(
            isinstance(item, dict)
            and item.get("model") == "gpt-5.6-sol"
            and item.get("reasoning_effort") == "ultra"
            for item in capabilities
        )
        if not proved:
            return (
                False,
                "workspace roster has no persisted gpt-5.6-sol/ultra probe",
                roster_digest,
            )
        return True, "workspace roster proves gpt-5.6-sol/ultra", roster_digest

    def _snapshot_universal_capability(
        self, lineage_id: str, cycle_number: int
    ) -> sqlite3.Row:
        """Arm or defer the exact rare trigger from its due-time capability roster."""

        cycle = self.connection.execute(
            """
            SELECT * FROM random_mutation_cycles
            WHERE lineage_id = ? AND cycle_number = ?
            """,
            (lineage_id, cycle_number),
        ).fetchone()
        if cycle is None:
            raise DeadlineError("Missing random mutation cycle")
        if (
            cycle["due_task_id"] is None
            or cycle["resolution_evidence"] is not None
            or cycle["universal_capability_status"] is not None
            or not self._universal_signature_seen(
                int(cycle["interval_windows"]), str(cycle["selected_lane"])
            )
        ):
            return cycle

        available, reason, roster_digest = self._sol_ultra_capability_snapshot()
        checked_at = time.time()
        self.connection.execute(
            """
            UPDATE random_mutation_cycles
            SET universal_capability_status = ?,
                universal_capability_reason = ?,
                universal_capability_checked_at = ?,
                universal_capability_roster_digest = ?,
                universal_required = ?
            WHERE lineage_id = ? AND cycle_number = ?
              AND universal_capability_status IS NULL
            """,
            (
                "available" if available else "unavailable",
                reason,
                checked_at,
                roster_digest,
                int(available),
                lineage_id,
                cycle_number,
            ),
        )
        refreshed = self.connection.execute(
            """
            SELECT * FROM random_mutation_cycles
            WHERE lineage_id = ? AND cycle_number = ?
            """,
            (lineage_id, cycle_number),
        ).fetchone()
        if refreshed is None:
            raise DeadlineError("Failed to persist universal capability snapshot")
        if (
            not available
            and refreshed["ordinary_resolution_evidence"] is not None
            and refreshed["resolution_evidence"] is None
        ):
            combined = json.dumps(
                {
                    "ordinary": refreshed["ordinary_resolution_evidence"],
                    "universal": None,
                },
                sort_keys=True,
            )
            restart, _ = self._request_coordinator_restart(
                lineage_id,
                f"random mutation cycle {cycle_number} resolved",
                checked_at,
            )
            self.connection.execute(
                """
                UPDATE random_mutation_cycles
                SET resolution_evidence = ?, restart_generation = ?
                WHERE lineage_id = ? AND cycle_number = ?
                """,
                (
                    combined,
                    restart["generation"],
                    lineage_id,
                    cycle_number,
                ),
            )
            refreshed = self.connection.execute(
                """
                SELECT * FROM random_mutation_cycles
                WHERE lineage_id = ? AND cycle_number = ?
                """,
                (lineage_id, cycle_number),
            ).fetchone()
            if refreshed is None:
                raise DeadlineError("Failed to close deferred universal cycle")
        return refreshed

    def _latest_random_cycle(self, lineage_id: str) -> sqlite3.Row | None:
        return self.connection.execute(
            """
            SELECT * FROM random_mutation_cycles
            WHERE lineage_id = ?
            ORDER BY cycle_number DESC
            LIMIT 1
            """,
            (lineage_id,),
        ).fetchone()

    def _latest_coordinator_restart(self, lineage_id: str) -> sqlite3.Row | None:
        return self.connection.execute(
            """
            SELECT * FROM coordinator_restart_requests
            WHERE lineage_id = ?
            ORDER BY generation DESC
            LIMIT 1
            """,
            (lineage_id,),
        ).fetchone()

    @staticmethod
    def _coordinator_restart_result(row: sqlite3.Row) -> dict[str, Any]:
        return {
            "generation": row["generation"],
            "pending": row["acknowledged_at"] is None,
            "requested_at": row["requested_at"],
            "reason": row["reason"],
            "claimed_at": row["claimed_at"],
            "expected_run_id": row["expected_run_id"],
            "acknowledged_at": row["acknowledged_at"],
            "run_id": row["run_id"],
        }

    def _coordinator_restart_status(
        self, lineage_id: str
    ) -> dict[str, Any] | None:
        row = self._latest_coordinator_restart(lineage_id)
        return self._coordinator_restart_result(row) if row is not None else None

    def _request_coordinator_restart(
        self,
        lineage_id: str,
        reason: str,
        requested_at: float,
    ) -> tuple[sqlite3.Row, bool]:
        latest = self._latest_coordinator_restart(lineage_id)
        if latest is not None and latest["acknowledged_at"] is None:
            return latest, False
        generation = 1 if latest is None else int(latest["generation"]) + 1
        self.connection.execute(
            """
            INSERT INTO coordinator_restart_requests (
                lineage_id, generation, requested_at, reason
            ) VALUES (?, ?, ?, ?)
            """,
            (lineage_id, generation, requested_at, reason),
        )
        created = self.connection.execute(
            """
            SELECT * FROM coordinator_restart_requests
            WHERE lineage_id = ? AND generation = ?
            """,
            (lineage_id, generation),
        ).fetchone()
        if created is None:
            raise DeadlineError("Failed to persist coordinator restart request")
        return created, True

    def _ensure_random_cycle(self, lineage_id: str) -> sqlite3.Row:
        cycle = self._latest_random_cycle(lineage_id)
        if cycle is not None and cycle["resolution_evidence"] is None:
            cycle = self._mark_random_cycle_due_if_reached(cycle)
            if cycle["due_task_id"] is not None:
                cycle = self._snapshot_universal_capability(
                    lineage_id, int(cycle["cycle_number"])
                )
            if cycle["resolution_evidence"] is None:
                return cycle
        number = 1 if cycle is None else int(cycle["cycle_number"]) + 1
        cycle_start = (
            0 if cycle is None else int(cycle["due_after_terminal_windows"])
        )
        interval, lane = self._draw_random_cycle()
        self.connection.execute(
            """
            INSERT INTO random_mutation_cycles (
                lineage_id, cycle_number, interval_windows,
                cadence_version, due_after_terminal_windows, selected_lane,
                universal_required
            ) VALUES (?, ?, ?, ?, ?, ?, ?)
            """,
            (
                lineage_id,
                number,
                interval,
                CADENCE_VERSION,
                cycle_start + interval,
                lane,
                0,
            ),
        )
        cycle = self._latest_random_cycle(lineage_id)
        if cycle is None:
            raise DeadlineError("Failed to persist random mutation cycle")
        cycle = self._mark_random_cycle_due_if_reached(cycle)
        if cycle["due_task_id"] is not None:
            cycle = self._snapshot_universal_capability(
                lineage_id, int(cycle["cycle_number"])
            )
        return cycle

    def _mark_random_cycle_due_if_reached(self, cycle: sqlite3.Row) -> sqlite3.Row:
        if cycle["due_task_id"] is not None:
            return cycle
        boundary = self.connection.execute(
            """
            SELECT task_id FROM tasks
            WHERE lineage_id = ? AND attempt_terminal_at IS NOT NULL
              AND NOT (
                attempt_terminal_kind = 'restart_normalized'
                OR (
                    attempt_terminal_kind = 'abandoned'
                    AND abandonment_reason = 'external_supervisor_restart_normalization'
                )
              )
            ORDER BY attempt_terminal_at, task_id
            LIMIT 1 OFFSET ?
            """,
            (
                cycle["lineage_id"],
                cycle["due_after_terminal_windows"] - 1,
            ),
        ).fetchone()
        if boundary is None:
            return cycle
        self.connection.execute(
            """
            UPDATE random_mutation_cycles SET due_task_id = ?
            WHERE lineage_id = ? AND cycle_number = ?
              AND due_task_id IS NULL AND resolution_evidence IS NULL
            """,
            (
                boundary["task_id"],
                cycle["lineage_id"],
                cycle["cycle_number"],
            ),
        )
        refreshed = self._latest_random_cycle(cycle["lineage_id"])
        if refreshed is None:
            raise DeadlineError("Failed to reconcile random mutation cycle")
        return refreshed

    def _random_cycle_result(self, row: sqlite3.Row) -> dict[str, Any]:
        completed = self._terminal_window_count(row["lineage_id"])
        due = row["due_task_id"] is not None and row["resolution_evidence"] is None
        ordinary_resolved = row["ordinary_resolution_evidence"] is not None
        universal_triggered = self._universal_signature_seen(
            int(row["interval_windows"]), str(row["selected_lane"])
        )
        universal_required = bool(row["universal_required"])
        universal_resolved = (
            not universal_required
            or row["universal_resolution_evidence"] is not None
        )
        return {
            "cycle_number": row["cycle_number"],
            "interval_windows": row["interval_windows"],
            "cadence_version": row["cadence_version"],
            "due_after_terminal_windows": row["due_after_terminal_windows"],
            "selected_lane": row["selected_lane"],
            "completed_terminal_windows": completed,
            "due": due,
            "due_task_id": row["due_task_id"],
            "ordinary_resolved": ordinary_resolved,
            "universal_signature_seen": universal_triggered,
            "universal_required": universal_required,
            "universal_resolved": universal_resolved,
            "universal_receipt_id": row["universal_receipt_id"],
            "universal_capability_status": row["universal_capability_status"],
            "universal_capability_reason": row["universal_capability_reason"],
            "universal_capability_checked_at": row[
                "universal_capability_checked_at"
            ],
            "universal_capability_roster_digest": row[
                "universal_capability_roster_digest"
            ],
            "pending_components": (
                ["ordinary"] if not ordinary_resolved else []
            ) + (["universal"] if not universal_resolved else []),
            "restart_generation": row["restart_generation"],
        }

    def _random_mutation_status(self, lineage_id: str) -> dict[str, Any]:
        return self._random_cycle_result(self._ensure_random_cycle(lineage_id))

    def _record_terminal_window(
        self,
        task: sqlite3.Row,
        terminal_at: float,
        terminal_kind: str,
    ) -> None:
        current = self._task(task["lineage_id"], task["task_id"])
        if current["attempt_terminal_at"] is not None:
            return
        cycle = self._ensure_random_cycle(task["lineage_id"])
        self.connection.execute(
            """
            UPDATE tasks
            SET terminal_at = ?, attempt_terminal_at = ?, attempt_terminal_kind = ?
            WHERE lineage_id = ? AND task_id = ? AND attempt_terminal_at IS NULL
            """,
            (
                terminal_at,
                terminal_at,
                terminal_kind,
                task["lineage_id"],
                task["task_id"],
            ),
        )
        self.connection.execute(
            """
            UPDATE worker_claims
            SET released_at = ?, release_reason = ?
            WHERE lineage_id = ? AND task_id = ? AND released_at IS NULL
            """,
            (
                terminal_at,
                f"task_{terminal_kind}",
                task["lineage_id"],
                task["task_id"],
            ),
        )
        completed = self._terminal_window_count(task["lineage_id"])
        if (
            cycle["due_task_id"] is None
            and completed >= cycle["due_after_terminal_windows"]
        ):
            self.connection.execute(
                """
                UPDATE random_mutation_cycles
                SET due_task_id = ?
                WHERE lineage_id = ? AND cycle_number = ?
                """,
                (
                    task["task_id"],
                    task["lineage_id"],
                    cycle["cycle_number"],
                ),
            )

    @staticmethod
    def _incident_result(row: sqlite3.Row, recorded: bool) -> dict[str, Any]:
        threshold = row["cadence_threshold"]
        return {
            "lineage_id": row["lineage_id"],
            "task_id": row["task_id"],
            "kind": row["kind"],
            "recorded": recorded,
            "recorded_at": row["recorded_at"],
            "short_verdict": row["short_verdict"],
            "reviewed_at": row["reviewed_at"],
            "reason": row["reason"],
            "long_detail": row["long_detail"],
            "units": row["units"],
            "cumulative_before": row["cumulative_before"],
            "cumulative_after": row["cumulative_after"],
            "independent_review_required": row["reviewed_at"] is None,
            "cadence_crossed": threshold is not None,
            "cadence_threshold": threshold,
        }

    @staticmethod
    def _finding_result(row: sqlite3.Row, recorded: bool) -> dict[str, Any]:
        return {
            "lineage_id": row["lineage_id"],
            "task_id": row["task_id"],
            "kind": row["kind"],
            "reported_at": row["reported_at"],
            "short_verdict": row["short_verdict"],
            "evidence": row["evidence"],
            "recorded": recorded,
        }

    def _worker_finding(
        self, lineage_id: str, task_id: str
    ) -> sqlite3.Row | None:
        return self.connection.execute(
            """
            SELECT * FROM worker_findings
            WHERE lineage_id = ? AND task_id = ?
            """,
            (lineage_id, task_id),
        ).fetchone()

    def _record_incident(
        self,
        lineage_id: str,
        task_id: str,
        kind: str,
        units: int,
        recorded_at: float,
        reason: str | None = None,
    ) -> dict[str, Any]:
        existing = self.connection.execute(
            "SELECT * FROM incidents WHERE lineage_id = ? AND task_id = ? AND kind = ?",
            (lineage_id, task_id, kind),
        ).fetchone()
        if existing is not None:
            return self._incident_result(existing, recorded=False)

        prior = self._cumulative_units(lineage_id)
        new = prior + units
        crossed = prior // 3 < new // 3
        threshold = (prior // 3 + 1) * 3 if crossed else None
        short_verdict = kind.replace("_", " ")
        long_detail = self._default_incident_detail(kind, reason)
        cursor = self.connection.execute(
            """
            INSERT INTO incidents (
                lineage_id, task_id, kind, recorded_at, reason,
                short_verdict, long_detail, units,
                cumulative_before, cumulative_after, cadence_threshold
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            """,
            (
                lineage_id,
                task_id,
                kind,
                recorded_at,
                reason,
                short_verdict,
                long_detail,
                units,
                prior,
                new,
                threshold,
            ),
        )
        row = self.connection.execute(
            "SELECT * FROM incidents WHERE incident_id = ?", (cursor.lastrowid,)
        ).fetchone()
        return self._incident_result(row, recorded=True)

    def _record_miss_if_due(
        self,
        task: sqlite3.Row,
        now: float,
        *,
        completion_invalid: bool = False,
    ) -> dict[str, Any] | None:
        claim = self._task_claim(task["lineage_id"], task["task_id"])
        if claim["retired_at"] is not None:
            return None
        accepted = self._latest_valid_acceptance(
            str(task["lineage_id"]), str(task["claim_id"])
        )
        missed = now >= claim["deadline_at"] and (
            completion_invalid
            or task["integrity_breached_at"] is not None
            or accepted is None
            or accepted["accepted_at"] >= claim["deadline_at"]
        )
        if not missed:
            return None
        if self.connection.execute(
            "SELECT 1 FROM claim_clocks WHERE lineage_id = ? AND claim_id = ?",
            (task["lineage_id"], task["claim_id"]),
        ).fetchone() is None:
            return self._record_incident(
                task["lineage_id"], task["task_id"], "deadline_miss", 1, now
            )
        existing = self._claim_deadline_incident(
            str(task["lineage_id"]), str(task["claim_id"]),
            int(task["deadline_generation"]),
        )
        if existing is None:
            self.connection.execute(
                """
                INSERT INTO claim_deadline_generation_incidents (
                    lineage_id, claim_id, generation, source_task_id, recorded_at,
                    short_verdict, long_detail
                ) VALUES (?, ?, ?, ?, ?, 'deadline miss', ?)
                """,
                (
                    task["lineage_id"],
                    task["claim_id"],
                    task["deadline_generation"],
                    task["task_id"],
                    now,
                    self._default_incident_detail("deadline_miss", None),
                ),
            )
            if self.connection.execute(
                """
                SELECT 1 FROM claim_deadline_incidents
                WHERE lineage_id = ? AND claim_id = ?
                """,
                (task["lineage_id"], task["claim_id"]),
            ).fetchone() is None:
                self.connection.execute(
                    """
                    INSERT INTO claim_deadline_incidents (
                        lineage_id, claim_id, source_task_id, recorded_at,
                        short_verdict, long_detail
                    ) VALUES (?, ?, ?, ?, 'deadline miss', ?)
                    """,
                    (
                        task["lineage_id"], task["claim_id"], task["task_id"], now,
                        self._default_incident_detail("deadline_miss", None),
                    ),
                )
            compatibility = self._record_incident(
                task["lineage_id"], task["task_id"], "deadline_miss", 1, now
            )
            recorded = True
        else:
            compatibility_row = self.connection.execute(
                """
                SELECT * FROM incidents
                WHERE lineage_id = ? AND task_id = ? AND kind = 'deadline_miss'
                """,
                (existing["lineage_id"], existing["source_task_id"]),
            ).fetchone()
            compatibility = (
                self._incident_result(compatibility_row, recorded=False)
                if compatibility_row is not None
                else {
                    "lineage_id": existing["lineage_id"],
                    "task_id": existing["source_task_id"],
                    "kind": "deadline_miss",
                    "recorded": False,
                    "recorded_at": existing["recorded_at"],
                    "units": 1,
                }
            )
            recorded = False
        result = dict(compatibility)
        result["recorded"] = recorded
        result["claim_id"] = task["claim_id"]
        result["deadline_generation"] = task["deadline_generation"]
        result["mutation_pending"] = self._deadline_mutation_pending(
            str(task["lineage_id"]), str(task["claim_id"])
        )
        result["pending_mutation_components"] = ["micro", "macro"]
        return result

    def _status(self, lineage_id: str, task_id: str, now: float) -> dict[str, Any]:
        task = self._task(lineage_id, task_id)
        claim = (
            self._task_claim(lineage_id, task_id)
            if self.connection.execute(
                "SELECT 1 FROM claim_clocks WHERE lineage_id = ? AND claim_id = ?",
                (lineage_id, task["claim_id"]),
            ).fetchone() is not None
            else None
        )
        migration_conflict = (
            self._migration_conflict(lineage_id, str(task["claim_id"]))
            if claim is None
            else None
        )
        finding = self._worker_finding(lineage_id, task_id)
        incidents = self.connection.execute(
            """
            SELECT * FROM incidents
            WHERE lineage_id = ? AND task_id = ?
            ORDER BY incident_id
            """,
            (lineage_id, task_id),
        ).fetchall()
        kinds = {row["kind"] for row in incidents}
        attempt_completed = (
            task["attempt_terminal_kind"] == "completed"
            and task["completed_at"] is not None
            and task["integrity_breached_at"] is None
            and finding is None
        )
        claim_acceptance = self._latest_valid_acceptance(
            lineage_id, str(task["claim_id"])
        )
        completion_accepted = claim_acceptance is not None
        claim_deadline_incident = self._claim_deadline_incident(
            lineage_id, str(task["claim_id"]), int(task["deadline_generation"])
        )
        if task["integrity_breached_at"] is not None:
            state = "integrity_breach"
        elif task["attempt_terminal_kind"] == "restart_normalized":
            state = "restart_normalized"
        elif task["attempt_terminal_kind"] == "abandoned":
            state = "abandoned"
        elif attempt_completed:
            state = "completed"
        elif finding is not None:
            state = "worker_finding"
        elif "deadline_miss" in kinds:
            state = "deadline_missed"
        else:
            state = "running"
        return {
            "lineage_id": lineage_id,
            "task_id": task_id,
            "claim_id": task["claim_id"],
            "deadline_generation": task["deadline_generation"],
            "state": state,
            "phase": claim["phase"] if claim is not None else "migration_conflict",
            "phase_at_dispatch": task["phase_at_dispatch"],
            "phase_sequence_at_dispatch": task["phase_sequence_at_dispatch"],
            "closure_gap_id": task["closure_gap_id"],
            "closure_gap_revision": task["closure_gap_revision"],
            "estimate_seconds": (
                claim["estimate_seconds"] if claim is not None else task["estimate_seconds"]
            ),
            "attempt_estimate_seconds": task["estimate_seconds"],
            "started_at": claim["started_at"] if claim is not None else task["started_at"],
            "claim_started_at": (
                claim["started_at"] if claim is not None else None
            ),
            "attempt_dispatched_at": task["started_at"],
            "deadline_at": claim["deadline_at"] if claim is not None else task["deadline_at"],
            "checked_at": now,
            "completion_accepted": completion_accepted,
            "attempt_completed": attempt_completed,
            "completed_at": task["completed_at"],
            "completion_evidence": task["completion_evidence"],
            "attempt_terminal_at": task["attempt_terminal_at"],
            "attempt_terminal_kind": task["attempt_terminal_kind"],
            "finding_reported": finding is not None,
            "worker_finding": (
                self._finding_result(finding, recorded=False)
                if finding is not None
                else None
            ),
            "deadline_missed": claim_deadline_incident is not None,
            "integrity_breached": "integrity_breach" in kinds,
            "integrity_reason": task["integrity_reason"],
            "integrity_mutation_pending": self._integrity_mutation_pending(
                lineage_id, task_id
            ),
            "deadline_mutation_pending": self._deadline_mutation_pending(
                lineage_id, str(task["claim_id"])
            ),
            "claim_clock_migration_conflict": migration_conflict is not None,
            "claim_acceptance": (
                {
                    "task_id": claim_acceptance["task_id"],
                    "closure_sequence": claim_acceptance["closure_sequence"],
                    "accepted_at": claim_acceptance["accepted_at"],
                    "evidence": claim_acceptance["evidence"],
                }
                if claim_acceptance is not None
                else None
            ),
            "cumulative_miss_units": self._cumulative_units(lineage_id),
            "incidents": [self._incident_result(row, recorded=False) for row in incidents],
        }

    @staticmethod
    def _compact_task_result(status: dict[str, Any]) -> dict[str, Any]:
        finding = status["worker_finding"]
        incidents_by_kind = {
            incident["kind"]: incident for incident in status["incidents"]
        }
        if status["state"] == "integrity_breach":
            current_short_verdict = incidents_by_kind["integrity_breach"][
                "short_verdict"
            ]
        elif finding is not None:
            current_short_verdict = finding["short_verdict"]
        else:
            current_incident = next(
                (
                    incidents_by_kind[kind]
                    for kind in ("integrity_breach", "deadline_miss")
                    if kind in incidents_by_kind
                ),
                None,
            )
            current_short_verdict = (
                current_incident["short_verdict"]
                if current_incident is not None
                else None
            )
        return {
            "task_id": status["task_id"],
            "claim_id": status["claim_id"],
            "state": status["state"],
            "phase": status["phase"],
            "closure_gap_id": status["closure_gap_id"],
            "closure_gap_revision": status["closure_gap_revision"],
            "deadline_at": status["deadline_at"],
            "deadline_mutation_pending": status["deadline_mutation_pending"],
            "integrity_mutation_pending": status["integrity_mutation_pending"],
            "current_short_verdict": current_short_verdict,
        }

    @staticmethod
    def _pending_random_gate(
        random_mutation: dict[str, Any] | None,
    ) -> dict[str, Any] | None:
        if random_mutation is None or not random_mutation["due"]:
            return None
        return {
            "cycle_number": random_mutation["cycle_number"],
            "selected_lane": random_mutation["selected_lane"],
            "due_task_id": random_mutation["due_task_id"],
            "universal_signature_seen": random_mutation[
                "universal_signature_seen"
            ],
            "universal_required": random_mutation["universal_required"],
            "universal_capability_status": random_mutation[
                "universal_capability_status"
            ],
            "universal_capability_reason": random_mutation[
                "universal_capability_reason"
            ],
            "universal_capability_checked_at": random_mutation[
                "universal_capability_checked_at"
            ],
            "universal_capability_roster_digest": random_mutation[
                "universal_capability_roster_digest"
            ],
            "pending_components": random_mutation["pending_components"],
        }

    @staticmethod
    def _pending_restart_gate(
        coordinator_restart: dict[str, Any] | None,
    ) -> dict[str, Any] | None:
        if coordinator_restart is None or not coordinator_restart["pending"]:
            return None
        return {
            "generation": coordinator_restart["generation"],
            "pending": True,
            "expected_run_id": coordinator_restart["expected_run_id"],
        }

    def _pending_incident_reviews(self, lineage_id: str) -> list[dict[str, Any]]:
        rows = self.connection.execute(
            """
            SELECT incidents.task_id, tasks.claim_id, incidents.kind,
                   incidents.recorded_at
            FROM incidents
            JOIN tasks USING (lineage_id, task_id)
            WHERE incidents.lineage_id = ? AND incidents.reviewed_at IS NULL
            ORDER BY incidents.recorded_at, incidents.incident_id
            """,
            (lineage_id,),
        ).fetchall()
        return [
            {
                "task_id": row["task_id"],
                "claim_id": row["claim_id"],
                "kind": row["kind"],
                "recorded_at": row["recorded_at"],
            }
            for row in rows
        ]

    def _recent_failure_verdicts(self, lineage_id: str) -> list[dict[str, Any]]:
        rows = self.connection.execute(
            """
            SELECT task_id, claim_id, kind, recorded_at, short_verdict
            FROM (
                SELECT incidents.task_id, tasks.claim_id, incidents.kind,
                       incidents.recorded_at, incidents.short_verdict,
                       0 AS source_order
                FROM incidents
                JOIN tasks USING (lineage_id, task_id)
                WHERE incidents.lineage_id = ?
                  AND incidents.reviewed_at IS NOT NULL
                UNION ALL
                SELECT worker_findings.task_id, tasks.claim_id,
                       worker_findings.kind, worker_findings.reported_at,
                       worker_findings.short_verdict, 1 AS source_order
                FROM worker_findings
                JOIN tasks USING (lineage_id, task_id)
                WHERE worker_findings.lineage_id = ?
            )
            ORDER BY recorded_at DESC, task_id DESC, source_order DESC, kind DESC
            LIMIT ?
            """,
            (lineage_id, lineage_id, RECENT_FAILURE_VERDICT_LIMIT),
        ).fetchall()
        return [
            {
                "task_id": row["task_id"],
                "claim_id": row["claim_id"],
                "kind": row["kind"],
                "recorded_at": row["recorded_at"],
                "short_verdict": row["short_verdict"],
            }
            for row in rows
        ]

    def claim_worker(
        self,
        lineage_id: str,
        task_id: str,
        worker_id: str,
        coordinator_session_id: str,
        supervisor_id: str,
        *,
        now: float | None = None,
    ) -> dict[str, Any]:
        lineage_id = self._identity(lineage_id, "Lineage id")
        task_id = self._identity(task_id, "Task id")
        worker_id = self._identity(worker_id, "Worker id")
        coordinator_session_id = self._identity(
            coordinator_session_id, "Coordinator session id"
        )
        supervisor_id = self._identity(supervisor_id, "Supervisor id")
        claimed_at = self._now(now)
        self._begin()
        try:
            task = self._task(lineage_id, task_id)
            if task["attempt_terminal_at"] is not None:
                raise DeadlineError("Cannot claim a terminal worker attempt")
            existing = self.connection.execute(
                "SELECT * FROM worker_claims WHERE lineage_id = ? AND task_id = ?",
                (lineage_id, task_id),
            ).fetchone()
            identity = (worker_id, coordinator_session_id, supervisor_id)
            if existing is None:
                self.connection.execute(
                    """
                    INSERT INTO worker_claims (
                        lineage_id, task_id, worker_id, coordinator_session_id,
                        supervisor_id, claimed_at, last_checkpoint_at
                    ) VALUES (?, ?, ?, ?, ?, ?, ?)
                    """,
                    (lineage_id, task_id, *identity, claimed_at, claimed_at),
                )
                recorded = True
            elif existing["released_at"] is None and identity == (
                existing["worker_id"],
                existing["coordinator_session_id"],
                existing["supervisor_id"],
            ):
                recorded = False
            else:
                raise DeadlineError("Worker attempt already has another ownership claim")
            self.connection.commit()
            return {
                "task_id": task_id,
                "worker_id": worker_id,
                "coordinator_session_id": coordinator_session_id,
                "supervisor_id": supervisor_id,
                "claimed_at": claimed_at if existing is None else existing["claimed_at"],
                "recorded": recorded,
                "state": "claimed",
            }
        except Exception:
            self.connection.rollback()
            raise

    def checkpoint_worker(
        self,
        lineage_id: str,
        task_id: str,
        worker_id: str,
        kind: str,
        evidence: str,
        *,
        now: float | None = None,
    ) -> dict[str, Any]:
        lineage_id = self._identity(lineage_id, "Lineage id")
        task_id = self._identity(task_id, "Task id")
        worker_id = self._identity(worker_id, "Worker id")
        kind = self._identity(kind, "Checkpoint kind")
        evidence = self._nonempty_text(evidence, "Checkpoint evidence")
        recorded_at = self._now(now)
        self._begin()
        try:
            claim = self.connection.execute(
                "SELECT * FROM worker_claims WHERE lineage_id = ? AND task_id = ?",
                (lineage_id, task_id),
            ).fetchone()
            if claim is None or claim["released_at"] is not None:
                raise DeadlineError("Worker attempt has no active ownership claim")
            if claim["worker_id"] != worker_id:
                raise DeadlineError("Checkpoint worker does not own this attempt")
            sequence = int(
                self.connection.execute(
                    "SELECT COUNT(*) FROM worker_checkpoints WHERE lineage_id = ? AND task_id = ?",
                    (lineage_id, task_id),
                ).fetchone()[0]
            ) + 1
            self.connection.execute(
                """
                INSERT INTO worker_checkpoints
                (lineage_id, task_id, sequence, worker_id, kind, evidence, recorded_at)
                VALUES (?, ?, ?, ?, ?, ?, ?)
                """,
                (lineage_id, task_id, sequence, worker_id, kind, evidence, recorded_at),
            )
            self.connection.execute(
                """
                UPDATE worker_claims SET last_checkpoint_at = ?
                WHERE lineage_id = ? AND task_id = ?
                """,
                (recorded_at, lineage_id, task_id),
            )
            self.connection.commit()
            return {"task_id": task_id, "worker_id": worker_id, "sequence": sequence,
                    "kind": kind, "state": "checkpointed"}
        except Exception:
            self.connection.rollback()
            raise

    @staticmethod
    def _decode_worker_receipt(row: sqlite3.Row) -> dict[str, Any]:
        try:
            envelope = json.loads(str(row["evidence"]))
        except json.JSONDecodeError as error:
            raise DeadlineError("Stored worker result receipt is corrupt") from error
        if not isinstance(envelope, dict) or not isinstance(
            envelope.get("receipt"), dict
        ):
            raise DeadlineError("Stored worker result receipt envelope is corrupt")
        receipt = dict(envelope["receipt"])
        expected_id = receipt_envelope(receipt)["receipt_id"]
        if envelope.get("receipt_id") != expected_id:
            raise DeadlineError("Stored worker result receipt digest is corrupt")
        return {
            "receipt_id": expected_id,
            "receipt": receipt,
            "sequence": int(row["sequence"]),
            "recorded_at": float(row["recorded_at"]),
        }

    def _worker_result_receipts(
        self, lineage_id: str, task_id: str | None = None
    ) -> list[dict[str, Any]]:
        parameters: list[Any] = [lineage_id]
        task_clause = ""
        if task_id is not None:
            task_clause = " AND checkpoint.task_id = ?"
            parameters.append(task_id)
        rows = self.connection.execute(
            """
            SELECT checkpoint.task_id, checkpoint.sequence, checkpoint.worker_id,
                   checkpoint.evidence, checkpoint.recorded_at
            FROM worker_checkpoints AS checkpoint
            WHERE checkpoint.lineage_id = ?
              AND checkpoint.kind = 'result-receipt-v1'
            """ + task_clause + " ORDER BY checkpoint.recorded_at, checkpoint.task_id, checkpoint.sequence",
            parameters,
        ).fetchall()
        return [self._decode_worker_receipt(row) for row in rows]

    def record_worker_result_receipt(
        self,
        lineage_id: str,
        task_id: str,
        worker_id: str,
        value: Any,
        *,
        workspace: Path | None = None,
        now: float | None = None,
    ) -> dict[str, Any]:
        """Persist one compact, identity-bound result or continuation receipt."""

        lineage_id = self._identity(lineage_id, "Lineage id")
        task_id = self._identity(task_id, "Task id")
        worker_id = self._identity(worker_id, "Worker id")
        recorded_at = self._now(now)
        self._begin()
        try:
            task = self._task(lineage_id, task_id)
            claim = self.connection.execute(
                """
                SELECT * FROM worker_claims
                WHERE lineage_id = ? AND task_id = ?
                """,
                (lineage_id, task_id),
            ).fetchone()
            if claim is None or claim["worker_id"] != worker_id:
                raise DeadlineError("Worker result receipt does not match task ownership")
            try:
                receipt = normalize_worker_receipt(
                    value,
                    lineage_id=lineage_id,
                    task_id=task_id,
                    claim_id=str(task["claim_id"]),
                    worker_id=worker_id,
                    workspace=workspace,
                )
            except WorkerReceiptError as error:
                raise DeadlineError(str(error)) from error
            envelope = receipt_envelope(receipt)
            receipt_id = str(envelope["receipt_id"])
            existing = self._worker_result_receipts(lineage_id, task_id)
            same = next(
                (item for item in existing if item["receipt_id"] == receipt_id), None
            )
            if same is not None:
                result = compact_worker_receipt(
                    same["receipt"], recorded_at=same["recorded_at"]
                )
                result.update({"recorded": False, "sequence": same["sequence"]})
                self.connection.commit()
                return result
            disposition = str(receipt["disposition"])
            terminal_receipts = [
                item
                for item in existing
                if item["receipt"].get("disposition") != "checkpoint"
            ]
            # A returned worker turn can be resumed without terminalizing the
            # task.  In that case an earlier non-checkpoint receipt is durable
            # ingress, but it must not prevent the still-live attempt from
            # recording its eventual terminal result.  Once the task itself
            # is terminal, keep the one-terminal-receipt invariant.
            if (
                terminal_receipts
                and disposition != "checkpoint"
                and task["attempt_terminal_kind"] is not None
            ):
                raise DeadlineError("Worker attempt already has a terminal result receipt")
            terminal_kind = task["attempt_terminal_kind"]
            if terminal_kind is not None and disposition not in {
                str(terminal_kind), "checkpoint"
            }:
                raise DeadlineError(
                    "Worker result receipt disposition does not match terminal task state"
                )
            sequence = int(
                self.connection.execute(
                    """
                    SELECT COUNT(*) FROM worker_checkpoints
                    WHERE lineage_id = ? AND task_id = ?
                    """,
                    (lineage_id, task_id),
                ).fetchone()[0]
            ) + 1
            self.connection.execute(
                """
                INSERT INTO worker_checkpoints
                (lineage_id, task_id, sequence, worker_id, kind, evidence, recorded_at)
                VALUES (?, ?, ?, ?, 'result-receipt-v1', ?, ?)
                """,
                (
                    lineage_id,
                    task_id,
                    sequence,
                    worker_id,
                    json.dumps(envelope, ensure_ascii=False, sort_keys=True, separators=(",", ":")),
                    recorded_at,
                ),
            )
            if claim["released_at"] is None:
                self.connection.execute(
                    """
                    UPDATE worker_claims SET last_checkpoint_at = ?
                    WHERE lineage_id = ? AND task_id = ?
                    """,
                    (recorded_at, lineage_id, task_id),
                )
            self.connection.commit()
            result = compact_worker_receipt(receipt, recorded_at=recorded_at)
            result.update({"recorded": True, "sequence": sequence})
            return result
        except Exception:
            self.connection.rollback()
            raise

    def require_worker_result_receipt(
        self,
        lineage_id: str,
        task_id: str,
        receipt_id: str,
        disposition: str,
    ) -> dict[str, Any]:
        """Return the exact receipt that permits one terminal transition."""

        receipt_id = self._identity(receipt_id, "Worker result receipt id")
        receipts = self._worker_result_receipts(lineage_id, task_id)
        match = next(
            (item for item in receipts if item["receipt_id"] == receipt_id), None
        )
        if match is None:
            raise DeadlineError("Terminal transition requires its worker result receipt")
        if match["receipt"].get("disposition") != disposition:
            raise DeadlineError(
                f"Worker result receipt must have disposition {disposition}"
            )
        return match

    def _require_terminal_worker_receipt(
        self,
        lineage_id: str,
        task_id: str,
        receipt_id: str | None,
        disposition: str,
    ) -> None:
        ownership = self.connection.execute(
            "SELECT 1 FROM worker_claims WHERE lineage_id = ? AND task_id = ?",
            (lineage_id, task_id),
        ).fetchone()
        if ownership is None:
            return
        if receipt_id is None:
            raise DeadlineError(
                "A worker-owned terminal transition requires its worker result receipt"
            )
        self.require_worker_result_receipt(
            lineage_id, task_id, receipt_id, disposition
        )

    def query_worker_result_receipts(
        self,
        lineage_id: str,
        *,
        filters: dict[str, str],
        latest: bool = False,
        full: bool = False,
    ) -> dict[str, Any]:
        """Return only receipts matching explicit continuation/evidence identities."""

        lineage_id = self._identity(lineage_id, "Lineage id")
        if not filters and not latest:
            raise DeadlineError("Worker receipt query requires a filter or --latest")
        task_id = filters.get("task_id")
        rows = self._worker_result_receipts(lineage_id, task_id)

        def matches(item: dict[str, Any]) -> bool:
            receipt = item["receipt"]
            direct = {
                "receipt_id": item["receipt_id"],
                "task_id": receipt.get("task_id"),
                "claim_id": receipt.get("claim_id"),
                "worker_id": receipt.get("worker_id"),
                "disposition": receipt.get("disposition"),
                "verdict": receipt.get("verdict"),
                "run_id": receipt.get("bindings", {}).get("run_id"),
                "scenario_id": receipt.get("bindings", {}).get("scenario_id"),
                "binding_id": receipt.get("bindings", {}).get("binding_id"),
                "first_divergence_class": (
                    receipt.get("first_divergence") or {}
                ).get("class"),
            }
            for key, expected in filters.items():
                if key in direct:
                    if str(direct.get(key) or "") != expected:
                        return False
                    continue
                entry_field = {
                    "event_type": "event_type",
                    "evidence_class": "evidence_class",
                    "actor_id": "actor_id",
                    "action_id": "action_id",
                    "native_receipt_id": "receipt_id",
                }.get(key)
                if entry_field is None or not any(
                    str(entry.get(entry_field, "")) == expected
                    for entry in receipt.get("journal_entries", [])
                ):
                    return False
            return True

        selected = [item for item in rows if matches(item)]
        if latest and selected:
            selected = [selected[-1]]
        results = []
        for item in selected:
            if full:
                value = {
                    "receipt_id": item["receipt_id"],
                    "sequence": item["sequence"],
                    "recorded_at": item["recorded_at"],
                    "receipt": item["receipt"],
                }
            else:
                value = compact_worker_receipt(
                    item["receipt"], recorded_at=item["recorded_at"]
                )
                value["sequence"] = item["sequence"]
            results.append(value)
        return {"lineage_id": lineage_id, "matches": results}

    def release_worker_claim(
        self,
        lineage_id: str,
        task_id: str,
        worker_id: str,
        reason: str,
        *,
        now: float | None = None,
    ) -> dict[str, Any]:
        lineage_id = self._identity(lineage_id, "Lineage id")
        task_id = self._identity(task_id, "Task id")
        worker_id = self._identity(worker_id, "Worker id")
        reason = self._nonempty_text(reason, "Release reason")
        released_at = self._now(now)
        self._begin()
        try:
            claim = self.connection.execute(
                "SELECT * FROM worker_claims WHERE lineage_id = ? AND task_id = ?",
                (lineage_id, task_id),
            ).fetchone()
            if claim is None:
                raise DeadlineError("Worker attempt has no ownership claim")
            if claim["worker_id"] != worker_id:
                raise DeadlineError("Release worker does not own this attempt")
            if claim["released_at"] is None:
                task = self._task(lineage_id, task_id)
                if task["attempt_terminal_at"] is None:
                    self._record_miss_if_due(task, released_at)
                    self.connection.execute(
                        """UPDATE tasks SET abandoned_at = ?, abandonment_reason = ?
                           WHERE lineage_id = ? AND task_id = ?""",
                        (released_at, reason, lineage_id, task_id),
                    )
                    self._record_terminal_window(task, released_at, "abandoned")
                self.connection.execute(
                    """UPDATE worker_claims SET released_at = ?, release_reason = ?
                       WHERE lineage_id = ? AND task_id = ?""",
                    (released_at, reason, lineage_id, task_id),
                )
                recorded = True
            else:
                recorded = False
            self.connection.commit()
            return {"task_id": task_id, "worker_id": worker_id,
                    "released_at": claim["released_at"] or released_at,
                    "recorded": recorded, "state": "released"}
        except Exception:
            self.connection.rollback()
            raise
    def start_task(
        self,
        lineage_id: str,
        task_id: str,
        claim_id: str,
        estimate_seconds: float,
        *,
        attempt_estimate_seconds: float | None = None,
        phase: str | None = None,
        gap_id: str | None = None,
        now: float | None = None,
    ) -> dict[str, Any]:
        lineage_id = self._identity(lineage_id, "Lineage id")
        task_id = self._identity(task_id, "Task id")
        claim_id = self._identity(claim_id, "Claim id")
        if gap_id is not None:
            gap_id = self._identity(gap_id, "Closure gap id")
        estimate = self._positive_estimate(estimate_seconds)
        attempt_estimate = (
            estimate
            if attempt_estimate_seconds is None
            else self._positive_estimate(attempt_estimate_seconds)
        )
        started_at = self._now(now)
        self._begin()
        try:
            self._bind_lineage(lineage_id)
            self._ensure_random_cycle(lineage_id)
            existing = self.connection.execute(
                """
                SELECT * FROM tasks
                WHERE lineage_id = ? AND task_id = ?
                """,
                (lineage_id, task_id),
            ).fetchone()
            random_mutation = self._random_mutation_status(lineage_id)
            coordinator_restart = self._coordinator_restart_status(lineage_id)
            claim_exists = self.connection.execute(
                "SELECT 1 FROM claim_clocks WHERE lineage_id = ? AND claim_id = ?",
                (lineage_id, claim_id),
            ).fetchone()
            claim = self._claim(lineage_id, claim_id) if claim_exists is not None else None
            conflict = self._migration_conflict(lineage_id, claim_id)
            if (
                claim is None
                and conflict is not None
                and conflict["resolved_at"] is None
            ):
                raise DeadlineError(
                    "Claim has divergent legacy task clocks; resolve the migration "
                    "before dispatching a new attempt"
                )
            if claim is not None:
                anchor = existing or self.connection.execute(
                    """
                    SELECT * FROM tasks
                    WHERE lineage_id = ? AND claim_id = ?
                    ORDER BY started_at DESC, task_id DESC LIMIT 1
                    """,
                    (lineage_id, claim_id),
                ).fetchone()
                if anchor is None:
                    raise DeadlineError("Claim has no worker attempt")
                self._record_miss_if_due(anchor, started_at)
            if (
                existing is None
                and coordinator_restart is not None
                and coordinator_restart["pending"]
            ):
                raise DeadlineError(
                    "Coordinator restart generation "
                    f"{coordinator_restart['generation']} is pending; "
                    "acknowledge it before dispatching a new task"
                )
            pending_deadline_mutations = self._pending_deadline_mutations(lineage_id)
            if existing is None and pending_deadline_mutations:
                # A just-detected claim miss is durable even though this dispatch
                # is rejected.  The outer rollback is then a no-op.
                self.connection.commit()
                raise DeadlineError(
                    "Deadline mutation is pending; resolve micro and macro components "
                    "before dispatching a new attempt"
                )
            pending_integrity_mutations = self._pending_integrity_mutations(lineage_id)
            if existing is None and pending_integrity_mutations:
                raise DeadlineError(
                    "Integrity mutation is pending; diagnose the exact incident and "
                    "resolve its micro and macro components before dispatching a new task"
                )
            if existing is None and random_mutation["due"]:
                raise DeadlineError(
                    "Random improvement review is due; resolve it before dispatching a new task"
                )
            advanced_deadline_generation = False
            if existing is None and claim is not None:
                if claim["retired_at"] is not None:
                    administrative_restart = (
                        claim["retirement_reason"]
                        == "external_supervisor_restart_normalization"
                    )
                    restart = None
                    if not administrative_restart:
                        restart = self.connection.execute(
                            """
                            SELECT * FROM coordinator_restart_requests
                            WHERE lineage_id = ? AND acknowledged_at IS NOT NULL
                              AND requested_at >= ?
                            ORDER BY generation DESC LIMIT 1
                            """,
                            (lineage_id, claim["retired_at"]),
                        ).fetchone()
                        if restart is None:
                            raise DeadlineError(
                                "A retired mutation clock requires its acknowledged fresh "
                                "coordinator before a new deadline can be armed"
                            )
                    next_generation = int(claim["deadline_generation"]) + 1
                    self.connection.execute(
                        """
                        INSERT INTO claim_deadline_generations (
                            lineage_id, claim_id, generation, estimate_seconds,
                            started_at, deadline_at, armed_by_restart_generation
                        ) VALUES (?, ?, ?, ?, ?, ?, ?)
                        """,
                        (
                            lineage_id,
                            claim_id,
                            next_generation,
                            estimate,
                            started_at,
                            started_at + estimate,
                            restart["generation"] if restart is not None else None,
                        ),
                    )
                    claim = self._claim(lineage_id, claim_id)
                    advanced_deadline_generation = True
                else:
                    incident = self._claim_deadline_incident(
                        lineage_id, claim_id, int(claim["deadline_generation"])
                    )
                if (
                    not advanced_deadline_generation
                    and incident is not None
                    and not self._deadline_mutation_pending(
                    lineage_id, claim_id
                    )
                ):
                    restart_generation = incident["restart_generation"]
                    restart = (
                        self.connection.execute(
                            """
                            SELECT * FROM coordinator_restart_requests
                            WHERE lineage_id = ? AND generation = ?
                            """,
                            (lineage_id, restart_generation),
                        ).fetchone()
                        if restart_generation is not None
                        else None
                    )
                    if restart is None or restart["acknowledged_at"] is None:
                        raise DeadlineError(
                            "Resolved deadline generation requires its acknowledged "
                            "successor before a new work deadline can be armed"
                        )
                    next_generation = int(claim["deadline_generation"]) + 1
                    self.connection.execute(
                        """
                        INSERT INTO claim_deadline_generations (
                            lineage_id, claim_id, generation, estimate_seconds,
                            started_at, deadline_at, armed_by_restart_generation
                        ) VALUES (?, ?, ?, ?, ?, ?, ?)
                        """,
                        (
                            lineage_id,
                            claim_id,
                            next_generation,
                            estimate,
                            started_at,
                            started_at + estimate,
                            restart_generation,
                        ),
                    )
                    claim = self._claim(lineage_id, claim_id)
                    advanced_deadline_generation = True
            if claim is None:
                dispatch_phase = "exploration" if phase is None else phase
                if dispatch_phase != "exploration":
                    raise DeadlineError(
                        "A new claim must begin in exploration before closure"
                    )
                self.connection.execute(
                    """
                    INSERT INTO claim_clocks (
                        lineage_id, claim_id, estimate_seconds, started_at,
                        deadline_at, phase
                    ) VALUES (?, ?, ?, ?, ?, 'exploration')
                    """,
                    (
                        lineage_id,
                        claim_id,
                        estimate,
                        started_at,
                        started_at + estimate,
                    ),
                )
                self.connection.execute(
                    """
                    INSERT INTO claim_deadline_generations (
                        lineage_id, claim_id, generation, estimate_seconds,
                        started_at, deadline_at
                    ) VALUES (?, ?, 1, ?, ?, ?)
                    """,
                    (lineage_id, claim_id, estimate, started_at, started_at + estimate),
                )
                self.connection.execute(
                    """
                    INSERT INTO claim_phase_events (
                        lineage_id, claim_id, sequence, phase,
                        recorded_at, basis_task_id
                    ) VALUES (?, ?, 1, 'exploration', ?, ?)
                    """,
                    (lineage_id, claim_id, started_at, task_id),
                )
                claim = self._claim(lineage_id, claim_id)
                claim_created = True
            else:
                claim_created = False
                if not advanced_deadline_generation and claim["estimate_seconds"] != estimate:
                    raise DeadlineError(
                        "Repeated claim dispatch cannot change the claim estimate or deadline"
                    )
                if (
                    existing is None
                    and attempt_estimate_seconds is not None
                    and started_at + attempt_estimate > float(claim["deadline_at"])
                ):
                    remaining_seconds = max(
                        0.0, float(claim["deadline_at"]) - started_at
                    )
                    raise DeadlineError(
                        "Attempt estimate exceeds the remaining claim deadline: "
                        f"estimate={attempt_estimate:g}s "
                        f"remaining={remaining_seconds:g}s"
                    )
                if phase is not None and phase != claim["phase"]:
                    raise DeadlineError(
                        "Attempt phase must match the persisted claim phase transition"
                    )
            dispatch_phase = str(claim["phase"])
            phase_event = self.connection.execute(
                """
                SELECT sequence FROM claim_phase_events
                WHERE lineage_id = ? AND claim_id = ?
                ORDER BY sequence DESC LIMIT 1
                """,
                (lineage_id, claim_id),
            ).fetchone()
            if phase_event is None:
                raise DeadlineError("Claim has no phase event")
            phase_sequence = int(phase_event["sequence"])
            selected_gap_id: str | None = None
            selected_gap_revision: int | None = None
            if dispatch_phase == "exploration":
                if gap_id is not None:
                    raise DeadlineError("Exploration attempts cannot bind a closure gap")
            else:
                valid_acceptance = self._latest_valid_acceptance(
                    lineage_id, claim_id
                )
                if existing is not None:
                    if existing["closure_gap_id"] is None:
                        if gap_id is not None:
                            raise DeadlineError(
                                "A legacy repeated start cannot add a closure gap binding"
                            )
                    else:
                        if gap_id is not None and gap_id != existing["closure_gap_id"]:
                            raise DeadlineError("Repeated start cannot change closure gap")
                        selected_gap_id = str(existing["closure_gap_id"])
                else:
                    candidates = self._closure_gaps(
                        lineage_id,
                        claim_id,
                        phase_sequence,
                        include_closed=valid_acceptance is not None,
                    )
                    if gap_id is not None:
                        candidates = [
                            candidate
                            for candidate in candidates
                            if candidate["gap_id"] == gap_id
                        ]
                        if not candidates:
                            raise DeadlineError(
                                "Closure gap is unknown, closed, or outside the active epoch"
                            )
                    if len(candidates) != 1:
                        raise DeadlineError(
                            "Closure attempts must name exactly one active gap"
                        )
                    selected_gap_id = str(candidates[0]["gap_id"])
                if selected_gap_id is not None:
                    revision = self._latest_gap_revision(
                        lineage_id,
                        claim_id,
                        phase_sequence,
                        selected_gap_id,
                    )
                    if revision["route_kind"] == "owner_wait" and existing is None:
                        raise DeadlineError("Closure gap awaits an explicit owner decision; no worker route is authorized")
                    selected_gap_revision = int(revision["revision"])
                if existing is None:
                    prior = self.connection.execute(
                        """
                        SELECT task_id, attempt_terminal_kind
                        FROM tasks
                        WHERE lineage_id = ? AND claim_id = ?
                          AND phase_sequence_at_dispatch = ?
                          AND closure_gap_id = ?
                          AND closure_gap_revision = ?
                        ORDER BY started_at DESC, task_id DESC LIMIT 1
                        """,
                        (
                            lineage_id,
                            claim_id,
                            phase_sequence,
                            selected_gap_id,
                            selected_gap_revision,
                        ),
                    ).fetchone()
                    if prior is not None:
                        if prior["attempt_terminal_kind"] is None:
                            raise DeadlineError(
                                "Closure gap revision already has a live attempt; "
                                "abandon it before dispatching a replacement"
                            )
            supervisor_epoch = self.connection.execute(
                "SELECT COALESCE(MAX(generation), 0) FROM external_supervisor_epochs "
                "WHERE lineage_id = ?",
                (lineage_id,),
            ).fetchone()[0]
            cursor = self.connection.execute(
                """
                INSERT OR IGNORE INTO tasks (
                    lineage_id, task_id, claim_id,
                    estimate_seconds, started_at, deadline_at,
                    deadline_generation,
                    phase_at_dispatch, phase_sequence_at_dispatch,
                    closure_gap_id, closure_gap_revision,
                    supervisor_epoch_generation
                ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                """,
                (
                    lineage_id,
                    task_id,
                    claim_id,
                    attempt_estimate,
                    started_at,
                    claim["deadline_at"],
                    claim["deadline_generation"],
                    dispatch_phase,
                    phase_sequence,
                    selected_gap_id,
                    selected_gap_revision,
                    supervisor_epoch,
                ),
            )
            created = cursor.rowcount == 1
            task = self._task(lineage_id, task_id)
            if not created and task["claim_id"] != claim_id:
                raise DeadlineError("Repeated start cannot change claim id")
            if not created and task["estimate_seconds"] != claim["estimate_seconds"]:
                raise DeadlineError("Repeated start cannot change estimate")
            if not created and task["phase_at_dispatch"] != dispatch_phase:
                raise DeadlineError("Repeated start cannot change dispatch phase")
            if (
                not created
                and task["phase_sequence_at_dispatch"] != phase_sequence
            ):
                raise DeadlineError("Repeated start cannot change dispatch phase epoch")
            if not created and task["closure_gap_id"] != selected_gap_id:
                raise DeadlineError("Repeated start cannot change closure gap")
            result = self._status(lineage_id, task_id, started_at)
            result["created"] = created
            result["attempt_created"] = created
            result["claim_created"] = claim_created
            result["phase"] = dispatch_phase
            result["random_mutation"] = self._random_mutation_status(lineage_id)
            result["coordinator_restart"] = self._coordinator_restart_status(
                lineage_id
            )
            self.connection.commit()
            return result
        except Exception:
            self.connection.rollback()
            raise

    def status_task(
        self, lineage_id: str, task_id: str, *, now: float | None = None
    ) -> dict[str, Any]:
        lineage_id = self._identity(lineage_id, "Lineage id")
        task_id = self._identity(task_id, "Task id")
        checked_at = self._now(now)
        self._begin()
        try:
            task = self._task(lineage_id, task_id)
            self._record_miss_if_due(task, checked_at)
            result = self._status(lineage_id, task_id, checked_at)
            result["random_mutation"] = self._random_mutation_status(lineage_id)
            self.connection.commit()
            return result
        except Exception:
            self.connection.rollback()
            raise

    def list_tasks(self, *, now: float | None = None) -> dict[str, Any]:
        checked_at = self._now(now)
        self._begin()
        try:
            binding = self.connection.execute(
                "SELECT lineage_id FROM lineage_binding WHERE singleton = 1"
            ).fetchone()
            if binding is None:
                raise DeadlineError("State database has not been bound to a lineage")
            lineage_id = binding["lineage_id"]
            task_ids = self.connection.execute(
                """
                SELECT current.task_id
                FROM tasks AS current
                LEFT JOIN claim_clocks AS active_clock
                  ON active_clock.lineage_id = current.lineage_id
                 AND active_clock.claim_id = current.claim_id
                WHERE current.lineage_id = ?
                  AND (
                    active_clock.claim_id IS NULL
                    OR current.phase_at_dispatch = active_clock.phase
                  )
                  AND NOT EXISTS (
                    SELECT 1
                    FROM tasks AS newer
                    WHERE newer.lineage_id = current.lineage_id
                      AND newer.claim_id = current.claim_id
                      AND (
                        newer.started_at > current.started_at
                        OR (
                          newer.started_at = current.started_at
                          AND newer.task_id > current.task_id
                        )
                      )
                  )
                ORDER BY current.started_at, current.task_id
                """,
                (lineage_id,),
            ).fetchall()
            for row in task_ids:
                task = self._task(lineage_id, row["task_id"])
                if self.connection.execute(
                    """
                    SELECT 1 FROM claim_clocks
                    WHERE lineage_id = ? AND claim_id = ?
                    """,
                    (lineage_id, task["claim_id"]),
                ).fetchone() is not None:
                    self._record_miss_if_due(task, checked_at)
            task_ids = self.connection.execute(
                """
                SELECT current.task_id
                FROM tasks AS current
                LEFT JOIN claim_clocks AS active_clock
                  ON active_clock.lineage_id = current.lineage_id
                 AND active_clock.claim_id = current.claim_id
                WHERE current.lineage_id = ?
                  AND (
                    active_clock.claim_id IS NULL
                    OR current.phase_at_dispatch = active_clock.phase
                  )
                  AND NOT EXISTS (
                    SELECT 1 FROM claim_acceptances AS accepted
                    WHERE accepted.lineage_id = current.lineage_id
                      AND accepted.claim_id = current.claim_id
                      AND accepted.invalidated_at IS NULL
                      AND current.attempt_terminal_at IS NOT NULL
                      AND current.attempt_terminal_at <= accepted.accepted_at
                  )
                  AND (
                    current.attempt_terminal_at IS NULL
                    OR NOT EXISTS (
                      SELECT 1 FROM tasks AS newer
                      WHERE newer.lineage_id = current.lineage_id
                        AND newer.claim_id = current.claim_id
                        AND (
                          (
                            current.closure_gap_id IS NULL
                            AND newer.closure_gap_id IS NULL
                          )
                          OR (
                            current.closure_gap_id IS NOT NULL
                            AND newer.phase_sequence_at_dispatch =
                                current.phase_sequence_at_dispatch
                            AND newer.closure_gap_id = current.closure_gap_id
                            AND newer.closure_gap_revision =
                                current.closure_gap_revision
                          )
                        )
                        AND (
                          newer.started_at > current.started_at
                          OR (newer.started_at = current.started_at AND newer.task_id > current.task_id)
                        )
                    )
                  )
                  AND (
                    current.attempt_terminal_at IS NULL
                    OR current.closure_gap_id IS NULL
                    OR current.integrity_breached_at IS NOT NULL
                    OR EXISTS (
                      SELECT 1 FROM closure_gaps AS gap
                      WHERE gap.lineage_id = current.lineage_id
                        AND gap.claim_id = current.claim_id
                        AND gap.closure_sequence =
                            current.phase_sequence_at_dispatch
                        AND gap.gap_id = current.closure_gap_id
                        AND gap.closed_at IS NULL
                        AND current.closure_gap_revision = (
                          SELECT MAX(revision.revision)
                          FROM closure_gap_revisions AS revision
                          WHERE revision.lineage_id = gap.lineage_id
                            AND revision.claim_id = gap.claim_id
                            AND revision.closure_sequence = gap.closure_sequence
                            AND revision.gap_id = gap.gap_id
                        )
                    )
                    OR EXISTS (
                      SELECT 1 FROM claim_acceptances AS accepted
                      WHERE accepted.lineage_id = current.lineage_id
                        AND accepted.claim_id = current.claim_id
                        AND accepted.invalidated_at IS NULL
                        AND current.attempt_terminal_at > accepted.accepted_at
                    )
                  )
                ORDER BY current.started_at, current.task_id
                """,
                (lineage_id,),
            ).fetchall()
            tasks = [
                self._compact_task_result(
                    self._status(lineage_id, row["task_id"], checked_at)
                )
                for row in task_ids
            ]
            result = {
                "lineage_id": lineage_id,
                "checked_at": checked_at,
                "cumulative_miss_units": self._cumulative_units(lineage_id),
                "random_mutation": self._random_mutation_status(lineage_id),
                "coordinator_restart": self._coordinator_restart_status(lineage_id),
                "tasks": tasks,
                "pending_deadline_mutations": self._pending_deadline_mutations(
                    lineage_id
                ),
                "pending_integrity_mutations": self._pending_integrity_mutations(
                    lineage_id
                ),
                "claim_clock_migration_conflicts": self._pending_migration_conflicts(
                    lineage_id
                ),
                "reopened_unaccepted_claims": self._reopened_unaccepted_claims(
                    lineage_id
                ),
                "invalidated_unaccepted_claims": self._invalidated_unaccepted_claims(
                    lineage_id
                ),
                "closure_gaps": self._active_gap_summaries(lineage_id),
                "pending_incident_reviews": self._pending_incident_reviews(
                    lineage_id
                ),
                "recent_failure_verdicts": self._recent_failure_verdicts(lineage_id),
            }
            self.connection.commit()
            return result
        except Exception:
            self.connection.rollback()
            raise

    def coordinator_view(
        self,
        *,
        include_recent_verdicts: bool = False,
        now: float | None = None,
    ) -> dict[str, Any]:
        """Return only live work and gates, with optional short startup memory."""
        summary = self.list_tasks(now=now)
        result = {
            "lineage_id": summary["lineage_id"],
            "tasks": summary["tasks"],
            "pending_incident_reviews": [
                {
                    "task_id": review["task_id"],
                    "claim_id": review["claim_id"],
                    "kind": review["kind"],
                }
                for review in summary["pending_incident_reviews"]
            ],
            "pending_deadline_mutations": summary["pending_deadline_mutations"],
            "pending_integrity_mutations": summary[
                "pending_integrity_mutations"
            ],
            "claim_clock_migration_conflicts": [
                {
                    "claim_id": conflict["claim_id"],
                    "detected_at": conflict["detected_at"],
                    "legacy_clock_option_count": len(
                        conflict["legacy_clock_options"]
                    ),
                }
                for conflict in summary["claim_clock_migration_conflicts"]
            ],
            "reopened_unaccepted_claims": summary["reopened_unaccepted_claims"],
            "invalidated_unaccepted_claims": summary[
                "invalidated_unaccepted_claims"
            ],
            "closure_gaps": summary["closure_gaps"],
            "random_mutation": self._pending_random_gate(
                summary["random_mutation"]
            ),
            "coordinator_restart": self._pending_restart_gate(
                summary["coordinator_restart"]
            ),
        }
        if include_recent_verdicts:
            result["recent_failure_verdicts"] = [
                {
                    "task_id": verdict["task_id"],
                    "claim_id": verdict["claim_id"],
                    "kind": verdict["kind"],
                    "short_verdict": verdict["short_verdict"],
                }
                for verdict in summary["recent_failure_verdicts"]
            ]
        return result

    def expire_task(
        self, lineage_id: str, task_id: str, *, now: float | None = None
    ) -> dict[str, Any]:
        lineage_id = self._identity(lineage_id, "Lineage id")
        task_id = self._identity(task_id, "Task id")
        checked_at = self._now(now)
        self._begin()
        try:
            task = self._task(lineage_id, task_id)
            incident = self._record_miss_if_due(task, checked_at)
            result = {
                "incident": incident,
                "status": self._status(lineage_id, task_id, checked_at),
                "random_mutation": self._random_mutation_status(lineage_id),
            }
            self.connection.commit()
            return result
        except Exception:
            self.connection.rollback()
            raise

    def complete_task(
        self,
        lineage_id: str,
        task_id: str,
        evidence: str,
        *,
        receipt_id: str | None = None,
        now: float | None = None,
    ) -> dict[str, Any]:
        lineage_id = self._identity(lineage_id, "Lineage id")
        task_id = self._identity(task_id, "Task id")
        evidence = self._nonempty_text(evidence, "Completion evidence")
        completed_at = self._now(now)
        self._begin()
        try:
            task = self._task(lineage_id, task_id)
            self._require_terminal_worker_receipt(
                lineage_id, task_id, receipt_id, "completed"
            )
            if task["integrity_breached_at"] is not None:
                raise DeadlineError("An integrity breach invalidates task completion")
            if self._worker_finding(lineage_id, task_id) is not None:
                raise DeadlineError(
                    "A worker finding terminates this task without accepting completion"
                )
            if task["attempt_terminal_at"] is None:
                self._record_miss_if_due(task, completed_at)
                self.connection.execute(
                    """
                    UPDATE tasks
                    SET completed_at = ?, completion_evidence = ?
                    WHERE lineage_id = ? AND task_id = ?
                    """,
                    (completed_at, evidence, lineage_id, task_id),
                )
                self._record_terminal_window(task, completed_at, "completed")
            elif task["attempt_terminal_kind"] != "completed":
                raise DeadlineError("Attempt already has another terminal outcome")
            result = self._status(lineage_id, task_id, completed_at)
            result["random_mutation"] = self._random_mutation_status(lineage_id)
            self.connection.commit()
            return result
        except Exception:
            self.connection.rollback()
            raise

    def expire_claim(
        self, lineage_id: str, claim_id: str, *, now: float | None = None
    ) -> dict[str, Any]:
        lineage_id = self._identity(lineage_id, "Lineage id")
        claim_id = self._identity(claim_id, "Claim id")
        checked_at = self._now(now)
        self._begin()
        try:
            self._claim(lineage_id, claim_id)
            task = self.connection.execute(
                """
                SELECT * FROM tasks
                WHERE lineage_id = ? AND claim_id = ?
                ORDER BY started_at DESC, task_id DESC
                LIMIT 1
                """,
                (lineage_id, claim_id),
            ).fetchone()
            if task is None:
                raise DeadlineError("Claim has no worker attempt to anchor its incident")
            incident = self._record_miss_if_due(task, checked_at)
            result = {
                "incident": incident,
                "claim_id": claim_id,
                "checked_at": checked_at,
                "deadline_at": self._claim(lineage_id, claim_id)["deadline_at"],
                "mutation_pending": self._deadline_mutation_pending(
                    lineage_id, claim_id
                ),
                "random_mutation": self._random_mutation_status(lineage_id),
            }
            self.connection.commit()
            return result
        except Exception:
            self.connection.rollback()
            raise

    def miss_claim_deadline(
        self, lineage_id: str, claim_id: str, *, now: float | None = None
    ) -> dict[str, Any]:
        """Record an actual expired item deadline through the ordinary miss route."""

        lineage_id = self._identity(lineage_id, "Lineage id")
        claim_id = self._identity(claim_id, "Claim id")
        recorded_at = self._now(now)
        self._begin()
        try:
            claim = self._claim(lineage_id, claim_id)
            task = self.connection.execute(
                """
                SELECT * FROM tasks
                WHERE lineage_id = ? AND claim_id = ?
                  AND deadline_generation = ?
                ORDER BY started_at DESC, task_id DESC
                LIMIT 1
                """,
                (lineage_id, claim_id, claim["deadline_generation"]),
            ).fetchone()
            if task is None:
                raise DeadlineError(
                    "Claim has no current-generation worker attempt to anchor its miss"
                )
            accepted = self._latest_valid_acceptance(lineage_id, claim_id)
            if accepted is not None and accepted["accepted_at"] < claim["deadline_at"]:
                raise DeadlineError("An accepted claim cannot miss its deadline")
            if recorded_at < float(claim["deadline_at"]):
                raise DeadlineError(
                    "The item deadline has not expired; continue the ledger item"
                )
            incident = self._record_miss_if_due(task, recorded_at)
            result = {
                "incident": incident,
                "claim_id": claim_id,
                "recorded_at": recorded_at,
                "deadline_at": claim["deadline_at"],
                "mutation_pending": self._deadline_mutation_pending(
                    lineage_id, claim_id
                ),
                "random_mutation": self._random_mutation_status(lineage_id),
            }
            self.connection.commit()
            return result
        except Exception:
            self.connection.rollback()
            raise

    def transition_claim_to_closure(
        self,
        lineage_id: str,
        claim_id: str,
        basis_task_id: str,
        closure_outcome: str,
        closure_evidence: str,
        closure_remaining_gap: str | None = None,
        *,
        gaps: list[tuple[str, str, str]] | None = None,
        now: float | None = None,
    ) -> dict[str, Any]:
        lineage_id = self._identity(lineage_id, "Lineage id")
        claim_id = self._identity(claim_id, "Claim id")
        basis_task_id = self._identity(basis_task_id, "Basis task id")
        closure_outcome = self._nonempty_text(closure_outcome, "Closure outcome")
        closure_evidence = self._nonempty_text(
            closure_evidence, "Closure evidence route"
        )
        if gaps is not None and closure_remaining_gap is not None:
            raise DeadlineError("Use either one legacy remaining gap or named gaps")
        gap_specs: list[tuple[str, str, str]] = []
        if gaps is None:
            closure_remaining_gap = self._nonempty_text(
                closure_remaining_gap, "Closure remaining gap"
            )
            gap_specs.append(("G-001", closure_remaining_gap, closure_evidence))
        else:
            if not gaps:
                raise DeadlineError("Closure requires at least one named gap")
            seen_gap_ids: set[str] = set()
            for raw_gap_id, raw_description, raw_proof_route in gaps:
                normalized_gap_id = self._identity(raw_gap_id, "Closure gap id")
                if normalized_gap_id in seen_gap_ids:
                    raise DeadlineError("Closure gap ids must be unique")
                seen_gap_ids.add(normalized_gap_id)
                gap_specs.append(
                    (
                        normalized_gap_id,
                        self._nonempty_text(
                            raw_description, "Closure gap description"
                        ),
                        self._nonempty_text(
                            raw_proof_route, "Closure gap proof route"
                        ),
                    )
                )
            closure_remaining_gap = "; ".join(
                f"{gap_id}: {description}"
                for gap_id, description, _ in gap_specs
            )
        recorded_at = self._now(now)
        self._begin()
        try:
            claim = self._claim(lineage_id, claim_id)
            task = self._task(lineage_id, basis_task_id)
            if task["claim_id"] != claim_id:
                raise DeadlineError("Closure basis task belongs to another claim")
            if task["attempt_terminal_kind"] != "completed":
                raise DeadlineError(
                    "Closure needs a successfully completed exploration attempt"
                )
            if task["phase_at_dispatch"] != "exploration":
                raise DeadlineError("Closure basis must be an exploration attempt")
            if task["integrity_breached_at"] is not None:
                raise DeadlineError("An integrity breach cannot establish closure")
            if task["completion_evidence"] is None or not str(
                task["completion_evidence"]
            ).strip():
                raise DeadlineError("Closure basis has no completed exploration evidence")
            if claim["phase"] != "exploration":
                raise DeadlineError("Claim is not in exploration")
            active_phase = self.connection.execute(
                """
                SELECT * FROM claim_phase_events
                WHERE lineage_id = ? AND claim_id = ?
                ORDER BY sequence DESC LIMIT 1
                """,
                (lineage_id, claim_id),
            ).fetchone()
            if (
                active_phase is None
                or active_phase["phase"] != "exploration"
                or task["phase_sequence_at_dispatch"] != active_phase["sequence"]
            ):
                raise DeadlineError(
                    "Closure basis must belong to the current exploration epoch"
                )
            self._record_miss_if_due(task, recorded_at)
            sequence = self.connection.execute(
                """
                SELECT COALESCE(MAX(sequence), 0) + 1 AS next_sequence
                FROM claim_phase_events
                WHERE lineage_id = ? AND claim_id = ?
                """,
                (lineage_id, claim_id),
            ).fetchone()["next_sequence"]
            self.connection.execute(
                """
                INSERT INTO claim_phase_events (
                    lineage_id, claim_id, sequence, phase, recorded_at,
                    basis_task_id, closure_outcome, closure_evidence,
                    closure_remaining_gap
                ) VALUES (?, ?, ?, 'closure', ?, ?, ?, ?, ?)
                """,
                (
                    lineage_id,
                    claim_id,
                    sequence,
                    recorded_at,
                    basis_task_id,
                    closure_outcome,
                    closure_evidence,
                    closure_remaining_gap,
                ),
            )
            self.connection.execute(
                """
                UPDATE claim_clocks SET phase = 'closure'
                WHERE lineage_id = ? AND claim_id = ?
                """,
                (lineage_id, claim_id),
            )
            for gap_id, description, proof_route in gap_specs:
                self.connection.execute(
                    """
                    INSERT INTO closure_gaps (
                        lineage_id, claim_id, closure_sequence, gap_id,
                        opened_at, basis_task_id
                    ) VALUES (?, ?, ?, ?, ?, ?)
                    """,
                    (
                        lineage_id,
                        claim_id,
                        sequence,
                        gap_id,
                        recorded_at,
                        basis_task_id,
                    ),
                )
                self.connection.execute(
                    """
                    INSERT INTO closure_gap_revisions (
                        lineage_id, claim_id, closure_sequence, gap_id,
                        revision, recorded_at, basis_task_id,
                        description, proof_route
                    ) VALUES (?, ?, ?, ?, 1, ?, ?, ?, ?)
                    """,
                    (
                        lineage_id,
                        claim_id,
                        sequence,
                        gap_id,
                        recorded_at,
                        basis_task_id,
                        description,
                        proof_route,
                    ),
                )
            result = dict(self._claim(lineage_id, claim_id))
            result.update(
                {
                    "basis_task_id": basis_task_id,
                    "closure_outcome": closure_outcome,
                    "closure_evidence": closure_evidence,
                    "closure_remaining_gap": closure_remaining_gap,
                    "closure_gaps": [
                        {
                            "gap_id": gap_id,
                            "description": description,
                            "proof_route": proof_route,
                            "revision": 1,
                        }
                        for gap_id, description, proof_route in gap_specs
                    ],
                }
            )
            self.connection.commit()
            return result
        except Exception:
            self.connection.rollback()
            raise

    def reopen_claim_for_owner(
        self,
        lineage_id: str,
        claim_id: str,
        owner_request: str,
        *,
        now: float | None = None,
    ) -> dict[str, Any]:
        """Reassess accepted work during an owner-authorized exclusive review.

        The caller supplies the owner's decision and its durable source reference.
        The accepted task remains evidence, not a fabricated worker finding. This
        does not change deadlines, task history or another claim's acceptance.
        """
        lineage_id = self._identity(lineage_id, "Lineage id")
        claim_id = self._identity(claim_id, "Claim id")
        owner_request = self._nonempty_text(owner_request, "Owner request and source")
        reason = f"owner requested reassessment: {owner_request}"
        recorded_at = self._now(now)
        self._begin()
        try:
            claim = self._claim(lineage_id, claim_id)
            acceptance = self._latest_acceptance(lineage_id, claim_id)
            if acceptance is None:
                raise DeadlineError("Owner reassessment requires an accepted claim")
            if claim["phase"] == "exploration" and acceptance["invalidation_reason"] == reason:
                result = dict(claim)
                result["recorded"] = False
                self.connection.commit()
                result["dfs_status_synchronized"] = list(self.synchronize_dfs_statuses())
                return result
            if claim["phase"] != "closure" or acceptance["invalidated_at"] is not None:
                raise DeadlineError("Owner reassessment requires a currently accepted closure")
            if claim["retired_at"] is None:
                raise DeadlineError("Owner reassessment requires exclusive mutation clock retirement")
            live = self.connection.execute(
                "SELECT 1 FROM worker_claims WHERE lineage_id = ? AND released_at IS NULL LIMIT 1",
                (lineage_id,),
            ).fetchone()
            if live is not None:
                raise DeadlineError("Owner reassessment requires quiet worker ownership")
            sequence = self.connection.execute(
                "SELECT COALESCE(MAX(sequence), 0) + 1 FROM claim_phase_events "
                "WHERE lineage_id = ? AND claim_id = ?", (lineage_id, claim_id),
            ).fetchone()[0]
            self.connection.execute(
                "INSERT INTO claim_phase_events "
                "(lineage_id, claim_id, sequence, phase, recorded_at, basis_task_id, closure_evidence) "
                "VALUES (?, ?, ?, 'exploration', ?, ?, ?)",
                (lineage_id, claim_id, sequence, recorded_at, acceptance["task_id"], owner_request),
            )
            self.connection.execute(
                "UPDATE claim_clocks SET phase = 'exploration' WHERE lineage_id = ? AND claim_id = ?",
                (lineage_id, claim_id),
            )
            self.connection.execute(
                "UPDATE claim_acceptances SET invalidated_at = ?, invalidation_reason = ? "
                "WHERE lineage_id = ? AND claim_id = ? AND invalidated_at IS NULL",
                (recorded_at, reason, lineage_id, claim_id),
            )
            result = dict(self._claim(lineage_id, claim_id))
            result.update(recorded=True, owner_request=owner_request)
            self.synchronize_dfs_statuses(persist=False)
            self.connection.commit()
            result["dfs_status_synchronized"] = list(self.synchronize_dfs_statuses())
            return result
        except Exception:
            self.connection.rollback()
            raise

    def reopen_claim_exploration(
        self,
        lineage_id: str,
        claim_id: str,
        basis_task_id: str,
        contradicted_premise: str,
        *,
        now: float | None = None,
    ) -> dict[str, Any]:
        lineage_id = self._identity(lineage_id, "Lineage id")
        claim_id = self._identity(claim_id, "Claim id")
        basis_task_id = self._identity(basis_task_id, "Basis task id")
        contradicted_premise = self._nonempty_text(
            contradicted_premise, "Contradicted closure premise"
        )
        recorded_at = self._now(now)
        self._begin()
        try:
            claim = self._claim(lineage_id, claim_id)
            task = self._task(lineage_id, basis_task_id)
            finding = self._worker_finding(lineage_id, basis_task_id)
            if task["claim_id"] != claim_id:
                raise DeadlineError("Reopen basis task belongs to another claim")
            if claim["phase"] != "closure":
                raise DeadlineError("Only a closure claim can reopen exploration")
            if task["phase_at_dispatch"] != "closure" or finding is None:
                raise DeadlineError(
                    "Reopening exploration needs a recorded closure finding"
                )
            if contradicted_premise not in str(finding["evidence"]):
                raise DeadlineError(
                    "The named contradicted premise must appear in the finding evidence"
                )
            closure = self.connection.execute(
                """
                SELECT * FROM claim_phase_events
                WHERE lineage_id = ? AND claim_id = ? AND phase = 'closure'
                ORDER BY sequence DESC LIMIT 1
                """,
                (lineage_id, claim_id),
            ).fetchone()
            premise_is_current = closure is not None and any(
                contradicted_premise in str(closure[field] or "")
                for field in (
                    "closure_outcome",
                    "closure_evidence",
                    "closure_remaining_gap",
                )
            )
            if closure is not None and task["closure_gap_id"] is not None:
                revision = self._latest_gap_revision(
                    lineage_id,
                    claim_id,
                    int(closure["sequence"]),
                    str(task["closure_gap_id"]),
                )
                premise_is_current = premise_is_current or any(
                    contradicted_premise in str(revision[field])
                    for field in ("description", "proof_route")
                )
            if closure is None or not premise_is_current:
                raise DeadlineError(
                    "The named premise must belong to the active closure contract"
                )
            if task["phase_sequence_at_dispatch"] != closure["sequence"]:
                raise DeadlineError(
                    "Reopen finding must belong to the active closure epoch"
                )
            self._record_miss_if_due(task, recorded_at)
            sequence = self.connection.execute(
                """
                SELECT COALESCE(MAX(sequence), 0) + 1 AS next_sequence
                FROM claim_phase_events
                WHERE lineage_id = ? AND claim_id = ?
                """,
                (lineage_id, claim_id),
            ).fetchone()["next_sequence"]
            self.connection.execute(
                """
                INSERT INTO claim_phase_events (
                    lineage_id, claim_id, sequence, phase, recorded_at,
                    basis_task_id, contradicted_premise
                ) VALUES (?, ?, ?, 'exploration', ?, ?, ?)
                """,
                (
                    lineage_id,
                    claim_id,
                    sequence,
                    recorded_at,
                    basis_task_id,
                    contradicted_premise,
                ),
            )
            self.connection.execute(
                """
                UPDATE claim_clocks SET phase = 'exploration'
                WHERE lineage_id = ? AND claim_id = ?
                """,
                (lineage_id, claim_id),
            )
            self.connection.execute(
                """
                UPDATE claim_acceptances
                SET invalidated_at = ?, invalidation_reason = ?
                WHERE lineage_id = ? AND claim_id = ?
                  AND invalidated_at IS NULL
                """,
                (
                    recorded_at,
                    f"closure premise falsified: {contradicted_premise}",
                    lineage_id,
                    claim_id,
                ),
            )
            result = dict(self._claim(lineage_id, claim_id))
            result["basis_task_id"] = basis_task_id
            result["contradicted_premise"] = contradicted_premise
            self.synchronize_dfs_statuses(persist=False)
            self.connection.commit()
            result["dfs_status_synchronized"] = list(
                self.synchronize_dfs_statuses()
            )
            return result
        except Exception:
            self.connection.rollback()
            raise

    def abandon_attempt(
        self,
        lineage_id: str,
        task_id: str,
        reason: str,
        *,
        receipt_id: str | None = None,
        now: float | None = None,
    ) -> dict[str, Any]:
        lineage_id = self._identity(lineage_id, "Lineage id")
        task_id = self._identity(task_id, "Task id")
        reason = self._nonempty_text(reason, "Abandonment reason")
        abandoned_at = self._now(now)
        self._begin()
        try:
            task = self._task(lineage_id, task_id)
            self._require_terminal_worker_receipt(
                lineage_id, task_id, receipt_id, "abandoned"
            )
            if task["attempt_terminal_at"] is None:
                self._record_miss_if_due(task, abandoned_at)
                self.connection.execute(
                    """
                    UPDATE tasks SET abandoned_at = ?, abandonment_reason = ?
                    WHERE lineage_id = ? AND task_id = ?
                    """,
                    (abandoned_at, reason, lineage_id, task_id),
                )
                self._record_terminal_window(task, abandoned_at, "abandoned")
                recorded = True
            elif (
                task["attempt_terminal_kind"] == "abandoned"
                and task["abandonment_reason"] == reason
            ):
                recorded = False
            else:
                raise DeadlineError("Attempt already has another terminal outcome")
            result = self._status(lineage_id, task_id, abandoned_at)
            result["recorded"] = recorded
            result["random_mutation"] = self._random_mutation_status(lineage_id)
            self.connection.commit()
            return result
        except Exception:
            self.connection.rollback()
            raise

    def normalize_external_supervisor_start(
        self,
        lineage_id: str,
        *,
        now: float | None = None,
    ) -> dict[str, Any]:
        """Remove prior-epoch runtime ownership before one explicit start."""

        lineage_id = self._identity(lineage_id, "Lineage id")
        normalized_at = self._now(now)
        reason = "external_supervisor_restart_normalization"
        self._begin()
        try:
            self._bind_lineage(lineage_id)
            epoch = self.connection.execute(
                "SELECT COALESCE(MAX(generation), 0) + 1 FROM external_supervisor_epochs "
                "WHERE lineage_id = ?",
                (lineage_id,),
            ).fetchone()[0]
            self.connection.execute(
                "INSERT INTO external_supervisor_epochs "
                "(lineage_id, generation, normalized_at) VALUES (?, ?, ?)",
                (lineage_id, epoch, normalized_at),
            )
            active = self.connection.execute(
                """
                SELECT * FROM tasks
                WHERE lineage_id = ? AND attempt_terminal_at IS NULL
                ORDER BY started_at, task_id
                """,
                (lineage_id,),
            ).fetchall()
            for task in active:
                self.connection.execute(
                    """
                    UPDATE tasks
                    SET terminal_at = ?, attempt_terminal_at = ?,
                        attempt_terminal_kind = 'restart_normalized',
                        abandoned_at = ?, abandonment_reason = ?
                    WHERE lineage_id = ? AND task_id = ?
                    """,
                    (
                        normalized_at, normalized_at, normalized_at, reason,
                        lineage_id, task["task_id"],
                    ),
                )
                self.connection.execute(
                    """
                    UPDATE worker_claims
                    SET released_at = ?, release_reason = 'restart_normalized'
                    WHERE lineage_id = ? AND task_id = ? AND released_at IS NULL
                    """,
                    (normalized_at, lineage_id, task["task_id"]),
                )
            self.connection.execute(
                """
                UPDATE claim_deadline_generations
                SET retired_at = ?, retirement_reason = ?
                WHERE lineage_id = ? AND retired_at IS NULL
                  AND generation = (
                    SELECT MAX(latest.generation)
                    FROM claim_deadline_generations AS latest
                    WHERE latest.lineage_id = claim_deadline_generations.lineage_id
                      AND latest.claim_id = claim_deadline_generations.claim_id
                  )
                """,
                (normalized_at, reason, lineage_id),
            )
            released = self.connection.execute(
                """
                UPDATE coordinator_restart_requests
                SET claimed_at = NULL, expected_run_id = NULL
                WHERE lineage_id = ? AND acknowledged_at IS NULL
                  AND expected_run_id IS NOT NULL
                """,
                (lineage_id,),
            ).rowcount
            self.connection.commit()
            return {
                "abandoned_attempts": len(active),
                "released_restart_claim": released > 0,
            }
        except Exception:
            self.connection.rollback()
            raise

    def revise_closure_gap(
        self,
        lineage_id: str,
        claim_id: str,
        gap_id: str,
        basis_task_id: str,
        description: str,
        proof_route: str,
        *,
        now: float | None = None,
        route_kind: str = "executable",
        owner_decision: str | None = None,
    ) -> dict[str, Any]:
        """Append a changed causal contract after one terminal gap attempt."""

        lineage_id = self._identity(lineage_id, "Lineage id")
        claim_id = self._identity(claim_id, "Claim id")
        gap_id = self._identity(gap_id, "Closure gap id")
        basis_task_id = self._identity(basis_task_id, "Basis task id")
        description = self._nonempty_text(description, "Closure gap description")
        proof_route = self._nonempty_text(proof_route, "Closure gap proof route")
        if route_kind not in {"executable", "owner_wait"}:
            raise DeadlineError("Closure route kind must be executable or owner_wait")
        recorded_at = self._now(now)
        self._begin()
        try:
            claim = self._claim(lineage_id, claim_id)
            if claim["phase"] != "closure":
                raise DeadlineError("Only an active closure gap can be revised")
            closure = self._active_closure(lineage_id, claim_id)
            gap = self._closure_gap(
                lineage_id, claim_id, int(closure["sequence"]), gap_id
            )
            if gap["closed_at"] is not None:
                raise DeadlineError("A closed closure gap is immutable")
            task = self._task(lineage_id, basis_task_id)
            current = self._latest_gap_revision(
                lineage_id, claim_id, int(closure["sequence"]), gap_id
            )
            owner_release = (current["route_kind"] == "owner_wait"
                             and route_kind == "executable"
                             and bool(owner_decision and owner_decision.strip())
                             and basis_task_id == current["basis_task_id"])
            if current["route_kind"] == "owner_wait" and not owner_release:
                raise DeadlineError("Releasing an owner wait requires an explicit owner decision")
            if owner_decision is not None and not owner_release:
                raise DeadlineError("Owner decision may only release the current owner wait")
            if (
                task["claim_id"] != claim_id
                or task["phase_sequence_at_dispatch"] != closure["sequence"]
                or task["closure_gap_id"] != gap_id
                or (task["closure_gap_revision"] != current["revision"] and not owner_release)
                or task["attempt_terminal_kind"]
                not in {"finding", "completed", "integrity_breach"}
            ):
                raise DeadlineError(
                    "Gap revision needs a terminal attempt from its current revision"
                )
            normalized_description = " ".join(description.split()).casefold()
            normalized_proof_route = " ".join(proof_route.split()).casefold()
            if (
                route_kind == current["route_kind"]
                and normalized_description
                == " ".join(str(current["description"]).split()).casefold()
                and normalized_proof_route
                == " ".join(str(current["proof_route"]).split()).casefold()
            ):
                raise DeadlineError(
                    "A gap revision must materially change its description or proof route"
                )
            revision = int(current["revision"]) + 1
            self.connection.execute(
                """
                INSERT INTO closure_gap_revisions (
                    lineage_id, claim_id, closure_sequence, gap_id,
                    revision, recorded_at, basis_task_id,
                    description, proof_route, route_kind
                ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                """,
                (
                    lineage_id,
                    claim_id,
                    closure["sequence"],
                    gap_id,
                    revision,
                    recorded_at,
                    basis_task_id,
                    description,
                    proof_route + ("\nExplicit owner decision: " + owner_decision if owner_release else ""),
                    route_kind,
                ),
            )
            result = {
                "lineage_id": lineage_id,
                "claim_id": claim_id,
                "closure_sequence": closure["sequence"],
                "gap_id": gap_id,
                "revision": revision,
                "basis_task_id": basis_task_id,
                "description": description,
                "proof_route": proof_route,
                "route_kind": route_kind,
                "owner_decision": owner_decision,
            }
            self.connection.commit()
            return result
        except Exception:
            self.connection.rollback()
            raise

    def close_closure_gap(
        self,
        lineage_id: str,
        claim_id: str,
        gap_id: str,
        task_id: str,
        evidence: str,
        *,
        now: float | None = None,
    ) -> dict[str, Any]:
        """Close one named gap through its completed current-revision attempt."""

        lineage_id = self._identity(lineage_id, "Lineage id")
        claim_id = self._identity(claim_id, "Claim id")
        gap_id = self._identity(gap_id, "Closure gap id")
        task_id = self._identity(task_id, "Task id")
        evidence = self._nonempty_text(evidence, "Closure gap evidence")
        closed_at = self._now(now)
        self._begin()
        try:
            claim = self._claim(lineage_id, claim_id)
            if claim["phase"] != "closure":
                raise DeadlineError("Only an active closure gap can close")
            closure = self._active_closure(lineage_id, claim_id)
            gap = self._closure_gap(
                lineage_id, claim_id, int(closure["sequence"]), gap_id
            )
            task = self._task(lineage_id, task_id)
            revision = self._latest_gap_revision(
                lineage_id, claim_id, int(closure["sequence"]), gap_id
            )
            if gap["closed_at"] is not None:
                if (
                    gap["closed_by_task_id"] == task_id
                    and gap["closure_evidence"] == evidence
                ):
                    result = dict(gap)
                    result["recorded"] = False
                    self.connection.commit()
                    return result
                raise DeadlineError("Closure gap disposition is immutable")
            if (
                task["claim_id"] != claim_id
                or task["phase_sequence_at_dispatch"] != closure["sequence"]
                or task["closure_gap_id"] != gap_id
                or task["closure_gap_revision"] != revision["revision"]
                or task["attempt_terminal_kind"] != "completed"
                or task["integrity_breached_at"] is not None
            ):
                raise DeadlineError(
                    "Gap closure needs a completed attempt bound to its current revision"
                )
            self.connection.execute(
                """
                UPDATE closure_gaps
                SET closed_at = ?, closed_by_task_id = ?, closure_evidence = ?
                WHERE lineage_id = ? AND claim_id = ?
                  AND closure_sequence = ? AND gap_id = ?
                """,
                (
                    closed_at,
                    task_id,
                    evidence,
                    lineage_id,
                    claim_id,
                    closure["sequence"],
                    gap_id,
                ),
            )
            result = dict(
                self._closure_gap(
                    lineage_id, claim_id, int(closure["sequence"]), gap_id
                )
            )
            result["recorded"] = True
            result["remaining_gap_ids"] = [
                row["gap_id"]
                for row in self._closure_gaps(
                    lineage_id, claim_id, int(closure["sequence"])
                )
            ]
            self.connection.commit()
            return result
        except Exception:
            self.connection.rollback()
            raise

    def accept_claim(
        self,
        lineage_id: str,
        claim_id: str,
        task_id: str,
        evidence: str,
        *,
        now: float | None = None,
    ) -> dict[str, Any]:
        lineage_id = self._identity(lineage_id, "Lineage id")
        claim_id = self._identity(claim_id, "Claim id")
        task_id = self._identity(task_id, "Task id")
        evidence = self._nonempty_text(evidence, "Claim acceptance evidence")
        accepted_at = self._now(now)
        self._begin()
        try:
            claim = self._claim(lineage_id, claim_id)
            task = self._task(lineage_id, task_id)
            if task["claim_id"] != claim_id:
                raise DeadlineError("Acceptance attempt belongs to another claim")
            if claim["phase"] != "closure" or task["phase_at_dispatch"] != "closure":
                raise DeadlineError("Claim acceptance requires a closure attempt")
            if task["attempt_terminal_kind"] != "completed":
                raise DeadlineError("Claim acceptance requires a completed closure attempt")
            if task["integrity_breached_at"] is not None:
                raise DeadlineError("An integrity breach invalidates claim acceptance")
            closure = self.connection.execute(
                """
                SELECT * FROM claim_phase_events
                WHERE lineage_id = ? AND claim_id = ? AND phase = 'closure'
                ORDER BY sequence DESC LIMIT 1
                """,
                (lineage_id, claim_id),
            ).fetchone()
            if (
                closure is None
                or task["phase_sequence_at_dispatch"] != closure["sequence"]
            ):
                raise DeadlineError(
                    "Claim acceptance requires a completed attempt from the active closure epoch"
                )
            self._record_miss_if_due(task, accepted_at)
            # A deadline incident is a clock fact, not part of the acceptance verdict.
            # Commit it before later acceptance checks so their rollback cannot erase it.
            self.connection.commit()
        except Exception:
            self.connection.rollback()
            raise

        self._begin()
        try:
            claim = self._claim(lineage_id, claim_id)
            task = self._task(lineage_id, task_id)
            if task["claim_id"] != claim_id:
                raise DeadlineError("Acceptance attempt belongs to another claim")
            if claim["phase"] != "closure" or task["phase_at_dispatch"] != "closure":
                raise DeadlineError("Claim acceptance requires a closure attempt")
            if task["attempt_terminal_kind"] != "completed":
                raise DeadlineError("Claim acceptance requires a completed closure attempt")
            if task["integrity_breached_at"] is not None:
                raise DeadlineError("An integrity breach invalidates claim acceptance")
            closure = self.connection.execute(
                """
                SELECT * FROM claim_phase_events
                WHERE lineage_id = ? AND claim_id = ? AND phase = 'closure'
                ORDER BY sequence DESC LIMIT 1
                """,
                (lineage_id, claim_id),
            ).fetchone()
            if (
                closure is None
                or task["phase_sequence_at_dispatch"] != closure["sequence"]
            ):
                raise DeadlineError(
                    "Claim acceptance requires a completed attempt from the active closure epoch"
                )
            closure_sequence = int(closure["sequence"])
            live_siblings = self.connection.execute(
                """
                SELECT task_id FROM tasks
                WHERE lineage_id = ? AND claim_id = ?
                  AND task_id != ? AND attempt_terminal_at IS NULL
                ORDER BY started_at, task_id
                """,
                (lineage_id, claim_id, task_id),
            ).fetchall()
            if live_siblings:
                raise DeadlineError(
                    "Claim acceptance requires every sibling attempt to be terminal: "
                    + ", ".join(str(row["task_id"]) for row in live_siblings)
                )
            existing = self._latest_valid_acceptance(lineage_id, claim_id)
            gap_id = task["closure_gap_id"]
            open_gaps = self._closure_gaps(
                lineage_id, claim_id, closure_sequence
            )
            if gap_id is None:
                if not (
                    existing is not None
                    and existing["task_id"] == task_id
                    and existing["evidence"] == evidence
                    and not open_gaps
                ):
                    raise DeadlineError("Claim acceptance requires a named closure gap")
            else:
                gap_id = str(gap_id)
                revision = self._latest_gap_revision(
                    lineage_id, claim_id, closure_sequence, gap_id
                )
                if task["closure_gap_revision"] != revision["revision"]:
                    raise DeadlineError(
                        "Claim acceptance requires the current closure gap revision"
                    )
                if open_gaps:
                    if len(open_gaps) != 1 or open_gaps[0]["gap_id"] != gap_id:
                        raise DeadlineError(
                            "Claim acceptance is blocked by other named closure gaps"
                        )
                    self.connection.execute(
                        """
                        UPDATE closure_gaps
                        SET closed_at = ?, closed_by_task_id = ?, closure_evidence = ?
                        WHERE lineage_id = ? AND claim_id = ?
                          AND closure_sequence = ? AND gap_id = ?
                        """,
                        (
                            accepted_at,
                            task_id,
                            evidence,
                            lineage_id,
                            claim_id,
                            closure_sequence,
                            gap_id,
                        ),
                    )
                else:
                    gap = self._closure_gap(
                        lineage_id, claim_id, closure_sequence, gap_id
                    )
                    if gap["closed_by_task_id"] != task_id:
                        raise DeadlineError(
                            "Acceptance task must own the final gap disposition"
                        )
            if existing is not None:
                if (
                    existing["task_id"] != task_id
                    or existing["evidence"] != evidence
                    or existing["closure_sequence"] != closure_sequence
                ):
                    raise DeadlineError("Claim acceptance is immutable while valid")
                recorded = False
                acceptance = existing
            else:
                number = self.connection.execute(
                    """
                    SELECT COALESCE(MAX(acceptance_number), 0) + 1 AS next_number
                    FROM claim_acceptances
                    WHERE lineage_id = ? AND claim_id = ?
                    """,
                    (lineage_id, claim_id),
                ).fetchone()["next_number"]
                self.connection.execute(
                    """
                    INSERT INTO claim_acceptances (
                        lineage_id, claim_id, acceptance_number,
                        task_id, closure_sequence, accepted_at, evidence
                    ) VALUES (?, ?, ?, ?, ?, ?, ?)
                    """,
                    (
                        lineage_id,
                        claim_id,
                        number,
                        task_id,
                        closure_sequence,
                        accepted_at,
                        evidence,
                    ),
                )
                acceptance = self._latest_valid_acceptance(lineage_id, claim_id)
                recorded = True
            if acceptance is None:
                raise DeadlineError("Failed to persist claim acceptance")
            result = dict(acceptance)
            result["recorded"] = recorded
            result["closure_gap_id"] = gap_id
            result["deadline_missed"] = (
                self._claim_deadline_incident(lineage_id, claim_id) is not None
            )
            self.synchronize_dfs_statuses(persist=False)
            self.connection.commit()
            result["dfs_status_synchronized"] = list(
                self.synchronize_dfs_statuses()
            )
            return result
        except Exception:
            self.connection.rollback()
            raise

    def report_worker_finding(
        self,
        lineage_id: str,
        task_id: str,
        kind: str,
        evidence: str,
        *,
        receipt_id: str | None = None,
        short_verdict: str | None = None,
        now: float | None = None,
    ) -> dict[str, Any]:
        lineage_id = self._identity(lineage_id, "Lineage id")
        task_id = self._identity(task_id, "Task id")
        kind = self._identity(kind, "Worker finding kind")
        if kind not in {"blocker", "unexpected"}:
            raise DeadlineError("Worker finding kind must be blocker or unexpected")
        evidence = self._nonempty_text(evidence, "Worker finding evidence")
        if short_verdict is not None:
            short_verdict = self._nonempty_text(
                short_verdict, "Worker finding short verdict"
            )
        reported_at = self._now(now)
        self._begin()
        try:
            task = self._task(lineage_id, task_id)
            self._require_terminal_worker_receipt(
                lineage_id, task_id, receipt_id, "finding"
            )
            if task["integrity_breached_at"] is not None:
                raise DeadlineError(
                    "An integrity breach prevents a worker finding from being recorded"
                )
            if task["completed_at"] is not None:
                raise DeadlineError(
                    "An accepted task cannot later report a worker finding"
                )
            existing = self._worker_finding(lineage_id, task_id)
            if task["attempt_terminal_at"] is not None and existing is None:
                raise DeadlineError("Attempt already has another terminal outcome")

            if short_verdict is None:
                short_verdict = (
                    existing["short_verdict"]
                    if existing is not None
                    else kind.replace("_", " ")
                )
            if existing is not None and (
                existing["kind"] != kind
                or existing["short_verdict"] != short_verdict
                or existing["evidence"] != evidence
            ):
                raise DeadlineError("A worker finding is immutable once recorded")

            deadline_incident = self._record_miss_if_due(task, reported_at)
            if existing is None:
                self.connection.execute(
                    """
                    INSERT INTO worker_findings (
                        lineage_id, task_id, kind, reported_at,
                        short_verdict, evidence
                    ) VALUES (?, ?, ?, ?, ?, ?)
                    """,
                    (
                        lineage_id,
                        task_id,
                        kind,
                        reported_at,
                        short_verdict,
                        evidence,
                    ),
                )
                finding = self._worker_finding(lineage_id, task_id)
                recorded = True
            else:
                finding = existing
                recorded = False

            self._record_terminal_window(task, reported_at, "finding")

            result = {
                "deadline_incident": deadline_incident,
                "finding": self._finding_result(finding, recorded=recorded),
                "status": self._status(lineage_id, task_id, reported_at),
                "random_mutation": self._random_mutation_status(lineage_id),
            }
            self.connection.commit()
            return result
        except Exception:
            self.connection.rollback()
            raise

    def diagnose_incident(
        self,
        lineage_id: str,
        task_id: str,
        kind: str,
        short_verdict: str,
        diagnosis: str,
        *,
        now: float | None = None,
    ) -> dict[str, Any]:
        """Attach one immutable independent review to an exact incident."""
        lineage_id = self._identity(lineage_id, "Lineage id")
        task_id = self._identity(task_id, "Task id")
        kind = self._identity(kind, "Incident kind")
        if kind not in INCIDENT_KINDS:
            raise DeadlineError(
                "Incident kind must be deadline_miss or integrity_breach"
            )
        short_verdict = self._nonempty_text(short_verdict, "Short verdict")
        diagnosis = self._nonempty_text(diagnosis, "Long diagnosis")
        reviewed_at = self._now(now)
        if kind == "deadline_miss":
            task = self._task(lineage_id, task_id)
            claim_id = str(task["claim_id"])
            claim_incident = self._claim_deadline_incident(lineage_id, claim_id)
            if claim_incident is not None:
                diagnosed_claim = self.diagnose_claim_deadline(
                    lineage_id,
                    claim_id,
                    short_verdict,
                    diagnosis,
                    now=reviewed_at,
                )
                compatibility = self.connection.execute(
                    """
                    SELECT * FROM incidents
                    WHERE lineage_id = ? AND task_id = ? AND kind = 'deadline_miss'
                    """,
                    (lineage_id, diagnosed_claim["source_task_id"]),
                ).fetchone()
                if compatibility is None:
                    raise DeadlineError(
                        "Claim deadline diagnosis lacks its compatibility incident"
                    )
                return {
                    "recorded": diagnosed_claim["recorded"],
                    "incident": self._incident_result(
                        compatibility, recorded=False
                    ),
                    "claim_id": claim_id,
                    "pending_components": diagnosed_claim["pending_components"],
                }
        self._begin()
        try:
            task = self._task(lineage_id, task_id)
            incident = self.connection.execute(
                """
                SELECT * FROM incidents
                WHERE lineage_id = ? AND task_id = ? AND kind = ?
                """,
                (lineage_id, task_id, kind),
            ).fetchone()
            if incident is None:
                raise DeadlineError(
                    f"Unknown incident: {lineage_id}/{task_id}/{kind}"
                )
            if incident["reviewed_at"] is None:
                self.connection.execute(
                    """
                    UPDATE incidents
                    SET short_verdict = ?, long_detail = ?, reviewed_at = ?
                    WHERE lineage_id = ? AND task_id = ? AND kind = ?
                    """,
                    (
                        short_verdict,
                        diagnosis,
                        reviewed_at,
                        lineage_id,
                        task_id,
                        kind,
                    ),
                )
                recorded = True
            elif (
                incident["short_verdict"] == short_verdict
                and incident["long_detail"] == diagnosis
            ):
                recorded = False
            else:
                raise DeadlineError(
                    "An incident diagnosis is immutable once recorded"
                )
            diagnosed = self.connection.execute(
                """
                SELECT * FROM incidents
                WHERE lineage_id = ? AND task_id = ? AND kind = ?
                """,
                (lineage_id, task_id, kind),
            ).fetchone()
            if diagnosed is None:
                raise DeadlineError("Failed to read the diagnosed incident")
            result = {
                "recorded": recorded,
                "incident": self._incident_result(diagnosed, recorded=False),
            }
            if kind == "integrity_breach":
                result["claim_id"] = task["claim_id"]
                result["pending_components"] = [
                    component
                    for component in ("micro", "macro")
                    if component
                    not in self._integrity_mutation_components(lineage_id, task_id)
                ]
            self.connection.commit()
            return result
        except Exception:
            self.connection.rollback()
            raise

    def diagnose_claim_deadline(
        self,
        lineage_id: str,
        claim_id: str,
        short_verdict: str,
        diagnosis: str,
        *,
        now: float | None = None,
    ) -> dict[str, Any]:
        lineage_id = self._identity(lineage_id, "Lineage id")
        claim_id = self._identity(claim_id, "Claim id")
        short_verdict = self._nonempty_text(short_verdict, "Short verdict")
        diagnosis = self._nonempty_text(diagnosis, "Long diagnosis")
        reviewed_at = self._now(now)
        self._begin()
        try:
            incident = self._claim_deadline_incident(lineage_id, claim_id)
            if incident is None:
                raise DeadlineError(
                    f"Unknown claim deadline incident: {lineage_id}/{claim_id}"
                )
            if incident["reviewed_at"] is None:
                self.connection.execute(
                    """
                    UPDATE claim_deadline_generation_incidents
                    SET short_verdict = ?, long_detail = ?, reviewed_at = ?
                    WHERE lineage_id = ? AND claim_id = ? AND generation = ?
                    """,
                    (
                        short_verdict,
                        diagnosis,
                        reviewed_at,
                        lineage_id,
                        claim_id,
                        incident["generation"],
                    ),
                )
                self.connection.execute(
                    """
                    UPDATE incidents
                    SET short_verdict = ?, long_detail = ?, reviewed_at = ?
                    WHERE lineage_id = ? AND task_id = ?
                      AND kind = 'deadline_miss' AND reviewed_at IS NULL
                    """,
                    (
                        short_verdict,
                        diagnosis,
                        reviewed_at,
                        lineage_id,
                        incident["source_task_id"],
                    ),
                )
                self.connection.execute(
                    """
                    UPDATE claim_deadline_incidents
                    SET short_verdict = ?, long_detail = ?, reviewed_at = ?
                    WHERE lineage_id = ? AND claim_id = ?
                      AND source_task_id = ?
                    """,
                    (
                        short_verdict, diagnosis, reviewed_at, lineage_id,
                        claim_id, incident["source_task_id"],
                    ),
                )
                recorded = True
            elif (
                incident["short_verdict"] == short_verdict
                and incident["long_detail"] == diagnosis
            ):
                recorded = False
            else:
                raise DeadlineError(
                    "A claim deadline diagnosis is immutable once recorded"
                )
            diagnosed = self._claim_deadline_incident(lineage_id, claim_id)
            if diagnosed is None:
                raise DeadlineError("Failed to read diagnosed claim deadline")
            result = dict(diagnosed)
            result["recorded"] = recorded
            result["pending_components"] = [
                component
                for component in ("micro", "macro")
                if component
                not in self._deadline_mutation_components(
                    lineage_id, claim_id, int(diagnosed["generation"])
                )
            ]
            self.connection.commit()
            return result
        except Exception:
            self.connection.rollback()
            raise

    @staticmethod
    def _normal_method_receipt_contract(receipt: sqlite3.Row) -> dict[str, Any]:
        try:
            changed_paths = json.loads(str(receipt["changed_paths"]))
        except json.JSONDecodeError as error:
            raise DeadlineError("Method receipt has invalid changed-path evidence") from error
        if (
            not isinstance(changed_paths, list)
            or not changed_paths
            or any(not isinstance(path, str) or not path for path in changed_paths)
            or changed_paths != sorted(set(changed_paths))
        ):
            raise DeadlineError("Method receipt has invalid changed-path evidence")
        return {
            "lineage_id": receipt["lineage_id"],
            "task_id": receipt["task_id"],
            "claim_id": receipt["claim_id"],
            "incident_kind": receipt["incident_kind"],
            "candidate_digest": receipt["candidate_digest"],
            "changed_paths": changed_paths,
            "protected_baseline_digest": receipt["protected_baseline_digest"],
            "live_tree_digest": receipt["live_tree_digest"],
        }

    def _validate_normal_method_receipt(
        self,
        receipt_id: str | None,
        *,
        lineage_id: str,
        task_id: str,
        claim_id: str,
        incident_kind: str,
    ) -> sqlite3.Row:
        if receipt_id is None:
            raise DeadlineError("Macro mutation requires a guard-issued method receipt")
        receipt = self.connection.execute(
            """
            SELECT * FROM normal_method_receipts
            WHERE receipt_id = ? AND lineage_id = ? AND task_id = ?
              AND claim_id = ? AND incident_kind = ?
            """,
            (receipt_id, lineage_id, task_id, claim_id, incident_kind),
        ).fetchone()
        if receipt is None:
            raise DeadlineError(
                "Method receipt does not match this exact incident and task"
            )
        contract = self._normal_method_receipt_contract(receipt)
        for field in (
            "candidate_digest",
            "protected_baseline_digest",
            "live_tree_digest",
        ):
            value = str(contract[field])
            if (
                len(value) != 64
                or any(character not in "0123456789abcdef" for character in value)
            ):
                raise DeadlineError("Method receipt contains an invalid digest")
        expected_id = hashlib.sha256(
            json.dumps(contract, sort_keys=True, separators=(",", ":")).encode(
                "utf-8"
            )
        ).hexdigest()
        if receipt_id != expected_id:
            raise DeadlineError("Method receipt digest does not match its contract")

        candidate_digests = {
            method_tree_digest(),
            workspace_method_digest(self.state_path),
        }
        if receipt["candidate_digest"] not in candidate_digests:
            raise DeadlineError(
                "Method receipt candidate is not the active published or workspace-local Phase-3 tree"
            )
        if protected_method_digest() != receipt["protected_baseline_digest"]:
            raise DeadlineError(
                "Active Phase-3 protected files differ from the guarded baseline"
            )

        uses = self.connection.execute(
            """
            SELECT 'deadline_miss' AS incident_kind, lineage_id,
                   claim_id AS target_id, receipt_id
            FROM deadline_mutation_components
            WHERE receipt_id = ?
            UNION ALL
            SELECT 'integrity_breach', lineage_id, task_id, receipt_id
            FROM integrity_mutation_components
            WHERE receipt_id = ?
            """,
            (receipt_id, receipt_id),
        ).fetchall()
        expected_target = claim_id if incident_kind == "deadline_miss" else task_id
        if any(
            use["incident_kind"] != incident_kind
            or use["lineage_id"] != lineage_id
            or use["target_id"] != expected_target
            for use in uses
        ):
            raise DeadlineError("Method receipt has already been consumed")
        return receipt

    def resolve_deadline_mutation(
        self,
        lineage_id: str,
        claim_id: str,
        component: str,
        evidence: str,
        *,
        receipt_id: str | None = None,
        no_change_required: bool = False,
        now: float | None = None,
    ) -> dict[str, Any]:
        lineage_id = self._identity(lineage_id, "Lineage id")
        claim_id = self._identity(claim_id, "Claim id")
        component = self._identity(component, "Mutation component")
        if component not in {"micro", "macro"}:
            raise DeadlineError("Deadline mutation component must be micro or macro")
        evidence = self._nonempty_text(evidence, "Mutation evidence")
        if component == "macro":
            if no_change_required and receipt_id is not None:
                raise DeadlineError(
                    "No-change macro resolution cannot consume a method receipt"
                )
            if receipt_id is not None:
                receipt_id = self._identity(receipt_id, "Method receipt id")
        elif no_change_required:
            raise DeadlineError(
                "No-change resolution is only valid for a reviewed macro proposal"
            )
        elif receipt_id is not None:
            raise DeadlineError("Deadline micro mutation cannot consume a receipt")
        resolved_at = self._now(now)
        self._begin()
        try:
            incident = self._claim_deadline_incident(lineage_id, claim_id)
            if incident is None:
                raise DeadlineError(
                    f"Unknown claim deadline incident: {lineage_id}/{claim_id}"
                )
            if incident["reviewed_at"] is None:
                raise DeadlineError(
                    "Claim deadline needs an independent diagnosis before mutation"
                )
            if component == "macro" and receipt_id is not None:
                self._validate_normal_method_receipt(
                    receipt_id,
                    lineage_id=lineage_id,
                    task_id=str(incident["source_task_id"]),
                    claim_id=claim_id,
                    incident_kind="deadline_miss",
                )
            existing = self.connection.execute(
                """
                SELECT * FROM deadline_generation_mutation_components
                WHERE lineage_id = ? AND claim_id = ?
                  AND generation = ? AND component = ?
                """,
                (lineage_id, claim_id, incident["generation"], component),
            ).fetchone()
            if existing is None:
                self.connection.execute(
                    """
                    INSERT INTO deadline_generation_mutation_components (
                        lineage_id, claim_id, generation, component,
                        resolved_at, evidence, receipt_id, no_change_required
                    ) VALUES (?, ?, ?, ?, ?, ?, ?, ?)
                    """,
                    (
                        lineage_id,
                        claim_id,
                        incident["generation"],
                        component,
                        resolved_at,
                        evidence,
                        receipt_id,
                        int(no_change_required),
                    ),
                )
                if int(incident["generation"]) == 1:
                    self.connection.execute(
                        """
                        INSERT OR IGNORE INTO deadline_mutation_components (
                            lineage_id, claim_id, component, resolved_at,
                            evidence, receipt_id
                        ) VALUES (?, ?, ?, ?, ?, ?)
                        """,
                        (
                            lineage_id, claim_id, component, resolved_at,
                            evidence, receipt_id,
                        ),
                    )
                recorded = True
            elif (
                existing["evidence"] == evidence
                and existing["receipt_id"] == receipt_id
                and bool(existing["no_change_required"]) == no_change_required
            ):
                recorded = False
            else:
                raise DeadlineError(
                    "Deadline mutation resolution is immutable once recorded"
                )
            components = self._deadline_mutation_components(
                lineage_id, claim_id, int(incident["generation"])
            )
            pending = [
                name for name in ("micro", "macro") if name not in components
            ]
            restart: sqlite3.Row | None = None
            if not pending:
                refreshed = self._claim_deadline_incident(lineage_id, claim_id)
                if refreshed is None:
                    raise DeadlineError("Failed to read claim deadline incident")
                if refreshed["restart_generation"] is None:
                    restart, _ = self._request_coordinator_restart(
                        lineage_id,
                        f"deadline mutation resolved for {claim_id}; "
                        "successor must set a fresh clock without inheritance",
                        resolved_at,
                    )
                    self.connection.execute(
                        """
                        UPDATE claim_deadline_generation_incidents
                        SET restart_generation = ?
                        WHERE lineage_id = ? AND claim_id = ? AND generation = ?
                        """,
                        (
                            restart["generation"], lineage_id, claim_id,
                            incident["generation"],
                        ),
                    )
                    if int(incident["generation"]) == 1:
                        self.connection.execute(
                            """
                            UPDATE claim_deadline_incidents
                            SET restart_generation = ?
                            WHERE lineage_id = ? AND claim_id = ?
                            """,
                            (restart["generation"], lineage_id, claim_id),
                        )
                else:
                    restart = self.connection.execute(
                        """
                        SELECT * FROM coordinator_restart_requests
                        WHERE lineage_id = ? AND generation = ?
                        """,
                        (lineage_id, refreshed["restart_generation"]),
                    ).fetchone()
            result = {
                "recorded": recorded,
                "lineage_id": lineage_id,
                "claim_id": claim_id,
                "deadline_generation": incident["generation"],
                "component": component,
                "disposition": (
                    "no_change_required" if no_change_required else "applied"
                ),
                "receipt_id": receipt_id,
                "pending_components": pending,
                "coordinator_restart": (
                    self._coordinator_restart_result(restart)
                    if restart is not None
                    else None
                ),
            }
            self.connection.commit()
            return result
        except Exception:
            self.connection.rollback()
            raise

    def resolve_integrity_mutation(
        self,
        lineage_id: str,
        task_id: str,
        component: str,
        evidence: str,
        *,
        receipt_id: str | None = None,
        now: float | None = None,
    ) -> dict[str, Any]:
        """Resolve one exact breach's micro or macro method obligation."""

        lineage_id = self._identity(lineage_id, "Lineage id")
        task_id = self._identity(task_id, "Task id")
        component = self._identity(component, "Mutation component")
        if component not in {"micro", "macro"}:
            raise DeadlineError("Integrity mutation component must be micro or macro")
        evidence = self._nonempty_text(evidence, "Mutation evidence")
        if component == "macro":
            if receipt_id is not None:
                receipt_id = self._identity(receipt_id, "Method receipt id")
        elif receipt_id is not None:
            raise DeadlineError("Integrity micro mutation cannot consume a receipt")
        resolved_at = self._now(now)
        self._begin()
        try:
            task = self._task(lineage_id, task_id)
            incident = self._integrity_incident(lineage_id, task_id)
            if incident is None:
                raise DeadlineError(
                    f"Unknown integrity incident: {lineage_id}/{task_id}"
                )
            if incident["reviewed_at"] is None:
                raise DeadlineError(
                    "Integrity incident needs an independent diagnosis before mutation"
                )
            if component == "macro" and receipt_id is not None:
                self._validate_normal_method_receipt(
                    receipt_id,
                    lineage_id=lineage_id,
                    task_id=task_id,
                    claim_id=str(task["claim_id"]),
                    incident_kind="integrity_breach",
                )
            existing = self.connection.execute(
                """
                SELECT * FROM integrity_mutation_components
                WHERE lineage_id = ? AND task_id = ? AND component = ?
                """,
                (lineage_id, task_id, component),
            ).fetchone()
            if existing is None:
                self.connection.execute(
                    """
                    INSERT INTO integrity_mutation_components (
                        lineage_id, task_id, component, resolved_at, evidence,
                        receipt_id
                    ) VALUES (?, ?, ?, ?, ?, ?)
                    """,
                    (
                        lineage_id,
                        task_id,
                        component,
                        resolved_at,
                        evidence,
                        receipt_id,
                    ),
                )
                recorded = True
            elif (
                existing["evidence"] == evidence
                and existing["receipt_id"] == receipt_id
            ):
                recorded = False
            else:
                raise DeadlineError(
                    "Integrity mutation resolution is immutable once recorded"
                )
            components = self._integrity_mutation_components(lineage_id, task_id)
            pending = [
                name for name in ("micro", "macro") if name not in components
            ]
            restart: sqlite3.Row | None = None
            if not pending:
                refreshed = self._integrity_incident(lineage_id, task_id)
                if refreshed is None:
                    raise DeadlineError("Failed to read integrity incident")
                if refreshed["restart_generation"] is None:
                    restart, _ = self._request_coordinator_restart(
                        lineage_id,
                        f"integrity mutation resolved for {task_id}",
                        resolved_at,
                    )
                    self.connection.execute(
                        """
                        UPDATE incidents SET restart_generation = ?
                        WHERE lineage_id = ? AND task_id = ?
                          AND kind = 'integrity_breach'
                        """,
                        (restart["generation"], lineage_id, task_id),
                    )
                else:
                    restart = self.connection.execute(
                        """
                        SELECT * FROM coordinator_restart_requests
                        WHERE lineage_id = ? AND generation = ?
                        """,
                        (lineage_id, refreshed["restart_generation"]),
                    ).fetchone()
            result = {
                "recorded": recorded,
                "lineage_id": lineage_id,
                "task_id": task_id,
                "claim_id": task["claim_id"],
                "incident_kind": "integrity_breach",
                "component": component,
                "receipt_id": receipt_id,
                "pending_components": pending,
                "coordinator_restart": (
                    self._coordinator_restart_result(restart)
                    if restart is not None
                    else None
                ),
            }
            self.connection.commit()
            return result
        except Exception:
            self.connection.rollback()
            raise

    def _append_breach_successor_gap(
        self, task: sqlite3.Row, reason: str, recorded_at: float
    ) -> dict[str, Any] | None:
        """Restore actionable closure work without reopening old dispositions."""

        claim = self._claim(str(task["lineage_id"]), str(task["claim_id"]))
        if claim["phase"] != "closure":
            return None
        closure = self._active_closure(
            str(task["lineage_id"]), str(task["claim_id"])
        )
        if task["phase_sequence_at_dispatch"] != closure["sequence"]:
            return None
        source_gap_id = task["closure_gap_id"]
        source_revision_number = task["closure_gap_revision"]
        if source_gap_id is None or source_revision_number is None:
            legacy_source = self.connection.execute(
                """
                SELECT gap_id FROM closure_gaps
                WHERE lineage_id = ? AND claim_id = ? AND closure_sequence = ?
                  AND closed_by_task_id = ?
                ORDER BY gap_id
                """,
                (
                    task["lineage_id"],
                    task["claim_id"],
                    closure["sequence"],
                    task["task_id"],
                ),
            ).fetchall()
            if len(legacy_source) != 1:
                return None
            source_gap_id = str(legacy_source[0]["gap_id"])
            source_revision_number = int(
                self._latest_gap_revision(
                    str(task["lineage_id"]),
                    str(task["claim_id"]),
                    int(closure["sequence"]),
                    source_gap_id,
                )["revision"]
            )
        source = self._closure_gap(
            str(task["lineage_id"]),
            str(task["claim_id"]),
            int(closure["sequence"]),
            source_gap_id,
        )
        if (
            source["closed_at"] is None
            or source["closed_by_task_id"] != task["task_id"]
        ):
            return None
        existing = self.connection.execute(
            """
            SELECT * FROM closure_gaps
            WHERE lineage_id = ? AND claim_id = ? AND closure_sequence = ?
              AND successor_of_gap_id = ? AND successor_of_revision = ?
              AND basis_task_id = ?
            ORDER BY opened_at, gap_id LIMIT 1
            """,
            (
                task["lineage_id"],
                task["claim_id"],
                closure["sequence"],
                source_gap_id,
                source_revision_number,
                task["task_id"],
            ),
        ).fetchone()
        if existing is not None:
            revision = self._latest_gap_revision(
                str(task["lineage_id"]),
                str(task["claim_id"]),
                int(closure["sequence"]),
                str(existing["gap_id"]),
            )
            return {
                "gap_id": existing["gap_id"],
                "revision": revision["revision"],
                "recorded": False,
            }
        source_revision = self.connection.execute(
            """
            SELECT * FROM closure_gap_revisions
            WHERE lineage_id = ? AND claim_id = ? AND closure_sequence = ?
              AND gap_id = ? AND revision = ?
            """,
            (
                task["lineage_id"],
                task["claim_id"],
                closure["sequence"],
                source_gap_id,
                source_revision_number,
            ),
        ).fetchone()
        if source_revision is None:
            raise DeadlineError("Breached closure proof has no bound gap revision")
        suffix = 1
        while True:
            successor_gap_id = f"{source_gap_id}~B{suffix}"
            if self.connection.execute(
                """
                SELECT 1 FROM closure_gaps
                WHERE lineage_id = ? AND claim_id = ?
                  AND closure_sequence = ? AND gap_id = ?
                """,
                (
                    task["lineage_id"],
                    task["claim_id"],
                    closure["sequence"],
                    successor_gap_id,
                ),
            ).fetchone() is None:
                break
            suffix += 1
        description = (
            f"Re-prove {source_revision['description']} after integrity breach: {reason}"
        )
        proof_route = (
            f"Replace invalidated evidence from {task['task_id']}; "
            f"then {source_revision['proof_route']}"
        )
        self.connection.execute(
            """
            INSERT INTO closure_gaps (
                lineage_id, claim_id, closure_sequence, gap_id,
                opened_at, basis_task_id, successor_of_gap_id,
                successor_of_revision, reopen_reason
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
            """,
            (
                task["lineage_id"],
                task["claim_id"],
                closure["sequence"],
                successor_gap_id,
                recorded_at,
                task["task_id"],
                source_gap_id,
                source_revision_number,
                reason,
            ),
        )
        self.connection.execute(
            """
            INSERT INTO closure_gap_revisions (
                lineage_id, claim_id, closure_sequence, gap_id,
                revision, recorded_at, basis_task_id, description, proof_route
            ) VALUES (?, ?, ?, ?, 1, ?, ?, ?, ?)
            """,
            (
                task["lineage_id"],
                task["claim_id"],
                closure["sequence"],
                successor_gap_id,
                recorded_at,
                task["task_id"],
                description,
                proof_route,
            ),
        )
        return {"gap_id": successor_gap_id, "revision": 1, "recorded": True}

    def record_integrity_breach(
        self,
        lineage_id: str,
        task_id: str,
        reason: str,
        *,
        now: float | None = None,
    ) -> dict[str, Any]:
        lineage_id = self._identity(lineage_id, "Lineage id")
        task_id = self._identity(task_id, "Task id")
        reason = self._nonempty_text(reason, "Integrity breach reason")
        recorded_at = self._now(now)
        self._begin()
        try:
            task = self._task(lineage_id, task_id)
            deadline_incident = self._record_miss_if_due(
                task, recorded_at, completion_invalid=True
            )
            incident = self._record_incident(
                lineage_id, task_id, "integrity_breach", 1, recorded_at, reason
            )
            successor_gap = None
            if task["integrity_breached_at"] is None:
                self.connection.execute(
                    """
                    UPDATE tasks
                    SET integrity_breached_at = ?, integrity_reason = ?
                    WHERE lineage_id = ? AND task_id = ?
                    """,
                    (recorded_at, reason, lineage_id, task_id),
                )
                accepted = self.connection.execute(
                    """
                    SELECT * FROM claim_acceptances AS accepted
                    WHERE accepted.lineage_id = ? AND accepted.claim_id = ?
                      AND accepted.invalidated_at IS NULL
                      AND (
                        accepted.task_id = ?
                        OR EXISTS (
                            SELECT 1 FROM closure_gaps AS gap
                            WHERE gap.lineage_id = accepted.lineage_id
                              AND gap.claim_id = accepted.claim_id
                              AND gap.closure_sequence = accepted.closure_sequence
                              AND gap.closed_by_task_id = ?
                        )
                      )
                    ORDER BY accepted.acceptance_number DESC
                    LIMIT 1
                    """,
                    (lineage_id, task["claim_id"], task_id, task_id),
                ).fetchone()
                invalidation_reason = (
                    reason
                    if accepted is None or accepted["task_id"] == task_id
                    else f"closure proof {task_id} breached: {reason}"
                )
                self.connection.execute(
                    """
                    UPDATE claim_acceptances
                    SET invalidated_at = ?, invalidation_reason = ?
                    WHERE lineage_id = ? AND claim_id = ?
                      AND acceptance_number = ? AND invalidated_at IS NULL
                    """,
                    (
                        recorded_at,
                        invalidation_reason,
                        lineage_id,
                        task["claim_id"],
                        accepted["acceptance_number"] if accepted is not None else -1,
                    ),
                )
                successor_gap = self._append_breach_successor_gap(
                    task, reason, recorded_at
                )
            self._record_terminal_window(task, recorded_at, "integrity_breach")
            result = {
                "deadline_incident": deadline_incident,
                "incident": incident,
                "status": self._status(lineage_id, task_id, recorded_at),
                "successor_closure_gap": successor_gap,
                "mutation_pending": self._integrity_mutation_pending(
                    lineage_id, task_id
                ),
                "random_mutation": self._random_mutation_status(lineage_id),
            }
            self.connection.commit()
            return result
        except Exception:
            self.connection.rollback()
            raise

    def request_coordinator_restart(
        self,
        lineage_id: str,
        reason: str,
        *,
        now: float | None = None,
    ) -> dict[str, Any]:
        lineage_id = self._identity(lineage_id, "Lineage id")
        reason = self._nonempty_text(reason, "Coordinator restart reason")
        requested_at = self._now(now)
        self._begin()
        try:
            self._bind_lineage(lineage_id)
            request, created = self._request_coordinator_restart(
                lineage_id, reason, requested_at
            )
            result = {
                "created": created,
                "coordinator_restart": self._coordinator_restart_result(request),
            }
            self.connection.commit()
            return result
        except Exception:
            self.connection.rollback()
            raise

    def claim_coordinator_restart(
        self,
        lineage_id: str,
        generation: int,
        run_id: str,
        *,
        now: float | None = None,
    ) -> dict[str, Any]:
        """Bind a pending generation to the supervisor-issued successor id."""
        lineage_id = self._identity(lineage_id, "Lineage id")
        if isinstance(generation, bool) or not isinstance(generation, int):
            raise DeadlineError("Coordinator restart generation must be a positive integer")
        if generation <= 0:
            raise DeadlineError("Coordinator restart generation must be a positive integer")
        run_id = self._nonempty_text(run_id, "Coordinator run id")
        claimed_at = self._now(now)
        self._begin()
        try:
            self._bind_lineage(lineage_id)
            request = self.connection.execute(
                """
                SELECT * FROM coordinator_restart_requests
                WHERE lineage_id = ? AND generation = ?
                """,
                (lineage_id, generation),
            ).fetchone()
            if request is None:
                raise DeadlineError(
                    f"Unknown coordinator restart generation: {lineage_id}/{generation}"
                )
            if request["acknowledged_at"] is not None:
                raise DeadlineError("Acknowledged coordinator restart cannot be claimed")
            if request["expected_run_id"] is None:
                self.connection.execute(
                    """
                    UPDATE coordinator_restart_requests
                    SET claimed_at = ?, expected_run_id = ?
                    WHERE lineage_id = ? AND generation = ?
                    """,
                    (claimed_at, run_id, lineage_id, generation),
                )
                recorded = True
            elif request["expected_run_id"] == run_id:
                recorded = False
            else:
                raise DeadlineError(
                    "Coordinator restart is already claimed by a different run"
                )
            claimed = self.connection.execute(
                """
                SELECT * FROM coordinator_restart_requests
                WHERE lineage_id = ? AND generation = ?
                """,
                (lineage_id, generation),
            ).fetchone()
            if claimed is None:
                raise DeadlineError("Failed to read coordinator restart claim")
            result = {
                "recorded": recorded,
                "coordinator_restart": self._coordinator_restart_result(claimed),
            }
            self.connection.commit()
            return result
        except Exception:
            self.connection.rollback()
            raise

    def release_coordinator_restart_claim(
        self,
        lineage_id: str,
        generation: int,
        run_id: str,
    ) -> dict[str, Any]:
        """Release one unacknowledged claim after its runner is confirmed gone."""
        lineage_id = self._identity(lineage_id, "Lineage id")
        if isinstance(generation, bool) or not isinstance(generation, int):
            raise DeadlineError("Coordinator restart generation must be a positive integer")
        if generation <= 0:
            raise DeadlineError("Coordinator restart generation must be a positive integer")
        run_id = self._nonempty_text(run_id, "Coordinator run id")
        self._begin()
        try:
            self._bind_lineage(lineage_id)
            request = self.connection.execute(
                """
                SELECT * FROM coordinator_restart_requests
                WHERE lineage_id = ? AND generation = ?
                """,
                (lineage_id, generation),
            ).fetchone()
            if request is None:
                raise DeadlineError(
                    f"Unknown coordinator restart generation: {lineage_id}/{generation}"
                )
            if request["acknowledged_at"] is not None:
                raise DeadlineError("Acknowledged coordinator restart claim cannot be released")
            if request["expected_run_id"] != run_id:
                raise DeadlineError("Coordinator restart claim does not match that run")
            self.connection.execute(
                """
                UPDATE coordinator_restart_requests
                SET claimed_at = NULL, expected_run_id = NULL
                WHERE lineage_id = ? AND generation = ?
                """,
                (lineage_id, generation),
            )
            released = self.connection.execute(
                """
                SELECT * FROM coordinator_restart_requests
                WHERE lineage_id = ? AND generation = ?
                """,
                (lineage_id, generation),
            ).fetchone()
            if released is None:
                raise DeadlineError("Failed to read released coordinator restart claim")
            self.connection.commit()
            return {
                "released": True,
                "coordinator_restart": self._coordinator_restart_result(released),
            }
        except Exception:
            self.connection.rollback()
            raise

    def acknowledge_coordinator_restart(
        self,
        lineage_id: str,
        generation: int,
        run_id: str,
        *,
        now: float | None = None,
    ) -> dict[str, Any]:
        lineage_id = self._identity(lineage_id, "Lineage id")
        if isinstance(generation, bool) or not isinstance(generation, int):
            raise DeadlineError("Coordinator restart generation must be a positive integer")
        if generation <= 0:
            raise DeadlineError("Coordinator restart generation must be a positive integer")
        run_id = self._nonempty_text(run_id, "Coordinator run id")
        acknowledged_at = self._now(now)
        self._begin()
        try:
            self._bind_lineage(lineage_id)
            request = self.connection.execute(
                """
                SELECT * FROM coordinator_restart_requests
                WHERE lineage_id = ? AND generation = ?
                """,
                (lineage_id, generation),
            ).fetchone()
            if request is None:
                raise DeadlineError(
                    f"Unknown coordinator restart generation: {lineage_id}/{generation}"
                )
            if request["expected_run_id"] is None:
                raise DeadlineError(
                    "Coordinator restart must be claimed by its supervisor before acknowledgement"
                )
            if request["expected_run_id"] != run_id:
                raise DeadlineError(
                    "Coordinator restart acknowledgement does not match its claimed run"
                )
            if request["acknowledged_at"] is None:
                self.connection.execute(
                    """
                    UPDATE coordinator_restart_requests
                    SET acknowledged_at = ?, run_id = ?
                    WHERE lineage_id = ? AND generation = ?
                    """,
                    (acknowledged_at, run_id, lineage_id, generation),
                )
                recorded = True
            elif request["run_id"] == run_id:
                recorded = False
            else:
                raise DeadlineError(
                    "Coordinator restart acknowledgement is immutable once recorded"
                )
            acknowledged = self.connection.execute(
                """
                SELECT * FROM coordinator_restart_requests
                WHERE lineage_id = ? AND generation = ?
                """,
                (lineage_id, generation),
            ).fetchone()
            if acknowledged is None:
                raise DeadlineError("Failed to read coordinator restart acknowledgement")
            result = {
                "recorded": recorded,
                "coordinator_restart": self._coordinator_restart_result(acknowledged),
            }
            self.connection.commit()
            return result
        except Exception:
            self.connection.rollback()
            raise

    def resolve_random_mutation(
        self,
        lineage_id: str,
        cycle_number: int,
        evidence: str,
        *,
        component: str = "ordinary",
        receipt_id: str | None = None,
        now: float | None = None,
    ) -> dict[str, Any]:
        lineage_id = self._identity(lineage_id, "Lineage id")
        if isinstance(cycle_number, bool) or not isinstance(cycle_number, int):
            raise DeadlineError("Cycle number must be a positive integer")
        if cycle_number <= 0:
            raise DeadlineError("Cycle number must be a positive integer")
        component = self._identity(component, "Random review component")
        if component not in {"ordinary", "universal"}:
            raise DeadlineError("Random review component must be ordinary or universal")
        if component == "universal":
            if receipt_id is None:
                raise DeadlineError(
                    "Universal mutation resolution requires its validated receipt id"
                )
            receipt_id = self._identity(receipt_id, "Universal review receipt id")
        elif receipt_id is not None:
            raise DeadlineError("Ordinary mutation resolution cannot consume a receipt")
        evidence = self._nonempty_text(evidence, "Random review evidence")
        resolved_at = self._now(now)
        self._begin()
        try:
            self._bind_lineage(lineage_id)
            cycle = self.connection.execute(
                """
                SELECT * FROM random_mutation_cycles
                WHERE lineage_id = ? AND cycle_number = ?
                """,
                (lineage_id, cycle_number),
            ).fetchone()
            if cycle is None:
                raise DeadlineError(
                    f"Unknown random mutation cycle: {lineage_id}/{cycle_number}"
                )
            if cycle["due_task_id"] is None:
                raise DeadlineError("Random mutation cycle is not due")
            if component == "universal" and not bool(cycle["universal_required"]):
                raise DeadlineError("Random mutation cycle has no universal component")
            if component == "universal":
                receipt = self.connection.execute(
                    """
                    SELECT * FROM universal_review_receipts
                    WHERE receipt_id = ? AND lineage_id = ? AND cycle_number = ?
                    """,
                    (receipt_id, lineage_id, cycle_number),
                ).fetchone()
                if receipt is None:
                    raise DeadlineError(
                        "Universal review receipt does not match this lineage and cycle"
                    )
                try:
                    changed_paths = json.loads(str(receipt["changed_paths"]))
                except json.JSONDecodeError as error:
                    raise DeadlineError(
                        "Universal review receipt has invalid changed-path evidence"
                    ) from error
                receipt_contract = {
                    "lineage_id": lineage_id,
                    "cycle_number": cycle_number,
                    "candidate_digest": receipt["candidate_digest"],
                    "changed_paths": changed_paths,
                    "interval_windows": receipt["interval_windows"],
                    "selected_lane": receipt["selected_lane"],
                    "reviewer_model": receipt["reviewer_model"],
                    "reviewer_effort": receipt["reviewer_effort"],
                    "capability_roster_digest": receipt[
                        "capability_roster_digest"
                    ],
                }
                expected_receipt_id = hashlib.sha256(
                    json.dumps(
                        receipt_contract,
                        sort_keys=True,
                        separators=(",", ":"),
                    ).encode("utf-8")
                ).hexdigest()
                if (
                    receipt["interval_windows"] != UNIVERSAL_RANDOM_INTERVAL
                    or receipt["selected_lane"] != "DFS.md"
                    or receipt["reviewer_model"] != "gpt-5.6-sol"
                    or receipt["reviewer_effort"] != "ultra"
                    or receipt["capability_roster_digest"]
                        != cycle["universal_capability_roster_digest"]
                    or not isinstance(changed_paths, list)
                    or not changed_paths
                    or not all(
                        isinstance(path, str) and path.strip()
                        for path in changed_paths
                    )
                    or len(str(receipt["candidate_digest"])) != 64
                    or receipt_id != expected_receipt_id
                ):
                    raise DeadlineError(
                        "Universal review receipt does not prove the required trigger, "
                        "reviewer, and changed candidate"
                    )
                prior_use = self.connection.execute(
                    """
                    SELECT lineage_id, cycle_number FROM random_mutation_cycles
                    WHERE universal_receipt_id = ?
                    """,
                    (receipt_id,),
                ).fetchone()
                if prior_use is not None:
                    raise DeadlineError(
                        "Universal review receipt has already been consumed"
                    )
            column = (
                "ordinary_resolution_evidence"
                if component == "ordinary"
                else "universal_resolution_evidence"
            )
            existing_evidence = cycle[column]
            if component == "universal" and existing_evidence is not None:
                raise DeadlineError(
                    "Universal mutation component is already resolved; receipt replay is forbidden"
                )
            if existing_evidence is None:
                if component == "universal":
                    self.connection.execute(
                        """
                        UPDATE random_mutation_cycles
                        SET universal_resolution_evidence = ?,
                            universal_receipt_id = ?
                        WHERE lineage_id = ? AND cycle_number = ?
                        """,
                        (evidence, receipt_id, lineage_id, cycle_number),
                    )
                else:
                    self.connection.execute(
                        f"""
                        UPDATE random_mutation_cycles SET {column} = ?
                        WHERE lineage_id = ? AND cycle_number = ?
                        """,
                        (evidence, lineage_id, cycle_number),
                    )
                recorded = True
            else:
                if existing_evidence != evidence:
                    raise DeadlineError(
                        "Random mutation component is immutable once recorded"
                    )
                recorded = False
            refreshed = self.connection.execute(
                """
                SELECT * FROM random_mutation_cycles
                WHERE lineage_id = ? AND cycle_number = ?
                """,
                (lineage_id, cycle_number),
            ).fetchone()
            if refreshed is None:
                raise DeadlineError("Failed to read random mutation cycle")
            all_resolved = (
                refreshed["ordinary_resolution_evidence"] is not None
                and (
                    not bool(refreshed["universal_required"])
                    or refreshed["universal_resolution_evidence"] is not None
                )
            )
            restart: sqlite3.Row | None = None
            if all_resolved:
                if refreshed["resolution_evidence"] is None:
                    combined = json.dumps(
                        {
                            "ordinary": refreshed["ordinary_resolution_evidence"],
                            "universal": refreshed["universal_resolution_evidence"],
                        },
                        sort_keys=True,
                    )
                    restart, _ = self._request_coordinator_restart(
                        lineage_id,
                        f"random mutation cycle {cycle_number} resolved",
                        resolved_at,
                    )
                    self.connection.execute(
                        """
                        UPDATE random_mutation_cycles
                        SET resolution_evidence = ?, restart_generation = ?
                        WHERE lineage_id = ? AND cycle_number = ?
                        """,
                        (
                            combined,
                            restart["generation"],
                            lineage_id,
                            cycle_number,
                        ),
                    )
                elif refreshed["restart_generation"] is not None:
                    restart = self.connection.execute(
                        """
                        SELECT * FROM coordinator_restart_requests
                        WHERE lineage_id = ? AND generation = ?
                        """,
                        (lineage_id, refreshed["restart_generation"]),
                    ).fetchone()
            next_cycle = (
                self._ensure_random_cycle(lineage_id)
                if all_resolved
                else self._latest_random_cycle(lineage_id)
            )
            if next_cycle is None:
                raise DeadlineError("Failed to read next random mutation cycle")
            result = {
                "recorded": recorded,
                "cycle_number": cycle_number,
                "selected_lane": cycle["selected_lane"],
                "component": component,
                "resolution_evidence": evidence,
                "universal_receipt_id": (
                    receipt_id if component == "universal" else None
                ),
                "random_mutation": self._random_cycle_result(next_cycle),
                "coordinator_restart": (
                    self._coordinator_restart_result(restart)
                    if restart is not None
                    else None
                ),
            }
            self.connection.commit()
            return result
        except Exception:
            self.connection.rollback()
            raise


def _state_path_for_watcher(state_path: str | Path) -> str:
    if str(state_path) == ":memory:":
        raise DeadlineError("CLI start requires a persistent SQLite state path")
    return str(Path(state_path).expanduser().resolve())


def spawn_watcher(state_path: str | Path, lineage_id: str, claim_id: str) -> None:
    command = [
        sys.executable,
        str(Path(__file__).resolve()),
        "--state",
        _state_path_for_watcher(state_path),
        "watch",
        "--lineage",
        lineage_id,
        "--claim",
        claim_id,
    ]
    options: dict[str, Any] = {
        "stdin": subprocess.DEVNULL,
        "stdout": subprocess.DEVNULL,
        "stderr": subprocess.DEVNULL,
        "close_fds": True,
    }
    if os.name == "nt":
        options["creationflags"] = (
            subprocess.CREATE_NEW_PROCESS_GROUP | subprocess.DETACHED_PROCESS
        )
    else:
        options["start_new_session"] = True
    subprocess.Popen(command, **options)


def watch_claim(
    state_path: str | Path, lineage_id: str, claim_id: str
) -> dict[str, Any]:
    with DeadlineHarness(state_path) as harness:
        claim = harness._claim(lineage_id, claim_id)
        deadline = float(claim["deadline_at"])
    while True:
        remaining = deadline - time.time()
        if remaining <= 0:
            break
        time.sleep(remaining)
    with DeadlineHarness(state_path) as harness:
        return harness.expire_claim(lineage_id, claim_id)


def watch_task(
    state_path: str | Path, lineage_id: str, task_id: str
) -> dict[str, Any]:
    """Compatibility alias for a v1 task-keyed watcher."""

    with DeadlineHarness(state_path) as harness:
        task = harness._task(lineage_id, task_id)
        claim_id = str(task["claim_id"])
    return watch_claim(state_path, lineage_id, claim_id)


def quiet_result(result: dict[str, Any]) -> dict[str, Any]:
    """Remove stored narrative and bookkeeping from an ordinary CLI result."""
    hidden = {
        "acknowledged_at", "checked_at", "claimed_at", "completed_at",
        "completed_terminal_windows", "completion_evidence", "cumulative_after",
        "cumulative_before", "cumulative_miss_units", "due_after_terminal_windows",
        "estimate_seconds", "evidence", "incidents", "integrity_reason",
        "interval_windows", "long_detail", "reason", "recorded_at", "reported_at",
        "requested_at", "resolution_evidence", "reviewed_at", "run_id", "started_at",
        "worker_finding",
    }

    def project(name: str, value: Any) -> Any:
        if value is None or value == []:
            return None
        if name == "random_mutation" and isinstance(value, dict) and not value["due"]:
            return None
        if name == "coordinator_restart" and isinstance(value, dict) and not value["pending"]:
            return None
        if isinstance(value, dict):
            return {
                key: projected
                for key, item in value.items()
                if key not in hidden
                and (projected := project(key, item)) is not None
            }
        if isinstance(value, list):
            return [project(name, item) for item in value]
        return value

    return project("result", result)


def add_task_identity_flags(command: argparse.ArgumentParser) -> None:
    command.add_argument("--state", dest="command_state")
    command.add_argument("--lineage", required=True)
    command.add_argument("--task", required=True)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--state", help="SQLite state path")
    commands = parser.add_subparsers(dest="command", required=True)

    start = commands.add_parser("start", help="Start a task once")
    add_task_identity_flags(start)
    start.add_argument("--claim", required=True)
    start.add_argument(
        "--estimate-seconds",
        required=True,
        type=float,
        help=(
            "Evidence-derived estimate for this attempt; the first attempt also "
            "uses it to arm each fresh immutable claim deadline generation"
        ),
    )
    start.add_argument("--phase", choices=("exploration", "closure"))
    start.add_argument("--gap")

    status = commands.add_parser(
        "status",
        help="Read one exact task with long evidence and record a due miss",
    )
    add_task_identity_flags(status)

    expire = commands.add_parser("expire", help="Record a due miss once")
    add_task_identity_flags(expire)

    deadline_miss = commands.add_parser(
        "deadline-miss",
        help="Record the current item deadline after its wall-clock expiry",
    )
    deadline_miss.add_argument("--state", dest="command_state")
    deadline_miss.add_argument("--lineage", required=True)
    deadline_miss.add_argument("--claim", required=True)

    complete = commands.add_parser(
        "complete", help="Terminalize one attempt with completion evidence"
    )
    add_task_identity_flags(complete)
    complete.add_argument("--evidence", required=True)
    complete.add_argument(
        "--receipt",
        help="Exact durable worker result receipt permitting completion",
    )

    finding = commands.add_parser(
        "finding", help="Record one terminal worker blocker or unexpected result"
    )
    add_task_identity_flags(finding)
    finding.add_argument("--kind", choices=("blocker", "unexpected"), required=True)
    finding.add_argument("--short-verdict")
    finding.add_argument("--evidence", required=True)
    finding.add_argument(
        "--receipt",
        help="Exact durable worker result receipt permitting the finding",
    )

    diagnose = commands.add_parser(
        "diagnose",
        help="Attach one immutable short verdict and long diagnosis to an incident",
    )
    add_task_identity_flags(diagnose)
    diagnose.add_argument("--kind", choices=INCIDENT_KINDS, required=True)
    diagnose.add_argument("--short-verdict", required=True)
    diagnose.add_argument("--diagnosis", required=True)

    breach = commands.add_parser("breach", help="Record an integrity breach once")
    add_task_identity_flags(breach)
    breach.add_argument("--reason", required=True)

    watch = commands.add_parser("watch", help="Wait for and expire one claim clock")
    watch.add_argument("--state", dest="command_state")
    watch.add_argument("--lineage", required=True)
    watch.add_argument("--claim", required=True)

    transition_closure = commands.add_parser(
        "transition-closure", help="Freeze one finite closure contract"
    )
    transition_closure.add_argument("--state", dest="command_state")
    transition_closure.add_argument("--lineage", required=True)
    transition_closure.add_argument("--claim", required=True)
    transition_closure.add_argument("--basis-task", required=True)
    transition_closure.add_argument("--outcome", required=True)
    transition_closure.add_argument("--evidence", required=True)
    closure_gap_source = transition_closure.add_mutually_exclusive_group(
        required=True
    )
    closure_gap_source.add_argument("--remaining-gap")
    closure_gap_source.add_argument(
        "--gap",
        dest="named_gaps",
        action="append",
        help="Repeat ID::description to freeze the finite closure frontier",
    )

    close_gap = commands.add_parser(
        "close-gap", help="Accept one named closure gap"
    )
    close_gap.add_argument("--state", dest="command_state")
    close_gap.add_argument("--lineage", required=True)
    close_gap.add_argument("--claim", required=True)
    close_gap.add_argument("--gap", required=True)
    close_gap.add_argument("--task", required=True)
    close_gap.add_argument("--evidence", required=True)

    revise_gap = commands.add_parser(
        "revise-gap", help="Append an evidence-bound replacement gap route"
    )
    revise_gap.add_argument("--state", dest="command_state")
    revise_gap.add_argument("--lineage", required=True)
    revise_gap.add_argument("--claim", required=True)
    revise_gap.add_argument("--gap", required=True)
    revise_gap.add_argument("--basis-task", required=True)
    revise_gap.add_argument("--description", required=True)
    revise_gap.add_argument("--proof-route", required=True)
    revise_gap.add_argument("--route-kind", choices=("executable", "owner_wait"), default="executable")
    revise_gap.add_argument("--owner-decision", help="Exact received owner decision authorizing release of the current wait")

    reopen = commands.add_parser(
        "reopen-exploration", help="Reopen from a closure finding"
    )
    reopen.add_argument("--state", dest="command_state")
    reopen.add_argument("--lineage", required=True)
    reopen.add_argument("--claim", required=True)
    reopen.add_argument("--basis-task", required=True)
    reopen.add_argument("--contradicted-premise", required=True)

    accept_claim = commands.add_parser(
        "accept-claim", help="Accept a claim through one completed closure attempt"
    )
    accept_claim.add_argument("--state", dest="command_state")
    accept_claim.add_argument("--lineage", required=True)
    accept_claim.add_argument("--claim", required=True)
    accept_claim.add_argument("--task", required=True)
    accept_claim.add_argument("--evidence", required=True)

    abandon = commands.add_parser(
        "abandon-attempt", help="Terminalize one replaced worker attempt"
    )
    add_task_identity_flags(abandon)
    abandon.add_argument("--reason", required=True)
    abandon.add_argument(
        "--receipt",
        help="Exact durable worker result receipt permitting abandonment",
    )

    checkpoint_worker = commands.add_parser(
        "checkpoint-worker", help="Record durable progress for the owning worker"
    )
    add_task_identity_flags(checkpoint_worker)
    checkpoint_worker.add_argument("--worker", required=True)
    checkpoint_worker.add_argument("--kind", required=True)
    checkpoint_worker.add_argument("--evidence", required=True)

    worker_receipt = commands.add_parser(
        "record-worker-receipt",
        help="Validate and persist one compact worker result or continuation receipt",
    )
    add_task_identity_flags(worker_receipt)
    worker_receipt.add_argument("--worker", required=True)
    worker_receipt.add_argument("--receipt-file", required=True)
    worker_receipt.add_argument("--workspace", required=True)

    receipt_query = commands.add_parser(
        "worker-receipts",
        help="Query compact worker receipts by exact continuation or evidence identity",
    )
    receipt_query.add_argument("--state", dest="command_state")
    receipt_query.add_argument("--lineage", required=True)
    for flag, destination in (
        ("--receipt", "receipt_id"),
        ("--task", "task_id"),
        ("--claim", "claim_id"),
        ("--worker", "worker_id"),
        ("--run", "run_id"),
        ("--scenario", "scenario_id"),
        ("--binding", "binding_id"),
        ("--disposition", "disposition"),
        ("--verdict", "verdict"),
        ("--first-divergence-class", "first_divergence_class"),
        ("--event-type", "event_type"),
        ("--evidence-class", "evidence_class"),
        ("--actor", "actor_id"),
        ("--action", "action_id"),
        ("--native-receipt", "native_receipt_id"),
    ):
        receipt_query.add_argument(flag, dest=destination)
    receipt_query.add_argument("--latest", action="store_true")
    receipt_query.add_argument(
        "--full", action="store_true",
        help="Explicitly retrieve complete matching receipts instead of compact projections",
    )

    release_worker = commands.add_parser(
        "release-worker", help="Release one durable worker ownership claim"
    )
    add_task_identity_flags(release_worker)
    release_worker.add_argument("--worker", required=True)
    release_worker.add_argument("--reason", required=True)
    diagnose_claim = commands.add_parser(
        "diagnose-claim-deadline", help="Diagnose one exact claim deadline miss"
    )
    diagnose_claim.add_argument("--state", dest="command_state")
    diagnose_claim.add_argument("--lineage", required=True)
    diagnose_claim.add_argument("--claim", required=True)
    diagnose_claim.add_argument("--short-verdict", required=True)
    diagnose_claim.add_argument("--diagnosis", required=True)

    resolve_deadline = commands.add_parser(
        "resolve-deadline-mutation", help="Resolve one required deadline mutation component"
    )
    resolve_deadline.add_argument("--state", dest="command_state")
    resolve_deadline.add_argument("--lineage", required=True)
    resolve_deadline.add_argument("--claim", required=True)
    resolve_deadline.add_argument(
        "--component", choices=("micro", "macro"), required=True
    )
    resolve_deadline.add_argument("--evidence", required=True)
    resolve_deadline.add_argument(
        "--receipt", help="Guard-issued method receipt; required for macro"
    )
    resolve_deadline.add_argument(
        "--no-change-required",
        action="store_true",
        help=(
            "Resolve a reviewed macro proposal that independent evidence shows "
            "requires no method change"
        ),
    )

    resolve_integrity = commands.add_parser(
        "resolve-integrity-mutation",
        help="Resolve one exact integrity mutation component",
    )
    add_task_identity_flags(resolve_integrity)
    resolve_integrity.add_argument(
        "--component", choices=("micro", "macro"), required=True
    )
    resolve_integrity.add_argument("--evidence", required=True)
    resolve_integrity.add_argument(
        "--receipt", help="Guard-issued method receipt; required for macro"
    )

    resolve_clock = commands.add_parser(
        "resolve-clock-migration", help="Resolve one ambiguous v1 claim clock"
    )
    resolve_clock.add_argument("--state", dest="command_state")
    resolve_clock.add_argument("--lineage", required=True)
    resolve_clock.add_argument("--claim", required=True)
    resolve_clock.add_argument("--reason", required=True)
    clock_source = resolve_clock.add_mutually_exclusive_group(required=True)
    clock_source.add_argument("--source-task")
    clock_source.add_argument("--estimate-seconds", type=float)

    migration_details = commands.add_parser(
        "clock-migration-details",
        help="Show the exact immutable choices for one unresolved v1 clock",
    )
    migration_details.add_argument("--state", dest="command_state")
    migration_details.add_argument("--lineage", required=True)
    migration_details.add_argument("--claim", required=True)

    invalidation_details = commands.add_parser(
        "claim-invalidation-details",
        help="Show the durable evidence for one currently invalidated acceptance",
    )
    invalidation_details.add_argument("--state", dest="command_state")
    invalidation_details.add_argument("--lineage", required=True)
    invalidation_details.add_argument("--claim", required=True)

    list_command = commands.add_parser(
        "list", help="Show current work and pending gates"
    )
    list_command.add_argument("--state", dest="command_state")

    startup_view = commands.add_parser(
        "startup-view",
        help="Show current work, pending gates, and up to ten short prior verdicts",
    )
    startup_view.add_argument("--state", dest="command_state")

    resolve_random = commands.add_parser(
        "resolve-random-mutation",
        help="Record one guarded random improvement review",
    )
    resolve_random.add_argument("--state", dest="command_state")
    resolve_random.add_argument("--lineage", required=True)
    resolve_random.add_argument("--cycle", required=True, type=int)
    resolve_random.add_argument(
        "--component", choices=("ordinary", "universal"), default="ordinary"
    )
    resolve_random.add_argument(
        "--receipt",
        help="Validated universal-review receipt id; required for universal",
    )
    resolve_random.add_argument("--evidence", required=True)

    request_restart = commands.add_parser(
        "request-restart",
        help="Request one durable fresh-coordinator handover",
    )
    request_restart.add_argument("--state", dest="command_state")
    request_restart.add_argument("--lineage", required=True)
    request_restart.add_argument("--reason", required=True)

    acknowledge_restart = commands.add_parser(
        "ack-restart",
        help="Acknowledge one live fresh coordinator",
    )
    acknowledge_restart.add_argument("--state", dest="command_state")
    acknowledge_restart.add_argument("--lineage", required=True)
    acknowledge_restart.add_argument("--generation", required=True, type=int)
    acknowledge_restart.add_argument("--run-id", required=True)

    release_restart = commands.add_parser(
        "release-restart-claim",
        help="Release one dead unacknowledged successor claim",
    )
    release_restart.add_argument("--state", dest="command_state")
    release_restart.add_argument("--lineage", required=True)
    release_restart.add_argument("--generation", required=True, type=int)
    release_restart.add_argument("--run-id", required=True)
    return parser


def main(argv: list[str] | None = None) -> int:
    arguments = build_parser().parse_args(argv)
    try:
        state_path = arguments.state or arguments.command_state
        if state_path is None:
            raise DeadlineError("A SQLite --state path is required")
        if (
            arguments.state is not None
            and arguments.command_state is not None
            and arguments.state != arguments.command_state
        ):
            raise DeadlineError("Conflicting --state paths")
        if arguments.command == "watch":
            result = watch_claim(state_path, arguments.lineage, arguments.claim)
        else:
            with DeadlineHarness(state_path) as harness:
                if arguments.command == "start":
                    claim_exists = harness.connection.execute(
                        "SELECT 1 FROM claim_clocks "
                        "WHERE lineage_id = ? AND claim_id = ?",
                        (arguments.lineage, arguments.claim),
                    ).fetchone()
                    claim_estimate = arguments.estimate_seconds
                    if claim_exists is not None:
                        claim = harness._claim(arguments.lineage, arguments.claim)
                        incident = harness._claim_deadline_incident(
                            arguments.lineage,
                            arguments.claim,
                            int(claim["deadline_generation"]),
                        )
                        may_advance_generation = (
                            claim["retired_at"] is not None
                            or (
                                incident is not None
                                and not harness._deadline_mutation_pending(
                                    arguments.lineage, arguments.claim
                                )
                            )
                        )
                        if not may_advance_generation:
                            claim_estimate = float(claim["estimate_seconds"])
                    result = harness.start_task(
                        arguments.lineage,
                        arguments.task,
                        arguments.claim,
                        claim_estimate,
                        attempt_estimate_seconds=arguments.estimate_seconds,
                        phase=arguments.phase,
                        gap_id=arguments.gap,
                    )
                elif arguments.command == "status":
                    result = harness.status_task(arguments.lineage, arguments.task)
                elif arguments.command == "expire":
                    result = harness.expire_task(arguments.lineage, arguments.task)
                elif arguments.command == "deadline-miss":
                    result = harness.miss_claim_deadline(
                        arguments.lineage, arguments.claim
                    )
                elif arguments.command == "list":
                    result = harness.coordinator_view()
                elif arguments.command == "startup-view":
                    result = harness.coordinator_view(include_recent_verdicts=True)
                elif arguments.command == "complete":
                    result = harness.complete_task(
                        arguments.lineage, arguments.task, arguments.evidence,
                        receipt_id=arguments.receipt,
                    )
                elif arguments.command == "transition-closure":
                    named_gaps = None
                    if arguments.named_gaps is not None:
                        named_gaps = []
                        for raw_gap in arguments.named_gaps:
                            gap_id, separator, description = raw_gap.partition("::")
                            if not separator:
                                raise DeadlineError(
                                    "Named closure gaps use ID::description"
                                )
                            named_gaps.append(
                                (gap_id, description, arguments.evidence)
                            )
                    result = harness.transition_claim_to_closure(
                        arguments.lineage,
                        arguments.claim,
                        arguments.basis_task,
                        arguments.outcome,
                        arguments.evidence,
                        arguments.remaining_gap,
                        gaps=named_gaps,
                    )
                elif arguments.command == "close-gap":
                    result = harness.close_closure_gap(
                        arguments.lineage,
                        arguments.claim,
                        arguments.gap,
                        arguments.task,
                        arguments.evidence,
                    )
                elif arguments.command == "revise-gap":
                    result = harness.revise_closure_gap(
                        arguments.lineage,
                        arguments.claim,
                        arguments.gap,
                        arguments.basis_task,
                        arguments.description,
                        arguments.proof_route,
                        route_kind=arguments.route_kind,
                        owner_decision=arguments.owner_decision,
                    )
                elif arguments.command == "reopen-exploration":
                    result = harness.reopen_claim_exploration(
                        arguments.lineage,
                        arguments.claim,
                        arguments.basis_task,
                        arguments.contradicted_premise,
                    )
                elif arguments.command == "accept-claim":
                    result = harness.accept_claim(
                        arguments.lineage,
                        arguments.claim,
                        arguments.task,
                        arguments.evidence,
                    )
                elif arguments.command == "abandon-attempt":
                    result = harness.abandon_attempt(
                        arguments.lineage, arguments.task, arguments.reason,
                        receipt_id=arguments.receipt,
                    )
                elif arguments.command == "checkpoint-worker":
                    result = harness.checkpoint_worker(
                        arguments.lineage, arguments.task, arguments.worker,
                        arguments.kind, arguments.evidence,
                    )
                elif arguments.command == "record-worker-receipt":
                    try:
                        receipt = json.loads(
                            Path(arguments.receipt_file).read_text(encoding="utf-8")
                        )
                    except (OSError, UnicodeError, json.JSONDecodeError) as error:
                        raise DeadlineError(
                            f"Worker result receipt file is unreadable: {error}"
                        ) from error
                    result = harness.record_worker_result_receipt(
                        arguments.lineage,
                        arguments.task,
                        arguments.worker,
                        receipt,
                        workspace=Path(arguments.workspace),
                    )
                elif arguments.command == "worker-receipts":
                    filters = {
                        name: str(value)
                        for name, value in vars(arguments).items()
                        if name in {
                            "receipt_id", "task_id", "claim_id", "worker_id",
                            "run_id", "scenario_id", "binding_id", "disposition",
                            "verdict", "first_divergence_class", "event_type",
                            "evidence_class", "actor_id", "action_id",
                            "native_receipt_id",
                        }
                        and value is not None
                    }
                    result = harness.query_worker_result_receipts(
                        arguments.lineage,
                        filters=filters,
                        latest=bool(arguments.latest),
                        full=bool(arguments.full),
                    )
                elif arguments.command == "release-worker":
                    result = harness.release_worker_claim(
                        arguments.lineage, arguments.task, arguments.worker,
                        arguments.reason,
                    )
                elif arguments.command == "finding":
                    result = harness.report_worker_finding(
                        arguments.lineage,
                        arguments.task,
                        arguments.kind,
                        arguments.evidence,
                        receipt_id=arguments.receipt,
                        short_verdict=arguments.short_verdict,
                    )
                elif arguments.command == "diagnose":
                    result = harness.diagnose_incident(
                        arguments.lineage,
                        arguments.task,
                        arguments.kind,
                        arguments.short_verdict,
                        arguments.diagnosis,
                    )
                elif arguments.command == "diagnose-claim-deadline":
                    result = harness.diagnose_claim_deadline(
                        arguments.lineage,
                        arguments.claim,
                        arguments.short_verdict,
                        arguments.diagnosis,
                    )
                elif arguments.command == "resolve-deadline-mutation":
                    result = harness.resolve_deadline_mutation(
                        arguments.lineage,
                        arguments.claim,
                        arguments.component,
                        arguments.evidence,
                        receipt_id=arguments.receipt,
                        no_change_required=arguments.no_change_required,
                    )
                elif arguments.command == "resolve-integrity-mutation":
                    result = harness.resolve_integrity_mutation(
                        arguments.lineage,
                        arguments.task,
                        arguments.component,
                        arguments.evidence,
                        receipt_id=arguments.receipt,
                    )
                elif arguments.command == "resolve-clock-migration":
                    result = harness.resolve_claim_clock_migration(
                        arguments.lineage,
                        arguments.claim,
                        arguments.reason,
                        source_task_id=arguments.source_task,
                        estimate_seconds=arguments.estimate_seconds,
                    )
                elif arguments.command == "clock-migration-details":
                    result = harness.claim_clock_migration_details(
                        arguments.lineage,
                        arguments.claim,
                    )
                elif arguments.command == "claim-invalidation-details":
                    result = harness.claim_invalidation_details(
                        arguments.lineage,
                        arguments.claim,
                    )
                elif arguments.command == "resolve-random-mutation":
                    result = harness.resolve_random_mutation(
                        arguments.lineage,
                        arguments.cycle,
                        arguments.evidence,
                        component=arguments.component,
                        receipt_id=arguments.receipt,
                    )
                elif arguments.command == "request-restart":
                    result = harness.request_coordinator_restart(
                        arguments.lineage,
                        arguments.reason,
                    )
                elif arguments.command == "ack-restart":
                    result = harness.acknowledge_coordinator_restart(
                        arguments.lineage,
                        arguments.generation,
                        arguments.run_id,
                    )
                elif arguments.command == "release-restart-claim":
                    result = harness.release_coordinator_restart_claim(
                        arguments.lineage,
                        arguments.generation,
                        arguments.run_id,
                    )
                else:
                    result = harness.record_integrity_breach(
                        arguments.lineage, arguments.task, arguments.reason
                    )
            if arguments.command == "start" and result["claim_created"]:
                spawn_watcher(state_path, arguments.lineage, arguments.claim)
            if arguments.command == "resolve-clock-migration":
                spawn_watcher(state_path, arguments.lineage, arguments.claim)
            if arguments.command not in {
                "status",
                "list",
                "startup-view",
                "clock-migration-details",
                "claim-invalidation-details",
                "record-worker-receipt",
                "worker-receipts",
            }:
                result = quiet_result(result)
        print(json.dumps(result, sort_keys=True))
        return 0
    except (DeadlineError, sqlite3.Error, OSError) as error:
        print(json.dumps({"error": str(error)}), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
