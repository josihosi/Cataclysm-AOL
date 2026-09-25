from __future__ import annotations

from copy import deepcopy
import json
from pathlib import Path
import unittest

from test_worker_library import FakeRpc, WorkerFixture, library


class CapturedFinalRpc(FakeRpc):
    def __init__(self, case, workspace, mode):
        super().__init__()
        self.case, self.workspace, self.mode = case, workspace, mode
        self.thread_id = case["item_completed"]["params"]["threadId"]
        self.turn_id = case["item_completed"]["params"]["turnId"]
        self.accepted_input = None

    def call(self, method, params):
        self.calls.append((method, params))
        if method == "thread/start":
            return {"thread": {"id": self.thread_id, "cwd": str(self.workspace), "status": {"type": "idle"}}}
        if method == "turn/start":
            self.turn_count += 1
            self.accepted_input = {"type": "userMessage", "id": "accepted-user-input",
                                   "clientId": params["clientUserMessageId"], "content": params["input"]}
            if self.mode == "recovery":
                raise TimeoutError("The accepted turn receipt was lost")
            return {"turn": {"id": self.turn_id, "status": "inProgress"}}
        if method == "thread/read":
            turn = deepcopy(self.case["turn_completed"]["params"]["turn"])
            turn["items"].insert(0, self.accepted_input)
            return {"thread": {"id": self.thread_id, "cwd": str(self.workspace),
                               "status": {"type": "idle"}, "turns": [turn]}}
        raise AssertionError("Unexpected RPC during captured final replay: " + method)


class WorkerLibraryFinalEventTests(WorkerFixture, unittest.TestCase):
    def captured_cases(self):
        path = Path(__file__).parent / "fixtures/worker_final_events.json"
        return json.loads(path.read_text(encoding="utf-8"))

    def replay(self, case, mode):
        expected = case["item_completed"]["params"]["item"]["text"]
        case = deepcopy(case)
        commentary = {"type": "agentMessage", "id": "commentary-only", "phase": "commentary",
                      "text": "Progress commentary must stay outside the final result artifact"}
        case["turn_completed"]["params"]["turn"]["items"].insert(0, commentary)
        self.rpc = CapturedFinalRpc(case, self.workspace, mode)
        self.dispatcher = library.WorkerDispatcher(self.workspace, self.rpc, self.binding)
        self.worker(case["name"])
        self.assign(case["name"], case["name"], self.task(case["name"]))
        self.dispatcher.process_pending()
        if mode == "recovery":
            self.dispatcher.reconcile()
        else:
            if mode in {"streamed", "both"}:
                self.dispatcher.observe({"method": "item/completed", "params": {
                    "threadId": self.rpc.thread_id, "turnId": self.rpc.turn_id, "item": commentary}})
                self.dispatcher.observe(case["item_completed"])
            completed = deepcopy(case["turn_completed"])
            if mode == "streamed":
                completed["params"]["turn"]["items"] = []
            self.dispatcher.observe(completed)
        assignment = library.describe(self.workspace, case["name"])["assignment"]
        self.assertEqual(assignment["status"], "returned")
        self.assertEqual(Path(assignment["result_path"]).read_text(encoding="utf-8"), expected)
        self.assertEqual(self.rpc.turn_count, 1)
        self.assertIsNone(library._task(self.state, "project", case["name"])["attempt_terminal_at"])

    def test_captured_final_answer_stream_survives_empty_completion_items(self):
        for case in self.captured_cases():
            with self.subTest(source=case["name"]):
                self.replay(case, "streamed")

    def test_captured_completion_only_final_answer_excludes_commentary(self):
        for case in self.captured_cases():
            with self.subTest(source=case["name"]):
                self.replay(case, "completion")

    def test_captured_final_answer_recovery_preserves_exact_final_without_replay(self):
        for case in self.captured_cases():
            with self.subTest(source=case["name"]):
                self.replay(case, "recovery")

    def test_captured_stream_and_completion_do_not_duplicate_same_final(self):
        for case in self.captured_cases():
            with self.subTest(source=case["name"]):
                self.replay(case, "both")

    def test_legacy_unknown_and_final_phases_still_work(self):
        for case, phase in zip(self.captured_cases(), (None, "final")):
            with self.subTest(phase=phase):
                case["item_completed"]["params"]["item"]["phase"] = phase
                case["turn_completed"]["params"]["turn"]["items"][0]["phase"] = phase
                self.replay(case, "both")


if __name__ == "__main__":
    unittest.main()
