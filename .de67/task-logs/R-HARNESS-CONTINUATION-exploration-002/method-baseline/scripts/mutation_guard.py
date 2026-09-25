#!/usr/bin/env python3
"""Validate DE-67-3 Markdown mutations without constraining their prose."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import sqlite3
import sys
import tempfile
import time
from typing import NamedTuple

SCRIPT_ROOT = str(Path(__file__).resolve().parent)
if SCRIPT_ROOT not in sys.path:
    sys.path.insert(0, SCRIPT_ROOT)
from specification import FS_FILE, SpecificationError, resolve_path


TASK_GUIDELINES = "test-and-task-guidelines.md"
GUIDELINE_FILES = (TASK_GUIDELINES,)
# Historical SQLite lane and receipt label, never a filesystem path.
DFS_FILE = "DFS.md"
MUTATION_LEDGER = "mutation-suggestions.md"
RANDOM_MUTATION_LANES = (*GUIDELINE_FILES, DFS_FILE)
# Previously stored lane labels are metadata, not files to load.
LEGACY_RANDOM_MUTATION_LANES = ("orchestrator-guidelines.md",)
INCIDENT_KINDS = ("deadline_miss", "integrity_breach")
SKILL_ROOT = Path(__file__).resolve().parents[1]
CANONICAL_GUIDELINES_ROOT = SKILL_ROOT / "assets" / "environment"

ATX_HEADING = re.compile(r"^[ \t]*#{1,6}[ \t]+\S.*$")
FENCE = re.compile(r"^[ \t]*(?P<marker>`{3,}|~{3,})")
CLAIM_ID_PATTERN = r"(?:R-|DE67-MAINT-)[A-Za-z0-9._-]+"
CLAIM_REFERENCE = rf"{CLAIM_ID_PATTERN}[ \t]+—[ \t]+\S.*?"
CLAIM_ID = re.compile(rf"^{CLAIM_ID_PATTERN}$")
ACTIVE_ITEM = re.compile(rf"^- \[ \] (?P<reference>{CLAIM_REFERENCE})[ \t]*$")
DFS_SLICE_ID_PATTERN = rf"{CLAIM_ID_PATTERN}-S[0-9]{{3,}}"
DFS_SLICE_ID = re.compile(rf"^{DFS_SLICE_ID_PATTERN}$")
DFS_SLICE_MARKER = re.compile(
    rf"^<!-- DE67:DFS-SLICE:(?P<kind>BEGIN|END) "
    rf"id=(?P<id>{DFS_SLICE_ID_PATTERN}) "
    rf"claim=(?P<claim>{CLAIM_ID_PATTERN}) -->$"
)
DFS_SLICE_MARKER_TOKEN = "DE67:DFS-SLICE:"
DFS_SLICE_LEDGER_TOKEN = "DFS slices:"
DFS_SLICE_LEDGER = re.compile(
    rf"^[ \t]+- DFS slices:[ \t]+"
    rf"(?P<ids>`{DFS_SLICE_ID_PATTERN}`(?:,[ \t]*`{DFS_SLICE_ID_PATTERN}`)*)"
    rf"[ \t]*$"
)
RED_CLAIM = re.compile(
    rf"^(?P<lead>- )\[ \] 🔴 (?P<label>{CLAIM_REFERENCE})(?P<trailing>[ \t]*)$"
)
STABLE_CLAIM = re.compile(
    rf"^- \[(?P<status>[ xX])\](?P<red> 🔴)? "
    rf"(?P<label>{CLAIM_REFERENCE})(?P<trailing>[ \t]*)$"
)
PROTECTED_DFS_SECTIONS = (
    "## Functional contract",
    "## Project language and terminology",
)
METHOD_REQUIRED_FILES = (
    "SKILL.md",
    "scripts/deadline_harness.py",
    "scripts/mutation_guard.py",
)
NORMAL_METHOD_PROTECTED_FILES = (
    "scripts/deadline_harness.py",
    "scripts/mutation_guard.py",
    "tests/test_deadline_harness.py",
    "tests/test_mutation_guard.py",
)
NORMAL_METHOD_MUTABLE_ROOTS = (
    "SKILL.md",
    "agents/",
    "assets/environment/",
    "scripts/",
    "tests/",
)
IGNORED_METHOD_PARTS = {".git", "__pycache__", ".pytest_cache"}


class GuardError(RuntimeError):
    """The proposed Markdown state breaks a frozen DE-67 invariant."""


class DfsSlice(NamedTuple):
    """One validated durable FS context slice."""

    slice_id: str
    claim_id: str
    begin_index: int
    end_index: int
    logical_start: int
    logical_end: int
    content: str


def read_markdown(path: Path) -> str:
    if not path.is_file():
        raise GuardError(f"Missing Markdown file: {path}")
    return path.read_text(encoding="utf-8")


def _method_files(root: Path) -> dict[str, bytes]:
    """Read a candidate skill tree without following links or Git/runtime debris."""

    if not root.is_dir():
        raise GuardError(f"Method candidate root does not exist: {root}")
    result: dict[str, bytes] = {}
    for path in sorted(root.rglob("*")):
        relative = path.relative_to(root)
        if any(part in IGNORED_METHOD_PARTS for part in relative.parts):
            continue
        if path.is_symlink():
            raise GuardError(f"Method candidates cannot contain symlinks: {relative.as_posix()}")
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


def method_tree_digest(root: Path = SKILL_ROOT) -> str:
    return _files_digest(_method_files(root))


def protected_method_digest(root: Path = SKILL_ROOT) -> str:
    files = _method_files(root)
    protected = {
        relative: files[relative]
        for relative in NORMAL_METHOD_PROTECTED_FILES
        if relative in files
    }
    if len(protected) != len(NORMAL_METHOD_PROTECTED_FILES):
        missing = sorted(set(NORMAL_METHOD_PROTECTED_FILES) - set(protected))
        raise GuardError(
            "Active method tree is missing protected files: " + ", ".join(missing)
        )
    return _files_digest(protected)


def normal_method_candidate_snapshot(
    guideline_candidate_root: Path,
    method_candidate_root: Path | None,
) -> tuple[str, str, str, tuple[str, ...]]:
    """Bind a normal candidate to the active tree and its hard protected files."""

    live = _method_files(SKILL_ROOT)
    candidate = (
        dict(live)
        if method_candidate_root is None
        else _method_files(method_candidate_root)
    )
    overlays = {
        f"assets/environment/{name}": (guideline_candidate_root / name).read_bytes()
        for name in GUIDELINE_FILES
    }
    if method_candidate_root is None:
        candidate.update(overlays)
    else:
        mismatched = [
            relative
            for relative, content in overlays.items()
            if candidate.get(relative) != content
        ]
        if mismatched:
            raise GuardError(
                "Method candidate must contain the exact guarded guideline candidates: "
                + ", ".join(sorted(mismatched))
            )

    protected = [
        relative
        for relative in NORMAL_METHOD_PROTECTED_FILES
        if candidate.get(relative) != live.get(relative)
    ]
    if protected:
        raise GuardError(
            "Normal method mutation cannot change the active hard clock/guard surface: "
            + ", ".join(protected)
        )
    changed_paths = tuple(
        sorted(
            relative
            for relative in set(live) | set(candidate)
            if live.get(relative) != candidate.get(relative)
        )
    )
    if not changed_paths:
        raise GuardError("Deadline macro mutation candidate makes no live-tree change")
    outside = [
        relative for relative in changed_paths if not _normal_method_path_allowed(relative)
    ]
    if outside:
        raise GuardError(
            "Normal method mutation changed a path outside its broad mutable surface: "
            + ", ".join(outside)
        )
    return (
        _files_digest(candidate),
        protected_method_digest(),
        _files_digest(live),
        changed_paths,
    )


def _normal_method_path_allowed(relative: str) -> bool:
    return any(
        relative == root or (root.endswith("/") and relative.startswith(root))
        for root in NORMAL_METHOD_MUTABLE_ROOTS
    )


def validate_method_mutation(
    baseline_root: Path,
    candidate_root: Path,
    *,
    universal: bool,
    require_change: bool = True,
) -> tuple[str, ...]:
    """Validate a broad method candidate while keeping the normal hard kernel small."""

    supplied_baseline = _method_files(baseline_root)
    candidate = _method_files(candidate_root)
    for relative in METHOD_REQUIRED_FILES:
        if relative not in candidate:
            raise GuardError(f"Method candidate is missing required file: {relative}")

    if universal:
        baseline = supplied_baseline
    else:
        baseline = _method_files(SKILL_ROOT)
        baseline_mismatch = [
            relative
            for relative in NORMAL_METHOD_PROTECTED_FILES
            if supplied_baseline.get(relative) != baseline.get(relative)
        ]
        if baseline_mismatch:
            raise GuardError(
                "Normal method baseline is not the active live protected surface: "
                + ", ".join(baseline_mismatch)
            )

    changed = tuple(
        sorted(
            relative
            for relative in set(baseline) | set(candidate)
            if baseline.get(relative) != candidate.get(relative)
        )
    )
    if require_change and not changed:
        raise GuardError("Method mutation candidate makes no change")
    if universal:
        return changed

    protected = [
        relative
        for relative in NORMAL_METHOD_PROTECTED_FILES
        if baseline.get(relative) != candidate.get(relative)
    ]
    if protected:
        raise GuardError(
            "Normal method mutation cannot change the hard clock/guard surface: "
            + ", ".join(protected)
        )
    outside = [relative for relative in changed if not _normal_method_path_allowed(relative)]
    if outside:
        raise GuardError(
            "Normal method mutation changed a path outside its broad mutable surface: "
            + ", ".join(outside)
        )
    return changed


def validate_guideline_mutation(
    baseline_root: Path,
    candidate_root: Path,
    *,
    broader_mutation: bool,
    require_change: bool = True,
) -> tuple[str, ...]:
    """Validate meaningful changes to workspace-local mutable guidance."""

    if not isinstance(broader_mutation, bool):
        raise GuardError("Guideline mutation scope must be derived from a stored incident")
    changed: list[str] = []
    for name in GUIDELINE_FILES:
        baseline = read_markdown(baseline_root / name)
        candidate = read_markdown(candidate_root / name)
        if baseline != candidate:
            changed.append(name)

    for name in changed:
        baseline = read_markdown(baseline_root / name)
        candidate = read_markdown(candidate_root / name)
        if _meaningful_markdown(baseline) == _meaningful_markdown(candidate):
            raise GuardError(f"{name} mutation cannot be whitespace-only")
    if require_change and not changed:
        raise GuardError("Incident guideline candidate makes no change")
    return tuple(changed)


def _meaningful_markdown(text: str) -> str:
    return re.sub(r"\s+", " ", text).strip()


def random_review_details_from_state(
    state: str | Path,
    lineage_id: str,
    cycle_number: int,
) -> sqlite3.Row:
    """Read one exact due random-review draw without changing machine state."""

    state_path = Path(state).expanduser()
    if not state_path.is_file():
        raise GuardError(f"Deadline state does not exist: {state_path}")
    try:
        connection = sqlite3.connect(state_path.resolve().as_uri() + "?mode=ro", uri=True)
        connection.row_factory = sqlite3.Row
        row = connection.execute(
            """
            SELECT interval_windows, selected_lane, due_task_id,
                   resolution_evidence, ordinary_resolution_evidence,
                   universal_required, universal_resolution_evidence,
                   universal_capability_status, universal_capability_reason,
                   universal_capability_checked_at,
                   universal_capability_roster_digest
            FROM random_mutation_cycles
            WHERE lineage_id = ? AND cycle_number = ?
            """,
            (lineage_id, cycle_number),
        ).fetchone()
    except sqlite3.Error as error:
        raise GuardError(f"Cannot read random mutation state: {error}") from error
    finally:
        if "connection" in locals():
            connection.close()

    if row is None:
        raise GuardError(
            f"Missing random mutation cycle for {lineage_id}/{cycle_number}"
        )
    if row["due_task_id"] is None:
        raise GuardError("Random mutation cycle is not due")
    if row["resolution_evidence"] is not None:
        raise GuardError("Random mutation cycle is already resolved")
    lane = str(row["selected_lane"])
    if lane not in RANDOM_MUTATION_LANES:
        raise GuardError(f"Unsupported random mutation lane: {lane}")
    return row


def random_review_from_state(
    state: str | Path,
    lineage_id: str,
    cycle_number: int,
) -> str:
    """Read one exact due ordinary random-review lane."""

    row = random_review_details_from_state(state, lineage_id, cycle_number)
    if row["ordinary_resolution_evidence"] is not None:
        raise GuardError("Ordinary random review is already resolved")
    return str(row["selected_lane"])


def require_universal_random_review(
    state: str | Path,
    lineage_id: str,
    cycle_number: int,
) -> sqlite3.Row:
    """Require the rare signature derived from the existing authorized draw."""

    row = random_review_details_from_state(state, lineage_id, cycle_number)
    if (
        int(row["interval_windows"]) != 30
        or row["selected_lane"] != DFS_FILE
    ):
        raise GuardError(
            "Universal review is due only for the persisted 30-attempt FS draw"
        )
    if not bool(row["universal_required"]):
        reason = row["universal_capability_reason"] or (
            "the due-time workspace roster did not prove gpt-5.6-sol/ultra"
        )
        raise GuardError(f"Universal review was deferred at due time: {reason}")
    if (
        row["universal_capability_status"] != "available"
        or row["universal_capability_checked_at"] is None
        or not isinstance(row["universal_capability_roster_digest"], str)
        or len(row["universal_capability_roster_digest"]) != 64
    ):
        raise GuardError(
            "Universal review lacks an immutable due-time Sol/ultra capability snapshot"
        )
    if row["universal_resolution_evidence"] is not None:
        raise GuardError("Universal random review is already resolved")
    return row


def require_universal_reviewer_capability(cycle: sqlite3.Row) -> str:
    """Return the immutable due-time roster digest that authorized this cycle."""

    digest = cycle["universal_capability_roster_digest"]
    if (
        cycle["universal_capability_status"] != "available"
        or cycle["universal_capability_checked_at"] is None
        or not isinstance(digest, str)
        or len(digest) != 64
        or any(character not in "0123456789abcdef" for character in digest)
    ):
        raise GuardError(
            "Universal review lacks an immutable due-time Sol/ultra capability snapshot"
        )
    return digest


def universal_candidate_digest(
    candidate_root: Path, dfs_candidate: Path | None = None
) -> str:
    """Digest the complete isolated candidate, including an optional FS."""

    digest = hashlib.sha256()
    for relative, content in sorted(_method_files(candidate_root).items()):
        encoded = relative.encode("utf-8")
        digest.update(len(encoded).to_bytes(8, "big"))
        digest.update(encoded)
        digest.update(len(content).to_bytes(8, "big"))
        digest.update(content)
    if dfs_candidate is not None:
        content = dfs_candidate.read_bytes()
        label = b"\0paired-DFS.md"
        digest.update(len(label).to_bytes(8, "big"))
        digest.update(label)
        digest.update(len(content).to_bytes(8, "big"))
        digest.update(content)
    return digest.hexdigest()


def _ensure_normal_method_receipt_schema(connection: sqlite3.Connection) -> None:
    connection.execute(
        """
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
        )
        """
    )
    connection.execute(
        """
        CREATE TRIGGER IF NOT EXISTS normal_method_receipts_cannot_change
        BEFORE UPDATE ON normal_method_receipts
        BEGIN
            SELECT RAISE(ABORT, 'normal method receipts are append-only');
        END
        """
    )
    connection.execute(
        """
        CREATE TRIGGER IF NOT EXISTS normal_method_receipts_cannot_be_deleted
        BEFORE DELETE ON normal_method_receipts
        BEGIN
            SELECT RAISE(ABORT, 'normal method receipts are append-only');
        END
        """
    )


def persist_normal_method_receipt(
    state: str | Path,
    lineage_id: str,
    task_id: str,
    incident_kind: str,
    candidate_digest: str,
    changed_paths: tuple[str, ...],
    protected_baseline_digest: str,
    live_tree_digest: str,
) -> str:
    """Atomically recheck one diagnosed incident and append its method receipt."""

    if incident_kind not in INCIDENT_KINDS:
        raise GuardError(f"Unsupported incident kind: {incident_kind}")
    if not changed_paths or changed_paths != tuple(sorted(set(changed_paths))):
        raise GuardError("Normal method receipt requires stable changed-path evidence")
    if method_tree_digest() != live_tree_digest:
        raise GuardError("Active Phase-3 tree changed during method validation")
    if protected_method_digest() != protected_baseline_digest:
        raise GuardError("Active Phase-3 protected files changed during method validation")
    state_path = Path(state).expanduser()
    if not state_path.is_file():
        raise GuardError(f"Deadline state does not exist: {state_path}")
    paths_json = json.dumps(list(changed_paths), separators=(",", ":"))
    connection: sqlite3.Connection | None = None
    try:
        connection = sqlite3.connect(state_path)
        connection.row_factory = sqlite3.Row
        connection.execute("PRAGMA foreign_keys = ON")
        connection.execute("BEGIN IMMEDIATE")
        _ensure_normal_method_receipt_schema(connection)
        if incident_kind == "deadline_miss":
            incident = connection.execute(
                """
                SELECT task.claim_id, incident.reviewed_at
                FROM claim_deadline_generation_incidents AS incident
                JOIN tasks AS task
                  ON task.lineage_id = incident.lineage_id
                 AND task.task_id = incident.source_task_id
                 AND task.claim_id = incident.claim_id
                 AND task.deadline_generation = incident.generation
                WHERE incident.lineage_id = ? AND incident.source_task_id = ?
                """,
                (lineage_id, task_id),
            ).fetchone()
            already_resolved = connection.execute(
                """
                SELECT 1 FROM deadline_generation_mutation_components AS component
                JOIN claim_deadline_generation_incidents AS incident
                  ON incident.lineage_id = component.lineage_id
                 AND incident.claim_id = component.claim_id
                 AND incident.generation = component.generation
                WHERE incident.lineage_id = ? AND incident.source_task_id = ?
                  AND component.component = 'macro'
                """,
                (lineage_id, task_id),
            ).fetchone()
        else:
            incident = connection.execute(
                """
                SELECT task.claim_id, incident.reviewed_at
                FROM incidents AS incident
                JOIN tasks AS task
                  ON task.lineage_id = incident.lineage_id
                 AND task.task_id = incident.task_id
                WHERE incident.lineage_id = ? AND incident.task_id = ?
                  AND incident.kind = 'integrity_breach'
                """,
                (lineage_id, task_id),
            ).fetchone()
            already_resolved = connection.execute(
                """
                SELECT 1 FROM integrity_mutation_components
                WHERE lineage_id = ? AND task_id = ? AND component = 'macro'
                """,
                (lineage_id, task_id),
            ).fetchone()
        if incident is None:
            raise GuardError(
                f"Missing stored {incident_kind} incident for {lineage_id}/{task_id}"
            )
        if incident["reviewed_at"] is None:
            raise GuardError(
                "The exact incident needs an independent diagnosis before mutation"
            )
        if already_resolved is not None:
            raise GuardError("The exact incident macro mutation is already resolved")
        claim_id = str(incident["claim_id"])
        contract = {
            "lineage_id": lineage_id,
            "task_id": task_id,
            "claim_id": claim_id,
            "incident_kind": incident_kind,
            "candidate_digest": candidate_digest,
            "changed_paths": list(changed_paths),
            "protected_baseline_digest": protected_baseline_digest,
            "live_tree_digest": live_tree_digest,
        }
        receipt_id = hashlib.sha256(
            json.dumps(contract, sort_keys=True, separators=(",", ":")).encode(
                "utf-8"
            )
        ).hexdigest()
        connection.execute(
            """
            INSERT OR IGNORE INTO normal_method_receipts (
                receipt_id, lineage_id, task_id, claim_id, incident_kind,
                validated_at, candidate_digest, changed_paths,
                protected_baseline_digest, live_tree_digest
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            """,
            (
                receipt_id,
                lineage_id,
                task_id,
                claim_id,
                incident_kind,
                time.time(),
                candidate_digest,
                paths_json,
                protected_baseline_digest,
                live_tree_digest,
            ),
        )
        receipt = connection.execute(
            "SELECT * FROM normal_method_receipts WHERE receipt_id = ?",
            (receipt_id,),
        ).fetchone()
        if (
            receipt is None
            or receipt["lineage_id"] != lineage_id
            or receipt["task_id"] != task_id
            or receipt["claim_id"] != claim_id
            or receipt["incident_kind"] != incident_kind
            or receipt["candidate_digest"] != candidate_digest
            or receipt["changed_paths"] != paths_json
            or receipt["protected_baseline_digest"] != protected_baseline_digest
            or receipt["live_tree_digest"] != live_tree_digest
        ):
            raise GuardError("Normal method receipt conflicts with persisted state")
        connection.commit()
        return receipt_id
    except GuardError:
        if connection is not None:
            connection.rollback()
        raise
    except (OSError, sqlite3.Error) as error:
        if connection is not None:
            connection.rollback()
        raise GuardError(f"Cannot persist normal method receipt: {error}") from error
    finally:
        if connection is not None:
            connection.close()


def _ensure_universal_receipt_schema(connection: sqlite3.Connection) -> None:
    connection.execute(
        """
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
        )
        """
    )
    receipt_columns = {
        row[1]
        for row in connection.execute(
            "PRAGMA table_info(universal_review_receipts)"
        ).fetchall()
    }
    if "capability_roster_digest" not in receipt_columns:
        connection.execute(
            "ALTER TABLE universal_review_receipts ADD COLUMN capability_roster_digest TEXT"
        )
    connection.execute(
        """
        CREATE TRIGGER IF NOT EXISTS universal_review_receipts_cannot_change
        BEFORE UPDATE ON universal_review_receipts
        BEGIN
            SELECT RAISE(ABORT, 'universal review receipts are append-only');
        END
        """
    )
    connection.execute(
        """
        CREATE TRIGGER IF NOT EXISTS universal_review_receipts_cannot_be_deleted
        BEFORE DELETE ON universal_review_receipts
        BEGIN
            SELECT RAISE(ABORT, 'universal review receipts are append-only');
        END
        """
    )


def persist_universal_review_receipt(
    state: str | Path,
    lineage_id: str,
    cycle_number: int,
    candidate_digest: str,
    changed_paths: tuple[str, ...],
    capability_roster_digest: str,
) -> str:
    """Atomically recheck the rare trigger and append one validated receipt."""

    if not changed_paths:
        raise GuardError("Universal review receipt requires changed candidate paths")
    state_path = Path(state).expanduser()
    if not state_path.is_file():
        raise GuardError(f"Deadline state does not exist: {state_path}")
    paths_json = json.dumps(list(changed_paths), separators=(",", ":"))
    receipt_contract = {
        "lineage_id": lineage_id,
        "cycle_number": cycle_number,
        "candidate_digest": candidate_digest,
        "changed_paths": list(changed_paths),
        "interval_windows": 30,
        "selected_lane": DFS_FILE,
        "reviewer_model": "gpt-5.6-sol",
        "reviewer_effort": "ultra",
        "capability_roster_digest": capability_roster_digest,
    }
    receipt_id = hashlib.sha256(
        json.dumps(receipt_contract, sort_keys=True, separators=(",", ":")).encode(
            "utf-8"
        )
    ).hexdigest()
    connection: sqlite3.Connection | None = None
    try:
        connection = sqlite3.connect(state_path)
        connection.row_factory = sqlite3.Row
        connection.execute("PRAGMA foreign_keys = ON")
        connection.execute("BEGIN IMMEDIATE")
        _ensure_universal_receipt_schema(connection)
        cycle = connection.execute(
            """
            SELECT interval_windows, selected_lane, due_task_id,
                   resolution_evidence, universal_required,
                   universal_resolution_evidence,
                   universal_capability_status,
                   universal_capability_checked_at,
                   universal_capability_roster_digest
            FROM random_mutation_cycles
            WHERE lineage_id = ? AND cycle_number = ?
            """,
            (lineage_id, cycle_number),
        ).fetchone()
        if cycle is None:
            raise GuardError(
                f"Missing random mutation cycle for {lineage_id}/{cycle_number}"
            )
        if (
            cycle["due_task_id"] is None
            or cycle["resolution_evidence"] is not None
            or cycle["universal_resolution_evidence"] is not None
            or int(cycle["interval_windows"]) != 30
            or cycle["selected_lane"] != DFS_FILE
            or not bool(cycle["universal_required"])
            or cycle["universal_capability_status"] != "available"
            or cycle["universal_capability_checked_at"] is None
            or cycle["universal_capability_roster_digest"]
                != capability_roster_digest
        ):
            raise GuardError(
                "Universal receipt requires the still-due persisted 30-attempt FS draw"
            )
        connection.execute(
            """
            INSERT OR IGNORE INTO universal_review_receipts (
                receipt_id, lineage_id, cycle_number, validated_at,
                candidate_digest, changed_paths, interval_windows,
                selected_lane, reviewer_model, reviewer_effort,
                capability_roster_digest
            ) VALUES (?, ?, ?, ?, ?, ?, 30, 'DFS.md', 'gpt-5.6-sol', 'ultra', ?)
            """,
            (
                receipt_id,
                lineage_id,
                cycle_number,
                time.time(),
                candidate_digest,
                paths_json,
                capability_roster_digest,
            ),
        )
        receipt = connection.execute(
            """
            SELECT * FROM universal_review_receipts
            WHERE receipt_id = ? AND lineage_id = ? AND cycle_number = ?
            """,
            (receipt_id, lineage_id, cycle_number),
        ).fetchone()
        if (
            receipt is None
            or receipt["candidate_digest"] != candidate_digest
            or receipt["changed_paths"] != paths_json
            or receipt["capability_roster_digest"] != capability_roster_digest
        ):
            raise GuardError("Universal review receipt conflicts with persisted state")
        connection.commit()
        return receipt_id
    except GuardError:
        if connection is not None:
            connection.rollback()
        raise
    except (OSError, sqlite3.Error) as error:
        if connection is not None:
            connection.rollback()
        raise GuardError(f"Cannot persist universal review receipt: {error}") from error
    finally:
        if connection is not None:
            connection.close()


def validate_random_review_mutation(
    baseline_root: Path,
    candidate_root: Path,
    *,
    selected_lane: str,
) -> tuple[str, ...]:
    """Validate trajectory-driven local changes; the stored lane is a review seed."""

    if selected_lane not in (*RANDOM_MUTATION_LANES, *LEGACY_RANDOM_MUTATION_LANES):
        raise GuardError(f"Unsupported random mutation lane: {selected_lane}")
    baseline_files = {
        name: read_markdown(baseline_root / name)
        for name in GUIDELINE_FILES
    }
    candidate_files = {
        name: read_markdown(candidate_root / name)
        for name in GUIDELINE_FILES
    }
    # Existing receipts keep their lane label; the contract has one filename.
    try:
        baseline_specification = resolve_path(baseline_root / FS_FILE)
        candidate_specification = resolve_path(candidate_root / FS_FILE)
    except SpecificationError as error:
        raise GuardError(str(error)) from error
    baseline_files[DFS_FILE] = baseline_specification.text
    candidate_files[DFS_FILE] = candidate_specification.text
    changed = tuple(
        name
        for name in (*GUIDELINE_FILES, DFS_FILE)
        if baseline_files[name] != candidate_files[name]
    )
    for name in changed:
        if name in GUIDELINE_FILES:
            if _meaningful_markdown(baseline_files[name]) == _meaningful_markdown(candidate_files[name]):
                raise GuardError("Random guideline mutation cannot be whitespace-only")
        elif name == DFS_FILE:
            validate_random_dfs_mutation(
                baseline_specification.path, candidate_specification.path
            )
    return changed


def broader_mutation_from_incident(
    state: str | Path,
    lineage_id: str,
    task_id: str,
    incident_kind: str,
) -> bool:
    """Derive guideline scope from one exact, stored deadline incident."""

    if incident_kind not in INCIDENT_KINDS:
        raise GuardError(f"Unsupported incident kind: {incident_kind}")
    state_path = Path(state).expanduser()
    if not state_path.is_file():
        raise GuardError(f"Deadline state does not exist: {state_path}")
    try:
        connection = sqlite3.connect(state_path.resolve().as_uri() + "?mode=ro", uri=True)
        connection.row_factory = sqlite3.Row
        if incident_kind == "deadline_miss":
            row = connection.execute(
                """
                SELECT short_verdict, long_detail, reviewed_at
                FROM claim_deadline_generation_incidents
                WHERE lineage_id = ? AND source_task_id = ?
                ORDER BY generation DESC LIMIT 1
                """,
                (lineage_id, task_id),
            ).fetchone()
        else:
            row = connection.execute(
                """
                SELECT short_verdict, long_detail, reviewed_at
                FROM incidents
                WHERE lineage_id = ? AND task_id = ? AND kind = ?
                """,
                (lineage_id, task_id, incident_kind),
            ).fetchone()
    except sqlite3.Error as error:
        raise GuardError(f"Cannot read deadline state: {error}") from error
    finally:
        if "connection" in locals():
            connection.close()

    if row is None:
        raise GuardError(
            f"Missing stored {incident_kind} incident for {lineage_id}/{task_id}"
        )
    if (
        row["reviewed_at"] is None
        or not str(row["short_verdict"] or "").strip()
        or not str(row["long_detail"] or "").strip()
    ):
        raise GuardError(
            "The exact incident needs an independent short and long diagnosis before mutation"
        )
    return True


def _outside_fences(text: str):
    fence_character: str | None = None
    for line_number, line in enumerate(text.splitlines(), start=1):
        fence = FENCE.match(line)
        if fence:
            character = fence.group("marker")[0]
            if fence_character is None:
                fence_character = character
            elif fence_character == character:
                fence_character = None
            continue
        if fence_character is None:
            yield line_number, line


def _normalize_reference(value: str) -> str:
    normalized = " ".join(value.strip().split())
    if normalized.startswith("🔴 "):
        normalized = normalized[2:].strip()
    for prefix in ("FS claim:", "DFS claim:", "Claim:"):
        if normalized.casefold().startswith(prefix.casefold()):
            normalized = normalized[len(prefix) :].strip()
            break
    return normalized


def _stable_key(value: str) -> str:
    for separator in (" — ", " – ", ": "):
        if separator in value:
            return value.split(separator, 1)[0].strip()
    return value


def _same_claim(reference: str, label: str) -> bool:
    return reference == label or _stable_key(reference) == _stable_key(label)


def _selected_claim_id(selected_claim: str) -> str:
    claim_id = _stable_key(_normalize_reference(selected_claim))
    if not CLAIM_ID.fullmatch(claim_id):
        raise GuardError(f"Selected claim has no valid product or maintenance id: {selected_claim}")
    return claim_id


def validate_accepted_task_state(
    state: str | Path,
    lineage_id: str,
    task_id: str,
    selected_claim: str,
) -> str:
    """Require claim acceptance from one matching, breach-free closure attempt."""

    state_path = Path(state).expanduser()
    if not state_path.is_file():
        raise GuardError(f"Deadline state does not exist: {state_path}")
    expected_claim = _selected_claim_id(selected_claim)
    try:
        connection = sqlite3.connect(state_path.resolve().as_uri() + "?mode=ro", uri=True)
        connection.row_factory = sqlite3.Row
        row = connection.execute(
            """
            SELECT tasks.claim_id,
                   tasks.phase_at_dispatch,
                   tasks.attempt_terminal_kind,
                   tasks.completion_evidence,
                   tasks.integrity_breached_at,
                   claim_acceptances.evidence AS acceptance_evidence,
                   EXISTS (
                       SELECT 1 FROM incidents
                       WHERE incidents.lineage_id = tasks.lineage_id
                         AND incidents.task_id = tasks.task_id
                         AND incidents.kind = 'integrity_breach'
                   ) AS has_integrity_incident
            FROM tasks
            LEFT JOIN claim_acceptances
              ON claim_acceptances.lineage_id = tasks.lineage_id
             AND claim_acceptances.claim_id = tasks.claim_id
             AND claim_acceptances.task_id = tasks.task_id
             AND claim_acceptances.invalidated_at IS NULL
            WHERE tasks.lineage_id = ? AND tasks.task_id = ?
            """,
            (lineage_id, task_id),
        ).fetchone()
    except sqlite3.Error as error:
        raise GuardError(f"Cannot read deadline state: {error}") from error
    finally:
        if "connection" in locals():
            connection.close()

    if row is None:
        raise GuardError(f"Unknown deadline task: {lineage_id}/{task_id}")
    if row["claim_id"] != expected_claim:
        raise GuardError(
            f"Deadline task claim {row['claim_id']} does not match selected claim {expected_claim}"
        )
    if row["integrity_breached_at"] is not None or row["has_integrity_incident"]:
        raise GuardError("An integrity breach invalidates FS completion")
    if (
        row["phase_at_dispatch"] != "closure"
        or row["attempt_terminal_kind"] != "completed"
    ):
        raise GuardError("Claim acceptance requires a completed closure attempt")
    evidence = row["acceptance_evidence"]
    if evidence is None or not str(evidence).strip():
        raise GuardError("Deadline claim has not been accepted")
    if row["completion_evidence"] is None or not str(
        row["completion_evidence"]
    ).strip():
        raise GuardError("Accepted closure attempt has no completion evidence")
    return expected_claim


def validate_invalidated_claim_state(
    state: str | Path,
    lineage_id: str,
    selected_claim: str,
) -> tuple[str, str]:
    """Require one latest invalidated acceptance with its durable trigger."""

    state_path = Path(state).expanduser()
    if not state_path.is_file():
        raise GuardError(f"Deadline state does not exist: {state_path}")
    expected_claim = _selected_claim_id(selected_claim)
    try:
        connection = sqlite3.connect(state_path.resolve().as_uri() + "?mode=ro", uri=True)
        connection.row_factory = sqlite3.Row
        acceptance = connection.execute(
            """
            SELECT * FROM claim_acceptances
            WHERE lineage_id = ? AND claim_id = ?
            ORDER BY acceptance_number DESC
            LIMIT 1
            """,
            (lineage_id, expected_claim),
        ).fetchone()
        valid = connection.execute(
            """
            SELECT 1 FROM claim_acceptances
            WHERE lineage_id = ? AND claim_id = ? AND invalidated_at IS NULL
            LIMIT 1
            """,
            (lineage_id, expected_claim),
        ).fetchone()
        trigger = None
        if acceptance is not None and acceptance["invalidated_at"] is not None:
            trigger = connection.execute(
                """
                SELECT 'integrity_breach' AS trigger_kind,
                       incidents.task_id AS trigger_task_id
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
                UNION ALL
                SELECT 'closure_reopen' AS trigger_kind,
                       event.basis_task_id AS trigger_task_id
                FROM claim_phase_events AS event
                JOIN worker_findings AS finding
                  ON finding.lineage_id = event.lineage_id
                 AND finding.task_id = event.basis_task_id
                WHERE event.lineage_id = ?
                  AND event.claim_id = ?
                  AND event.phase = 'exploration'
                  AND event.sequence > COALESCE(?, 0)
                  AND event.recorded_at = ?
                LIMIT 1
                """,
                (
                    lineage_id,
                    expected_claim,
                    acceptance["invalidated_at"],
                    acceptance["task_id"],
                    acceptance["closure_sequence"],
                    lineage_id,
                    expected_claim,
                    acceptance["closure_sequence"],
                    acceptance["invalidated_at"],
                ),
            ).fetchone()
        if trigger is None and acceptance is not None and acceptance["invalidated_at"] is not None:
            trigger = connection.execute(
                "SELECT 'owner_reopen' AS trigger_kind, basis_task_id AS trigger_task_id "
                "FROM claim_phase_events WHERE lineage_id = ? AND claim_id = ? "
                "AND phase = 'exploration' AND sequence > COALESCE(?, 0) "
                "AND recorded_at = ? AND basis_task_id = ? AND contradicted_premise IS NULL "
                "AND ? = 'owner requested reassessment: ' || closure_evidence LIMIT 1",
                (lineage_id, expected_claim, acceptance["closure_sequence"],
                 acceptance["invalidated_at"], acceptance["task_id"],
                 acceptance["invalidation_reason"]),
            ).fetchone()
    except sqlite3.Error as error:
        raise GuardError(f"Cannot read claim invalidation state: {error}") from error
    finally:
        if "connection" in locals():
            connection.close()

    if acceptance is None:
        raise GuardError(f"Claim has no recorded acceptance: {lineage_id}/{expected_claim}")
    if valid is not None:
        raise GuardError("A currently valid acceptance prevents FS reopening")
    if acceptance["invalidated_at"] is None:
        raise GuardError("Latest claim acceptance has not been invalidated")
    if not str(acceptance["evidence"] or "").strip():
        raise GuardError("Invalidated claim acceptance has no durable evidence")
    if not str(acceptance["invalidation_reason"] or "").strip():
        raise GuardError("Invalidated claim acceptance has no durable reason")
    if trigger is None:
        raise GuardError("Claim invalidation has no durable reopen or integrity trigger")
    return expected_claim, str(trigger["trigger_kind"])


def worker_finding_from_state(
    state: str | Path,
    lineage_id: str,
    task_id: str,
) -> tuple[str, str]:
    """Read one exact, unresolved worker finding without mutating deadline state."""

    state_path = Path(state).expanduser()
    if not state_path.is_file():
        raise GuardError(f"Deadline state does not exist: {state_path}")
    try:
        connection = sqlite3.connect(state_path.resolve().as_uri() + "?mode=ro", uri=True)
        connection.row_factory = sqlite3.Row
        row = connection.execute(
            """
            SELECT tasks.claim_id,
                   tasks.completed_at,
                   tasks.integrity_breached_at,
                   worker_findings.kind,
                   worker_findings.evidence,
                   EXISTS (
                       SELECT 1 FROM incidents
                       WHERE incidents.lineage_id = tasks.lineage_id
                         AND incidents.task_id = tasks.task_id
                         AND incidents.kind = 'integrity_breach'
                   ) AS has_integrity_incident
            FROM tasks
            JOIN worker_findings
              ON worker_findings.lineage_id = tasks.lineage_id
             AND worker_findings.task_id = tasks.task_id
            WHERE tasks.lineage_id = ? AND tasks.task_id = ?
            """,
            (lineage_id, task_id),
        ).fetchone()
    except sqlite3.Error as error:
        raise GuardError(f"Cannot read worker finding state: {error}") from error
    finally:
        if "connection" in locals():
            connection.close()

    if row is None:
        raise GuardError(f"Missing stored worker finding for {lineage_id}/{task_id}")
    if row["kind"] not in ("blocker", "unexpected"):
        raise GuardError(f"Unsupported worker finding kind: {row['kind']}")
    if row["evidence"] is None or not str(row["evidence"]).strip():
        raise GuardError("Stored worker finding has no evidence")
    if row["completed_at"] is not None:
        raise GuardError("A completed task cannot authorize FS expansion")
    if row["integrity_breached_at"] is not None or row["has_integrity_incident"]:
        raise GuardError("An integrity breach invalidates the worker finding")
    return str(row["claim_id"]), str(row["kind"])


def _markdown_sections(text: str) -> tuple[tuple[int, int, str], ...]:
    """Return heading positions and levels outside fenced examples."""

    sections: list[tuple[int, int, str]] = []
    fence_character: str | None = None
    for index, line in enumerate(text.splitlines(keepends=True)):
        body = line.rstrip("\r\n")
        fence = FENCE.match(body)
        if fence:
            character = fence.group("marker")[0]
            if fence_character is None:
                fence_character = character
            elif fence_character == character:
                fence_character = None
            continue
        if fence_character is not None:
            continue
        heading = ATX_HEADING.match(body)
        if heading:
            stripped = body.rstrip()
            sections.append((index, len(stripped) - len(stripped.lstrip("#")), stripped))
    return tuple(sections)


def _exact_markdown_section(text: str, heading: str) -> str:
    lines = text.splitlines(keepends=True)
    sections = _markdown_sections(text)
    matches = [section for section in sections if section[2] == heading]
    if len(matches) != 1:
        raise GuardError(f"FS must contain exactly one {heading!r} section")
    start, level, _ = matches[0]
    end = len(lines)
    for index, other_level, _ in sections:
        if index > start and other_level <= level:
            end = index
            break
    return "".join(lines[start:end])


def _stable_claim_records(dfs_text: str) -> tuple[tuple[str, str, bool, str], ...]:
    records: list[tuple[str, str, bool, str]] = []
    for _, line in _outside_fences(dfs_text):
        match = STABLE_CLAIM.match(line)
        if match:
            label = _normalize_reference(match.group("label"))
            records.append(
                (
                    _selected_claim_id(label),
                    match.group("status"),
                    match.group("red") is not None,
                    line,
                )
            )
    return tuple(records)


def _claims_by_id(
    records: tuple[tuple[str, str, bool, str], ...],
    version: str,
) -> dict[str, tuple[str, str, bool, str]]:
    claims: dict[str, tuple[str, str, bool, str]] = {}
    for record in records:
        claim_id = record[0]
        if claim_id in claims:
            raise GuardError(f"{version} FS has duplicate stable claim id: {claim_id}")
        claims[claim_id] = record
    return claims


def _require_baseline_line_subsequence(baseline: str, candidate: str) -> None:
    remaining = iter(candidate.splitlines(keepends=True))
    for baseline_line in baseline.splitlines(keepends=True):
        if not any(line == baseline_line for line in remaining):
            raise GuardError(
                "FS expansion cannot delete or rewrite any baseline line"
            )


def _validate_append_only_dfs(
    before: Path,
    candidate: Path,
    *,
    task_claim_id: str | None,
) -> tuple[str, ...]:
    baseline = read_markdown(before)
    proposed = read_markdown(candidate)
    for heading in PROTECTED_DFS_SECTIONS:
        if _exact_markdown_section(baseline, heading) != _exact_markdown_section(
            proposed, heading
        ):
            raise GuardError(f"FS expansion cannot change {heading}")
    _require_baseline_line_subsequence(baseline, proposed)

    baseline_records = _stable_claim_records(baseline)
    candidate_records = _stable_claim_records(proposed)
    baseline_by_id = _claims_by_id(baseline_records, "Baseline")
    candidate_by_id = _claims_by_id(candidate_records, "Candidate")

    if task_claim_id is not None:
        task_claim_id = _selected_claim_id(task_claim_id)
        task_claim = baseline_by_id.get(task_claim_id)
        if task_claim is None or task_claim[1] != " " or not task_claim[2]:
            raise GuardError(
                "Worker finding task claim is not exactly one still-red FS claim: "
                f"{task_claim_id}"
            )

    candidate_positions = {
        record[0]: index for index, record in enumerate(candidate_records)
    }
    prior_position = -1
    for record in baseline_records:
        claim_id = record[0]
        candidate_record = candidate_by_id.get(claim_id)
        if candidate_record is None:
            raise GuardError(f"FS expansion cannot delete stable claim {claim_id}")
        if candidate_record[3] != record[3]:
            raise GuardError(
                f"FS expansion cannot rename, rewrite, or change status of stable claim {claim_id}"
            )
        position = candidate_positions[claim_id]
        if position <= prior_position:
            raise GuardError("FS expansion cannot reorder existing stable claims")
        prior_position = position

    new_records = [
        record for record in candidate_records if record[0] not in baseline_by_id
    ]
    if not new_records:
        raise GuardError("FS expansion must add at least one new red claim")
    for record in new_records:
        if record[1] != " " or not record[2] or RED_CLAIM.match(record[3]) is None:
            raise GuardError(f"New FS claim must be unchecked and red: {record[0]}")
    return tuple(record[0] for record in new_records)


def validate_dfs_expansion(before: Path, candidate: Path, task_claim_id: str) -> tuple[str, ...]:
    """Preserve the frontier for an expansion authorized by a worker finding."""

    return _validate_append_only_dfs(
        before,
        candidate,
        task_claim_id=task_claim_id,
    )


def validate_random_dfs_mutation(before: Path, candidate: Path) -> tuple[str, ...]:
    """Allow status projection metadata repair or same-contract new red work."""

    baseline = read_markdown(before)
    proposed = read_markdown(candidate)
    # The status heading is a parser delimiter, not a new product obligation.
    # Admit only the canonical missing heading immediately before an existing
    # red claim inside its matching slice. Every other byte remains protected.
    normalized = baseline
    for item in parse_dfs_slices(baseline):
        if re.search(r"(?m)^Implementation status:\s*$", item.content):
            continue
        row = re.search(r"(?m)^- \[ \] 🔴 " + re.escape(item.claim_id)
                        + r"(?=[ \t]+—|[ \t]*$)", item.content)
        if row is not None:
            content = item.content[:row.start()] + "Implementation status:\n\n" + item.content[row.start():]
            normalized = normalized.replace(item.content, content, 1)
    if proposed == normalized and proposed != baseline:
        return ()

    return _validate_append_only_dfs(
        before,
        candidate,
        task_claim_id=None,
    )


def _pending_suggestion_entries(text: str) -> tuple[str, ...]:
    """Return exact top-level entries from the mutation ledger's pending section."""

    section = _exact_markdown_section(text, "## Pending suggestions")
    entries: list[str] = []
    current: list[str] = []
    for line in section.splitlines(keepends=True)[1:]:
        if line.startswith("- "):
            if current:
                entries.append("".join(current))
            current = [line]
        elif current:
            current.append(line)
        elif line.strip():
            raise GuardError("Pending suggestions must be top-level Markdown entries")
    if current:
        entries.append("".join(current))
    return tuple(entries)


def validate_owner_suggestion_consumption(before: Path, candidate: Path) -> tuple[str, ...]:
    """Require exact consumption of one or more owner-authored pending entries."""

    baseline = read_markdown(before)
    proposed = read_markdown(candidate)
    baseline_prefix = baseline.split("## Pending suggestions", 1)[0]
    proposed_prefix = proposed.split("## Pending suggestions", 1)[0]
    if baseline_prefix != proposed_prefix:
        raise GuardError("Owner suggestion consumption cannot rewrite the ledger preface")
    baseline_entries = _pending_suggestion_entries(baseline)
    candidate_entries = _pending_suggestion_entries(proposed)
    remaining = list(baseline_entries)
    for entry in candidate_entries:
        if entry not in remaining:
            raise GuardError("Ledger candidate added or rewrote a pending suggestion")
        remaining.remove(entry)
    if not remaining:
        raise GuardError("Owner FS expansion must consume at least one pending suggestion")
    if any(not entry.startswith("- Owner-authorized") for entry in remaining):
        raise GuardError("Owner FS expansion may consume only owner-authorized suggestions")
    return tuple(entry.splitlines()[0][2:].strip() for entry in remaining)


def validate_owner_dfs_expansion(
    before: Path,
    candidate: Path,
    ledger_before: Path,
    ledger_candidate: Path,
) -> tuple[tuple[str, ...], tuple[str, ...]]:
    """Guard a same-outcome FS expansion authorized by pending owner guidance."""

    consumed = validate_owner_suggestion_consumption(ledger_before, ledger_candidate)
    baseline = read_markdown(before)
    proposed = read_markdown(candidate)
    normalized = proposed
    refreeze_pairs = (
        ("Status: Frozen", "Status: Refrozen"),
        ("- Status: `Frozen`", "- Status: `Refrozen`"),
        ("After freeze, automation", "After this refreeze, automation"),
    )
    for frozen, refrozen in refreeze_pairs:
        if frozen in baseline:
            if refrozen not in normalized:
                raise GuardError(f"Owner FS expansion must refreeze through {refrozen!r}")
            normalized = normalized.replace(refrozen, frozen, 1)
    descriptor, temporary_name = tempfile.mkstemp(suffix=".owner-dfs.md")
    temporary = Path(temporary_name)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8", newline="") as stream:
            stream.write(normalized)
        added = _validate_append_only_dfs(before, temporary, task_claim_id=None)
    finally:
        if temporary.exists():
            temporary.unlink()
    return added, consumed


def _read_utf8_exact(path: Path) -> str:
    if not path.is_file():
        raise GuardError(f"Missing Markdown file: {path}")
    return path.read_bytes().decode("utf-8")


def _atomic_write_utf8(path: Path, text: str) -> None:
    """Replace one output only after its complete UTF-8 payload is durable."""

    if not path.parent.is_dir():
        raise GuardError(f"Output directory does not exist: {path.parent}")
    descriptor, temporary_name = tempfile.mkstemp(
        prefix=f".{path.name}.de67-",
        suffix=".tmp",
        dir=path.parent,
    )
    temporary = Path(temporary_name)
    try:
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(text.encode("utf-8"))
            stream.flush()
            os.fsync(stream.fileno())
        if path.exists():
            os.chmod(temporary, path.stat().st_mode)
        os.replace(temporary, path)
    finally:
        if temporary.exists():
            temporary.unlink()


def _write_stdout_utf8(text: str) -> None:
    """Emit extracted FS text without inheriting a legacy console code page."""

    binary = getattr(sys.stdout, "buffer", None)
    if binary is None:
        sys.stdout.write(text)
        return
    binary.write(text.encode("utf-8"))
    binary.flush()


def parse_dfs_slices(dfs_text: str) -> tuple[DfsSlice, ...]:
    """Validate and return every durable properly nested FS slice marker pair."""

    lines = dfs_text.splitlines(keepends=True)
    markers: dict[int, re.Match[str]] = {}
    fence_character: str | None = None
    for index, line in enumerate(lines):
        body = line.rstrip("\r\n")
        fence = FENCE.match(body)
        if fence:
            character = fence.group("marker")[0]
            if fence_character is None:
                fence_character = character
            elif fence_character == character:
                fence_character = None
        if DFS_SLICE_MARKER_TOKEN not in body:
            continue
        if fence_character is not None or fence is not None:
            raise GuardError(
                f"FS slice marker token is not allowed in a fenced block on line {index + 1}"
            )
        marker = DFS_SLICE_MARKER.fullmatch(body)
        if marker is None:
            raise GuardError(f"Malformed FS slice marker on line {index + 1}")
        if not marker.group("id").startswith(f"{marker.group('claim')}-S"):
            raise GuardError(
                f"FS slice id is not bound to its marker claim on line {index + 1}"
            )
        markers[index] = marker

    slices: list[DfsSlice] = []
    active: list[tuple[str, str, int, int]] = []
    seen_ids: set[str] = set()
    logical_line = 0
    for index, line in enumerate(lines):
        marker = markers.get(index)
        if marker is None:
            logical_line += 1
            continue
        kind = marker.group("kind")
        slice_id = marker.group("id")
        claim_id = marker.group("claim")
        if kind == "BEGIN":
            if slice_id in seen_ids:
                raise GuardError(f"Duplicate FS slice marker id: {slice_id}")
            seen_ids.add(slice_id)
            active.append((slice_id, claim_id, index, logical_line))
            continue
        if not active:
            raise GuardError(f"FS slice END has no matching BEGIN: {slice_id}")
        begin_id, begin_claim, begin_index, logical_before = active[-1]
        if slice_id != begin_id or claim_id != begin_claim:
            raise GuardError(
                "FS slice markers are crossed or bind different ids/claims"
            )
        logical_start = logical_before + 1
        logical_end = logical_line
        if logical_end < logical_start:
            raise GuardError(f"FS slice is empty: {slice_id}")
        slices.append(
            DfsSlice(
                slice_id=slice_id,
                claim_id=claim_id,
                begin_index=begin_index,
                end_index=index,
                logical_start=logical_start,
                logical_end=logical_end,
                content="".join(
                    nested_line
                    for nested_line in lines[begin_index + 1 : index]
                    if DFS_SLICE_MARKER_TOKEN
                    not in nested_line.rstrip("\r\n")
                ),
            )
        )
        active.pop()
    if active:
        raise GuardError(f"FS slice BEGIN has no matching END: {active[-1][0]}")
    bindings: set[tuple[str, int, int]] = set()
    for item in slices:
        binding = (item.claim_id, item.logical_start, item.logical_end)
        if binding in bindings:
            raise GuardError(
                "A claim cannot bind two FS slice ids to the same logical range: "
                f"{item.claim_id} {item.logical_start}:{item.logical_end}"
            )
        bindings.add(binding)
    return tuple(slices)


def strip_dfs_slice_markers(dfs_text: str) -> str:
    """Return exact FS text with only validated marker lines removed."""

    parse_dfs_slices(dfs_text)
    return "".join(
        line
        for line in dfs_text.splitlines(keepends=True)
        if DFS_SLICE_MARKER_TOKEN not in line.rstrip("\r\n")
    )


def validate_dfs_slice_candidate(before: Path, candidate: Path) -> tuple[str, ...]:
    """Prove an anchor-only candidate preserves every semantic FS byte."""

    baseline = _read_utf8_exact(before)
    proposed = _read_utf8_exact(candidate)
    baseline_slices = parse_dfs_slices(baseline)
    candidate_slices = parse_dfs_slices(proposed)
    if strip_dfs_slice_markers(baseline) != strip_dfs_slice_markers(proposed):
        raise GuardError(
            "FS slice candidate changes content other than validated marker lines"
        )
    candidate_by_id = {item.slice_id: item for item in candidate_slices}
    for item in baseline_slices:
        replacement = candidate_by_id.get(item.slice_id)
        if replacement is None or (
            replacement.claim_id,
            replacement.logical_start,
            replacement.logical_end,
            replacement.content,
        ) != (
            item.claim_id,
            item.logical_start,
            item.logical_end,
            item.content,
        ):
            raise GuardError(
                f"FS slice candidate removes or rebinds durable slice {item.slice_id}"
            )
    baseline_ids = {item.slice_id for item in baseline_slices}
    return tuple(
        item.slice_id for item in candidate_slices if item.slice_id not in baseline_ids
    )


def _slice_id(
    claim_id: str,
    occupied: set[str],
) -> str:
    prefix = f"{claim_id}-S"
    used_ordinals = [
        int(slice_id[len(prefix) :])
        for slice_id in occupied
        if slice_id.startswith(prefix) and slice_id[len(prefix) :].isdigit()
    ]
    ordinal = max(used_ordinals, default=0) + 1
    candidate = f"{prefix}{ordinal:03d}"
    if candidate in occupied:
        raise GuardError(f"Cannot allocate a collision-free FS slice id: {candidate}")
    return candidate


def _line_ending(line: str) -> str:
    if line.endswith("\r\n"):
        return "\r\n"
    if line.endswith("\n"):
        return "\n"
    if line.endswith("\r"):
        return "\r"
    return ""


def _acquire_dfs_path_locks(paths: tuple[Path, ...]) -> tuple[tuple[int, Path], ...]:
    acquired: list[tuple[int, Path]] = []
    lock_paths = sorted(
        {
            path.parent / f".{path.name}.de67-dfs-slices.lock"
            for path in paths
        },
        key=lambda path: str(path.resolve()),
    )
    try:
        for lock_path in lock_paths:
            try:
                descriptor = os.open(
                    lock_path,
                    os.O_CREAT | os.O_EXCL | os.O_WRONLY,
                    0o600,
                )
            except FileExistsError as error:
                raise GuardError(
                    f"FS slice path is locked by another marker operation: {lock_path}"
                ) from error
            acquired.append((descriptor, lock_path))
            os.write(descriptor, f"pid={os.getpid()}\n".encode("ascii"))
    except Exception:
        _release_dfs_path_locks(tuple(acquired))
        raise
    return tuple(acquired)


def _release_dfs_path_locks(locks: tuple[tuple[int, Path], ...]) -> None:
    for descriptor, lock_path in reversed(locks):
        try:
            os.close(descriptor)
        finally:
            try:
                lock_path.unlink()
            except FileNotFoundError:
                pass


def insert_dfs_slices(
    source: Path,
    output: Path,
    selected_claim: str,
    ranges: tuple[tuple[int, int], ...],
) -> tuple[str, ...]:
    """Lock, validate, and atomically insert durable FS slice markers."""

    locks = _acquire_dfs_path_locks((source, output))
    try:
        return _insert_dfs_slices_locked(source, output, selected_claim, ranges)
    finally:
        _release_dfs_path_locks(locks)


def _insert_dfs_slices_locked(
    source: Path,
    output: Path,
    selected_claim: str,
    ranges: tuple[tuple[int, int], ...],
) -> tuple[str, ...]:
    """Allocate stable ids and atomically insert markers around logical lines."""

    if not ranges:
        raise GuardError("FS slice insertion needs at least one inclusive line range")
    claim_id = _selected_claim_id(selected_claim)
    baseline = _read_utf8_exact(source)
    existing = parse_dfs_slices(baseline)
    semantic = strip_dfs_slice_markers(baseline)
    claim_records = [
        record for record in _stable_claim_records(semantic) if record[0] == claim_id
    ]
    if len(claim_records) != 1:
        raise GuardError(
            f"FS slice claim must identify exactly one stable FS claim: {claim_id}"
        )

    semantic_lines = semantic.splitlines(keepends=True)
    normalized_ranges = tuple(sorted(ranges))
    prior_end = 0
    for start, end in normalized_ranges:
        if start < 1 or end < start or end > len(semantic_lines):
            raise GuardError(
                f"Invalid inclusive FS slice range {start}:{end}; "
                f"logical FS has {len(semantic_lines)} lines"
            )
        if start <= prior_end:
            raise GuardError("FS slice ranges must be disjoint and non-overlapping")
        prior_end = end

    occupied = {item.slice_id for item in existing}
    selected_ids: list[str] = []
    all_bindings = [
        (item.slice_id, item.claim_id, item.logical_start, item.logical_end)
        for item in existing
    ]
    for start, end in normalized_ranges:
        overlaps = [
            item
            for item in existing
            if start <= item.logical_end and item.logical_start <= end
        ]
        exact = [
            item
            for item in overlaps
            if item.logical_start == start and item.logical_end == end
        ]
        if exact:
            same_claim = [item for item in exact if item.claim_id == claim_id]
            if same_claim:
                selected_ids.append(same_claim[0].slice_id)
                continue
        if any(item.claim_id == claim_id for item in overlaps):
            raise GuardError(
                f"FS slices for {claim_id} must be non-overlapping; "
                f"range {start}:{end} overlaps an existing slice"
            )
        crossing = [
            item
            for item in overlaps
            if not (
                (start <= item.logical_start and item.logical_end <= end)
                or (item.logical_start <= start and end <= item.logical_end)
            )
        ]
        if crossing:
            item = crossing[0]
            raise GuardError(
                f"FS slice range {start}:{end} crosses existing slice "
                f"{item.slice_id} at {item.logical_start}:{item.logical_end}"
            )

        newline = _line_ending(semantic_lines[end - 1])
        if not newline:
            raise GuardError(
                "Cannot place a line marker after a selected final line without a line ending"
            )
        slice_id = _slice_id(claim_id, occupied)
        occupied.add(slice_id)
        selected_ids.append(slice_id)
        all_bindings.append((slice_id, claim_id, start, end))

    opens: dict[int, list[tuple[str, str, int, int]]] = {}
    closes: dict[int, list[tuple[str, str, int, int]]] = {}
    for binding in all_bindings:
        opens.setdefault(binding[2], []).append(binding)
        closes.setdefault(binding[3], []).append(binding)
    for bindings in opens.values():
        bindings.sort(key=lambda binding: (-binding[3], binding[0]))
    for bindings in closes.values():
        bindings.sort(key=lambda binding: binding[0], reverse=True)
        bindings.sort(key=lambda binding: binding[2], reverse=True)

    candidate_parts: list[str] = []
    for logical_line, line in enumerate(semantic_lines, start=1):
        newline = _line_ending(line)
        for slice_id, binding_claim, _, _ in opens.get(logical_line, ()):
            candidate_parts.append(
                f"<!-- DE67:DFS-SLICE:BEGIN id={slice_id} "
                f"claim={binding_claim} -->{newline}"
            )
        candidate_parts.append(line)
        for slice_id, binding_claim, _, _ in closes.get(logical_line, ()):
            candidate_parts.append(
                f"<!-- DE67:DFS-SLICE:END id={slice_id} "
                f"claim={binding_claim} -->{newline}"
            )
    candidate = "".join(candidate_parts)

    temporary_candidate = output.parent / f".{output.name}.de67-validation-{time.time_ns()}"
    try:
        temporary_candidate.write_bytes(candidate.encode("utf-8"))
        validate_dfs_slice_candidate(source, temporary_candidate)
    finally:
        if temporary_candidate.exists():
            temporary_candidate.unlink()
    if _read_utf8_exact(source) != baseline:
        raise GuardError("FS source changed while slice markers were being prepared")
    _atomic_write_utf8(output, candidate)
    return tuple(selected_ids)


def extract_dfs_slices(
    dfs: Path,
    selected_claim: str,
    slice_ids: tuple[str, ...],
) -> str:
    """Return only the requested validated marked blocks in request order."""

    if not slice_ids:
        raise GuardError("FS slice extraction needs at least one slice id")
    if len(slice_ids) != len(set(slice_ids)):
        raise GuardError("FS slice extraction cannot request a duplicate id")
    claim_id = _selected_claim_id(selected_claim)
    try:
        text = resolve_path(dfs).text
    except SpecificationError as error:
        raise GuardError(str(error)) from error
    lines = text.splitlines(keepends=True)
    by_id = {item.slice_id: item for item in parse_dfs_slices(text)}
    blocks: list[str] = []
    for slice_id in slice_ids:
        if not DFS_SLICE_ID.fullmatch(slice_id):
            raise GuardError(f"Malformed FS slice id: {slice_id}")
        item = by_id.get(slice_id)
        if item is None:
            raise GuardError(f"FS slice does not exist: {slice_id}")
        if item.claim_id != claim_id:
            raise GuardError(
                f"FS slice {slice_id} belongs to another claim: {item.claim_id}"
            )
        blocks.append(item.content)
    return "".join(blocks)


def red_dfs_claims(dfs_text: str) -> tuple[str, ...]:
    claims: list[str] = []
    for _, line in _outside_fences(dfs_text):
        match = RED_CLAIM.match(line)
        if match:
            claims.append(_normalize_reference(match.group("label")))
    return tuple(claims)


def specification_claims(specification_text: str) -> tuple[str, ...]:
    """Return active legacy lamps or canonical claim identities from slices.

    A functional-only FS intentionally has no implementation-status block.  Its
    durable slice claims replace status prose for ledger binding; a legacy FS
    retains its stricter red-lamp interpretation unchanged.
    """
    legacy = red_dfs_claims(specification_text)
    if legacy:
        return legacy
    return tuple(dict.fromkeys(item.claim_id for item in parse_dfs_slices(specification_text)))


def active_work_items(ledger_text: str) -> tuple[str, ...]:
    items: list[str] = []
    for _, line in _outside_fences(ledger_text):
        match = ACTIVE_ITEM.match(line)
        if match:
            items.append(_normalize_reference(match.group("reference")))
    return tuple(items)


def _active_work_blocks(ledger_text: str) -> tuple[tuple[str, str], ...]:
    lines = ledger_text.splitlines()
    starts: list[tuple[int, str]] = []
    boundaries: list[int] = []
    for line_number, line in _outside_fences(ledger_text):
        if STABLE_CLAIM.match(line):
            boundaries.append(line_number - 1)
        match = ACTIVE_ITEM.match(line)
        if match:
            starts.append(
                (line_number - 1, _normalize_reference(match.group("reference")))
            )
    blocks: list[tuple[str, str]] = []
    for start, reference in starts:
        end = next(
            (boundary for boundary in boundaries if boundary > start),
            len(lines),
        )
        blocks.append((reference, "\n".join(lines[start:end])))
    return tuple(blocks)


def _ledger_slice_ids(block: str, reference: str) -> tuple[str, ...]:
    pointer_lines = [
        line for line in block.splitlines() if DFS_SLICE_LEDGER_TOKEN in line
    ]
    if not pointer_lines:
        raise GuardError(f"Active work item has no FS slices: {reference}")
    if len(pointer_lines) != 1:
        raise GuardError(
            f"Active work item must have exactly one FS slices line: {reference}"
        )
    match = DFS_SLICE_LEDGER.fullmatch(pointer_lines[0])
    if match is None:
        raise GuardError(f"Malformed FS slices line for active item: {reference}")
    slice_ids = tuple(re.findall(r"`([^`]+)`", match.group("ids")))
    if not slice_ids or len(slice_ids) != len(set(slice_ids)):
        raise GuardError(
            f"Active work item needs unique FS slice ids: {reference}"
        )
    return slice_ids


def dfs_slice_status(
    ledger: Path,
    dfs: Path,
) -> tuple[tuple[str, str, str], ...]:
    """Report bootstrap status without weakening strict work-ledger validation."""

    ledger_text = read_markdown(ledger)
    blocks = _active_work_blocks(ledger_text)
    try:
        specification = resolve_path(dfs)
    except SpecificationError as error:
        raise GuardError(str(error)) from error
    dfs_text = specification.text
    red_claims = specification_claims(dfs_text)
    slices_by_id = {item.slice_id: item for item in parse_dfs_slices(dfs_text)}
    statuses: list[tuple[str, str, str]] = []
    for reference, block in blocks:
        matches = [claim for claim in red_claims if _same_claim(reference, claim)]
        if len(matches) != 1:
            statuses.append(
                (reference, "invalid", "not exactly one still-red FS claim")
            )
            continue
        claim_id = _selected_claim_id(matches[0])
        pointer_lines = [
            line for line in block.splitlines() if DFS_SLICE_LEDGER_TOKEN in line
        ]
        if not pointer_lines:
            statuses.append((reference, "missing", "no FS slices line"))
            continue
        try:
            slice_ids = _ledger_slice_ids(block, reference)
        except GuardError as error:
            statuses.append((reference, "invalid", str(error)))
            continue
        problem = ""
        for slice_id in slice_ids:
            item = slices_by_id.get(slice_id)
            if item is None:
                problem = f"missing FS slice {slice_id}"
                break
            if item.claim_id != claim_id:
                problem = (
                    f"FS slice {slice_id} belongs to another claim {item.claim_id}"
                )
                break
        if problem:
            statuses.append((reference, "invalid", problem))
        else:
            statuses.append((reference, "ready", ", ".join(slice_ids)))
    return tuple(statuses)


def _stored_task_rows(state: Path, lineage_id: str) -> tuple[tuple[str, str], ...]:
    if not state.is_file():
        raise GuardError(f"Deadline state does not exist: {state}")
    try:
        connection = sqlite3.connect(state.resolve().as_uri() + "?mode=ro", uri=True)
        connection.row_factory = sqlite3.Row
        binding = connection.execute(
            "SELECT lineage_id FROM lineage_binding WHERE singleton = 1"
        ).fetchone()
        if binding is None or binding["lineage_id"] != lineage_id:
            raise GuardError("Work-ledger lineage does not match the deadline state")
        rows = connection.execute(
            "SELECT task_id, claim_id FROM tasks WHERE lineage_id = ?",
            (lineage_id,),
        ).fetchall()
    except sqlite3.Error as error:
        raise GuardError(f"Cannot read deadline state: {error}") from error
    finally:
        if "connection" in locals():
            connection.close()
    return tuple((str(row["task_id"]), str(row["claim_id"])) for row in rows)


def _mentions_task(text: str, task_id: str) -> bool:
    return re.search(
        rf"(?<![A-Za-z0-9_-]){re.escape(task_id)}(?![A-Za-z0-9_-])",
        text,
    ) is not None


def validate_work_ledger(
    ledger: Path,
    dfs: Path,
    *,
    state: Path | None = None,
    lineage_id: str | None = None,
) -> tuple[str, ...]:
    """Bind current entries to specification slices and preserve task ownership."""

    ledger_text = read_markdown(ledger)
    blocks = _active_work_blocks(ledger_text)
    items = tuple(reference for reference, _ in blocks)
    try:
        specification = resolve_path(dfs)
    except SpecificationError as error:
        raise GuardError(str(error)) from error
    dfs_text = specification.text
    red_claims = specification_claims(dfs_text)
    slices = parse_dfs_slices(dfs_text)
    slices_by_id = {item.slice_id: item for item in slices}
    selected_claims: list[str] = []
    for reference, block in blocks:
        matches = [claim for claim in red_claims if _same_claim(reference, claim)]
        if not matches:
            raise GuardError(f"Work item does not reference a still-red FS claim: {reference}")
        if len(matches) > 1:
            raise GuardError(f"Work item reference is ambiguous in the FS: {reference}")
        claim = matches[0]
        claim_id = _selected_claim_id(claim)
        slice_ids = _ledger_slice_ids(block, reference)
        for slice_id in slice_ids:
            item = slices_by_id.get(slice_id)
            if item is None:
                raise GuardError(
                    f"Active work item references missing FS slice {slice_id}: {reference}"
                )
            if item.claim_id != claim_id:
                raise GuardError(
                    f"Active work item references FS slice {slice_id} owned by another claim: "
                    f"{item.claim_id}"
                )
        selected_claims.append(claim)

    if (state is None) != (lineage_id is None):
        raise GuardError("Work-ledger clock validation needs both state and lineage")
    if state is not None and lineage_id is not None:
        task_rows = _stored_task_rows(state, lineage_id)
        for (reference, block), claim in zip(blocks, selected_claims):
            mentioned = {
                (task_id, task_claim)
                for task_id, task_claim in task_rows
                if _mentions_task(block, task_id)
            }
            # Independent assignments and valid earlier evidence may share a
            # claim. Their count does not change task-to-claim ownership.
            for _, task_claim in mentioned:
                if _stable_key(task_claim) != _stable_key(claim):
                    raise GuardError(
                        f"Work item mentions a task owned by another FS claim: {reference}"
                    )
    return items


def validate_dfs_completion(before: Path, after: Path, selected_claim: str) -> str:
    """Allow exactly one selected red marker to become an accepted marker."""

    baseline = read_markdown(before)
    candidate = read_markdown(after)
    selected = _normalize_reference(selected_claim)
    lines = baseline.splitlines(keepends=True)
    matches: list[tuple[int, re.Match[str], str]] = []
    for index, line in enumerate(lines):
        body = line.rstrip("\r\n")
        match = RED_CLAIM.match(body)
        if match:
            label = _normalize_reference(match.group("label"))
            if _same_claim(selected, label):
                matches.append((index, match, label))

    if len(matches) != 1:
        raise GuardError(
            f"Selected claim must identify exactly one still-red FS claim: {selected}"
        )

    index, match, label = matches[0]
    original = lines[index]
    body = original.rstrip("\r\n")
    ending = original[len(body) :]
    lines[index] = (
        f"{match.group('lead')}[x] {match.group('label')}"
        f"{match.group('trailing')}{ending}"
    )
    expected = "".join(lines)
    if candidate != expected:
        raise GuardError(
            "FS completion must only change the selected '[ ] 🔴' marker to '[x]'"
        )
    return label


def validate_dfs_reopen(before: Path, after: Path, selected_claim: str) -> str:
    """Allow exactly one invalidated accepted marker to return to red work."""

    baseline = read_markdown(before)
    candidate = read_markdown(after)
    selected = _normalize_reference(selected_claim)
    lines = baseline.splitlines(keepends=True)
    matches: list[tuple[int, re.Match[str], str]] = []
    for index, line in enumerate(lines):
        body = line.rstrip("\r\n")
        match = STABLE_CLAIM.match(body)
        if match and match.group("status").lower() == "x" and match.group("red") is None:
            label = _normalize_reference(match.group("label"))
            if _same_claim(selected, label):
                matches.append((index, match, label))

    if len(matches) != 1:
        raise GuardError(
            f"Selected claim must identify exactly one accepted FS claim: {selected}"
        )

    index, match, label = matches[0]
    original = lines[index]
    body = original.rstrip("\r\n")
    ending = original[len(body) :]
    lines[index] = (
        f"- [ ] 🔴 {match.group('label')}"
        f"{match.group('trailing')}{ending}"
    )
    expected = "".join(lines)
    if candidate != expected:
        raise GuardError(
            "FS reopening must only change the selected '[x]' marker to '[ ] 🔴'"
        )
    return label


def validate_universal_dfs_mutation(before: Path, candidate: Path) -> bool:
    """Allow a redesigned red map while preserving contract and accepted frontier."""

    baseline = read_markdown(before)
    proposed = read_markdown(candidate)
    if baseline == proposed:
        return False
    for heading in PROTECTED_DFS_SECTIONS:
        if _exact_markdown_section(baseline, heading) != _exact_markdown_section(
            proposed, heading
        ):
            raise GuardError(f"Universal FS candidate cannot change {heading}")

    baseline_records = _stable_claim_records(baseline)
    candidate_records = _stable_claim_records(proposed)
    candidate_by_id = _claims_by_id(candidate_records, "Candidate")
    accepted = [record for record in baseline_records if record[1].lower() == "x"]
    positions = {record[0]: index for index, record in enumerate(candidate_records)}
    prior_position = -1
    for record in accepted:
        replacement = candidate_by_id.get(record[0])
        if replacement is None or replacement[3] != record[3]:
            raise GuardError(
                f"Universal FS candidate cannot delete or rewrite accepted claim {record[0]}"
            )
        position = positions[record[0]]
        if position <= prior_position:
            raise GuardError("Universal FS candidate cannot reorder accepted claims")
        prior_position = position
    baseline_slices = {
        item.slice_id: item
        for item in parse_dfs_slices(baseline)
    }
    candidate_slices = {
        item.slice_id: item for item in parse_dfs_slices(proposed)
    }
    for slice_id, item in baseline_slices.items():
        replacement = candidate_slices.get(slice_id)
        if replacement is None or replacement.claim_id != item.claim_id:
            raise GuardError(
                "Universal FS candidate cannot remove or rebind durable "
                f"slice {slice_id}"
            )
    return True


def _inclusive_range(value: str) -> tuple[int, int]:
    match = re.fullmatch(r"([0-9]+):([0-9]+)", value)
    if match is None:
        raise argparse.ArgumentTypeError("range must be START:END using inclusive lines")
    return int(match.group(1)), int(match.group(2))


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)

    guidelines = commands.add_parser("guidelines", help="Validate guideline mutation scope")
    guidelines.add_argument("--baseline", type=Path, required=True)
    guidelines.add_argument("--candidate", type=Path, required=True)
    guidelines.add_argument("--state", type=Path, required=True)
    guidelines.add_argument("--lineage", required=True)
    guidelines.add_argument("--task", required=True)
    guidelines.add_argument("--incident-kind", choices=INCIDENT_KINDS, required=True)
    guidelines.add_argument("--ledger-candidate", type=Path)
    guidelines.add_argument("--method-baseline", type=Path)
    guidelines.add_argument("--method-candidate", type=Path)

    ledger = commands.add_parser("work-ledger", help="Validate active work against red FS claims")
    ledger.add_argument("--ledger", type=Path, required=True)
    ledger.add_argument("--dfs", type=Path, required=True)
    ledger.add_argument("--state", type=Path, required=True)
    ledger.add_argument("--lineage", required=True)

    slice_status = commands.add_parser(
        "dfs-slice-status",
        help="Report ready, missing, or invalid active-item FS slice pointers",
    )
    slice_status.add_argument("--ledger", type=Path, required=True)
    slice_status.add_argument("--dfs", type=Path, required=True)

    mark_slices = commands.add_parser(
        "mark-dfs-slices",
        help="Atomically add durable claim-bound FS slice markers",
    )
    mark_slices.add_argument("--source", type=Path, required=True)
    mark_slices.add_argument("--output", type=Path, required=True)
    mark_slices.add_argument("--claim", required=True)
    mark_slices.add_argument(
        "--range", dest="ranges", action="append", type=_inclusive_range, required=True
    )

    extract_slices = commands.add_parser(
        "extract-dfs-slices",
        help="Extract only selected validated FS slice content",
    )
    extract_slices.add_argument("--dfs", type=Path, required=True)
    extract_slices.add_argument("--claim", required=True)
    extract_slices.add_argument(
        "--slice", dest="slice_ids", action="append", required=True
    )
    extract_slices.add_argument("--output", type=Path)

    completion = commands.add_parser("complete-dfs", help="Validate one accepted FS claim")
    completion.add_argument("--before", type=Path, required=True)
    completion.add_argument("--after", type=Path, required=True)
    completion.add_argument("--claim", required=True)
    completion.add_argument("--state", type=Path, required=True)
    completion.add_argument("--lineage", required=True)
    completion.add_argument("--task", required=True)

    reopen = commands.add_parser(
        "reopen-dfs",
        help="Validate one currently invalidated accepted claim returning to red",
    )
    reopen.add_argument("--before", type=Path, required=True)
    reopen.add_argument("--after", type=Path, required=True)
    reopen.add_argument("--claim", required=True)
    reopen.add_argument("--state", type=Path, required=True)
    reopen.add_argument("--lineage", required=True)

    expansion = commands.add_parser(
        "expand-dfs", help="Validate expansion from one stored worker finding"
    )
    expansion.add_argument("--before", type=Path, required=True)
    expansion.add_argument("--candidate", type=Path, required=True)
    expansion.add_argument("--state", type=Path, required=True)
    expansion.add_argument("--lineage", required=True)
    expansion.add_argument("--task", required=True)
    expansion.add_argument("--ledger-candidate", type=Path)

    owner_expansion = commands.add_parser(
        "owner-expand-dfs",
        help="Validate same-outcome FS expansion from pending owner guidance",
    )
    owner_expansion.add_argument("--before", type=Path, required=True)
    owner_expansion.add_argument("--candidate", type=Path, required=True)
    owner_expansion.add_argument("--ledger-before", type=Path, required=True)
    owner_expansion.add_argument("--ledger-candidate", type=Path, required=True)

    random_review = commands.add_parser(
        "random-review",
        help="Validate one due random improvement-review lane",
    )
    random_review.add_argument("--baseline", type=Path, required=True)
    random_review.add_argument("--candidate", type=Path, required=True)
    random_review.add_argument("--state", type=Path, required=True)
    random_review.add_argument("--lineage", required=True)
    random_review.add_argument("--cycle", type=int, required=True)
    random_review.add_argument("--ledger-candidate", type=Path)
    random_review.add_argument("--method-baseline", type=Path)
    random_review.add_argument("--method-candidate", type=Path)

    universal_review = commands.add_parser(
        "universal-review",
        help="Validate an isolated whole-method candidate for a due rare review",
    )
    universal_review.add_argument("--baseline", type=Path, required=True)
    universal_review.add_argument("--candidate", type=Path, required=True)
    universal_review.add_argument("--state", type=Path, required=True)
    universal_review.add_argument("--workspace-config", type=Path, required=True)
    universal_review.add_argument("--lineage", required=True)
    universal_review.add_argument("--cycle", type=int, required=True)
    universal_review.add_argument("--dfs-before", type=Path)
    universal_review.add_argument("--dfs-candidate", type=Path)
    return parser


def main(argv: list[str] | None = None) -> int:
    arguments = build_parser().parse_args(argv)
    try:
        if arguments.command == "guidelines":
            broader_mutation = broader_mutation_from_incident(
                arguments.state,
                arguments.lineage,
                arguments.task,
                arguments.incident_kind,
            )
            changed = validate_guideline_mutation(
                arguments.baseline,
                arguments.candidate,
                broader_mutation=broader_mutation,
                require_change=False,
            )
            if (arguments.method_baseline is None) != (
                arguments.method_candidate is None
            ):
                raise GuardError(
                    "Deadline method validation needs both baseline and candidate roots"
                )
            method_changed: tuple[str, ...] = ()
            if arguments.method_baseline is not None:
                method_changed = validate_method_mutation(
                    arguments.method_baseline,
                    arguments.method_candidate,
                    universal=False,
                    require_change=False,
                )
            if not changed and not method_changed:
                raise GuardError(
                    "Deadline macro mutation needs an evidence-backed guideline or method change"
                )
            (
                candidate_digest,
                protected_baseline_digest,
                live_tree_digest,
                changed_paths,
            ) = normal_method_candidate_snapshot(
                arguments.candidate,
                arguments.method_candidate,
            )
            receipt_id = persist_normal_method_receipt(
                arguments.state,
                arguments.lineage,
                arguments.task,
                arguments.incident_kind,
                candidate_digest,
                changed_paths,
                protected_baseline_digest,
                live_tree_digest,
            )
            print(
                "ok: guarded incident macro mutation; candidate changed "
                + ", ".join(changed_paths)
                + f"; receipt {receipt_id}"
            )
        elif arguments.command == "work-ledger":
            items = validate_work_ledger(
                arguments.ledger,
                arguments.dfs,
                state=arguments.state,
                lineage_id=arguments.lineage,
            )
            print(f"ok: {len(items)} active work items")
        elif arguments.command == "dfs-slice-status":
            statuses = dfs_slice_status(arguments.ledger, arguments.dfs)
            for reference, status, detail in statuses:
                print(f"{status}: {reference}: {detail}")
            if any(status == "invalid" for _, status, _ in statuses):
                return 1
        elif arguments.command == "mark-dfs-slices":
            slice_ids = insert_dfs_slices(
                arguments.source,
                arguments.output,
                arguments.claim,
                tuple(arguments.ranges),
            )
            print("ok: FS slices " + ", ".join(slice_ids))
        elif arguments.command == "extract-dfs-slices":
            extracted = extract_dfs_slices(
                arguments.dfs,
                arguments.claim,
                tuple(arguments.slice_ids),
            )
            if arguments.output is None:
                _write_stdout_utf8(extracted)
            else:
                _atomic_write_utf8(arguments.output, extracted)
                print(
                    f"ok: wrote {len(arguments.slice_ids)} FS slices to "
                    f"{arguments.output}"
                )
        elif arguments.command == "complete-dfs":
            validate_accepted_task_state(
                arguments.state,
                arguments.lineage,
                arguments.task,
                arguments.claim,
            )
            claim = validate_dfs_completion(
                arguments.before, arguments.after, arguments.claim
            )
            print(f"ok: completed {claim}")
        elif arguments.command == "reopen-dfs":
            _, trigger_kind = validate_invalidated_claim_state(
                arguments.state,
                arguments.lineage,
                arguments.claim,
            )
            claim = validate_dfs_reopen(
                arguments.before, arguments.after, arguments.claim
            )
            print(f"ok: reopened {claim} from {trigger_kind}")
        elif arguments.command == "expand-dfs":
            task_claim, finding_kind = worker_finding_from_state(
                arguments.state,
                arguments.lineage,
                arguments.task,
            )
            added = validate_dfs_expansion(
                arguments.before,
                arguments.candidate,
                task_claim,
            )
            print(
                f"ok: {finding_kind} finding expanded {task_claim}; added "
                + ", ".join(added)
            )
        elif arguments.command == "owner-expand-dfs":
            added, consumed = validate_owner_dfs_expansion(
                arguments.before,
                arguments.candidate,
                arguments.ledger_before,
                arguments.ledger_candidate,
            )
            print(
                "ok: owner-guided FS expansion added "
                + ", ".join(added)
                + f"; consumed {len(consumed)} owner suggestion(s)"
            )
        elif arguments.command == "random-review":
            lane = random_review_from_state(
                arguments.state,
                arguments.lineage,
                arguments.cycle,
            )
            changed = validate_random_review_mutation(
                arguments.baseline,
                arguments.candidate,
                selected_lane=lane,
            )
            if (arguments.method_baseline is None) != (
                arguments.method_candidate is None
            ):
                raise GuardError(
                    "Random method validation needs both baseline and candidate roots"
                )
            method_changed: tuple[str, ...] = ()
            if arguments.method_baseline is not None:
                method_changed = validate_method_mutation(
                    arguments.method_baseline,
                    arguments.method_candidate,
                    universal=False,
                    require_change=False,
                )
            all_changed = (*changed, *method_changed)
            if all_changed:
                print(
                    f"ok: random review cycle {arguments.cycle}; changed "
                    + ", ".join(all_changed)
                )
            else:
                print(
                    f"ok: random review cycle {arguments.cycle}; guarded no-op"
                )
        else:
            cycle = require_universal_random_review(
                arguments.state,
                arguments.lineage,
                arguments.cycle,
            )
            capability_roster_digest = require_universal_reviewer_capability(cycle)
            changed = validate_method_mutation(
                arguments.baseline,
                arguments.candidate,
                universal=True,
                require_change=False,
            )
            if (arguments.dfs_before is None) != (arguments.dfs_candidate is None):
                raise GuardError(
                    "Universal FS validation needs both baseline and candidate files"
                )
            dfs_changed = False
            if arguments.dfs_before is not None:
                dfs_changed = validate_universal_dfs_mutation(
                    arguments.dfs_before,
                    arguments.dfs_candidate,
                )
            if not changed and not dfs_changed:
                raise GuardError("Universal mutation candidate makes no change")
            changed_labels = (*changed, *((DFS_FILE,) if dfs_changed else ()))
            candidate_digest = universal_candidate_digest(
                arguments.candidate,
                arguments.dfs_candidate,
            )
            receipt_id = persist_universal_review_receipt(
                arguments.state,
                arguments.lineage,
                arguments.cycle,
                candidate_digest,
                tuple(changed_labels),
                capability_roster_digest,
            )
            print(
                f"ok: universal review cycle {arguments.cycle}; candidate changed "
                + ", ".join(changed_labels)
                + f"; receipt {receipt_id}"
            )
    except (GuardError, OSError, UnicodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
