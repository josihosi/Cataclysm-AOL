from __future__ import annotations

import hashlib
import importlib.util
import sqlite3
import subprocess
import sys
import tempfile
import unittest
from contextlib import closing
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MODULE_PATH = ROOT / "scripts" / "trajectory_sidecar.py"
SPEC = importlib.util.spec_from_file_location("de67_trajectory_sidecar", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
sidecar = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = sidecar
SPEC.loader.exec_module(sidecar)


class TrajectorySidecarTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.workspace = self.root / "workspace"
        self.workspace.mkdir()
        subprocess.run(["git", "init", "-q", str(self.workspace)], check=True)
        subprocess.run(
            ["git", "-C", str(self.workspace), "config", "user.email", "test@example.invalid"],
            check=True,
        )
        subprocess.run(
            ["git", "-C", str(self.workspace), "config", "user.name", "Trajectory Test"],
            check=True,
        )
        (self.workspace / "owner.cpp").write_text("void owner() {}\n", encoding="utf-8")
        (self.workspace / "owner_test.cpp").write_text("void test_owner() {}\n", encoding="utf-8")
        (self.workspace / ".de67").mkdir()
        (self.workspace / ".de67" / "work-ledger.md").write_text("open\n", encoding="utf-8")
        subprocess.run(["git", "-C", str(self.workspace), "add", "."], check=True)
        subprocess.run(["git", "-C", str(self.workspace), "commit", "-qm", "baseline"], check=True)
        (self.workspace / "owner.cpp").write_text(
            "void owner() { persist_visual_observation(); publish_returned_report(); }\n",
            encoding="utf-8",
        )
        (self.workspace / "owner_test.cpp").write_text(
            "void test_owner() { prove_visual_observation_and_returned_report(); }\n",
            encoding="utf-8",
        )
        (self.workspace / ".de67" / "work-ledger.md").write_text(
            "Persist physical visual observation and publish returned report.\n",
            encoding="utf-8",
        )
        self.state = self.root / "state.sqlite3"
        with closing(sqlite3.connect(self.state)) as connection:
            connection.executescript(
                """
                CREATE TABLE lineage_binding (lineage_id TEXT PRIMARY KEY);
                CREATE TABLE closure_gaps (
                    lineage_id TEXT, claim_id TEXT, closure_sequence INTEGER, gap_id TEXT,
                    opened_at REAL, basis_task_id TEXT, closed_at REAL,
                    closure_evidence TEXT, PRIMARY KEY (lineage_id, claim_id, closure_sequence, gap_id)
                );
                CREATE TABLE closure_gap_revisions (
                    lineage_id TEXT, claim_id TEXT, closure_sequence INTEGER, gap_id TEXT,
                    revision INTEGER, recorded_at REAL, basis_task_id TEXT,
                    description TEXT, proof_route TEXT,
                    PRIMARY KEY (lineage_id, claim_id, closure_sequence, gap_id, revision)
                );
                CREATE TABLE tasks (
                    lineage_id TEXT, task_id TEXT, claim_id TEXT,
                    phase_sequence_at_dispatch INTEGER, closure_gap_id TEXT,
                    attempt_terminal_kind TEXT, started_at REAL
                );
                CREATE TABLE worker_findings (
                    lineage_id TEXT, task_id TEXT, kind TEXT, reported_at REAL,
                    short_verdict TEXT, evidence TEXT
                );
                INSERT INTO lineage_binding VALUES ('project');
                INSERT INTO closure_gaps VALUES
                    ('project', 'R-001', 2, 'G-001', 1, 'W-001', 3, 'bound production proof'),
                    ('project', 'R-001', 2, 'G-002', 1, 'W-001', NULL, NULL);
                INSERT INTO closure_gap_revisions VALUES
                    ('project', 'R-001', 2, 'G-001', 1, 1, 'W-001',
                     'Persist physical visual observation and publish returned report',
                     'Run the production owner and exact return test'),
                    ('project', 'R-001', 2, 'G-002', 1, 1, 'W-001',
                     'Continue darkness wait after reload',
                     'Run the natural night scheduler');
                INSERT INTO tasks VALUES
                    ('project', 'W-001', 'R-001', 2, 'G-001', 'completed', 1),
                    ('project', 'W-002', 'R-001', 2, 'G-002', 'finding', 2);
                INSERT INTO worker_findings VALUES
                    ('project', 'W-002', 'finding', 3,
                     'Night scheduler still needs proof',
                     'Continue darkness wait after reload');
                """
            )
            connection.commit()

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def test_report_separates_relation_from_accepted_proof_and_is_read_only(self) -> None:
        before = hashlib.sha256(self.state.read_bytes()).hexdigest()
        report = sidecar.build_report(self.workspace, self.state, "R-001")
        after = hashlib.sha256(self.state.read_bytes()).hexdigest()

        self.assertEqual(before, after)
        self.assertEqual(report.closure_sequence, 2)
        proved, open_gap = report.gaps
        self.assertTrue(proved.accepted_proof)
        self.assertFalse(open_gap.accepted_proof)
        self.assertGreater(proved.implementation_relation, open_gap.implementation_relation)
        self.assertGreater(proved.test_relation, open_gap.test_relation)
        self.assertIn("owner.cpp", proved.implementation_unit)
        self.assertNotIn("work-ledger", proved.implementation_unit)
        attention = {series.key: series for series in report.attention}
        self.assertEqual(set(attention), {"target", "code", "test", "result"})
        self.assertEqual(attention["target"].source, "W-002")
        self.assertEqual(
            [point.relative_pull for point in attention["target"].points],
            [0.0, 1.0],
        )
        self.assertEqual(attention["code"].source, "Current uncommitted diff")
        self.assertEqual(max(point.relative_pull for point in attention["code"].points), 1.0)
        self.assertEqual(attention["result"].source, "W-002")

        rendered = sidecar.render_text(report)
        self.assertIn("semantic proximity, not completion or proof", rendered)
        self.assertNotIn("%", rendered)
        self.assertNotIn("recommend", rendered.lower())
        self.assertIn("Churn vector (advisory observations, not a score)", rendered)
        self.assertEqual(report.churn_vector.product_owner.direction, "product-surface-present")
        self.assertEqual(report.churn_vector.code_surface.direction, "mixed-surface")
        self.assertEqual(
            report.churn_vector.accepted_frontier.direction,
            "open-frontier-without-latest-closure",
        )

        encoded = sidecar.asdict(report)
        self.assertEqual(
            encoded["churn_vector"]["product_owner"]["direction"],
            "product-surface-present",
        )

    def test_missing_claim_fails_without_creating_state(self) -> None:
        with self.assertRaisesRegex(sidecar.TrajectoryError, "No closure gaps"):
            sidecar.build_report(self.workspace, self.state, "R-999")

    def test_explicit_ledger_subtasks_replace_gap_axes_without_replacing_gaps(self) -> None:
        (self.workspace / ".de67" / "work-ledger.md").write_text(
            "# Ledger\n\n## Active work\n\n"
            "- [ ] R-001 — Deliver the whole route\n"
            "  - Subtasks:\n"
            "    - [done] canonical-setup :: Prepare the canonical fixture\n"
            "    - [active] signal-causation :: Establish signal causation\n"
            "    - [open] bandit-lifecycle :: Exercise the bandit lifecycle\n"
            "    - [finding] camp-interaction :: Record the camp interaction issue\n",
            encoding="utf-8",
        )

        report = sidecar.build_report(self.workspace, self.state, "R-001")

        self.assertEqual(len(report.gaps), 2)
        self.assertEqual(
            [subtask.subtask_id for subtask in report.subtasks],
            ["canonical-setup", "signal-causation", "bandit-lifecycle", "camp-interaction"],
        )
        target = next(series for series in report.attention if series.key == "target")
        self.assertEqual([point.gap_id for point in target.points], [
            "canonical-setup", "signal-causation", "bandit-lifecycle", "camp-interaction"
        ])
        self.assertEqual([point.relative_pull for point in target.points], [0.0, 1.0, 0.0, 0.0])

    def test_subtask_parser_rejects_malformed_and_duplicate_rows(self) -> None:
        ledger = self.workspace / ".de67" / "work-ledger.md"
        for body, message in (
            ("    - [doing] bad-state :: Not in the grammar\n", "Malformed subtask row"),
            ("    - [open] same-id :: First\n    - [done] same-id :: Second\n", "Duplicate subtask id"),
        ):
            with self.subTest(message=message):
                ledger.write_text(
                    "- [ ] R-001 — Route\n  - Subtasks:\n" + body,
                    encoding="utf-8",
                )
                with self.assertRaisesRegex(sidecar.TrajectoryError, message):
                    sidecar.build_report(self.workspace, self.state, "R-001")


if __name__ == "__main__":
    unittest.main()
