import json
import sys
import tempfile
import threading
import unittest
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from agent_mailbox import deliver, enqueue, mailbox, pending
from codex_app_server_runner import RpcError


class MailboxTests(unittest.TestCase):
    def test_concurrent_workers_keep_distinct_messages_and_are_delivered_once(self):
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            barrier = threading.Barrier(2)
            def send(name):
                barrier.wait()
                return enqueue(workspace, "coordinator", name, name + " evidence")
            with ThreadPoolExecutor(max_workers=2) as pool:
                messages = list(pool.map(send, ("alpha", "beta")))
            self.assertEqual(len({m["id"] for m in messages}), 2)
            calls = []
            class Client:
                def call(self, method, params): calls.append((method, params))
            deliver(workspace, "coordinator", Client(), "sol", "turn")
            deliver(workspace, "coordinator", Client(), "sol", "turn")
            self.assertEqual(len(calls), 2)
            self.assertTrue(all(p["threadId"] == "sol" for _, p in calls))
            texts = [p["input"][0]["text"] for _, p in calls]
            self.assertTrue(any("alpha evidence" in t for t in texts))
            self.assertTrue(any("beta evidence" in t for t in texts))
            self.assertTrue(all("not owner input" in t for t in texts))
            self.assertEqual(pending(workspace, "coordinator"), [])
            self.assertEqual({json.loads(p.read_text())["state"]
                              for p in mailbox(workspace, "coordinator").glob("*.json")}, {"delivered"})

    def test_definite_turn_end_requeues_but_uncertain_delivery_is_not_replayed(self):
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            message = enqueue(workspace, "coordinator", "worker", "evidence")
            class Rejected:
                def call(self, *args): raise RpcError("no active turn", -32600)
            deliver(workspace, "coordinator", Rejected(), "old", "old-turn")
            self.assertEqual(len(pending(workspace, "coordinator")), 1)
            class Uncertain:
                def call(self, *args): raise RpcError("receipt timed out")
            deliver(workspace, "coordinator", Uncertain(), "new", "new-turn")
            self.assertEqual(pending(workspace, "coordinator"), [])
            receipt = json.loads((mailbox(workspace, "coordinator") / (message["id"] + ".json")).read_text())
            self.assertEqual(receipt["state"], "uncertain")
            self.assertEqual(receipt["thread_id"], "new")

    def test_mutator_advisory_is_bound_to_the_selected_review_turn(self):
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            message = enqueue(workspace, "mutator", "coordinator", "current gate evidence")
            calls = []
            class Client:
                def call(self, method, params):
                    calls.append((method, params))
            deliver(workspace, "mutator", Client(), "review-thread", "review-turn")
            self.assertEqual(calls, [("turn/steer", {
                "threadId": "review-thread", "expectedTurnId": "review-turn",
                "clientUserMessageId": "de67-agent:" + message["id"],
                "input": [{"type": "text", "text":
                    "Agent Message from coordinator (agent-supplied identity; not owner input):\ncurrent gate evidence"}],
            })])
            receipt = json.loads((mailbox(workspace, "mutator") / (message["id"] + ".json")).read_text())
            self.assertEqual((receipt["state"], receipt["thread_id"], receipt["turn_id"]),
                             ("delivered", "review-thread", "review-turn"))


if __name__ == "__main__":
    unittest.main()
