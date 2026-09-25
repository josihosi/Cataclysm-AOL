from __future__ import annotations

from copy import deepcopy
from pathlib import Path
import unittest

from test_worker_library import FakeRpc, WorkerFixture, library


class RecoveryRpc(FakeRpc):
    def __init__(self):
        super().__init__()
        self.history = {}
        self.runtime_status = "active"
        self.lose_next_turn_receipt = False

    def call(self, method, params):
        if method == "thread/read":
            self.calls.append((method, params))
            return {"thread": {**self.threads[params["threadId"]],
                               "status": {"type": self.runtime_status},
                               "turns": deepcopy(self.history.get(params["threadId"], []))}}
        result = super().call(method, params)
        if method == "turn/start":
            turn = {**result["turn"], "items": [{"type": "userMessage", "id": "user-" + result["turn"]["id"],
                    "clientId": params["clientUserMessageId"], "content": params["input"]}]}
            self.history.setdefault(params["threadId"], []).append(turn)
            if self.lose_next_turn_receipt:
                self.lose_next_turn_receipt = False
                raise TimeoutError("Accepted turn/start response was lost")
        return result


class WorkerLibraryRecoveryTests(WorkerFixture, unittest.TestCase):
    def setUp(self):
        super().setUp()
        self.rpc = RecoveryRpc()
        self.dispatcher = library.WorkerDispatcher(self.workspace, self.rpc, self.binding)

    def lost_receipt(self, task_id="task-a"):
        if not library.catalog(self.workspace)["workers"]:
            self.worker()
        request = self.assign(task_id=task_id)
        self.rpc.lose_next_turn_receipt = True
        self.dispatcher.process_pending()
        assignment = library.describe(self.workspace, "pilot")["assignment"]
        self.assertEqual(assignment["status"], "uncertain")
        self.assertIsNone(assignment["turn_id"])
        return request, assignment, self.rpc.history[assignment["worker_id"]][-1]

    def restart(self):
        self.dispatcher.shutdown()
        Path(self.binding["socket"]).unlink()
        self.binding = self.bind("sol-a", "run-b", "supervisor-b")
        self.rpc.runtime_status = "notLoaded"
        self.dispatcher = library.WorkerDispatcher(self.workspace, self.rpc, self.binding)

    def test_lost_receipt_binds_only_accepted_user_message_then_observes_completion(self):
        request, assignment, turn = self.lost_receipt()
        self.assertTrue(self.dispatcher.has_active_turns())
        self.dispatcher.observe({"method": "turn/started", "params": {"threadId": assignment["worker_id"],
            "turn": {"id": turn["id"], "status": "inProgress", "items": []}}})
        self.assertIsNone(library.describe(self.workspace, "pilot")["assignment"]["turn_id"])
        self.dispatcher.observe({"method": "item/started", "params": {"threadId": assignment["worker_id"],
            "turnId": turn["id"], "item": turn["items"][0]}})
        current = library.describe(self.workspace, "pilot")["assignment"]
        self.assertEqual(current["status"], "running")
        self.assertEqual(current["turn_id"], turn["id"])
        returned = self.returned(text="Accepted work completed once")
        self.assertEqual(returned["status"], "returned")
        self.assertEqual(library.request_status(self.workspace, request["request_id"])["state"], "submitted")
        self.assertEqual(self.rpc.turn_count, 1)
        self.assertFalse(self.dispatcher.has_active_turns())

    def test_read_only_history_recovers_completed_turn_without_generation(self):
        request, assignment, turn = self.lost_receipt()
        turn.update(status="completed")
        turn["items"].append({"type": "agentMessage", "id": "final", "phase": "final", "text": "Stored final evidence"})
        self.dispatcher.reconcile()
        current = library.describe(self.workspace, "pilot")["assignment"]
        self.assertEqual(current["status"], "returned")
        self.assertEqual(Path(current["result_path"]).read_text(encoding="utf-8"), "Stored final evidence")
        self.assertEqual(self.rpc.turn_count, 1)
        self.assertIsNone(library._task(self.state, "project", "task-a")["attempt_terminal_at"])

    def test_unknown_or_previous_turn_cannot_hijack_current_assignment(self):
        self.worker()
        self.assign()
        self.dispatcher.process_pending()
        previous = self.returned()
        self.settle()
        request, assignment, turn = self.lost_receipt("task-b")
        old_turn = self.rpc.history[assignment["worker_id"]][0]
        self.dispatcher.observe({"method": "turn/completed", "params": {"threadId": assignment["worker_id"],
            "turn": {**old_turn, "status": "completed"}}})
        self.dispatcher.observe({"method": "item/started", "params": {"threadId": assignment["worker_id"],
            "turnId": turn["id"], "item": {**turn["items"][0], "clientId": "unrelated-request"}}})
        self.dispatcher.observe({"method": "item/started", "params": {"threadId": assignment["worker_id"],
            "turnId": previous["turn_id"], "item": turn["items"][0]}})
        self.assertIsNone(library.describe(self.workspace, "pilot")["assignment"]["turn_id"])
        self.dispatcher.reconcile()
        self.assertEqual(library.describe(self.workspace, "pilot")["assignment"]["turn_id"], turn["id"])
        self.assertEqual(self.rpc.turn_count, 2)

    def test_restart_recovers_persisted_completion_and_original_claim_without_replay(self):
        request, assignment, turn = self.lost_receipt()
        turn.update(status="completed")
        turn["items"].append({"type": "agentMessage", "id": "final", "phase": "final", "text": "Done before restart"})
        self.restart()
        self.dispatcher.reconcile()
        current = library.describe(self.workspace, "pilot")["assignment"]
        self.assertEqual(current["status"], "returned")
        self.assertEqual(current["turn_id"], turn["id"])
        self.assertEqual(library._task(self.state, "project", "task-a")["supervisor_id"], "supervisor-a")
        self.assertEqual(library.owned_assignments(self.workspace, self.state, "project"), {"task-a": assignment["worker_id"]})
        self.assertEqual(self.rpc.turn_count, 1)

    def test_restart_marks_stopped_in_progress_turn_interrupted_without_replay(self):
        request, assignment, turn = self.lost_receipt()
        self.restart()
        self.dispatcher.reconcile()
        current = library.describe(self.workspace, "pilot")["assignment"]
        self.assertEqual(current["status"], "interrupted")
        self.assertEqual(current["turn_id"], turn["id"])
        self.assertFalse(self.dispatcher.has_active_turns())
        self.assertEqual(self.rpc.turn_count, 1)
        library.message(self.workspace, "pilot", "Inspect current artifacts and continue the remaining work", environment=self.env)
        self.dispatcher.process_pending()
        self.assertEqual(self.rpc.turn_count, 2)

    def test_missing_history_keeps_live_uncertainty_then_allows_deliberate_stopped_continuation(self):
        request, assignment, turn = self.lost_receipt()
        self.rpc.history[assignment["worker_id"]] = []
        self.dispatcher.reconcile()
        self.assertTrue(self.dispatcher.has_active_turns())
        self.assertEqual(library.describe(self.workspace, "pilot")["assignment"]["status"], "uncertain")
        self.restart()
        self.dispatcher.reconcile()
        current = library.describe(self.workspace, "pilot")["assignment"]
        self.assertEqual(current["status"], "interrupted")
        self.assertIn("outcome remains uncertain", current["error"])
        self.assertEqual(library.request_status(self.workspace, request["request_id"])["state"], "uncertain")
        self.assertFalse(self.dispatcher.has_active_turns())
        self.assertEqual(self.rpc.turn_count, 1)
        library.message(self.workspace, "pilot", "Inspect current state before deciding which work remains", environment=self.env)
        self.dispatcher.process_pending()
        self.assertEqual(self.rpc.turn_count, 2)

    def test_loading_timeout_sends_no_generation_and_keeps_initial_task_assignable(self):
        self.worker()
        request = self.assign()
        self.rpc.fail_next = ("thread/start", TimeoutError("Loading response lost"))
        self.dispatcher.process_pending()
        self.assertEqual(library.request_status(self.workspace, request["request_id"])["state"], "rejected")
        self.assertIsNone(library._task(self.state, "project", "task-a")["worker_id"])
        self.assertEqual(self.rpc.turn_count, 0)
        packet = self.workspace / ".de67/state/worker-dispatch/task-a.md"
        self.assign(packet=(packet, library.hashlib.sha256(packet.read_bytes()).hexdigest()))
        self.dispatcher.process_pending()
        self.assertEqual(self.rpc.turn_count, 1)

    def test_continuation_loading_timeout_restores_previous_known_assignment(self):
        self.worker()
        self.assign()
        self.dispatcher.process_pending()
        previous = self.returned()
        request = library.message(self.workspace, "pilot", "Continue the known assignment", environment=self.env)
        self.rpc.fail_next = ("thread/resume", TimeoutError("Loading response lost"))
        self.dispatcher.process_pending()
        current = library.describe(self.workspace, "pilot")["assignment"]
        self.assertEqual(current["status"], "returned")
        self.assertEqual(current["turn_id"], previous["turn_id"])
        self.assertEqual(current["request_id"], previous["request_id"])
        self.assertEqual(library.request_status(self.workspace, request["request_id"])["state"], "rejected")
        self.assertEqual(self.rpc.turn_count, 1)
        library.message(self.workspace, "pilot", "Continue the known assignment now", environment=self.env)
        self.dispatcher.process_pending()
        self.assertEqual(self.rpc.turn_count, 2)


if __name__ == "__main__":
    unittest.main()
