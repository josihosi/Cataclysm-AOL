from __future__ import annotations

import io
import hashlib
import json
import sqlite3
import subprocess
import sys
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path
from unittest.mock import patch


SCRIPTS = Path(__file__).resolve().parents[1] / "scripts"
sys.path.insert(0, str(SCRIPTS))

from deadline_harness import (  # noqa: E402
    DeadlineError,
    DeadlineHarness,
    build_parser,
    main,
    method_tree_digest,
    protected_method_digest,
    workspace_method_digest,
)


class DeadlineHarnessTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.state_path = Path(self.temporary.name) / "state.sqlite"
        self.harness = DeadlineHarness(self.state_path)

    def tearDown(self) -> None:
        self.harness.close()
        self.temporary.cleanup()

    def worker_receipt(
        self,
        task_id: str,
        claim_id: str,
        worker_id: str,
        disposition: str,
    ) -> dict[str, object]:
        return {
            "schema": "de67.worker-result-receipt.v1",
            "lineage_id": "project",
            "task_id": task_id,
            "claim_id": claim_id,
            "worker_id": worker_id,
            "disposition": disposition,
            "verdict": "continuation preserved",
            "outcome": "Prove the owned route.",
            "summary": "The prior footing is durable and the next boundary is explicit.",
            "material_changes": [],
            "tests": ["Focused contract check passed."],
            "live_actions": [],
            "evidence_ceiling": ["No product closure is claimed."],
            "bindings": {"run_id": "run-1", "scenario_id": "scenario-1"},
            "journal_entries": [
                {
                    "id": "event-1",
                    "sequence": 1,
                    "event_type": "dispatch",
                    "evidence_class": "transport",
                    "action_id": "action-1",
                    "summary": "The first dispatch crossed the owned boundary.",
                }
            ],
            "artifacts": [],
            "first_divergence": {
                "class": "next-boundary",
                "summary": "The live witness remains open.",
            },
            "accepted_no_replay": ["Do not repeat the accepted transport check."],
            "active_work": ["Capture the live witness."],
            "first_open_boundary": "Capture the live witness.",
            "narrow_queries": ["query by run_id=run-1"],
            "entrypoints": ["src/route.cpp"],
            "context_metrics": {"replaced_checkpoints": 3},
        }

    def test_worker_claim_is_durable_idempotent_and_exclusive(self) -> None:
        self.harness.start_task("project", "route-a", "R-1", 60, now=0)

        first = self.harness.claim_worker(
            "project", "route-a", "worker-a", "coordinator-a", "supervisor-a", now=1
        )
        duplicate = self.harness.claim_worker(
            "project", "route-a", "worker-a", "coordinator-a", "supervisor-a", now=2
        )

        self.assertTrue(first["recorded"])
        self.assertFalse(duplicate["recorded"])
        self.assertEqual(duplicate["claimed_at"], 1)
        with self.assertRaisesRegex(DeadlineError, "another ownership claim"):
            self.harness.claim_worker(
                "project", "route-a", "worker-b", "coordinator-a", "supervisor-a", now=3
            )

    def test_worker_checkpoints_are_ordered_and_require_the_owner(self) -> None:
        self.harness.start_task("project", "route-a", "R-1", 60, now=0)
        self.harness.claim_worker(
            "project", "route-a", "worker-a", "coordinator-a", "supervisor-a", now=1
        )

        first = self.harness.checkpoint_worker(
            "project", "route-a", "worker-a", "delegated", "handoff", now=2
        )
        second = self.harness.checkpoint_worker(
            "project", "route-a", "worker-a", "tested", "focused test", now=3
        )

        self.assertEqual((first["sequence"], second["sequence"]), (1, 2))
        with self.assertRaisesRegex(DeadlineError, "does not own"):
            self.harness.checkpoint_worker(
                "project", "route-a", "worker-b", "tested", "not mine", now=4
            )

    def test_terminal_task_releases_its_worker_claim(self) -> None:
        self.harness.start_task("project", "route-a", "R-1", 60, now=0)
        self.harness.claim_worker(
            "project", "route-a", "worker-a", "coordinator-a", "supervisor-a", now=1
        )

        receipt = self.harness.record_worker_result_receipt(
            "project",
            "route-a",
            "worker-a",
            self.worker_receipt("route-a", "R-1", "worker-a", "abandoned"),
            now=2,
        )
        self.harness.abandon_attempt(
            "project", "route-a", "owner lost", receipt_id=receipt["receipt_id"], now=3
        )

        claim = self.harness.connection.execute(
            "SELECT * FROM worker_claims WHERE lineage_id = 'project' AND task_id = 'route-a'"
        ).fetchone()
        self.assertEqual(claim["released_at"], 3)
        self.assertEqual(claim["release_reason"], "task_abandoned")

    def test_worker_owned_terminal_requires_indexed_receipt_and_queries_narrowly(self) -> None:
        self.harness.start_task("project", "route-a", "R-1", 60, now=0)
        self.harness.claim_worker(
            "project", "route-a", "worker-a", "coordinator-a", "supervisor-a", now=1
        )
        with self.assertRaisesRegex(DeadlineError, "requires its worker result receipt"):
            self.harness.abandon_attempt("project", "route-a", "owner lost", now=2)

        receipt = self.harness.record_worker_result_receipt(
            "project",
            "route-a",
            "worker-a",
            self.worker_receipt("route-a", "R-1", "worker-a", "abandoned"),
            now=3,
        )
        compact = self.harness.query_worker_result_receipts(
            "project", filters={"action_id": "action-1"}
        )
        self.assertEqual(len(compact["matches"]), 1)
        self.assertNotIn("journal_entries", compact["matches"][0])
        self.assertEqual(compact["matches"][0]["receipt_id"], receipt["receipt_id"])
        full = self.harness.query_worker_result_receipts(
            "project", filters={"run_id": "run-1"}, full=True
        )
        self.assertEqual(
            full["matches"][0]["receipt"]["journal_entries"][0]["id"],
            "event-1",
        )
        self.harness.abandon_attempt(
            "project", "route-a", "owner lost", receipt_id=receipt["receipt_id"], now=4
        )

    def test_resumed_live_attempt_can_record_later_terminal_receipt(self) -> None:
        self.harness.start_task("project", "route-a", "R-1", 60, now=0)
        self.harness.claim_worker(
            "project", "route-a", "worker-a", "coordinator-a", "supervisor-a", now=1
        )
        first_receipt = self.worker_receipt(
            "route-a", "R-1", "worker-a", "finding"
        )
        first_receipt["finding_kind"] = "blocker"
        self.harness.record_worker_result_receipt(
            "project",
            "route-a",
            "worker-a",
            first_receipt,
            now=2,
        )

        completed = self.harness.record_worker_result_receipt(
            "project",
            "route-a",
            "worker-a",
            self.worker_receipt("route-a", "R-1", "worker-a", "completed"),
            now=3,
        )
        result = self.harness.complete_task(
            "project", "route-a", "resumed work completed", receipt_id=completed["receipt_id"], now=4
        )

        self.assertEqual(result["attempt_terminal_kind"], "completed")

    def test_parallel_tasks_keep_independent_worker_claims(self) -> None:
        for task, worker in (("route-a", "worker-a"), ("route-b", "worker-b")):
            self.harness.start_task("project", task, "R-1", 60, now=0)
            self.harness.claim_worker(
                "project", task, worker, "coordinator-a", "supervisor-a", now=1
            )

        rows = self.harness.connection.execute(
            "SELECT task_id, worker_id FROM worker_claims ORDER BY task_id"
        ).fetchall()
        self.assertEqual(
            [(row["task_id"], row["worker_id"]) for row in rows],
            [("route-a", "worker-a"), ("route-b", "worker-b")],
        )

    def resolve_claim_miss(self, claim_id: str, *, diagnose: bool = True) -> None:
        if diagnose:
            self.harness.diagnose_claim_deadline(
                "project",
                claim_id,
                "estimate unsound",
                "The item clock expired before closure proof was accepted.",
                now=3,
            )
        self.harness.resolve_deadline_mutation(
            "project", claim_id, "micro", "micro guidance guarded", now=4
        )
        self.resolve_deadline_macro(
            claim_id, "macro guidance guarded", now=5
        )
        restart = self.harness.coordinator_restart_status("project")[
            "coordinator_restart"
        ]
        if restart is not None and restart["pending"]:
            run_id = f"resolved-{claim_id}"
            self.harness.claim_coordinator_restart(
                "project", restart["generation"], run_id, now=5
            )
            self.harness.acknowledge_coordinator_restart(
                "project", restart["generation"], run_id, now=5
            )

    def record_normal_receipt(
        self,
        task_id: str,
        incident_kind: str,
        *,
        harness: DeadlineHarness | None = None,
    ) -> str:
        target = self.harness if harness is None else harness
        task = target.connection.execute(
            "SELECT claim_id FROM tasks WHERE lineage_id = 'project' AND task_id = ?",
            (task_id,),
        ).fetchone()
        self.assertIsNotNone(task)
        changed_paths = ["SKILL.md"]
        contract = {
            "lineage_id": "project",
            "task_id": task_id,
            "claim_id": task["claim_id"],
            "incident_kind": incident_kind,
            "candidate_digest": method_tree_digest(),
            "changed_paths": changed_paths,
            "protected_baseline_digest": protected_method_digest(),
            "live_tree_digest": method_tree_digest(),
        }
        receipt_id = hashlib.sha256(
            json.dumps(contract, sort_keys=True, separators=(",", ":")).encode(
                "utf-8"
            )
        ).hexdigest()
        target.connection.execute(
            """
            INSERT OR IGNORE INTO normal_method_receipts (
                receipt_id, lineage_id, task_id, claim_id, incident_kind,
                validated_at, candidate_digest, changed_paths,
                protected_baseline_digest, live_tree_digest
            ) VALUES (?, 'project', ?, ?, ?, 1, ?, ?, ?, ?)
            """,
            (
                receipt_id,
                task_id,
                task["claim_id"],
                incident_kind,
                contract["candidate_digest"],
                json.dumps(changed_paths, separators=(",", ":")),
                contract["protected_baseline_digest"],
                contract["live_tree_digest"],
            ),
        )
        target.connection.commit()
        return receipt_id

    def test_workspace_local_guideline_candidate_can_retire_macro_mutation(self) -> None:
        self.harness.close()
        workspace = Path(self.temporary.name) / "workspace"
        state_path = workspace / ".de67" / "state" / "deadlines.sqlite3"
        environment = workspace / ".de67"
        environment.mkdir(parents=True)
        packaged = Path(__file__).resolve().parents[1] / "assets" / "environment"
        for name in ("test-and-task-guidelines.md",):
            content = (packaged / name).read_text(encoding="utf-8")
            if name == "test-and-task-guidelines.md":
                content += "\nWorkspace-local guarded rule.\n"
            (environment / name).write_text(content, encoding="utf-8")

        with DeadlineHarness(state_path) as harness:
            harness.start_task("project", "late", "R-LATE", 1, now=0)
            harness.expire_task("project", "late", now=2)
            harness.diagnose_claim_deadline(
                "project", "R-LATE", "late", "The item clock expired.", now=3
            )
            harness.resolve_deadline_mutation(
                "project", "R-LATE", "micro", "finite recovery", now=4
            )
            candidate_digest = workspace_method_digest(state_path.resolve())
            self.assertIsNotNone(candidate_digest)
            changed_paths = ["assets/environment/test-and-task-guidelines.md"]
            contract = {
                "lineage_id": "project",
                "task_id": "late",
                "claim_id": "R-LATE",
                "incident_kind": "deadline_miss",
                "candidate_digest": candidate_digest,
                "changed_paths": changed_paths,
                "protected_baseline_digest": protected_method_digest(),
                "live_tree_digest": method_tree_digest(),
            }
            receipt_id = hashlib.sha256(
                json.dumps(contract, sort_keys=True, separators=(",", ":")).encode(
                    "utf-8"
                )
            ).hexdigest()
            harness.connection.execute(
                """
                INSERT INTO normal_method_receipts (
                    receipt_id, lineage_id, task_id, claim_id, incident_kind,
                    validated_at, candidate_digest, changed_paths,
                    protected_baseline_digest, live_tree_digest
                ) VALUES (?, 'project', 'late', 'R-LATE', 'deadline_miss',
                          4, ?, ?, ?, ?)
                """,
                (
                    receipt_id,
                    candidate_digest,
                    json.dumps(changed_paths, separators=(",", ":")),
                    protected_method_digest(),
                    method_tree_digest(),
                ),
            )
            harness.connection.commit()

            resolved = harness.resolve_deadline_mutation(
                "project",
                "R-LATE",
                "macro",
                "workspace-local method changed",
                receipt_id=receipt_id,
                now=5,
            )

            self.assertEqual(resolved["pending_components"], [])
            self.assertEqual(resolved["receipt_id"], receipt_id)

    def resolve_deadline_macro(
        self, claim_id: str, evidence: str, *, now: float
    ) -> dict[str, object]:
        incident = self.harness.connection.execute(
            """
            SELECT source_task_id FROM claim_deadline_incidents
            WHERE lineage_id = 'project' AND claim_id = ?
            """,
            (claim_id,),
        ).fetchone()
        self.assertIsNotNone(incident)
        receipt_id = self.record_normal_receipt(
            str(incident["source_task_id"]), "deadline_miss"
        )
        return self.harness.resolve_deadline_mutation(
            "project",
            claim_id,
            "macro",
            evidence,
            receipt_id=receipt_id,
            now=now,
        )

    def resolve_integrity_macro(
        self, task_id: str, evidence: str, *, now: float
    ) -> dict[str, object]:
        receipt_id = self.record_normal_receipt(task_id, "integrity_breach")
        return self.harness.resolve_integrity_mutation(
            "project",
            task_id,
            "macro",
            evidence,
            receipt_id=receipt_id,
            now=now,
        )

    def establish_accepted_claim(
        self, claim_id: str = "R-001", *, prefix: str = ""
    ) -> None:
        explore = f"{prefix}explore"
        closure = f"{prefix}closure"
        self.harness.start_task("project", explore, claim_id, 100, now=0)
        self.harness.complete_task(
            "project", explore, "exploration proved", now=1
        )
        self.harness.transition_claim_to_closure(
            "project",
            claim_id,
            explore,
            "The owner route returns the finished outcome.",
            "Run the owner route and inspect its durable artifact.",
            "Only owner-route closure remains.",
            now=2,
        )
        self.harness.start_task(
            "project", closure, claim_id, 100, phase="closure", now=3
        )
        self.harness.complete_task("project", closure, "closure proved", now=4)
        self.harness.accept_claim(
            "project", claim_id, closure, "closure evidence accepted", now=5
        )

    def record_universal_receipt(self, cycle_number: int = 1) -> str:
        candidate_digest = "a" * 64
        changed_paths = ["scripts/deadline_harness.py"]
        cycle = self.harness.connection.execute(
            """
            SELECT universal_capability_roster_digest
            FROM random_mutation_cycles
            WHERE lineage_id = 'project' AND cycle_number = ?
            """,
            (cycle_number,),
        ).fetchone()
        capability_roster_digest = cycle["universal_capability_roster_digest"]
        receipt_contract = {
            "lineage_id": "project",
            "cycle_number": cycle_number,
            "candidate_digest": candidate_digest,
            "changed_paths": changed_paths,
            "interval_windows": 30,
            "selected_lane": "DFS.md",
            "reviewer_model": "gpt-5.6-sol",
            "reviewer_effort": "ultra",
            "capability_roster_digest": capability_roster_digest,
        }
        receipt_id = hashlib.sha256(
            json.dumps(
                receipt_contract, sort_keys=True, separators=(",", ":")
            ).encode("utf-8")
        ).hexdigest()
        self.harness.connection.execute(
            """
            INSERT INTO universal_review_receipts (
                receipt_id, lineage_id, cycle_number, validated_at,
                candidate_digest, changed_paths, interval_windows,
                selected_lane, reviewer_model, reviewer_effort,
                capability_roster_digest
            ) VALUES (?, 'project', ?, 31, ?, ?, 30, 'DFS.md',
                      'gpt-5.6-sol', 'ultra', ?)
            """,
            (
                receipt_id,
                cycle_number,
                candidate_digest,
                json.dumps(changed_paths),
                capability_roster_digest,
            ),
        )
        self.harness.connection.commit()
        return receipt_id

    def write_sol_ultra_capability(self) -> None:
        (self.state_path.parent / "workspace.json").write_text(
            json.dumps(
                {
                    "version": 1,
                    "worker_capabilities": [
                        {
                            "model": "gpt-5.6-sol",
                            "reasoning_effort": "ultra",
                        }
                    ],
                }
            ),
            encoding="utf-8",
        )

    def test_repeated_start_preserves_identity_clock_and_estimate(self) -> None:
        first = self.harness.start_task("project", "task", "R-1", 10, now=100)
        self.harness.close()
        self.harness = DeadlineHarness(self.state_path)
        repeated = self.harness.start_task("project", "task", "R-1", 10, now=500)

        self.assertTrue(first["created"])
        self.assertFalse(repeated["created"])
        self.assertEqual(repeated["claim_id"], "R-1")
        self.assertEqual(repeated["estimate_seconds"], 10)
        self.assertEqual(repeated["started_at"], 100)
        self.assertEqual(repeated["deadline_at"], 110)
        with self.assertRaises(DeadlineError):
            self.harness.start_task("project", "task", "R-2", 10, now=500)
        with self.assertRaises(DeadlineError):
            self.harness.start_task("project", "task", "R-1", 11, now=500)

    def test_explicit_attempt_estimate_must_fit_remaining_claim_deadline(self) -> None:
        first = self.harness.start_task("project", "first", "R-1", 100, now=0)
        self.harness.complete_task("project", "first", "usable result", now=20)

        second = self.harness.start_task(
            "project", "second", "R-1", 100,
            attempt_estimate_seconds=70, now=30,
        )

        self.assertEqual(first["deadline_at"], 100)
        self.assertEqual(second["estimate_seconds"], 100)
        self.assertEqual(second["attempt_estimate_seconds"], 70)
        self.assertEqual(second["deadline_at"], 100)
        self.harness.complete_task("project", "second", "usable result", now=31)
        with self.assertRaisesRegex(
            DeadlineError,
            "Attempt estimate exceeds the remaining claim deadline",
        ):
            self.harness.start_task(
                "project", "too-large", "R-1", 100,
                attempt_estimate_seconds=70, now=31,
            )

    def test_repeated_task_id_cannot_cross_a_phase_transition(self) -> None:
        self.harness.start_task("project", "same", "R-1", 100, now=0)
        self.harness.complete_task("project", "same", "learning", now=1)
        self.harness.transition_claim_to_closure(
            "project",
            "R-1",
            "same",
            "Finished owner outcome.",
            "Run the owner route.",
            "Owner-route proof remains.",
            now=2,
        )

        with self.assertRaisesRegex(DeadlineError, "dispatch phase"):
            self.harness.start_task(
                "project", "same", "R-1", 100, phase="closure", now=3
            )
        with self.assertRaisesRegex(sqlite3.IntegrityError, "dispatch phase"):
            self.harness.connection.execute(
                """
                UPDATE tasks SET phase_sequence_at_dispatch = 99
                WHERE lineage_id = 'project' AND task_id = 'same'
                """
            )
        self.harness.connection.rollback()

    def test_deadline_miss_is_recorded_once(self) -> None:
        self.harness.start_task("project", "task", "R-1", 10, now=100)

        first = self.harness.expire_task("project", "task", now=111)
        repeated = self.harness.expire_task("project", "task", now=200)

        self.assertTrue(first["incident"]["recorded"])
        self.assertEqual(first["incident"]["units"], 1)
        self.assertTrue(first["incident"]["independent_review_required"])
        self.assertFalse(repeated["incident"]["recorded"])
        self.assertEqual(repeated["status"]["cumulative_miss_units"], 1)

    def test_deadline_miss_command_rejects_an_early_item_miss(self) -> None:
        self.harness.start_task("project", "task", "R-1", 100, now=100)

        with self.assertRaisesRegex(DeadlineError, "has not expired"):
            self.harness.miss_claim_deadline("project", "R-1", now=110)

        first = self.harness.miss_claim_deadline("project", "R-1", now=200)
        repeated = self.harness.miss_claim_deadline("project", "R-1", now=220)

        self.assertTrue(first["incident"]["recorded"])
        self.assertEqual(first["deadline_at"], 200)
        self.assertTrue(first["mutation_pending"])
        self.assertFalse(repeated["incident"]["recorded"])
        self.assertEqual(repeated["deadline_at"], 200)
        self.assertEqual(
            self.harness.coordinator_view(now=220)["pending_incident_reviews"],
            [{"claim_id": "R-1", "task_id": "task", "kind": "deadline_miss"}],
        )

    def test_late_completion_is_accepted_without_erasing_miss(self) -> None:
        self.harness.start_task("project", "task", "R-1", 10, now=100)

        completed = self.harness.complete_task("project", "task", "test output", now=111)
        later = self.harness.status_task("project", "task", now=500)

        self.assertTrue(completed["attempt_completed"])
        self.assertFalse(completed["completion_accepted"])
        self.assertEqual(completed["state"], "completed")
        self.assertTrue(completed["deadline_missed"])
        self.assertEqual(later["cumulative_miss_units"], 1)
        self.assertTrue(later["deadline_missed"])

    def test_parallel_task_ids_have_independent_clocks(self) -> None:
        self.harness.start_task("project", "short", "R-1", 10, now=100)
        self.harness.start_task("project", "long", "R-2", 30, now=100)

        short = self.harness.status_task("project", "short", now=111)
        long = self.harness.status_task("project", "long", now=111)
        completed = self.harness.complete_task("project", "long", "green test", now=120)

        self.assertTrue(short["deadline_missed"])
        self.assertEqual(long["state"], "running")
        self.assertFalse(long["deadline_missed"])
        self.assertTrue(completed["attempt_completed"])
        self.assertFalse(completed["deadline_missed"])

    def test_cumulative_misses_report_three_and_six_cadence(self) -> None:
        incidents = []
        for number in range(1, 7):
            task_id = f"task-{number}"
            self.harness.start_task("project", task_id, f"R-{number}", 1, now=0)
            incidents.append(
                self.harness.expire_task("project", task_id, now=2)["incident"]
            )
            self.resolve_claim_miss(f"R-{number}")

        self.assertFalse(incidents[1]["cadence_crossed"])
        self.assertTrue(incidents[2]["cadence_crossed"])
        self.assertEqual(incidents[2]["cadence_threshold"], 3)
        self.assertFalse(incidents[4]["cadence_crossed"])
        self.assertTrue(incidents[5]["cadence_crossed"])
        self.assertEqual(incidents[5]["cadence_threshold"], 6)
        self.assertEqual(incidents[5]["cumulative_after"], 6)

    def test_state_database_rejects_lineage_reset_but_allows_new_tasks(self) -> None:
        self.harness.start_task("alpha", "task", "R-A", 10, now=100)
        self.harness.start_task("alpha", "retry", "R-A", 10, now=101)
        self.harness.close()
        self.harness = DeadlineHarness(self.state_path)

        with self.assertRaises(DeadlineError):
            self.harness.start_task("beta", "task", "R-B", 30, now=102)

        original = self.harness.status_task("alpha", "task", now=102)
        retry = self.harness.status_task("alpha", "retry", now=102)
        self.assertEqual(original["claim_id"], "R-A")
        self.assertEqual(retry["claim_id"], "R-A")
        self.assertEqual(retry["deadline_at"], 110)
        self.assertEqual(retry["started_at"], 100)

    def test_list_reconciles_multiple_tasks_and_miss_summary(self) -> None:
        self.harness.start_task("project", "late", "R-1", 10, now=100)
        self.harness.start_task("project", "active", "R-2", 30, now=100)

        summary = self.harness.list_tasks(now=111)
        tasks = {task["task_id"]: task for task in summary["tasks"]}

        self.assertEqual(summary["lineage_id"], "project")
        self.assertEqual(summary["cumulative_miss_units"], 1)
        self.assertNotIn("incidents", summary)
        self.assertEqual(summary["recent_failure_verdicts"], [])
        self.assertEqual(len(summary["pending_incident_reviews"]), 1)
        self.assertEqual(
            summary["pending_incident_reviews"][0]["kind"], "deadline_miss"
        )
        self.assertEqual(
            summary["pending_incident_reviews"][0]["task_id"], "late"
        )
        self.assertEqual(tasks["late"]["state"], "deadline_missed")
        self.assertEqual(tasks["active"]["state"], "running")

    def test_list_requires_binding_and_cli_needs_no_task_identity(self) -> None:
        unbound_path = Path(self.temporary.name) / "unbound.sqlite"
        with DeadlineHarness(unbound_path) as unbound:
            with self.assertRaises(DeadlineError):
                unbound.list_tasks(now=100)

        cli_path = Path(self.temporary.name) / "list-cli.sqlite"
        with DeadlineHarness(cli_path) as harness:
            harness.start_task("project", "task", "R-1", 100)
        output = io.StringIO()
        with redirect_stdout(output):
            self.assertEqual(main(["list", "--state", str(cli_path)]), 0)
        payload = json.loads(output.getvalue())

        self.assertEqual(payload["lineage_id"], "project")
        self.assertEqual([task["task_id"] for task in payload["tasks"]], ["task"])

    def test_list_omits_accepted_tasks_but_keeps_nonaccepted_current_work(self) -> None:
        with patch("deadline_harness.secrets.randbelow", side_effect=[10, 0]):
            self.harness.start_task(
                "project", "accepted-basis", "R-001", 100, now=0
            )
        self.harness.complete_task(
            "project", "accepted-basis", "exploration green", now=1
        )
        self.harness.transition_claim_to_closure(
            "project",
            "R-001",
            "accepted-basis",
            "Prove the finite closure outcome.",
            "Run the named closure check.",
            "The named closure check remains.",
            now=2,
        )
        self.harness.start_task(
            "project", "accepted", "R-001", 100, phase="closure", now=3
        )
        self.harness.complete_task("project", "accepted", "closure green", now=4)
        self.harness.accept_claim(
            "project", "R-001", "accepted", "closure evidence accepted", now=5
        )

        self.harness.start_task(
            "project", "accepted-2-basis", "R-002", 100, now=0
        )
        self.harness.complete_task(
            "project", "accepted-2-basis", "exploration green", now=1
        )
        self.harness.transition_claim_to_closure(
            "project",
            "R-002",
            "accepted-2-basis",
            "Prove the second finite outcome.",
            "Run the second closure check.",
            "The second closure check remains.",
            now=2,
        )
        self.harness.start_task(
            "project", "late-accepted", "R-002", 100, phase="closure", now=3
        )
        self.harness.complete_task(
            "project", "late-accepted", "second closure green", now=4
        )
        self.harness.accept_claim(
            "project",
            "R-002",
            "late-accepted",
            "second closure evidence accepted",
            now=5,
        )

        self.harness.start_task("project", "breached", "R-003", 100, now=0)
        self.harness.start_task("project", "finding", "R-004", 100, now=0)
        self.harness.start_task("project", "running", "R-005", 100, now=0)
        self.harness.complete_task("project", "breached", "later invalidated", now=1)
        self.harness.record_integrity_breach(
            "project", "breached", "fabricated long reason", now=2
        )

        self.harness.report_worker_finding(
            "project",
            "finding",
            "blocker",
            "long dependency evidence",
            short_verdict="dependency absent",
            now=3,
        )
        with patch.object(
            self.harness, "_status", wraps=self.harness._status
        ) as status:
            summary = self.harness.list_tasks(now=4)
        listed = {task["task_id"]: task for task in summary["tasks"]}

        self.assertEqual(set(listed), {"breached", "finding", "running"})
        self.assertEqual(
            {call.args[1] for call in status.call_args_list},
            {"breached", "finding", "running"},
        )
        self.assertEqual(listed["breached"]["state"], "integrity_breach")
        self.assertEqual(listed["finding"]["state"], "worker_finding")
        self.assertEqual(listed["running"]["state"], "running")
        for task in listed.values():
            self.assertNotIn("completion_evidence", task)
            self.assertNotIn("incidents", task)
            self.assertNotIn("worker_finding", task)
            self.assertNotIn("integrity_reason", task)

        pending = summary["pending_incident_reviews"]
        self.assertEqual(
            {review["task_id"] for review in pending},
            {"breached"},
        )
        self.assertEqual(
            [verdict["task_id"] for verdict in summary["recent_failure_verdicts"]],
            ["finding"],
        )
        verdicts = pending + summary["recent_failure_verdicts"]
        for verdict in verdicts:
            self.assertNotIn("evidence", verdict)
            self.assertNotIn("reason", verdict)
            self.assertNotIn("long_detail", verdict)

    def test_list_keeps_only_ten_recent_short_failure_verdicts(self) -> None:
        with patch("deadline_harness.secrets.randbelow", side_effect=[10, 0]):
            for number in range(1, 13):
                task_id = f"W-{number:03d}"
                if number % 2:
                    self.harness.start_task(
                        "project", task_id, f"R-{number:03d}", 1, now=0
                    )
                    self.harness.expire_task("project", task_id, now=number)
                    self.harness.diagnose_incident(
                        "project",
                        task_id,
                        "deadline_miss",
                        f"short {number}",
                        f"long incident diagnosis {number}",
                        now=100 + number,
                    )
                    self.resolve_claim_miss(f"R-{number:03d}", diagnose=False)
                else:
                    self.harness.start_task(
                        "project", task_id, f"R-{number:03d}", 100, now=0
                    )
                    self.harness.report_worker_finding(
                        "project",
                        task_id,
                        "unexpected",
                        f"long worker evidence {number}",
                        short_verdict=f"short {number}",
                        now=number,
                    )

        summary = self.harness.list_tasks(now=20)
        verdicts = summary["recent_failure_verdicts"]
        quiet = self.harness.coordinator_view(now=20)
        startup = self.harness.coordinator_view(
            include_recent_verdicts=True, now=20
        )

        self.assertEqual(summary["pending_incident_reviews"], [])
        self.assertEqual(len(verdicts), 10)
        self.assertNotIn("recent_failure_verdicts", quiet)
        self.assertEqual(len(startup["recent_failure_verdicts"]), 10)
        self.assertEqual(
            [verdict["task_id"] for verdict in verdicts],
            [f"W-{number:03d}" for number in range(12, 2, -1)],
        )
        self.assertEqual(
            [verdict["short_verdict"] for verdict in verdicts],
            [f"short {number}" for number in range(12, 2, -1)],
        )
        for verdict in verdicts:
            self.assertNotIn("evidence", verdict)
            self.assertNotIn("reason", verdict)
            self.assertNotIn("long_detail", verdict)
        for verdict in startup["recent_failure_verdicts"]:
            self.assertNotIn("evidence", verdict)
            self.assertNotIn("reason", verdict)
            self.assertNotIn("long_detail", verdict)

        incident_status = self.harness.status_task("project", "W-011", now=20)
        self.assertEqual(
            incident_status["incidents"][0]["long_detail"],
            "long incident diagnosis 11",
        )
        finding_status = self.harness.status_task("project", "W-012", now=20)
        self.assertEqual(
            finding_status["worker_finding"]["evidence"],
            "long worker evidence 12",
        )

    def test_list_omits_completed_task_with_prior_finding_but_keeps_verdict(self) -> None:
        self.harness.start_task("project", "task", "R-001", 100, now=0)
        self.harness.report_worker_finding(
            "project",
            "task",
            "unexpected",
            "Long finding evidence retained for exact diagnostics.",
            short_verdict="owner contradicted",
            now=1,
        )
        self.harness.connection.execute(
            """
            UPDATE tasks
            SET completed_at = 2, completion_evidence = 'accepted after finding review'
            WHERE lineage_id = 'project' AND task_id = 'task'
            """
        )
        self.harness.connection.commit()

        summary = self.harness.list_tasks(now=3)

        self.assertEqual(
            [task["task_id"] for task in summary["tasks"]], ["task"]
        )
        self.assertEqual(summary["tasks"][0]["state"], "worker_finding")
        self.assertEqual(summary["pending_incident_reviews"], [])
        self.assertEqual(
            summary["recent_failure_verdicts"],
            [
                {
                    "task_id": "task",
                    "claim_id": "R-001",
                    "kind": "unexpected",
                    "recorded_at": 1,
                    "short_verdict": "owner contradicted",
                }
            ],
        )

    def test_list_keeps_only_current_route_for_an_unresolved_claim(self) -> None:
        for number in (1, 2):
            task_id = f"attempt-{number}"
            self.harness.start_task(
                "project", task_id, "R-001", 100, now=number
            )
            self.harness.report_worker_finding(
                "project",
                task_id,
                "unexpected",
                f"Long evidence for attempt {number}.",
                short_verdict=f"route {number} failed",
                now=number + 0.25,
            )

        summary = self.harness.list_tasks(now=3)
        self.assertEqual(
            [task["task_id"] for task in summary["tasks"]], ["attempt-2"]
        )
        self.assertEqual(
            [item["task_id"] for item in summary["recent_failure_verdicts"]],
            ["attempt-2", "attempt-1"],
        )

        self.harness.start_task("project", "attempt-3", "R-001", 100, now=4)
        summary = self.harness.list_tasks(now=5)
        self.assertEqual(
            [task["task_id"] for task in summary["tasks"]], ["attempt-3"]
        )

        self.harness.complete_task(
            "project", "attempt-3", "The FS claim is now proven.", now=6
        )
        self.assertEqual(
            [task["task_id"] for task in self.harness.list_tasks(now=7)["tasks"]],
            ["attempt-3"],
        )

    def test_accepted_subtask_does_not_hide_a_new_route_for_the_same_claim(self) -> None:
        self.harness.start_task("project", "partial", "R-001", 100, now=0)
        self.harness.complete_task(
            "project", "partial", "One necessary seam is accepted.", now=1
        )
        self.harness.start_task("project", "next", "R-001", 100, now=2)

        summary = self.harness.list_tasks(now=3)

        self.assertEqual(
            [task["task_id"] for task in summary["tasks"]], ["next"]
        )

    def test_acknowledged_restart_arms_a_new_deadline_generation(self) -> None:
        self.harness.start_task("project", "miss-1", "R-001", 1, now=0)
        self.harness.expire_task("project", "miss-1", now=2)
        self.resolve_claim_miss("R-001")
        self.harness.abandon_attempt(
            "project", "miss-1", "the next worker replaces this attempt", now=2.5
        )

        self.harness.start_task("project", "miss-2", "R-001", 1, now=3)
        repeated = self.harness.expire_task("project", "miss-2", now=4)

        summary = self.harness.list_tasks(now=4)
        self.assertTrue(repeated["incident"]["recorded"])
        self.assertEqual(repeated["status"]["deadline_generation"], 2)
        generations = self.harness.connection.execute(
            """
            SELECT generation, started_at, deadline_at
            FROM claim_deadline_generations
            WHERE lineage_id = 'project' AND claim_id = 'R-001'
            ORDER BY generation
            """
        ).fetchall()
        self.assertEqual(
            [tuple(row) for row in generations],
            [(1, 0.0, 1.0), (2, 3.0, 4.0)],
        )
        self.assertEqual(summary["cumulative_miss_units"], 2)
        self.assertEqual(
            [task["task_id"] for task in summary["tasks"]], ["miss-2"]
        )
        self.assertEqual(
            summary["pending_incident_reviews"][0]["task_id"], "miss-2"
        )
        self.assertEqual(
            [item["task_id"] for item in summary["recent_failure_verdicts"]],
            ["miss-1"],
        )

    def test_mutation_retires_old_clock_and_fresh_coordinator_sets_new_one(self) -> None:
        self.harness.start_task("project", "before-mutation", "R-001", 10, now=0)
        self.harness.complete_task(
            "project", "before-mutation", "terminal before mutation", now=5
        )

        retired = self.harness.retire_claim_clocks_for_mutation(
            "project", "random mutation", now=6
        )
        self.assertEqual(retired, 1)
        self.assertEqual(
            self.harness.list_tasks(now=100)["pending_deadline_mutations"], []
        )

        restart = self.harness.request_coordinator_restart(
            "project", "mutation complete", now=101
        )["coordinator_restart"]
        self.harness.claim_coordinator_restart(
            "project", restart["generation"], "fresh", now=102
        )
        self.harness.acknowledge_coordinator_restart(
            "project", restart["generation"], "fresh", now=102
        )
        started = self.harness.start_task(
            "project", "after-mutation", "R-001", 300, now=103
        )

        self.assertEqual(started["deadline_generation"], 2)
        self.assertEqual(started["estimate_seconds"], 300)
        self.assertEqual(started["deadline_at"], 403)

    def test_cli_retired_generation_uses_new_estimate_without_rewriting_history(self) -> None:
        self.harness.start_task("project", "before", "R-001", 259200, now=0)
        self.harness.complete_task("project", "before", "preserved evidence", now=5)
        self.harness.retire_claim_clocks_for_mutation("project", "owner review", now=6)
        history = tuple(self.harness.connection.execute(
            "SELECT * FROM claim_deadline_generations WHERE generation = 1"
        ).fetchone())
        restart = self.harness.request_coordinator_restart(
            "project", "review complete", now=7
        )["coordinator_restart"]
        args = ["start", "--state", str(self.state_path), "--lineage", "project",
                "--task", "after", "--claim", "R-001", "--estimate-seconds", "345600"]
        with patch("deadline_harness.time.time", return_value=10), redirect_stdout(io.StringIO()):
            self.assertEqual(main(args), 2)  # Pending restart cannot arm a generation.
        self.harness.claim_coordinator_restart("project", restart["generation"], "fresh", now=8)
        self.harness.acknowledge_coordinator_restart("project", restart["generation"], "fresh", now=9)
        with patch("deadline_harness.time.time", return_value=10), redirect_stdout(io.StringIO()):
            self.assertEqual(main(args), 0)
            self.assertEqual(main(args), 0)  # Replay cannot create another generation.
        claim = self.harness._claim("project", "R-001")
        self.assertEqual(claim["deadline_generation"], 2)
        self.assertEqual(claim["estimate_seconds"], 345600)
        self.assertEqual(claim["deadline_at"], 345610)
        self.assertEqual(claim["armed_by_restart_generation"], restart["generation"])
        self.assertEqual(self.harness._task("project", "after")["deadline_generation"], 2)
        self.assertEqual(tuple(self.harness.connection.execute(
            "SELECT * FROM claim_deadline_generations WHERE generation = 1"
        ).fetchone()), history)
        # An ordinary subsequent attempt keeps the current whole-claim deadline.
        args[args.index("after")] = "next"
        args[-1] = "100"
        with patch("deadline_harness.time.time", return_value=11), redirect_stdout(io.StringIO()):
            self.assertEqual(main(args), 0)
        self.assertEqual(self.harness._claim("project", "R-001")["deadline_at"], 345610)
        self.assertEqual(self.harness.connection.execute(
            "SELECT COUNT(*) FROM claim_deadline_generations"
        ).fetchone()[0], 2)

    def test_mutation_clock_retirement_refuses_a_live_worker(self) -> None:
        self.harness.start_task("project", "live", "R-001", 100, now=0)

        with self.assertRaisesRegex(DeadlineError, "worker attempt is running"):
            self.harness.retire_claim_clocks_for_mutation(
                "project", "random mutation", now=1
            )

    def test_seven_due_breaches_cannot_crowd_out_pending_incident_reviews(self) -> None:
        with patch("deadline_harness.secrets.randbelow", side_effect=[10, 0]):
            for number in range(1, 8):
                task_id = f"W-{number:03d}"
                self.harness.start_task(
                    "project", task_id, f"R-{number:03d}", 1, now=0
                )
            for number in range(1, 8):
                task_id = f"W-{number:03d}"
                self.harness.record_integrity_breach(
                    "project", task_id, f"long breach reason {number}", now=2
                )
                self.resolve_claim_miss(f"R-{number:03d}")

        summary = self.harness.list_tasks(now=3)
        pending = summary["pending_incident_reviews"]

        self.assertEqual(len(pending), 7)
        self.assertEqual(
            {
                (review["task_id"], review["kind"])
                for review in pending
            },
            {
                (f"W-{number:03d}", "integrity_breach")
                for number in range(1, 8)
            },
        )
        self.assertEqual(
            {
                (verdict["task_id"], verdict["kind"])
                for verdict in summary["recent_failure_verdicts"]
            },
            {
                (f"W-{number:03d}", "deadline_miss")
                for number in range(1, 8)
            },
        )

    def test_due_integrity_breach_records_miss_before_one_breach_unit(self) -> None:
        self.harness.start_task("project", "task", "R-1", 10, now=100)

        result = self.harness.record_integrity_breach(
            "project", "task", "fabricated result", now=111
        )

        miss = result["deadline_incident"]
        breach = result["incident"]
        self.assertTrue(miss["recorded"])
        self.assertEqual(miss["cumulative_before"], 0)
        self.assertEqual(miss["cumulative_after"], 1)
        self.assertEqual(breach["cumulative_before"], 1)
        self.assertEqual(breach["cumulative_after"], 2)
        self.assertIsNone(breach["cadence_threshold"])
        self.assertEqual(result["status"]["cumulative_miss_units"], 2)
        self.assertEqual(
            [incident["kind"] for incident in result["status"]["incidents"]],
            ["deadline_miss", "integrity_breach"],
        )

    def test_incident_diagnosis_is_persisted_immutable_and_cli_visible(self) -> None:
        self.harness.start_task("project", "task", "R-1", 10, now=100)
        self.harness.expire_task("project", "task", now=111)
        self.assertTrue(
            self.harness.status_task("project", "task", now=111)["incidents"][0][
                "independent_review_required"
            ]
        )

        first = self.harness.diagnose_incident(
            "project",
            "task",
            "deadline_miss",
            "test overdefined",
            "The intended proof required behavior outside the FS claim.",
            now=112,
        )
        repeated = self.harness.diagnose_incident(
            "project",
            "task",
            "deadline_miss",
            "test overdefined",
            "The intended proof required behavior outside the FS claim.",
            now=113,
        )

        self.assertTrue(first["recorded"])
        self.assertFalse(repeated["recorded"])
        self.assertEqual(first["incident"]["short_verdict"], "test overdefined")
        self.assertEqual(first["incident"]["reviewed_at"], 112)
        self.assertFalse(first["incident"]["independent_review_required"])
        with self.assertRaisesRegex(DeadlineError, "immutable"):
            self.harness.diagnose_incident(
                "project",
                "task",
                "deadline_miss",
                "different verdict",
                "Different diagnosis",
                now=114,
            )

        output = io.StringIO()
        with redirect_stdout(output):
            self.assertEqual(
                main(
                    [
                        "status",
                        "--state",
                        str(self.state_path),
                        "--lineage",
                        "project",
                        "--task",
                        "task",
                    ]
                ),
                0,
            )
        payload = json.loads(output.getvalue())
        self.assertEqual(payload["incidents"][0]["short_verdict"], "test overdefined")
        self.assertEqual(
            payload["incidents"][0]["long_detail"],
            "The intended proof required behavior outside the FS claim.",
        )

    def test_documented_diagnose_cli_records_exact_incident_review(self) -> None:
        self.harness.start_task("project", "task", "R-1", 10, now=100)
        self.harness.expire_task("project", "task", now=111)
        output = io.StringIO()
        with redirect_stdout(output):
            self.assertEqual(
                main(
                    [
                        "diagnose",
                        "--state",
                        str(self.state_path),
                        "--lineage",
                        "project",
                        "--task",
                        "task",
                        "--kind",
                        "deadline_miss",
                        "--short-verdict",
                        "estimate unsound",
                        "--diagnosis",
                        "Measured setup time contradicted the estimate premise.",
                    ]
                ),
                0,
            )
        payload = json.loads(output.getvalue())
        self.assertTrue(payload["recorded"])
        self.assertEqual(payload["incident"]["short_verdict"], "estimate unsound")
        self.assertEqual(payload["claim_id"], "R-1")
        self.assertEqual(payload["pending_components"], ["micro", "macro"])
        self.assertNotIn("long_detail", payload["incident"])
        claim_incident = self.harness.connection.execute(
            """
            SELECT short_verdict, long_detail, reviewed_at
            FROM claim_deadline_incidents
            WHERE lineage_id = 'project' AND claim_id = 'R-1'
            """
        ).fetchone()
        self.assertEqual(claim_incident["short_verdict"], "estimate unsound")
        self.assertEqual(
            claim_incident["long_detail"],
            "Measured setup time contradicted the estimate premise.",
        )
        self.assertIsNotNone(claim_incident["reviewed_at"])
        status = self.harness.status_task("project", "task", now=112)
        self.assertEqual(
            status["incidents"][0]["long_detail"],
            "Measured setup time contradicted the estimate premise.",
        )

    def test_cli_start_uses_documented_flags_and_spawns_only_once(self) -> None:
        state_path = Path(self.temporary.name) / "cli.sqlite"
        arguments = [
            "start",
            "--state",
            str(state_path),
            "--lineage",
            "project",
            "--task",
            "task",
            "--claim",
            "R-1",
            "--estimate-seconds",
            "100",
        ]

        with patch("deadline_harness.spawn_watcher") as spawn, redirect_stdout(io.StringIO()):
            self.assertEqual(main(arguments), 0)
            self.assertEqual(main(arguments), 0)

        spawn.assert_called_once_with(str(state_path), "project", "R-1")

    def test_integrity_breach_adds_one_once_and_invalidates_completion(self) -> None:
        self.harness.start_task("project", "first", "R-1", 100, now=0)
        self.harness.start_task("project", "second", "R-2", 100, now=0)
        self.harness.complete_task("project", "first", "initial evidence", now=1)

        first = self.harness.record_integrity_breach(
            "project", "first", "fabricated result", now=2
        )
        repeated = self.harness.record_integrity_breach(
            "project", "first", "new wording", now=3
        )

        self.assertTrue(first["incident"]["recorded"])
        self.assertEqual(first["incident"]["units"], 1)
        self.assertFalse(first["incident"]["cadence_crossed"])
        self.assertIsNone(first["incident"]["cadence_threshold"])
        self.assertFalse(first["status"]["completion_accepted"])
        self.assertFalse(repeated["incident"]["recorded"])
        self.assertEqual(repeated["status"]["cumulative_miss_units"], 1)
        self.assertEqual(repeated["status"]["integrity_reason"], "fabricated result")

        second = self.harness.record_integrity_breach(
            "project", "second", "hidden reset", now=4
        )
        self.assertIsNone(second["incident"]["cadence_threshold"])
        self.assertEqual(second["incident"]["cumulative_after"], 2)

        with self.assertRaises(DeadlineError):
            self.harness.complete_task(
                "project", "first", "replacement evidence", now=5
            )

    def test_completion_rejects_empty_evidence(self) -> None:
        self.harness.start_task("project", "task", "R-1", 10, now=100)

        for evidence in ("", "   "):
            with self.subTest(evidence=repr(evidence)):
                with self.assertRaises(DeadlineError):
                    self.harness.complete_task("project", "task", evidence, now=101)

        self.assertFalse(
            self.harness.status_task("project", "task", now=101)["completion_accepted"]
        )

    def test_on_time_worker_finding_terminalizes_attempt_but_claim_clock_continues(self) -> None:
        self.harness.start_task("project", "task", "R-1", 10, now=100)

        reported = self.harness.report_worker_finding(
            "project", "task", "blocker", "missing production dependency", now=109
        )
        later = self.harness.status_task("project", "task", now=500)

        self.assertTrue(reported["finding"]["recorded"])
        self.assertIsNone(reported["deadline_incident"])
        self.assertEqual(reported["status"]["state"], "worker_finding")
        self.assertFalse(reported["status"]["completion_accepted"])
        self.assertFalse(reported["status"]["deadline_missed"])
        self.assertEqual(later["state"], "worker_finding")
        self.assertEqual(later["cumulative_miss_units"], 1)
        self.assertTrue(later["deadline_mutation_pending"])
        self.assertEqual(later["worker_finding"]["kind"], "blocker")
        self.assertEqual(
            later["worker_finding"]["evidence"], "missing production dependency"
        )
        with self.assertRaises(DeadlineError):
            self.harness.complete_task("project", "task", "green test", now=501)

    def test_late_worker_finding_preserves_exactly_one_deadline_miss(self) -> None:
        self.harness.start_task("project", "task", "R-1", 10, now=100)

        reported = self.harness.report_worker_finding(
            "project", "task", "unexpected", "runtime contradicts FS", now=110
        )
        repeated_expiry = self.harness.expire_task("project", "task", now=500)

        self.assertTrue(reported["deadline_incident"]["recorded"])
        self.assertEqual(reported["deadline_incident"]["units"], 1)
        self.assertEqual(reported["status"]["state"], "worker_finding")
        self.assertTrue(reported["status"]["deadline_missed"])
        self.assertFalse(repeated_expiry["incident"]["recorded"])
        self.assertEqual(repeated_expiry["status"]["cumulative_miss_units"], 1)

    def test_late_worker_finding_verdict_precedes_same_time_deadline_miss(self) -> None:
        self.harness.start_task("project", "task", "R-1", 10, now=100)

        self.harness.report_worker_finding(
            "project",
            "task",
            "unexpected",
            "The production owner returned a contradictory long result.",
            short_verdict="owner contradicted",
            now=110,
        )
        summary = self.harness.list_tasks(now=110)

        self.assertEqual(summary["tasks"][0]["state"], "worker_finding")
        self.assertEqual(
            summary["tasks"][0]["current_short_verdict"], "owner contradicted"
        )
        self.assertEqual(
            summary["recent_failure_verdicts"][0]["short_verdict"],
            "owner contradicted",
        )
        self.assertEqual(
            summary["pending_incident_reviews"],
            [
                {
                    "task_id": "task",
                    "claim_id": "R-1",
                    "kind": "deadline_miss",
                    "recorded_at": 110,
                }
            ],
        )

    def test_worker_finding_is_validated_immutable_and_terminal(self) -> None:
        self.harness.start_task("project", "task", "R-1", 10, now=100)

        for kind, evidence in (("unknown", "evidence"), ("blocker", "   ")):
            with self.subTest(kind=kind, evidence=repr(evidence)):
                with self.assertRaises(DeadlineError):
                    self.harness.report_worker_finding(
                        "project", "task", kind, evidence, now=101
                    )

        first = self.harness.report_worker_finding(
            "project",
            "task",
            "unexpected",
            "observed result",
            short_verdict="production owner contradicted",
            now=102,
        )
        repeated = self.harness.report_worker_finding(
            "project", "task", "unexpected", "observed result", now=500
        )

        self.assertTrue(first["finding"]["recorded"])
        self.assertEqual(
            first["finding"]["short_verdict"], "production owner contradicted"
        )
        self.assertFalse(repeated["finding"]["recorded"])
        self.assertEqual(
            repeated["finding"]["short_verdict"], "production owner contradicted"
        )
        self.assertEqual(repeated["finding"]["reported_at"], 102)
        self.assertEqual(repeated["status"]["cumulative_miss_units"], 1)
        for kind, evidence in (
            ("blocker", "observed result"),
            ("unexpected", "different evidence"),
        ):
            with self.subTest(kind=kind, evidence=evidence):
                with self.assertRaisesRegex(DeadlineError, "immutable"):
                    self.harness.report_worker_finding(
                        "project", "task", kind, evidence, now=501
                    )
        with self.assertRaisesRegex(DeadlineError, "immutable"):
            self.harness.report_worker_finding(
                "project",
                "task",
                "unexpected",
                "observed result",
                short_verdict="different short verdict",
                now=501,
            )

    def test_completed_or_breached_task_rejects_worker_finding(self) -> None:
        self.harness.start_task("project", "completed", "R-1", 10, now=100)
        self.harness.complete_task("project", "completed", "green test", now=101)
        with self.assertRaises(DeadlineError):
            self.harness.report_worker_finding(
                "project", "completed", "blocker", "too late", now=102
            )

        self.harness.start_task("project", "breached", "R-2", 10, now=100)
        self.harness.record_integrity_breach(
            "project", "breached", "fabricated output", now=101
        )
        with self.assertRaises(DeadlineError):
            self.harness.report_worker_finding(
                "project", "breached", "unexpected", "too late", now=102
            )

    def test_cli_finding_and_list_expose_terminal_finding(self) -> None:
        state_path = Path(self.temporary.name) / "finding-cli.sqlite"
        with DeadlineHarness(state_path) as harness:
            harness.start_task("project", "task", "R-1", 100)
        finding_output = io.StringIO()
        with redirect_stdout(finding_output):
            self.assertEqual(
                main(
                    [
                        "finding",
                        "--state",
                        str(state_path),
                        "--lineage",
                        "project",
                        "--task",
                        "task",
                        "--kind",
                        "blocker",
                        "--short-verdict",
                        "dependency absent",
                        "--evidence",
                        "dependency is absent",
                    ]
                ),
                0,
            )
        finding_payload = json.loads(finding_output.getvalue())

        list_output = io.StringIO()
        with redirect_stdout(list_output):
            self.assertEqual(main(["list", "--state", str(state_path)]), 0)
        listed = json.loads(list_output.getvalue())
        listed_task = listed["tasks"][0]

        startup_output = io.StringIO()
        with redirect_stdout(startup_output):
            self.assertEqual(
                main(["startup-view", "--state", str(state_path)]), 0
            )
        startup = json.loads(startup_output.getvalue())

        self.assertEqual(finding_payload["finding"]["kind"], "blocker")
        self.assertNotIn("evidence", finding_payload["finding"])
        self.assertEqual(listed_task["state"], "worker_finding")
        self.assertNotIn("worker_finding", listed_task)
        self.assertEqual(listed_task["current_short_verdict"], "dependency absent")
        self.assertNotIn("recent_failure_verdicts", listed)
        self.assertEqual(
            startup["recent_failure_verdicts"][0]["short_verdict"],
            "dependency absent",
        )
        self.assertNotIn("evidence", startup["recent_failure_verdicts"][0])

    def test_restart_request_persists_and_blocks_only_new_task_ids(self) -> None:
        initial = self.harness.start_task(
            "project", "existing", "R-001", 100, now=0
        )
        self.assertIsNone(initial["coordinator_restart"])

        first = self.harness.request_coordinator_restart(
            "project", "guarded guideline mutation", now=1
        )
        repeated = self.harness.request_coordinator_restart(
            "project", "later request coalesces into the pending baton", now=2
        )

        self.assertTrue(first["created"])
        self.assertFalse(repeated["created"])
        self.assertEqual(first["coordinator_restart"]["generation"], 1)
        self.assertTrue(repeated["coordinator_restart"]["pending"])
        self.assertEqual(
            repeated["coordinator_restart"]["reason"],
            "guarded guideline mutation",
        )
        resumed = self.harness.start_task(
            "project", "existing", "R-001", 100, now=3
        )
        self.assertFalse(resumed["created"])
        self.assertTrue(resumed["coordinator_restart"]["pending"])
        with self.assertRaisesRegex(DeadlineError, "restart generation 1 is pending"):
            self.harness.start_task("project", "new", "R-002", 100, now=3)

        self.harness.close()
        self.harness = DeadlineHarness(self.state_path)
        persisted = self.harness.list_tasks(now=3)["coordinator_restart"]
        self.assertEqual(persisted["generation"], 1)
        self.assertTrue(persisted["pending"])

        with self.assertRaisesRegex(DeadlineError, "claimed by its supervisor"):
            self.harness.acknowledge_coordinator_restart(
                "project", 1, "retiring-coordinator", now=4
            )
        claimed = self.harness.claim_coordinator_restart(
            "project", 1, "run-fresh-1", now=4
        )
        repeated_claim = self.harness.claim_coordinator_restart(
            "project", 1, "run-fresh-1", now=5
        )
        self.assertTrue(claimed["recorded"])
        self.assertFalse(repeated_claim["recorded"])
        with self.assertRaisesRegex(DeadlineError, "different run"):
            self.harness.claim_coordinator_restart(
                "project", 1, "wrong-run", now=5
            )
        with self.assertRaisesRegex(DeadlineError, "does not match"):
            self.harness.release_coordinator_restart_claim(
                "project", 1, "wrong-run"
            )

        acknowledged = self.harness.acknowledge_coordinator_restart(
            "project", 1, "run-fresh-1", now=4
        )
        repeated_ack = self.harness.acknowledge_coordinator_restart(
            "project", 1, "run-fresh-1", now=5
        )
        self.assertTrue(acknowledged["recorded"])
        self.assertFalse(repeated_ack["recorded"])
        self.assertEqual(
            repeated_ack["coordinator_restart"]["acknowledged_at"], 4
        )
        with self.assertRaisesRegex(DeadlineError, "claimed run"):
            self.harness.acknowledge_coordinator_restart(
                "project", 1, "different-run", now=6
            )
        with self.assertRaisesRegex(DeadlineError, "cannot be released"):
            self.harness.release_coordinator_restart_claim(
                "project", 1, "run-fresh-1"
            )

        started = self.harness.start_task("project", "new", "R-002", 100, now=6)
        self.assertTrue(started["created"])
        self.assertFalse(started["coordinator_restart"]["pending"])

        next_request = self.harness.request_coordinator_restart(
            "project", "next guarded mutation", now=7
        )
        self.assertTrue(next_request["created"])
        self.assertEqual(next_request["coordinator_restart"]["generation"], 2)
        generations = self.harness.connection.execute(
            """
            SELECT generation FROM coordinator_restart_requests
            ORDER BY generation
            """
        ).fetchall()
        self.assertEqual([row["generation"] for row in generations], [1, 2])

    def test_restart_request_and_acknowledgement_cli(self) -> None:
        state_path = Path(self.temporary.name) / "restart-cli.sqlite"
        with DeadlineHarness(state_path) as harness:
            harness.start_task("project", "existing", "R-001", 100, now=0)

        request_output = io.StringIO()
        with redirect_stdout(request_output):
            self.assertEqual(
                main(
                    [
                        "request-restart",
                        "--state",
                        str(state_path),
                        "--lineage",
                        "project",
                        "--reason",
                        "guarded FS expansion",
                    ]
                ),
                0,
            )
        requested = json.loads(request_output.getvalue())
        self.assertTrue(requested["created"])
        self.assertTrue(requested["coordinator_restart"]["pending"])

        with DeadlineHarness(state_path) as harness:
            harness.claim_coordinator_restart("project", 1, "dead-run-cli")
        release_output = io.StringIO()
        with redirect_stdout(release_output):
            self.assertEqual(
                main(
                    [
                        "release-restart-claim",
                        "--state",
                        str(state_path),
                        "--lineage",
                        "project",
                        "--generation",
                        "1",
                        "--run-id",
                        "dead-run-cli",
                    ]
                ),
                0,
            )
        self.assertTrue(json.loads(release_output.getvalue())["released"])
        with DeadlineHarness(state_path) as harness:
            harness.claim_coordinator_restart("project", 1, "run-cli-1")

        ack_output = io.StringIO()
        with redirect_stdout(ack_output):
            self.assertEqual(
                main(
                    [
                        "ack-restart",
                        "--state",
                        str(state_path),
                        "--lineage",
                        "project",
                        "--generation",
                        "1",
                        "--run-id",
                        "run-cli-1",
                    ]
                ),
                0,
            )
        acknowledged = json.loads(ack_output.getvalue())
        self.assertTrue(acknowledged["recorded"])
        self.assertNotIn("coordinator_restart", acknowledged)
        with DeadlineHarness(state_path) as harness:
            restart = harness.coordinator_restart_status("project")[
                "coordinator_restart"
            ]
        self.assertFalse(restart["pending"])
        self.assertEqual(restart["run_id"], "run-cli-1")

    @staticmethod
    def complete_windows(
        harness: DeadlineHarness,
        count: int,
        *,
        first: int = 1,
    ) -> None:
        for number in range(first, first + count):
            task = f"window-{number}"
            harness.start_task("project", task, f"R-{number:03d}", 10, now=0)
            harness.complete_task("project", task, "green", now=1)

    def test_random_interval_includes_twenty_and_fifty_boundaries(self) -> None:
        self.harness.close()
        for offset, boundary in ((0, 20), (30, 50)):
            with self.subTest(boundary=boundary):
                state = Path(self.temporary.name) / f"boundary-{boundary}.sqlite"
                with patch(
                    "deadline_harness.secrets.randbelow", side_effect=[offset, 0]
                ), DeadlineHarness(state) as harness:
                    self.complete_windows(harness, boundary - 1)
                    before = harness.list_tasks(now=2)["random_mutation"]
                    self.assertFalse(before["due"])
                    task = f"window-{boundary}"
                    harness.start_task(
                        "project", task, f"R-{boundary:03d}", 10, now=0
                    )
                    running = harness.status_task("project", task, now=1)
                    self.assertFalse(running["random_mutation"]["due"])
                    terminal = harness.complete_task(
                        "project", task, "green", now=2
                    )
                    self.assertTrue(terminal["random_mutation"]["due"])
                    self.assertEqual(
                        terminal["random_mutation"]["interval_windows"], boundary
                    )
        self.harness = DeadlineHarness(self.state_path)

    def test_random_schedule_persists_across_restart_without_redraw(self) -> None:
        with patch(
            "deadline_harness.secrets.randbelow", side_effect=[0, 1]
        ) as draw:
            self.complete_windows(self.harness, 8)
            self.assertEqual(draw.call_count, 2)
        self.harness.close()
        with patch(
            "deadline_harness.secrets.randbelow",
            side_effect=AssertionError("persisted schedule must not redraw"),
        ):
            self.harness = DeadlineHarness(self.state_path)
            self.complete_windows(self.harness, 12, first=9)
            schedule = self.harness.list_tasks(now=2)["random_mutation"]

        self.assertEqual(schedule["interval_windows"], 20)
        self.assertEqual(schedule["selected_lane"], "DFS.md")
        self.assertEqual(schedule["completed_terminal_windows"], 20)
        self.assertTrue(schedule["due"])


    def test_random_lane_draw_is_persisted_and_not_cli_controlled(self) -> None:
        self.harness.close()
        for lane_index, lane in enumerate(
            (
                "test-and-task-guidelines.md",
                "DFS.md",
            )
        ):
            with self.subTest(lane=lane):
                state = Path(self.temporary.name) / f"lane-{lane_index}.sqlite"
                with patch(
                    "deadline_harness.secrets.randbelow",
                    side_effect=[0, lane_index],
                ), DeadlineHarness(state) as harness:
                    started = harness.start_task(
                        "project", "task", "R-001", 10, now=0
                    )
                    self.assertEqual(
                        started["random_mutation"]["selected_lane"], lane
                    )
                    self.assertEqual(
                        started["random_mutation"]["due_after_terminal_windows"],
                        20,
                    )
        self.harness = DeadlineHarness(self.state_path)

    def test_each_terminal_route_counts_once_and_late_followups_do_not(self) -> None:
        with patch("deadline_harness.secrets.randbelow", side_effect=[0, 0]):
            self.complete_windows(self.harness, 10)
            self.complete_windows(self.harness, 6, first=11)

            self.harness.start_task("project", "finding", "R-007", 10, now=0)
            self.harness.report_worker_finding(
                "project", "finding", "blocker", "direct evidence", now=1
            )

            self.harness.start_task("project", "miss", "R-008", 10, now=0)
            self.harness.expire_task("project", "miss", now=11)
            self.resolve_claim_miss("R-008")
            self.harness.complete_task("project", "miss", "late green", now=12)

            self.harness.start_task("project", "breach", "R-009", 10, now=0)
            self.harness.start_task("project", "completion", "R-010", 10, now=0)
            self.harness.record_integrity_breach(
                "project", "breach", "fabricated evidence", now=1
            )

            result = self.harness.complete_task(
                "project", "completion", "green", now=1
            )
            repeated = self.harness.complete_task(
                "project", "completion", "different text", now=2
            )

        self.assertEqual(result["random_mutation"]["completed_terminal_windows"], 20)
        self.assertTrue(result["random_mutation"]["due"])
        self.assertEqual(repeated["random_mutation"]["completed_terminal_windows"], 20)
        self.assertEqual(
            self.harness.list_tasks(now=20)["random_mutation"]["completed_terminal_windows"],
            20,
        )

    def test_due_review_blocks_new_dispatch_until_exactly_once_resolution(self) -> None:
        with patch("deadline_harness.secrets.randbelow", side_effect=[0, 1]):
            self.complete_windows(self.harness, 20)
        schedule = self.harness.list_tasks(now=2)["random_mutation"]
        self.assertTrue(schedule["due"])
        self.harness.close()
        self.harness = DeadlineHarness(self.state_path)
        with self.assertRaisesRegex(DeadlineError, "resolve it before dispatching"):
            self.harness.start_task("project", "blocked", "R-011", 10, now=2)

        with patch("deadline_harness.secrets.randbelow", side_effect=[0, 1]) as draw:
            first = self.harness.resolve_random_mutation(
                "project",
                schedule["cycle_number"],
                "guard accepted exact FS no-op; ledger review retained",
            )
            repeated = self.harness.resolve_random_mutation(
                "project",
                schedule["cycle_number"],
                "guard accepted exact FS no-op; ledger review retained",
            )
        self.assertTrue(first["recorded"])
        self.assertFalse(repeated["recorded"])
        self.assertTrue(first["coordinator_restart"]["pending"])
        self.assertEqual(first["coordinator_restart"]["generation"], 1)
        self.assertEqual(
            repeated["coordinator_restart"]["generation"],
            first["coordinator_restart"]["generation"],
        )
        restart_count = self.harness.connection.execute(
            "SELECT COUNT(*) AS total FROM coordinator_restart_requests"
        ).fetchone()["total"]
        self.assertEqual(restart_count, 1)
        self.assertEqual(draw.call_count, 2)
        self.assertEqual(first["random_mutation"]["interval_windows"], 20)
        self.assertFalse(first["random_mutation"]["due"])
        with self.assertRaisesRegex(DeadlineError, "restart generation 1 is pending"):
            self.harness.start_task("project", "unblocked", "R-011", 10, now=5)
        self.harness.claim_coordinator_restart(
            "project", 1, "run-after-random-noop"
        )
        self.harness.acknowledge_coordinator_restart(
            "project", 1, "run-after-random-noop"
        )
        started = self.harness.start_task(
            "project", "unblocked", "R-011", 10, now=5
        )
        self.assertTrue(started["created"])

    def test_documented_random_resolution_cli_flags_execute(self) -> None:
        with patch("deadline_harness.secrets.randbelow", side_effect=[0, 1]):
            self.complete_windows(self.harness, 20)
        cycle = self.harness.list_tasks(now=2)["random_mutation"]["cycle_number"]
        output = io.StringIO()
        with patch(
            "deadline_harness.secrets.randbelow", side_effect=[0, 0]
        ), redirect_stdout(output):
            self.assertEqual(
                main(
                    [
                        "resolve-random-mutation",
                        "--state",
                        str(self.state_path),
                        "--lineage",
                        "project",
                        "--cycle",
                        str(cycle),
                        "--evidence",
                        "guard passed and target was applied",
                    ]
                ),
                0,
            )
        payload = json.loads(output.getvalue())
        self.assertTrue(payload["recorded"])
        self.assertEqual(payload["cycle_number"], cycle)

    def test_running_status_and_list_never_trigger_random_review(self) -> None:
        with patch("deadline_harness.secrets.randbelow", side_effect=[0, 1]):
            self.complete_windows(self.harness, 9)
            running = self.harness.start_task(
                "project", "tenth", "R-010", 100, now=0
            )
            status = self.harness.status_task("project", "tenth", now=50)
            listed = self.harness.list_tasks(now=50)

        self.assertFalse(running["random_mutation"]["due"])
        self.assertFalse(status["random_mutation"]["due"])
        self.assertFalse(listed["random_mutation"]["due"])
        self.assertEqual(listed["random_mutation"]["completed_terminal_windows"], 9)

    @staticmethod
    def strip_random_cadence(state: Path) -> None:
        connection = sqlite3.connect(state)
        try:
            connection.execute("DROP TABLE random_mutation_cycles")
            connection.execute("DROP TABLE coordinator_restart_requests")
            connection.execute("ALTER TABLE tasks DROP COLUMN terminal_at")
            connection.commit()
        finally:
            connection.close()

    def test_legacy_database_backfills_compact_and_long_failure_forms(self) -> None:
        self.harness.close()
        legacy = Path(self.temporary.name) / "legacy-compact-history.sqlite"
        connection = sqlite3.connect(legacy)
        try:
            connection.executescript(
                """
                CREATE TABLE tasks (
                    lineage_id TEXT NOT NULL,
                    task_id TEXT NOT NULL,
                    claim_id TEXT NOT NULL,
                    estimate_seconds REAL NOT NULL,
                    started_at REAL NOT NULL,
                    deadline_at REAL NOT NULL,
                    completed_at REAL,
                    completion_evidence TEXT,
                    integrity_breached_at REAL,
                    integrity_reason TEXT,
                    PRIMARY KEY (lineage_id, task_id)
                );
                CREATE TABLE incidents (
                    incident_id INTEGER PRIMARY KEY AUTOINCREMENT,
                    lineage_id TEXT NOT NULL,
                    task_id TEXT NOT NULL,
                    kind TEXT NOT NULL,
                    recorded_at REAL NOT NULL,
                    reason TEXT,
                    units INTEGER NOT NULL,
                    cumulative_before INTEGER NOT NULL,
                    cumulative_after INTEGER NOT NULL,
                    cadence_threshold INTEGER,
                    UNIQUE (lineage_id, task_id, kind)
                );
                CREATE TABLE worker_findings (
                    lineage_id TEXT NOT NULL,
                    task_id TEXT NOT NULL,
                    kind TEXT NOT NULL,
                    reported_at REAL NOT NULL,
                    evidence TEXT NOT NULL,
                    PRIMARY KEY (lineage_id, task_id)
                );
                CREATE TABLE lineage_binding (
                    singleton INTEGER PRIMARY KEY,
                    lineage_id TEXT NOT NULL
                );
                """
            )
            connection.executemany(
                """
                INSERT INTO tasks VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                """,
                (
                    ("project", "legacy-miss", "R-001", 10, 0, 10, None, None, None, None),
                    (
                        "project",
                        "legacy-finding",
                        "R-002",
                        10,
                        0,
                        10,
                        None,
                        None,
                        None,
                        None,
                    ),
                ),
            )
            connection.execute(
                """
                INSERT INTO incidents (
                    lineage_id, task_id, kind, recorded_at, reason, units,
                    cumulative_before, cumulative_after, cadence_threshold
                ) VALUES ('project', 'legacy-miss', 'deadline_miss', 11, NULL, 1, 0, 1, NULL)
                """
            )
            connection.execute(
                """
                INSERT INTO worker_findings
                VALUES ('project', 'legacy-finding', 'blocker', 5, 'legacy long evidence')
                """
            )
            connection.execute(
                "INSERT INTO lineage_binding VALUES (1, 'project')"
            )
            connection.commit()
        finally:
            connection.close()

        with patch(
            "deadline_harness.secrets.randbelow", side_effect=[10, 0]
        ), DeadlineHarness(legacy) as migrated:
            summary = migrated.list_tasks(now=6)
            self.assertEqual(
                [item["short_verdict"] for item in summary["recent_failure_verdicts"]],
                ["blocker"],
            )
            self.assertEqual(
                summary["pending_incident_reviews"],
                [
                    {
                        "task_id": "legacy-miss",
                        "claim_id": "R-001",
                        "kind": "deadline_miss",
                        "recorded_at": 11,
                    }
                ],
            )
            miss = migrated.status_task("project", "legacy-miss", now=6)
            self.assertEqual(
                miss["incidents"][0]["long_detail"],
                "The immutable deadline passed without an on-time terminal result.",
            )
            finding = migrated.status_task("project", "legacy-finding", now=6)
            self.assertEqual(
                finding["worker_finding"]["evidence"], "legacy long evidence"
            )
            diagnosed = migrated.diagnose_incident(
                "project",
                "legacy-miss",
                "deadline_miss",
                "tooling unchecked",
                "The required tool had not been probed before dispatch.",
                now=12,
            )
            self.assertTrue(diagnosed["recorded"])
            incident_columns = {
                row["name"]
                for row in migrated.connection.execute(
                    "PRAGMA table_info(incidents)"
                ).fetchall()
            }
            finding_columns = {
                row["name"]
                for row in migrated.connection.execute(
                    "PRAGMA table_info(worker_findings)"
                ).fetchall()
            }
            self.assertTrue(
                {"short_verdict", "long_detail", "reviewed_at"}
                <= incident_columns
            )
            self.assertIn("short_verdict", finding_columns)
        self.harness = DeadlineHarness(self.state_path)

    def test_legacy_terminal_history_seeds_the_first_random_cycle(self) -> None:
        self.harness.close()
        legacy = Path(self.temporary.name) / "legacy-history.sqlite"
        with patch("deadline_harness.secrets.randbelow", side_effect=[0, 0]), DeadlineHarness(
            legacy
        ) as harness:
            for number in range(1, 12):
                task = f"window-{number}"
                harness.start_task(
                    "project", task, f"R-{number:03d}", 100, now=0
                )
                harness.complete_task("project", task, "green", now=number)
            harness.start_task("project", "window-12", "R-012", 100, now=0)
            harness.report_worker_finding(
                "project", "window-12", "blocker", "legacy finding", now=12
            )
            harness.start_task("project", "window-13", "R-013", 100, now=0)
            harness.expire_task("project", "window-13", now=101)
            harness.diagnose_claim_deadline(
                "project", "R-013", "late", "legacy miss diagnosis", now=102
            )
            harness.resolve_deadline_mutation(
                "project", "R-013", "micro", "micro guarded", now=103
            )
            receipt = harness.connection.execute(
                "SELECT source_task_id FROM claim_deadline_incidents WHERE lineage_id = 'project' AND claim_id = 'R-013'"
            ).fetchone()
            self.assertIsNotNone(receipt)
            receipt_id = self.record_normal_receipt(
                str(receipt["source_task_id"]), "deadline_miss", harness=harness
            )
            harness.resolve_deadline_mutation(
                "project", "R-013", "macro", "macro guarded",
                receipt_id=receipt_id, now=104
            )
            restart = harness.coordinator_restart_status("project")[
                "coordinator_restart"
            ]
            harness.claim_coordinator_restart(
                "project", restart["generation"], "legacy-resume", now=104
            )
            harness.acknowledge_coordinator_restart(
                "project", restart["generation"], "legacy-resume", now=104
            )
            harness.start_task("project", "window-14", "R-014", 100, now=0)
            harness.record_integrity_breach(
                "project", "window-14", "legacy breach", now=14
            )
        self.strip_random_cadence(legacy)

        with patch("deadline_harness.secrets.randbelow", side_effect=[0, 1]), DeadlineHarness(
            legacy
        ) as migrated:
            schedule = migrated.list_tasks(now=2)["random_mutation"]
            self.assertEqual(schedule["completed_terminal_windows"], 13)
            self.assertEqual(schedule["due_after_terminal_windows"], 20)
            self.assertIsNone(schedule["due_task_id"])
            self.assertFalse(schedule["due"])
            with self.assertRaisesRegex(DeadlineError, "Integrity mutation is pending"):
                migrated.start_task("project", "blocked", "R-015", 10, now=3)
        self.harness = DeadlineHarness(self.state_path)

    def test_first_post_upgrade_terminal_window_is_not_skipped(self) -> None:
        self.harness.close()
        legacy = Path(self.temporary.name) / "legacy-running.sqlite"
        with patch("deadline_harness.secrets.randbelow", side_effect=[10, 0]), DeadlineHarness(
            legacy
        ) as harness:
            harness.start_task("project", "running", "R-001", 100, now=0)
        self.strip_random_cadence(legacy)

        with patch("deadline_harness.secrets.randbelow", side_effect=[0, 1]), DeadlineHarness(
            legacy
        ) as migrated:
            result = migrated.complete_task(
                "project", "running", "post-upgrade green", now=1
            )
            self.assertEqual(
                result["random_mutation"]["completed_terminal_windows"], 1
            )
            self.assertEqual(result["random_mutation"]["due_after_terminal_windows"], 20)
        self.harness = DeadlineHarness(self.state_path)

    def test_claim_clock_survives_attempts_closure_and_reopen(self) -> None:
        first = self.harness.start_task(
            "project", "explore", "R-001", 50, now=100
        )
        self.harness.complete_task(
            "project", "explore", "exploration evidence", now=105
        )
        closure = self.harness.transition_claim_to_closure(
            "project",
            "R-001",
            "explore",
            "Prove the finite outcome.",
            "Run the named closure check.",
            "The named closure check remains open.",
            now=106,
        )
        attempt = self.harness.start_task(
            "project", "close", "R-001", 50, phase="closure", now=110
        )

        self.assertEqual(first["claim_started_at"], 100)
        self.assertEqual(first["attempt_dispatched_at"], 100)
        self.assertEqual(attempt["claim_started_at"], 100)
        self.assertEqual(attempt["attempt_dispatched_at"], 110)
        self.assertEqual(attempt["deadline_at"], 150)
        self.assertEqual(attempt["phase_sequence_at_dispatch"], 2)
        self.assertEqual(closure["deadline_at"], 150)
        with self.assertRaisesRegex(DeadlineError, "cannot change"):
            self.harness.start_task(
                "project", "changed-clock", "R-001", 51, now=111
            )
        with self.assertRaises(sqlite3.IntegrityError):
            self.harness.connection.execute(
                """
                UPDATE claim_clocks SET deadline_at = 999
                WHERE lineage_id = 'project' AND claim_id = 'R-001'
                """
            )
        self.harness.connection.rollback()

        self.harness.close()
        self.harness = DeadlineHarness(self.state_path)
        persisted = self.harness.status_task("project", "close", now=120)
        view = self.harness.coordinator_view(now=120)

        self.assertEqual(persisted["claim_started_at"], 100)
        self.assertEqual(persisted["attempt_dispatched_at"], 110)
        self.assertEqual(persisted["deadline_at"], 150)
        self.assertEqual(view["claim_clock_migration_conflicts"], [])

    def test_deadline_miss_is_claim_scoped_and_does_not_terminalize_attempt(self) -> None:
        self.harness.start_task("project", "finding", "R-001", 5, now=100)
        self.harness.start_task("project", "running", "R-001", 5, now=101)
        self.harness.report_worker_finding(
            "project",
            "finding",
            "unexpected",
            "The exploration route was falsified.",
            now=102,
        )

        missed = self.harness.status_task("project", "running", now=106)
        repeated = self.harness.status_task("project", "finding", now=107)
        counts = self.harness.connection.execute(
            """
            SELECT
              (SELECT COUNT(*) FROM claim_deadline_incidents) AS claim_misses,
              (SELECT COUNT(*) FROM incidents WHERE kind = 'deadline_miss') AS legacy_misses,
              (SELECT COUNT(*) FROM tasks WHERE attempt_terminal_at IS NOT NULL) AS terminals
            """
        ).fetchone()

        self.assertTrue(missed["deadline_missed"])
        self.assertIsNone(missed["attempt_terminal_at"])
        self.assertEqual(repeated["cumulative_miss_units"], 1)
        self.assertEqual(dict(counts), {
            "claim_misses": 1,
            "legacy_misses": 1,
            "terminals": 1,
        })
        with self.assertRaisesRegex(DeadlineError, "Deadline mutation is pending"):
            self.harness.start_task(
                "project", "blocked", "R-001", 5, now=108
            )

        diagnosis = self.harness.diagnose_claim_deadline(
            "project",
            "R-001",
            "estimate premise failed",
            "The immutable item clock expired before closure acceptance.",
            now=109,
        )
        micro = self.harness.resolve_deadline_mutation(
            "project", "R-001", "micro", "micro guidance changed", now=110
        )
        macro = self.resolve_deadline_macro(
            "R-001", "macro method changed", now=111
        )
        repeated_macro = self.resolve_deadline_macro(
            "R-001", "macro method changed", now=112
        )
        restart_count = self.harness.connection.execute(
            "SELECT COUNT(*) AS total FROM coordinator_restart_requests"
        ).fetchone()["total"]

        self.assertTrue(diagnosis["recorded"])
        self.assertEqual(micro["pending_components"], ["macro"])
        self.assertIsNone(micro["coordinator_restart"])
        self.assertEqual(macro["pending_components"], [])
        self.assertTrue(macro["coordinator_restart"]["pending"])
        self.assertFalse(repeated_macro["recorded"])
        self.assertEqual(restart_count, 1)
        self.assertEqual(
            self.harness.list_tasks(now=112)["pending_incident_reviews"], []
        )

    def test_dispatch_after_expiry_persists_miss_and_is_blocked(self) -> None:
        self.harness.start_task("project", "first", "R-001", 5, now=100)

        with self.assertRaisesRegex(DeadlineError, "Deadline mutation is pending"):
            self.harness.start_task(
                "project", "must-not-dispatch", "R-001", 5, now=106
            )

        self.assertIsNone(
            self.harness.connection.execute(
                "SELECT 1 FROM tasks WHERE task_id = 'must-not-dispatch'"
            ).fetchone()
        )
        self.assertEqual(
            self.harness.connection.execute(
                "SELECT COUNT(*) AS total FROM claim_deadline_incidents"
            ).fetchone()["total"],
            1,
        )
        gate = self.harness.coordinator_view(now=106)[
            "pending_deadline_mutations"
        ]
        self.assertEqual(gate[0]["pending_components"], ["micro", "macro"])

    def test_two_dispatched_attempts_after_expiry_create_one_claim_miss(self) -> None:
        self.harness.start_task("project", "first", "R-001", 5, now=100)
        self.harness.start_task("project", "second", "R-001", 5, now=101)

        first = self.harness.complete_task(
            "project", "first", "late first completion", now=106
        )
        second = self.harness.complete_task(
            "project", "second", "late second completion", now=107
        )
        counts = self.harness.connection.execute(
            """
            SELECT
              (SELECT COUNT(*) FROM claim_deadline_incidents) AS claim_misses,
              (SELECT COUNT(*) FROM incidents WHERE kind = 'deadline_miss') AS legacy_misses,
              (SELECT COALESCE(SUM(units), 0) FROM incidents) AS units,
              (SELECT COUNT(*) FROM tasks WHERE attempt_terminal_at IS NOT NULL) AS terminals
            """
        ).fetchone()

        self.assertTrue(first["deadline_missed"])
        self.assertTrue(second["deadline_missed"])
        self.assertEqual(dict(counts), {
            "claim_misses": 1,
            "legacy_misses": 1,
            "units": 1,
            "terminals": 2,
        })

    def test_post_acceptance_attempt_remains_visible(self) -> None:
        self.establish_accepted_claim()

        self.harness.start_task(
            "project", "post-acceptance-check", "R-001", 100,
            phase="closure", now=5
        )
        view = self.harness.coordinator_view(now=5)

        self.assertEqual(
            [task["task_id"] for task in view["tasks"]],
            ["post-acceptance-check"],
        )
        self.assertEqual(view["tasks"][0]["state"], "running")

    def test_acceptance_rejects_a_live_sibling_until_it_is_terminal(self) -> None:
        self.harness.start_task("project", "explore", "R-001", 100, now=0)
        self.harness.complete_task("project", "explore", "strategy proved", now=1)
        self.harness.transition_claim_to_closure(
            "project",
            "R-001",
            "explore",
            "The owner route finishes.",
            "Observe the owner route.",
            "One closure proof remains.",
            now=2,
        )
        self.harness.start_task(
            "project", "winner", "R-001", 100, phase="closure", now=3
        )
        with self.assertRaisesRegex(DeadlineError, "already has a live attempt"):
            self.harness.start_task(
                "project", "live-sibling", "R-001", 100,
                phase="closure", now=3,
            )
        self.harness.connection.execute(
            """
            INSERT INTO tasks (
                lineage_id, task_id, claim_id, estimate_seconds,
                started_at, deadline_at, phase_at_dispatch,
                phase_sequence_at_dispatch, closure_gap_id,
                closure_gap_revision
            )
            SELECT lineage_id, 'live-sibling', claim_id, estimate_seconds,
                   started_at, deadline_at, phase_at_dispatch,
                   phase_sequence_at_dispatch, closure_gap_id,
                   closure_gap_revision
            FROM tasks WHERE lineage_id = 'project' AND task_id = 'winner'
            """
        )
        self.harness.connection.commit()
        self.harness.complete_task("project", "winner", "closure proved", now=4)

        with self.assertRaisesRegex(DeadlineError, "live-sibling"):
            self.harness.accept_claim(
                "project", "R-001", "winner", "accepted proof", now=5
            )

        self.harness.abandon_attempt(
            "project", "live-sibling", "winner supplied the proof", now=6
        )
        accepted = self.harness.accept_claim(
            "project", "R-001", "winner", "accepted proof", now=7
        )
        self.assertTrue(accepted["recorded"])

    def functional_projection_workspace(self) -> tuple[Path, str]:
        self.harness.close()
        workspace = Path(self.temporary.name) / "functional-workspace"
        environment = workspace / ".de67"
        environment.mkdir(parents=True)
        functional_specification = (
            "# Functional Specification\n\n"
            "The owner route must retain this functional requirement.\n"
        )
        fs_path = environment / "FS.md"
        fs_path.write_text(functional_specification, encoding="utf-8")
        (environment / "work-ledger.md").write_text(
            "# Work ledger\n\n"
            "- [x] R-001 — Owner assignment remains active.\n"
            "  - Assignment: closure owner\n"
            "  - Functional reference: owner route\n",
            encoding="utf-8",
        )
        self.state_path = environment / "state" / "deadlines.sqlite3"
        self.state_path.parent.mkdir()
        self.harness = DeadlineHarness(self.state_path)
        return workspace, functional_specification

    def test_projection_failure_does_not_partially_accept_claim(self) -> None:
        workspace, _ = self.functional_projection_workspace()
        self.harness.start_task("project", "explore", "R-001", 100, now=0)
        self.harness.complete_task("project", "explore", "route proved", now=1)
        self.harness.transition_claim_to_closure(
            "project", "R-001", "explore", "Close it.", "Run it.",
            "One proof remains.", now=2,
        )
        self.harness.start_task("project", "closure", "R-001", 100, phase="closure", now=3)
        self.harness.complete_task("project", "closure", "proof complete", now=4)
        (workspace / ".de67/work-ledger.md").write_text("# Ledger\n")
        with self.assertRaisesRegex(DeadlineError, "lacks a work-ledger projection"):
            self.harness.accept_claim("project", "R-001", "closure", "accepted proof", now=5)
        self.assertIsNone(self.harness.connection.execute(
            "SELECT 1 FROM claim_acceptances WHERE claim_id = 'R-001'"
        ).fetchone())
        self.assertIsNone(self.harness.connection.execute(
            "SELECT closed_at FROM closure_gaps WHERE claim_id = 'R-001'"
        ).fetchone()["closed_at"])

    def test_functional_ledger_reopens_and_removes_durable_acceptance(self) -> None:
        workspace, functional_specification = self.functional_projection_workspace()
        self.establish_accepted_claim()

        self.harness.start_task(
            "project", "closure-check", "R-001", 100, phase="closure", now=6
        )
        self.harness.report_worker_finding(
            "project",
            "closure-check",
            "unexpected",
            "The owner route returns the finished outcome premise is contradicted.",
            now=7,
        )
        reopened = self.harness.reopen_claim_exploration(
            "project",
            "R-001",
            "closure-check",
            "The owner route returns the finished outcome",
            now=8,
        )

        ledger = (workspace / ".de67" / "work-ledger.md").read_text(encoding="utf-8")
        self.assertEqual(reopened["dfs_status_synchronized"], ["R-001"])
        self.assertIn("- [ ] R-001 — Owner assignment remains active.", ledger)
        self.assertIn("  - Assignment: closure owner", ledger)
        self.assertIn("  - Functional reference: owner route", ledger)
        self.assertNotIn("Durable acceptance:", ledger)
        self.assertEqual(
            (workspace / ".de67" / "FS.md").read_text(encoding="utf-8"),
            functional_specification,
        )

    def test_functional_ledger_projects_latest_acceptance_from_bound_lineage(self) -> None:
        workspace, _functional_specification = self.functional_projection_workspace()
        self.establish_accepted_claim()
        self.harness.connection.execute("PRAGMA foreign_keys = OFF")
        self.harness.connection.execute(
            """
            INSERT INTO claim_acceptances (
                lineage_id, claim_id, acceptance_number, task_id,
                closure_sequence, accepted_at, evidence
            ) VALUES ('other', 'R-001', 2, 'other-closure', 1, 6, 'other evidence')
            """
        )
        self.harness.connection.commit()
        self.harness.connection.execute("PRAGMA foreign_keys = ON")
        ledger_path = workspace / ".de67" / "work-ledger.md"
        ledger_path.write_text(
            "\n".join(
                line for line in ledger_path.read_text(encoding="utf-8").splitlines()
                if "Durable acceptance:" not in line
            ) + "\n",
            encoding="utf-8",
        )

        changed = self.harness.synchronize_delivery_statuses()

        ledger = ledger_path.read_text(encoding="utf-8")
        self.assertEqual(changed, ("R-001",))
        self.assertIn("Durable acceptance: #1 via `closure`", ledger)
        self.assertNotIn("Durable acceptance: #2 via `other-closure`", ledger)

    def test_failed_late_acceptance_preserves_the_new_deadline_miss(self) -> None:
        self.harness.start_task("project", "explore", "R-LATE", 5, now=0)
        self.harness.complete_task("project", "explore", "strategy proved", now=1)
        self.harness.transition_claim_to_closure(
            "project",
            "R-LATE",
            "explore",
            "The owner route finishes.",
            "Observe the owner route.",
            "One closure proof remains.",
            now=2,
        )
        self.harness.start_task(
            "project", "winner", "R-LATE", 5, phase="closure", now=3
        )
        self.harness.connection.execute(
            """
            INSERT INTO tasks (
                lineage_id, task_id, claim_id, estimate_seconds,
                started_at, deadline_at, phase_at_dispatch,
                phase_sequence_at_dispatch, closure_gap_id,
                closure_gap_revision
            )
            SELECT lineage_id, 'live-sibling', claim_id, estimate_seconds,
                   started_at, deadline_at, phase_at_dispatch,
                   phase_sequence_at_dispatch, closure_gap_id,
                   closure_gap_revision
            FROM tasks WHERE lineage_id = 'project' AND task_id = 'winner'
            """
        )
        self.harness.connection.commit()
        self.harness.complete_task("project", "winner", "closure proved", now=4)

        with self.assertRaisesRegex(DeadlineError, "live-sibling"):
            self.harness.accept_claim(
                "project", "R-LATE", "winner", "accepted proof", now=6
            )

        incident = self.harness.connection.execute(
            """
            SELECT * FROM claim_deadline_incidents
            WHERE lineage_id = 'project' AND claim_id = 'R-LATE'
            """
        ).fetchone()
        acceptance = self.harness.connection.execute(
            """
            SELECT 1 FROM claim_acceptances
            WHERE lineage_id = 'project' AND claim_id = 'R-LATE'
            """
        ).fetchone()
        self.assertIsNotNone(incident)
        self.assertEqual(incident["source_task_id"], "winner")
        self.assertIsNone(acceptance)

    def test_post_acceptance_finding_stays_visible(self) -> None:
        self.establish_accepted_claim()
        self.harness.start_task(
            "project", "late-sibling", "R-001", 100,
            phase="closure", now=6
        )
        self.harness.report_worker_finding(
            "project",
            "late-sibling",
            "unexpected",
            "A post-acceptance owner observation contradicted the route.",
            now=7,
        )

        view = self.harness.coordinator_view(now=8)
        self.assertEqual(
            [task["task_id"] for task in view["tasks"]],
            ["late-sibling"],
        )
        self.assertEqual(view["tasks"][0]["state"], "worker_finding")

    def test_reopened_claim_stays_visible_until_new_closure_acceptance(self) -> None:
        self.establish_accepted_claim()
        self.harness.start_task(
            "project", "closure-check", "R-001", 100,
            phase="closure", now=6
        )
        self.harness.report_worker_finding(
            "project",
            "closure-check",
            "unexpected",
            "The owner route returns the finished outcome premise is contradicted.",
            now=7,
        )
        self.harness.reopen_claim_exploration(
            "project",
            "R-001",
            "closure-check",
            "The owner route returns the finished outcome",
            now=8,
        )
        self.harness.start_task("project", "reexplore", "R-001", 100, now=9)
        self.harness.complete_task(
            "project", "reexplore", "replacement strategy proved", now=10
        )

        reopened = self.harness.coordinator_view(now=10)[
            "reopened_unaccepted_claims"
        ]
        self.assertEqual([item["claim_id"] for item in reopened], ["R-001"])
        self.assertEqual(reopened[0]["phase"], "exploration")
        details = self.harness.claim_invalidation_details("project", "R-001")
        self.assertEqual(details["accepted_task_id"], "closure")
        self.assertEqual(details["trigger"]["kind"], "closure_reopen")
        self.assertEqual(details["trigger"]["task_id"], "closure-check")
        self.assertIn(
            "owner route returns",
            details["trigger"]["contradicted_premise"],
        )
        self.assertEqual(
            self.harness.coordinator_view(now=10)[
                "invalidated_unaccepted_claims"
            ][0]["trigger_kind"],
            "closure_reopen",
        )
        self.harness.transition_claim_to_closure(
            "project",
            "R-001",
            "reexplore",
            "The repaired owner route returns the finished outcome.",
            "Run the repaired owner route and inspect its artifact.",
            "Only repaired closure remains.",
            now=11,
        )
        self.assertEqual(
            [
                item["claim_id"]
                for item in self.harness.coordinator_view(now=11)[
                    "reopened_unaccepted_claims"
                ]
            ],
            ["R-001"],
        )
        self.harness.start_task(
            "project", "reclosure", "R-001", 100,
            phase="closure", now=12
        )
        self.harness.complete_task(
            "project", "reclosure", "repaired closure proved", now=13
        )
        self.harness.accept_claim(
            "project", "R-001", "reclosure", "repaired evidence accepted",
            now=14
        )

        self.assertEqual(
            self.harness.coordinator_view(now=14)["reopened_unaccepted_claims"],
            [],
        )

    def test_closure_reopen_and_acceptance_are_bound_to_phase_epochs(self) -> None:
        self.harness.start_task("project", "explore-1", "R-001", 100, now=0)
        self.harness.complete_task(
            "project", "explore-1", "first exploration proof", now=1
        )
        self.harness.transition_claim_to_closure(
            "project",
            "R-001",
            "explore-1",
            "The solver returns feasible.",
            "Run the exact solver route.",
            "Only the exact solver route remains.",
            now=2,
        )
        self.harness.start_task(
            "project", "closure-1", "R-001", 100, phase="closure", now=3
        )
        self.harness.complete_task(
            "project", "closure-1", "first closure proof", now=4
        )
        self.harness.accept_claim(
            "project", "R-001", "closure-1", "owner accepted closure", now=5
        )
        self.harness.start_task(
            "project", "closure-check", "R-001", 100, phase="closure", now=6
        )
        self.harness.report_worker_finding(
            "project",
            "closure-check",
            "unexpected",
            "The solver returns feasible premise is false on the retained fixture.",
            now=7,
        )

        with self.assertRaisesRegex(DeadlineError, "active closure contract"):
            self.harness.reopen_claim_exploration(
                "project", "R-001", "closure-check", "retained fixture", now=8
            )
        reopened = self.harness.reopen_claim_exploration(
            "project",
            "R-001",
            "closure-check",
            "The solver returns feasible",
            now=8,
        )
        self.assertEqual(reopened["phase"], "exploration")
        self.assertFalse(
            self.harness.status_task("project", "closure-1", now=8)[
                "completion_accepted"
            ]
        )

        self.harness.start_task("project", "explore-2", "R-001", 100, now=9)
        self.harness.complete_task(
            "project", "explore-2", "second exploration proof", now=10
        )
        with self.assertRaisesRegex(DeadlineError, "current exploration epoch"):
            self.harness.transition_claim_to_closure(
                "project",
                "R-001",
                "explore-1",
                "Stale outcome.",
                "Stale evidence route.",
                "Stale remaining gap.",
                now=11,
            )
        self.harness.transition_claim_to_closure(
            "project",
            "R-001",
            "explore-2",
            "Prove the repaired solver outcome.",
            "Run the repaired solver route.",
            "Only the repaired solver route remains.",
            now=11,
        )
        with self.assertRaisesRegex(DeadlineError, "active closure epoch"):
            self.harness.accept_claim(
                "project", "R-001", "closure-1", "stale acceptance", now=12
            )
        self.harness.start_task(
            "project", "closure-2", "R-001", 100, phase="closure", now=12
        )
        self.harness.complete_task(
            "project", "closure-2", "second closure proof", now=13
        )
        accepted = self.harness.accept_claim(
            "project", "R-001", "closure-2", "repaired closure accepted", now=14
        )

        self.assertEqual(accepted["closure_sequence"], 4)
        rows = self.harness.connection.execute(
            """
            SELECT task_id, closure_sequence, invalidated_at
            FROM claim_acceptances ORDER BY acceptance_number
            """
        ).fetchall()
        self.assertEqual(
            [(row["task_id"], row["closure_sequence"]) for row in rows],
            [("closure-1", 2), ("closure-2", 4)],
        )
        self.assertIsNotNone(rows[0]["invalidated_at"])
        self.assertIsNone(rows[1]["invalidated_at"])

    def test_breach_invalidates_a_valid_closure_acceptance(self) -> None:
        self.harness.start_task("project", "explore", "R-001", 100, now=0)
        self.harness.complete_task("project", "explore", "explored", now=1)
        self.harness.transition_claim_to_closure(
            "project",
            "R-001",
            "explore",
            "Prove closure.",
            "Run closure.",
            "Closure remains.",
            now=2,
        )
        self.harness.start_task(
            "project", "closure", "R-001", 100, phase="closure", now=3
        )
        self.harness.complete_task("project", "closure", "closed", now=4)
        self.harness.accept_claim(
            "project", "R-001", "closure", "accepted proof", now=5
        )

        breached = self.harness.record_integrity_breach(
            "project", "closure", "closure evidence was fabricated", now=6
        )
        acceptance = self.harness.connection.execute(
            "SELECT * FROM claim_acceptances WHERE task_id = 'closure'"
        ).fetchone()

        self.assertFalse(breached["status"]["completion_accepted"])
        self.assertIsNotNone(acceptance["invalidated_at"])
        self.assertEqual(
            acceptance["invalidation_reason"], "closure evidence was fabricated"
        )
        details = self.harness.claim_invalidation_details("project", "R-001")
        self.assertEqual(details["acceptance_evidence"], "accepted proof")
        self.assertEqual(details["trigger"]["kind"], "integrity_breach")
        self.assertEqual(details["trigger"]["task_id"], "closure")
        self.assertEqual(
            [task["task_id"] for task in self.harness.list_tasks(now=6)["tasks"]],
            ["closure"],
        )

    def test_abandoned_attempt_cannot_later_report_a_finding(self) -> None:
        self.harness.start_task("project", "attempt", "R-001", 100, now=0)
        abandoned = self.harness.abandon_attempt(
            "project", "attempt", "worker was replaced", now=1
        )

        with self.assertRaisesRegex(DeadlineError, "terminal outcome"):
            self.harness.report_worker_finding(
                "project", "attempt", "blocker", "late finding", now=2
            )
        with self.assertRaisesRegex(DeadlineError, "terminal outcome"):
            self.harness.complete_task(
                "project", "attempt", "late completion", now=2
            )

        self.assertEqual(abandoned["state"], "abandoned")
        self.assertFalse(abandoned["attempt_completed"])
        self.assertEqual(abandoned["attempt_terminal_kind"], "abandoned")
        self.assertEqual(
            abandoned["random_mutation"]["completed_terminal_windows"], 1
        )

    def test_retired_model_probe_and_replacement_count_two_worker_windows(self) -> None:
        self.harness.start_task("project", "model-probe", "R-001", 100, now=0)
        retired = self.harness.abandon_attempt(
            "project", "model-probe", "worker model could not be verified", now=1
        )
        self.harness.start_task("project", "replacement", "R-001", 100, now=2)
        completed = self.harness.complete_task(
            "project", "replacement", "bounded worker result accepted", now=3
        )

        self.assertEqual(
            retired["random_mutation"]["completed_terminal_windows"], 1
        )
        self.assertEqual(
            completed["random_mutation"]["completed_terminal_windows"], 2
        )
        repeated = self.harness.complete_task(
            "project", "replacement", "duplicate callback", now=4
        )
        self.assertEqual(
            repeated["random_mutation"]["completed_terminal_windows"], 2
        )

    def test_random_cadence_carries_terminal_overflow_to_next_boundary(self) -> None:
        with patch(
            "deadline_harness.secrets.randbelow", side_effect=[0, 0, 0, 1]
        ):
            for number in range(1, 41):
                self.harness.start_task(
                    "project", f"window-{number}", f"R-{number:03d}", 100, now=0
                )
            for number in range(1, 23):
                self.harness.complete_task(
                    "project", f"window-{number}", "green", now=number
                )
            first = self.harness.list_tasks(now=13)["random_mutation"]
            resolved = self.harness.resolve_random_mutation(
                "project", 1, "ordinary guarded mutation", now=13
            )
            for number in range(23, 41):
                self.harness.complete_task(
                    "project", f"window-{number}", "green", now=number
                )
            second = self.harness.list_tasks(now=21)["random_mutation"]

        self.assertEqual(first["completed_terminal_windows"], 22)
        self.assertEqual(first["due_after_terminal_windows"], 20)
        self.assertEqual(
            resolved["random_mutation"]["due_after_terminal_windows"], 40
        )
        self.assertFalse(resolved["random_mutation"]["due"])
        self.assertEqual(second["completed_terminal_windows"], 40)
        self.assertEqual(second["due_after_terminal_windows"], 40)
        self.assertTrue(second["due"])
        self.assertEqual(second["due_task_id"], "window-40")

    def test_status_repairs_missed_random_due_marker_while_worker_is_live(self) -> None:
        with patch("deadline_harness.secrets.randbelow", side_effect=[0, 0]):
            for number in range(1, 22):
                self.harness.start_task(
                    "project", f"window-{number}", f"R-{number:03d}", 100, now=0
                )
            for number in range(1, 21):
                self.harness.complete_task(
                    "project", f"window-{number}", "green", now=number
                )

        self.harness.connection.execute(
            """
            UPDATE random_mutation_cycles
            SET due_task_id = NULL
            WHERE lineage_id = 'project' AND cycle_number = 1
            """
        )
        self.harness.connection.commit()

        view = self.harness.coordinator_view(now=21)
        cycle = self.harness.connection.execute(
            """
            SELECT due_task_id FROM random_mutation_cycles
            WHERE lineage_id = 'project' AND cycle_number = 1
            """
        ).fetchone()

        self.assertIn(
            "window-21",
            [task["task_id"] for task in view["tasks"] if task["state"] == "running"],
        )
        self.assertEqual(view["random_mutation"]["due_task_id"], "window-20")
        self.assertEqual(cycle["due_task_id"], "window-20")

    def test_interval_thirty_dfs_requires_ordinary_and_universal_before_restart(self) -> None:
        self.write_sol_ultra_capability()
        with patch(
            "deadline_harness.secrets.randbelow", side_effect=[10, 1, 0, 0]
        ), patch("deadline_harness.time.time", return_value=12345):
            for number in range(1, 31):
                self.harness.start_task(
                    "project", f"window-{number}", f"R-{number:03d}", 100, now=0
                )
            for number in range(1, 31):
                self.harness.complete_task(
                    "project", f"window-{number}", "green", now=number
                )
            due = self.harness.list_tasks(now=31)["random_mutation"]
            ordinary = self.harness.resolve_random_mutation(
                "project",
                1,
                "ordinary FS mutation guarded",
                component="ordinary",
                now=31,
            )
            receipt_id = self.record_universal_receipt()
            universal = self.harness.resolve_random_mutation(
                "project",
                1,
                "universal mutation guarded across all mutable surfaces",
                component="universal",
                receipt_id=receipt_id,
                now=32,
            )
            with self.assertRaisesRegex(DeadlineError, "already been consumed"):
                self.harness.resolve_random_mutation(
                    "project",
                    1,
                    "universal mutation guarded across all mutable surfaces",
                    component="universal",
                    receipt_id=receipt_id,
                    now=33,
                )

        self.assertEqual(due["interval_windows"], 30)
        self.assertEqual(due["selected_lane"], "DFS.md")
        self.assertTrue(due["universal_signature_seen"])
        self.assertTrue(due["universal_required"])
        self.assertEqual(due["universal_capability_status"], "available")
        self.assertEqual(due["universal_capability_checked_at"], 12345)
        self.assertEqual(due["pending_components"], ["ordinary", "universal"])
        self.assertEqual(ordinary["random_mutation"]["cycle_number"], 1)
        self.assertEqual(ordinary["random_mutation"]["pending_components"], ["universal"])
        self.assertIsNone(ordinary["coordinator_restart"])
        self.assertTrue(universal["coordinator_restart"]["pending"])
        self.assertEqual(universal["universal_receipt_id"], receipt_id)
        self.assertEqual(
            self.harness.connection.execute(
                "SELECT COUNT(*) AS total FROM coordinator_restart_requests"
            ).fetchone()["total"],
            1,
        )

    def test_rare_trigger_without_due_time_capability_is_visible_and_nonblocking(self) -> None:
        with patch(
            "deadline_harness.secrets.randbelow", side_effect=[10, 1, 0, 0]
        ), patch("deadline_harness.time.time", return_value=23456):
            for number in range(1, 31):
                self.harness.start_task(
                    "project", f"window-{number}", f"R-{number:03d}", 100, now=0
                )
            for number in range(1, 31):
                self.harness.complete_task(
                    "project", f"window-{number}", "green", now=number
                )
            due = self.harness.coordinator_view(now=31)["random_mutation"]
            self.write_sol_ultra_capability()
            frozen = self.harness.coordinator_view(now=31)["random_mutation"]
            resolved = self.harness.resolve_random_mutation(
                "project", 1, "ordinary FS mutation guarded",
                component="ordinary", now=31,
            )

        self.assertTrue(due["universal_signature_seen"])
        self.assertFalse(due["universal_required"])
        self.assertEqual(due["universal_capability_status"], "unavailable")
        self.assertEqual(due["universal_capability_checked_at"], 23456)
        self.assertIn("workspace roster is missing", due["universal_capability_reason"])
        self.assertEqual(due["pending_components"], ["ordinary"])
        self.assertFalse(frozen["universal_required"])
        self.assertEqual(frozen["universal_capability_status"], "unavailable")
        self.assertEqual(
            frozen["universal_capability_checked_at"],
            due["universal_capability_checked_at"],
        )
        self.assertTrue(resolved["coordinator_restart"]["pending"])
        cycle = self.harness.connection.execute(
            """
            SELECT * FROM random_mutation_cycles
            WHERE lineage_id = 'project' AND cycle_number = 1
            """
        ).fetchone()
        self.assertIsNotNone(cycle["resolution_evidence"])
        self.assertIsNone(cycle["universal_resolution_evidence"])
        self.assertEqual(cycle["universal_capability_status"], "unavailable")

    def test_legacy_ordinary_only_rare_cycle_cannot_become_zero_action_due_gate(self) -> None:
        with patch(
            "deadline_harness.secrets.randbelow", side_effect=[10, 1, 0, 0]
        ):
            for number in range(1, 31):
                self.harness.start_task(
                    "project", f"window-{number}", f"R-{number:03d}", 100, now=0
                )
            for number in range(1, 31):
                self.harness.complete_task(
                    "project", f"window-{number}", "green", now=number
                )
        self.harness.connection.execute(
            "DROP TRIGGER random_cycle_capability_snapshot_is_immutable"
        )
        self.harness.connection.execute(
            """
            UPDATE random_mutation_cycles
            SET ordinary_resolution_evidence = 'legacy ordinary proof',
                universal_required = 1,
                universal_capability_status = NULL,
                universal_capability_reason = NULL,
                universal_capability_checked_at = NULL,
                universal_capability_roster_digest = NULL,
                resolution_evidence = NULL,
                restart_generation = NULL
            WHERE lineage_id = 'project' AND cycle_number = 1
            """
        )
        self.harness.connection.commit()

        with patch("deadline_harness.time.time", return_value=34567):
            view = self.harness.coordinator_view(now=31)

        cycle = self.harness.connection.execute(
            """
            SELECT * FROM random_mutation_cycles
            WHERE lineage_id = 'project' AND cycle_number = 1
            """
        ).fetchone()
        self.assertIsNone(view["random_mutation"])
        self.assertTrue(view["coordinator_restart"]["pending"])
        self.assertIsNotNone(cycle["resolution_evidence"])
        self.assertFalse(cycle["universal_required"])
        self.assertEqual(cycle["universal_capability_status"], "unavailable")
        self.assertEqual(cycle["universal_capability_checked_at"], 34567)

    def test_v1_diagnosis_does_not_grandfather_unproved_mutation_components(self) -> None:
        self.harness.close()
        legacy = Path(self.temporary.name) / "diagnosed-v1.sqlite"
        connection = sqlite3.connect(legacy)
        try:
            connection.executescript(
                """
                CREATE TABLE tasks (
                    lineage_id TEXT NOT NULL,
                    task_id TEXT NOT NULL,
                    claim_id TEXT NOT NULL,
                    estimate_seconds REAL NOT NULL,
                    started_at REAL NOT NULL,
                    deadline_at REAL NOT NULL,
                    completed_at REAL,
                    completion_evidence TEXT,
                    integrity_breached_at REAL,
                    integrity_reason TEXT,
                    PRIMARY KEY (lineage_id, task_id)
                );
                CREATE TABLE incidents (
                    incident_id INTEGER PRIMARY KEY AUTOINCREMENT,
                    lineage_id TEXT NOT NULL,
                    task_id TEXT NOT NULL,
                    kind TEXT NOT NULL,
                    recorded_at REAL NOT NULL,
                    reason TEXT,
                    short_verdict TEXT,
                    long_detail TEXT,
                    reviewed_at REAL,
                    units INTEGER NOT NULL,
                    cumulative_before INTEGER NOT NULL,
                    cumulative_after INTEGER NOT NULL,
                    cadence_threshold INTEGER,
                    UNIQUE (lineage_id, task_id, kind)
                );
                CREATE TABLE worker_findings (
                    lineage_id TEXT NOT NULL,
                    task_id TEXT NOT NULL,
                    kind TEXT NOT NULL,
                    reported_at REAL NOT NULL,
                    evidence TEXT NOT NULL,
                    PRIMARY KEY (lineage_id, task_id)
                );
                CREATE TABLE lineage_binding (
                    singleton INTEGER PRIMARY KEY,
                    lineage_id TEXT NOT NULL
                );
                INSERT INTO lineage_binding VALUES (1, 'project');
                INSERT INTO tasks VALUES
                  ('project', 'legacy', 'R-001', 10, 0, 10, NULL, NULL, NULL, NULL);
                INSERT INTO incidents (
                    lineage_id, task_id, kind, recorded_at, reason,
                    short_verdict, long_detail, reviewed_at,
                    units, cumulative_before, cumulative_after, cadence_threshold
                ) VALUES (
                    'project', 'legacy', 'deadline_miss', 11, NULL,
                    'estimate unsound', 'Legacy diagnosis only.', 12,
                    1, 0, 1, NULL
                );
                """
            )
            connection.commit()
        finally:
            connection.close()

        with patch(
            "deadline_harness.secrets.randbelow", side_effect=[10, 0]
        ), DeadlineHarness(legacy) as migrated:
            view = migrated.coordinator_view(now=20)
            self.assertEqual(view["pending_incident_reviews"], [])
            self.assertEqual(
                view["pending_deadline_mutations"],
                [
                {
                    "claim_id": "R-001",
                    "deadline_generation": 1,
                    "source_task_id": "legacy",
                        "recorded_at": 11,
                        "reviewed": True,
                        "pending_components": ["micro", "macro"],
                        "restart_generation": None,
                    }
                ],
            )
            self.assertEqual(
                migrated.connection.execute(
                    "SELECT COUNT(*) AS total FROM deadline_mutation_components"
                ).fetchone()["total"],
                0,
            )
        self.harness = DeadlineHarness(self.state_path)

    def test_divergent_v1_clocks_block_and_require_explicit_reconciliation(self) -> None:
        self.harness.close()
        legacy = Path(self.temporary.name) / "divergent-v1.sqlite"
        connection = sqlite3.connect(legacy)
        try:
            connection.executescript(
                """
                CREATE TABLE tasks (
                    lineage_id TEXT NOT NULL,
                    task_id TEXT NOT NULL,
                    claim_id TEXT NOT NULL,
                    estimate_seconds REAL NOT NULL,
                    started_at REAL NOT NULL,
                    deadline_at REAL NOT NULL,
                    completed_at REAL,
                    completion_evidence TEXT,
                    integrity_breached_at REAL,
                    integrity_reason TEXT,
                    PRIMARY KEY (lineage_id, task_id)
                );
                CREATE TABLE incidents (
                    incident_id INTEGER PRIMARY KEY AUTOINCREMENT,
                    lineage_id TEXT NOT NULL,
                    task_id TEXT NOT NULL,
                    kind TEXT NOT NULL,
                    recorded_at REAL NOT NULL,
                    reason TEXT,
                    units INTEGER NOT NULL,
                    cumulative_before INTEGER NOT NULL,
                    cumulative_after INTEGER NOT NULL,
                    cadence_threshold INTEGER,
                    UNIQUE (lineage_id, task_id, kind)
                );
                CREATE TABLE worker_findings (
                    lineage_id TEXT NOT NULL,
                    task_id TEXT NOT NULL,
                    kind TEXT NOT NULL,
                    reported_at REAL NOT NULL,
                    evidence TEXT NOT NULL,
                    PRIMARY KEY (lineage_id, task_id)
                );
                CREATE TABLE lineage_binding (
                    singleton INTEGER PRIMARY KEY,
                    lineage_id TEXT NOT NULL
                );
                INSERT INTO lineage_binding VALUES (1, 'project');
                INSERT INTO tasks VALUES
                  ('project', 'legacy-a', 'R-001', 10, 0, 10, NULL, NULL, NULL, NULL),
                  ('project', 'legacy-b', 'R-001', 20, 1, 21, NULL, NULL, NULL, NULL),
                  ('project', 'legacy-c', 'R-002', 15, 2, 17, NULL, NULL, NULL, NULL),
                  ('project', 'legacy-d', 'R-002', 25, 3, 28, NULL, NULL, NULL, NULL);
                INSERT INTO incidents (
                    lineage_id, task_id, kind, recorded_at, reason, units,
                    cumulative_before, cumulative_after, cadence_threshold
                ) VALUES (
                    'project', 'legacy-a', 'deadline_miss', 11, NULL, 1, 0, 1, NULL
                );
                """
            )
            connection.commit()
        finally:
            connection.close()

        with patch(
            "deadline_harness.secrets.randbelow", side_effect=[10, 0]
        ), DeadlineHarness(legacy) as migrated:
            view = migrated.coordinator_view(now=5)
            self.assertEqual(
                view["claim_clock_migration_conflicts"],
                [
                    {
                        "claim_id": "R-001",
                        "detected_at": view["claim_clock_migration_conflicts"][0][
                            "detected_at"
                        ],
                        "legacy_clock_option_count": 2,
                    },
                    {
                        "claim_id": "R-002",
                        "detected_at": view["claim_clock_migration_conflicts"][1][
                            "detected_at"
                        ],
                        "legacy_clock_option_count": 2,
                    },
                ],
            )
            with self.assertRaisesRegex(DeadlineError, "divergent legacy"):
                migrated.start_task(
                    "project", "legacy-a", "R-001", 10, now=50
                )
            with self.assertRaisesRegex(DeadlineError, "divergent legacy"):
                migrated.start_task("project", "new", "R-001", 10, now=50)

            estimated = migrated.resolve_claim_clock_migration(
                "project",
                "R-002",
                "No authoritative legacy item clock existed; create the first one.",
                estimate_seconds=30,
                now=50,
            )
            new_attempt = migrated.start_task(
                "project", "new-r2", "R-002", 30, now=51
            )
            with self.assertRaisesRegex(DeadlineError, "legacy deadline miss"):
                migrated.resolve_claim_clock_migration(
                    "project",
                    "R-001",
                    "A new clock would detach the recorded miss.",
                    estimate_seconds=30,
                    now=52,
                )
            details = migrated.claim_clock_migration_details("project", "R-001")
            self.assertEqual(
                [item["task_id"] for item in details["legacy_clock_options"]],
                ["legacy-a", "legacy-b"],
            )
            self.assertEqual(details["required_source_task_id"], "legacy-a")
            self.assertEqual(
                details["earliest_legacy_deadline_miss"]["recorded_at"], 11
            )
            with self.assertRaisesRegex(DeadlineError, "earliest exact deadline miss"):
                migrated.resolve_claim_clock_migration(
                    "project",
                    "R-001",
                    "The later clock must not absorb the earlier miss.",
                    source_task_id="legacy-b",
                    now=52,
                )
            sourced = migrated.resolve_claim_clock_migration(
                "project",
                "R-001",
                "Adopt the exact legacy clock that owns the recorded miss.",
                source_task_id="legacy-a",
                now=52,
            )

            self.assertEqual(
                (estimated["started_at"], estimated["deadline_at"]), (50, 80)
            )
            self.assertEqual(new_attempt["attempt_dispatched_at"], 51)
            self.assertEqual(new_attempt["deadline_at"], 80)
            self.assertEqual(
                (sourced["started_at"], sourced["deadline_at"]), (0, 10)
            )
            self.assertEqual(
                migrated.connection.execute(
                    "SELECT COUNT(*) AS total FROM claim_deadline_incidents"
                ).fetchone()["total"],
                1,
            )
            originals = migrated.connection.execute(
                """
                SELECT task_id, estimate_seconds, started_at, deadline_at
                FROM tasks WHERE task_id LIKE 'legacy-%' ORDER BY task_id
                """
            ).fetchall()
            self.assertEqual(
                [tuple(row) for row in originals],
                [
                    ("legacy-a", 10, 0, 10),
                    ("legacy-b", 20, 1, 21),
                    ("legacy-c", 15, 2, 17),
                    ("legacy-d", 25, 3, 28),
                ],
            )

        with patch(
            "deadline_harness.secrets.randbelow",
            side_effect=AssertionError("persisted v2 state must not redraw"),
        ), DeadlineHarness(legacy) as reopened:
            self.assertEqual(
                reopened.coordinator_view(now=53)[
                    "claim_clock_migration_conflicts"
                ],
                [],
            )
        self.harness = DeadlineHarness(self.state_path)

    def test_clock_migration_cli_starts_detached_claim_watcher(self) -> None:
        output = io.StringIO()
        resolved = {
            "lineage_id": "project",
            "claim_id": "R-001",
            "resolution_kind": "new_item_clock",
            "deadline_at": 100,
        }
        with patch.object(
            DeadlineHarness,
            "resolve_claim_clock_migration",
            return_value=resolved,
        ), patch("deadline_harness.spawn_watcher") as spawn, redirect_stdout(output):
            exit_code = main(
                [
                    "resolve-clock-migration",
                    "--state",
                    str(self.state_path),
                    "--lineage",
                    "project",
                    "--claim",
                    "R-001",
                    "--reason",
                    "Create the explicit item clock.",
                    "--estimate-seconds",
                    "30",
                ]
            )

        self.assertEqual(exit_code, 0)
        spawn.assert_called_once_with(str(self.state_path), "project", "R-001")
        self.assertEqual(json.loads(output.getvalue())["resolution_kind"], "new_item_clock")

    def test_clock_migration_details_cli_keeps_exact_clock_values(self) -> None:
        output = io.StringIO()
        details = {
            "lineage_id": "project",
            "claim_id": "R-001",
            "legacy_clock_options": [
                {
                    "task_id": "legacy-a",
                    "estimate_seconds": 10,
                    "started_at": 0,
                    "deadline_at": 10,
                }
            ],
            "required_source_task_id": "legacy-a",
        }
        with patch.object(
            DeadlineHarness,
            "claim_clock_migration_details",
            return_value=details,
        ), redirect_stdout(output):
            exit_code = main(
                [
                    "clock-migration-details",
                    "--state",
                    str(self.state_path),
                    "--lineage",
                    "project",
                    "--claim",
                    "R-001",
                ]
            )

        self.assertEqual(exit_code, 0)
        self.assertEqual(json.loads(output.getvalue()), details)

    def test_named_closure_gaps_decrease_to_zero_before_acceptance(self) -> None:
        self.harness.start_task("project", "explore", "R-GAPS", 100, now=0)
        self.harness.complete_task("project", "explore", "strategy known", now=1)
        self.harness.transition_claim_to_closure(
            "project",
            "R-GAPS",
            "explore",
            "Prove both controls.",
            "Run the retained route.",
            gaps=[
                ("G-A", "Prove control A.", "Run fixture A."),
                ("G-B", "Prove control B.", "Run fixture B."),
            ],
            now=2,
        )
        with self.assertRaisesRegex(DeadlineError, "name exactly one active gap"):
            self.harness.start_task(
                "project", "ambiguous", "R-GAPS", 100, phase="closure", now=3
            )
        with self.assertRaisesRegex(DeadlineError, "unknown, closed"):
            self.harness.start_task(
                "project", "missing", "R-GAPS", 100,
                phase="closure", gap_id="G-MISSING", now=3,
            )

        self.harness.start_task(
            "project", "a", "R-GAPS", 100,
            phase="closure", gap_id="G-A", now=3,
        )
        self.harness.complete_task("project", "a", "A passed", now=4)
        closed = self.harness.close_closure_gap(
            "project", "R-GAPS", "G-A", "a", "A accepted", now=5
        )
        self.assertEqual(closed["remaining_gap_ids"], ["G-B"])

        self.harness.start_task(
            "project", "b", "R-GAPS", 100,
            phase="closure", gap_id="G-B", now=6,
        )
        self.harness.complete_task("project", "b", "B passed", now=7)
        accepted = self.harness.accept_claim(
            "project", "R-GAPS", "b", "both controls accepted", now=8
        )
        self.assertEqual(accepted["closure_gap_id"], "G-B")
        self.assertEqual(
            [gap for gap in self.harness.coordinator_view(now=9)["closure_gaps"]
             if gap["status"] == "open"],
            [],
        )

    def test_terminal_attempt_can_retry_the_same_gap_revision(self) -> None:
        self.harness.start_task("project", "explore", "R-REV", 100, now=0)
        self.harness.complete_task("project", "explore", "strategy known", now=1)
        self.harness.transition_claim_to_closure(
            "project", "R-REV", "explore", "Prove it.", "Run route.",
            "One proof remains.", now=2,
        )
        self.harness.start_task(
            "project", "finding", "R-REV", 100, phase="closure", now=3
        )
        self.harness.report_worker_finding(
            "project", "finding", "unexpected", "Route lacked the owner signal.", now=4
        )
        retry = self.harness.start_task(
            "project", "finding-retry", "R-REV", 100, phase="closure", now=5
        )
        self.assertEqual(retry["closure_gap_revision"], 1)
        self.harness.abandon_attempt(
            "project", "finding-retry", "Changed route selected.", now=5.5
        )
        revised = self.harness.revise_closure_gap(
            "project", "R-REV", "G-001", "finding",
            "One proof remains.", "Run an owner-visible route.", now=5,
        )
        self.assertEqual(revised["revision"], 2)
        self.assertEqual(revised["basis_task_id"], "finding")
        self.assertEqual(
            self.harness.connection.execute(
                "SELECT COUNT(*) AS total FROM closure_gap_revisions"
            ).fetchone()["total"],
            2,
        )

        self.harness.start_task(
            "project", "completed", "R-REV", 100, phase="closure", now=6
        )
        self.harness.complete_task(
            "project", "completed", "route still incomplete", now=7
        )
        completed_retry = self.harness.start_task(
            "project", "completed-retry", "R-REV", 100,
            phase="closure", now=8,
        )
        self.assertEqual(completed_retry["closure_gap_revision"], 2)

    def test_compact_views_keep_latest_result_for_each_active_closure_gap(self) -> None:
        self.harness.start_task("project", "explore", "R-COMPACT", 100, now=0)
        self.harness.complete_task("project", "explore", "strategy known", now=1)
        self.harness.transition_claim_to_closure(
            "project",
            "R-COMPACT",
            "explore",
            "Prove both owner controls.",
            "Run each owner route.",
            gaps=[
                ("G-A", "Prove A.", "Run route A."),
                ("G-B", "Prove B.", "Run route B."),
            ],
            now=2,
        )
        self.harness.start_task(
            "project", "proof-a", "R-COMPACT", 100,
            phase="closure", gap_id="G-A", now=3,
        )
        self.harness.complete_task("project", "proof-a", "A result", now=4)
        self.harness.start_task(
            "project", "proof-b", "R-COMPACT", 100,
            phase="closure", gap_id="G-B", now=5,
        )

        listed = self.harness.list_tasks(now=6)["tasks"]
        startup = self.harness.coordinator_view(now=6)["tasks"]

        self.assertEqual(
            [(task["closure_gap_id"], task["state"]) for task in listed],
            [("G-A", "completed"), ("G-B", "running")],
        )
        self.assertEqual(startup, listed)

    def test_second_live_attempt_for_same_gap_revision_requires_abandonment(self) -> None:
        self.harness.start_task("project", "explore", "R-LIVE", 100, now=0)
        self.harness.complete_task("project", "explore", "strategy known", now=1)
        self.harness.transition_claim_to_closure(
            "project", "R-LIVE", "explore", "Prove it.", "Run it.",
            "Proof remains.", now=2,
        )
        self.harness.start_task(
            "project", "first", "R-LIVE", 100, phase="closure", now=3
        )
        with self.assertRaisesRegex(DeadlineError, "already has a live attempt"):
            self.harness.start_task(
                "project", "overlap", "R-LIVE", 100, phase="closure", now=4
            )
        self.harness.abandon_attempt("project", "first", "worker gone", now=5)
        replacement = self.harness.start_task(
            "project", "replacement", "R-LIVE", 100, phase="closure", now=6
        )
        self.assertEqual(replacement["closure_gap_revision"], 1)

    def test_abandoned_gap_attempt_can_retry_same_revision(self) -> None:
        self.harness.start_task("project", "explore", "R-ABANDON", 100, now=0)
        self.harness.complete_task("project", "explore", "strategy known", now=1)
        self.harness.transition_claim_to_closure(
            "project", "R-ABANDON", "explore", "Prove it.", "Run it.",
            "Proof remains.", now=2,
        )
        first = self.harness.start_task(
            "project", "first", "R-ABANDON", 100, phase="closure", now=3
        )
        self.harness.abandon_attempt("project", "first", "worker gone", now=4)
        retried = self.harness.start_task(
            "project", "retry", "R-ABANDON", 100, phase="closure", now=5
        )
        self.assertEqual(first["closure_gap_revision"], 1)
        self.assertEqual(retried["closure_gap_revision"], 1)

    def test_v2_gap_migration_is_additive_and_preserves_valid_acceptance(self) -> None:
        self.establish_accepted_claim("R-MIGRATE")
        self.harness.close()
        connection = sqlite3.connect(self.state_path)
        try:
            connection.executescript(
                """
                DROP TABLE closure_gap_revisions;
                DROP TABLE closure_gaps;
                UPDATE tasks SET closure_gap_id = NULL, closure_gap_revision = NULL;
                PRAGMA user_version = 2;
                """
            )
            before = connection.execute(
                "SELECT * FROM tasks WHERE task_id = 'closure'"
            ).fetchone()
            connection.commit()
        finally:
            connection.close()

        self.harness = DeadlineHarness(self.state_path)
        after = self.harness.connection.execute(
            "SELECT * FROM tasks WHERE task_id = 'closure'"
        ).fetchone()
        gap = self.harness.connection.execute(
            "SELECT * FROM closure_gaps WHERE claim_id = 'R-MIGRATE'"
        ).fetchone()
        self.assertEqual(tuple(before), tuple(after))
        self.assertEqual(gap["gap_id"], "G-001")
        self.assertEqual(gap["closed_by_task_id"], "closure")
        self.assertEqual(gap["closure_evidence"], "closure evidence accepted")
        self.assertEqual(
            self.harness.connection.execute("PRAGMA user_version").fetchone()[0], 5
        )

    def test_breach_of_closed_final_gap_appends_actionable_successor(self) -> None:
        self.establish_accepted_claim("R-BREACH")
        original = dict(
            self.harness.connection.execute(
                "SELECT * FROM closure_gaps WHERE claim_id = 'R-BREACH'"
            ).fetchone()
        )
        result = self.harness.record_integrity_breach(
            "project", "closure", "artifact checksum was fabricated", now=6
        )
        successor = result["successor_closure_gap"]
        self.assertTrue(successor["recorded"])
        original_after = dict(
            self.harness.connection.execute(
                """SELECT * FROM closure_gaps
                   WHERE claim_id = 'R-BREACH' AND gap_id = 'G-001'"""
            ).fetchone()
        )
        self.assertEqual(original, original_after)
        reopened = self.harness.connection.execute(
            """SELECT * FROM closure_gaps
               WHERE claim_id = 'R-BREACH' AND gap_id = ?""",
            (successor["gap_id"],),
        ).fetchone()
        self.assertIsNone(reopened["closed_at"])
        self.assertEqual(reopened["successor_of_gap_id"], "G-001")
        self.assertEqual(reopened["successor_of_revision"], 1)
        view = self.harness.coordinator_view(now=7)
        self.assertIn("R-BREACH", {
            claim["claim_id"] for claim in view["invalidated_unaccepted_claims"]
        })
        self.assertIn(successor["gap_id"], {
            gap["gap_id"] for gap in view["closure_gaps"]
            if gap["status"] == "open"
        })
        repeated = self.harness.record_integrity_breach(
            "project", "closure", "artifact checksum was fabricated", now=8
        )
        self.assertIsNone(repeated["successor_closure_gap"])
        self.assertEqual(
            self.harness.connection.execute(
                "SELECT COUNT(*) FROM closure_gaps WHERE claim_id = 'R-BREACH'"
            ).fetchone()[0],
            2,
        )

    def test_pre_v3_accepted_database_backfills_closure_epoch_before_trigger(self) -> None:
        self.establish_accepted_claim("R-OLD")
        self.harness.close()
        connection = sqlite3.connect(self.state_path)
        try:
            connection.executescript(
                """
                PRAGMA foreign_keys = OFF;
                DROP TRIGGER IF EXISTS claim_acceptance_core_is_immutable;
                DROP TRIGGER IF EXISTS claim_acceptance_closure_sequence_is_immutable;
                ALTER TABLE claim_acceptances RENAME TO claim_acceptances_v3;
                CREATE TABLE claim_acceptances (
                    lineage_id TEXT NOT NULL,
                    claim_id TEXT NOT NULL,
                    acceptance_number INTEGER NOT NULL CHECK (acceptance_number > 0),
                    task_id TEXT NOT NULL,
                    accepted_at REAL NOT NULL,
                    evidence TEXT NOT NULL,
                    invalidated_at REAL,
                    invalidation_reason TEXT,
                    PRIMARY KEY (lineage_id, claim_id, acceptance_number)
                );
                INSERT INTO claim_acceptances (
                    lineage_id, claim_id, acceptance_number, task_id,
                    accepted_at, evidence, invalidated_at, invalidation_reason
                )
                SELECT lineage_id, claim_id, acceptance_number, task_id,
                       accepted_at, evidence, invalidated_at, invalidation_reason
                FROM claim_acceptances_v3;
                DROP TABLE claim_acceptances_v3;
                PRAGMA user_version = 2;
                """
            )
            self.assertNotIn(
                "closure_sequence",
                {
                    row[1]
                    for row in connection.execute(
                        "PRAGMA table_info(claim_acceptances)"
                    ).fetchall()
                },
            )
            self.assertEqual(
                connection.execute(
                    "SELECT COUNT(*) FROM claim_acceptances"
                ).fetchone()[0],
                1,
            )
            connection.commit()
        finally:
            connection.close()

        self.harness = DeadlineHarness(self.state_path)
        accepted = self.harness.connection.execute(
            "SELECT * FROM claim_acceptances WHERE claim_id = 'R-OLD'"
        ).fetchone()
        self.assertEqual(accepted["closure_sequence"], 2)
        self.assertEqual(accepted["task_id"], "closure")
        self.assertIsNone(accepted["invalidated_at"])
        with self.assertRaisesRegex(sqlite3.IntegrityError, "closure epoch"):
            self.harness.connection.execute(
                """
                UPDATE claim_acceptances SET closure_sequence = 999
                WHERE claim_id = 'R-OLD'
                """
            )
        self.harness.connection.rollback()

    def test_breach_of_any_accepted_gap_proof_invalidates_multi_gap_acceptance(self) -> None:
        self.harness.start_task("project", "explore", "R-MULTI", 100, now=0)
        self.harness.complete_task("project", "explore", "strategy known", now=1)
        self.harness.transition_claim_to_closure(
            "project",
            "R-MULTI",
            "explore",
            "Prove both controls.",
            "Run both owner routes.",
            gaps=[
                ("G-A", "Prove control A.", "Run fixture A."),
                ("G-B", "Prove control B.", "Run fixture B."),
            ],
            now=2,
        )
        self.harness.start_task(
            "project", "proof-a", "R-MULTI", 100,
            phase="closure", gap_id="G-A", now=3,
        )
        self.harness.complete_task("project", "proof-a", "A passed", now=4)
        self.harness.close_closure_gap(
            "project", "R-MULTI", "G-A", "proof-a", "A accepted", now=5
        )
        self.harness.start_task(
            "project", "proof-b", "R-MULTI", 100,
            phase="closure", gap_id="G-B", now=6,
        )
        self.harness.complete_task("project", "proof-b", "B passed", now=7)
        self.harness.accept_claim(
            "project", "R-MULTI", "proof-b", "A and B accepted", now=8
        )

        breached = self.harness.record_integrity_breach(
            "project", "proof-a", "fixture A artifact was forged", now=9
        )

        acceptance = self.harness.connection.execute(
            "SELECT * FROM claim_acceptances WHERE claim_id = 'R-MULTI'"
        ).fetchone()
        self.assertIsNotNone(acceptance["invalidated_at"])
        self.assertIn("proof-a", acceptance["invalidation_reason"])
        successor = breached["successor_closure_gap"]
        self.assertEqual(successor["gap_id"], "G-A~B1")
        gaps = self.harness.connection.execute(
            """
            SELECT gap_id, closed_by_task_id, successor_of_gap_id,
                   successor_of_revision, reopen_reason
            FROM closure_gaps WHERE claim_id = 'R-MULTI' ORDER BY gap_id
            """
        ).fetchall()
        gaps_by_id = {row["gap_id"]: row for row in gaps}
        self.assertEqual(gaps_by_id["G-A"]["closed_by_task_id"], "proof-a")
        self.assertEqual(gaps_by_id["G-B"]["closed_by_task_id"], "proof-b")
        self.assertEqual(gaps_by_id["G-A~B1"]["successor_of_gap_id"], "G-A")
        self.assertEqual(gaps_by_id["G-A~B1"]["successor_of_revision"], 1)
        self.assertEqual(
            gaps_by_id["G-A~B1"]["reopen_reason"],
            "fixture A artifact was forged",
        )
        details = self.harness.claim_invalidation_details("project", "R-MULTI")
        self.assertEqual(details["accepted_task_id"], "proof-b")
        self.assertEqual(details["trigger"]["task_id"], "proof-a")
        view = self.harness.coordinator_view(now=10)
        self.assertEqual(
            view["invalidated_unaccepted_claims"][0]["trigger_task_id"],
            "proof-a",
        )

    def test_macro_receipt_is_optional_but_invalid_receipts_are_rejected(self) -> None:
        self.harness.start_task("project", "late", "R-LATE", 1, now=0)
        self.harness.start_task("project", "breach", "R-BREACH", 100, now=0)
        self.harness.expire_task("project", "late", now=2)
        self.harness.diagnose_claim_deadline(
            "project", "R-LATE", "late", "The item clock expired.", now=3
        )
        self.harness.resolve_deadline_mutation(
            "project", "R-LATE", "micro", "finite recovery", now=4
        )
        with self.assertRaisesRegex(DeadlineError, "exact incident"):
            self.harness.resolve_deadline_mutation(
                "project",
                "R-LATE",
                "macro",
                "method changed",
                receipt_id="f" * 64,
                now=5,
            )

        resolved = self.harness.resolve_deadline_mutation(
            "project",
            "R-LATE",
            "macro",
            "method changed",
            now=5,
        )
        self.assertIsNone(resolved["receipt_id"])

        self.harness.record_integrity_breach(
            "project", "breach", "forged proof", now=2
        )
        self.harness.diagnose_incident(
            "project",
            "breach",
            "integrity_breach",
            "forged",
            "The proof route had false provenance.",
            now=3,
        )
        self.harness.resolve_integrity_mutation(
            "project", "breach", "micro", "replace proof", now=4
        )
        with self.assertRaisesRegex(DeadlineError, "exact incident"):
            self.harness.resolve_integrity_mutation(
                "project",
                "breach",
                "macro",
                "method guard",
                receipt_id="f" * 64,
                now=5,
            )
        resolved = self.harness.resolve_integrity_mutation(
            "project", "breach", "macro", "method reviewed", now=5
        )
        self.assertIsNone(resolved["receipt_id"])

    def test_reviewed_deadline_macro_can_end_with_no_change_required(self) -> None:
        self.harness.start_task("project", "late", "R-LATE", 1, now=0)
        self.harness.expire_task("project", "late", now=2)
        self.harness.diagnose_claim_deadline(
            "project", "R-LATE", "late", "The item clock expired.", now=3
        )
        self.harness.resolve_deadline_mutation(
            "project", "R-LATE", "micro", "finite recovery", now=4
        )

        resolved = self.harness.resolve_deadline_mutation(
            "project",
            "R-LATE",
            "macro",
            "Independent review found the proposed rule already present.",
            no_change_required=True,
            now=5,
        )

        self.assertEqual(resolved["disposition"], "no_change_required")
        self.assertIsNone(resolved["receipt_id"])
        self.assertEqual(resolved["pending_components"], [])
        restart = resolved["coordinator_restart"]
        self.assertIsNotNone(restart)
        self.assertTrue(restart["pending"])
        self.assertEqual(
            self.harness.coordinator_view(now=5)["pending_deadline_mutations"], []
        )
        with self.assertRaisesRegex(DeadlineError, "restart generation 1 is pending"):
            self.harness.start_task("project", "retry", "R-LATE", 100, now=6)
        self.harness.claim_coordinator_restart(
            "project", restart["generation"], "fresh-coordinator", now=6
        )
        self.harness.acknowledge_coordinator_restart(
            "project", restart["generation"], "fresh-coordinator", now=6
        )
        resumed = self.harness.start_task(
            "project", "retry", "R-LATE", 100, now=7
        )
        self.assertEqual(resumed["deadline_generation"], 2)
        self.assertEqual(resumed["deadline_at"], 107)
        generation = self.harness.connection.execute(
            """
            SELECT armed_by_restart_generation
            FROM claim_deadline_generations
            WHERE lineage_id = ? AND claim_id = ? AND generation = 2
            """,
            ("project", "R-LATE"),
        ).fetchone()
        self.assertIsNotNone(generation)
        self.assertEqual(
            generation["armed_by_restart_generation"], restart["generation"]
        )

    def test_no_change_required_is_macro_only_and_cannot_consume_receipt(self) -> None:
        self.harness.start_task("project", "late", "R-LATE", 1, now=0)
        self.harness.expire_task("project", "late", now=2)
        self.harness.diagnose_claim_deadline(
            "project", "R-LATE", "late", "The item clock expired.", now=3
        )
        with self.assertRaisesRegex(DeadlineError, "only valid.*macro"):
            self.harness.resolve_deadline_mutation(
                "project",
                "R-LATE",
                "micro",
                "not applicable",
                no_change_required=True,
                now=4,
            )
        with self.assertRaisesRegex(DeadlineError, "cannot consume"):
            self.harness.resolve_deadline_mutation(
                "project",
                "R-LATE",
                "macro",
                "not applicable",
                receipt_id="f" * 64,
                no_change_required=True,
                now=4,
            )

    def test_on_time_integrity_gate_persists_and_requires_two_components(self) -> None:
        self.harness.start_task("project", "breached", "R-I", 100, now=0)
        self.harness.record_integrity_breach(
            "project", "breached", "proof was fabricated", now=1
        )
        self.assertEqual(
            self.harness.coordinator_view(now=2)["pending_integrity_mutations"],
            [
                {
                    "task_id": "breached",
                    "claim_id": "R-I",
                    "recorded_at": 1,
                    "reviewed": False,
                    "pending_components": ["micro", "macro"],
                    "restart_generation": None,
                }
            ],
        )
        with self.assertRaisesRegex(DeadlineError, "Integrity mutation is pending"):
            self.harness.start_task("project", "blocked", "R-NEXT", 100, now=2)
        self.harness.close()
        self.harness = DeadlineHarness(self.state_path)
        self.assertEqual(
            self.harness.coordinator_view(now=2)["pending_integrity_mutations"][0][
                "pending_components"
            ],
            ["micro", "macro"],
        )
        with self.assertRaisesRegex(DeadlineError, "independent diagnosis"):
            self.harness.resolve_integrity_mutation(
                "project", "breached", "micro", "recovery", now=3
            )
        diagnosed = self.harness.diagnose_incident(
            "project",
            "breached",
            "integrity_breach",
            "proof route untrusted",
            "The artifact had no independent provenance.",
            now=3,
        )
        self.assertEqual(diagnosed["pending_components"], ["micro", "macro"])
        micro = self.harness.resolve_integrity_mutation(
            "project", "breached", "micro", "replace the proof route", now=4
        )
        self.assertEqual(micro["pending_components"], ["macro"])
        with self.assertRaisesRegex(DeadlineError, "Integrity mutation is pending"):
            self.harness.start_task("project", "still-blocked", "R-NEXT", 100, now=4)
        macro = self.resolve_integrity_macro(
            "breached", "guarded provenance requirement", now=5
        )
        repeated = self.resolve_integrity_macro(
            "breached", "guarded provenance requirement", now=6
        )
        self.assertEqual(macro["pending_components"], [])
        self.assertFalse(repeated["recorded"])
        self.assertEqual(
            self.harness.connection.execute(
                "SELECT COUNT(*) FROM coordinator_restart_requests"
            ).fetchone()[0],
            1,
        )
        self.assertEqual(
            self.harness.coordinator_view(now=6)["pending_integrity_mutations"],
            [],
        )

    def test_deadline_and_integrity_mutations_coexist_without_conflation(self) -> None:
        self.harness.start_task("project", "both", "R-BOTH", 1, now=0)
        self.harness.record_integrity_breach(
            "project", "both", "late proof was fabricated", now=2
        )
        self.harness.diagnose_claim_deadline(
            "project", "R-BOTH", "clock missed", "Closure outlived its item clock.", now=3
        )
        self.harness.diagnose_incident(
            "project", "both", "integrity_breach", "proof invalid",
            "The late artifact had false provenance.", now=3,
        )
        self.harness.resolve_deadline_mutation(
            "project", "R-BOTH", "micro", "deadline micro", now=4
        )
        deadline_done = self.resolve_deadline_macro(
            "R-BOTH", "deadline macro", now=5
        )
        mid = self.harness.coordinator_view(now=5)
        self.assertEqual(mid["pending_deadline_mutations"], [])
        self.assertEqual(
            mid["pending_integrity_mutations"][0]["pending_components"],
            ["micro", "macro"],
        )
        self.harness.resolve_integrity_mutation(
            "project", "both", "micro", "integrity micro", now=6
        )
        integrity_done = self.resolve_integrity_macro(
            "both", "integrity macro", now=7
        )
        self.assertEqual(
            deadline_done["coordinator_restart"]["generation"],
            integrity_done["coordinator_restart"]["generation"],
        )
        self.assertEqual(
            self.harness.connection.execute(
                "SELECT COUNT(*) FROM coordinator_restart_requests"
            ).fetchone()[0],
            1,
        )

    def test_new_claim_attempt_cli_surface_has_explicit_required_flags(self) -> None:
        parser = build_parser()
        commands = {
            "transition-closure": [
                "--lineage", "project", "--claim", "R-001",
                "--basis-task", "W-001", "--outcome", "outcome",
                "--evidence", "evidence", "--remaining-gap", "gap",
            ],
            "reopen-exploration": [
                "--lineage", "project", "--claim", "R-001",
                "--basis-task", "W-002", "--contradicted-premise", "premise",
            ],
            "accept-claim": [
                "--lineage", "project", "--claim", "R-001",
                "--task", "W-003", "--evidence", "accepted",
            ],
            "close-gap": [
                "--lineage", "project", "--claim", "R-001", "--gap", "G-001",
                "--task", "W-003", "--evidence", "accepted",
            ],
            "revise-gap": [
                "--lineage", "project", "--claim", "R-001", "--gap", "G-001",
                "--basis-task", "W-003", "--description", "changed gap",
                "--proof-route", "changed route",
            ],
            "abandon-attempt": [
                "--lineage", "project", "--task", "W-004",
                "--reason", "replaced",
            ],
            "diagnose-claim-deadline": [
                "--lineage", "project", "--claim", "R-001",
                "--short-verdict", "late", "--diagnosis", "diagnosis",
            ],
            "deadline-miss": [
                "--lineage", "project", "--claim", "R-001",
            ],
            "resolve-deadline-mutation": [
                "--lineage", "project", "--claim", "R-001",
                "--component", "micro", "--evidence", "guarded",
            ],
            "resolve-integrity-mutation": [
                "--lineage", "project", "--task", "W-003",
                "--component", "micro", "--evidence", "guarded",
            ],
            "resolve-clock-migration": [
                "--lineage", "project", "--claim", "R-001",
                "--reason", "adopt", "--source-task", "W-001",
            ],
            "clock-migration-details": [
                "--lineage", "project", "--claim", "R-001",
            ],
            "claim-invalidation-details": [
                "--lineage", "project", "--claim", "R-001",
            ],
            "resolve-random-mutation": [
                "--lineage", "project", "--cycle", "1",
                "--component", "universal", "--receipt", "receipt-id",
                "--evidence", "guarded",
            ],
        }

        for command, flags in commands.items():
            with self.subTest(command=command):
                parsed = parser.parse_args([command, "--state", "state.sqlite", *flags])
                self.assertEqual(parsed.command, command)

        named = parser.parse_args(
            [
                "transition-closure", "--state", "state.sqlite",
                "--lineage", "project", "--claim", "R-001",
                "--basis-task", "W-001", "--outcome", "outcome",
                "--evidence", "proof route", "--gap", "G-001::first",
                "--gap", "G-002::second",
            ]
        )
        self.assertEqual(named.named_gaps, ["G-001::first", "G-002::second"])

    def test_worker_claim_is_not_a_coordinator_callable_cli_command(self) -> None:
        with self.assertRaises(SystemExit):
            build_parser().parse_args(
                [
                    "claim-worker", "--state", "state.sqlite",
                    "--lineage", "project", "--task", "W-001",
                    "--worker", "/root/r008_closure_088",
                    "--coordinator-session", "coordinator",
                    "--supervisor", "123",
                ]
            )


if __name__ == "__main__":
    unittest.main()
