from __future__ import annotations

import json
import os
import sqlite3
import subprocess
import sys
import tempfile
import time
import unittest
from contextlib import closing
from pathlib import Path
from unittest.mock import patch


SCRIPTS = Path(__file__).resolve().parents[1] / "scripts"
sys.path.insert(0, str(SCRIPTS))

from coordinator_supervisor import (  # noqa: E402
    ChildResult,
    MutationGate,
    SupervisorJournal,
    SupervisionEvent,
    SupervisorError,
    _complete_mutation_review,
    _fresh_deadline_harness,
    _supervisor_lock,
    build_parser,
    coordinator_context_contract,
    coordinator_ledger_contract,
    coordinator_prompt,
    live_coordination_contract,
    consume_supervision_event,
    blocked_ledger_audit_reason,
    ledger_has_only_blocked_work,
    main,
    mutation_gate,
    pending_mutation_suggestions,
    mutation_reviewer_prompt,
    ordinary_worker_evidence_contract,
    read_clock,
    run_child,
    run_supervisor,
    supervision_fingerprint,
    terminalize_unowned_worker_windows,
    active_worker_coordinator_session,
    wait_for_supervision_event,
    work_is_complete,
    worker_handoff_contract,
    nested_worker_contract,
    worker_result_ingress_contract,
)
from blocker_adapter import BlockerReply  # noqa: E402
from deadline_harness import DeadlineHarness  # noqa: E402


FAKE_RUNNER = r'''from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("--cwd", required=True)
arguments = parser.parse_args()
sys.path.insert(0, os.environ["FAKE_DE67_SCRIPTS"])
from deadline_harness import DeadlineHarness

generation_text = os.environ.get("DE67_COORDINATOR_RESTART_GENERATION")
generation = int(generation_text) if generation_text else None
event = {
    "pid": os.getpid(),
    "ppid": os.getppid(),
    "cwd": str(Path.cwd()),
    "workspace_argument": arguments.cwd,
    "generation": generation,
    "run_id": os.environ["DE67_COORDINATOR_RUN_ID"],
    "role": os.environ.get("DE67_PROCESS_ROLE"),
    "model": os.environ.get("DE67_COORDINATOR_MODEL"),
    "effort": os.environ.get("DE67_COORDINATOR_REASONING_EFFORT"),
    "ack_argv": json.loads(os.environ["DE67_COORDINATOR_ACK_ARGV_JSON"])
    if generation is not None
    else None,
    "policy_argv": json.loads(os.environ["DE67_POLICY_DECIDE_ARGV_JSON"]),
    "policy_guard_argv": json.loads(os.environ["DE67_POLICY_GUARD_ARGV_JSON"]),
    "resume_session": os.environ.get("DE67_COORDINATOR_RESUME_SESSION"),
}
with Path(os.environ["FAKE_EVENTS"]).open("a", encoding="utf-8") as output:
    output.write(json.dumps(event) + "\n")
event_count = len(Path(os.environ["FAKE_EVENTS"]).read_text(encoding="utf-8").splitlines())
mode = os.environ["FAKE_MODE"]
if not (mode == "crash-without-session-then-complete" and event_count == 1):
    Path(os.environ["DE67_COORDINATOR_SESSION_FILE"]).write_text(
        os.environ.get("DE67_COORDINATOR_RESUME_SESSION", "fake-session") + "\n",
        encoding="utf-8",
    )

with DeadlineHarness(os.environ["DE67_DEADLINE_STATE"]) as harness:
    def complete_owned(task_id, evidence):
        owner = harness.connection.execute(
            "SELECT task.claim_id, claim.worker_id FROM tasks AS task "
            "JOIN worker_claims AS claim USING (lineage_id, task_id) "
            "WHERE task.lineage_id = ? AND task.task_id = ?",
            (os.environ["DE67_LINEAGE"], task_id),
        ).fetchone()
        value = {
            "schema": "de67.worker-result-receipt.v1",
            "lineage_id": os.environ["DE67_LINEAGE"],
            "task_id": task_id,
            "claim_id": owner["claim_id"],
            "worker_id": owner["worker_id"],
            "disposition": "completed",
            "verdict": "fake worker outcome completed",
            "outcome": "Complete the fake worker outcome.",
            "summary": evidence,
            "material_changes": [], "tests": [], "live_actions": [],
            "evidence_ceiling": [], "bindings": {}, "journal_entries": [],
            "artifacts": [], "first_divergence": None,
            "accepted_no_replay": [evidence], "active_work": [],
            "first_open_boundary": "", "narrow_queries": [f"task_id={task_id}"],
            "entrypoints": [], "context_metrics": {},
        }
        receipt = harness.record_worker_result_receipt(
            os.environ["DE67_LINEAGE"], task_id, owner["worker_id"], value
        )
        return harness.complete_task(
            os.environ["DE67_LINEAGE"], task_id, evidence,
            receipt_id=receipt["receipt_id"],
        )

    if mode in {
        "mutation-lifecycle",
        "mutation-after-coordinator",
        "deadline-mutation-lifecycle",
        "recovery-before-and-after-mutation",
    }:
        if os.environ.get("DE67_PROCESS_ROLE") == "mutation-reviewer":
            suggestions = Path(os.environ["DE67_WORKSPACE"]) / ".de67" / "mutation-suggestions.md"
            if not suggestions.is_file():
                raise AssertionError("mutation reviewer must receive the suggestion ledger")
            if mode in {
                "mutation-lifecycle",
                "mutation-after-coordinator",
                "recovery-before-and-after-mutation",
            }:
                cycle = harness.list_tasks()["random_mutation"]
                harness.resolve_random_mutation(
                    os.environ["DE67_LINEAGE"],
                    cycle["cycle_number"],
                    "reviewed the complete suggestion ledger; no change required",
                )
            else:
                incident = harness.list_tasks()["pending_incident_reviews"][0]
                harness.diagnose_claim_deadline(
                    os.environ["DE67_LINEAGE"],
                    incident["claim_id"],
                    incident["task_id"],
                    "The deadline expired before the worker returned.",
                )
                harness.resolve_deadline_mutation(
                    os.environ["DE67_LINEAGE"], incident["claim_id"],
                    "micro", "Recover with a fresh whole-item clock."
                )
                harness.resolve_deadline_mutation(
                    os.environ["DE67_LINEAGE"], incident["claim_id"],
                    "macro", "Keep the existing general deadline guidance."
                )
        else:
            if mode == "recovery-before-and-after-mutation":
                root = Path(os.environ["DE67_WORKSPACE"]) / ".de67"
                if event_count == 1:
                    harness.claim_worker(
                        os.environ["DE67_LINEAGE"], "seed", "worker-one",
                        "fake-session", os.environ["DE67_SUPERVISOR_PID"],
                    )
                    complete_owned("seed", "worker one proof")
                    raise SystemExit(9)
                if event_count == 2:
                    harness.start_task(
                        os.environ["DE67_LINEAGE"], "worker-two-task", "R-001", 3600
                    )
                    harness.claim_worker(
                        os.environ["DE67_LINEAGE"], "worker-two-task", "worker-two",
                        "fake-session", os.environ["DE67_SUPERVISOR_PID"],
                    )
                    complete_owned("worker-two-task", "worker two proof")
                    raise SystemExit(0)
                if event_count == 3:
                    harness.start_task(
                        os.environ["DE67_LINEAGE"], "worker-three-task", "R-001", 3600
                    )
                    harness.claim_worker(
                        os.environ["DE67_LINEAGE"], "worker-three-task", "worker-three",
                        "fake-session", os.environ["DE67_SUPERVISOR_PID"],
                    )
                    complete_owned(
                        "worker-three-task", "worker three proof triggers mutation"
                    )
                    raise SystemExit(0)
                if event_count == 5:
                    harness.acknowledge_coordinator_restart(
                        os.environ["DE67_LINEAGE"],
                        generation,
                        os.environ["DE67_COORDINATOR_RUN_ID"],
                    )
                    harness.start_task(
                        os.environ["DE67_LINEAGE"], "post-mutation", "R-001", 3600
                    )
                    harness.claim_worker(
                        os.environ["DE67_LINEAGE"], "post-mutation", "worker-four",
                        "fake-session", os.environ["DE67_SUPERVISOR_PID"],
                    )
                    raise SystemExit(9)
                if event_count == 6:
                    complete_owned("post-mutation", "post-mutation recovery proof")
                    (root / "FS.md").write_text(
                        "# FS\n\nStatus: Frozen\n\n- [x] R-001 \N{EM DASH} Done\n",
                        encoding="utf-8",
                    )
                    (root / "work-ledger.md").write_text(
                        "# Work ledger\n\n## Active work\n",
                        encoding="utf-8",
                    )
                    raise SystemExit(0)
                raise AssertionError(f"unexpected event count: {event_count}")
            if mode == "mutation-after-coordinator" and generation is None:
                harness.complete_task(
                    os.environ["DE67_LINEAGE"], "seed", "trigger mutation boundary"
                )
                raise SystemExit(0)
            if generation is None:
                raise AssertionError("post-mutation coordinator must be a fresh generation")
            harness.acknowledge_coordinator_restart(
                os.environ["DE67_LINEAGE"],
                generation,
                os.environ["DE67_COORDINATOR_RUN_ID"],
            )
            root = Path(os.environ["DE67_WORKSPACE"]) / ".de67"
            (root / "FS.md").write_text(
                "# FS\n\nStatus: Frozen\n\n- [x] R-001 \N{EM DASH} Done\n",
                encoding="utf-8",
            )
            (root / "work-ledger.md").write_text(
                "# Work ledger\n\n## Active work\n",
                encoding="utf-8",
            )
    elif mode == "two-restarts":
        if generation is None:
            harness.request_coordinator_restart(
                os.environ["DE67_LINEAGE"], "fake first retirement"
            )
        else:
            harness.acknowledge_coordinator_restart(
                os.environ["DE67_LINEAGE"],
                generation,
                os.environ["DE67_COORDINATOR_RUN_ID"],
            )
            if generation == 1:
                harness.request_coordinator_restart(
                    os.environ["DE67_LINEAGE"], "fake second retirement"
                )
    elif mode == "restart-then-exit-nonzero":
        if generation is None:
            harness.request_coordinator_restart(
                os.environ["DE67_LINEAGE"], "valid baton before non-zero exit"
            )
            raise SystemExit(2)
        harness.acknowledge_coordinator_restart(
            os.environ["DE67_LINEAGE"],
            generation,
            os.environ["DE67_COORDINATOR_RUN_ID"],
        )
        root = Path(os.environ["DE67_WORKSPACE"]) / ".de67"
        (root / "FS.md").write_text(
            "# FS\n\nStatus: Frozen\n\n- [x] R-001 \N{EM DASH} Done\n",
            encoding="utf-8",
        )
        (root / "work-ledger.md").write_text(
            "# Work ledger\n\n## Active work\n",
            encoding="utf-8",
        )
    elif mode == "ack":
        harness.acknowledge_coordinator_restart(
            os.environ["DE67_LINEAGE"],
            generation,
            os.environ["DE67_COORDINATOR_RUN_ID"],
        )
    elif mode == "ack-from-json":
        completed = subprocess.run(
            json.loads(os.environ["DE67_COORDINATOR_ACK_ARGV_JSON"]),
            capture_output=True,
            text=True,
        )
        Path(os.environ["FAKE_EVENTS"] + ".ack").write_text(
            json.dumps(
                {
                    "returncode": completed.returncode,
                    "stdout": completed.stdout,
                    "stderr": completed.stderr,
                }
            ),
            encoding="utf-8",
        )
        completed.check_returncode()
    elif mode == "unacknowledged":
        pass
    elif mode == "ack-on-second-opportunity":
        if event_count == 1:
            pass
        else:
            harness.acknowledge_coordinator_restart(
                os.environ["DE67_LINEAGE"],
                generation,
                os.environ["DE67_COORDINATOR_RUN_ID"],
            )
            root = Path(os.environ["DE67_WORKSPACE"]) / ".de67"
            (root / "FS.md").write_text(
                "# FS\n\nStatus: Frozen\n\n- [x] R-001 \N{EM DASH} Done\n",
                encoding="utf-8",
            )
            (root / "work-ledger.md").write_text(
                "# Work ledger\n\n## Active work\n", encoding="utf-8"
            )
    elif mode == "complete-program":
        harness.complete_task(
            os.environ["DE67_LINEAGE"], "seed", "final proof"
        )
        root = Path(os.environ["DE67_WORKSPACE"]) / ".de67"
        (root / "FS.md").write_text(
            "# FS\n\nStatus: Frozen\n\n- [x] R-001 \N{EM DASH} Done\n",
            encoding="utf-8",
        )
        (root / "work-ledger.md").write_text(
            "# Work ledger\n\n## Active work\n",
            encoding="utf-8",
        )
    elif mode == "leave-task-and-close-work":
        root = Path(os.environ["DE67_WORKSPACE"]) / ".de67"
        (root / "FS.md").write_text(
            "# FS\n\nStatus: Frozen\n\n- [x] R-001 \N{EM DASH} Done\n",
            encoding="utf-8",
        )
        (root / "work-ledger.md").write_text(
            "# Work ledger\n\n## Active work\n",
            encoding="utf-8",
        )
    elif mode == "handover-then-complete":
        root = Path(os.environ["DE67_WORKSPACE"]) / ".de67"
        if event_count == 1:
            with (root / "work-ledger.md").open("a", encoding="utf-8") as output:
                output.write("\nDurable coordinator handoff recorded.\n")
        else:
            if generation is not None:
                harness.acknowledge_coordinator_restart(
                    os.environ["DE67_LINEAGE"],
                    generation,
                    os.environ["DE67_COORDINATOR_RUN_ID"],
                )
            harness.complete_task(
                os.environ["DE67_LINEAGE"], "seed", "final proof"
            )
            root = Path(os.environ["DE67_WORKSPACE"]) / ".de67"
            (root / "FS.md").write_text(
                "# FS\n\nStatus: Frozen\n\n- [x] R-001 \N{EM DASH} Done\n",
                encoding="utf-8",
            )
            (root / "work-ledger.md").write_text(
                "# Work ledger\n\n## Active work\n",
                encoding="utf-8",
            )
    elif mode == "drain-then-no-refill":
        root = Path(os.environ["DE67_WORKSPACE"]) / ".de67"
        if event_count == 1:
            harness.complete_task(
                os.environ["DE67_LINEAGE"], "seed", "ledger item retired"
            )
            (root / "work-ledger.md").write_text(
                "# Work ledger\n\n## Active work\n",
                encoding="utf-8",
            )
        else:
            if generation is not None:
                harness.acknowledge_coordinator_restart(
                    os.environ["DE67_LINEAGE"],
                    generation,
                    os.environ["DE67_COORDINATOR_RUN_ID"],
                )
            (root / "FS.md").write_text(
                "# FS\n\nStatus: Frozen\n\n- [x] R-001 \N{EM DASH} Done\n",
                encoding="utf-8",
            )
            (root / "work-ledger.md").write_text(
                "# Work ledger\n\n## Active work\n",
                encoding="utf-8",
            )
    elif mode == "crash-then-complete":
        if event_count == 1:
            raise SystemExit(9)
        if generation is not None:
            raise AssertionError("resumable crash must keep the same coordinator generation")
        harness.start_task(os.environ["DE67_LINEAGE"], "recovery", "R-001", 3600)
        harness.complete_task(os.environ["DE67_LINEAGE"], "recovery", "successor proof")
        root = Path(os.environ["DE67_WORKSPACE"]) / ".de67"
        (root / "FS.md").write_text(
            "# FS\n\nStatus: Frozen\n\n- [x] R-001 \N{EM DASH} Done\n",
            encoding="utf-8",
        )
        (root / "work-ledger.md").write_text(
            "# Work ledger\n\n## Active work\n",
            encoding="utf-8",
        )
    elif mode == "crash-three-times":
        root = Path(os.environ["DE67_WORKSPACE"]) / ".de67"
        if event_count == 1:
            with (root / "work-ledger.md").open("a", encoding="utf-8") as output:
                output.write("\nFirst crash made durable progress.\n")
            raise SystemExit(9)
        if event_count == 2:
            harness.start_task(
                os.environ["DE67_LINEAGE"], "second-crash", "R-001", 3600
            )
            raise SystemExit(9)
        if event_count == 3:
            harness.start_task(
                os.environ["DE67_LINEAGE"], "third-crash", "R-001", 3600
            )
            raise SystemExit(9)
        raise AssertionError("the decision-opportunity fuse allowed a fourth launch")
    elif mode == "successful-pass-resets-decision-fuse":
        root = Path(os.environ["DE67_WORKSPACE"]) / ".de67"
        if event_count == 1:
            with (root / "work-ledger.md").open("a", encoding="utf-8") as output:
                output.write("\nInitial failed decision.\n")
            raise SystemExit(9)
        harness.start_task(
            os.environ["DE67_LINEAGE"], f"reset-{event_count}", "R-001", 3600
        )
        if event_count == 2:
            harness.complete_task(
                os.environ["DE67_LINEAGE"], "reset-2", "successful recovery pass"
            )
            raise SystemExit(0)
        if event_count <= 5:
            raise SystemExit(9)
        raise AssertionError("the reset decision fuse allowed a sixth launch")
    elif mode == "claimed-worker-crash-then-complete":
        if event_count == 1:
            harness.claim_worker(
                os.environ["DE67_LINEAGE"], "seed", "worker-a",
                "fake-session", os.environ["DE67_SUPERVISOR_PID"],
            )
            raise SystemExit(9)
        if generation is not None:
            raise AssertionError("worker recovery must keep the coordinator generation")
        complete_owned("seed", "worker result ingested")
        root = Path(os.environ["DE67_WORKSPACE"]) / ".de67"
        (root / "FS.md").write_text(
            "# FS\n\nStatus: Frozen\n\n- [x] R-001 N{EM DASH} Done\n",
            encoding="utf-8",
        )
        (root / "work-ledger.md").write_text(
            "# Work ledger\n\n## Active work\n",
            encoding="utf-8",
        )
    elif mode == "crash-without-session-then-complete":
        if event_count == 1:
            raise SystemExit(9)
        if generation is not None:
            raise AssertionError("crash recovery must not manufacture a mutation generation")
        harness.start_task(os.environ["DE67_LINEAGE"], "recovery", "R-001", 3600)
        harness.complete_task(os.environ["DE67_LINEAGE"], "recovery", "successor proof")
        root = Path(os.environ["DE67_WORKSPACE"]) / ".de67"
        (root / "FS.md").write_text(
            "# FS\n\nStatus: Frozen\n\n- [x] R-001 \N{EM DASH} Done\n",
            encoding="utf-8",
        )
        (root / "work-ledger.md").write_text(
            "# Work ledger\n\n## Active work\n",
            encoding="utf-8",
        )
    elif mode == "confirm-blocked":
        if generation is None:
            raise AssertionError("blocked audit must be a restart generation")
        harness.acknowledge_coordinator_restart(
            os.environ["DE67_LINEAGE"],
            generation,
            os.environ["DE67_COORDINATOR_RUN_ID"],
        )
        harness.report_worker_finding(
            os.environ["DE67_LINEAGE"],
            "seed",
            "blocker",
            "Fresh coordinator confirmed the exact owner choice remains unavailable.",
            short_verdict="owner choice is still required",
        )
    elif mode == "blocked-reply-complete":
        if generation is None:
            raise AssertionError("blocked reply route must use a restart generation")
        harness.acknowledge_coordinator_restart(
            os.environ["DE67_LINEAGE"],
            generation,
            os.environ["DE67_COORDINATOR_RUN_ID"],
        )
        if generation == 2:
            root = Path(os.environ["DE67_WORKSPACE"]) / ".de67"
            (root / "FS.md").write_text(
                "# FS\n\nStatus: Frozen\n\n- [x] R-001 \N{EM DASH} Done\n",
                encoding="utf-8",
            )
            (root / "work-ledger.md").write_text(
                "# Work ledger\n\n## Active work\n",
                encoding="utf-8",
            )
    elif mode == "matrix-recover":
        if generation is not None:
            harness.acknowledge_coordinator_restart(
                os.environ["DE67_LINEAGE"],
                generation,
                os.environ["DE67_COORDINATOR_RUN_ID"],
            )
        event_count = len(Path(os.environ["FAKE_EVENTS"]).read_text(encoding="utf-8").splitlines())
        if event_count == 1:
            raise SystemExit(int(os.environ["FAKE_FIRST_EXIT"]))
        harness.start_task(os.environ["DE67_LINEAGE"], "recovery", "R-001", 3600)
        harness.complete_task(os.environ["DE67_LINEAGE"], "recovery", "matrix recovery proof")
        root = Path(os.environ["DE67_WORKSPACE"]) / ".de67"
        (root / "FS.md").write_text(
            "# FS\n\nStatus: Frozen\n\n- [x] R-001 \N{EM DASH} Done\n",
            encoding="utf-8",
        )
        (root / "work-ledger.md").write_text(
            "# Work ledger\n\n## Active work\n",
            encoding="utf-8",
        )
    elif mode == "fail-before-ack":
        raise SystemExit(7)
    else:
        raise AssertionError(f"unknown fake mode: {mode}")
'''


def restart_required(restart: dict[str, object]) -> bool:
    value = restart.get("required", restart.get("pending"))
    if not isinstance(value, bool):
        raise AssertionError("restart state lacks required/pending boolean")
    return value


class ReviewerLaunchPromotionTests(unittest.TestCase):
    def test_retained_supervisor_uses_promoted_machine_bindings(self):
        import coordinator_supervisor as supervisor
        gate = MutationGate("random", "cycle 13", "DFS.md")
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            promoted = root / "coordinator_supervisor.py"
            source = Path(supervisor.__file__).read_text()
            promoted.write_text(source.replace(
                '"DE67_COORDINATOR_MODEL": "gpt-6-astra",',
                '"PROMOTED_BINDING": "revision-2", "DE67_COORDINATOR_MODEL": "gpt-6-astra",'
            ))
            with patch.object(supervisor, "__file__", str(promoted)), \
                 patch.object(supervisor, "mutation_reviewer_environment", side_effect=AssertionError("stale producer used")), \
                 patch.object(supervisor, "run_child") as child:
                supervisor.run_mutation_reviewer([], root, root / "state", "lineage", root, gate,
                    extra_env={"PRESERVED": "yes", "DE67_MUTATION_GATE_JSON": "stale"}, run_id="review")
            values = child.call_args.kwargs["extra_env"]
            self.assertEqual(values["PROMOTED_BINDING"], "revision-2")
            self.assertEqual(values["PRESERVED"], "yes")
            self.assertEqual(values["DE67_COORDINATOR_MODEL"], "gpt-6-astra")
            self.assertEqual(child.call_args.kwargs["role"], "mutation-reviewer")
            self.assertEqual(child.call_count, 1)



class CoordinatorSupervisorTests(unittest.TestCase):
    def test_post_review_projection_loads_the_installed_delivery_writer(self) -> None:
        # A long-lived parent keeps its ordinary import, but the sole
        # post-review projection must use the exact on-disk writer that the
        # exclusive reviewer promoted before it can authorize one successor.
        fresh = _fresh_deadline_harness()
        self.assertIsNot(fresh, DeadlineHarness)
        self.assertEqual(
            Path(sys.modules[fresh.__module__].__file__).resolve(),
            (SCRIPTS / "deadline_harness.py").resolve(),
        )
        with fresh(self.state_path) as harness:
            self.assertEqual(harness.synchronize_dfs_statuses(persist=False), ())

    def test_packaged_ledger_refills_until_dfs_is_green_without_batch_cap(self) -> None:
        template = (
            SCRIPTS.parent / "assets" / "environment" / "work-ledger.md"
        ).read_text(encoding="utf-8")

        self.assertIn("Refill this projection", template)
        self.assertIn("empty batch is not completion", template)
        self.assertIn("Do not impose a batch-size limit", template)
        self.assertNotIn("at most ten", template)

    def test_mutation_suggestion_modes_separate_wakeup_from_consumption(self) -> None:
        with DeadlineHarness(self.state_path) as harness:
            harness.complete_task("project", "seed", "terminal proof")
        (self.workspace / ".de67").mkdir()
        ledger = self.workspace / ".de67" / "mutation-suggestions.md"
        ledger.write_text(
            "# Mutation suggestions\n\n## Pending suggestions\n\n"
            "- Owner-authorized [defer]: Improve the worker proof handoff.\n",
            encoding="utf-8",
        )

        suggestions = pending_mutation_suggestions(self.workspace)

        self.assertEqual([item.mode for item in suggestions], ["defer"])
        self.assertIsNone(mutation_gate(self.state_path, "project", self.workspace))

        ledger.write_text(
            ledger.read_text(encoding="utf-8")
            + "- Owner-authorized [trigger]: Repair the supervisor immediately.\n",
            encoding="utf-8",
        )
        gate = mutation_gate(self.state_path, "project", self.workspace)
        self.assertIsNotNone(gate)
        assert gate is not None
        self.assertEqual(gate.kind, "owner-suggestion")

    def test_consumed_suggestions_cannot_reactivate_review(self) -> None:
        with DeadlineHarness(self.state_path) as harness:
            harness.complete_task("project", "seed", "terminal proof")
        (self.workspace / ".de67").mkdir()
        ledger = self.workspace / ".de67" / "mutation-suggestions.md"
        ledger.write_text(
            "# Mutation suggestions\n\n## Pending suggestions\n\n"
            "- Owner-authorized [defer]: Preserve the next owner request.\n"
            "  Its continuation remains pending.\n\n"
            "## Consumed suggestions\n\n"
            "- Owner-authorized [trigger]: Already applied and proved.\n"
            "- An old unlabelled request was also completed.\n",
            encoding="utf-8",
        )

        suggestions = pending_mutation_suggestions(self.workspace)

        self.assertEqual(len(suggestions), 1)
        self.assertEqual(suggestions[0].mode, "defer")
        self.assertIn("next owner request", suggestions[0].entry)
        self.assertIsNone(mutation_gate(self.state_path, "project", self.workspace))

    def test_legacy_unlabelled_owner_suggestion_still_triggers(self) -> None:
        with DeadlineHarness(self.state_path) as harness:
            harness.complete_task("project", "seed", "terminal proof")
        (self.workspace / ".de67").mkdir()
        (self.workspace / ".de67" / "mutation-suggestions.md").write_text(
            "# Mutation suggestions\n\n## Pending suggestions\n\n- Keep legacy behavior.\n",
            encoding="utf-8",
        )

        gate = mutation_gate(self.state_path, "project", self.workspace)

        self.assertIsNotNone(gate)
        assert gate is not None
        self.assertEqual(gate.kind, "owner-suggestion")

    def test_supervisor_journal_persists_frontier_replay_fuse(self) -> None:
        first = SupervisorJournal(self.state_path, "project", "owner-one")
        first.begin("coordinator", "frontier-a", "run-one")
        first.finish("run-one", "failed", "runner died")

        recovered = SupervisorJournal(self.state_path, "project", "owner-two")
        with self.assertRaisesRegex(SupervisorError, "frontier was already attempted"):
            recovered.begin("coordinator", "frontier-a", "run-two")

        recovered.begin("coordinator", "frontier-b", "run-three")

    def test_supervisor_journal_scopes_replay_fuse_to_explicit_start_epoch(self) -> None:
        first = SupervisorJournal(self.state_path, "project", "owner-one", "start-one")
        first.begin("coordinator", "frontier-a", "run-one")
        first.finish("run-one", "failed", "service died")

        restarted = SupervisorJournal(
            self.state_path, "project", "owner-two", "start-two"
        )
        restarted.begin("coordinator", "frontier-a", "run-two")
        with self.assertRaisesRegex(SupervisorError, "frontier was already attempted"):
            restarted.begin("coordinator", "frontier-a", "run-three")

    def test_worker_handoff_contract_explains_runtime_owned_claim(self) -> None:
        contract = worker_handoff_contract()
        for requirement in ('exact task and hash-bound packet', 'explicit model/effort',
                            'outcome and exit condition', 'Assignment TASK-ID',
                            'worker input, not coordinator context', 'Assign through worker_library.py',
                            'one distinct worker each before waiting', 'Native spawn_agent remains available',
                            'records verified ownership', 'Do not invoke claim-worker yourself',
                            '/root/<task-name> is not a worker UUID', 'a worker result is outstanding'):
            self.assertIn(requirement, contract)


    def test_nested_worker_contract_preserves_primary_task_ownership(self) -> None:
        contract = nested_worker_contract()

        self.assertIn('fork_turns="none"', contract)
        self.assertIn("native Luna helpers", contract)
        self.assertIn("Helpers need no deadline task", contract)
        self.assertIn("caller owns the result", contract)
        self.assertIn("collects or stops helpers before return", contract)
        self.assertIn("never own coordinator state", contract)

    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.state_path = self.root / "deadlines.sqlite3"
        self.workspace = self.root / "workspace"
        self.workspace.mkdir()
        self.run_root = self.root / "runs"
        self.events = self.root / "events.jsonl"
        self.fake_runner = self.root / "fake_runner.py"
        self.fake_runner.write_text(FAKE_RUNNER, encoding="utf-8")
        with DeadlineHarness(self.state_path) as harness:
            harness.start_task(
                "project", "seed", "R-000", 3600, now=time.time()
            )
        self.waiter = patch(
            "coordinator_supervisor.wait_for_supervision_event",
            return_value=None,
        )
        self.waiter.start()

    def tearDown(self) -> None:
        self.waiter.stop()
        self.temporary.cleanup()

    def test_runner_receives_unicode_context_without_locale_loss(self) -> None:
        self.run_root.mkdir()
        receiver = self.root / "receive_prompt.py"
        received = self.root / "received.txt"
        receiver.write_text(
            "import os, pathlib, sys\n"
            "assert 'DE67_INITIAL_INPUT_PATH' not in os.environ, 'owner launch input leaked into supervisor child'\n"
            "pathlib.Path(os.environ['RECEIVED_PROMPT']).write_text(sys.stdin.read(), encoding='utf-8')\n",
            encoding="utf-8",
        )
        prompt = "Worker observation: 🔴 東京 — revised route."
        result = run_child(
            [sys.executable, str(receiver)], self.workspace, self.state_path,
            "project", self.run_root, "unicode-context", None,
            prompt_override=prompt,
            extra_env={"RECEIVED_PROMPT": str(received), "PYTHONIOENCODING": "cp1252",
                       "DE67_INITIAL_INPUT_PATH": "previous-owner-launch.json"},
        )
        self.assertEqual(result.exit_code, 0)
        delivered = received.read_text(encoding="utf-8")
        self.assertTrue(delivered.startswith(prompt + "\n"))
        self.assertEqual(delivered, (result.run_dir / "prompt.txt").read_text(encoding="utf-8"))

    def test_coordinator_exit_preserves_claimed_worker_window(self) -> None:
        with DeadlineHarness(self.state_path) as harness:
            harness.claim_worker(
                "project", "seed", "worker-a", "coordinator-a", "supervisor-a", now=1
            )
        terminalized = terminalize_unowned_worker_windows(
            self.state_path,
            "project",
            {"worker-a": "coordinator-a"},
        )

        self.assertEqual(terminalized, ())
        with DeadlineHarness(self.state_path) as harness:
            status = harness.status_task("project", "seed")
        self.assertEqual(status["state"], "running")
        self.assertIsNone(status["attempt_terminal_kind"])

    def test_coordinator_exit_terminalizes_only_unclaimed_worker_window(self) -> None:
        terminalized = terminalize_unowned_worker_windows(
            self.state_path,
            "project",
        )

        self.assertEqual(terminalized, ("seed",))
        with DeadlineHarness(self.state_path) as harness:
            status = harness.status_task("project", "seed")
        self.assertEqual(status["state"], "abandoned")

    def test_coordinator_exit_terminalizes_stale_worker_claim(self) -> None:
        with DeadlineHarness(self.state_path) as harness:
            harness.claim_worker(
                "project", "seed", "worker-a", "coordinator-a", "supervisor-a", now=1
            )

        terminalized = terminalize_unowned_worker_windows(
            self.state_path,
            "project",
            {},
        )

        self.assertEqual(terminalized, ("seed",))
        with DeadlineHarness(self.state_path) as harness:
            status = harness.status_task("project", "seed")
        self.assertEqual(status["state"], "abandoned")

    def test_supervisor_restart_recovers_the_active_worker_owner_session(self) -> None:
        with DeadlineHarness(self.state_path) as harness:
            harness.claim_worker(
                "project", "seed", "worker-a", "coordinator-a", "supervisor-old", now=1
            )

        self.assertEqual(
            active_worker_coordinator_session(self.state_path, "project"),
            "coordinator-a",
        )

    def test_supervisor_restart_rejects_multiple_active_worker_owners(self) -> None:
        with DeadlineHarness(self.state_path) as harness:
            harness.claim_worker(
                "project", "seed", "worker-a", "coordinator-a", "supervisor-old", now=1
            )
            harness.start_task("project", "other", "R-001", 3600, now=2)
            harness.claim_worker(
                "project", "other", "worker-b", "coordinator-b", "supervisor-old", now=3
            )

        with self.assertRaisesRegex(SupervisorError, "multiple coordinator sessions"):
            active_worker_coordinator_session(self.state_path, "project")

    def test_worker_checkpoint_changes_the_durable_progress_fingerprint(self) -> None:
        before = supervision_fingerprint(
            self.state_path, "project", self.workspace
        )
        with DeadlineHarness(self.state_path) as harness:
            harness.claim_worker(
                "project", "seed", "worker-a", "coordinator-a", "supervisor-a", now=1
            )
            claimed = supervision_fingerprint(
                self.state_path, "project", self.workspace
            )
            harness.checkpoint_worker(
                "project", "seed", "worker-a", "tested", "focused test passed", now=2
            )
        checkpointed = supervision_fingerprint(
            self.state_path, "project", self.workspace
        )

        self.assertNotEqual(before, claimed)
        self.assertNotEqual(claimed, checkpointed)

    def test_product_checkpoint_runs_only_after_supervisor_journal_is_quiescent(self) -> None:
        self.write_work_documents(red=True, active=True)
        observed_live_attempts: list[int] = []

        def observe_checkpoint(
            _workspace: Path,
            state: Path,
            lineage: str,
            **_options: str,
        ) -> dict[str, str]:
            with closing(sqlite3.connect(state)) as connection:
                observed_live_attempts.append(
                    connection.execute(
                        "SELECT COUNT(*) FROM supervisor_attempts "
                        "WHERE lineage_id = ? AND finished_at IS NULL",
                        (lineage,),
                    ).fetchone()[0]
                )
            return {"status": "disabled"}

        with patch(
            "coordinator_supervisor.checkpoint_repository",
            side_effect=observe_checkpoint,
        ):
            result = run_supervisor(
                self.state_path,
                "project",
                self.workspace,
                self.runner_command(),
                self.run_root,
                extra_env=self.environment("complete-program"),
                run_id_factory=lambda _generation: "checkpoint-boundary",
            )

        self.assertEqual(result, 0)
        self.assertEqual(observed_live_attempts, [0, 0])

    def test_supervisor_does_not_resume_after_child_leaves_orphan_clock(self) -> None:
        self.write_work_documents(red=True, active=True)

        with patch("coordinator_supervisor.mutation_gate", return_value=None):
            result = run_supervisor(
                self.state_path,
                "project",
                self.workspace,
                self.runner_command(),
                self.run_root,
                extra_env=self.environment("leave-task-and-close-work"),
                run_id_factory=lambda _generation: "orphan-clock",
            )

        self.assertEqual(result, 0)
        self.assertEqual(len(self.read_events()), 1)
        with DeadlineHarness(self.state_path) as harness:
            row = harness.connection.execute(
                "SELECT attempt_terminal_kind, abandonment_reason "
                "FROM tasks WHERE task_id = 'seed'"
            ).fetchone()
        self.assertEqual(row["attempt_terminal_kind"], "abandoned")
        self.assertIn("worker_owner_lost", row["abandonment_reason"])

    def test_unchanged_success_with_executable_work_is_not_resumed(self) -> None:
        self.write_work_documents(red=True, active=True)
        (self.workspace / ".de67" / "mutation-suggestions.md").write_text(
            "# Mutation suggestions\n\n## Pending suggestions\n",
            encoding="utf-8",
        )
        with DeadlineHarness(self.state_path) as harness:
            harness.complete_task("project", "seed", "prior proof")

        result = run_supervisor(
            self.state_path,
            "project",
            self.workspace,
            self.runner_command(),
            self.run_root,
            extra_env=self.environment("unacknowledged"),
            run_id_factory=lambda _generation: "unchanged-success",
        )

        self.assertEqual(result, 1)
        self.assertEqual(len(self.read_events()), 1)
        error = (self.run_root / "unchanged-success" / "supervisor_error.txt").read_text()
        self.assertIn("made no durable progress", error)

    def test_persistent_crash_without_durable_progress_is_not_retried(self) -> None:
        self.write_work_documents(red=True, active=True)
        (self.workspace / ".de67" / "mutation-suggestions.md").write_text(
            "# Mutation suggestions\n\n## Pending suggestions\n",
            encoding="utf-8",
        )
        with DeadlineHarness(self.state_path) as harness:
            harness.complete_task("project", "seed", "prior proof")

        result = run_supervisor(
            self.state_path,
            "project",
            self.workspace,
            self.runner_command(),
            self.run_root,
            extra_env=self.environment("fail-before-ack"),
            run_id_factory=lambda _generation: "unchanged-crash",
        )

        self.assertEqual(result, 7)
        self.assertEqual(len(self.read_events()), 1)
        error = (self.run_root / "unchanged-crash" / "supervisor_error.txt").read_text()
        self.assertIn("crashed", error)
        self.assertIn("made no durable progress", error)

    def test_unresolved_mutation_gate_is_not_reviewed_twice(self) -> None:
        self.write_work_documents(red=True, active=True)
        with DeadlineHarness(self.state_path) as harness:
            harness.complete_task("project", "seed", "terminal proof")
        gate = MutationGate("owner-suggestion", "same-gate", None)
        result_dir = self.run_root / "mutation-one"
        result_dir.mkdir(parents=True)
        result = ChildResult("mutation-one", result_dir, 0, True)

        with patch(
            "coordinator_supervisor.run_mutation_reviewer", return_value=result
        ) as reviewer, patch(
            "coordinator_supervisor.mutation_gate", return_value=gate
        ):
            with self.assertRaisesRegex(SupervisorError, "repeated without resolution"):
                _complete_mutation_review(
                    self.runner_command(),
                    self.workspace,
                    self.state_path,
                    "project",
                    self.run_root,
                    gate,
                    extra_env=self.environment("unacknowledged"),
                )

        self.assertEqual(reviewer.call_count, 1)

    def test_supervision_event_signature_is_consumed_once(self) -> None:
        consumed: set[str] = set()
        event = SupervisionEvent(None, "gate:stable")

        consume_supervision_event(consumed, event)
        with self.assertRaisesRegex(SupervisorError, "event replayed"):
            consume_supervision_event(consumed, event)

    def test_alternating_mutation_gates_cannot_cycle_back(self) -> None:
        self.write_work_documents(red=True, active=True)
        with DeadlineHarness(self.state_path) as harness:
            harness.complete_task("project", "seed", "terminal proof")
        gate_a = MutationGate("owner-suggestion", "gate-a", None)
        gate_b = MutationGate("incident-review", "gate-b", None)
        results = []
        for name in ("mutation-a", "mutation-b"):
            run_dir = self.run_root / name
            run_dir.mkdir(parents=True)
            results.append(ChildResult(name, run_dir, 0, True))

        with patch(
            "coordinator_supervisor.run_mutation_reviewer", side_effect=results
        ) as reviewer, patch(
            "coordinator_supervisor.mutation_gate", side_effect=(gate_b, gate_a)
        ):
            with self.assertRaisesRegex(SupervisorError, "repeated without resolution"):
                _complete_mutation_review(
                    self.runner_command(),
                    self.workspace,
                    self.state_path,
                    "project",
                    self.run_root,
                    gate_a,
                    extra_env=self.environment("unacknowledged"),
                )

        self.assertEqual(reviewer.call_count, 2)

    def test_recovery_start_terminalizes_preexisting_orphan_before_child(self) -> None:
        self.write_work_documents(red=True, active=True)
        (self.workspace / ".de67" / "mutation-suggestions.md").write_text(
            "# Mutation suggestions\n\n## Pending suggestions\n",
            encoding="utf-8",
        )
        self.run_root.mkdir()
        (self.run_root / "prior-run").mkdir()

        result = run_supervisor(
            self.state_path,
            "project",
            self.workspace,
            self.runner_command(),
            self.run_root,
            extra_env=self.environment("complete-program"),
            run_id_factory=lambda _generation: "recovered-run",
        )

        self.assertEqual(result, 1)
        self.assertEqual(len(self.read_events()), 1)
        with DeadlineHarness(self.state_path) as harness:
            row = harness.connection.execute(
                "SELECT attempt_terminal_kind, abandonment_reason "
                "FROM tasks WHERE task_id = 'seed'"
            ).fetchone()
        self.assertEqual(row["attempt_terminal_kind"], "abandoned")
        self.assertIn("worker_owner_lost", row["abandonment_reason"])

    def test_clock_event_waits_once_and_signals_mutation_without_restart(self) -> None:
        base = time.time()
        with DeadlineHarness(self.state_path) as harness:
            deadline = harness._claim("project", "R-000")["deadline_at"]
        sleeps: list[float] = []

        event = wait_for_supervision_event(
            self.state_path,
            "project",
            now=lambda: base,
            sleep=sleeps.append,
        )

        self.assertIsInstance(event, SupervisionEvent)
        assert event is not None
        self.assertAlmostEqual(sleeps[0], float(deadline) - base, places=3)
        self.assertTrue(event.signature.startswith("gate:"))
        self.assertIsNone(event.restart)
        with DeadlineHarness(self.state_path) as harness:
            summary = harness.list_tasks(now=float(deadline))
        self.assertEqual(
            summary["pending_incident_reviews"][0]["kind"], "deadline_miss"
        )

    def test_pending_incident_signals_mutation_without_sleeping(self) -> None:
        with DeadlineHarness(self.state_path) as harness:
            deadline = float(harness._claim("project", "R-000")["deadline_at"])
            harness.expire_claim("project", "R-000", now=deadline)
        sleeps: list[float] = []

        event = wait_for_supervision_event(
            self.state_path,
            "project",
            now=lambda: deadline,
            sleep=sleeps.append,
        )

        self.assertIsNotNone(event)
        self.assertEqual(sleeps, [])
        assert event is not None
        self.assertIsNone(event.restart)
        self.assertTrue(event.signature.startswith("gate:"))

    def test_unchanged_incident_does_not_manufacture_restarts(self) -> None:
        with DeadlineHarness(self.state_path) as harness:
            deadline = float(harness._claim("project", "R-000")["deadline_at"])
            harness.expire_claim("project", "R-000", now=deadline)
        first = wait_for_supervision_event(
            self.state_path,
            "project",
            now=lambda: deadline,
            sleep=lambda _seconds: None,
        )
        assert first is not None
        self.assertIsNone(first.restart)

        second = wait_for_supervision_event(
            self.state_path,
            "project",
            now=lambda: deadline,
            sleep=lambda _seconds: None,
        )

        assert second is not None
        with DeadlineHarness(self.state_path) as harness:
            restart = harness.coordinator_restart_status("project")[
                "coordinator_restart"
            ]
        self.assertIsNone(restart)
        self.assertIsNone(second.restart)

    def test_resolved_deadline_does_not_rearm_a_successor(self) -> None:
        with DeadlineHarness(self.state_path) as harness:
            deadline = float(harness._claim("project", "R-000")["deadline_at"])
            harness.expire_claim("project", "R-000", now=deadline)
            harness.connection.execute(
                """
                UPDATE incidents SET reviewed_at = ?
                WHERE lineage_id = 'project' AND task_id = 'seed'
                  AND kind = 'deadline_miss'
                """,
                (deadline,),
            )
            harness.connection.executemany(
                """
                INSERT INTO deadline_mutation_components (
                    lineage_id, claim_id, component, resolved_at, evidence
                ) VALUES ('project', 'R-000', ?, ?, ?)
                """,
                (
                    ("micro", deadline, "micro guarded"),
                    ("macro", deadline, "macro guarded"),
                ),
            )
            harness.connection.commit()

        sleeps: list[float] = []
        event = wait_for_supervision_event(
            self.state_path,
            "project",
            now=lambda: deadline,
            sleep=sleeps.append,
        )

        self.assertIsNone(event)
        self.assertEqual(sleeps, [])
        with DeadlineHarness(self.state_path) as harness:
            self.assertIsNone(
                harness.coordinator_restart_status("project")[
                    "coordinator_restart"
                ]
            )

    def environment(self, mode: str) -> dict[str, str]:
        return {
            "FAKE_DE67_SCRIPTS": str(SCRIPTS),
            "FAKE_EVENTS": str(self.events),
            "FAKE_MODE": mode,
        }

    def runner_command(self) -> list[str]:
        return [sys.executable, str(self.fake_runner)]

    def write_work_documents(self, *, red: bool = False, active: bool = False) -> None:
        state_root = self.workspace / ".de67"
        state_root.mkdir(exist_ok=True)
        claim = "- [ ] \N{LARGE RED CIRCLE} R-001 \N{EM DASH} Open\n" if red else "- [x] R-001 \N{EM DASH} Done\n"
        item = "- [ ] R-001 \N{EM DASH} Current route\n" if active else ""
        (state_root / "FS.md").write_text(
            "# FS\n\nStatus: Frozen\n\n" + claim,
            encoding="utf-8",
        )
        (state_root / "work-ledger.md").write_text(
            "# Work ledger\n\n## Active work\n\n" + item,
            encoding="utf-8",
        )
        (state_root / "mutation-suggestions.md").write_text(
            "# Mutation suggestions\n\n## Pending suggestions\n",
            encoding="utf-8",
        )

    def request_restart(self) -> int:
        with DeadlineHarness(self.state_path) as harness:
            return harness.request_coordinator_restart(
                "project", "test restart"
            )["coordinator_restart"]["generation"]

    def statuses(self) -> dict[str, str]:
        return {
            path.parent.name: path.read_text(encoding="utf-8").strip()
            for path in self.run_root.glob("*/status.txt")
        }

    def read_events(self) -> list[dict[str, object]]:
        return [
            json.loads(line)
            for line in self.events.read_text(encoding="utf-8").splitlines()
        ]

    def test_supervisor_prompt_routes_only_compiled_workspace_policy(self) -> None:
        prompt = coordinator_prompt(
            self.workspace.resolve(),
            self.state_path.resolve(),
            "project",
            "prompt-test-run",
            None,
        )

        self.assertIn("Do not read packaged DE-67", prompt)
        self.assertIn(".de67/phase3-policy.d67", prompt)
        self.assertIn("DE67_POLICY_DECIDE_ARGV_JSON", prompt)
        self.assertIn("machine-canonical", prompt)
        self.assertIn("Never review, apply, or resolve a mutation", prompt)
        self.assertIn("exit immediately", prompt)
        self.assertNotIn("promote both candidate source", prompt)
        self.assertIn("legacy differential fixtures", prompt)
        self.assertIn("Write every owner-facing text field rendered on the hosted dashboard in simple English", prompt)
        self.assertIn("ledger items, latest findings, waiting work", prompt)
        self.assertIn("explain what happened and why it matters", prompt)
        self.assertIn("state what remains or happens next", prompt)
        self.assertIn("exposes a contradiction or a missing causal step", prompt)
        self.assertIn("Internal machine state and FS detail", prompt)
        self.assertIn('fork_turns="none"', prompt)
        self.assertIn("Explicitly choose gpt-6-luna or gpt-6-sol", prompt)
        self.assertIn("Never omit model selection", prompt)
        self.assertIn("pass coordinator or predecessor history", prompt)
        # Verify the complete producing contract reaches routing without freezing its prose.
        self.assertIn(coordinator_ledger_contract(), prompt)
        self.assertIn("`  - Subtasks:`", prompt)
        self.assertIn("`    - [STATE] ID :: DESCRIPTION`", prompt)
        self.assertIn(ordinary_worker_evidence_contract(), prompt)
        ingress = worker_result_ingress_contract()
        self.assertIn(ingress, prompt)
        self.assertLess(prompt.index(ingress), prompt.index("Before each coordinator routing transition"))
        self.assertIn("before executing DE67_POLICY_DECIDE_ARGV_JSON", ingress)
        self.assertIn("exactly one", ingress)
        self.assertIn("completed attempt settles only that task", ingress)
        self.assertIn("close that gap and preserve its proof", ingress)
        self.assertIn("separate claim-acceptance transition", ingress)
        self.assertIn("not as terminal authority", ingress)
        self.assertIn("name the assigned-outcome exit", ingress)
        self.assertIn("checkpoint-worker", ingress)
        self.assertIn("keep the same task live.", ingress)
        self.assertNotIn("would repeat an unchanged request", ingress)
        self.assertIn("why the attempt was inconclusive", ingress)
        self.assertNotIn("next turn supplies new", ingress)
        self.assertIn("marked current owner-contract section", ingress)
        self.assertIn("Verify receipt and use", ingress)
        self.assertIn("project its remaining frontier to a fresh task", ingress)
        self.assertIn("Context exhaustion is not a formal finding", ingress)
        self.assertNotIn("followup_task to the same bound worker", ingress)
        self.assertNotIn("Read .de67/orchestrator-guidelines.md", prompt)
        self.assertNotIn("test-and-task-guidelines.md", prompt)

    def test_recoverable_return_is_checkpointed_before_terminal_admission(self) -> None:
        ingress = worker_result_ingress_contract()

        # Historical counterexample: closure-031 disproved loopback TCP and named no successor,
        # while renderer parity still had authorized repository implementation routes.
        self.assertIn("only disproves the current strategy", ingress)
        self.assertIn("even when the worker names no successor", ingress)
        self.assertIn("choose the next route", ingress)
        self.assertIn("authorized repository repair, rerun", ingress)
        self.assertLess(
            ingress.index("keep the same task live."),
            ingress.index("execution context is exhausted"),
        )
        self.assertIn("abandon only that attempt", ingress)
        self.assertIn("unfinished ledger outcome visible", ingress)
        self.assertNotIn("Do not record finding, release the worker", ingress)

    def test_pending_owner_suggestion_becomes_gate_only_after_workers_are_quiet(self) -> None:
        self.write_work_documents(red=True, active=True)
        (self.workspace / ".de67" / "mutation-suggestions.md").write_text(
            "# Mutation suggestions\n\n## Pending suggestions\n\n- Review this.\n",
            encoding="utf-8",
        )
        self.assertIsNone(mutation_gate(self.state_path, "project", self.workspace))
        with DeadlineHarness(self.state_path) as harness:
            harness.complete_task("project", "seed", "done", now=time.time())
        gate = mutation_gate(self.state_path, "project", self.workspace)
        self.assertIsNotNone(gate)
        assert gate is not None
        self.assertEqual(gate.kind, "owner-suggestion")

    def test_coordinator_contract_trusts_worker_and_coordinator_agents(self) -> None:
        contract = coordinator_ledger_contract()
        self.assertEqual(contract.count("Trust the agent"), 2)
        self.assertIn("retry fuse ends a strategy, not recoverable work", contract)
        self.assertIn("non-credit observation/bootstrap step", contract)

    def test_worker_evidence_contract_keeps_context_current_without_duplicate_lifecycle(self) -> None:
        contract = ordinary_worker_evidence_contract()
        self.assertIn("preserving independent accepted work", contract)
        self.assertIn("missing knowledge", contract)
        self.assertIn("show --revision SHA256", contract)
        # Outcome/lifecycle guidance has one producing owner instead of duplicate packet prose.
        ingress = worker_result_ingress_contract()
        self.assertIn("only disproves the current strategy", ingress)
        self.assertIn("abandon only that attempt", ingress)

    def test_fresh_restart_prompt_includes_exact_owner_reason(self) -> None:
        prompt = coordinator_prompt(
            self.workspace.resolve(),
            self.state_path.resolve(),
            "project",
            "owner-restart",
            3,
            "Return to the still-red R-008 tooling route.",
        )

        self.assertIn("exact owner-authorized restart reason", prompt)
        self.assertIn("Return to the still-red R-008 tooling route.", prompt)

    def test_ordinary_worker_route_retrieves_successive_evidence_slices(self) -> None:
        prompt = coordinator_prompt(
            self.workspace.resolve(),
            self.state_path.resolve(),
            "project",
            "evidence-route",
            None,
        )
        report = {
            "irrelevant_history": [f"old-event-{number}" for number in range(2_000)],
            "proof": {
                "first_divergence": "owner stayed local after dematerialization",
                "expected_owner": "overmap",
                "observed_owner": "local",
            },
        }
        search_results = [
            {"path": f"archive/noise-{number}.cpp", "symbol": "unrelated"}
            for number in range(2_000)
        ] + [{"path": "src/owner.cpp", "symbol": "commit_owner_transfer"}]
        verbose_output = [f"trace: repeated step {number}" for number in range(2_000)] + [
            "error: owner acknowledgement was not persisted",
            "trace: cleanup",
        ]

        retrieved = [
            next(
                item["path"]
                for item in search_results
                if item["symbol"] == "commit_owner_transfer"
            ),
            report["proof"]["first_divergence"],
            next(line for line in verbose_output if line.startswith("error:")),
            {
                "expected": report["proof"]["expected_owner"],
                "observed": report["proof"]["observed_owner"],
            },
        ]
        evidence_grounded_result = (
            f"{retrieved[0]} failed because {retrieved[2]}; "
            f"expected {retrieved[3]['expected']}, observed {retrieved[3]['observed']}."
        )
        bulk_result = (
            f"src/owner.cpp failed because "
            f"{next(line for line in verbose_output if line.startswith('error:'))}; "
            f"expected {report['proof']['expected_owner']}, "
            f"observed {report['proof']['observed_owner']}."
        )

        self.assertIn(ordinary_worker_evidence_contract(), prompt)
        self.assertEqual(evidence_grounded_result, bulk_result)
        self.assertEqual(
            retrieved,
            [
                "src/owner.cpp",
                "owner stayed local after dematerialization",
                "error: owner acknowledgement was not persisted",
                {"expected": "overmap", "observed": "local"},
            ],
        )
        self.assertNotIn(report["irrelevant_history"][0], json.dumps(retrieved))
        self.assertNotIn(verbose_output[0], json.dumps(retrieved))

    def test_mutation_reviewer_prompt_is_outcome_led_and_autonomy_first(self) -> None:
        prompt = mutation_reviewer_prompt(
            self.workspace.resolve(),
            self.state_path.resolve(),
            "project",
            MutationGate("random mutation", "cycle 2", "test-and-task-guidelines.md"),
        )

        self.assertIn("complete pending section of .de67/mutation-suggestions.md is mandatory owner input", prompt)
        self.assertIn("repair the earliest preventable systemic cause", prompt)
        self.assertIn("Separate immediate recovery from repeatable method correction", prompt)
        self.assertIn("prove the correction with a reproduction or counterexample", prompt)
        self.assertIn("preserve that entry and state the exact gap", prompt)
        self.assertIn("external supervisor alone launches the successor", prompt)
        self.assertIn("coordinator_ledger_contract()", prompt)
        self.assertIn("when a ledger change makes it relevant", prompt)
        self.assertNotIn("test-and-task-guidelines.md", prompt)
        self.assertNotIn("Read the exact live selected mutation target", prompt)
        self.assertNotIn("deadline_harness.py", prompt)
        self.assertNotIn("mutation guard", prompt)
        self.assertNotIn("FS candidates", prompt)

    def test_supervisor_exports_exact_compiled_policy_decision_command(self) -> None:
        self.write_work_documents(red=True, active=True)
        result = run_supervisor(
            self.state_path,
            "project",
            self.workspace,
            self.runner_command(),
            self.run_root,
            extra_env=self.environment("complete-program"),
            run_id_factory=lambda _generation: "policy-command",
        )

        self.assertEqual(result, 0)
        command = self.read_events()[0]["policy_argv"]
        self.assertEqual(command[2], "decide")
        self.assertEqual(
            command[3:5],
            ["--policy", str((self.workspace / ".de67" / "phase3-policy.d67").resolve())],
        )
        self.assertIn("--workspace", command)
        self.assertIn("--state", command)
        guard = self.read_events()[0]["policy_guard_argv"]
        self.assertEqual(guard[2], "guard")
        self.assertTrue(any(str(item).endswith("phase3-policy.candidate.json") for item in guard))
        self.assertTrue(any(str(item).endswith("phase3-policy.candidate.d67") for item in guard))

    def test_cli_defaults_coordinator_to_sol_low_without_changing_runner_args(self) -> None:
        arguments = build_parser().parse_args(
            [
                "--state", "state.sqlite3",
                "--lineage", "project",
                "--workspace", "workspace",
                "--run-root", "runs",
                "--runner", "runner", "--runner-owned-option",
            ]
        )

        self.assertEqual(arguments.coordinator_model, "gpt-6-sol")
        self.assertEqual(arguments.coordinator_reasoning_effort, "low")
        self.assertEqual(arguments.runner, ["runner", "--runner-owned-option"])

    def test_due_mutation_exclusively_runs_astra_medium_then_sol_low(self) -> None:
        self.write_work_documents(red=True, active=True)
        with DeadlineHarness(self.state_path) as harness:
            harness.connection.execute(
                """
                UPDATE random_mutation_cycles
                SET interval_windows = 10, due_after_terminal_windows = 10,
                    selected_lane = 'orchestrator-guidelines.md'
                WHERE lineage_id = 'project' AND cycle_number = 1
                """
            )
            harness.connection.commit()
            harness.complete_task("project", "seed", "terminal one")
            for number in range(2, 11):
                task_id = f"terminal-{number}"
                harness.start_task("project", task_id, "R-001", 3600)
                harness.complete_task("project", task_id, f"terminal {number}")

        result = run_supervisor(
            self.state_path,
            "project",
            self.workspace,
            self.runner_command(),
            self.run_root,
            extra_env={
                **self.environment("mutation-lifecycle"),
                "DE67_COORDINATOR_MODEL": "gpt-6-sol",
                "DE67_COORDINATOR_REASONING_EFFORT": "low",
            },
            run_id_factory=lambda _generation: "fresh-low-coordinator",
        )

        self.assertEqual(result, 0)
        events = self.read_events()
        self.assertEqual([event["role"] for event in events], [
            "mutation-reviewer", "coordinator",
        ])
        self.assertEqual(
            [(event["model"], event["effort"]) for event in events],
            [("gpt-6-astra", "medium"), ("gpt-6-sol", "low")],
        )
        reviewer_run = next(
            path for path in self.run_root.iterdir() if path.name.startswith("mutation-")
        )
        reviewer_prompt = (reviewer_run / "prompt.txt").read_text(encoding="utf-8")
        self.assertIn("complete pending section of .de67/mutation-suggestions.md is mandatory owner input", reviewer_prompt)
        self.assertIn("repair the earliest preventable systemic cause", reviewer_prompt)
        self.assertIn("reproduction or counterexample", reviewer_prompt)
        self.assertIn("Resolve the gate only after every pending entry is dispositioned", reviewer_prompt)
        self.assertIn("external supervisor alone launches the successor", reviewer_prompt)
        self.assertNotIn("orchestrator-guidelines.md", reviewer_prompt)
        self.assertNotIn("test-and-task-guidelines.md", reviewer_prompt)
        self.assertNotIn("deadline_harness.py", reviewer_prompt)

    def test_mutation_becoming_due_retires_coordinator_before_reviewer(self) -> None:
        self.write_work_documents(red=True, active=True)
        with DeadlineHarness(self.state_path) as harness:
            harness.connection.execute(
                """
                UPDATE random_mutation_cycles
                SET interval_windows = 10, due_after_terminal_windows = 10,
                    selected_lane = 'test-and-task-guidelines.md'
                WHERE lineage_id = 'project' AND cycle_number = 1
                """
            )
            harness.connection.commit()
            for number in range(2, 11):
                task_id = f"terminal-{number}"
                harness.start_task("project", task_id, "R-001", 3600)
                harness.complete_task("project", task_id, f"terminal {number}")

        run_ids = iter(("retiring-low-coordinator", "fresh-low-coordinator"))
        result = run_supervisor(
            self.state_path,
            "project",
            self.workspace,
            self.runner_command(),
            self.run_root,
            extra_env={
                **self.environment("mutation-after-coordinator"),
                "DE67_COORDINATOR_MODEL": "gpt-6-sol",
                "DE67_COORDINATOR_REASONING_EFFORT": "low",
            },
            run_id_factory=lambda _generation: next(run_ids),
        )

        self.assertEqual(result, 0)
        events = self.read_events()
        self.assertEqual(
            [(event["role"], event["effort"]) for event in events],
            [("coordinator", "low"), ("mutation-reviewer", "medium"),
             ("coordinator", "low")],
        )
        self.assertEqual([event["generation"] for event in events], [None, None, 1])

    def test_mutation_starts_a_fresh_process_recovery_episode(self) -> None:
        self.write_work_documents(red=True, active=True)
        with DeadlineHarness(self.state_path) as harness:
            harness.connection.execute(
                """
                UPDATE random_mutation_cycles
                SET interval_windows = 10, due_after_terminal_windows = 10,
                    selected_lane = 'test-and-task-guidelines.md'
                WHERE lineage_id = 'project' AND cycle_number = 1
                """
            )
            harness.connection.commit()
            for number in range(2, 9):
                task_id = f"pre-mutation-terminal-{number}"
                harness.start_task("project", task_id, "R-001", 3600)
                harness.complete_task("project", task_id, f"terminal {number}")

        run_ids = iter(
            (
                "pre-mutation-crash",
                "pre-mutation-recovery",
                "pre-mutation-third-wave",
                "post-mutation-crash",
                "post-mutation-recovery",
            )
        )
        with patch(
            "coordinator_supervisor.runtime_worker_owners",
            return_value={"worker-four": "fake-session"},
        ):
            result = run_supervisor(
                self.state_path,
                "project",
                self.workspace,
                self.runner_command(),
                self.run_root,
                extra_env=self.environment("recovery-before-and-after-mutation"),
                run_id_factory=lambda _generation: next(run_ids),
            )

        events = self.read_events()
        self.assertEqual(
            result,
            0,
            {"statuses": self.statuses(), "events": events},
        )
        self.assertEqual(
            [event["role"] for event in events],
            [
                "coordinator",
                "coordinator",
                "coordinator",
                "mutation-reviewer",
                "coordinator",
                "coordinator",
            ],
        )
        self.assertEqual(
            [event["run_id"] for event in events if event["role"] == "coordinator"],
            [
                "pre-mutation-crash",
                "pre-mutation-recovery",
                "pre-mutation-third-wave",
                "post-mutation-crash",
                "post-mutation-recovery",
            ],
        )
        self.assertEqual(
            [event["resume_session"] for event in events],
            [None, "fake-session", "fake-session", None, None, "fake-session"],
        )
        self.assertEqual(
            self.statuses(),
            {
                "pre-mutation-crash": "FAILED",
                "pre-mutation-recovery": "DONE",
                "pre-mutation-third-wave": "DONE",
                next(
                    path.name
                    for path in self.run_root.iterdir()
                    if path.name.startswith("mutation-")
                ): "DONE",
                "post-mutation-crash": "FAILED",
                "post-mutation-recovery": "DONE",
            },
        )
        with DeadlineHarness(self.state_path) as harness:
            tasks = {
                task["task_id"]: task
                for task in harness.list_tasks()["tasks"]
            }
            restart = harness.coordinator_restart_status("project")[
                "coordinator_restart"
            ]
            claims = harness.connection.execute(
                "SELECT task_id, worker_id, released_at FROM worker_claims "
                "WHERE lineage_id = 'project' ORDER BY claimed_at"
            ).fetchall()
        self.assertEqual(
            [
                (claim["task_id"], claim["worker_id"], claim["released_at"] is not None)
                for claim in claims
            ],
            [
                ("seed", "worker-one", True),
                ("worker-two-task", "worker-two", True),
                ("worker-three-task", "worker-three", True),
                ("post-mutation", "worker-four", True),
            ],
        )
        self.assertTrue(all(task["state"] == "completed" for task in tasks.values()))
        self.assertEqual(restart["generation"], 1)
        self.assertFalse(restart_required(restart))
        self.assertEqual(restart["run_id"], "post-mutation-crash")

    def test_optional_adapter_argument_stays_outside_runner_arguments(self) -> None:
        arguments = build_parser().parse_args(
            [
                "--state", "state.sqlite3",
                "--lineage", "project",
                "--workspace", "workspace",
                "--run-root", "runs",
                "--blocker-adapter-command-json", "not-json",
                "--runner", "runner",
            ]
        )

        self.assertEqual(arguments.blocker_adapter_command_json, "not-json")
        self.assertEqual(arguments.runner, ["runner"])

    def test_invalid_optional_adapter_is_disabled_without_failing_core_main(self) -> None:
        arguments = [
            "--state", "state.sqlite3",
            "--lineage", "project",
            "--workspace", "workspace",
            "--run-root", "runs",
            "--blocker-adapter-command-json", "not-json",
            "--runner", "runner",
        ]
        with patch("coordinator_supervisor.run_supervisor", return_value=0) as run:
            result = main(arguments)

        self.assertEqual(result, 0)
        self.assertIsNone(run.call_args.kwargs["blocker_waiter"])

    def test_external_parent_continues_two_restart_generations_once_each(self) -> None:
        run_ids = iter(("initial-run", "generation-1-run", "generation-2-run"))

        with patch("coordinator_supervisor.read_clock", wraps=read_clock) as clock_reads:
            result = run_supervisor(
                self.state_path,
                "project",
                self.workspace,
                self.runner_command(),
                self.run_root,
                extra_env=self.environment("two-restarts"),
                run_id_factory=lambda _generation: next(run_ids),
            )

        self.assertEqual(result, 0)
        events = self.read_events()
        self.assertEqual([event["generation"] for event in events], [None, 1, 2])
        self.assertEqual(
            [event["run_id"] for event in events],
            ["initial-run", "generation-1-run", "generation-2-run"],
        )
        self.assertTrue(all(event["ppid"] == os.getpid() for event in events))
        self.assertTrue(
            all(event["cwd"] == str(self.workspace.resolve()) for event in events)
        )
        self.assertEqual(clock_reads.call_count, 4)
        initial_prompt = (self.run_root / "initial-run" / "prompt.txt").read_text(
            encoding="utf-8"
        )
        self.assertIn("DE67_DEADLINE_STATE", initial_prompt)
        self.assertIn("DE67_LINEAGE", initial_prompt)
        self.assertIn("DE67_POLICY_DECIDE_ARGV_JSON", initial_prompt)
        for event in events:
            prompt = (self.run_root / event["run_id"] / "prompt.txt").read_text(
                encoding="utf-8"
            )
            self.assertIn(coordinator_context_contract(), prompt)
            self.assertIn(live_coordination_contract(), prompt)
            bindings = json.loads(prompt.rsplit("```json\n", 1)[1].split("\n```", 1)[0])
            self.assertEqual(bindings["DE67_POLICY_DECIDE_ARGV_JSON"], event["policy_argv"])
            self.assertEqual(bindings["DE67_LINEAGE"], "project")
            self.assertEqual(Path(bindings["DE67_DEADLINE_STATE"]).resolve(), self.state_path.resolve())
            if event["generation"] is not None:
                self.assertEqual(bindings["DE67_COORDINATOR_ACK_ARGV_JSON"], event["ack_argv"])
        # A separate tool process can execute the supplied route without DE67 environment inheritance.
        policy_dir = self.workspace / ".de67"
        policy_dir.mkdir(exist_ok=True)
        (policy_dir / "phase3-policy.d67").write_bytes(
            (SCRIPTS.parent / "assets/environment/phase3-policy.d67").read_bytes()
        )
        tool_environment = {key: value for key, value in os.environ.items() if not key.startswith("DE67_")}
        probe = subprocess.run(bindings["DE67_POLICY_DECIDE_ARGV_JSON"], env=tool_environment,
                               cwd=self.workspace, capture_output=True, text=True)
        self.assertEqual(probe.returncode, 0, probe.stderr)
        self.assertIn("action", json.loads(probe.stdout))
        self.assertNotIn("Before spawning each worker", initial_prompt)

        with DeadlineHarness(self.state_path) as harness:
            restart = harness.list_tasks()["coordinator_restart"]
        self.assertEqual(restart["generation"], 2)
        self.assertFalse(restart_required(restart))
        self.assertEqual(restart["run_id"], "generation-2-run")
        self.assertEqual(
            self.statuses(),
            {
                "initial-run": "DONE",
                "generation-1-run": "DONE",
                "generation-2-run": "DONE",
            },
        )
        self.assertNotIn("RUNNING", self.statuses().values())

    def test_valid_baton_continues_after_any_child_exit_code(self) -> None:
        self.write_work_documents(red=True, active=True)
        run_ids = iter(("retiring-run", "successor-run"))

        result = run_supervisor(
            self.state_path,
            "project",
            self.workspace,
            self.runner_command(),
            self.run_root,
            extra_env=self.environment("restart-then-exit-nonzero"),
            run_id_factory=lambda _generation: next(run_ids),
        )

        self.assertEqual(result, 0)
        self.assertEqual(
            [event["run_id"] for event in self.read_events()],
            ["retiring-run", "successor-run"],
        )
        self.assertEqual(
            self.statuses(),
            {"retiring-run": "FAILED", "successor-run": "DONE"},
        )

    def test_fresh_workspace_initializes_clock_before_first_coordinator(self) -> None:
        self.state_path.unlink()
        run_ids = iter(
            ("fresh-initial", "fresh-generation-1", "fresh-generation-2")
        )

        result = run_supervisor(
            self.state_path,
            "fresh-project",
            self.workspace,
            self.runner_command(),
            self.run_root,
            extra_env=self.environment("two-restarts"),
            run_id_factory=lambda _generation: next(run_ids),
        )

        self.assertEqual(result, 0)
        with DeadlineHarness(self.state_path) as harness:
            summary = harness.list_tasks()
        self.assertEqual(summary["lineage_id"], "fresh-project")
        self.assertFalse(restart_required(summary["coordinator_restart"]))

    def test_all_green_empty_work_returns_before_runner(self) -> None:
        self.write_work_documents()
        with DeadlineHarness(self.state_path) as harness:
            harness.complete_task("project", "seed", "green", now=time.time())

        result = run_supervisor(
            self.state_path,
            "project",
            self.workspace,
            self.runner_command(),
            self.run_root,
            extra_env=self.environment("unacknowledged"),
        )

        self.assertEqual(result, 0)
        self.assertFalse(self.events.exists())
        self.assertFalse(self.run_root.exists())

    def test_blocked_only_ledger_is_audited_once_then_stops(self) -> None:
        self.write_work_documents(red=True)
        ledger = self.workspace / ".de67" / "work-ledger.md"
        ledger.write_text(
            "# Work ledger\n\n## Active work\n\n"
            "## Blocked work\n\n"
            "- Blocked: R-001 \N{EM DASH} Owner choice required\n",
            encoding="utf-8",
        )

        self.assertTrue(ledger_has_only_blocked_work(self.workspace))
        audit_reason = blocked_ledger_audit_reason(self.workspace)
        self.assertIsNotNone(audit_reason)
        result = run_supervisor(
            self.state_path,
            "project",
            self.workspace,
            self.runner_command(),
            self.run_root,
            extra_env=self.environment("confirm-blocked"),
            run_id_factory=lambda _generation: "blocked-audit",
        )

        self.assertEqual(result, 0)
        self.assertEqual(
            [event["generation"] for event in self.read_events()],
            [1],
        )
        with DeadlineHarness(self.state_path) as harness:
            restart = harness.coordinator_restart_status("project")[
                "coordinator_restart"
            ]
        self.assertEqual(restart["reason"], audit_reason)
        self.assertFalse(restart_required(restart))
        with DeadlineHarness(self.state_path) as harness:
            tasks = harness.list_tasks()["tasks"]
        self.assertFalse(any(task["state"] == "running" for task in tasks))

        second_run_root = self.root / "second-runs"
        second_result = run_supervisor(
            self.state_path,
            "project",
            self.workspace,
            self.runner_command(),
            second_run_root,
            extra_env=self.environment("unacknowledged"),
        )

        self.assertEqual(second_result, 0)
        self.assertFalse(second_run_root.exists())
        self.assertEqual(len(self.read_events()), 1)

    def test_authenticated_blocker_reply_starts_one_fresh_coordinator(self) -> None:
        self.write_work_documents(red=True)
        ledger = self.workspace / ".de67" / "work-ledger.md"
        ledger.write_text(
            "# Work ledger\n\n## Active work\n\n"
            "## Blocked work\n\n"
            "- Blocked: R-001 \N{EM DASH} Owner choice required\n",
            encoding="utf-8",
        )
        with DeadlineHarness(self.state_path) as harness:
            harness.complete_task("project", "seed", "prior proof")

        replies = [
            BlockerReply(
                "digest",
                "notice-1",
                "reply-1",
                "Use the clean recovery route.",
                "owner",
            )
        ]

        def wait_for_owner_reply(*, workspace: Path, lineage_id: str) -> BlockerReply | None:
            self.assertEqual(workspace, self.workspace.resolve())
            self.assertEqual(lineage_id, "project")
            return replies.pop(0) if replies else None

        run_ids = iter(["blocked-audit", "owner-reply"])
        result = run_supervisor(
            self.state_path,
            "project",
            self.workspace,
            self.runner_command(),
            self.run_root,
            extra_env=self.environment("blocked-reply-complete"),
            run_id_factory=lambda _generation: next(run_ids),
            blocker_waiter=wait_for_owner_reply,
        )

        self.assertEqual(result, 0)
        self.assertEqual(
            [event["generation"] for event in self.read_events()],
            [1, 2],
        )
        with DeadlineHarness(self.state_path) as harness:
            restart = harness.coordinator_restart_status("project")[
                "coordinator_restart"
            ]
        self.assertEqual(
            restart["reason"], "owner replied through blocker adapter reply-1"
        )
        self.assertFalse(restart_required(restart))

    def test_recoverable_lifecycle_matrix_converges_after_child_exit(self) -> None:
        ledger_cases = {
            "active": "# Work ledger\n\n## Active work\n\n- [ ] R-001 \N{EM DASH} Work\n",
            "empty": "# Work ledger\n\n## Active work\n",
            "malformed": "# Work ledger\n\n## Active work\n\n- maybe R-001\n",
        }
        for ledger_name, ledger_text in ledger_cases.items():
            for first_exit in (0, 9):
                with self.subTest(ledger=ledger_name, first_exit=first_exit):
                    with tempfile.TemporaryDirectory() as case_directory:
                        case_root = Path(case_directory)
                        workspace = case_root / "workspace"
                        state_root = workspace / ".de67"
                        state_root.mkdir(parents=True)
                        state = case_root / "deadlines.sqlite3"
                        runs = case_root / "runs"
                        events = case_root / "events.jsonl"
                        (state_root / "FS.md").write_text(
                            "# FS\n\nStatus: Frozen\n\n- [ ] \N{LARGE RED CIRCLE} R-001 \N{EM DASH} Open\n",
                            encoding="utf-8",
                        )
                        (state_root / "work-ledger.md").write_text(
                            ledger_text,
                            encoding="utf-8",
                        )
                        with DeadlineHarness(state) as harness:
                            harness.start_task(
                                "project", "seed", "R-000", 3600, now=time.time()
                            )
                        run_ids = iter(("first", "recovery"))

                        result = run_supervisor(
                            state,
                            "project",
                            workspace,
                            self.runner_command(),
                            runs,
                            extra_env={
                                **self.environment("matrix-recover"),
                                "FAKE_EVENTS": str(events),
                                "FAKE_FIRST_EXIT": str(first_exit),
                            },
                            run_id_factory=lambda _generation: next(run_ids),
                        )

                        self.assertEqual(result, 0)
                        recorded = [
                            json.loads(line)
                            for line in events.read_text(encoding="utf-8").splitlines()
                        ]
                        self.assertEqual(len(recorded), 2)
                        self.assertIsNone(recorded[0]["generation"])
                        self.assertIsNone(recorded[1]["generation"])
                        self.assertEqual(recorded[1]["resume_session"], "fake-session")
                        self.assertEqual(
                            {
                                path.parent.name: path.read_text(encoding="utf-8").strip()
                                for path in runs.glob("*/status.txt")
                            },
                            {
                                "first": "DONE" if first_exit == 0 else "FAILED",
                                "recovery": "DONE",
                            },
                        )

    def test_active_item_outranks_blocked_items(self) -> None:
        self.write_work_documents(red=True, active=True)
        ledger = self.workspace / ".de67" / "work-ledger.md"
        with ledger.open("a", encoding="utf-8") as output:
            output.write(
                "\n## Blocked work\n\n"
                "- Blocked: R-002 \N{EM DASH} Different owner choice\n"
            )

        self.assertFalse(ledger_has_only_blocked_work(self.workspace))

    def test_green_documents_cannot_hide_an_open_named_closure_gap(self) -> None:
        self.write_work_documents()
        with DeadlineHarness(self.state_path) as harness:
            harness.complete_task("project", "seed", "strategy known", now=time.time())
            harness.transition_claim_to_closure(
                "project",
                "R-000",
                "seed",
                "Prove the product outcome.",
                "Run the owner route.",
                gaps=[("G-001", "Owner-route proof remains.", "Run owner route.")],
                now=time.time(),
            )

        self.assertFalse(work_is_complete(self.workspace, self.state_path, "project"))

    def test_final_wave_exits_without_manufacturing_a_successor(self) -> None:
        self.write_work_documents(red=True, active=True)

        result = run_supervisor(
            self.state_path,
            "project",
            self.workspace,
            self.runner_command(),
            self.run_root,
            extra_env=self.environment("complete-program"),
            run_id_factory=lambda _generation: "final-wave-run",
        )

        self.assertEqual(result, 0)
        self.assertEqual(len(self.read_events()), 1)
        with DeadlineHarness(self.state_path) as harness:
            restart = harness.coordinator_restart_status("project")[
                "coordinator_restart"
            ]
        self.assertIsNone(restart)

    def test_empty_ledger_with_red_dfs_still_launches(self) -> None:
        self.write_work_documents(red=True)
        with DeadlineHarness(self.state_path) as harness:
            harness.complete_task("project", "seed", "green", now=time.time())
        run_ids = iter(("red-work-run", "red-work-successor"))

        result = run_supervisor(
            self.state_path,
            "project",
            self.workspace,
            self.runner_command(),
            self.run_root,
            extra_env=self.environment("handover-then-complete"),
            run_id_factory=lambda _generation: next(run_ids),
        )

        self.assertEqual(result, 0)
        self.assertEqual(len(self.read_events()), 2)

    def test_empty_replenishment_keeps_same_coordinator_session(self) -> None:
        self.write_work_documents(red=True, active=True)
        run_ids = iter(("draining-run", "empty-replenishment"))

        result = run_supervisor(
            self.state_path,
            "project",
            self.workspace,
            self.runner_command(),
            self.run_root,
            extra_env=self.environment("drain-then-no-refill"),
            run_id_factory=lambda _generation: next(run_ids),
        )

        self.assertEqual(result, 0)
        events = self.read_events()
        self.assertEqual(len(events), 2)
        self.assertIsNone(events[0]["generation"])
        self.assertIsNone(events[1]["generation"])
        self.assertEqual(events[1]["resume_session"], "fake-session")
        with DeadlineHarness(self.state_path) as harness:
            restart = harness.coordinator_restart_status("project")[
                "coordinator_restart"
            ]
        self.assertIsNone(restart)

    def test_crashed_cli_with_active_work_resumes_the_same_coordinator(self) -> None:
        self.write_work_documents(red=True, active=True)
        run_ids = iter(("crashed-run", "recovery-run"))

        result = run_supervisor(
            self.state_path,
            "project",
            self.workspace,
            self.runner_command(),
            self.run_root,
            extra_env=self.environment("crash-then-complete"),
            run_id_factory=lambda _generation: next(run_ids),
        )

        self.assertEqual(result, 0)
        self.assertEqual(
            [event["run_id"] for event in self.read_events()],
            ["crashed-run", "recovery-run"],
        )
        events = self.read_events()
        self.assertIsNone(events[0]["generation"])
        self.assertIsNone(events[1]["generation"])
        self.assertEqual(events[1]["resume_session"], "fake-session")
        with DeadlineHarness(self.state_path) as harness:
            self.assertIsNone(
                harness.coordinator_restart_status("project")["coordinator_restart"]
            )
        self.assertEqual(
            self.statuses(),
            {"crashed-run": "FAILED", "recovery-run": "DONE"},
        )

    def test_three_failed_decisions_stop_without_a_fourth_launch(self) -> None:
        self.write_work_documents(red=True, active=True)
        run_ids = iter((
            "normal-decision",
            "corrective-decision",
            "fresh-final-decision",
            "forbidden-fourth-run",
        ))

        result = run_supervisor(
            self.state_path,
            "project",
            self.workspace,
            self.runner_command(),
            self.run_root,
            extra_env=self.environment("crash-three-times"),
            run_id_factory=lambda _generation: next(run_ids),
        )

        self.assertEqual(result, 9)
        events = self.read_events()
        self.assertEqual(
            [event["run_id"] for event in events],
            ["normal-decision", "corrective-decision", "fresh-final-decision"],
        )
        self.assertEqual(events[1]["resume_session"], "fake-session")
        self.assertIsNone(events[2]["resume_session"])
        error = (
            self.run_root / "fresh-final-decision" / "supervisor_error.txt"
        ).read_text(encoding="utf-8")
        self.assertIn("Three coordinator decision opportunities were exhausted", error)

        corrective_prompt = (
            self.run_root / "corrective-decision" / "prompt.txt"
        ).read_text(encoding="utf-8")
        self.assertIn("decision opportunity 2 of 3", corrective_prompt)
        self.assertIn("Do not open a replacement task", corrective_prompt)
        self.assertIn("spawn_agent", corrective_prompt)
        self.assertIn("Durably close or block", corrective_prompt)
        self.assertIn("SQL schemas, queries, migrations", corrective_prompt)
        self.assertIn("SQLite-backed harness transitions", corrective_prompt)
        self.assertIn("FS red lamps:\n- [ ] 🔴 R-001 — Open", corrective_prompt)
        self.assertIn(
            "Ledger executable entries:\n- [ ] R-001 — Current route",
            corrective_prompt,
        )
        self.assertIn(
            "If either list is nonempty, Phase 3 is unfinished", corrective_prompt
        )
        self.assertIn("Trust your own causal judgment", corrective_prompt)
        self.assertIn("repair the edge case", corrective_prompt)
        self.assertIn("rerun the policy decision", corrective_prompt)

        final_prompt = (
            self.run_root / "fresh-final-decision" / "prompt.txt"
        ).read_text(encoding="utf-8")
        self.assertIn("decision opportunity 3 of 3", final_prompt)
        self.assertIn("final automatic opportunity", final_prompt)
        self.assertIn("Trust your own causal judgment", final_prompt)

    def test_successful_pass_resets_the_consecutive_decision_fuse(self) -> None:
        self.write_work_documents(red=True, active=True)
        run_ids = iter(("failed", "successful", "new-1", "new-2", "new-3", "forbidden"))

        result = run_supervisor(
            self.state_path,
            "project",
            self.workspace,
            self.runner_command(),
            self.run_root,
            extra_env=self.environment("successful-pass-resets-decision-fuse"),
            run_id_factory=lambda _generation: next(run_ids),
        )

        self.assertEqual(result, 9)
        self.assertEqual(
            [event["run_id"] for event in self.read_events()],
            ["failed", "successful", "new-1", "new-2", "new-3"],
        )
        normal_prompt = (self.run_root / "new-1" / "prompt.txt").read_text(
            encoding="utf-8"
        )
        self.assertNotIn("decision opportunity 2 of 3", normal_prompt)

    def test_crashed_coordinator_preserves_claimed_worker_and_resumes_once(self) -> None:
        self.write_work_documents(red=True, active=True)
        run_ids = iter(("worker-owner-crashed", "worker-result-ingress"))

        with patch(
            "coordinator_supervisor.runtime_worker_owners",
            return_value={"worker-a": "fake-session"},
        ):
            result = run_supervisor(
                self.state_path,
                "project",
                self.workspace,
                self.runner_command(),
                self.run_root,
                extra_env=self.environment("claimed-worker-crash-then-complete"),
                run_id_factory=lambda _generation: next(run_ids),
            )

        self.assertEqual(result, 0)
        events = self.read_events()
        self.assertEqual(len(events), 2)
        self.assertEqual(events[1]["resume_session"], "fake-session")
        with DeadlineHarness(self.state_path) as harness:
            task = harness.status_task("project", "seed")
        self.assertEqual(task["state"], "completed")
        self.assertEqual(
            self.statuses(),
            {"worker-owner-crashed": "FAILED", "worker-result-ingress": "DONE"},
        )

    def test_nonresumable_crash_with_active_work_gets_a_fresh_coordinator(self) -> None:
        self.write_work_documents(red=True, active=True)
        run_ids = iter(("crashed-run", "recovery-run"))

        result = run_supervisor(
            self.state_path,
            "project",
            self.workspace,
            self.runner_command(),
            self.run_root,
            extra_env=self.environment("crash-without-session-then-complete"),
            run_id_factory=lambda _generation: next(run_ids),
        )

        self.assertEqual(result, 0)
        events = self.read_events()
        self.assertIsNone(events[0]["generation"])
        self.assertIsNone(events[1]["generation"])
        self.assertIsNone(events[1]["resume_session"])
        with DeadlineHarness(self.state_path) as harness:
            restart = harness.coordinator_restart_status("project")[
                "coordinator_restart"
            ]
        self.assertIsNone(restart)

    def test_missing_runner_is_a_concrete_environment_blocker(self) -> None:
        self.write_work_documents(red=True, active=True)

        result = run_supervisor(
            self.state_path,
            "project",
            self.workspace,
            [str(self.root / "missing-runner")],
            self.run_root,
            run_id_factory=lambda _generation: "missing-runner",
        )

        self.assertEqual(result, 1)
        self.assertFalse(self.events.exists())
        self.assertEqual(self.statuses(), {"missing-runner": "FAILED"})

    def test_active_ledger_resumes_same_session_without_restart(self) -> None:
        self.write_work_documents(active=True)
        with DeadlineHarness(self.state_path) as harness:
            harness.complete_task("project", "seed", "green", now=time.time())

        run_ids = iter(("active-ledger-initial", "active-ledger-continuation"))
        result = run_supervisor(
            self.state_path,
            "project",
            self.workspace,
            self.runner_command(),
            self.run_root,
            extra_env=self.environment("handover-then-complete"),
            run_id_factory=lambda _generation: next(run_ids),
        )

        self.assertEqual(result, 0)
        events = self.read_events()
        self.assertEqual(len(events), 2)
        self.assertIsNone(events[0]["generation"])
        self.assertIsNone(events[1]["generation"])
        self.assertEqual(events[1]["resume_session"], "fake-session")
        with DeadlineHarness(self.state_path) as harness:
            restart = harness.coordinator_restart_status("project")[
                "coordinator_restart"
            ]
        self.assertIsNone(restart)
        continuation_prompt = (
            self.run_root / "active-ledger-continuation" / "prompt.txt"
        ).read_text(encoding="utf-8")
        self.assertNotIn("orchestrator-guidelines.md", continuation_prompt)
        # The same session retains its original contracts; continuation adds only the next action.
        self.assertNotIn(live_coordination_contract(), continuation_prompt)
        self.assertNotIn(worker_result_ingress_contract(), continuation_prompt)
        self.assertIn("DE67_POLICY_DECIDE_ARGV_JSON", continuation_prompt)

    def test_active_clock_prevents_completion(self) -> None:
        self.write_work_documents()

        result = run_supervisor(
            self.state_path,
            "project",
            self.workspace,
            self.runner_command(),
            self.run_root,
            extra_env=self.environment("unacknowledged"),
            run_id_factory=lambda _generation: "active-clock-run",
        )

        self.assertEqual(result, 0)
        self.assertEqual(len(self.read_events()), 1)

    def test_reopened_unaccepted_claim_prevents_green_dfs_completion(self) -> None:
        self.write_work_documents()
        base = time.time()
        with DeadlineHarness(self.state_path) as harness:
            harness.complete_task("project", "seed", "explored", now=base)
            harness.transition_claim_to_closure(
                "project",
                "R-000",
                "seed",
                "The owner route returns the finished outcome.",
                "Run the owner route and inspect its artifact.",
                "Only closure remains.",
                now=base + 1,
            )
            harness.start_task(
                "project", "closure", "R-000", 3600,
                phase="closure", now=base + 2
            )
            harness.complete_task(
                "project", "closure", "closure proved", now=base + 3
            )
            harness.accept_claim(
                "project", "R-000", "closure", "accepted proof", now=base + 4
            )
            harness.start_task(
                "project", "closure-check", "R-000", 3600,
                phase="closure", now=base + 5
            )
            harness.report_worker_finding(
                "project",
                "closure-check",
                "unexpected",
                "The owner route returns the finished outcome premise is false.",
                now=base + 6,
            )
            harness.reopen_claim_exploration(
                "project",
                "R-000",
                "closure-check",
                "The owner route returns the finished outcome",
                now=base + 7,
            )
            harness.start_task(
                "project", "reexplore", "R-000", 3600, now=base + 8
            )
            harness.complete_task(
                "project", "reexplore", "replacement strategy proved",
                now=base + 9
            )

        self.assertFalse(work_is_complete(self.workspace, self.state_path, "project"))
        result = run_supervisor(
            self.state_path,
            "project",
            self.workspace,
            self.runner_command(),
            self.run_root,
            extra_env=self.environment("unacknowledged"),
            run_id_factory=lambda _generation: "reopened-claim-run",
        )

        self.assertEqual(result, 0)
        self.assertEqual(len(self.read_events()), 1)

    def test_pending_restart_prevents_completion(self) -> None:
        self.write_work_documents()
        with DeadlineHarness(self.state_path) as harness:
            harness.complete_task("project", "seed", "green", now=time.time())
            harness.request_coordinator_restart("project", "guarded mutation")

        result = run_supervisor(
            self.state_path,
            "project",
            self.workspace,
            self.runner_command(),
            self.run_root,
            extra_env=self.environment("ack"),
            run_id_factory=lambda _generation: "pending-restart-run",
        )

        self.assertEqual(result, 0)
        events = self.read_events()
        self.assertEqual(len(events), 1)
        self.assertEqual(events[0]["generation"], 1)

    def test_pending_incident_prevents_completion(self) -> None:
        self.write_work_documents()
        with DeadlineHarness(self.state_path) as harness:
            harness.complete_task("project", "seed", "late green", now=time.time() + 7200)

        result = run_supervisor(
            self.state_path,
            "project",
            self.workspace,
            self.runner_command(),
            self.run_root,
            extra_env=self.environment("deadline-mutation-lifecycle"),
            run_id_factory=lambda _generation: "incident-run",
        )

        self.assertEqual(result, 0)
        events = self.read_events()
        self.assertEqual(len(events), 2)
        self.assertEqual([event["role"] for event in events], [
            "mutation-reviewer", "coordinator",
        ])

    def test_pending_integrity_mutation_prevents_completion(self) -> None:
        self.write_work_documents()
        with DeadlineHarness(self.state_path) as harness:
            harness.complete_task("project", "seed", "green", now=time.time())
            summary = harness.list_tasks()
        summary["tasks"] = [
            {"task_id": "seed", "claim_id": "R-000", "state": "completed"}
        ]
        summary["pending_incident_reviews"] = []
        summary["pending_integrity_mutations"] = [
            {
                "task_id": "proof-breach",
                "claim_id": "R-000",
                "pending_components": ["micro", "macro"],
            }
        ]

        with patch.object(DeadlineHarness, "list_tasks", return_value=summary):
            self.assertFalse(
                work_is_complete(self.workspace, self.state_path, "project")
            )

    def test_final_completion_supersedes_random_improvement_due(self) -> None:
        self.write_work_documents()
        with DeadlineHarness(self.state_path) as harness:
            harness.connection.execute(
                """
                UPDATE random_mutation_cycles
                SET interval_windows = 10, due_after_terminal_windows = 10
                WHERE lineage_id = 'project' AND cycle_number = 1
                """
            )
            harness.connection.commit()
            harness.complete_task("project", "seed", "green", now=time.time())
            for number in range(2, 11):
                task_id = f"terminal-{number}"
                dispatched_at = time.time()
                harness.start_task(
                    "project",
                    task_id,
                    f"R-{number:03d}",
                    3600,
                    now=dispatched_at,
                )
                result = harness.complete_task(
                    "project", task_id, "green", now=dispatched_at + 1
                )
        self.assertTrue(result["random_mutation"]["due"])

        supervised = run_supervisor(
            self.state_path,
            "project",
            self.workspace,
            self.runner_command(),
            self.run_root,
            extra_env=self.environment("unacknowledged"),
        )

        self.assertEqual(supervised, 0)
        self.assertFalse(self.events.exists())

    def test_acknowledgement_argv_preserves_hostile_valid_values(self) -> None:
        hostile_root = self.root / "state and runs [literal]"
        state = hostile_root / "deadline state.sqlite3"
        workspace = self.root / "workspace & code [literal]"
        workspace.mkdir()
        run_root = hostile_root / "coordinator runs"
        lineage = "project & branch [literal]"
        with DeadlineHarness(state) as harness:
            generation = harness.request_coordinator_restart(
                lineage, "path quoting proof"
            )["coordinator_restart"]["generation"]

        result = run_supervisor(
            state,
            lineage,
            workspace,
            self.runner_command(),
            run_root,
            extra_env=self.environment("ack-from-json"),
            run_id_factory=lambda _generation: "hostile-values-run",
        )

        self.assertEqual(
            result,
            0,
            Path(str(self.events) + ".ack").read_text(encoding="utf-8"),
        )
        event = self.read_events()[0]
        self.assertEqual(
            event["ack_argv"],
            [
                sys.executable,
                str((SCRIPTS / "deadline_harness.py").resolve()),
                "ack-restart",
                "--state",
                str(state.resolve()),
                "--lineage",
                lineage,
                "--generation",
                str(generation),
                "--run-id",
                "hostile-values-run",
            ],
        )

    def test_unacknowledged_successor_gets_three_opportunities_then_stops(self) -> None:
        generation = self.request_restart()
        run_ids = iter(("ack-1", "ack-2", "ack-3", "forbidden-4"))

        result = run_supervisor(
            self.state_path,
            "project",
            self.workspace,
            self.runner_command(),
            self.run_root,
            extra_env=self.environment("unacknowledged"),
            run_id_factory=lambda _generation: next(run_ids),
        )

        self.assertEqual(result, 1)
        events = self.read_events()
        self.assertEqual(len(events), 3)
        self.assertEqual([event["generation"] for event in events], [generation] * 3)
        self.assertEqual(events[1]["resume_session"], "fake-session")
        self.assertIsNone(events[2]["resume_session"])
        self.assertEqual(events[0]["ppid"], os.getpid())
        with DeadlineHarness(self.state_path) as harness:
            restart = harness.list_tasks()["coordinator_restart"]
        self.assertTrue(restart_required(restart))
        self.assertEqual(restart["generation"], generation)
        self.assertIsNone(restart["run_id"])
        self.assertEqual(restart["expected_run_id"], "ack-3")
        self.assertEqual(
            self.statuses(), {"ack-1": "FAILED", "ack-2": "FAILED", "ack-3": "FAILED"}
        )
        self.assertNotIn("RUNNING", self.statuses().values())

    def test_unacknowledged_successor_can_acknowledge_on_second_opportunity(self) -> None:
        self.write_work_documents(red=True, active=True)
        generation = self.request_restart()
        run_ids = iter(("missed-ack", "recovered-ack", "forbidden-third"))

        result = run_supervisor(
            self.state_path,
            "project",
            self.workspace,
            self.runner_command(),
            self.run_root,
            extra_env=self.environment("ack-on-second-opportunity"),
            run_id_factory=lambda _generation: next(run_ids),
        )

        self.assertEqual(result, 0)
        events = self.read_events()
        self.assertEqual([event["run_id"] for event in events], ["missed-ack", "recovered-ack"])
        self.assertEqual([event["generation"] for event in events], [generation, generation])
        prompt = (self.run_root / "recovered-ack" / "prompt.txt").read_text(
            encoding="utf-8"
        )
        self.assertIn("decision opportunity 2 of 3", prompt)
        self.assertIn("DE67_COORDINATOR_ACK_ARGV_JSON", prompt)

    def test_failed_successor_gets_three_opportunities_and_stays_pending(self) -> None:
        generation = self.request_restart()
        run_ids = iter(("failed-1", "failed-2", "failed-3", "forbidden-4"))

        result = run_supervisor(
            self.state_path,
            "project",
            self.workspace,
            self.runner_command(),
            self.run_root,
            extra_env=self.environment("fail-before-ack"),
            run_id_factory=lambda _generation: next(run_ids),
        )

        self.assertEqual(result, 7)
        events = self.read_events()
        self.assertEqual(len(events), 3)
        self.assertEqual([event["generation"] for event in events], [generation] * 3)
        with DeadlineHarness(self.state_path) as harness:
            restart = harness.list_tasks()["coordinator_restart"]
        self.assertTrue(restart_required(restart))
        self.assertEqual(restart["generation"], generation)
        self.assertEqual(restart["expected_run_id"], "failed-3")
        self.assertEqual(
            self.statuses(),
            {"failed-1": "FAILED", "failed-2": "FAILED", "failed-3": "FAILED"},
        )
        self.assertEqual(
            (self.run_root / "failed-3" / "exit_code.txt")
            .read_text(encoding="utf-8")
            .strip(),
            "7",
        )
        self.assertNotIn("RUNNING", self.statuses().values())

    def test_second_supervisor_is_rejected_before_launch(self) -> None:
        lock_state = self.state_path.resolve()
        with _supervisor_lock(lock_state):
            with self.assertRaisesRegex(SupervisorError, "already owns"):
                run_supervisor(
                    self.state_path,
                    "project",
                    self.workspace,
                    self.runner_command(),
                    self.run_root,
                    extra_env=self.environment("two-restarts"),
                )

        self.assertFalse(self.events.exists())
        self.assertFalse(self.run_root.exists())


if __name__ == "__main__":
    unittest.main()
