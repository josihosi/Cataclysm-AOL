from __future__ import annotations

from contextlib import redirect_stdout
import io
import json
import os
from pathlib import Path
import queue
import unittest
from unittest.mock import patch

from test_worker_library import FakeRpc, WorkerFixture, library
import codex_app_server_runner as transport
from agent_mailbox import pending


class WorkerLibraryTransportTests(WorkerFixture, unittest.TestCase):
    def test_sol_final_keeps_worker_alive_and_preserves_return_outside_sol_stream(self):
        self.exercise_early_sol_final()

    def test_sol_final_after_lost_worker_receipt_preserves_work_without_replay(self):
        self.exercise_early_sol_final(lose_receipt=True)

    def test_inactive_unmatched_request_allows_owned_teardown_without_replay(self):
        self.exercise_early_sol_final(lose_receipt=True, no_accepted_turn=True)

    def exercise_early_sol_final(self, lose_receipt=False, no_accepted_turn=False):
        self.worker()
        packet = self.task()
        run_directory = self.workspace / "run"
        run_directory.mkdir()
        fixture = self
        server_events = []

        class Server:
            pid = 999997

            def __init__(self, command, **kwargs):
                self.stopped = False
                Path(command[-1].removeprefix("unix://")).touch()

            def poll(self):
                return 0 if self.stopped else None

            def terminate(self):
                server_events.append("terminated")
                self.stopped = True

            def wait(self, timeout=None):
                return 0

        class Client(FakeRpc):
            def __init__(self, socket):
                super().__init__()
                self.notifications = []
                self.queued = False
                self.sol_started = False
                self.history = {}
                fixture.client = self

            def send(self, message):
                pass

            def close(self):
                server_events.append("connection closed")

            def receive(self, timeout=1):
                if not self.queued:
                    self.queued = True
                    fixture.assign(packet=packet)
                    raise queue.Empty
                raise AssertionError("Worker notifications were not handled before transport waited again")

            def call(self, method, params):
                if method == "initialize":
                    return {}
                if method == "thread/read":
                    return {"thread": {"id": params["threadId"], "cwd": str(fixture.workspace),
                        "status": {"type": "idle" if no_accepted_turn else "active"},
                        "turns": [] if no_accepted_turn else self.history[params["threadId"]]}}
                if method == "thread/start" and not self.sol_started:
                    self.sol_started = True
                    return {"thread": {"id": "sol-a"}}
                if method == "turn/start" and params["threadId"] == "sol-a":
                    return {"turn": {"id": "sol-turn", "status": "inProgress"}}
                result = super().call(method, params)
                if method == "turn/start":
                    turn_id = result["turn"]["id"]
                    worker_id = params["threadId"]
                    self.history[worker_id] = [{"id": turn_id, "status": "inProgress", "items": [{
                        "type": "userMessage", "id": "user", "clientId": params["clientUserMessageId"],
                        "content": params["input"]}]}]
                    self.notifications.extend([
                        {"method": "turn/completed", "params": {"threadId": "sol-a",
                            "turn": {"id": "sol-turn", "status": "completed"}}},
                        {"method": "thread/tokenUsage/updated", "params": {"threadId": worker_id,
                            "tokenUsage": {"last": {"inputTokens": 100, "cachedInputTokens": 40,
                                                     "outputTokens": 20}}}},
                        {"method": "item/completed", "params": {"threadId": worker_id, "turnId": turn_id,
                            "item": {"type": "commandExecution", "id": "command", "command": "worker private tool",
                                     "aggregatedOutput": "worker private tool output", "exitCode": 0}}},
                        {"method": "turn/completed", "params": {"threadId": worker_id,
                            "turn": {"id": turn_id, "status": "completed", "items": [
                                {"type": "agentMessage", "id": "final", "phase": "final",
                                 "text": "worker private final after Sol"}]}}},
                    ])
                    if no_accepted_turn:
                        self.notifications = self.notifications[:1]  # Only Sol completed; no worker input was observed.
                    if lose_receipt:
                        raise TimeoutError("Accepted worker turn/start receipt was lost")
                return result

        environment = {"CODEX_HOME": str(self.workspace / "codex"), "CODEX_THREAD_ID": "sol-a",
                       "DE67_RUNNER_ACTIVE_DIR": str(run_directory), "DE67_COORDINATOR_RUN_ID": "run-a",
                       "DE67_DEADLINE_STATE": str(self.state), "DE67_LINEAGE": "project",
                       "DE67_SUPERVISOR_PID": "supervisor-a"}
        output = io.StringIO()
        with patch.dict(os.environ, environment, clear=True), patch.object(transport.sys, "platform", "darwin"), \
                patch.object(transport.signal, "signal"), patch.object(transport.subprocess, "Popen", Server), \
                patch.object(transport, "Rpc", Client), redirect_stdout(output):
            self.assertEqual(transport.run("codex", self.workspace, "Coordinate the assigned task"), 0)

        assignment = library.describe(self.workspace, "pilot")["assignment"]
        if no_accepted_turn:
            self.assertEqual(assignment["status"], "interrupted")
            self.assertIn("outcome remains uncertain", assignment["error"])
            self.assertEqual(library.request_status(self.workspace, assignment["request_id"])["state"], "uncertain")
            self.assertIsNone(assignment["result_path"])
        else:
            self.assertEqual(assignment["status"], "returned")
            self.assertEqual(Path(assignment["result_path"]).read_text(encoding="utf-8"), "worker private final after Sol")
            audit = Path(assignment["events_path"]).read_text(encoding="utf-8")
            self.assertIn("tokenUsage", audit)
            self.assertIn("worker private tool output", audit)
            self.assertTrue(any("nonterminal evidence" in message["text"] for _, message in pending(self.workspace, "coordinator")))
        self.assertNotIn("worker private", output.getvalue())
        self.assertIsNone(library._task(self.state, "project", "task-a")["attempt_terminal_at"])
        self.assertEqual(server_events, ["connection closed", "terminated"])
        self.assertEqual(fixture.client.turn_count, 1)
        self.assertFalse((self.workspace / ".de67/state/coordinator-input.json").exists())


if __name__ == "__main__":
    unittest.main()
