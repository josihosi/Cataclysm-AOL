from __future__ import annotations

import io
import importlib.util
import json
import os
from pathlib import Path
import shutil
import sqlite3
import sys
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
MODULE_PATH = ROOT / "scripts" / "mutation_guard.py"
SPEC = importlib.util.spec_from_file_location("de67_phase3_mutation_guard", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
guard = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(guard)

DEADLINE_MODULE_PATH = ROOT / "scripts" / "deadline_harness.py"
DEADLINE_SPEC = importlib.util.spec_from_file_location(
    "de67_phase3_deadline_harness", DEADLINE_MODULE_PATH
)
assert DEADLINE_SPEC is not None and DEADLINE_SPEC.loader is not None
deadline = importlib.util.module_from_spec(DEADLINE_SPEC)
DEADLINE_SPEC.loader.exec_module(deadline)


TASK_GUIDANCE = guard.read_markdown(
    guard.CANONICAL_GUIDELINES_ROOT / guard.TASK_GUIDELINES
)
SKILL_TEXT = (ROOT / "SKILL.md").read_text(encoding="utf-8")
WORK_LEDGER_TEXT = (
    ROOT / "assets" / "environment" / "work-ledger.md"
).read_text(encoding="utf-8")


class MutationGuardTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.baseline = self.root / "baseline"
        self.candidate = self.root / "candidate"
        self.baseline.mkdir()
        self.candidate.mkdir()
        self.write_guidelines(self.baseline, TASK_GUIDANCE)
        self.write_guidelines(self.candidate, TASK_GUIDANCE)
        dfs_text = self.expansion_dfs(new_claim="")
        (self.baseline / guard.FS_FILE).write_text(dfs_text, encoding="utf-8")
        (self.candidate / guard.FS_FILE).write_text(dfs_text, encoding="utf-8")
        self.empty_ledger = self.root / guard.MUTATION_LEDGER
        self.empty_ledger.write_text(
            guard.read_markdown(
                guard.CANONICAL_GUIDELINES_ROOT / guard.MUTATION_LEDGER
            ),
            encoding="utf-8",
        )

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def test_legacy_universal_receipts_migrate_without_losing_history_or_immutability(self) -> None:
        with sqlite3.connect(":memory:") as connection:
            connection.execute("""CREATE TABLE universal_review_receipts (
                receipt_id TEXT PRIMARY KEY, lineage_id TEXT NOT NULL,
                cycle_number INTEGER NOT NULL CHECK (cycle_number > 0),
                validated_at REAL NOT NULL, candidate_digest TEXT NOT NULL,
                changed_paths TEXT NOT NULL,
                interval_windows INTEGER NOT NULL CHECK (interval_windows = 30),
                selected_lane TEXT NOT NULL CHECK (selected_lane = 'DFS.md'),
                reviewer_model TEXT NOT NULL CHECK (reviewer_model = 'gpt-5.6-sol'),
                reviewer_effort TEXT NOT NULL CHECK (reviewer_effort = 'ultra'),
                UNIQUE (lineage_id, cycle_number, receipt_id)
            )""")
            connection.execute("""INSERT INTO universal_review_receipts VALUES
                ('old', 'project', 1, 1.0, 'candidate', '[]', 30, 'DFS.md', 'gpt-5.6-sol', 'ultra')""")
            connection.execute("""CREATE TRIGGER universal_review_receipts_cannot_change
                BEFORE UPDATE ON universal_review_receipts BEGIN
                SELECT RAISE(ABORT, 'universal review receipts are append-only'); END""")
            guard._ensure_universal_receipt_schema(connection)
            self.assertEqual(
                connection.execute("SELECT reviewer_model FROM universal_review_receipts WHERE receipt_id='old'").fetchone()[0],
                "gpt-5.6-sol",
            )
            connection.execute("""INSERT INTO universal_review_receipts
                (receipt_id, lineage_id, cycle_number, validated_at, candidate_digest,
                 changed_paths, interval_windows, selected_lane, reviewer_model, reviewer_effort)
                VALUES ('new', 'project', 2, 2.0, 'candidate', '[]', 30, 'DFS.md', 'gpt-6-sol', 'ultra')""")
            with self.assertRaisesRegex(sqlite3.IntegrityError, "append-only"):
                connection.execute("UPDATE universal_review_receipts SET candidate_digest='changed' WHERE receipt_id='old'")
            with self.assertRaisesRegex(sqlite3.IntegrityError, "append-only"):
                connection.execute("DELETE FROM universal_review_receipts WHERE receipt_id='new'")

    def test_canonical_work_ledger_template_has_no_fake_active_item(self) -> None:
        self.assertEqual(guard.active_work_items(WORK_LEDGER_TEXT), ())

    def test_removed_guidance_is_not_required_for_a_stored_legacy_lane(self) -> None:
        self.assertFalse((ROOT / "references" / "kernel.md").exists())
        self.assertFalse((self.baseline / "orchestrator-guidelines.md").exists())
        self.mutate_task()
        changed = guard.validate_random_review_mutation(
            self.baseline, self.candidate, selected_lane="orchestrator-guidelines.md"
        )
        self.assertEqual(changed, (guard.TASK_GUIDELINES,))
        self.assertEqual(len(guard.protected_method_digest()), 64)

    def test_legacy_random_lane_resolves_and_guards_the_canonical_target(self) -> None:
        source = self.expansion_dfs(new_claim="")
        for root in (self.baseline, self.candidate):
            fs = root / "FS.md"
            fs.write_text(source, encoding="utf-8")
        candidate_fs = self.candidate / "FS.md"
        candidate_fs.write_text(source + "\nChanged canonical contract.\n", encoding="utf-8")

        with self.assertRaises(guard.GuardError) as rejected:
            guard.validate_random_review_mutation(
                self.baseline, self.candidate, selected_lane=guard.DFS_FILE
            )
        self.assertNotIn("must contain exactly one", str(rejected.exception))

    @staticmethod
    def write_guidelines(root: Path, task_text: str) -> None:
        (root / guard.TASK_GUIDELINES).write_text(task_text, encoding="utf-8")

    def mutate_task(self) -> None:
        path = self.candidate / guard.TASK_GUIDELINES
        path.write_text(
            TASK_GUIDANCE + "\nUse a saved harness route when it is the shortest honest proof.\n",
            encoding="utf-8",
        )


    def test_guideline_heading_change_is_allowed(self) -> None:
        path = self.candidate / guard.TASK_GUIDELINES
        path.write_text(
            TASK_GUIDANCE.replace("## Own the outcome", "## Own useful work"),
            encoding="utf-8",
        )
        self.assertEqual(
            guard.validate_guideline_mutation(
                self.baseline, self.candidate, broader_mutation=False
            ),
            (guard.TASK_GUIDELINES,),
        )

    def test_changed_baseline_heading_does_not_block_a_real_mutation(self) -> None:
        corrupt = TASK_GUIDANCE.replace("## Own the outcome", "## Own useful work")
        baseline_corrupt = corrupt
        candidate_corrupt = corrupt + "\nPreserve the useful local context.\n"
        (self.baseline / guard.TASK_GUIDELINES).write_text(
            baseline_corrupt, encoding="utf-8"
        )
        (self.candidate / guard.TASK_GUIDELINES).write_text(
            candidate_corrupt, encoding="utf-8"
        )
        self.assertEqual(
            guard.validate_guideline_mutation(
                self.baseline, self.candidate, broader_mutation=False
            ),
            (guard.TASK_GUIDELINES,),
        )

    def test_incident_guidance_may_change_any_evidence_backed_subset(self) -> None:
        self.mutate_task()
        changed = guard.validate_guideline_mutation(
            self.baseline, self.candidate, broader_mutation=True
        )
        self.assertEqual(changed, (guard.TASK_GUIDELINES,))

        self.setUp_candidate_again()
        self.mutate_task()
        changed = guard.validate_guideline_mutation(
            self.baseline, self.candidate, broader_mutation=True
        )
        self.assertEqual(changed, (guard.TASK_GUIDELINES,))

    def test_incident_mutations_reject_whitespace_only_ledger_consumption(self) -> None:
        task_path = self.candidate / guard.TASK_GUIDELINES
        task_path.write_text(
            TASK_GUIDANCE.replace("# Test and task guidelines", "# Test  and task guidelines"),
            encoding="utf-8",
        )
        with self.assertRaisesRegex(guard.GuardError, "whitespace-only"):
            guard.validate_guideline_mutation(
                self.baseline, self.candidate, broader_mutation=False
            )

        orchestrator_path = self.candidate / guard.TASK_GUIDELINES
        orchestrator_path.write_text(
            TASK_GUIDANCE.replace("# Test and task guidelines", "# Test  and task guidelines"),
            encoding="utf-8",
        )
        with self.assertRaisesRegex(guard.GuardError, "whitespace-only"):
            guard.validate_guideline_mutation(
                self.baseline, self.candidate, broader_mutation=True
            )

    def test_incident_guideline_candidate_requires_a_real_change(self) -> None:
        with self.assertRaisesRegex(guard.GuardError, "makes no change"):
            guard.validate_guideline_mutation(
                self.baseline, self.candidate, broader_mutation=True
            )

        self.mutate_task()
        self.mutate_task()
        changed = guard.validate_guideline_mutation(
            self.baseline,
            self.candidate,
            broader_mutation=True,
        )
        self.assertEqual(set(changed), set(guard.GUIDELINE_FILES))

    def incident_state(self) -> Path:
        state = self.root / "incidents.sqlite"
        with deadline.DeadlineHarness(state) as harness:
            for number in range(1, 4):
                harness.start_task(
                    "project", f"miss-{number}", f"R-{number:03d}", 1, now=0
                )
            harness.start_task("project", "breach", "R-004", 10, now=0)
            for number in range(1, 4):
                task = f"miss-{number}"
                harness.expire_task("project", task, now=2)
                harness.diagnose_claim_deadline(
                    "project",
                    f"R-{number:03d}",
                    f"miss {number}",
                    f"Independent diagnosis for miss {number}.",
                    now=3,
                )
            harness.record_integrity_breach(
                "project", "breach", "fabricated evidence", now=1
            )
            harness.diagnose_incident(
                "project",
                "breach",
                "integrity_breach",
                "integrity breach",
                "Independent diagnosis of fabricated evidence.",
                now=2,
            )
        return state

    def run_guidelines_cli(
        self, state: Path, task: str, incident_kind: str
    ) -> tuple[int, str]:
        stdout = io.StringIO()
        stderr = io.StringIO()
        with redirect_stdout(stdout), redirect_stderr(stderr):
            result = guard.main(
                [
                    "guidelines",
                    "--baseline",
                    str(self.baseline),
                    "--candidate",
                    str(self.candidate),
                    "--state",
                    str(state),
                    "--lineage",
                    "project",
                    "--task",
                    task,
                    "--incident-kind",
                    incident_kind,
                    "--ledger-candidate",
                    str(self.empty_ledger),
                ]
            )
        return result, stdout.getvalue() + stderr.getvalue()

    def test_guidelines_cli_accepts_one_causal_macro_change_on_every_miss(self) -> None:
        state = self.incident_state()
        self.mutate_task()

        result, output = self.run_guidelines_cli(state, "miss-2", "deadline_miss")
        self.assertEqual(result, 0, output)
        self.assertIn(guard.TASK_GUIDELINES, output)
        receipt_id = output.rsplit("receipt ", 1)[1].strip()
        connection = sqlite3.connect(state)
        connection.row_factory = sqlite3.Row
        try:
            receipt = connection.execute(
                "SELECT * FROM normal_method_receipts WHERE receipt_id = ?",
                (receipt_id,),
            ).fetchone()
            self.assertIsNotNone(receipt)
            self.assertEqual(receipt["lineage_id"], "project")
            self.assertEqual(receipt["task_id"], "miss-2")
            self.assertEqual(receipt["claim_id"], "R-002")
            self.assertEqual(receipt["incident_kind"], "deadline_miss")
            self.assertEqual(receipt["live_tree_digest"], guard.method_tree_digest())
            self.assertEqual(
                receipt["protected_baseline_digest"],
                guard.protected_method_digest(),
            )
            self.assertNotEqual(
                receipt["candidate_digest"], receipt["live_tree_digest"]
            )
            changed_paths = json.loads(receipt["changed_paths"])
            self.assertIn(
                f"assets/environment/{guard.TASK_GUIDELINES}", changed_paths
            )
            self.assertFalse(
                set(changed_paths) & set(guard.NORMAL_METHOD_PROTECTED_FILES)
            )
            with self.assertRaises(sqlite3.IntegrityError):
                connection.execute(
                    "UPDATE normal_method_receipts SET candidate_digest = ?",
                    ("f" * 64,),
                )
        finally:
            connection.close()

    def test_guidelines_cli_uses_current_generation_incident(self) -> None:
        state = self.incident_state()
        connection = sqlite3.connect(state)
        try:
            connection.execute(
                """
                INSERT INTO claim_deadline_generations (
                    lineage_id, claim_id, generation, estimate_seconds,
                    started_at, deadline_at
                ) VALUES ('project', 'R-001', 2, 1, 10, 11)
                """
            )
            connection.execute(
                """
                INSERT INTO tasks (
                    lineage_id, task_id, claim_id, estimate_seconds,
                    started_at, deadline_at, deadline_generation,
                    terminal_at, attempt_terminal_at, attempt_terminal_kind
                ) VALUES (
                    'project', 'miss-current', 'R-001', 1,
                    10, 11, 2, 12, 12, 'finding'
                )
                """
            )
            connection.execute(
                """
                INSERT INTO claim_deadline_generation_incidents (
                    lineage_id, claim_id, generation, source_task_id,
                    recorded_at, short_verdict, long_detail, reviewed_at
                ) VALUES (
                    'project', 'R-001', 2, 'miss-current',
                    12, 'current miss',
                    'Independent diagnosis for the current generation.', 13
                )
                """
            )
            connection.commit()
        finally:
            connection.close()
        self.mutate_task()

        result, output = self.run_guidelines_cli(
            state, "miss-current", "deadline_miss"
        )

        self.assertEqual(result, 0, output)
        connection = sqlite3.connect(state)
        connection.row_factory = sqlite3.Row
        try:
            receipt = connection.execute(
                """
                SELECT task_id, claim_id FROM normal_method_receipts
                WHERE task_id = 'miss-current'
                """
            ).fetchone()
            self.assertIsNotNone(receipt)
            self.assertEqual(receipt["claim_id"], "R-001")
        finally:
            connection.close()

    def test_guidelines_cli_derives_integrity_scope(self) -> None:
        state = self.incident_state()
        self.mutate_task()
        self.mutate_task()

        result, output = self.run_guidelines_cli(
            state, "breach", "integrity_breach"
        )
        self.assertEqual(result, 0, output)
        self.assertIn(guard.TASK_GUIDELINES, output)

    def test_guidelines_cli_rejects_missing_or_mismatched_incident(self) -> None:
        state = self.incident_state()
        self.mutate_task()

        result, output = self.run_guidelines_cli(
            state, "missing", "deadline_miss"
        )
        self.assertEqual(result, 1)
        self.assertIn("Missing stored deadline_miss incident", output)

        result, output = self.run_guidelines_cli(
            state, "miss-2", "integrity_breach"
        )
        self.assertEqual(result, 1)
        self.assertIn("Missing stored integrity_breach incident", output)

    def test_guidelines_cli_rejects_an_undiagnosed_incident(self) -> None:
        state = self.root / "undiagnosed.sqlite"
        with deadline.DeadlineHarness(state) as harness:
            harness.start_task("project", "miss", "R-001", 1, now=0)
            harness.expire_task("project", "miss", now=2)
        self.mutate_task()

        result, output = self.run_guidelines_cli(
            state, "miss", "deadline_miss"
        )

        self.assertEqual(result, 1)
        self.assertIn("independent short and long diagnosis", output)

    def write_dfs(self, text: str, *, with_slices: bool = True) -> Path:
        path = self.root / "FS.md"
        if with_slices:
            rendered: list[str] = []
            for line in text.splitlines(keepends=True):
                match = guard.STABLE_CLAIM.match(line.rstrip("\r\n"))
                if match is not None:
                    claim_id = guard._selected_claim_id(match.group("label"))
                    ending = "\r\n" if line.endswith("\r\n") else "\n"
                    rendered.append(
                        f"<!-- DE67:DFS-SLICE:BEGIN id={claim_id}-S001 "
                        f"claim={claim_id} -->{ending}"
                    )
                    rendered.append(line)
                    rendered.append(
                        f"<!-- DE67:DFS-SLICE:END id={claim_id}-S001 "
                        f"claim={claim_id} -->{ending}"
                    )
                else:
                    rendered.append(line)
            text = "".join(rendered)
        path.write_text(text, encoding="utf-8")
        return path

    def write_ledger(self, text: str, *, with_slices: bool = True) -> Path:
        path = self.root / "work-ledger.md"
        if with_slices:
            rendered = []
            for line in text.splitlines(keepends=True):
                rendered.append(line)
                match = guard.ACTIVE_ITEM.match(line.rstrip("\r\n"))
                if match is not None:
                    claim_id = guard._selected_claim_id(match.group("reference"))
                    ending = "\r\n" if line.endswith("\r\n") else "\n"
                    rendered.append(
                        f"  - DFS slices: `{claim_id}-S001`{ending}"
                    )
            text = "".join(rendered)
        path.write_text(text, encoding="utf-8")
        return path

    def test_work_ledger_has_no_arbitrary_active_item_limit(self) -> None:
        dfs = self.write_dfs(
            "# FS\n\n"
            + "".join(f"- [ ] 🔴 R-{number:03d} — Work {number}\n" for number in range(1, 12))
        )
        ten = self.write_ledger(
            "# Work ledger\n\n## Active work\n\n"
            + "".join(f"- [ ] R-{number:03d} — Work {number}\n" for number in range(1, 11))
            + "  - [ ] R-999 — Nested worker prose is not another work item\n"
        )
        self.assertEqual(len(guard.validate_work_ledger(ten, dfs)), 10)

        eleven = self.write_ledger(
            "# Work ledger\n\n## Active work\n\n"
            + "".join(f"- [ ] R-{number:03d} — Work {number}\n" for number in range(1, 12))
        )
        self.assertEqual(len(guard.validate_work_ledger(eleven, dfs)), 11)

    def test_work_ledger_rejects_missing_and_non_red_claims(self) -> None:
        dfs = self.write_dfs(
            "# FS\n\n"
            "- [ ] 🔴 R-001 — Still open\n"
            "- [x] R-002 — Already accepted\n"
        )
        missing = self.write_ledger(
            "# Work ledger\n\n## Active work\n\n- [ ] R-404 — Missing\n"
        )
        with self.assertRaisesRegex(guard.GuardError, "still-red"):
            guard.validate_work_ledger(missing, dfs)

        non_red = self.write_ledger(
            "# Work ledger\n\n## Active work\n\n- [ ] R-002 — Already accepted\n"
        )
        with self.assertRaisesRegex(guard.GuardError, "still-red"):
            guard.validate_work_ledger(non_red, dfs)

    def test_exact_red_syntax_matches_an_active_item(self) -> None:
        dfs = self.write_dfs(
            "# FS\n\n- [ ] 🔴 R-001 — Missing production behavior\n"
        )
        ledger = self.write_ledger(
            "# Work ledger\n\n## Active work\n\n"
            "- [ ] R-001 — Implement the red claim\n"
        )
        items = guard.validate_work_ledger(ledger, dfs)
        self.assertEqual(items, ("R-001 — Implement the red claim",))

    def test_work_ledger_accepts_independent_items_for_one_claim(self) -> None:
        dfs = self.write_dfs("# FS\n\n- [ ] 🔴 R-001 — Still open\n")
        ledger = self.write_ledger(
            "# Work ledger\n\n## Active work\n\n"
            "- [ ] R-001 — First route\n\n"
            "- [ ] R-001 — Second route\n"
        )

        self.assertEqual(
            guard.validate_work_ledger(ledger, dfs),
            ("R-001 — First route", "R-001 — Second route"),
        )

    def test_work_ledger_preserves_multiple_same_claim_task_identities(self) -> None:
        dfs = self.write_dfs("# FS\n\n- [ ] 🔴 R-001 — Still open\n")
        state = self.root / "ledger-history.sqlite"
        with deadline.DeadlineHarness(state) as harness:
            harness.start_task("project", "R001-M1", "R-001", 10, now=0)
            harness.report_worker_finding(
                "project", "R001-M1", "unexpected", "first route contradicted", now=1
            )
            harness.start_task("project", "R001-M2", "R-001", 10, now=2)

        historical = self.write_ledger(
            "# Work ledger\n\n## Active work\n\n"
            "- [ ] R-001 — Still open\n\n"
            "  R001-M1 found the old premise. R001-M2 is the current route.\n"
        )
        self.assertEqual(
            guard.validate_work_ledger(
                historical, dfs, state=state, lineage_id="project"
            ),
            ("R-001 — Still open",),
        )

        current = self.write_ledger(
            "# Work ledger\n\n## Active work\n\n"
            "- [ ] R-001 — Still open\n\n"
            "  Current causal frontier and active route belong to R001-M2.\n"
        )
        self.assertEqual(
            guard.validate_work_ledger(
                current, dfs, state=state, lineage_id="project"
            ),
            ("R-001 — Still open",),
        )

    def test_work_ledger_rejects_task_owned_by_another_claim(self) -> None:
        dfs = self.write_dfs(
            "# FS\n\n"
            "- [ ] 🔴 R-001 — First\n"
            "- [ ] 🔴 R-002 — Second\n"
        )
        state = self.root / "ledger-owner.sqlite"
        with deadline.DeadlineHarness(state) as harness:
            harness.start_task("project", "second-task", "R-002", 10, now=0)
        ledger = self.write_ledger(
            "# Work ledger\n\n## Active work\n\n"
            "- [ ] R-001 — First\n\n  Active route: second-task.\n"
        )

        with self.assertRaisesRegex(guard.GuardError, "another FS claim"):
            guard.validate_work_ledger(
                ledger, dfs, state=state, lineage_id="project"
            )

    def slice_dfs(self) -> Path:
        return self.write_dfs(
            "# FS\n\n"
            "Shared contract.\n"
            "Shared proof rule.\n\n"
            "- [ ] 🔴 R-001 — First outcome\n"
            "- [ ] 🔴 R-002 — Second outcome\n"
            "- [ ] 🔴 R-003 — Third outcome\n"
            "Tail context.\n",
            with_slices=False,
        )

    def test_dfs_slice_insertion_is_stable_atomic_in_place_and_extracts_content(self) -> None:
        dfs = self.slice_dfs()
        semantic = dfs.read_bytes()

        allocated = guard.insert_dfs_slices(
            dfs, dfs, "R-001", ((3, 4), (6, 6))
        )

        self.assertEqual(allocated, ("R-001-S001", "R-001-S002"))
        self.assertEqual(
            guard.strip_dfs_slice_markers(guard._read_utf8_exact(dfs)).encode(),
            semantic,
        )
        extracted = guard.extract_dfs_slices(dfs, "R-001", allocated)
        self.assertEqual(
            extracted,
            "\n".join(
                (
                    "Shared contract.",
                    "Shared proof rule.",
                    "- [ ] 🔴 R-001 — First outcome",
                    "",
                )
            ),
        )
        self.assertNotIn("DE67:DFS-SLICE", extracted)

        first_render = dfs.read_bytes()
        repeated = guard.insert_dfs_slices(
            dfs, dfs, "R-001", ((3, 4), (6, 6))
        )
        self.assertEqual(repeated, allocated)
        self.assertEqual(dfs.read_bytes(), first_render)

    def test_dfs_slices_allow_nested_and_identical_other_claims_but_reject_crossing(self) -> None:
        dfs = self.slice_dfs()
        self.assertEqual(
            guard.insert_dfs_slices(dfs, dfs, "R-001", ((3, 4),)),
            ("R-001-S001",),
        )
        self.assertEqual(
            guard.insert_dfs_slices(dfs, dfs, "R-002", ((3, 4),)),
            ("R-002-S001",),
        )
        slices = guard.parse_dfs_slices(dfs.read_text(encoding="utf-8"))
        self.assertEqual({item.slice_id for item in slices}, {"R-001-S001", "R-002-S001"})
        self.assertEqual(
            guard.extract_dfs_slices(dfs, "R-002", ("R-002-S001",)),
            "\n".join(("Shared contract.", "Shared proof rule.", "")),
        )

        with self.assertRaisesRegex(guard.GuardError, "crosses existing slice"):
            guard.insert_dfs_slices(dfs, dfs, "R-003", ((4, 5),))

    def test_maintenance_slice_keeps_durable_identity_and_binding_checks(self):
        text = ("<!-- DE67:DFS-SLICE:BEGIN id=DE67-MAINT-CADENCE-S001 claim=DE67-MAINT-CADENCE -->\n"
                "- [ ] DE67-MAINT-CADENCE — Retain counted progress.\n"
                "<!-- DE67:DFS-SLICE:END id=DE67-MAINT-CADENCE-S001 claim=DE67-MAINT-CADENCE -->\n")
        slices = guard.parse_dfs_slices(text)
        self.assertEqual(slices[0].claim_id, "DE67-MAINT-CADENCE")
        self.assertEqual(guard._selected_claim_id("DE67-MAINT-CADENCE"), "DE67-MAINT-CADENCE")
        with self.assertRaises(guard.GuardError):
            guard.parse_dfs_slices(text.replace("claim=DE67-MAINT-CADENCE", "claim=DE67-MAINT-OTHER", 1))
        with self.assertRaises(guard.GuardError):
            guard.parse_dfs_slices(text.replace("DE67-MAINT-", "UNBOUND-"))

    def test_dfs_slice_parser_rejects_crossed_duplicate_and_fenced_markers(self) -> None:
        crossed = (
            "<!-- DE67:DFS-SLICE:BEGIN id=R-001-S001 claim=R-001 -->\n"
            "<!-- DE67:DFS-SLICE:BEGIN id=R-002-S001 claim=R-002 -->\n"
            "content\n"
            "<!-- DE67:DFS-SLICE:END id=R-001-S001 claim=R-001 -->\n"
            "<!-- DE67:DFS-SLICE:END id=R-002-S001 claim=R-002 -->\n"
        )
        with self.assertRaisesRegex(guard.GuardError, "crossed"):
            guard.parse_dfs_slices(crossed)

        duplicate = (
            "<!-- DE67:DFS-SLICE:BEGIN id=R-001-S001 claim=R-001 -->\n"
            "one\n"
            "<!-- DE67:DFS-SLICE:END id=R-001-S001 claim=R-001 -->\n"
            "<!-- DE67:DFS-SLICE:BEGIN id=R-001-S001 claim=R-001 -->\n"
            "two\n"
            "<!-- DE67:DFS-SLICE:END id=R-001-S001 claim=R-001 -->\n"
        )
        with self.assertRaisesRegex(guard.GuardError, "Duplicate"):
            guard.parse_dfs_slices(duplicate)

        with self.assertRaisesRegex(guard.GuardError, "fenced block"):
            guard.parse_dfs_slices(
                "```\n<!-- DE67:DFS-SLICE:BEGIN id=R-001-S001 claim=R-001 -->\n```\n"
            )

    def test_anchor_only_candidate_cannot_smuggle_semantic_dfs_edits(self) -> None:
        before = self.slice_dfs()
        candidate = self.root / "slice-candidate.md"
        guard.insert_dfs_slices(before, candidate, "R-001", ((3, 4),))
        self.assertEqual(
            guard.validate_dfs_slice_candidate(before, candidate),
            ("R-001-S001",),
        )
        candidate.write_text(
            candidate.read_text(encoding="utf-8").replace(
                "Shared contract.", "Changed contract."
            ),
            encoding="utf-8",
        )
        with self.assertRaisesRegex(guard.GuardError, "other than validated marker"):
            guard.validate_dfs_slice_candidate(before, candidate)

    def test_work_ledger_slice_status_bootstraps_then_strictly_binds_claim(self) -> None:
        dfs = self.slice_dfs()
        guard.insert_dfs_slices(dfs, dfs, "R-001", ((3, 4),))
        pointerless = self.write_ledger(
            "# Work ledger\n\n## Active work\n\n- [ ] R-001 — First outcome\n",
            with_slices=False,
        )
        self.assertEqual(
            guard.dfs_slice_status(pointerless, dfs)[0][1], "missing"
        )
        with self.assertRaisesRegex(guard.GuardError, "has no FS slices"):
            guard.validate_work_ledger(pointerless, dfs)

        completed_pointer_is_not_in_active_block = self.write_ledger(
            "# Work ledger\n\n## Active work\n\n"
            "- [ ] R-001 — First outcome\n"
            "- [x] R-002 — Second outcome\n"
            "  - DFS slices: `R-001-S001`\n",
            with_slices=False,
        )
        self.assertEqual(
            guard.dfs_slice_status(
                completed_pointer_is_not_in_active_block, dfs
            )[0][1],
            "missing",
        )

        ready = self.write_ledger(
            "# Work ledger\n\n## Active work\n\n"
            "- [ ] R-001 — First outcome\n"
            "  - DFS slices: `R-001-S001`\n",
            with_slices=False,
        )
        self.assertEqual(guard.dfs_slice_status(ready, dfs)[0][1], "ready")
        self.assertEqual(
            guard.validate_work_ledger(ready, dfs),
            ("R-001 — First outcome",),
        )

        missing = self.write_ledger(
            "# Work ledger\n\n## Active work\n\n"
            "- [ ] R-001 — First outcome\n"
            "  - DFS slices: `R-001-S999`\n",
            with_slices=False,
        )
        self.assertEqual(guard.dfs_slice_status(missing, dfs)[0][1], "invalid")
        with self.assertRaisesRegex(guard.GuardError, "missing FS slice"):
            guard.validate_work_ledger(missing, dfs)

        guard.insert_dfs_slices(dfs, dfs, "R-002", ((7, 7),))
        wrong_owner = self.write_ledger(
            "# Work ledger\n\n## Active work\n\n"
            "- [ ] R-001 — First outcome\n"
            "  - DFS slices: `R-002-S001`\n",
            with_slices=False,
        )
        with self.assertRaisesRegex(guard.GuardError, "another claim"):
            guard.validate_work_ledger(wrong_owner, dfs)

    def test_dfs_slice_cli_marks_statuses_and_extracts_only_context(self) -> None:
        dfs = self.slice_dfs()
        stdout = io.StringIO()
        with redirect_stdout(stdout):
            result = guard.main(
                [
                    "mark-dfs-slices", "--source", str(dfs), "--output", str(dfs),
                    "--claim", "R-001", "--range", "3:4", "--range", "6:6",
                ]
            )
        self.assertEqual(result, 0, stdout.getvalue())
        self.assertIn("R-001-S002", stdout.getvalue())

        stdout = io.StringIO()
        with redirect_stdout(stdout):
            result = guard.main(
                [
                    "extract-dfs-slices", "--dfs", str(dfs), "--claim", "R-001",
                    "--slice", "R-001-S002",
                ]
            )
        self.assertEqual(result, 0)
        self.assertEqual(
            stdout.getvalue(),
            "\n".join(("- [ ] 🔴 R-001 — First outcome", "")),
        )

        ledger = self.write_ledger(
            "# Work ledger\n\n## Active work\n\n- [ ] R-001 — First outcome\n",
            with_slices=False,
        )
        stdout = io.StringIO()
        with redirect_stdout(stdout):
            result = guard.main(
                ["dfs-slice-status", "--ledger", str(ledger), "--dfs", str(dfs)]
            )
        self.assertEqual(result, 0)
        self.assertIn("missing: R-001", stdout.getvalue())

    def test_dfs_slice_cli_writes_utf8_under_a_legacy_windows_code_page(self) -> None:
        dfs = self.slice_dfs()
        guard.insert_dfs_slices(dfs, dfs, "R-001", ((6, 6),))
        encoded = io.BytesIO()
        legacy_stdout = io.TextIOWrapper(encoded, encoding="cp1252")

        with patch.object(guard.sys, "stdout", legacy_stdout):
            result = guard.main(
                [
                    "extract-dfs-slices", "--dfs", str(dfs), "--claim", "R-001",
                    "--slice", "R-001-S001",
                ]
            )
            legacy_stdout.flush()

        self.assertEqual(result, 0)
        self.assertIn("🔴".encode("utf-8"), encoded.getvalue())

    def test_dfs_slice_marker_lock_fails_fast_and_is_always_cleaned_up(self) -> None:
        dfs = self.slice_dfs()
        original = dfs.read_bytes()
        lock = dfs.parent / f".{dfs.name}.de67-dfs-slices.lock"
        lock.write_text("held\n", encoding="utf-8")
        with self.assertRaisesRegex(guard.GuardError, "locked by another"):
            guard.insert_dfs_slices(dfs, dfs, "R-001", ((3, 4),))
        self.assertEqual(dfs.read_bytes(), original)
        lock.unlink()

        with self.assertRaisesRegex(guard.GuardError, "Invalid inclusive"):
            guard.insert_dfs_slices(dfs, dfs, "R-001", ((0, 4),))
        self.assertFalse(lock.exists())
        guard.insert_dfs_slices(dfs, dfs, "R-001", ((3, 4),))
        self.assertFalse(lock.exists())

    def test_universal_dfs_candidate_preserves_every_durable_slice_binding(self) -> None:
        before = self.root / "universal-sliced-before.md"
        candidate = self.root / "universal-sliced-candidate.md"
        before.write_text(self.expansion_dfs(), encoding="utf-8")
        claim_line = next(
            number
            for number, line in enumerate(
                guard._read_utf8_exact(before).splitlines(), start=1
            )
            if "R-002" in line and guard.STABLE_CLAIM.match(line)
        )
        guard.insert_dfs_slices(
            before, before, "R-002", ((claim_line, claim_line),)
        )
        candidate.write_bytes(
            guard.strip_dfs_slice_markers(
                guard._read_utf8_exact(before)
            ).encode("utf-8")
        )

        with self.assertRaisesRegex(guard.GuardError, "durable slice R-002-S001"):
            guard.validate_universal_dfs_mutation(before, candidate)

    def test_exact_selected_marker_removal_is_accepted(self) -> None:
        before = self.root / "before.md"
        after = self.root / "after.md"
        before.write_text(
            "# FS\n\n- [ ] 🔴 R-001 — First\n- [ ] 🔴 R-002 — Second\n",
            encoding="utf-8",
        )
        after.write_text(
            "# FS\n\n- [x] R-001 — First\n- [ ] 🔴 R-002 — Second\n",
            encoding="utf-8",
        )
        completed = guard.validate_dfs_completion(before, after, "R-001 — First")
        self.assertEqual(completed, "R-001 — First")

    def test_dfs_completion_rejects_collateral_edit(self) -> None:
        before = self.root / "before.md"
        after = self.root / "after.md"
        before.write_text(
            "# FS\n\n- [ ] 🔴 R-001 — First\n- [ ] 🔴 R-002 — Second\n",
            encoding="utf-8",
        )
        after.write_text(
            "# FS revised\n\n- [x] R-001 — First\n- [ ] 🔴 R-002 — Second\n",
            encoding="utf-8",
        )
        with self.assertRaisesRegex(guard.GuardError, "must only change"):
            guard.validate_dfs_completion(before, after, "R-001")

    def completion_files(self) -> tuple[Path, Path]:
        before = self.root / "state-before.md"
        after = self.root / "state-after.md"
        before.write_text("# FS\n\n- [ ] 🔴 R-001 — First\n", encoding="utf-8")
        after.write_text("# FS\n\n- [x] R-001 — First\n", encoding="utf-8")
        return before, after

    def deadline_state(
        self,
        name: str,
        *,
        lineage: str = "project",
        task: str = "task",
        claim: str = "R-001",
        accepted: bool = True,
        breached: bool = False,
    ) -> Path:
        state = self.root / f"{name}.sqlite"
        with deadline.DeadlineHarness(state) as harness:
            exploration_task = f"{task}-exploration"
            harness.start_task(lineage, exploration_task, claim, 10, now=0)
            harness.complete_task(
                lineage,
                exploration_task,
                "exploration found a finite strategy and proof route",
                now=1,
            )
            harness.transition_claim_to_closure(
                lineage,
                claim,
                exploration_task,
                "requested behavior works through the production owner",
                "focused regression plus natural runtime route",
                "implement and prove the remaining production transition",
                now=2,
            )
            harness.start_task(lineage, task, claim, 10, phase="closure", now=3)
            harness.complete_task(
                lineage, task, "coordinator accepted the real test", now=4
            )
            if accepted:
                harness.accept_claim(
                    lineage,
                    claim,
                    task,
                    "closure outcome and natural proof route are satisfied",
                    now=5,
                )
            if breached:
                harness.record_integrity_breach(
                    lineage, task, "completion evidence was fabricated", now=6
                )
        return state

    def run_complete_cli(
        self,
        state: Path,
        *,
        lineage: str = "project",
        task: str = "task",
        claim: str = "R-001",
    ) -> tuple[int, str]:
        before, after = self.completion_files()
        stdout = io.StringIO()
        stderr = io.StringIO()
        with redirect_stdout(stdout), redirect_stderr(stderr):
            result = guard.main(
                [
                    "complete-dfs",
                    "--before",
                    str(before),
                    "--after",
                    str(after),
                    "--claim",
                    claim,
                    "--state",
                    str(state),
                    "--lineage",
                    lineage,
                    "--task",
                    task,
                ]
            )
        return result, stdout.getvalue() + stderr.getvalue()

    def test_complete_dfs_cli_accepts_matching_accepted_task(self) -> None:
        state = self.deadline_state("accepted")
        result, output = self.run_complete_cli(state)
        self.assertEqual(result, 0, output)
        self.assertIn("ok: completed R-001 — First", output)

    def test_complete_dfs_cli_rejects_unknown_and_unaccepted_tasks(self) -> None:
        accepted = self.deadline_state("known", task="known")
        result, output = self.run_complete_cli(accepted, task="unknown")
        self.assertEqual(result, 1)
        self.assertIn("Unknown deadline task", output)

        pending = self.deadline_state("pending", task="pending", accepted=False)
        result, output = self.run_complete_cli(pending, task="pending")
        self.assertEqual(result, 1)
        self.assertIn("has not been accepted", output)

    def test_complete_dfs_cli_rejects_mismatch_and_integrity_breach(self) -> None:
        mismatched = self.deadline_state("mismatch", claim="R-002")
        result, output = self.run_complete_cli(mismatched)
        self.assertEqual(result, 1)
        self.assertIn("does not match selected claim", output)

        breached = self.deadline_state("breached", breached=True)
        result, output = self.run_complete_cli(breached)
        self.assertEqual(result, 1)
        self.assertIn("integrity breach", output)

    def reopening_files(
        self, *, collateral: bool = False
    ) -> tuple[Path, Path]:
        before = self.root / "reopen-before.md"
        after = self.root / "reopen-after.md"
        before.write_text(
            "# FS\n\n"
            "The product outcome and proof route stay unchanged.\n\n"
            "- [x] R-001 — First\n"
            "- [x] R-002 — Other accepted claim\n",
            encoding="utf-8",
        )
        candidate = (
            "# FS\n\n"
            "The product outcome and proof route stay unchanged.\n\n"
            "- [ ] 🔴 R-001 — First\n"
            "- [x] R-002 — Other accepted claim\n"
        )
        if collateral:
            candidate = candidate.replace(
                "- [x] R-002 — Other accepted claim",
                "- [ ] 🔴 R-002 — Other accepted claim",
            )
        after.write_text(candidate, encoding="utf-8")
        return before, after

    def invalidated_state(self, name: str, trigger: str) -> Path:
        state = self.deadline_state(name)
        with deadline.DeadlineHarness(state) as harness:
            if trigger == "integrity_breach":
                harness.record_integrity_breach(
                    "project",
                    "task",
                    "accepted proof was fabricated",
                    now=6,
                )
            elif trigger == "closure_reopen":
                harness.start_task(
                    "project",
                    "closure-finding",
                    "R-001",
                    10,
                    phase="closure",
                    now=6,
                )
                harness.report_worker_finding(
                    "project",
                    "closure-finding",
                    "unexpected",
                    "The requested behavior works through the production owner premise is false.",
                    now=7,
                )
                harness.reopen_claim_exploration(
                    "project",
                    "R-001",
                    "closure-finding",
                    "requested behavior works through the production owner",
                    now=8,
                )
            else:
                raise AssertionError(f"unsupported trigger: {trigger}")
        return state

    def run_reopen_cli(
        self,
        state: Path,
        *,
        claim: str = "R-001",
        collateral: bool = False,
    ) -> tuple[int, str]:
        before, after = self.reopening_files(collateral=collateral)
        stdout = io.StringIO()
        stderr = io.StringIO()
        with redirect_stdout(stdout), redirect_stderr(stderr):
            result = guard.main(
                [
                    "reopen-dfs",
                    "--before",
                    str(before),
                    "--after",
                    str(after),
                    "--claim",
                    claim,
                    "--state",
                    str(state),
                    "--lineage",
                    "project",
                ]
            )
        return result, stdout.getvalue() + stderr.getvalue()

    def test_reopen_dfs_accepts_exact_reopen_and_integrity_invalidations(self) -> None:
        for trigger in ("closure_reopen", "integrity_breach"):
            with self.subTest(trigger=trigger):
                state = self.invalidated_state(trigger, trigger)
                result, output = self.run_reopen_cli(state)
                self.assertEqual(result, 0, output)
                self.assertIn(f"from {trigger}", output)

                _, reopened_dfs = self.reopening_files()
                guard.insert_dfs_slices(
                    reopened_dfs, reopened_dfs, "R-001", ((5, 5),)
                )
                ledger = self.write_ledger(
                    "# Work ledger\n\n## Active work\n\n"
                    "- [ ] R-001 — First\n"
                )
                self.assertEqual(
                    guard.validate_work_ledger(
                        ledger,
                        reopened_dfs,
                        state=state,
                        lineage_id="project",
                    ),
                    ("R-001 — First",),
                )

    def test_reopen_dfs_accepts_breach_of_an_earlier_multi_gap_proof(self) -> None:
        state = self.root / "multi-gap-breach.sqlite"
        with deadline.DeadlineHarness(state) as harness:
            harness.start_task("project", "explore", "R-001", 100, now=0)
            harness.complete_task("project", "explore", "strategy", now=1)
            harness.transition_claim_to_closure(
                "project",
                "R-001",
                "explore",
                "Prove both controls.",
                "Run both fixtures.",
                gaps=[
                    ("G-A", "Prove A.", "Run fixture A."),
                    ("G-B", "Prove B.", "Run fixture B."),
                ],
                now=2,
            )
            harness.start_task(
                "project", "proof-a", "R-001", 100,
                phase="closure", gap_id="G-A", now=3,
            )
            harness.complete_task("project", "proof-a", "A passed", now=4)
            harness.close_closure_gap(
                "project", "R-001", "G-A", "proof-a", "A accepted", now=5
            )
            harness.start_task(
                "project", "proof-b", "R-001", 100,
                phase="closure", gap_id="G-B", now=6,
            )
            harness.complete_task("project", "proof-b", "B passed", now=7)
            harness.accept_claim(
                "project", "R-001", "proof-b", "both accepted", now=8
            )
            harness.record_integrity_breach(
                "project", "proof-a", "fixture A was forged", now=9
            )

        result, output = self.run_reopen_cli(state)

        self.assertEqual(result, 0, output)
        self.assertIn("from integrity_breach", output)

    def test_reopen_dfs_rejects_valid_or_unsupported_invalidation(self) -> None:
        valid = self.deadline_state("still-valid")
        result, output = self.run_reopen_cli(valid)
        self.assertEqual(result, 1)
        self.assertIn("currently valid acceptance", output)

        unsupported = self.deadline_state("unsupported")
        connection = sqlite3.connect(unsupported)
        try:
            connection.execute(
                """
                UPDATE claim_acceptances
                SET invalidated_at = 9, invalidation_reason = 'manual tamper'
                WHERE lineage_id = 'project' AND claim_id = 'R-001'
                """
            )
            connection.commit()
        finally:
            connection.close()
        result, output = self.run_reopen_cli(unsupported)
        self.assertEqual(result, 1)
        self.assertIn("no durable reopen or integrity trigger", output)

    def test_reopen_dfs_rejects_wrong_claim_and_collateral_status_change(self) -> None:
        state = self.invalidated_state("reopen-rejections", "integrity_breach")
        result, output = self.run_reopen_cli(state, claim="R-002")
        self.assertEqual(result, 1)
        self.assertIn("no recorded acceptance", output)

        result, output = self.run_reopen_cli(state, collateral=True)
        self.assertEqual(result, 1)
        self.assertIn("must only change", output)

    @staticmethod
    def expansion_dfs(*, new_claim: str = "- [ ] 🔴 R-003 — New prerequisite\n") -> str:
        return (
            "# FS\n\n"
            "## Functional contract\n\n"
            "The existing behavior remains the contract.\n\n"
            "## Project language and terminology\n\n"
            "Use the project's existing names.\n\n"
            "## Mechanistic design\n\n"
            "The coordinator may add source-grounded mechanics here.\n\n"
            "## Stable work claims\n\n"
            "- [x] R-000 — Accepted frontier\n"
            "- [ ] 🔴 R-001 — Worker found a blocker\n"
            "- [ ] 🔴 R-002 — Existing next work\n"
            + new_claim
        )

    def expansion_files(self, candidate_text: str | None = None) -> tuple[Path, Path]:
        before = self.root / "expansion-before.md"
        candidate = self.root / "expansion-candidate.md"
        before.write_text(self.expansion_dfs(new_claim=""), encoding="utf-8")
        candidate.write_text(
            self.expansion_dfs() if candidate_text is None else candidate_text,
            encoding="utf-8",
        )
        return before, candidate

    def finding_state(
        self,
        name: str,
        *,
        kind: str = "blocker",
        claim: str = "R-001",
        report: bool = True,
    ) -> Path:
        state = self.root / f"finding-{name}.sqlite"
        with deadline.DeadlineHarness(state) as harness:
            harness.start_task("project", "worker-task", claim, 10, now=0)
            if report:
                harness.report_worker_finding(
                    "project",
                    "worker-task",
                    kind,
                    "The production owner has an unmodelled competing writer.",
                    now=1,
                )
        return state

    def run_expand_cli(
        self,
        state: Path,
        *,
        lineage: str = "project",
        task: str = "worker-task",
        candidate_text: str | None = None,
    ) -> tuple[int, str]:
        before, candidate = self.expansion_files(candidate_text)
        stdout = io.StringIO()
        stderr = io.StringIO()
        with redirect_stdout(stdout), redirect_stderr(stderr):
            result = guard.main(
                [
                    "expand-dfs",
                    "--before",
                    str(before),
                    "--candidate",
                    str(candidate),
                    "--state",
                    str(state),
                    "--lineage",
                    lineage,
                    "--task",
                    task,
                    "--ledger-candidate",
                    str(self.empty_ledger),
                ]
            )
        return result, stdout.getvalue() + stderr.getvalue()

    def test_expand_dfs_cli_accepts_exact_blocker_and_unexpected_findings(self) -> None:
        for kind in ("blocker", "unexpected"):
            with self.subTest(kind=kind):
                state = self.finding_state(kind, kind=kind)
                result, output = self.run_expand_cli(state)
                self.assertEqual(result, 0, output)
                self.assertIn(f"ok: {kind} finding expanded R-001", output)
                self.assertIn("added R-003", output)

    def test_owner_expand_dfs_consumes_only_owner_guidance_and_preserves_frontier(self) -> None:
        before, candidate = self.expansion_files()
        ledger_before = self.root / "owner-ledger-before.md"
        ledger_after = self.root / "owner-ledger-after.md"
        ledger_before.write_text(
            "# Ledger\n\n## Pending suggestions\n\n"
            "- Owner-authorized same-outcome expansion\n"
            "- Reviewer-authored candidate remains\n",
            encoding="utf-8",
        )
        ledger_after.write_text(
            "# Ledger\n\n## Pending suggestions\n\n"
            "- Reviewer-authored candidate remains\n",
            encoding="utf-8",
        )
        added, consumed = guard.validate_owner_dfs_expansion(
            before, candidate, ledger_before, ledger_after
        )
        self.assertEqual(added, ("R-003",))
        self.assertEqual(consumed, ("Owner-authorized same-outcome expansion",))

        ledger_after.write_text(
            "# Ledger\n\n## Pending suggestions\n",
            encoding="utf-8",
        )
        with self.assertRaisesRegex(guard.GuardError, "only owner-authorized"):
            guard.validate_owner_dfs_expansion(
                before, candidate, ledger_before, ledger_after
            )

    def test_expand_dfs_cli_rejects_missing_or_mismatched_finding(self) -> None:
        missing = self.finding_state("missing", report=False)
        result, output = self.run_expand_cli(missing)
        self.assertEqual(result, 1)
        self.assertIn("Missing stored worker finding", output)

        stored = self.finding_state("stored")
        result, output = self.run_expand_cli(stored, task="other-task")
        self.assertEqual(result, 1)
        self.assertIn("Missing stored worker finding", output)

        wrong_claim = self.finding_state("wrong-claim", claim="R-999")
        result, output = self.run_expand_cli(wrong_claim)
        self.assertEqual(result, 1)
        self.assertIn("not exactly one still-red FS claim", output)

    def test_expand_dfs_preserves_protected_sections_exactly(self) -> None:
        before, candidate = self.expansion_files(
            self.expansion_dfs().replace(
                "The existing behavior remains the contract.",
                "The behavior is now broader.",
            )
        )
        with self.assertRaisesRegex(guard.GuardError, "Functional contract"):
            guard.validate_dfs_expansion(before, candidate, "R-001")

        before, candidate = self.expansion_files(
            self.expansion_dfs().replace(
                "Use the project's existing names.",
                "Introduce a new project vocabulary.",
            )
        )
        with self.assertRaisesRegex(guard.GuardError, "Project language"):
            guard.validate_dfs_expansion(before, candidate, "R-001")

    def test_expand_dfs_cannot_rewrite_or_delete_nonprotected_prose(self) -> None:
        baseline = self.expansion_dfs()
        candidates = (
            baseline.replace(
                "The coordinator may add source-grounded mechanics here.",
                "A different owner now controls the transition.",
            ),
            baseline.replace(
                "The coordinator may add source-grounded mechanics here.\n", ""
            ),
        )
        for candidate_text in candidates:
            with self.subTest(candidate=candidate_text):
                before, candidate = self.expansion_files(candidate_text)
                with self.assertRaisesRegex(
                    guard.GuardError, "cannot delete or rewrite"
                ):
                    guard.validate_dfs_expansion(before, candidate, "R-001")

    def test_expand_dfs_requires_a_unique_new_unchecked_red_claim(self) -> None:
        before, candidate = self.expansion_files(self.expansion_dfs(new_claim=""))
        with self.assertRaisesRegex(guard.GuardError, "at least one new red claim"):
            guard.validate_dfs_expansion(before, candidate, "R-001")

        before, candidate = self.expansion_files(
            self.expansion_dfs(new_claim="- [x] R-003 — Prematurely accepted\n")
        )
        with self.assertRaisesRegex(guard.GuardError, "unchecked and red"):
            guard.validate_dfs_expansion(before, candidate, "R-001")

        before, candidate = self.expansion_files(
            self.expansion_dfs(new_claim="- [ ] 🔴 R-001 — Duplicate id\n")
        )
        with self.assertRaisesRegex(guard.GuardError, "duplicate stable claim id"):
            guard.validate_dfs_expansion(before, candidate, "R-001")

    def test_expand_dfs_rejects_existing_claim_deletion_rewrite_closure_or_reorder(self) -> None:
        baseline = self.expansion_dfs()
        mutations = {
            "delete": baseline.replace("- [ ] 🔴 R-002 — Existing next work\n", ""),
            "rewrite": baseline.replace(
                "R-002 — Existing next work", "R-002 — Renamed work"
            ),
            "close": baseline.replace(
                "- [ ] 🔴 R-001 — Worker found a blocker",
                "- [x] R-001 — Worker found a blocker",
            ),
            "reopen accepted frontier": baseline.replace(
                "- [x] R-000 — Accepted frontier",
                "- [ ] 🔴 R-000 — Accepted frontier",
            ),
            "reorder": baseline.replace(
                "- [ ] 🔴 R-001 — Worker found a blocker\n"
                "- [ ] 🔴 R-002 — Existing next work\n",
                "- [ ] 🔴 R-002 — Existing next work\n"
                "- [ ] 🔴 R-001 — Worker found a blocker\n",
            ),
        }
        for name, candidate_text in mutations.items():
            with self.subTest(name=name):
                before, candidate = self.expansion_files(candidate_text)
                with self.assertRaises(guard.GuardError):
                    guard.validate_dfs_expansion(before, candidate, "R-001")

    def test_expand_dfs_never_authorizes_completed_task_or_claim(self) -> None:
        state = self.finding_state("completed")
        connection = sqlite3.connect(state)
        try:
            connection.execute(
                """
                UPDATE tasks
                SET completed_at = 2, completion_evidence = 'tampered completion'
                WHERE lineage_id = 'project' AND task_id = 'worker-task'
                """
            )
            connection.commit()
        finally:
            connection.close()
        result, output = self.run_expand_cli(state)
        self.assertEqual(result, 1)
        self.assertIn("completed task cannot authorize FS expansion", output)

        closed = self.expansion_dfs().replace(
            "- [ ] 🔴 R-001 — Worker found a blocker",
            "- [x] R-001 — Worker found a blocker",
        )
        state = self.finding_state("claim-closure")
        result, output = self.run_expand_cli(state, candidate_text=closed)
        self.assertEqual(result, 1)
        self.assertIn("cannot delete or rewrite", output)

    def random_review_state(self, lane_index: int) -> tuple[Path, int]:
        state = self.root / f"random-{lane_index}.sqlite"
        with patch.object(
            deadline.secrets, "randbelow", side_effect=[0, lane_index]
        ), deadline.DeadlineHarness(state) as harness:
            for number in range(1, deadline.RANDOM_INTERVAL_MIN + 1):
                task = f"terminal-{number}"
                harness.start_task("project", task, f"R-{number:03d}", 10, now=0)
                harness.complete_task("project", task, "green", now=1)
            cycle = harness.list_tasks(now=2)["random_mutation"]["cycle_number"]
        return state, cycle

    def run_random_review_cli(
        self,
        state: Path,
        cycle: int,
        *,
        ledger: Path | None = None,
    ) -> tuple[int, str]:
        stdout = io.StringIO()
        stderr = io.StringIO()
        arguments = [
            "random-review",
            "--baseline",
            str(self.baseline),
            "--candidate",
            str(self.candidate),
            "--state",
            str(state),
            "--lineage",
            "project",
            "--cycle",
            str(cycle),
        ]
        if ledger is not None:
            arguments.extend(["--ledger-candidate", str(ledger)])
        with redirect_stdout(stdout), redirect_stderr(stderr):
            result = guard.main(arguments)
        return result, stdout.getvalue() + stderr.getvalue()

    def test_random_review_applies_exactly_the_selected_guideline_lane(self) -> None:
        state, cycle = self.random_review_state(0)
        self.mutate_task()
        result, output = self.run_random_review_cli(state, cycle)
        self.assertEqual(result, 0, output)
        self.assertIn(guard.TASK_GUIDELINES, output)

        self.setUp_candidate_again()
        state, cycle = self.random_review_state(1)
        self.mutate_task()
        result, output = self.run_random_review_cli(state, cycle)
        self.assertEqual(result, 0, output)
        self.assertIn(guard.TASK_GUIDELINES, output)

    def test_random_trajectory_can_change_both_guidelines_and_refine_dfs(self) -> None:
        state, cycle = self.random_review_state(0)
        self.mutate_task()
        self.mutate_task()
        (self.candidate / guard.FS_FILE).write_text(self.expansion_dfs(), encoding="utf-8")
        result, output = self.run_random_review_cli(state, cycle)
        self.assertEqual(result, 0, output)
        for name in (*guard.GUIDELINE_FILES, guard.FS_FILE):
            self.assertIn(name, output)

    def test_random_guideline_review_accepts_exact_guarded_noop(self) -> None:
        for lane_index in (0, 1):
            with self.subTest(lane_index=lane_index):
                state, cycle = self.random_review_state(lane_index)
                result, output = self.run_random_review_cli(state, cycle)
                self.assertEqual(result, 0, output)
                self.assertIn("guarded no-op", output)

    def setUp_candidate_again(self) -> None:
        self.write_guidelines(self.candidate, TASK_GUIDANCE)
        (self.candidate / guard.FS_FILE).write_text(
            self.expansion_dfs(new_claim=""), encoding="utf-8"
        )

    def test_random_review_accepts_cross_lane_but_rejects_whitespace_only_change(self) -> None:
        state, cycle = self.random_review_state(1)
        self.mutate_task()
        result, output = self.run_random_review_cli(state, cycle)
        self.assertEqual(result, 0, output)
        self.assertIn(guard.TASK_GUIDELINES, output)

        self.setUp_candidate_again()
        state, cycle = self.random_review_state(0)
        path = self.candidate / guard.TASK_GUIDELINES
        path.write_text(
            TASK_GUIDANCE.replace("# Test and task guidelines", "# Test  and task guidelines"),
            encoding="utf-8",
        )
        result, output = self.run_random_review_cli(state, cycle)
        self.assertEqual(result, 1)
        self.assertIn("whitespace-only", output)

    def test_random_dfs_review_accepts_safe_expansion_or_exact_guarded_noop(self) -> None:
        state, cycle = self.random_review_state(1)
        (self.candidate / guard.FS_FILE).write_text(
            self.expansion_dfs(), encoding="utf-8"
        )
        result, output = self.run_random_review_cli(state, cycle)
        self.assertEqual(result, 0, output)
        self.assertIn("changed DFS.md", output)

        self.setUp_candidate_again()
        state, cycle = self.random_review_state(1)
        result, output = self.run_random_review_cli(state, cycle)
        self.assertEqual(result, 0, output)
        self.assertIn("guarded no-op", output)

    def test_random_review_repairs_missing_status_delimiter_without_new_work(self) -> None:
        state, cycle = self.random_review_state(1)
        original = (self.baseline / guard.FS_FILE).read_text(encoding="utf-8")
        original += (
            "\n<!-- DE67:DFS-SLICE:BEGIN id=R-099-S001 claim=R-099 -->\n"
            "Required behavior and proof remain unchanged.\n\n"
            "- [ ] 🔴 R-099 — Existing unaccepted outcome.\n"
            "<!-- DE67:DFS-SLICE:END id=R-099-S001 claim=R-099 -->\n"
        )
        (self.baseline / guard.FS_FILE).write_text(original, encoding="utf-8")
        repaired = original.replace("- [ ] 🔴 R-099", "Implementation status:\n\n- [ ] 🔴 R-099")
        (self.candidate / guard.FS_FILE).write_text(repaired, encoding="utf-8")
        result, output = self.run_random_review_cli(state, cycle)
        self.assertEqual(result, 0, output)
        for invalid in (repaired.replace("[ ] 🔴 R-099", "[x] R-099"),
                        repaired.replace("proof remain unchanged", "proof is waived")):
            (self.candidate / guard.FS_FILE).write_text(invalid, encoding="utf-8")
            result, output = self.run_random_review_cli(state, cycle)
            self.assertEqual(result, 1, output)

    def test_random_dfs_review_preserves_frozen_contract(self) -> None:
        state, cycle = self.random_review_state(1)
        candidate = self.expansion_dfs().replace(
            "The existing behavior remains the contract.",
            "The behavior is now broader.",
        )
        (self.candidate / guard.FS_FILE).write_text(candidate, encoding="utf-8")
        result, output = self.run_random_review_cli(state, cycle)
        self.assertEqual(result, 1)
        self.assertIn("Functional contract", output)

    def test_successful_random_mutation_does_not_require_empty_scratch_ledger(self) -> None:
        state, cycle = self.random_review_state(0)
        self.mutate_task()
        uncleared = self.root / "uncleared.md"
        uncleared.write_text("# pending suggestion\n", encoding="utf-8")
        result, output = self.run_random_review_cli(
            state, cycle, ledger=uncleared
        )
        self.assertEqual(result, 0, output)
        self.assertIn("random review cycle", output)

    def test_random_review_requires_due_unresolved_stored_cycle(self) -> None:
        state = self.root / "not-due.sqlite"
        with patch.object(
            deadline.secrets, "randbelow", side_effect=[0, 0]
        ), deadline.DeadlineHarness(state) as harness:
            harness.start_task("project", "running", "R-001", 100, now=0)
            cycle = harness.list_tasks(now=1)["random_mutation"]["cycle_number"]
        self.mutate_task()
        result, output = self.run_random_review_cli(state, cycle)
        self.assertEqual(result, 1)
        self.assertIn("not due", output)

    def method_candidate_roots(self) -> tuple[Path, Path]:
        baseline = self.root / "method-baseline"
        candidate = self.root / "method-candidate"
        shutil.copytree(ROOT, baseline)
        shutil.copytree(ROOT, candidate)
        return baseline, candidate

    def test_normal_method_mutation_has_broad_surface_but_not_clock_or_guard(self) -> None:
        baseline, candidate = self.method_candidate_roots()
        skill = candidate / "SKILL.md"
        skill.write_text(
            skill.read_text(encoding="utf-8") + "\n<!-- focused routing refinement -->\n",
            encoding="utf-8",
        )
        probe = candidate / "scripts" / "candidate_probe.py"
        probe.write_text("print('observation')\n", encoding="utf-8")

        changed = guard.validate_method_mutation(
            baseline, candidate, universal=False
        )

        self.assertIn("SKILL.md", changed)
        self.assertIn("scripts/candidate_probe.py", changed)

        clock = candidate / "scripts" / "deadline_harness.py"
        clock.write_text(
            clock.read_text(encoding="utf-8") + "\n# reset deadline\n",
            encoding="utf-8",
        )
        with self.assertRaisesRegex(guard.GuardError, "hard clock/guard"):
            guard.validate_method_mutation(baseline, candidate, universal=False)

    def test_normal_method_fake_matching_protected_baseline_is_rejected(self) -> None:
        baseline, candidate = self.method_candidate_roots()
        for root in (baseline, candidate):
            clock = root / "scripts" / "deadline_harness.py"
            clock.write_text(
                clock.read_text(encoding="utf-8") + "\n# matching fake baseline\n",
                encoding="utf-8",
            )

        with self.assertRaisesRegex(guard.GuardError, "active live protected"):
            guard.validate_method_mutation(baseline, candidate, universal=False)

    def test_normal_method_mutation_rejects_unrelated_surface(self) -> None:
        baseline, candidate = self.method_candidate_roots()
        (candidate / "README.md").write_text("replacement\n", encoding="utf-8")

        with self.assertRaisesRegex(guard.GuardError, "outside its broad mutable surface"):
            guard.validate_method_mutation(baseline, candidate, universal=False)

    def test_normal_method_mutation_may_restructure_guideline_headings(self) -> None:
        baseline, candidate = self.method_candidate_roots()
        guideline = (
            candidate
            / "assets"
            / "environment"
            / guard.TASK_GUIDELINES
        )
        original = guideline.read_text(encoding="utf-8")
        mutations = (
            ("rename", "## Own the outcome", "## Own useful outcomes"),
            ("delete", "## Own the outcome\n", ""),
        )
        for name, old, new in mutations:
            with self.subTest(name=name):
                guideline.write_text(
                    original.replace(old, new, 1),
                    encoding="utf-8",
                )
                changed = guard.validate_method_mutation(
                    baseline, candidate, universal=False
                )
                self.assertIn(
                    f"assets/environment/{guard.TASK_GUIDELINES}", changed
                )
                guideline.write_text(original, encoding="utf-8")

    def test_universal_method_candidate_may_challenge_the_hard_kernel(self) -> None:
        baseline, candidate = self.method_candidate_roots()
        kernel = candidate / "scripts" / "deadline_harness.py"
        kernel.write_text(
            kernel.read_text(encoding="utf-8") + "\n# universal candidate\n",
            encoding="utf-8",
        )

        changed = guard.validate_method_mutation(
            baseline, candidate, universal=True
        )

        self.assertEqual(changed, ("scripts/deadline_harness.py",))

    def universal_review_state(
        self, lane_index: int = 1, *, capability_effort: str = "ultra"
    ) -> tuple[Path, int]:
        (self.root / "workspace.json").write_text(
            json.dumps(
                {
                    "version": 1,
                    "worker_capabilities": [
                        {
                            "model": "gpt-6-sol",
                            "reasoning_effort": capability_effort,
                        }
                    ],
                }
            ),
            encoding="utf-8",
        )
        state = self.root / f"universal-{lane_index}-{capability_effort}.sqlite"
        with patch.object(
            deadline.secrets, "randbelow", side_effect=[deadline.UNIVERSAL_RANDOM_INTERVAL - deadline.RANDOM_INTERVAL_MIN, lane_index]
        ), deadline.DeadlineHarness(state) as harness:
            for number in range(1, 31):
                task = f"terminal-{number}"
                harness.start_task(
                    "project", task, f"R-{number:03d}", 100, now=0
                )
                harness.complete_task("project", task, "terminal evidence", now=1)
            cycle = harness.list_tasks(now=2)["random_mutation"]["cycle_number"]
        return state, cycle

    def run_universal_review_cli(
        self,
        state: Path,
        cycle: int,
        baseline: Path,
        candidate: Path,
    ) -> tuple[int, str]:
        workspace_config = self.root / "workspace.json"
        if not workspace_config.exists():
            workspace_config.write_text(
                json.dumps(
                    {
                        "version": 1,
                        "worker_capabilities": [
                            {
                                "model": "gpt-6-sol",
                                "reasoning_effort": "ultra",
                            }
                        ],
                    }
                ),
                encoding="utf-8",
            )
        stdout = io.StringIO()
        stderr = io.StringIO()
        with redirect_stdout(stdout), redirect_stderr(stderr):
            result = guard.main(
                [
                    "universal-review",
                    "--baseline",
                    str(baseline),
                    "--candidate",
                    str(candidate),
                    "--state",
                    str(state),
                    "--workspace-config",
                    str(workspace_config),
                    "--lineage",
                    "project",
                    "--cycle",
                    str(cycle),
                ]
            )
        return result, stdout.getvalue() + stderr.getvalue()

    def test_universal_review_cli_requires_exact_k30_dfs_signature(self) -> None:
        baseline, candidate = self.method_candidate_roots()
        kernel = candidate / "scripts" / "deadline_harness.py"
        kernel.write_text(
            kernel.read_text(encoding="utf-8") + "\n# isolated redesign\n",
            encoding="utf-8",
        )

        state, cycle = self.universal_review_state()
        result, output = self.run_universal_review_cli(
            state, cycle, baseline, candidate
        )
        self.assertEqual(result, 0, output)
        self.assertIn("universal review cycle", output)
        self.assertRegex(output, r"receipt [0-9a-f]{64}")

        state, cycle = self.universal_review_state(lane_index=0)
        result, output = self.run_universal_review_cli(
            state, cycle, baseline, candidate
        )
        self.assertEqual(result, 1)
        self.assertIn("persisted 30-attempt FS draw", output)

        state, cycle = self.universal_review_state(capability_effort="xhigh")
        result, output = self.run_universal_review_cli(
            state, cycle, baseline, candidate
        )
        self.assertEqual(result, 1)
        self.assertIn("deferred at due time", output)
        self.assertIn("no persisted gpt-6-sol/ultra probe", output)

    def test_universal_receipt_is_atomic_immutable_and_required_for_resolution(self) -> None:
        baseline, candidate = self.method_candidate_roots()
        kernel = candidate / "scripts" / "deadline_harness.py"
        kernel.write_text(
            kernel.read_text(encoding="utf-8") + "\n# receipt candidate\n",
            encoding="utf-8",
        )
        state, cycle = self.universal_review_state()

        result, output = self.run_universal_review_cli(
            state, cycle, baseline, candidate
        )

        self.assertEqual(result, 0, output)
        receipt_id = output.rsplit("receipt ", 1)[1].strip()
        connection = sqlite3.connect(state)
        connection.row_factory = sqlite3.Row
        try:
            receipt = connection.execute(
                "SELECT * FROM universal_review_receipts WHERE receipt_id = ?",
                (receipt_id,),
            ).fetchone()
            self.assertEqual(receipt["lineage_id"], "project")
            self.assertEqual(receipt["cycle_number"], cycle)
            self.assertEqual(receipt["interval_windows"], 30)
            self.assertEqual(receipt["selected_lane"], "DFS.md")
            self.assertEqual(receipt["reviewer_model"], "gpt-6-sol")
            self.assertEqual(receipt["reviewer_effort"], "ultra")
            self.assertEqual(
                receipt["capability_roster_digest"],
                connection.execute(
                    """
                    SELECT universal_capability_roster_digest
                    FROM random_mutation_cycles
                    WHERE lineage_id = 'project' AND cycle_number = ?
                    """,
                    (cycle,),
                ).fetchone()[0],
            )
            self.assertEqual(
                json.loads(receipt["changed_paths"]),
                ["scripts/deadline_harness.py"],
            )
            self.assertEqual(len(receipt["candidate_digest"]), 64)
            with self.assertRaisesRegex(sqlite3.IntegrityError, "append-only"):
                connection.execute(
                    "UPDATE universal_review_receipts SET candidate_digest = ?",
                    ("b" * 64,),
                )
            connection.rollback()
            with self.assertRaisesRegex(sqlite3.IntegrityError, "append-only"):
                connection.execute(
                    "DELETE FROM universal_review_receipts WHERE receipt_id = ?",
                    (receipt_id,),
                )
            connection.rollback()
        finally:
            connection.close()

        with deadline.DeadlineHarness(state) as harness:
            with self.assertRaisesRegex(
                deadline.DeadlineError, "validated receipt id"
            ):
                harness.resolve_random_mutation(
                    "project", cycle, "arbitrary success string",
                    component="universal",
                )
            with self.assertRaisesRegex(
                deadline.DeadlineError, "does not match"
            ):
                harness.resolve_random_mutation(
                    "project", cycle, "fabricated receipt annotation",
                    component="universal", receipt_id="f" * 64,
                )
            resolved = harness.resolve_random_mutation(
                "project", cycle, "candidate retained for independent review",
                component="universal", receipt_id=receipt_id,
            )
            self.assertEqual(resolved["universal_receipt_id"], receipt_id)
            with self.assertRaisesRegex(
                deadline.DeadlineError, "already been consumed"
            ):
                harness.resolve_random_mutation(
                    "project", cycle, "replay",
                    component="universal", receipt_id=receipt_id,
                )
            harness.connection.execute(
                """
                INSERT INTO random_mutation_cycles (
                    lineage_id, cycle_number, interval_windows,
                    due_after_terminal_windows, selected_lane, due_task_id,
                    universal_required
                ) VALUES ('project', 2, 30, 60, 'DFS.md', 'terminal-30', 1)
                """
            )
            harness.connection.commit()
            with self.assertRaisesRegex(
                deadline.DeadlineError, "does not match this lineage and cycle"
            ):
                harness.resolve_random_mutation(
                    "project", 2, "cross-cycle replay",
                    component="universal", receipt_id=receipt_id,
                )

    def test_universal_review_uses_frozen_due_time_capability_snapshot(self) -> None:
        baseline, candidate = self.method_candidate_roots()
        kernel = candidate / "scripts" / "deadline_harness.py"
        kernel.write_text(
            kernel.read_text(encoding="utf-8") + "\n# frozen snapshot\n",
            encoding="utf-8",
        )
        state, cycle = self.universal_review_state()
        (self.root / "workspace.json").write_text(
            json.dumps(
                {
                    "version": 1,
                    "worker_capabilities": [
                        {
                            "model": "gpt-6-sol",
                            "reasoning_effort": "xhigh",
                        }
                    ],
                }
            ),
            encoding="utf-8",
        )

        result, output = self.run_universal_review_cli(
            state, cycle, baseline, candidate
        )

        self.assertEqual(result, 0, output)
        self.assertIn("universal review cycle", output)

    def test_failed_universal_guard_persists_no_receipt(self) -> None:
        baseline, candidate = self.method_candidate_roots()
        state, cycle = self.universal_review_state()

        result, output = self.run_universal_review_cli(
            state, cycle, baseline, candidate
        )

        self.assertEqual(result, 1)
        self.assertIn("makes no change", output)
        connection = sqlite3.connect(state)
        try:
            self.assertEqual(
                connection.execute(
                    "SELECT COUNT(*) FROM universal_review_receipts"
                ).fetchone()[0],
                0,
            )
        finally:
            connection.close()

    def test_universal_dfs_redesign_preserves_the_accepted_frontier(self) -> None:
        before, candidate = self.expansion_files(
            self.expansion_dfs().replace(
                "Existing next work",
                "Redesigned remaining mechanism",
            )
        )
        self.assertTrue(guard.validate_universal_dfs_mutation(before, candidate))

        before, candidate = self.expansion_files(
            self.expansion_dfs().replace(
                "Accepted frontier",
                "Rewritten accepted frontier",
            )
        )
        with self.assertRaisesRegex(guard.GuardError, "accepted claim R-000"):
            guard.validate_universal_dfs_mutation(before, candidate)


    def test_ordinary_task_results_keep_the_same_coordinator(self) -> None:
        combined = SKILL_TEXT + "\n" + TASK_GUIDANCE
        normalized = " ".join(combined.split())
        self.assertNotIn("dispatch wave complete", combined)
        self.assertNotIn("One coordinator owns one dispatch wave", combined)
        self.assertIn(
            "stay with the same coordinator",
            normalized,
        )
        self.assertIn("Mutation completion is the only planned fresh-coordinator boundary", normalized)

    def test_worker_lifecycle_is_not_a_task_requirement(self) -> None:
        combined = SKILL_TEXT + "\n" + TASK_GUIDANCE
        self.assertNotIn("Every task uses a fresh", combined)
        self.assertNotIn("one fresh worker thread", combined)
        self.assertNotIn("terminal task retires", combined)
        self.assertNotIn("worker retirement", combined)




if __name__ == "__main__":
    unittest.main()
