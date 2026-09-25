#!/usr/bin/env python3
"""One runner-owned Codex App Server, with a live input address for its coordinator."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import queue
import signal
import subprocess
import sys
import time
from pathlib import Path
from typing import Any


class RpcError(RuntimeError):
    def __init__(self, message: str, code: int | None = None):
        super().__init__(message)
        self.code = code


class Rpc:
    """JSON RPC over the local, private App Server websocket."""

    def __init__(self, socket: Path):
        from websockets.exceptions import ConnectionClosed
        from websockets.sync.client import unix_connect
        self.closed_error = ConnectionClosed
        self.connection = unix_connect(str(socket), compression=None, max_size=None)
        self.notifications: list[dict[str, Any]] = []
        self.sequence = 0

    def send(self, message: dict[str, Any]) -> None:
        self.connection.send(json.dumps(message))

    def receive(self, timeout: float = 1) -> dict[str, Any]:
        try:
            message = json.loads(self.connection.recv(timeout=timeout))
        except TimeoutError:
            raise queue.Empty from None
        except self.closed_error as error:
            raise RpcError("Codex App Server connection closed") from error
        if "id" in message and "method" in message:
            # This runner is noninteractive, just like codex exec. Do not invent approvals.
            if message["method"] == "item/tool/requestUserInput":
                self.send({"id": message["id"], "result": {"answers": {}}})
            else:
                self.send({"id": message["id"], "error": {
                    "code": -32601, "message": "No interactive handler in DE67 runner"}})
        return message

    def call(self, method: str, params: dict[str, Any], timeout: float = 60) -> Any:
        self.sequence += 1
        request_id = self.sequence
        self.send({"id": request_id, "method": method, "params": params})
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                message = self.receive(min(1, max(.01, deadline - time.monotonic())))
            except queue.Empty:
                continue
            if message.get("id") == request_id and "method" not in message:
                if "error" in message:
                    raise RpcError(f"{method}: {message['error']}", message["error"].get("code"))
                return message.get("result")
            if "method" in message and "id" not in message:
                self.notifications.append(message)
        raise RpcError(f"{method} receipt timed out; delivery may be uncertain")

    def close(self) -> None:
        self.connection.close()


def normalize_item(item: dict[str, Any]) -> dict[str, Any]:
    """Keep the existing runner's CLI-event audit and worker-binding contract."""
    kind = item.get("type")
    result = dict(item)
    if kind == "commandExecution":
        result.update(type="command_execution", aggregated_output=item.get("aggregatedOutput"),
                      exit_code=item.get("exitCode"))
    elif kind == "collabAgentToolCall":
        tools = {"spawnAgent": "spawn_agent", "followupTask": "followup_task",
                 "sendMessage": "send_message", "interruptAgent": "interrupt_agent",
                 "listAgents": "list_agents", "wait": "wait"}
        result.update(type="collab_tool_call", tool=tools.get(item.get("tool"), item.get("tool")),
                      receiver_thread_ids=item.get("receiverThreadIds", []),
                      agents_states=item.get("agentsStates", {}))
    elif kind == "agentMessage":
        result["type"] = "agent_message"
    if result.get("status") == "inProgress":
        result["status"] = "in_progress"
    return result


def atomic_json(path: Path, value: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temp = path.with_name(path.name + f".{os.getpid()}.tmp")
    temp.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")
    temp.replace(path)


def emit(value: dict[str, Any]) -> None:
    print(json.dumps(value, ensure_ascii=False), flush=True)


def socket_path(run_directory: Path, environment: dict[str, str]) -> Path:
    root = Path(environment.get("CODEX_HOME") or Path.home() / ".codex") / "state" / "de67-input"
    tag = hashlib.sha256(str(run_directory).encode()).hexdigest()[:16]
    return root / (tag + ".sock")


def process_snapshot() -> dict[int, tuple[int, str, str]]:
    result = subprocess.run(["ps", "-axo", "pid=,ppid=,lstart=,stat=,command="],
                            check=True, capture_output=True, text=True)
    rows = {}
    for line in result.stdout.splitlines():
        fields = line.split(maxsplit=8)
        if len(fields) == 9 and fields[0].isdigit() and fields[1].isdigit() and not fields[7].startswith("Z"):
            rows[int(fields[0])] = (int(fields[1]), " ".join(fields[2:7]), fields[8])
    return rows


def stop_owned_runtime(process: Any, workspace: Path, run_directory: Path,
                       environment: dict[str, str], *, captured_rows=None,
                       owned_socket: Path | None = None) -> None:
    """Outer-runner cleanup, including an adapter killed before its finally block.

    Keep the supervisor's existing process group intact. Select only this child tree,
    or its orphaned server's unique run socket, and recheck birth identity before signals.
    """
    socket = owned_socket or socket_path(run_directory, environment)
    rows = process_snapshot() if captured_rows is None else captured_rows
    roots = {process.pid} if process.poll() is None else set()
    roots.update(pid for pid, (_, _, command) in rows.items()
                 if command.endswith(f"app-server --listen unix://{socket}"))
    owned = set(roots)
    while True:
        children = {pid for pid, (parent, _, _) in rows.items() if parent in owned}
        if children <= owned:
            break
        owned.update(children)
    identities = {pid: rows[pid][1] for pid in owned if pid in rows}

    def alive() -> dict[int, str]:
        current = process_snapshot()
        return {pid: birth for pid, birth in identities.items()
                if pid in current and current[pid][1] == birth
                and "<defunct>" not in current[pid][2]}

    def signal_owned(signum: int) -> None:
        for pid in alive():
            try:
                os.kill(pid, signum)
            except ProcessLookupError:
                pass

    signal_owned(signal.SIGTERM)
    deadline = time.monotonic() + 10
    while alive() and time.monotonic() < deadline:
        process.poll()  # Reap the adapter while observing its descendants.
        time.sleep(.05)
    signal_owned(signal.SIGKILL)
    role = "mutator" if environment.get("DE67_PROCESS_ROLE") == "mutation-reviewer" else "coordinator"
    address = workspace / ".de67/state" / f"{role}-input.json"
    try:
        binding = json.loads(address.read_text(encoding="utf-8"))
        if binding.get("runner_pid") == process.pid and binding.get("socket") == str(socket):
            address.unlink()
    except (OSError, ValueError):
        pass
    socket.unlink(missing_ok=True)


def run(codex: str, workspace: Path, prompt: str) -> int:
    if sys.platform == "win32":
        raise RpcError("The optional Unix App Server transport requires macOS or Linux")
    run_directory = Path(os.environ["DE67_RUNNER_ACTIVE_DIR"])
    socket = socket_path(run_directory, os.environ)
    socket.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    process_role = os.environ.get("DE67_PROCESS_ROLE", "coordinator")
    if process_role not in {"coordinator", "mutation-reviewer"}:
        raise RpcError(f"Unsupported App Server role: {process_role}")
    role = "mutator" if process_role == "mutation-reviewer" else "coordinator"
    if os.environ.get("DE67_INITIAL_INPUT_PATH") and role != "mutator":
        raise RpcError("Initial owner-conversation input is valid only for the mutator")
    address = workspace / ".de67" / "state" / f"{role}-input.json"
    parent_pid = os.getppid()
    server: subprocess.Popen[Any] | None = None
    rpc: Rpc | None = None
    session = None
    workers = None
    coordinator_done = False
    coordinator_exit = 0
    thread_id = None
    resume = ""
    completed = False
    stopping = False

    def stop(_signum: int, _frame: Any) -> None:
        nonlocal stopping
        stopping = True

    for signum in (signal.SIGINT, signal.SIGTERM):
        signal.signal(signum, stop)
    try:
        config_path = workspace / ".de67/state/workspace.json"
        workspace_config = json.loads(config_path.read_text(encoding="utf-8")) if config_path.exists() else {}
        initial_path = os.environ.get("DE67_INITIAL_INPUT_PATH")
        initial = json.loads(Path(initial_path).read_text(encoding="utf-8")) if initial_path else None
        if role == "mutator" and workspace_config.get("persistent_mutator") is True:
            from mutator_session import MutatorSession
            session = MutatorSession(workspace)
            session.acquire(lambda: stopping or os.getppid() != parent_pid)
            resume = session.thread_id() or ""
        if not session:
            resume = os.environ.get("DE67_COORDINATOR_RESUME_SESSION", "").strip()
        with (run_directory / "app-server.log").open("a", encoding="utf-8") as log:
            server = subprocess.Popen([codex, "app-server", "--listen", f"unix://{socket}"],
                                      stdin=subprocess.DEVNULL, stdout=log, stderr=log)
            ready_by = time.monotonic() + 30
            while not socket.exists():
                if server.poll() is not None or time.monotonic() >= ready_by or stopping:
                    raise RpcError("Runner-owned App Server did not open its socket")
                time.sleep(.05)
            rpc = Rpc(socket)
            rpc.call("initialize", {"clientInfo": {"name": "de67_runner", "version": "1.0"},
                                    "capabilities": {"experimentalApi": True}})
            rpc.send({"method": "initialized", "params": {}})
            model = os.environ.get("DE67_COORDINATOR_MODEL", "gpt-6-sol")
            if model not in {"gpt-6-luna", "gpt-6-sol", "gpt-6-astra"}:
                raise RpcError("Retired model: use GPT-6 Luna, Sol or Astra")
            effort = os.environ.get("DE67_COORDINATOR_REASONING_EFFORT", "low")
            params: dict[str, Any] = {
                "cwd": str(workspace), "model": model, "approvalPolicy": "never",
                "sandbox": os.environ.get("DE67_COORDINATOR_SANDBOX", "danger-full-access"),
                "config": {"model_reasoning_effort": effort},
            }
            if model == "gpt-6-astra":
                params["config"]["features.context_management.experimental_mode"] = True
            if role == "mutator" and "mutator_context_window" in workspace_config:
                params["config"]["model_context_window"] = workspace_config["mutator_context_window"]
            if resume:
                params.update(threadId=resume, excludeTurns=True)
            thread = rpc.call("thread/resume" if resume else "thread/start", params)["thread"]
            thread_id = thread["id"]
            if session:
                session.record(thread_id, state="starting", runner_pid=os.getpid())
            emit({"type": "thread.started", "thread_id": thread_id})
            turn_params = {"threadId": thread_id, "effort": effort,
                           "input": [{"type": "text", "text": prompt}]}
            if initial:
                # A resumed owner conversation already carries its standing guidance.
                # Supervisor review prompts (without initial input) remain per-invocation.
                if session and resume:
                    turn_params["input"] = []
                turn_params["input"].extend(initial["input"])
                turn_params["clientUserMessageId"] = initial["client_id"]
                atomic_json(Path(initial["receipt_path"]), {"state": "submitting", "thread_id": thread_id})
            turn = rpc.call("turn/start", turn_params)["turn"]
            turn_id = turn["id"]
            binding = {"workspace": str(workspace), "role": role,
                       "run_id": os.environ.get("DE67_COORDINATOR_RUN_ID"),
                       "runner_pid": os.getpid(), "server_pid": server.pid, "socket": str(socket),
                       "thread_id": thread_id, "turn_id": turn_id, "state": "active"}
            if role == "coordinator":
                binding.update(deadline_state=os.environ.get("DE67_DEADLINE_STATE"),
                               lineage=os.environ.get("DE67_LINEAGE"),
                               supervisor_id=os.environ.get("DE67_SUPERVISOR_PID"))
            atomic_json(address, binding)
            if role == "coordinator" and all(binding.get(key) for key in
                    ("deadline_state", "lineage", "supervisor_id")):
                from worker_library import WorkerDispatcher
                workers = WorkerDispatcher(workspace, rpc, binding)
            if initial:
                atomic_json(Path(initial["receipt_path"]), {**binding, "state": "submitted"})
            if session:
                session.record(thread_id, state="active", runner_pid=os.getpid(), turn_id=turn_id)
            pending = rpc.notifications
            rpc.notifications = []
            while not stopping:
                if os.getppid() != parent_pid:
                    raise RpcError("Owning DE67 runner exited")
                if workers is not None:
                    workers.reconcile()
                if coordinator_done and not pending and not rpc.notifications and (workers is None or not workers.has_active_turns()):
                    return coordinator_exit
                if not coordinator_done:
                    if workers is not None:
                        workers.process_pending()
                    from agent_mailbox import deliver
                    deliver(workspace, role, rpc, thread_id, turn_id)
                if rpc.notifications:
                    pending.extend(rpc.notifications)
                    rpc.notifications = []
                try:
                    message = pending.pop(0) if pending else rpc.receive()
                except queue.Empty:
                    continue
                if workers is not None and workers.observe(message):
                    continue
                method = message.get("method")
                payload = message.get("params", {})
                if payload.get("threadId") != thread_id:
                    continue
                if not str(method).endswith("/delta") and not str(method).endswith("/outputDelta"):
                    emit({"type": "app_server.notification", **message})
                if method in {"item/started", "item/completed"}:
                    emit({"type": method.replace("/", "."), "item": normalize_item(payload["item"])})
                elif method == "turn/started":
                    emit({"type": "turn.started", "turn_id": payload["turn"]["id"]})
                elif method == "turn/completed" and payload["turn"]["id"] == turn_id:
                    completed = payload["turn"]["status"] == "completed"
                    binding["state"] = "finished"
                    atomic_json(address, binding)
                    emit({"type": "turn.completed", "turn_id": turn_id})
                    coordinator_done = True
                    coordinator_exit = 0 if payload["turn"]["status"] == "completed" else 1
                    # Keep this server alive for already dispatched worker turns. The
                    # supervisor resumes Sol after returns; no new work is dispatched
                    # while Sol's own turn is finished.
            return 130
    finally:
        try:
            if address.exists() and json.loads(address.read_text(encoding="utf-8")).get("runner_pid") == os.getpid():
                address.unlink()
        except (OSError, ValueError):
            pass
        try:
            if rpc is not None:
                rpc.close()
        finally:
            try:
                if server is not None and server.poll() is None:
                    server.terminate()
                    try:
                        server.wait(timeout=10)
                    except subprocess.TimeoutExpired:
                        server.kill()
                        server.wait()
            finally:
                socket.unlink(missing_ok=True)
                if workers is not None and (server is None or server.poll() is not None):
                    workers.shutdown()
                if session:
                    try:
                        if thread_id:
                            session.record(thread_id, state="idle",
                                           result="completed" if completed else "interrupted")
                    finally:
                        session.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--codex", required=True)
    parser.add_argument("--cwd", required=True)
    args = parser.parse_args()
    try:
        raise SystemExit(run(args.codex, Path(args.cwd).resolve(), sys.stdin.read()))
    except (RpcError, OSError, KeyError) as error:
        print(f"DE67 App Server: {error}", file=sys.stderr)
        raise SystemExit(2)
