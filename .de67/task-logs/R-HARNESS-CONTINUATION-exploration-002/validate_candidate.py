#!/usr/bin/env python3
"""Validate the isolated current-FS guard candidate without touching live method state."""

from __future__ import annotations

import hashlib
import json
import shutil
import sqlite3
import sys
from pathlib import Path


HERE = Path(__file__).resolve().parent
WORKSPACE = HERE.parents[2]
BASELINE_METHOD = HERE / "method-baseline"
CANDIDATE_METHOD = HERE / "method-candidate"
sys.path.insert(0, str(CANDIDATE_METHOD / "scripts"))
import mutation_guard as guard  # noqa: E402


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def changed_paths(before: Path, after: Path) -> list[str]:
    files = set()
    for root in (before, after):
        for path in root.rglob("*"):
            if path.is_file() and "__pycache__" not in path.parts:
                files.add(path.relative_to(root).as_posix())
    changed: list[str] = []
    for relative in files:
        before_path = before / relative
        after_path = after / relative
        before_bytes = before_path.read_bytes() if before_path.is_file() else None
        after_bytes = after_path.read_bytes() if after_path.is_file() else None
        if before_bytes != after_bytes:
            changed.append(relative)
    return sorted(changed)


def replace_once(text: str, old: str, new: str) -> str:
    if text.count(old) != 1:
        raise AssertionError(f"expected one occurrence: {old[:80]!r}")
    return text.replace(old, new, 1)


def reject(before: Path, candidate: Path, text: str, expected: str) -> str:
    candidate.write_text(text, encoding="utf-8")
    try:
        guard.validate_universal_dfs_mutation(before, candidate)
    except guard.GuardError as error:
        rendered = str(error)
        if expected not in rendered:
            raise AssertionError(f"wanted {expected!r}, got {rendered!r}")
        return rendered
    raise AssertionError(f"candidate unexpectedly passed; expected {expected!r}")


def main() -> int:
    live_guard = Path("/Users/josefhorvath/.codex/skills/de67/de-67-3/scripts/mutation_guard.py")
    baseline_guard = BASELINE_METHOD / "scripts/mutation_guard.py"
    candidate_guard = CANDIDATE_METHOD / "scripts/mutation_guard.py"
    if digest(live_guard) != digest(baseline_guard):
        raise AssertionError("installed method guard differs from isolated baseline")

    staged_changed = changed_paths(BASELINE_METHOD, CANDIDATE_METHOD)
    expected_changed = ["scripts/mutation_guard.py", "tests/test_mutation_guard.py"]
    if staged_changed != expected_changed:
        raise AssertionError(f"unexpected candidate paths: {staged_changed!r}")
    if guard.validate_method_mutation(BASELINE_METHOD, CANDIDATE_METHOD, universal=True) != tuple(staged_changed):
        raise AssertionError("isolated universal method candidate was not recognized exactly")
    for relative in (
        "scripts/deadline_harness.py",
        "assets/environment/phase3-policy.json",
        "assets/environment/phase3-policy.d67",
    ):
        if (BASELINE_METHOD / relative).read_bytes() != (CANDIDATE_METHOD / relative).read_bytes():
            raise AssertionError(f"candidate changed preserved kernel/policy path: {relative}")

    fs_text = (WORKSPACE / ".de67/FS.md").read_text(encoding="utf-8")
    baseline = HERE / "canonical-fs-baseline.md"
    scoped = HERE / "canonical-fs-owner-scoped-change.md"
    baseline.write_text(fs_text, encoding="utf-8")
    scoped_text = replace_once(
        fs_text,
        "explicit owner-scoped preservation checks; it does not claim the legacy guard passed.",
        "explicit owner-scoped preservation checks; this isolated candidate validates the current "
        "format without changing the canonical FS.",
    )
    scoped.write_text(scoped_text, encoding="utf-8")
    if not guard.validate_universal_dfs_mutation(baseline, scoped):
        raise AssertionError("owner-scoped current-format candidate was unexpectedly a no-op")

    random_baseline = HERE / "random-review-baseline"
    random_candidate = HERE / "random-review-candidate"
    for root in (random_baseline, random_candidate):
        shutil.rmtree(root, ignore_errors=True)
        root.mkdir()
    guideline = (BASELINE_METHOD / "assets/environment/test-and-task-guidelines.md").read_text(
        encoding="utf-8"
    )
    (random_baseline / "test-and-task-guidelines.md").write_text(guideline, encoding="utf-8")
    (random_candidate / "test-and-task-guidelines.md").write_text(guideline, encoding="utf-8")
    (random_baseline / "FS.md").write_text(fs_text, encoding="utf-8")
    random_text = fs_text + (
        "\n<!-- DE67:DFS-SLICE:BEGIN id=R-MAINT-CURRENT-FS-GUARD-S001 "
        "claim=R-MAINT-CURRENT-FS-GUARD -->\n"
        "- [ ] 🔴 R-MAINT-CURRENT-FS-GUARD — Current-format compatibility follow-up.\n"
        "<!-- DE67:DFS-SLICE:END id=R-MAINT-CURRENT-FS-GUARD-S001 "
        "claim=R-MAINT-CURRENT-FS-GUARD -->\n"
    )
    (random_candidate / "FS.md").write_text(random_text, encoding="utf-8")
    random_changed = guard.validate_random_review_mutation(
        random_baseline, random_candidate, selected_lane="DFS.md"
    )
    if random_changed != ("DFS.md",):
        raise AssertionError(f"unexpected current-format random result: {random_changed!r}")

    failures = {
        "accepted-proof-loss": reject(
            baseline,
            scoped,
            replace_once(
                fs_text,
                "The following stable slices preserve the accepted contract at its original evidence ceiling.",
                "The accepted contract may be discarded during this change.",
            ),
            "Retained accepted gameplay contract",
        ),
        "slice-deletion": reject(
            baseline,
            scoped,
            replace_once(
                fs_text,
                "<!-- DE67:DFS-SLICE:BEGIN id=R-HARNESS-CONTINUATION-S001 "
                "claim=R-HARNESS-CONTINUATION -->\n",
                "",
            ),
            "FS slice END has no matching BEGIN",
        ),
        "slice-rebinding": reject(
            baseline,
            scoped,
            fs_text.replace(
                "R-HARNESS-CONTINUATION-S001 claim=R-HARNESS-CONTINUATION",
                "R-MAINT-CURRENT-FS-GUARD-S001 claim=R-MAINT-CURRENT-FS-GUARD",
            ),
            "durable slice R-HARNESS-CONTINUATION-S001",
        ),
        "unrelated-domain-rewrite": reject(
            baseline,
            scoped,
            replace_once(
                fs_text,
                "Keep **writhing stalker**",
                "Replace **writhing stalker**",
            ),
            "Language, coordinates and truth",
        ),
        "unauthorized-scope-change": reject(
            baseline,
            scoped,
            replace_once(
                fs_text,
                "Only the external supervisor launches the one requested post-review coordinator.",
                "Any ordinary worker may launch a post-review coordinator.",
            ),
            "Authority and outcome",
        ),
    }

    state = WORKSPACE / ".de67/state/deadlines.sqlite3"
    items = guard.validate_work_ledger(
        WORKSPACE / ".de67/work-ledger.md",
        baseline,
        state=state,
        lineage_id="semantic-surface-cockpit",
    )
    connection = sqlite3.connect("file:" + str(state) + "?mode=ro", uri=True)
    try:
        acceptances = connection.execute(
            "SELECT COUNT(*) FROM claim_acceptances WHERE lineage_id = ?",
            ("semantic-surface-cockpit",),
        ).fetchone()[0]
    finally:
        connection.close()
    output = {
        "candidate_root": str(CANDIDATE_METHOD),
        "baseline_root": str(BASELINE_METHOD),
        "installed_guard_sha256": digest(live_guard),
        "candidate_guard_sha256": digest(candidate_guard),
        "staged_changed_paths": staged_changed,
        "installed_method_modified": False,
        "clock_policy_lifecycle_paths_unchanged": True,
        "current_format_universal_owner_scoped_change": "passed",
        "current_format_random_review_change": list(random_changed),
        "counterexamples_rejected": failures,
        "active_ledger_items_bound": len(items),
        "accepted_rows_observed_read_only": acceptances,
        "evidence_ceiling": (
            "Candidate-only guard/unit and current canonical-FS validation; no due cycle was "
            "resolved, no receipt persisted, no coordinator/game/lifecycle action was started."
        ),
    }
    (HERE / "candidate-validation.json").write_text(
        json.dumps(output, indent=2) + "\n", encoding="utf-8"
    )
    print(json.dumps(output, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
