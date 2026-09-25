from __future__ import annotations

import json
import sqlite3
import sys
import tempfile
import time
import unittest
from pathlib import Path
from unittest.mock import patch


SCRIPTS = Path(__file__).resolve().parents[1] / "scripts"
sys.path.insert(0, str(SCRIPTS))

import codex_runner  # noqa: E402


class FakeProcess:
    def __init__(self, lines: list[str], exit_code: int) -> None:
        self.stdout = iter(lines)
        self.exit_code = exit_code
        self.terminated = False
        self.killed = False
        self.wait_calls = 0

    def wait(self) -> int:
        self.wait_calls += 1
        return self.exit_code

    def kill(self) -> None:
        self.killed = True

    def terminate(self) -> None:
        self.terminated = True


class CodexRunnerTests(unittest.TestCase):
    def test_transport_failure_is_retained_in_runner_status(self):
        reason = 'Codex App Server connection closed: keepalive ping timeout'
        process = FakeProcess([json.dumps({'type': 'de67.transport_error', 'message': reason}) + '\n'], 2)
        with patch('codex_runner.subprocess.Popen', return_value=process), \
                patch('codex_runner.shutil.which', return_value='/fake/codex'):
            with self.assertRaisesRegex(codex_runner.RunnerError, 'keepalive ping timeout'):
                codex_runner.run(self.workspace, 'Run assigned work.', environment=self.environment())
        status = json.loads(next((self.root / 'runs').glob('*/status.json')).read_text())
        self.assertEqual(status['exit_code'], 2)
        self.assertEqual(status['error'], reason)

    def test_terminal_unbound_task_reconciles_without_weakening_live_handoff(self):
        terminal = {'completed-before-roster-observation'}
        guard = codex_runner.CoordinatorLoopGuard(
            initial_unbound_tasks=('completed-before-roster-observation', 'still-live'),
            task_terminal=lambda task: task in terminal,
        )
        guard.reconcile_handoffs()
        self.assertEqual(guard.unbound_tasks, ('still-live',))
        with self.assertRaises(codex_runner.RunnerError):
            guard.observe({'type': 'item.started', 'item': {
                'type': 'collab_tool_call', 'tool': 'wait', 'status': 'in_progress'}})
        terminal.add('still-live')
        guard.reconcile_handoffs()
        self.assertEqual(guard.unbound_tasks, ())

    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.workspace = self.root / "workspace"
        self.workspace.mkdir()

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def environment(self) -> dict[str, str]:
        return {
            "DE67_CODEX": "codex-test",
            "DE67_RUNNER_ROOT": str(self.root / "runs"),
            "DE67_COORDINATOR_RUN_ID": "run-1",
            "DE67_COORDINATOR_MODEL": "gpt-6-sol",
            "DE67_COORDINATOR_REASONING_EFFORT": "low",
        }

    def test_reviewer_receives_current_maintenance_guidance_with_original_gate(self):
        from coordinator_supervisor import mutation_maintenance_contract
        prefix = f"Act as the exclusive Phase-3 mutation reviewer in {self.workspace}.\n"
        gate = "Resolve durable random gate cycle 9; no coordinator may start here.\n"
        bindings = '\nCurrent invocation bindings (use these values directly; exact):\n{"run":"unchanged"}'
        old = prefix + gate + bindings
        env = {"DE67_PROCESS_ROLE": "mutation-reviewer"}
        refreshed = codex_runner.current_coordinator_prompt(self.workspace, old, env)
        self.assertIn(gate, refreshed)
        self.assertTrue(refreshed.endswith(bindings))
        self.assertEqual(refreshed.count(mutation_maintenance_contract()), 1)
        self.assertEqual(codex_runner.current_coordinator_prompt(self.workspace, refreshed, env), refreshed)
        self.assertEqual(codex_runner.current_coordinator_prompt(self.workspace, "Custom review", env), "Custom review")

    def test_fresh_runner_replaces_loaded_supervisor_guidance_and_preserves_bindings(self) -> None:
        from deadline_harness import DeadlineHarness
        from coordinator_supervisor import coordinator_prompt, worker_selection_contract
        state = self.root / "state.sqlite3"
        with DeadlineHarness(state) as harness:
            restart = harness.request_coordinator_restart("project", "Owner outcome remains intact.")["coordinator_restart"]
        env = dict(DE67_PROCESS_ROLE="coordinator", DE67_DEADLINE_STATE=str(state),
                   DE67_LINEAGE="project", DE67_COORDINATOR_RUN_ID="fresh",
                   DE67_COORDINATOR_RESTART_GENERATION=str(restart["generation"]))
        bindings = "\nCurrent invocation bindings (use these values directly; exact array)\nBOUND-ARGUMENTS"
        recovery = "\nRecovery: this is coordinator decision opportunity 2 of 3.\nPreserve unfinished work."
        old = f"Act as a fresh Phase-3 delivery coordinator in {self.workspace}.\nSTALE ROLE GUIDANCE" + recovery + bindings
        refreshed = codex_runner.current_coordinator_prompt(self.workspace, old, env)
        self.assertNotIn("STALE ROLE GUIDANCE", refreshed)
        self.assertIn(worker_selection_contract(), refreshed)
        self.assertIn("context_library.py", refreshed)
        self.assertIn("Owner outcome remains intact.", refreshed)
        self.assertIn(recovery, refreshed)
        self.assertTrue(refreshed.endswith(bindings))
        with DeadlineHarness(state) as harness:
            observed = harness.coordinator_restart_status("project")["coordinator_restart"]
        self.assertTrue(observed["pending"])
        self.assertIsNone(observed["acknowledged_at"])
        for extra in ({"DE67_PROCESS_ROLE": "mutation-reviewer"}, {"DE67_COORDINATOR_RESUME_SESSION": "same-session"}):
            self.assertEqual(codex_runner.current_coordinator_prompt(self.workspace, old, {**env, **extra}), old)
        self.assertEqual(codex_runner.current_coordinator_prompt(self.workspace, "Custom prompt", env), "Custom prompt")
        with self.assertRaisesRegex(codex_runner.RunnerError, "invocation bindings"):
            codex_runner.current_coordinator_prompt(self.workspace, old.split(bindings)[0], env)

    def test_runner_uses_supervisor_model_and_records_auditable_result(self) -> None:
        captured: dict[str, object] = {}

        def launch(command: list[str], **options: object) -> FakeProcess:
            captured["command"] = command
            captured["options"] = options
            return FakeProcess(
                [
                    '{"type":"thread.started","thread_id":"session-1"}\n',
                    '{"type":"turn.started"}\n',
                ],
                0,
            )

        with patch("codex_runner.shutil.which", return_value="/tools/codex"), patch(
            "codex_runner.subprocess.Popen", side_effect=launch
        ):
            result = codex_runner.run(
                self.workspace, "coordinate this\n", environment=self.environment()
            )

        self.assertEqual(result, 0)
        command = captured["command"]
        self.assertEqual(command[0:2], ["/tools/codex", "exec"])
        self.assertIn("--json", command)
        self.assertIn("gpt-6-sol", command)
        self.assertIn("model_reasoning_effort=low", command)
        run_directory = next((self.root / "runs").iterdir())
        self.assertEqual(
            (run_directory / "prompt.txt").read_text(encoding="utf-8"),
            "coordinate this\n",
        )
        self.assertIn("turn.started", (run_directory / "events.jsonl").read_text())
        status = json.loads((run_directory / "status.json").read_text())
        self.assertEqual(status["status"], "done")
        self.assertEqual(status["model"], "gpt-6-sol")
        self.assertEqual(status["session_id"], "session-1")

    def test_runner_ignores_json_primitives_in_merged_diagnostic_output(self) -> None:
        lines = [
            "apply_patch verification failed:\n",
            '  "cockpit:run.finish"\n',
            '{"type":"thread.started","thread_id":"session-after-diagnostic"}\n',
        ]
        with patch("codex_runner.shutil.which", return_value="codex"), patch(
            "codex_runner.subprocess.Popen", return_value=FakeProcess(lines, 0)
        ):
            self.assertEqual(
                codex_runner.run(
                    self.workspace, "coordinate this", environment=self.environment()
                ),
                0,
            )

        run_directory = next((self.root / "runs").iterdir())
        self.assertEqual(
            json.loads((run_directory / "status.json").read_text())["session_id"],
            "session-after-diagnostic",
        )
        self.assertIn(
            '"cockpit:run.finish"',
            (run_directory / "events.jsonl").read_text(encoding="utf-8"),
        )

    def test_runner_resumes_the_exact_coordinator_session(self) -> None:
        captured: dict[str, object] = {}
        environment = self.environment()
        environment["DE67_COORDINATOR_RESUME_SESSION"] = "session-1"

        def launch(command: list[str], **options: object) -> FakeProcess:
            captured["command"] = command
            return FakeProcess(
                ['{"type":"thread.started","thread_id":"session-1"}\n'], 0
            )

        with patch("codex_runner.shutil.which", return_value="/tools/codex"), patch(
            "codex_runner.subprocess.Popen", side_effect=launch
        ):
            self.assertEqual(
                codex_runner.run(self.workspace, "continue\n", environment=environment),
                0,
            )

        command = captured["command"]
        self.assertEqual(
            command[0:5],
            ["/tools/codex", "exec", "--sandbox", "danger-full-access", "resume"],
        )
        self.assertIn("session-1", command)
        self.assertNotIn("--skip-git-repo-check", command)

    def test_resume_reasserts_selected_sandbox_instead_of_using_global_default(self) -> None:
        environment = self.environment()
        environment["DE67_COORDINATOR_RESUME_SESSION"] = "session-1"
        environment["DE67_COORDINATOR_SANDBOX"] = "workspace-write"

        command = codex_runner._command("/tools/codex", self.workspace, environment)

        self.assertEqual(
            command[0:5],
            ["/tools/codex", "exec", "--sandbox", "workspace-write", "resume"],
        )

    def test_runner_propagates_codex_failure(self) -> None:
        with patch("codex_runner.shutil.which", return_value="codex"), patch(
            "codex_runner.subprocess.Popen", return_value=FakeProcess([], 7)
        ):
            result = codex_runner.run(
                self.workspace, "coordinate this", environment=self.environment()
            )
        self.assertEqual(result, 7)
        run_directory = next((self.root / "runs").iterdir())
        status = json.loads((run_directory / "status.json").read_text())
        self.assertEqual(status["status"], "failed")
        self.assertEqual(status["exit_code"], 7)

    def test_runner_rejects_missing_cli_and_empty_prompt(self) -> None:
        with patch("codex_runner.shutil.which", return_value=None):
            with self.assertRaisesRegex(codex_runner.RunnerError, "was not found"):
                codex_runner.run(
                    self.workspace, "coordinate this", environment=self.environment()
                )
        with self.assertRaisesRegex(codex_runner.RunnerError, "non-empty prompt"):
            codex_runner.run(self.workspace, "  ", environment=self.environment())

    def test_runner_stops_and_abandons_exact_phantom_handoff_trace(self) -> None:
        started = {
            "type": "item.completed",
            "item": {
                "type": "command_execution",
                "command": "python3 deadline_harness.py start --task R-008-closure-003",
                "aggregated_output": json.dumps(
                    {
                        "attempt_created": True,
                        "state": "running",
                        "task_id": "R-008-closure-003",
                    }
                ),
                "exit_code": 0,
                "status": "completed",
            },
        }
        waited = {
            "type": "item.started",
            "item": {
                "type": "collab_tool_call",
                "tool": "wait",
                "receiver_thread_ids": [],
                "agents_states": {},
                "status": "in_progress",
            },
        }
        process = FakeProcess([json.dumps(started) + "\n", json.dumps(waited) + "\n"], 0)

        def abandon_after_reap(*_arguments: object) -> None:
            self.assertTrue(process.killed)
            self.assertEqual(process.wait_calls, 1)

        with patch("codex_runner.shutil.which", return_value="codex"), patch(
            "codex_runner.subprocess.Popen", return_value=process
        ), patch(
            "codex_runner._abandon_unbound_tasks", side_effect=abandon_after_reap
        ) as abandon:
            with self.assertRaisesRegex(codex_runner.RunnerError, "no roster worker"):
                codex_runner.run(
                    self.workspace, "coordinate this", environment=self.environment()
                )

        self.assertTrue(process.killed)
        abandon.assert_called_once()
        self.assertEqual(abandon.call_args.args[0], ("R-008-closure-003",))
        run_directory = next((self.root / "runs").iterdir())
        status = json.loads((run_directory / "status.json").read_text())
        self.assertEqual(status["status"], "failed")
        self.assertEqual(status["exit_code"], 2)
        self.assertIn("no roster worker", status["error"])

    def test_runner_reaps_launched_child_before_publishing_io_failure(self) -> None:
        process = FakeProcess(
            ['{"type":"thread.started","thread_id":"session-1"}\n'], 0
        )
        with patch("codex_runner.shutil.which", return_value="codex"), patch(
            "codex_runner.subprocess.Popen", return_value=process
        ), patch("codex_runner._record_session", side_effect=OSError("write failed")):
            with self.assertRaisesRegex(codex_runner.RunnerError, "write failed"):
                codex_runner.run(
                    self.workspace, "coordinate this", environment=self.environment()
                )

        self.assertTrue(process.killed)
        self.assertEqual(process.wait_calls, 1)
        run_directory = next((self.root / "runs").iterdir())
        status = json.loads((run_directory / "status.json").read_text())
        self.assertEqual(status["status"], "failed")

    def write_roster_state(
        self,
        model: str | None,
        *,
        task_id: str = "route",
        agent_path: str | None = None,
        created_at: float | None = None,
    ) -> Path:
        state = self.root / "codex-state.sqlite3"
        rollout = self.root / "coordinator-rollout.jsonl"
        expected_path = agent_path or f"/root/{codex_runner.worker_task_name(task_id)}"
        created = created_at if created_at is not None else time.time() + 60
        spawn_payload = {
                "type": "item_completed",
                "item": {
                    "type": "SubAgentActivity",
                    "kind": "started",
                    "agent_thread_id": "worker",
                    "agent_path": expected_path,
                },
                "started_at_ms": int(created * 1000),
        }
        rollout.write_text(
            json.dumps(
                {
                    "timestamp": "2099-01-01T00:00:00Z",
                    "type": "event_msg",
                    "payload": spawn_payload,
                }
            )
            + "\n",
            encoding="utf-8",
        )
        connection = sqlite3.connect(state)
        connection.executescript(
            """
            CREATE TABLE thread_spawn_edges (
                parent_thread_id TEXT, child_thread_id TEXT, status TEXT
            );
            CREATE TABLE threads (
                id TEXT, model TEXT, cwd TEXT, rollout_path TEXT, agent_path TEXT,
                created_at_ms INTEGER, created_at INTEGER,
                updated_at_ms INTEGER, updated_at INTEGER
            );
            """
        )
        connection.execute(
            "INSERT INTO thread_spawn_edges VALUES ('coordinator', 'worker', 'open')"
        )
        connection.execute(
            "INSERT INTO threads VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)",
            (
                "worker",
                model,
                str(self.workspace.resolve()),
                "",
                expected_path,
                int(created * 1000),
                int(created),
                int((created + 60) * 1000),
                int(created + 60),
            ),
        )
        connection.execute(
            "INSERT INTO threads VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)",
            (
                "coordinator", "gpt-6-sol", str(self.workspace.resolve()),
                str(rollout), "/root", int((created - 60) * 1000), int(created - 60),
                int(created * 1000), int(created),
            ),
        )
        connection.commit()
        connection.close()
        return state

    def handoff_trace(self) -> list[str]:
        started = {
            "type": "item.completed",
            "item": {
                "type": "command_execution",
                "aggregated_output": json.dumps(
                    {"attempt_created": True, "state": "running", "task_id": "route"}
                ),
                "exit_code": 0,
                "status": "completed",
            },
        }
        waited = {
            "type": "item.started",
            "item": {
                "type": "collab_tool_call",
                "tool": "wait",
                "receiver_thread_ids": [],
                "agents_states": {},
                "status": "in_progress",
            },
        }
        return [
            json.dumps({"type": "thread.started", "thread_id": "coordinator"}) + "\n",
            json.dumps(started) + "\n",
            json.dumps(
                {
                    "type": "item.completed",
                    "item": {
                        "type": "collab_tool_call",
                        "tool": "spawn_agent",
                        "status": "completed",
                        "receiver_thread_ids": [],
                    },
                }
            )
            + "\n",
            json.dumps(waited) + "\n",
        ]

    def test_runner_accepts_real_luna_child_lineage_before_empty_wait(self) -> None:
        environment = self.environment()
        environment["DE67_CODEX_STATE"] = str(self.write_roster_state("gpt-6-luna"))
        with patch("codex_runner.shutil.which", return_value="codex"), patch(
            "codex_runner.subprocess.Popen",
            return_value=FakeProcess(self.handoff_trace(), 0),
        ):
            self.assertEqual(
                codex_runner.run(self.workspace, "coordinate", environment=environment), 0
            )

    def test_runner_recovers_spawn_omitted_from_public_event_stream(self) -> None:
        environment = self.environment()
        environment["DE67_CODEX_STATE"] = str(
            self.write_roster_state("gpt-6-sol")
        )
        trace = [line for line in self.handoff_trace() if '"spawn_agent"' not in line]
        with patch("codex_runner.shutil.which", return_value="codex"), patch(
            "codex_runner.subprocess.Popen", return_value=FakeProcess(trace, 0)
        ):
            self.assertEqual(
                codex_runner.run(self.workspace, "coordinate", environment=environment), 0
            )

    def test_runner_accepts_exact_spawn_when_duplicate_model_metadata_is_missing(self) -> None:
        environment = self.environment()
        environment["DE67_CODEX_STATE"] = str(self.write_roster_state(None))
        trace = [line for line in self.handoff_trace() if '"spawn_agent"' not in line]
        with patch("codex_runner.shutil.which", return_value="codex"), patch(
            "codex_runner.subprocess.Popen", return_value=FakeProcess(trace, 0)
        ):
            self.assertEqual(
                codex_runner.run(self.workspace, "coordinate", environment=environment), 0
            )

    def test_roster_validator_accepts_exact_child_with_missing_model_metadata(self) -> None:
        environment = self.environment()
        environment["DE67_CODEX_STATE"] = str(self.write_roster_state(None))
        validate = codex_runner._roster_validator(self.workspace.resolve(), environment)
        self.assertTrue(validate("worker", "coordinator"))

    def test_helper_cannot_take_task_and_named_worker_binds_out_of_order(self) -> None:
        environment = self.environment()
        state = self.write_roster_state(None)
        environment["DE67_CODEX_STATE"] = str(state)
        connection = sqlite3.connect(state)
        try:
            connection.execute("UPDATE threads SET agent_path='/root/discovery' WHERE id='worker'")
            connection.commit()
        finally:
            connection.close()
        claims = []
        guard = codex_runner.CoordinatorLoopGuard(
            initial_unbound_tasks=("other-route", "route"),
            roster_validator=codex_runner._roster_validator(self.workspace.resolve(), environment),
            claim_recorder=lambda task, worker, parent: claims.append((task, worker)),
        )
        guard.observe({"type": "thread.started", "thread_id": "coordinator"})
        event = {"type": "item.completed", "item": {
            "type": "collab_tool_call", "tool": "spawn_agent", "status": "completed",
            "receiver_thread_ids": ["worker"]}}
        guard.observe(event)
        self.assertEqual(claims, [])
        self.assertEqual(guard.unbound_tasks, ("other-route", "route"))
        connection = sqlite3.connect(state)
        try:
            connection.execute("UPDATE threads SET agent_path=? WHERE id='worker'",
                               ("/root/" + codex_runner.worker_task_name("route"),))
            connection.commit()
        finally:
            connection.close()
        guard.observe(event)
        self.assertEqual(claims, [("route", "worker")])
        self.assertEqual(guard.unbound_tasks, ("other-route",))

    def test_roster_recovery_rejects_worker_for_different_task_name(self) -> None:
        environment = self.environment()
        environment["DE67_CODEX_STATE"] = str(
            self.write_roster_state(
                "gpt-6-sol", task_id="other-route", agent_path="/root/other_route"
            )
        )
        resolver = codex_runner._roster_resolver(self.workspace, environment)
        self.assertIsNone(resolver("route", time.time(), "coordinator", frozenset()))

    def test_worker_task_names_do_not_collapse_distinct_valid_ids(self) -> None:
        self.assertNotEqual(
            codex_runner.worker_task_name("a-b"),
            codex_runner.worker_task_name("a_b"),
        )
        self.assertEqual(codex_runner.worker_task_name("!!!"), "task_212121")

    def test_roster_recovery_rejects_stale_worker_even_if_recently_updated(self) -> None:
        environment = self.environment()
        started_at = time.time()
        environment["DE67_CODEX_STATE"] = str(
            self.write_roster_state(
                "gpt-6-sol", created_at=started_at - 60
            )
        )
        resolver = codex_runner._roster_resolver(self.workspace, environment)
        self.assertIsNone(resolver("route", started_at, "coordinator", frozenset()))

    def test_runner_does_not_police_model_after_exact_worker_handoff(self) -> None:
        environment = self.environment()
        environment["DE67_CODEX_STATE"] = str(self.write_roster_state("gpt-6-sol"))
        with patch("codex_runner.shutil.which", return_value="codex"), patch(
            "codex_runner.subprocess.Popen",
            return_value=FakeProcess(self.handoff_trace(), 0),
        ):
            self.assertEqual(
                codex_runner.run(self.workspace, "coordinate", environment=environment), 0
            )

    def test_loop_guard_rejects_symbolic_task_name_as_worker_identity(self) -> None:
        guard = codex_runner.CoordinatorLoopGuard(
            initial_unbound_tasks=("R-008-closure-088",)
        )
        guard.observe(
            {
                "type": "item.completed",
                "item": {
                    "type": "collab_tool_call",
                    "tool": "spawn_agent",
                    "status": "completed",
                    "receiver_thread_ids": ["/root/r008_closure_088"],
                },
            }
        )

        guard.observe(
            {
                "type": "item.started",
                "item": {"type": "collab_tool_call", "tool": "wait"},
            }
        )
        self.assertEqual(guard.unbound_tasks, ("R-008-closure-088",))

    def test_loop_guard_requires_runtime_roster_proof_for_receiver_id(self) -> None:
        claims: list[tuple[str, str, str | None]] = []
        guard = codex_runner.CoordinatorLoopGuard(
            initial_unbound_tasks=("route",),
            roster_validator=lambda _worker, _parent, _task: False,
            claim_recorder=lambda task, worker, parent: claims.append(
                (task, worker, parent)
            ),
        )
        guard.observe({"type": "thread.started", "thread_id": "coordinator"})
        guard.observe(
            {
                "type": "item.completed",
                "item": {
                    "type": "collab_tool_call",
                    "tool": "spawn_agent",
                    "status": "completed",
                    "receiver_thread_ids": ["fake-worker"],
                },
            }
        )

        self.assertEqual(claims, [])
        guard.observe(
            {
                "type": "item.started",
                "item": {"type": "collab_tool_call", "tool": "wait"},
            }
        )
        self.assertEqual(guard.unbound_tasks, ("route",))

    def test_loop_guard_allows_runtime_roster_visibility_to_arrive_during_wait(self) -> None:
        visible = False
        claims: list[tuple[str, str, str | None]] = []

        def resolve(
            _task: str,
            _started: float,
            _parent: str | None,
            _used: frozenset[str],
        ) -> str | None:
            return "terra-worker" if visible else None

        guard = codex_runner.CoordinatorLoopGuard(
            initial_unbound_tasks=("route",),
            roster_resolver=resolve,
            claim_recorder=lambda task, worker, parent: claims.append(
                (task, worker, parent)
            ),
        )
        guard.observe({"type": "thread.started", "thread_id": "coordinator"})
        guard.observe(
            {
                "type": "item.completed",
                "item": {
                    "type": "collab_tool_call",
                    "tool": "spawn_agent",
                    "status": "completed",
                    "receiver_thread_ids": [],
                },
            }
        )
        wait = {
            "type": "item.started",
            "item": {"type": "collab_tool_call", "tool": "wait"},
        }

        guard.observe(wait)
        self.assertEqual(guard.unbound_tasks, ("route",))
        visible = True
        guard.observe({"type": "turn.completed"})

        self.assertEqual(guard.unbound_tasks, ())
        self.assertEqual(claims, [("route", "terra-worker", "coordinator")])

    def test_loop_guard_allows_wait_with_bound_workers_and_one_capacity_queued_task(self) -> None:
        claims: list[tuple[str, str, str | None]] = []
        guard = codex_runner.CoordinatorLoopGuard(
            initial_unbound_tasks=("route-1", "route-2", "route-3"),
            roster_validator=lambda _worker, _parent, _task: True,
            claim_recorder=lambda task, worker, parent: claims.append(
                (task, worker, parent)
            ),
        )
        guard.observe({"type": "thread.started", "thread_id": "coordinator"})
        for worker in ("terra-1", "terra-2"):
            guard.observe(
                {
                    "type": "item.completed",
                    "item": {
                        "type": "collab_tool_call",
                        "tool": "spawn_agent",
                        "status": "completed",
                        "receiver_thread_ids": [worker],
                    },
                }
            )

        wait = {
            "type": "item.started",
            "item": {"type": "collab_tool_call", "tool": "wait"},
        }
        guard.observe(wait)
        self.assertEqual(guard.unbound_tasks, ("route-3",))

        guard.observe(
            {
                "type": "item.completed",
                "item": {
                    "type": "collab_tool_call",
                    "tool": "spawn_agent",
                    "status": "completed",
                    "receiver_thread_ids": ["terra-3"],
                },
            }
        )

        self.assertEqual(guard.unbound_tasks, ())
        self.assertEqual(
            claims,
            [
                ("route-1", "terra-1", "coordinator"),
                ("route-2", "terra-2", "coordinator"),
                ("route-3", "terra-3", "coordinator"),
            ],
        )

    def test_conversation_and_followup_preserve_bound_worker_and_queued_task(self) -> None:
        claims = []
        guard = codex_runner.CoordinatorLoopGuard(
            initial_unbound_tasks=("running", "queued"),
            roster_validator=lambda _worker, _parent, _task: True,
            claim_recorder=lambda task, worker, parent: claims.append((task, worker, parent)),
        )
        guard.observe({"type": "thread.started", "thread_id": "coordinator"})
        guard.observe({"type": "item.completed", "item": {
            "type": "collab_tool_call", "tool": "spawn_agent", "status": "completed",
            "receiver_thread_ids": ["terra-worker"],
        }})
        for tool in ("send_message", "wait", "followup_task", "send_message"):
            guard.observe({"type": "item.completed", "item": {
                "type": "collab_tool_call", "tool": tool, "status": "completed",
                "receiver_thread_ids": ["terra-worker"],
                "agents_states": {"terra-worker": {"completed": "Partial result; game still running"}},
            }})
        self.assertEqual(claims, [("running", "terra-worker", "coordinator")])
        self.assertEqual(guard.unbound_tasks, ("queued",))
        self.assertEqual(guard._task_workers, {"running": "terra-worker"})

    def test_resumed_runner_binds_durable_worker_without_new_spawn_timestamp(self) -> None:
        claims: list[tuple[str, str, str | None]] = []
        guard = codex_runner.CoordinatorLoopGuard(
            initial_unbound_tasks=("route",),
            initial_pending_delegations=("route",),
            recovered_workers={"route": "old-worker"},
            roster_validator=lambda worker, parent, _task: (
                worker == "old-worker" and parent == "coordinator"
            ),
            claim_recorder=lambda task, worker, parent: claims.append(
                (task, worker, parent)
            ),
        )

        guard.observe({"type": "thread.started", "thread_id": "coordinator"})

        self.assertEqual(guard.unbound_tasks, ())
        self.assertEqual(claims, [])

    def test_loop_guard_reuses_worker_after_authoritative_prior_terminal(self) -> None:
        terminal_tasks: set[str] = set()
        resolved_used_workers: list[frozenset[str]] = []

        def resolve(_task: str, _started: float, _parent: str | None,
                    used_workers: frozenset[str]) -> str:
            resolved_used_workers.append(used_workers)
            return "terra-worker"

        guard = codex_runner.CoordinatorLoopGuard(
            initial_unbound_tasks=("route-1",),
            initial_pending_delegations=("route-1",),
            roster_resolver=resolve,
            task_terminal=lambda task_id: task_id in terminal_tasks,
        )
        wait = {"type": "item.started", "item": {"type": "collab_tool_call", "tool": "wait"}}
        guard.observe(wait)
        terminal_tasks.add("route-1")
        guard.observe({
            "type": "item.completed",
            "item": {
                "type": "command_execution",
                "aggregated_output": json.dumps(
                    {"attempt_created": True, "state": "running", "task_id": "route-2"}
                ),
                "exit_code": 0,
                "status": "completed",
            },
        })
        guard.observe(
            {
                "type": "item.completed",
                "item": {
                    "type": "collab_tool_call",
                    "tool": "followup_task",
                    "status": "completed",
                    "receiver_thread_ids": [],
                },
            }
        )
        guard.observe(wait)

        self.assertEqual(resolved_used_workers, [frozenset(), frozenset()])
        self.assertEqual(guard.unbound_tasks, ())

    def test_task_terminal_resolver_reads_authoritative_deadline_state(self) -> None:
        state = self.root / "deadlines.sqlite3"
        connection = sqlite3.connect(state)
        connection.execute(
            "CREATE TABLE tasks (lineage_id TEXT, task_id TEXT, attempt_terminal_at REAL)"
        )
        connection.executemany(
            "INSERT INTO tasks VALUES (?, ?, ?)",
            [("project", "done", 12.0), ("project", "running", None)],
        )
        connection.commit()
        connection.close()
        resolve = codex_runner._task_terminal_resolver({
            "DE67_DEADLINE_STATE": str(state), "DE67_LINEAGE": "project"
        })

        self.assertTrue(resolve("done"))
        self.assertFalse(resolve("running"))
        self.assertFalse(resolve("missing"))

    def test_runner_reaps_child_when_claim_recording_raises_sqlite_error(self) -> None:
        process = FakeProcess(['{"type":"turn.started"}\n'], 0)
        environment = self.environment()
        environment["DE67_DEADLINE_STATE"] = str(self.root / "deadlines.sqlite3")
        environment["DE67_LINEAGE"] = "project"
        with patch("codex_runner.shutil.which", return_value="codex"), patch(
            "codex_runner.subprocess.Popen", return_value=process
        ), patch(
            "codex_runner._initial_unbound_tasks", return_value=("route",)
        ), patch(
            "codex_runner.CoordinatorLoopGuard.observe",
            side_effect=sqlite3.OperationalError("database is locked"),
        ), patch("codex_runner._abandon_unbound_tasks") as abandon:
            with self.assertRaisesRegex(codex_runner.RunnerError, "database is locked"):
                codex_runner.run(
                    self.workspace, "coordinate this", environment=environment
                )

        self.assertTrue(process.killed)
        self.assertEqual(process.wait_calls, 1)
        abandon.assert_called_once_with(("route",), environment)
        run_directory = next((self.root / "runs").iterdir())
        status = json.loads((run_directory / "status.json").read_text())
        self.assertEqual(status["status"], "failed")

    def test_claim_and_abandonment_failures_still_publish_primary_failure(self) -> None:
        process = FakeProcess(['{"type":"turn.started"}\n'], 0)
        environment = self.environment()
        with patch("codex_runner.shutil.which", return_value="codex"), patch(
            "codex_runner.subprocess.Popen", return_value=process
        ), patch(
            "codex_runner._initial_unbound_tasks", return_value=("route",)
        ), patch(
            "codex_runner.CoordinatorLoopGuard.observe",
            side_effect=sqlite3.OperationalError("claim database is locked"),
        ), patch(
            "codex_runner._abandon_unbound_tasks",
            side_effect=sqlite3.OperationalError("abandon database is locked"),
        ):
            with self.assertRaisesRegex(
                codex_runner.RunnerError, "claim database is locked"
            ):
                codex_runner.run(
                    self.workspace, "coordinate this", environment=environment
                )

        self.assertTrue(process.killed)
        self.assertEqual(process.wait_calls, 1)
        run_directory = next((self.root / "runs").iterdir())
        status = json.loads((run_directory / "status.json").read_text())
        self.assertEqual(status["status"], "failed")
        self.assertIn("claim database is locked", status["error"])
        self.assertIn("abandon database is locked", status["cleanup_error"])

    def test_reviewer_exit_does_not_enforce_or_abandon_coordinator_handoffs(self) -> None:
        environment = {**self.environment(), "DE67_PROCESS_ROLE": "mutation-reviewer"}
        process = FakeProcess([
            '{"type":"thread.started","thread_id":"reviewer"}\n',
            '{"type":"turn.completed"}\n',
        ], 0)
        with patch("codex_runner.shutil.which", return_value="codex"), patch(
            "codex_runner.subprocess.Popen", return_value=process
        ), patch("codex_runner._initial_unbound_tasks", return_value=("retained-sol-task",)) as initial, patch(
            "codex_runner._claim_recorder"
        ) as claim, patch("codex_runner._abandon_unbound_tasks") as abandon:
            self.assertEqual(codex_runner.run(
                self.workspace, "Review; preserve the returned Sol-owned task.",
                environment=environment,
            ), 0)
        initial.assert_not_called()
        claim.assert_not_called()
        abandon.assert_not_called()
        status = json.loads(next((self.root / "runs").glob("*/status.json")).read_text())
        self.assertEqual(status["status"], "done")

    def test_coordinator_exit_accepts_verified_owned_handoff(self) -> None:
        environment = {**self.environment(), "DE67_PROCESS_ROLE": "coordinator"}
        process = FakeProcess([
            '{"type":"thread.started","thread_id":"sol"}\n',
            '{"type":"turn.completed"}\n',
        ], 0)
        resolve = lambda task, started, parent, used: "worker" if parent == "sol" else None
        with patch("codex_runner.shutil.which", return_value="codex"), patch(
            "codex_runner.subprocess.Popen", return_value=process
        ), patch("codex_runner._initial_unbound_tasks", return_value=("owned-task",)), patch(
            "codex_runner._roster_resolver", return_value=resolve
        ), patch("codex_runner._claim_recorder", return_value=lambda *args: None), patch(
            "codex_runner._abandon_unbound_tasks"
        ) as abandon:
            self.assertEqual(codex_runner.run(self.workspace, "Continue", environment=environment), 0)
        abandon.assert_not_called()

    def test_runner_cannot_publish_success_with_an_unbound_attempt(self) -> None:
        process = FakeProcess([], 0)
        with patch("codex_runner.shutil.which", return_value="codex"), patch(
            "codex_runner.subprocess.Popen", return_value=process
        ), patch(
            "codex_runner._initial_unbound_tasks", return_value=("route",)
        ), patch("codex_runner._abandon_unbound_tasks") as abandon:
            with self.assertRaisesRegex(
                codex_runner.RunnerError, "still lacked a verified roster worker"
            ):
                codex_runner.run(
                    self.workspace, "coordinate this", environment=self.environment()
                )

        abandon.assert_called_once()
        run_directory = next((self.root / "runs").iterdir())
        status = json.loads((run_directory / "status.json").read_text())
        self.assertEqual(status["status"], "failed")
        self.assertEqual(status["exit_code"], 2)

    def test_abandonment_failure_after_child_exit_does_not_kill_reaped_process(self) -> None:
        process = FakeProcess([], 0)
        with patch("codex_runner.shutil.which", return_value="codex"), patch(
            "codex_runner.subprocess.Popen", return_value=process
        ), patch(
            "codex_runner._initial_unbound_tasks", return_value=("route",)
        ), patch(
            "codex_runner._abandon_unbound_tasks",
            side_effect=sqlite3.OperationalError("database is locked"),
        ):
            with self.assertRaisesRegex(codex_runner.RunnerError, "database is locked"):
                codex_runner.run(
                    self.workspace, "coordinate this", environment=self.environment()
                )

        self.assertFalse(process.killed)
        self.assertEqual(process.wait_calls, 1)


if __name__ == "__main__":
    unittest.main()
