#!/usr/bin/env python3
"""Behavioral tests for the real coordinator-to-worker handoff loop."""

from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path


SCRIPT_ROOT = Path(__file__).resolve().parents[1] / "scripts"
if str(SCRIPT_ROOT) not in sys.path:
    sys.path.insert(0, str(SCRIPT_ROOT))

from codex_runner import CoordinatorLoopGuard, RunnerError


def task_start(task_id: str) -> dict[str, object]:
    return {
        "type": "item.completed",
        "item": {
            "type": "command_execution",
            "command": f"python3 deadline_harness.py start --task {task_id}",
            "aggregated_output": json.dumps(
                {
                    "attempt_created": True,
                    "state": "running",
                    "task_id": task_id,
                }
            ),
            "exit_code": 0,
            "status": "completed",
        },
    }


def task_terminal(task_id: str, command: str = "complete") -> dict[str, object]:
    return {
        "type": "item.completed",
        "item": {
            "type": "command_execution",
            "command": f"python3 deadline_harness.py {command} --task {task_id}",
            "aggregated_output": json.dumps(
                {"task_id": task_id, "state": "completed", "attempt_completed": True}
            ),
            "exit_code": 0,
            "status": "completed",
        },
    }


def failed_task_start(task_id: str) -> dict[str, object]:
    event = task_start(task_id)
    event["item"]["exit_code"] = 2
    event["item"]["status"] = "failed"
    return event


def handoff(tool: str, worker_id: str, *, status: str = "completed") -> dict[str, object]:
    return {
        "type": "item.completed",
        "item": {
            "type": "collab_tool_call",
            "tool": tool,
            "receiver_thread_ids": [worker_id],
            "agents_states": {worker_id: {"status": "running"}},
            "status": status,
        },
    }


def wait_for(*worker_ids: str) -> dict[str, object]:
    return {
        "type": "item.started",
        "item": {
            "type": "collab_tool_call",
            "tool": "wait",
            "receiver_thread_ids": list(worker_ids),
            "agents_states": {
                worker_id: {"status": "running"} for worker_id in worker_ids
            },
            "status": "in_progress",
        },
    }


class CoordinatorLoopRuntimeTests(unittest.TestCase):
    def replay(
        self, *events: dict[str, object], initial_tasks: tuple[str, ...] = ()
    ) -> CoordinatorLoopGuard:
        guard = CoordinatorLoopGuard(initial_unbound_tasks=initial_tasks)
        for event in events:
            guard.observe(event)
        return guard

    def test_real_regression_running_task_then_empty_wait_is_rejected(self) -> None:
        with self.assertRaisesRegex(RunnerError, "R-008-closure-003.*no roster worker"):
            self.replay(task_start("R-008-closure-003"), wait_for())

    def test_fresh_worker_handoff_makes_wait_legal(self) -> None:
        self.replay(
            task_start("R-008-closure-003"),
            handoff("spawn_agent", "worker-luna"),
            wait_for("worker-luna"),
        )

    def test_successful_handoff_records_the_exact_task_worker_binding(self) -> None:
        claims: list[tuple[str, str, str | None]] = []
        guard = CoordinatorLoopGuard(claim_recorder=lambda *claim: claims.append(claim))

        guard.observe({"type": "thread.started", "thread_id": "coordinator-a"})
        guard.observe(task_start("route-a"))
        guard.observe(handoff("spawn_agent", "worker-a"))

        self.assertEqual(claims, [("route-a", "worker-a", "coordinator-a")])

    def test_failed_durable_claim_keeps_the_task_unbound(self) -> None:
        def reject(*_claim: object) -> None:
            raise RunnerError("durable claim rejected")

        guard = CoordinatorLoopGuard(claim_recorder=reject)
        guard.observe(task_start("route-a"))

        with self.assertRaisesRegex(RunnerError, "durable claim rejected"):
            guard.observe(handoff("spawn_agent", "worker-a"))
        self.assertEqual(guard.unbound_tasks, ("route-a",))

    def test_reused_worker_handoff_makes_wait_legal(self) -> None:
        self.replay(
            task_start("R-008-closure-004"),
            handoff("followup_task", "worker-terra"),
            wait_for("worker-terra"),
        )

    def test_failed_spawn_does_not_bind_the_task(self) -> None:
        with self.assertRaisesRegex(RunnerError, "R-008-closure-003.*no roster worker"):
            self.replay(
                task_start("R-008-closure-003"),
                handoff("spawn_agent", "worker-luna", status="failed"),
                wait_for(),
            )

    def test_previous_worker_cannot_satisfy_the_next_handoff(self) -> None:
        with self.assertRaisesRegex(RunnerError, "closure-004.*no roster worker"):
            self.replay(
                task_start("R-008-closure-003"),
                handoff("spawn_agent", "worker-luna"),
                wait_for("worker-luna"),
                task_terminal("R-008-closure-003"),
                task_start("R-008-closure-004"),
                wait_for(),
            )

    def test_parallel_windows_each_require_their_own_worker(self) -> None:
        self.replay(
            task_start("route-a"),
            handoff("spawn_agent", "worker-a"),
            task_start("route-b"),
            handoff("spawn_agent", "worker-b"),
            wait_for("worker-a", "worker-b"),
        )

    def test_parallel_second_window_stays_queued_while_first_worker_runs(self) -> None:
        guard = self.replay(
            task_start("route-a"),
            handoff("spawn_agent", "worker-a"),
            task_start("route-b"),
            wait_for("worker-a"),
        )
        self.assertEqual(guard.unbound_tasks, ("route-b",))

    def test_live_worker_followup_does_not_claim_queued_independent_work(self) -> None:
        claims: list[tuple[str, str, str | None]] = []
        guard = CoordinatorLoopGuard(claim_recorder=lambda *claim: claims.append(claim))
        guard.observe({"type": "thread.started", "thread_id": "coordinator-a"})
        guard.observe(task_start("playtest"))
        guard.observe(handoff("spawn_agent", "worker-player"))
        guard.observe(task_start("independent-repair"))

        # Live replies, idle-turn continuations, and receiver-less messages must
        # all preserve the current assignment and leave independent work queued.
        for tool, receiver in (
            ("send_message", "worker-player"),
            ("followup_task", "worker-player"),
            ("followup_task", ""),
        ):
            guard.observe(handoff(tool, receiver))
        guard.observe(wait_for("worker-player"))

        self.assertEqual(guard.unbound_tasks, ("independent-repair",))
        self.assertEqual(claims, [("playtest", "worker-player", "coordinator-a")])
        guard.observe(handoff("spawn_agent", "worker-repair"))
        self.assertEqual(guard.unbound_tasks, ())
        self.assertEqual(claims[-1], ("independent-repair", "worker-repair", "coordinator-a"))

    def test_receiverless_followup_cannot_stand_in_for_a_worker_handoff(self) -> None:
        with self.assertRaisesRegex(RunnerError, "repair.*no roster worker"):
            self.replay(
                task_start("playtest"),
                handoff("spawn_agent", "worker-player"),
                task_start("repair"),
                handoff("followup_task", ""),
                task_terminal("playtest"),
                wait_for(),
            )

    def test_resume_cannot_wait_on_an_unbound_durable_task(self) -> None:
        with self.assertRaisesRegex(RunnerError, "closure-004.*no roster worker"):
            self.replay(wait_for(), initial_tasks=("R-008-closure-004",))

    def test_idle_followup_without_receiver_can_await_roster_visibility(self) -> None:
        visible = False
        guard = CoordinatorLoopGuard(
            initial_unbound_tasks=("repair",),
            roster_resolver=lambda *_args: "worker-repair" if visible else None,
        )
        guard.observe(handoff("followup_task", ""))
        guard.observe(wait_for())
        self.assertEqual(guard.unbound_tasks, ("repair",))
        visible = True
        guard.reconcile_handoffs()
        self.assertEqual(guard.unbound_tasks, ())

    def test_mutation_retirement_may_terminalize_without_spawning(self) -> None:
        self.replay(
            task_start("route-before-mutation"),
            task_terminal("route-before-mutation", "abandon-attempt"),
        )

    def test_finding_terminalizes_the_worker_window_before_next_handoff(self) -> None:
        self.replay(
            task_start("route-finding"),
            handoff("spawn_agent", "worker-a"),
            wait_for("worker-a"),
            task_terminal("route-finding", "finding"),
            task_start("replacement"),
            handoff("spawn_agent", "worker-b"),
            wait_for("worker-b"),
        )

    def test_recoverable_return_can_continue_same_live_window(self) -> None:
        self.replay(
            task_start("route-repair"),
            handoff("spawn_agent", "worker-a"),
            wait_for("worker-a"),
            handoff("followup_task", "worker-a"),
            wait_for("worker-a"),
            task_terminal("route-repair"),
        )

    def test_abandonment_terminalizes_the_worker_window_before_replacement(self) -> None:
        self.replay(
            task_start("route-abandoned"),
            handoff("spawn_agent", "worker-a"),
            task_terminal("route-abandoned", "abandon-attempt"),
            task_start("replacement"),
            handoff("followup_task", "worker-a"),
            wait_for("worker-a"),
        )

    def test_failed_reuse_does_not_bind_the_task(self) -> None:
        with self.assertRaisesRegex(RunnerError, "replacement.*no roster worker"):
            self.replay(
                task_start("replacement"),
                handoff("followup_task", "worker-a", status="failed"),
                wait_for(),
            )

    def test_handoff_before_clock_start_cannot_prebind_future_work(self) -> None:
        with self.assertRaisesRegex(RunnerError, "future-route.*no roster worker"):
            self.replay(
                handoff("spawn_agent", "worker-a"),
                task_start("future-route"),
                wait_for("worker-a"),
            )

    def test_duplicate_handoff_does_not_prebind_the_queued_window(self) -> None:
        guard = self.replay(
            task_start("route-a"),
            handoff("spawn_agent", "worker-a"),
            handoff("spawn_agent", "worker-extra"),
            task_start("route-b"),
            wait_for("worker-a", "worker-extra"),
        )
        self.assertEqual(guard.unbound_tasks, ("route-b",))

    def test_failed_task_start_does_not_require_a_worker(self) -> None:
        self.replay(failed_task_start("never-created"), wait_for())

    def test_unrelated_command_output_cannot_create_a_worker_window(self) -> None:
        self.replay(
            {
                "type": "item.completed",
                "item": {
                    "type": "command_execution",
                    "command": "python3 unrelated.py",
                    "aggregated_output": json.dumps({"state": "running"}),
                    "exit_code": 0,
                    "status": "completed",
                },
            },
            wait_for(),
        )

    def test_resume_may_rebind_existing_task_before_wait(self) -> None:
        self.replay(
            handoff("followup_task", "worker-a"),
            wait_for("worker-a"),
            initial_tasks=("resumed-route",),
        )

    def test_one_handoff_leaves_the_other_resumed_task_queued(self) -> None:
        guard = self.replay(
            handoff("followup_task", "worker-a"),
            wait_for("worker-a"),
            initial_tasks=("resumed-a", "resumed-b"),
        )
        self.assertEqual(guard.unbound_tasks, ("resumed-b",))

    def test_wait_without_any_durable_task_is_not_a_phantom_worker_error(self) -> None:
        self.replay(wait_for())

    def test_exact_captured_regression_shape_is_rejected(self) -> None:
        captured_start = task_start("R-008-closure-003")
        captured_start["item"]["command"] = (
            "/Library/Developer/CommandLineTools/usr/bin/python3 "
            "/Users/example/.codex/skills/de67/de-67-3/scripts/deadline_harness.py "
            "start --state /tmp/deadlines.sqlite3 --lineage project "
            "--task R-008-closure-003 --claim R-008 --estimate-seconds 84000 "
            "--phase closure --gap R-008-production-matrix"
        )
        with self.assertRaisesRegex(RunnerError, "R-008-closure-003.*no roster worker"):
            self.replay(captured_start, wait_for())


if __name__ == "__main__":
    unittest.main()
