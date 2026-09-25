#!/usr/bin/env python3
"""Read-only semantic trajectory report for one DE67 claim."""

from __future__ import annotations

import argparse
import json
import math
import re
import sqlite3
import subprocess
from collections import Counter
from contextlib import closing
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Iterable


class TrajectoryError(RuntimeError):
    """Raised when a trajectory report cannot be derived honestly."""


STOPWORDS = {
    "a", "an", "and", "are", "as", "at", "be", "before", "by", "for", "from",
    "has", "if", "in", "into", "is", "it", "its", "of", "on", "one", "only",
    "or", "that", "the", "then", "this", "through", "to", "with",
}
TOKEN = re.compile(r"[A-Za-z][A-Za-z0-9_:-]*")
PRODUCT_SUFFIXES = {
    ".c", ".cc", ".cpp", ".cs", ".go", ".h", ".hpp", ".java", ".js", ".jsx",
    ".kt", ".kts", ".m", ".mm", ".py", ".rs", ".swift", ".ts", ".tsx",
}
TEST_PATH_PARTS = {"test", "tests", "testing"}
SUPPORT_PATH_PARTS = {".de67", "harness", "scripts", "tools"}


@dataclass(frozen=True)
class ChurnObservation:
    direction: str
    evidence: tuple[str, ...]


@dataclass(frozen=True)
class ChurnVector:
    product_owner: ChurnObservation
    proof_depth: ChurnObservation
    artifact_identity: ChurnObservation
    attempt_novelty: ChurnObservation
    code_surface: ChurnObservation
    accepted_frontier: ChurnObservation


@dataclass(frozen=True)
class GapReport:
    gap_id: str
    revision: int
    summary: str
    status: str
    implementation_relation: float
    implementation_unit: str | None
    test_relation: float
    test_unit: str | None
    accepted_proof: bool
    attempts: int
    completed: int
    findings: int
    abandoned: int
    attempt_path: tuple[str, ...]


@dataclass(frozen=True)
class SubtaskReport:
    subtask_id: str
    status: str
    summary: str


@dataclass(frozen=True)
class AttentionPoint:
    gap_id: str
    raw_relation: float
    relative_pull: float


@dataclass(frozen=True)
class AttentionSeries:
    key: str
    label: str
    source: str
    points: tuple[AttentionPoint, ...]


@dataclass(frozen=True)
class TrajectoryReport:
    lineage: str
    claim: str
    closure_sequence: int
    gaps: tuple[GapReport, ...]
    subtasks: tuple[SubtaskReport, ...]
    latest_task: str | None
    latest_task_gap: str | None
    latest_task_result: str
    attention: tuple[AttentionSeries, ...]
    churn_vector: ChurnVector


SUBTASK_ROW = re.compile(
    r"^    - \[(open|active|done|finding)\] ([a-z0-9][a-z0-9-]*) :: (.+)$"
)


def ledger_subtasks(workspace: Path, claim: str) -> tuple[SubtaskReport, ...]:
    """Parse explicit progress subdivisions for one active ledger item."""
    ledger = workspace / ".de67" / "work-ledger.md"
    if not ledger.is_file():
        return ()
    lines = ledger.read_text(encoding="utf-8").splitlines()
    item = re.compile(rf"^- \[ \] {re.escape(claim)}(?=[ \t]+—|[ \t]*$)")
    start = next((index for index, line in enumerate(lines) if item.match(line)), None)
    if start is None:
        return ()
    end = next(
        (index for index in range(start + 1, len(lines)) if lines[index].startswith("- [")),
        len(lines),
    )
    block = lines[start + 1:end]
    heading = next((index for index, line in enumerate(block) if line == "  - Subtasks:"), None)
    if heading is None:
        return ()
    parsed: list[SubtaskReport] = []
    seen: set[str] = set()
    for line in block[heading + 1:]:
        if not line.strip():
            continue
        if not line.startswith("    "):
            break
        match = SUBTASK_ROW.fullmatch(line)
        if match is None:
            raise TrajectoryError(
                f"Malformed subtask row for {claim}: {line!r}; expected "
                "    - [STATE] ID :: DESCRIPTION"
            )
        status, subtask_id, summary = match.groups()
        if subtask_id in seen:
            raise TrajectoryError(f"Duplicate subtask id for {claim}: {subtask_id}")
        seen.add(subtask_id)
        parsed.append(SubtaskReport(subtask_id, status, summary.strip()))
    if not parsed:
        raise TrajectoryError(f"Subtasks heading for {claim} has no valid rows")
    return tuple(parsed)


def git_changed_paths(workspace: Path) -> tuple[str, ...]:
    result = subprocess.run(
        ["git", "-C", str(workspace), "diff", "--name-only", "--no-ext-diff"],
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    if result.returncode != 0:
        raise TrajectoryError(result.stderr.strip() or "git diff --name-only failed")
    return tuple(line.strip() for line in result.stdout.splitlines() if line.strip())


def classify_path(path: str) -> str:
    parsed = Path(path)
    parts = {part.lower() for part in parsed.parts}
    if any(part in TEST_PATH_PARTS or "test" in part for part in parts):
        return "test"
    if parts & SUPPORT_PATH_PARTS:
        return "support"
    if parsed.suffix.lower() in PRODUCT_SUFFIXES:
        return "product"
    return "other"


def observation(direction: str, *evidence: str) -> ChurnObservation:
    return ChurnObservation(direction, tuple(item for item in evidence if item))


def derive_churn_vector(
    gaps: list[GapReport], changed_paths: tuple[str, ...], latest_result: str
) -> ChurnVector:
    surfaces = Counter(classify_path(path) for path in changed_paths)
    product_count = surfaces["product"]
    test_count = surfaces["test"]
    support_count = surfaces["support"]
    other_count = surfaces["other"]

    if product_count:
        product_owner = observation(
            "product-surface-present", f"{product_count} changed product path(s)"
        )
    elif changed_paths:
        product_owner = observation(
            "support-only-movement", "changed paths do not include a product source path"
        )
    else:
        product_owner = observation("no-uncommitted-movement", "git diff is empty")

    frontier_gaps = [gap for gap in gaps if gap.status == "open"] or gaps
    obligation_text = " ".join(gap.summary.lower() for gap in frontier_gaps)
    if re.search(r"\b(terminal|receipt|aftermath|save/reload|survivor)\b", obligation_text):
        proof_depth = observation("terminal-route-targeted", "current obligation names terminal proof")
    elif re.search(r"\b(interaction|input|combat|gameplay|click)\b", obligation_text):
        proof_depth = observation("interaction-targeted", "current obligation names interaction proof")
    elif re.search(r"\b(startup|preflight|launch|focus|hud)\b", obligation_text):
        proof_depth = observation("startup-or-preflight-targeted", "current obligation remains at setup depth")
    else:
        proof_depth = observation("proof-depth-unclassified", "no proof-depth marker in current obligations")

    if re.search(r"\b(source|head|commit)\b", obligation_text) and re.search(
        r"\b(binary|executable|hash)\b", obligation_text
    ) and re.search(r"\b(pid|process|window)\b", obligation_text):
        artifact_identity = observation(
            "source-binary-runtime-bound", "current obligation binds source, binary, and runtime identity"
        )
    elif re.search(r"\b(binary|executable|hash|pid|process|build)\b", obligation_text):
        artifact_identity = observation(
            "partial-artifact-binding", "artifact identity terms are present but the full binding is not"
        )
    else:
        artifact_identity = observation("artifact-binding-unobserved", "no artifact identity terms observed")

    attempts = sum(gap.attempts for gap in gaps)
    findings = sum(gap.findings for gap in gaps)
    active = sum(
        1 for gap in gaps
        if gap.attempt_path and gap.attempt_path[-1].startswith("active")
    )
    if findings > 1 and any(gap.revision > 1 for gap in gaps):
        attempt_novelty = observation(
            "finding-led-revision-chain",
            f"{findings} finding return(s) across revised closure gaps",
        )
    elif active and findings:
        attempt_novelty = observation("active-after-finding", "an active attempt follows a finding return")
    elif attempts == 1:
        attempt_novelty = observation("single-attempt", "one attempt is recorded")
    elif attempts:
        attempt_novelty = observation("multi-attempt", f"{attempts} attempts are recorded")
    else:
        attempt_novelty = observation("untouched", "no attempts are recorded")

    surface_evidence = (
        f"product={product_count}, test={test_count}, support={support_count}, other={other_count}"
    )
    nonzero = [name for name, count in surfaces.items() if count]
    if not changed_paths:
        code_surface = observation("clean", surface_evidence)
    elif len(nonzero) > 1:
        code_surface = observation("mixed-surface", surface_evidence)
    else:
        code_surface = observation(f"{nonzero[0]}-surface", surface_evidence)

    open_count = sum(gap.status == "open" for gap in gaps)
    proved_count = len(gaps) - open_count
    if not open_count:
        accepted_frontier = observation("all-current-gaps-proved", f"{proved_count} accepted gap(s)")
    elif latest_result == "active":
        accepted_frontier = observation(
            "active-open-frontier", f"{open_count} open gap(s); latest attempt is active"
        )
    else:
        accepted_frontier = observation(
            "open-frontier-without-latest-closure",
            f"{open_count} open gap(s); latest result is {latest_result}",
        )
    return ChurnVector(
        product_owner,
        proof_depth,
        artifact_identity,
        attempt_novelty,
        code_surface,
        accepted_frontier,
    )


def tokenize(text: str) -> list[str]:
    expanded = re.sub(r"([a-z0-9])([A-Z])", r"\1 \2", text)
    tokens: list[str] = []
    for match in TOKEN.findall(expanded.lower()):
        for part in re.split(r"[_:-]+", match):
            if len(part) > 1 and part not in STOPWORDS:
                tokens.append(part)
    return tokens


def tfidf_vectors(documents: Iterable[str]) -> list[dict[str, float]]:
    tokenized = [tokenize(document) for document in documents]
    document_count = len(tokenized)
    frequencies = Counter(
        token for tokens in tokenized for token in set(tokens)
    )
    vectors: list[dict[str, float]] = []
    for tokens in tokenized:
        counts = Counter(tokens)
        vector = {
            token: count * (math.log((document_count + 1) / (frequencies[token] + 1)) + 1)
            for token, count in counts.items()
        }
        norm = math.sqrt(sum(value * value for value in vector.values()))
        vectors.append(
            {token: value / norm for token, value in vector.items()} if norm else {}
        )
    return vectors


def cosine(left: dict[str, float], right: dict[str, float]) -> float:
    if len(left) > len(right):
        left, right = right, left
    return sum(value * right.get(token, 0.0) for token, value in left.items())


def git_diff_units(workspace: Path) -> tuple[list[tuple[str, str]], list[tuple[str, str]]]:
    result = subprocess.run(
        ["git", "-C", str(workspace), "diff", "--unified=0", "--no-ext-diff"],
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    if result.returncode != 0:
        raise TrajectoryError(result.stderr.strip() or "git diff failed")
    production: list[tuple[str, str]] = []
    tests: list[tuple[str, str]] = []
    path = ""
    header = ""
    lines: list[str] = []

    def flush() -> None:
        nonlocal lines
        if not lines:
            return
        added_unit = next(
            (
                line.strip()
                for line in lines
                if re.search(r"\b(TEST_CASE|SCENARIO|def|class|struct)\s*[( ]", line)
            ),
            None,
        )
        unit_name = added_unit or header
        label = f"{path} :: {unit_name}" if unit_name else path
        parsed = Path(path)
        if "test" in parsed.name.lower() or "tests" in parsed.parts:
            target = tests
        elif parsed.suffix.lower() in PRODUCT_SUFFIXES and ".de67" not in parsed.parts:
            target = production
        else:
            lines = []
            return
        target.append((label, "\n".join([path, header, *lines])))
        lines = []

    for line in result.stdout.splitlines():
        if line.startswith("+++ b/"):
            flush()
            path = line[6:]
            header = ""
        elif line.startswith("@@"):
            flush()
            header = line.split("@@", 2)[-1].strip()
        elif path and line.startswith(("+", "-")) and not line.startswith(("+++", "---")):
            lines.append(line[1:])
    flush()
    return production, tests


def nearest_relation(
    obligation: dict[str, float],
    units: list[tuple[str, str]],
    vectors: list[dict[str, float]],
) -> tuple[float, str | None]:
    if not units:
        return 0.0, None
    score, index = max((cosine(obligation, vector), index) for index, vector in enumerate(vectors))
    return round(score, 3), units[index][0]


def attention_series(
    key: str,
    label: str,
    source: str,
    gap_ids: list[str],
    relations: list[float],
) -> AttentionSeries:
    strongest = max(relations, default=0.0)
    points = tuple(
        AttentionPoint(
            gap_id,
            round(relation, 3),
            round(relation / strongest, 3) if strongest else 0.0,
        )
        for gap_id, relation in zip(gap_ids, relations)
    )
    return AttentionSeries(key, label, source, points)


def table_columns(connection: sqlite3.Connection, table: str) -> set[str]:
    return {str(row["name"]) for row in connection.execute(f"PRAGMA table_info({table})")}


def compact_path(kinds: list[str]) -> tuple[str, ...]:
    compact: list[str] = []
    for kind in kinds:
        if compact and compact[-1].split(" x", 1)[0] == kind:
            previous = compact.pop()
            count = int(previous.split(" x", 1)[1]) + 1 if " x" in previous else 2
            compact.append(f"{kind} x{count}")
        else:
            compact.append(kind)
    return tuple(compact)


def readonly_connection(state: Path) -> sqlite3.Connection:
    if not state.is_file():
        raise TrajectoryError(f"Deadline state does not exist: {state}")
    connection = sqlite3.connect(state.resolve().as_uri() + "?mode=ro", uri=True)
    connection.row_factory = sqlite3.Row
    return connection


def infer_lineage(connection: sqlite3.Connection, requested: str | None) -> str:
    if requested:
        return requested
    rows = connection.execute("SELECT lineage_id FROM lineage_binding").fetchall()
    if len(rows) != 1:
        raise TrajectoryError("Specify --lineage when state does not contain exactly one lineage")
    return str(rows[0][0])


def build_report(
    workspace: Path,
    state: Path,
    claim: str,
    lineage: str | None = None,
) -> TrajectoryReport:
    changed_paths = git_changed_paths(workspace)
    production_units, test_units = git_diff_units(workspace)
    subtasks = ledger_subtasks(workspace, claim)
    with closing(readonly_connection(state)) as connection:
        lineage_id = infer_lineage(connection, lineage)
        sequence_row = connection.execute(
            """
            SELECT MAX(closure_sequence)
            FROM closure_gaps
            WHERE lineage_id = ? AND claim_id = ?
            """,
            (lineage_id, claim),
        ).fetchone()
        if sequence_row is None or sequence_row[0] is None:
            raise TrajectoryError(f"No closure gaps found for {lineage_id}/{claim}")
        sequence = int(sequence_row[0])
        rows = connection.execute(
            """
            SELECT g.gap_id, g.closed_at, g.closure_evidence,
                   r.revision, r.description, r.proof_route
            FROM closure_gaps AS g
            JOIN closure_gap_revisions AS r
              ON r.lineage_id = g.lineage_id
             AND r.claim_id = g.claim_id
             AND r.closure_sequence = g.closure_sequence
             AND r.gap_id = g.gap_id
            WHERE g.lineage_id = ? AND g.claim_id = ?
              AND g.closure_sequence = ?
              AND r.revision = (
                  SELECT MAX(r2.revision)
                  FROM closure_gap_revisions AS r2
                  WHERE r2.lineage_id = r.lineage_id
                    AND r2.claim_id = r.claim_id
                    AND r2.closure_sequence = r.closure_sequence
                    AND r2.gap_id = r.gap_id
              )
            ORDER BY g.gap_id
            """,
            (lineage_id, claim, sequence),
        ).fetchall()
        if not rows:
            raise TrajectoryError(f"No current gap revisions found for {lineage_id}/{claim}")

        obligations = [f"{row['description']}\n{row['proof_route']}" for row in rows]
        unit_texts = [text for _, text in production_units] + [text for _, text in test_units]
        vectors = tfidf_vectors([*obligations, *unit_texts])
        obligation_vectors = vectors[: len(obligations)]
        production_vectors = vectors[len(obligations):len(obligations) + len(production_units)]
        test_vectors = vectors[len(obligations) + len(production_units):]
        reports: list[GapReport] = []
        for index, row in enumerate(rows):
            implementation_relation, implementation_unit = nearest_relation(
                obligation_vectors[index], production_units, production_vectors
            )
            test_relation, test_unit = nearest_relation(
                obligation_vectors[index], test_units, test_vectors
            )
            attempts = connection.execute(
                """
                SELECT attempt_terminal_kind
                FROM tasks
                WHERE lineage_id = ? AND claim_id = ?
                  AND phase_sequence_at_dispatch = ? AND closure_gap_id = ?
                ORDER BY started_at
                """,
                (lineage_id, claim, sequence, row["gap_id"]),
            ).fetchall()
            kinds = [str(attempt["attempt_terminal_kind"] or "active") for attempt in attempts]
            kind_counts = Counter(kinds)
            reports.append(
                GapReport(
                    gap_id=str(row["gap_id"]),
                    revision=int(row["revision"]),
                    summary=" ".join(str(row["description"]).split()),
                    status="proved" if row["closed_at"] is not None else "open",
                    implementation_relation=implementation_relation,
                    implementation_unit=implementation_unit,
                    test_relation=test_relation,
                    test_unit=test_unit,
                    accepted_proof=bool(row["closed_at"] is not None and row["closure_evidence"]),
                    attempts=len(kinds),
                    completed=kind_counts["completed"],
                    findings=kind_counts["finding"],
                    abandoned=kind_counts["abandoned"],
                    attempt_path=compact_path(kinds),
                )
            )
        latest = connection.execute(
            """
            SELECT task_id, closure_gap_id, attempt_terminal_kind
            FROM tasks
            WHERE lineage_id = ? AND claim_id = ?
            ORDER BY started_at DESC
            LIMIT 1
            """,
            (lineage_id, claim),
        ).fetchone()
        result_text = ""
        result_label = ""
        result_source = ""
        finding_columns = table_columns(connection, "worker_findings")
        if finding_columns:
            finding = connection.execute(
                """
                SELECT finding.task_id, finding.short_verdict, finding.evidence
                FROM worker_findings AS finding
                JOIN tasks AS task
                  ON task.lineage_id = finding.lineage_id
                 AND task.task_id = finding.task_id
                WHERE finding.lineage_id = ? AND task.claim_id = ?
                ORDER BY finding.reported_at DESC
                LIMIT 1
                """,
                (lineage_id, claim),
            ).fetchone()
            if finding is not None:
                result_text = f"{finding['short_verdict']}\n{finding['evidence']}"
                result_label = "Latest finding"
                result_source = str(finding["task_id"])
    latest_result = str(latest["attempt_terminal_kind"] or "active") if latest is not None else "none"
    axis_ids = [subtask.subtask_id for subtask in subtasks] or [report.gap_id for report in reports]
    axis_obligations = [subtask.summary for subtask in subtasks] or obligations
    axis_vectors = tfidf_vectors([
        *axis_obligations,
        *(text for _, text in production_units),
        *(text for _, text in test_units),
    ])
    obligation_count = len(axis_obligations)
    production_start = obligation_count
    test_start = production_start + len(production_units)
    code_relations = [
        nearest_relation(
            vector,
            production_units,
            axis_vectors[production_start:test_start],
        )[0]
        for vector in axis_vectors[:obligation_count]
    ]
    test_relations = [
        nearest_relation(vector, test_units, axis_vectors[test_start:])[0]
        for vector in axis_vectors[:obligation_count]
    ]
    active_gap = (
        str(latest["closure_gap_id"])
        if latest is not None and latest["closure_gap_id"]
        else ""
    )
    attention = [
        attention_series(
            "target",
            "Assigned gap",
            str(latest["task_id"]) if latest is not None else "No active task",
            axis_ids,
            ([1.0 if subtask.status == "active" else 0.0 for subtask in subtasks]
             if subtasks else [1.0 if gap_id == active_gap else 0.0 for gap_id in axis_ids]),
        ),
        attention_series(
            "code",
            "Workspace code",
            "Current uncommitted diff",
            axis_ids,
            code_relations,
        ),
        attention_series(
            "test",
            "Workspace tests",
            "Current uncommitted diff",
            axis_ids,
            test_relations,
        ),
    ]
    if result_text:
        result_vectors = tfidf_vectors([*axis_obligations, result_text])
        result_vector = result_vectors[-1]
        attention.append(
            attention_series(
                "result",
                result_label,
                result_source,
                axis_ids,
                [round(cosine(vector, result_vector), 3) for vector in result_vectors[:-1]],
            )
        )
    churn_vector = derive_churn_vector(reports, changed_paths, latest_result)
    return TrajectoryReport(
        lineage_id,
        claim,
        sequence,
        tuple(reports),
        subtasks,
        str(latest["task_id"]) if latest is not None else None,
        str(latest["closure_gap_id"]) if latest is not None and latest["closure_gap_id"] else None,
        latest_result,
        tuple(attention),
        churn_vector,
    )


def render_text(report: TrajectoryReport) -> str:
    lines = [f"{report.claim} / closure {report.closure_sequence}"]
    if report.latest_task:
        lines.append(
            f"Latest bound movement: {report.latest_task} -> {report.latest_task_gap or 'exploration'} ({report.latest_task_result})"
        )
    for gap in report.gaps:
        summary = gap.summary if len(gap.summary) <= 160 else gap.summary[:157].rstrip() + "..."
        lines.extend(
            [
                "",
                f"{gap.gap_id} revision {gap.revision}: {gap.status}",
                f"  obligation: {summary}",
                f"  nearest product-diff unit: {gap.implementation_unit or 'none'} ({gap.implementation_relation:.3f})",
                f"  nearest test-diff unit: {gap.test_unit or 'none'} ({gap.test_relation:.3f})",
                f"  accepted proof: {'present' if gap.accepted_proof else 'absent'}",
                f"  attempts: {gap.attempts} ({gap.completed} completed, {gap.findings} findings, {gap.abandoned} abandoned)",
                f"  path: {' -> '.join(gap.attempt_path) if gap.attempt_path else 'untouched'}",
            ]
        )
    shared_tests: dict[str, list[str]] = {}
    for gap in report.gaps:
        if gap.test_unit:
            shared_tests.setdefault(gap.test_unit, []).append(gap.gap_id)
    repeated = [(unit, gaps) for unit, gaps in shared_tests.items() if len(gaps) > 1]
    if repeated:
        lines.extend(["", "Shared nearest test units (not distinct coverage):"])
        for unit, gaps in repeated:
            lines.append(f"  {', '.join(gaps)} -> {unit}")
    lines.extend(["", "Churn vector (advisory observations, not a score):"])
    for name, value in asdict(report.churn_vector).items():
        evidence = "; ".join(value["evidence"]) or "none"
        lines.append(f"  {name.replace('_', ' ')}: {value['direction']} [{evidence}]")
    lines.extend(
        [
            "",
            "Relations are semantic proximity, not completion or proof.",
        ]
    )
    return "\n".join(lines)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workspace", type=Path, required=True)
    parser.add_argument("--state", type=Path, required=True)
    parser.add_argument("--claim", required=True)
    parser.add_argument("--lineage")
    parser.add_argument("--json", action="store_true")
    return parser


def main() -> int:
    arguments = build_parser().parse_args()
    try:
        report = build_report(
            arguments.workspace.resolve(),
            arguments.state.resolve(),
            arguments.claim,
            arguments.lineage,
        )
    except (OSError, sqlite3.Error, TrajectoryError) as error:
        print(f"error: {error}")
        return 1
    if arguments.json:
        print(json.dumps(asdict(report), indent=2))
    else:
        print(render_text(report))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
