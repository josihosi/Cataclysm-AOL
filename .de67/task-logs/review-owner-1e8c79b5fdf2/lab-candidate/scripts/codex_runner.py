#!/usr/bin/env python3
"""Run one auditable DE-67 coordinator through the local Codex CLI."""

from __future__ import annotations

import argparse
import json
import os
import shutil
import sqlite3
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Callable, Sequence

from deadline_harness import DeadlineError, DeadlineHarness


class RunnerError(RuntimeError):
    """Raised when the local Codex runner cannot start safely."""


def worker_task_name(task_id: str) -> str:
    """Return the deterministic spawn label for one deadline task."""
    return f"task_{task_id.encode('utf-8').hex()}"


class CoordinatorLoopGuard:
    """Reject a coordinator wait only when no durable task has a worker handoff."""

    def __init__(
        self,
        *,
        initial_unbound_tasks: Sequence[str] = (),
        initial_pending_delegations: Sequence[str] = (),
        recovered_workers: dict[str, str] | None = None,
        roster_resolver: Callable[[str, float, str | None, frozenset[str]], str | None]
        | None = None,
        roster_validator: Callable[[str, str | None, str | None], bool] | None = None,
        claim_recorder: Callable[[str, str, str | None], None] | None = None,
        task_terminal: Callable[[str], bool] | None = None,
        clock: Callable[[], float] = time.time,
    ) -> None:
        self._clock = clock
        self._unbound = {
            task_id: clock() for task_id in dict.fromkeys(initial_unbound_tasks)
        }
        self._task_workers: dict[str, str] = {}
        self._pending_delegations: set[str] = set()
        self._pending_delegations.update(initial_pending_delegations)
        self._recovered_workers = recovered_workers or {}
        self._parent_thread_id: str | None = None
        self._roster_resolver = roster_resolver
        self._roster_validator = roster_validator
        self._claim_recorder = claim_recorder
        self._task_terminal = task_terminal

    def _reconcile_terminal_tasks(self) -> None:
        if self._task_terminal is None:
            return
        for task_id in tuple(self._task_workers):
            if self._task_terminal(task_id):
                self._task_workers.pop(task_id, None)
                self._pending_delegations.discard(task_id)

    def reconcile_handoffs(self) -> None:
        """Bind workers from an exact library claim or verified native spawn edge."""
        self._reconcile_terminal_tasks()
        if self._roster_resolver is None:
            return
        for task_id, started_at in tuple(self._unbound.items()):
            worker_id = self._roster_resolver(
                task_id,
                started_at,
                self._parent_thread_id,
                frozenset(self._task_workers.values()),
            )
            if worker_id:
                self._bind(task_id, worker_id)

    def _bind(self, task_id: str, worker_id: str, *, record_claim: bool = True) -> None:
        if worker_id.startswith("/") or any(character.isspace() for character in worker_id):
            return
        if worker_id in self._task_workers.values():
            return
        if record_claim and self._claim_recorder is not None:
            self._claim_recorder(task_id, worker_id, self._parent_thread_id)
        del self._unbound[task_id]
        self._pending_delegations.discard(task_id)
        self._task_workers[task_id] = worker_id

    @property
    def unbound_tasks(self) -> tuple[str, ...]:
        return tuple(self._unbound)

    @staticmethod
    def _command_result(item: dict[str, object]) -> dict[str, object] | None:
        if item.get("type") != "command_execution" or item.get("status") != "completed":
            return None
        if item.get("exit_code") not in (0, None):
            return None
        output = item.get("aggregated_output")
        if not isinstance(output, str):
            return None
        try:
            result = json.loads(output)
        except json.JSONDecodeError:
            return None
        return result if isinstance(result, dict) else None

    @staticmethod
    def _spawn_worker_task_ids(result: dict[str, object]) -> tuple[str, ...]:
        """Read authoritative unbound task ids from a spawn-worker decision."""
        if result.get("action") != "spawn_worker":
            return ()
        worker_spawns = result.get("worker_spawns")
        if not isinstance(worker_spawns, list):
            return ()
        task_ids: list[str] = []
        for spawn in worker_spawns:
            if not isinstance(spawn, dict):
                continue
            task_id = spawn.get("task_id")
            if isinstance(task_id, str) and task_id and task_id not in task_ids:
                task_ids.append(task_id)
        return tuple(task_ids)

    def observe(self, event: dict[str, object]) -> None:
        item = event.get("item")
        if event.get("type") == "thread.started":
            thread_id = event.get("thread_id")
            if isinstance(thread_id, str) and thread_id:
                self._parent_thread_id = thread_id
                for task_id, worker_id in tuple(self._recovered_workers.items()):
                    if (
                        task_id in self._unbound
                        and (
                            self._roster_validator is None
                            or self._roster_validator(worker_id, thread_id, None)
                        )
                    ):
                        self._bind(task_id, worker_id, record_claim=False)
                    self._recovered_workers.pop(task_id, None)
            return
        if not isinstance(item, dict):
            self.reconcile_handoffs()
            return

        result = self._command_result(item)
        if result is not None:
            for announced_task_id in self._spawn_worker_task_ids(result):
                if (
                    announced_task_id not in self._unbound
                    and announced_task_id not in self._task_workers
                ):
                    self._unbound[announced_task_id] = self._clock()
            task_id = result.get("task_id")
            if isinstance(task_id, str) and task_id:
                if result.get("attempt_created") is True and result.get("state") == "running":
                    if task_id not in self._unbound and task_id not in self._task_workers:
                        self._unbound[task_id] = self._clock()
                if result.get("attempt_completed") is True or result.get("state") in {
                    "completed", "finding", "abandoned"
                }:
                    if task_id in self._unbound:
                        del self._unbound[task_id]
                    self._pending_delegations.discard(task_id)
                    self._task_workers.pop(task_id, None)

        if item.get("type") != "collab_tool_call":
            self.reconcile_handoffs()
            return
        tool = item.get("tool")
        receivers = item.get("receiver_thread_ids")
        worker_ids = [value for value in receivers or [] if isinstance(value, str) and value]
        self._reconcile_terminal_tasks()
        # A follow-up may steer the worker's existing assignment. It is a new
        # delegation only when the receiver is free of a live task. A missing
        # receiver is ambiguous while another assignment remains live. A fresh
        # or idle-worker handoff may still await authoritative roster visibility.
        eligible_handoff = (
            tool in {"spawn_agent", "followup_task"}
            and (not worker_ids or worker_ids[0] not in self._task_workers.values())
            and (tool == "spawn_agent" or worker_ids or not self._task_workers)
        )
        successful_delegation = (
            eligible_handoff
            and event.get("type") == "item.completed"
            and item.get("status") == "completed"
            and self._unbound
        )
        if successful_delegation:
            task_id = next(
                (
                    candidate
                    for candidate in self._unbound
                    if candidate not in self._pending_delegations
                ),
                next(iter(self._unbound)),
            )
            self._pending_delegations.add(task_id)
        if successful_delegation and worker_ids:
            if self._roster_validator is not None:
                # Helpers share the parent's workspace but never own deadline tasks.
                # A new worker's deterministic label also permits out-of-order spawns.
                matches = [candidate for candidate in self._unbound
                           if self._roster_validator(worker_ids[0], self._parent_thread_id,
                                                     candidate if tool == "spawn_agent" else "")]
                if not matches:
                    return
                self._pending_delegations.discard(task_id)
                task_id = matches[0]
            self._bind(task_id, worker_ids[0])
            return
        if tool == "wait" and event.get("type") == "item.started":
            # Codex persists the spawn edge independently from the tool event.
            # A successful delegation can therefore become visible after the
            # first wait begins. Keep reconciling subsequent events and make
            # the process-exit boundary, not this visibility race, fail closed.
            self.reconcile_handoffs()
            missing = [
                task_id
                for task_id in self._unbound
                if task_id not in self._pending_delegations
            ]
            if missing and not (self._task_workers or self._pending_delegations):
                raise RunnerError(
                    f"Running task {', '.join(missing)} has no roster worker before wait"
                )
            return
        self.reconcile_handoffs()


def _timestamp() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def _reap(process: subprocess.Popen[str]) -> int:
    while True:
        try:
            return process.wait()
        except InterruptedError:
            continue
        except ChildProcessError:
            return process.returncode if process.returncode is not None else 1


def _run_id() -> str:
    return f"{datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')}-{os.getpid()}"


def _codex_executable(environment: dict[str, str]) -> str:
    requested = environment.get("DE67_CODEX", "codex").strip()
    if not requested:
        raise RunnerError("DE67_CODEX must not be empty")
    resolved = shutil.which(requested)
    if resolved is None:
        raise RunnerError(f"Codex CLI was not found: {requested}")
    return resolved


def _command(codex: str, workspace: Path, environment: dict[str, str]) -> list[str]:
    sandbox = environment.get("DE67_COORDINATOR_SANDBOX", "danger-full-access").strip()
    if sandbox not in {"read-only", "workspace-write", "danger-full-access"}:
        raise RunnerError(f"Unsupported Codex sandbox: {sandbox}")
    transport = environment.get("DE67_AGENT_TRANSPORT", "").strip()
    transport_python = environment.get("DE67_AGENT_TRANSPORT_PYTHON", sys.executable)
    if not transport:
        binding = workspace / ".de67" / "state" / "workspace.json"
        if binding.is_file():
            settings = json.loads(binding.read_text(encoding="utf-8"))
            transport = settings.get("agent_transport", "cli")
            transport_python = settings.get("agent_transport_python", transport_python)
    if transport == "app-server":
        if sys.platform == "win32":
            raise RunnerError("The optional App Server transport requires macOS or Linux")
        return [transport_python, str(Path(__file__).with_name("codex_app_server_runner.py")),
                "--codex", codex, "--cwd", str(workspace)]
    if transport not in {"", "cli"}:
        raise RunnerError(f"Unsupported agent transport: {transport}")
    resume_session = environment.get("DE67_COORDINATOR_RESUME_SESSION", "").strip()
    command = [codex, "exec", "--sandbox", sandbox]
    if resume_session:
        command.append("resume")
    command.extend(["--json"])
    if not resume_session:
        command.extend(
            [
                "--skip-git-repo-check",
                "-C",
                str(workspace),
            ]
        )
    if environment.get("DE67_COORDINATOR_RUN_ID"):
        model = environment.get("DE67_COORDINATOR_MODEL", "gpt-6-sol").strip()
        if model not in {"gpt-6-luna", "gpt-6-sol", "gpt-6-astra"}:
            raise RunnerError("Retired model: use GPT-6 Luna, Sol or Astra")
        effort = environment.get(
            "DE67_COORDINATOR_REASONING_EFFORT", "low"
        ).strip()
        if not model or not effort:
            raise RunnerError("Coordinator model and reasoning effort must not be empty")
        command.extend(["-m", model, "-c", f"model_reasoning_effort={effort}"])
    if resume_session:
        command.append(resume_session)
    command.append("-")
    return command


def _record_session(line: str, environment: dict[str, str]) -> str | None:
    try:
        event = json.loads(line)
    except json.JSONDecodeError:
        return None
    if not isinstance(event, dict):
        return None
    if event.get("type") != "thread.started":
        return None
    session_id = event.get("thread_id")
    if not isinstance(session_id, str) or not session_id.strip():
        return None
    target_value = environment.get("DE67_COORDINATOR_SESSION_FILE", "").strip()
    if target_value:
        target = Path(target_value).expanduser().resolve()
        temporary = target.with_suffix(target.suffix + ".tmp")
        temporary.write_text(session_id.strip() + "\n", encoding="utf-8")
        temporary.replace(target)
    return session_id.strip()


def _initial_unbound_tasks(environment: dict[str, str]) -> tuple[str, ...]:
    state_value = environment.get("DE67_DEADLINE_STATE", "").strip()
    lineage = environment.get("DE67_LINEAGE", "").strip()
    if not state_value or not lineage:
        return ()
    state = Path(state_value).expanduser().resolve()
    if not state.is_file():
        return ()
    connection = sqlite3.connect(f"file:{state}?mode=ro", uri=True)
    try:
        rows = connection.execute(
            """
            SELECT task_id FROM tasks
            WHERE lineage_id = ? AND attempt_terminal_at IS NULL
            ORDER BY started_at
            """,
            (lineage,),
        ).fetchall()
    finally:
        connection.close()
    return tuple(str(row[0]) for row in rows)


def _initial_recovered_workers(environment: dict[str, str]) -> dict[str, str]:
    """Recover durable worker identities owned by the resumed coordinator session."""
    session = environment.get("DE67_COORDINATOR_RESUME_SESSION", "").strip()
    state_value = environment.get("DE67_DEADLINE_STATE", "").strip()
    lineage = environment.get("DE67_LINEAGE", "").strip()
    if not session or not state_value or not lineage:
        return {}
    state = Path(state_value).expanduser().resolve()
    if not state.is_file():
        return {}
    with sqlite3.connect(f"file:{state}?mode=ro", uri=True) as connection:
        rows = connection.execute(
            """
            SELECT task.task_id, claim.worker_id
            FROM tasks AS task
            JOIN worker_claims AS claim
              ON claim.lineage_id = task.lineage_id
             AND claim.task_id = task.task_id
            WHERE task.lineage_id = ? AND task.attempt_terminal_at IS NULL
              AND claim.released_at IS NULL
              AND claim.coordinator_session_id = ?
            ORDER BY task.started_at
            """,
            (lineage, session),
        ).fetchall()
    return {str(task_id): str(worker_id) for task_id, worker_id in rows}


def _task_terminal_resolver(environment: dict[str, str]) -> Callable[[str], bool]:
    state_value = environment.get("DE67_DEADLINE_STATE", "").strip()
    lineage = environment.get("DE67_LINEAGE", "").strip()
    state = Path(state_value).expanduser().resolve() if state_value else None

    def resolve(task_id: str) -> bool:
        if state is None or not lineage or not state.is_file():
            return False
        connection = sqlite3.connect(f"file:{state}?mode=ro", uri=True)
        try:
            row = connection.execute(
                "SELECT attempt_terminal_at FROM tasks WHERE lineage_id = ? AND task_id = ?",
                (lineage, task_id),
            ).fetchone()
        finally:
            connection.close()
        return row is not None and row[0] is not None

    return resolve


def _roster_resolver(
    workspace: Path, environment: dict[str, str]
) -> Callable[[str, float, str | None, frozenset[str]], str | None]:
    state_value = environment.get("DE67_CODEX_STATE", "").strip()
    state = (
        Path(state_value).expanduser().resolve()
        if state_value
        else Path.home() / ".codex" / "state_5.sqlite"
    )

    def resolve(
        _task_id: str,
        started_at: float,
        parent_thread_id: str | None,
        used_workers: frozenset[str],
    ) -> str | None:
        if parent_thread_id is None:
            return None
        from worker_library import owned_assignments
        deadline = environment.get("DE67_DEADLINE_STATE", "").strip()
        lineage = environment.get("DE67_LINEAGE", "").strip()
        if deadline and lineage:
            owned = owned_assignments(workspace, Path(deadline), lineage,
                                      coordinator_session_id=parent_thread_id)
            worker = owned.get(_task_id)
            if worker and worker not in used_workers:
                return worker
        if not state.is_file():
            return None
        connection = sqlite3.connect(f"file:{state}?mode=ro", uri=True)
        try:
            parent = connection.execute(
                "SELECT rollout_path FROM threads WHERE id = ?",
                (parent_thread_id,),
            ).fetchone()
            if parent is None or not parent[0]:
                return None
            rollout_path = Path(str(parent[0]))
            expected_path = f"/root/{worker_task_name(_task_id)}"
            spawned_ids: set[str] = set()
            try:
                with rollout_path.open("r", encoding="utf-8") as rollout:
                    for line in rollout:
                        try:
                            event = json.loads(line)
                        except json.JSONDecodeError:
                            continue
                        payload = event.get("payload")
                        if not isinstance(payload, dict):
                            continue
                        if payload.get("type") == "item_completed":
                            item = payload.get("item")
                            event_ms = payload.get("started_at_ms")
                        else:
                            continue
                        if (
                            isinstance(item, dict)
                            and item.get("type") == "SubAgentActivity"
                            and item.get("kind") == "started"
                            and item.get("agent_path") == expected_path
                            and isinstance(item.get("agent_thread_id"), str)
                            and isinstance(event_ms, (int, float))
                            and float(event_ms) / 1000.0 >= started_at
                        ):
                            spawned_ids.add(str(item["agent_thread_id"]))
            except OSError:
                return None
            rows = connection.execute(
                """
                SELECT child.id, child.agent_path,
                       child.created_at_ms, child.created_at
                FROM thread_spawn_edges AS edge
                JOIN threads AS child ON child.id = edge.child_thread_id
                WHERE edge.parent_thread_id = ? AND child.cwd = ?
                ORDER BY COALESCE(child.created_at_ms, child.created_at * 1000) DESC
                """,
                (parent_thread_id, str(workspace)),
            ).fetchall()
        finally:
            connection.close()
        for worker_id, agent_path, created_at_ms, created_at in rows:
            created = (
                float(created_at_ms) / 1000.0
                if created_at_ms is not None
                else float(created_at)
            )
            if (
                str(worker_id) in spawned_ids
                and str(worker_id) not in used_workers
                and agent_path == expected_path
                and created >= started_at
            ):
                return str(worker_id)
        return None

    return resolve


def _roster_validator(
    workspace: Path, environment: dict[str, str]
) -> Callable[[str, str | None, str | None], bool]:
    state_value = environment.get("DE67_CODEX_STATE", "").strip()
    state = (
        Path(state_value).expanduser().resolve()
        if state_value
        else Path.home() / ".codex" / "state_5.sqlite"
    )

    def validate(worker_id: str, parent_thread_id: str | None, task_id: str | None = None) -> bool:
        if parent_thread_id is None:
            return False
        from worker_library import owned_assignments
        deadline = environment.get("DE67_DEADLINE_STATE", "").strip()
        lineage = environment.get("DE67_LINEAGE", "").strip()
        if deadline and lineage and task_id != "":
            owned = owned_assignments(workspace, Path(deadline), lineage,
                                      coordinator_session_id=parent_thread_id)
            if (owned.get(task_id) == worker_id if task_id else worker_id in owned.values()):
                return True
        if not state.is_file():
            return False
        connection = sqlite3.connect(f"file:{state}?mode=ro", uri=True)
        try:
            row = connection.execute(
                """
                SELECT child.agent_path
                FROM thread_spawn_edges AS edge
                JOIN threads AS child ON child.id = edge.child_thread_id
                WHERE edge.parent_thread_id = ? AND edge.child_thread_id = ?
                  AND child.cwd = ?
                """,
                (parent_thread_id, worker_id, str(workspace)),
            ).fetchone()
        finally:
            connection.close()
        if row is None:
            return False
        if task_id is None:
            return True  # Recovery uses an existing durable task binding.
        if task_id:
            return row[0] == f"/root/{worker_task_name(task_id)}"
        return isinstance(row[0], str) and row[0].startswith("/root/task_")

    return validate


def _abandon_unbound_tasks(
    task_ids: Sequence[str], environment: dict[str, str]
) -> None:
    state = environment.get("DE67_DEADLINE_STATE", "").strip()
    lineage = environment.get("DE67_LINEAGE", "").strip()
    if not state or not lineage:
        return
    harness = Path(__file__).resolve().with_name("deadline_harness.py")
    for task_id in task_ids:
        subprocess.run(
            [
                sys.executable,
                str(harness),
                "abandon-attempt",
                "--state",
                state,
                "--lineage",
                lineage,
                "--task",
                task_id,
                "--reason",
                "Coordinator attempted to wait without delegating this task to a roster worker.",
            ],
            env=environment,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=False,
        )


def _claim_recorder(environment: dict[str, str]) -> Callable[[str, str, str | None], None]:
    state = environment.get("DE67_DEADLINE_STATE", "").strip()
    lineage = environment.get("DE67_LINEAGE", "").strip()
    supervisor = environment.get("DE67_SUPERVISOR_PID", "").strip()

    def record(task_id: str, worker_id: str, coordinator_session: str | None) -> None:
        if not state and not lineage and not supervisor:
            return
        if not state or not lineage or not supervisor or not coordinator_session:
            raise RunnerError("Durable worker claim lacks supervisor or coordinator identity")
        try:
            with DeadlineHarness(state) as harness:
                claim = harness.claim_worker(
                    lineage,
                    task_id,
                    worker_id,
                    coordinator_session,
                    supervisor,
                )
                if claim["recorded"]:
                    harness.checkpoint_worker(
                        lineage,
                        task_id,
                        worker_id,
                        "delegated",
                        "runner observed successful roster handoff",
                    )
        except DeadlineError as error:
            raise RunnerError(f"Durable worker claim failed: {error}") from error

    return record


def current_coordinator_prompt(workspace: Path, prompt: str, environment: dict[str, str]) -> str:
    """Render stable role guidance in the fresh runner, not the long-lived supervisor.

    Continuations and custom prompts retain their supplied context; canonical reviewers retain
    their gate and bindings while receiving current maintenance guidance. The supervisor
    still owns process identity, restart acknowledgement and all invocation bindings.
    """
    if (environment.get("DE67_PROCESS_ROLE") == "mutation-reviewer"
            and prompt.startswith(f"Act as the exclusive Phase-3 mutation reviewer in {workspace}.\n")):
        # The long-lived supervisor retains the gate and invocation bindings; the fresh
        # runner supplies the current role-owned maintenance guidance without reconstructing either.
        from coordinator_supervisor import mutation_maintenance_contract
        instruction = mutation_maintenance_contract()
        marker = "\nCurrent invocation bindings (use these values directly;"
        body, separator, bindings = prompt.partition(marker)
        if not separator:
            raise RunnerError("Reviewer prompt is missing its invocation bindings")
        if instruction not in body:
            body += "\n" + instruction + "\n"
        return body + separator + bindings
    if (environment.get("DE67_PROCESS_ROLE") != "coordinator"
            or environment.get("DE67_COORDINATOR_RESUME_SESSION")
            or not prompt.startswith(f"Act as a fresh Phase-3 delivery coordinator in {workspace}.\n")):
        return prompt
    marker = "\nCurrent invocation bindings (use these values directly;"
    body, separator, bindings = prompt.partition(marker)
    if not separator:
        raise RunnerError("Fresh supervisor prompt is missing its invocation bindings")
    from coordinator_supervisor import coordinator_prompt
    state = Path(environment["DE67_DEADLINE_STATE"])
    lineage = environment["DE67_LINEAGE"]
    run_id = environment["DE67_COORDINATOR_RUN_ID"]
    raw_generation = environment.get("DE67_COORDINATOR_RESTART_GENERATION")
    generation = int(raw_generation) if raw_generation else None
    reason = None
    if generation is not None:
        connection = sqlite3.connect(state.resolve().as_uri() + "?mode=ro", uri=True)
        try:
            row = connection.execute(
                "SELECT reason FROM coordinator_restart_requests WHERE lineage_id=? AND generation=?",
                (lineage, generation),
            ).fetchone()
            if row is None:
                raise RunnerError("Fresh coordinator restart generation is missing")
            reason = row[0]
        finally:
            connection.close()
    rendered = coordinator_prompt(workspace, state, lineage, run_id, generation, reason).rstrip()
    # The supervisor supplies the attempt-specific recovery instruction after its stable role.
    recovery_marker = "\nRecovery: this is coordinator decision opportunity "
    _, recovery_separator, recovery = body.partition(recovery_marker)
    if recovery_separator:
        rendered += recovery_separator + recovery.rstrip()
    return rendered + separator + bindings


def run(
    workspace_path: str | Path,
    prompt: str,
    *,
    environment: dict[str, str] | None = None,
) -> int:
    workspace = Path(workspace_path).expanduser().resolve()
    if not workspace.is_dir():
        raise RunnerError(f"Workspace does not exist: {workspace}")
    if not prompt.strip():
        raise RunnerError("Provide non-empty prompt text on standard input")

    selected_environment = os.environ.copy() if environment is None else environment.copy()
    prompt = current_coordinator_prompt(workspace, prompt, selected_environment)
    codex = _codex_executable(selected_environment)
    root_value = selected_environment.get("DE67_RUNNER_ROOT", "").strip()
    root = (
        Path(root_value).expanduser().resolve()
        if root_value
        else workspace / ".de67" / "state" / "runner-runs"
    )
    run_directory = root / _run_id()
    run_directory.mkdir(parents=True, exist_ok=False)

    prompt_path = run_directory / "prompt.txt"
    output_path = run_directory / "events.jsonl"
    status_path = run_directory / "status.json"
    prompt_path.write_text(prompt, encoding="utf-8")
    started_at = _timestamp()
    status_path.write_text(
        json.dumps({"status": "running", "started_at": started_at}, indent=2) + "\n",
        encoding="utf-8",
    )
    print(f"DE67_RUN_DIR={run_directory}", flush=True)

    from work_context import record_run
    record_run(workspace, run_directory, selected_environment)
    command = _command(codex, workspace, selected_environment)
    app_server_transport = len(command) > 1 and command[1] == str(
        Path(__file__).with_name("codex_app_server_runner.py")
    )
    process_environment = {**selected_environment, "DE67_RUNNER_ACTIVE_DIR": str(run_directory)}
    recovered_workers = _initial_recovered_workers(selected_environment)
    loop_guard = CoordinatorLoopGuard(
        initial_unbound_tasks=_initial_unbound_tasks(selected_environment),
        initial_pending_delegations=tuple(recovered_workers),
        recovered_workers=recovered_workers,
        roster_resolver=_roster_resolver(workspace, selected_environment),
        roster_validator=_roster_validator(workspace, selected_environment),
        claim_recorder=_claim_recorder(selected_environment),
        task_terminal=_task_terminal_resolver(selected_environment),
    )
    started = time.monotonic()
    session_id: str | None = None
    process: subprocess.Popen[str] | None = None
    tasks_to_abandon: tuple[str, ...] = ()
    try:
        with prompt_path.open("r", encoding="utf-8") as prompt_stream, output_path.open(
            "w", encoding="utf-8", newline="\n"
        ) as output_stream:
            process = subprocess.Popen(
                command,
                cwd=workspace,
                env=process_environment,
                stdin=prompt_stream,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                encoding="utf-8",
                errors="replace",
            )
            if process.stdout is None:
                raise RunnerError("Codex output pipe was not created")
            for line in process.stdout:
                output_stream.write(line)
                output_stream.flush()
                print(line, end="", flush=True)
                previous_session = session_id
                session_id = _record_session(line, selected_environment) or session_id
                if session_id != previous_session:
                    record_run(workspace, run_directory, selected_environment, session_id=session_id)
                try:
                    event = json.loads(line)
                except json.JSONDecodeError:
                    continue
                if not isinstance(event, dict):
                    continue
                try:
                    loop_guard.observe(event)
                except RunnerError:
                    tasks_to_abandon = loop_guard.unbound_tasks
                    raise
            exit_code = _reap(process)
            if app_server_transport:
                from codex_app_server_runner import stop_owned_runtime
                stop_owned_runtime(process, workspace, run_directory, selected_environment)
            process = None
            loop_guard.reconcile_handoffs()
            if loop_guard.unbound_tasks:
                _abandon_unbound_tasks(
                    loop_guard.unbound_tasks, selected_environment
                )
                raise RunnerError(
                    "Coordinator exited while a running task still lacked a verified roster worker"
                )
    except Exception as error:
        cleanup_errors: list[str] = []
        if process is not None:
            tasks_to_abandon = loop_guard.unbound_tasks
            try:
                if app_server_transport:
                    from codex_app_server_runner import stop_owned_runtime
                    stop_owned_runtime(process, workspace, run_directory, selected_environment)
                else:
                    process.kill()
            except Exception as cleanup_error:
                cleanup_errors.append(str(cleanup_error))
                try:
                    process.kill()
                except OSError:
                    pass
            _reap(process)
        if tasks_to_abandon:
            try:
                _abandon_unbound_tasks(tasks_to_abandon, selected_environment)
            except Exception as cleanup_error:
                cleanup_errors.append(str(cleanup_error))
        failure = error if isinstance(error, RunnerError) else RunnerError(str(error))
        status = {
            "status": "failed",
            "exit_code": 2,
            "error": str(failure),
            "started_at": started_at,
            "finished_at": _timestamp(),
            "elapsed_seconds": round(time.monotonic() - started, 3),
            "model": selected_environment.get("DE67_COORDINATOR_MODEL"),
            "reasoning_effort": selected_environment.get(
                "DE67_COORDINATOR_REASONING_EFFORT"
            ),
            "session_id": session_id,
        }
        if cleanup_errors:
            status["cleanup_error"] = "; ".join(cleanup_errors)
        status_path.write_text(
            json.dumps(status, indent=2, sort_keys=True)
            + "\n",
            encoding="utf-8",
        )
        record_run(workspace, run_directory, selected_environment, session_id=session_id)
        if failure is error:
            raise
        raise failure from error

    finished_at = _timestamp()
    status_path.write_text(
        json.dumps(
            {
                "status": "done" if exit_code == 0 else "failed",
                "exit_code": exit_code,
                "started_at": started_at,
                "finished_at": finished_at,
                "elapsed_seconds": round(time.monotonic() - started, 3),
                "model": selected_environment.get("DE67_COORDINATOR_MODEL"),
                "reasoning_effort": selected_environment.get(
                    "DE67_COORDINATOR_REASONING_EFFORT"
                ),
                "session_id": session_id,
            },
            indent=2,
            sort_keys=True,
        )
        + "\n",
        encoding="utf-8",
    )
    record_run(workspace, run_directory, selected_environment, session_id=session_id)
    return exit_code


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cwd", required=True, help="Coordinator workspace")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    arguments = build_parser().parse_args(argv)
    try:
        return run(arguments.cwd, sys.stdin.read())
    except (RunnerError, OSError) as error:
        print(f"DE-67 Codex runner: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
