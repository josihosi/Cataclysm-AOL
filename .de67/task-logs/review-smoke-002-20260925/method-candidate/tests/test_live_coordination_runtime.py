"""Replay live coordination through native events, real clocks, and compiled policy."""

from __future__ import annotations

import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from codex_runner import CoordinatorLoopGuard
from deadline_harness import DeadlineError, DeadlineHarness
from test_coordinator_loop_runtime import handoff, wait_for


def command_event(result: dict) -> dict:
    return {"type": "item.completed", "item": {
        "type": "command_execution", "status": "completed", "exit_code": 0,
        "aggregated_output": json.dumps(result),
    }}


class LiveCoordinationRuntimeTests(unittest.TestCase):
    def test_progress_steering_and_independent_completion_preserve_live_outcome(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            state = workspace / "clock.sqlite3"
            de67 = workspace / ".de67"
            de67.mkdir()
            (de67 / "work-ledger.md").write_text(
                "- [ ] R-1 — Observe the production outcome.\n"
                "  - DFS slices: `R-1-S001`\n", encoding="utf-8"
            )
            (de67 / "FS.md").write_text(
                "<!-- DE67:DFS-SLICE:BEGIN id=R-1-S001 claim=R-1 -->\n"
                "- [ ] 🔴 R-1 — Observe the production outcome.\n"
                "<!-- DE67:DFS-SLICE:END id=R-1-S001 claim=R-1 -->\n", encoding="utf-8")

            def decide(now: int) -> dict:
                result = subprocess.run([
                    sys.executable, "-B", str(ROOT / "scripts/policy_kernel.py"),
                    "decide", "--policy", str(ROOT / "assets/environment/phase3-policy.d67"),
                    "--workspace", str(workspace), "--state", str(state),
                    "--lineage", "project", "--now", str(now),
                ], capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                return json.loads(result.stdout)

            with DeadlineHarness(state) as harness:
                harness.start_task("project", "explore", "R-1", 100, now=0)
                harness.complete_task("project", "explore", "route understood", now=1)
                harness.transition_claim_to_closure(
                    "project", "R-1", "explore", "Two independent outcomes remain.",
                    "Observe the run while testing the isolated parser.",
                    gaps=[
                        ("observe", "Observe the production outcome.", "Run the isolated scenario."),
                        ("parser", "Prove the independent parser.", "Exercise its actual input route."),
                    ], now=2,
                )

                guard = CoordinatorLoopGuard(claim_recorder=lambda task, worker, parent:
                    harness.claim_worker("project", task, worker, parent, "supervisor", now=4))
                guard.observe({"type": "thread.started", "thread_id": "coordinator"})
                guard.observe(command_event(harness.start_task(
                    "project", "playtest", "R-1", 100, phase="closure", gap_id="observe", now=3,
                )))
                guard.observe(handoff("spawn_agent", "worker-player"))
                self.assertEqual(decide(5)["action"], "coordinate_live_work")
                original_clock = tuple(harness.connection.execute(
                    "SELECT started_at, deadline_at FROM claim_clocks WHERE claim_id = 'R-1'"
                ).fetchone())

                # Conversation itself requires no database transition or receipt.
                before = harness.connection.total_changes
                guard.observe(handoff("send_message", "coordinator"))
                guard.observe(handoff("send_message", "worker-player"))
                self.assertEqual(harness.connection.total_changes, before)
                checkpoint = harness.checkpoint_worker(
                    "project", "playtest", "worker-player", "progress",
                    "Partial repair: scroll works; prompt.cancel is wrong_surface. "
                    "Continue the existing query scope-lifetime investigation.", now=6,
                )
                guard.observe(command_event(checkpoint))
                self.assertEqual(decide(7)["action"], "coordinate_live_work")

                guard.observe(command_event(harness.start_task(
                    "project", "parser-test", "R-1", 100, phase="closure", gap_id="parser", now=8,
                )))
                dispatched = decide(9)
                self.assertEqual(dispatched["action"], "spawn_worker")
                self.assertEqual([item["task_id"] for item in dispatched["worker_spawns"]], ["parser-test"])
                guard.observe(command_event(dispatched))
                guard.observe(handoff("followup_task", "worker-player"))
                self.assertEqual(guard.unbound_tasks, ("parser-test",))
                # Continuing the partial defect report preserves worker, task, and clock;
                # unrelated parser work still receives its own fresh worker below.
                bound = harness.connection.execute(
                    "SELECT worker_id, released_at FROM worker_claims WHERE task_id = 'playtest'"
                ).fetchone()
                self.assertEqual(tuple(bound), ("worker-player", None))
                self.assertEqual(original_clock, tuple(harness.connection.execute(
                    "SELECT started_at, deadline_at FROM claim_clocks WHERE claim_id = 'R-1'"
                ).fetchone()))
                guard.observe(handoff("spawn_agent", "worker-parser"))
                guard.observe(wait_for("worker-player", "worker-parser"))

                with self.assertRaisesRegex(DeadlineError, "requires its worker result receipt"):
                    harness.complete_task("project", "parser-test", "done", now=10)
                receipt = harness.record_worker_result_receipt(
                    "project", "parser-test", "worker-parser", {
                        "schema": "de67.worker-result-receipt.v1", "lineage_id": "project",
                        "task_id": "parser-test", "claim_id": "R-1", "worker_id": "worker-parser",
                        "disposition": "completed", "verdict": "parser route passed",
                        "outcome": "Prove the independent parser.", "summary": "Actual input accepted.",
                        "material_changes": [], "tests": ["parser input check"], "live_actions": [],
                        "evidence_ceiling": ["parser only"], "bindings": {}, "journal_entries": [],
                        "artifacts": [], "first_divergence": None, "accepted_no_replay": [],
                        "active_work": [], "first_open_boundary": "", "narrow_queries": [],
                        "entrypoints": [], "context_metrics": {},
                    }, now=10,
                )
                guard.observe(command_event(harness.complete_task(
                    "project", "parser-test", "parser route passed", receipt_id=receipt["receipt_id"], now=11,
                )))
                self.assertEqual(decide(12)["action"], "coordinate_live_work")
                guard.observe(wait_for("worker-player"))
                self.assertEqual(tuple(harness.connection.execute(
                    "SELECT started_at, deadline_at FROM claim_clocks WHERE claim_id = 'R-1'"
                ).fetchone()), original_clock)
                self.assertEqual(tuple(harness.connection.execute(
                    "SELECT attempt_terminal_at, worker_id, released_at FROM tasks "
                    "JOIN worker_claims USING (lineage_id, task_id) WHERE task_id = 'playtest'"
                ).fetchone()), (None, "worker-player", None))
                self.assertEqual(guard.unbound_tasks, ())

                # A newly due gate supersedes useful coordination and independent dispatch.
                (de67 / "mutation-suggestions.md").write_text(
                    "## Pending suggestions\n\n- Review the delivery guidance.\n", encoding="utf-8"
                )
                gated = decide(13)
                self.assertEqual(gated["action"], "wait_for_mutation_quiescence")
                self.assertNotIn("worker_spawns", gated)


if __name__ == "__main__":
    unittest.main()
