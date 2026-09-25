from __future__ import annotations

from contextlib import closing
import hashlib
import json
from pathlib import Path
import shutil
import sqlite3
import sys
import tempfile
import time
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from deadline_harness import DeadlineHarness
from work_context import record_dispatch
import worker_library as library


class Rejected(RuntimeError):
    code = -32600


class FakeRpc:
    def __init__(self):
        self.calls = []
        self.threads = {}
        self.turn_count = 0
        self.fail_next = None

    def call(self, method, params):
        self.calls.append((method, params))
        if self.fail_next and self.fail_next[0] == method:
            error = self.fail_next[1]
            self.fail_next = None
            raise error
        if method == "thread/start":
            thread_id = "worker-" + str(len(self.threads) + 1)
            self.threads[thread_id] = {"id": thread_id, "cwd": params["cwd"], "status": {"type": "idle"}}
            return {"thread": self.threads[thread_id]}
        if method == "thread/resume":
            return {"thread": {"id": params["threadId"], "cwd": params["cwd"], "status": {"type": "idle"}}}
        if method == "turn/start":
            self.turn_count += 1
            return {"turn": {"id": "turn-" + str(self.turn_count), "status": "inProgress"}}
        if method == "turn/steer":
            return {"turnId": params["expectedTurnId"]}
        raise AssertionError("Unexpected RPC: " + method)


class WorkerFixture:
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.workspace = Path(self.temporary.name).resolve()
        self.state = self.workspace / ".de67/state/deadlines.sqlite3"
        self.state.parent.mkdir(parents=True)
        self.harness = DeadlineHarness(self.state)
        self.addCleanup(self.harness.close)
        self.binding = self.bind("sol-a", "run-a", "supervisor-a")
        self.rpc = FakeRpc()
        self.dispatcher = library.WorkerDispatcher(self.workspace, self.rpc, self.binding)

    def bind(self, thread_id, run_id, supervisor):
        socket = self.workspace / (run_id + ".sock")
        socket.touch()
        value = {"workspace": str(self.workspace), "role": "coordinator", "state": "active",
                 "thread_id": thread_id, "run_id": run_id, "runner_pid": 999999,
                 "server_pid": 999998, "socket": str(socket), "deadline_state": str(self.state),
                 "lineage": "project", "supervisor_id": supervisor, "turn_id": "sol-turn"}
        (self.workspace / ".de67/state/coordinator-input.json").write_text(json.dumps(value), encoding="utf-8")
        self.env = {"CODEX_THREAD_ID": thread_id, "DE67_COORDINATOR_RUN_ID": run_id}
        return value

    def worker(self, name="pilot", model="gpt-6-luna", effort="low"):
        return library.create(self.workspace, name, "Observe the current route", model, effort, environment=self.env)

    def task(self, task_id="task-a", text="Prove this exact assignment"):
        self.harness.start_task("project", task_id, "R-" + task_id, 3600, now=time.time())
        packet_dir = self.workspace / ".de67/state/worker-dispatch"
        packet_dir.mkdir(parents=True, exist_ok=True)
        packet = packet_dir / (task_id + ".md")
        packet.write_text(text, encoding="utf-8")
        digest = hashlib.sha256(packet.read_bytes()).hexdigest()
        from policy_kernel import current_owner_contract
        record_dispatch(self.workspace, self.state, "project", task_id, packet, digest,
                        {"owner_contract_sha256": hashlib.sha256(current_owner_contract(self.workspace).encode("utf-8")).hexdigest()})
        return packet, digest

    def assign(self, name="pilot", task_id="task-a", packet=None):
        packet, digest = packet or self.task(task_id)
        return library.assign(self.workspace, name, task_id, packet, digest, self.state, "project", environment=self.env)

    def returned(self, name="pilot", text="Verified the requested route", status="completed"):
        assignment = library.describe(self.workspace, name)["assignment"]
        self.dispatcher.observe({"method": "item/completed", "params": {
            "threadId": assignment["worker_id"], "turnId": assignment["turn_id"],
            "item": {"type": "agentMessage", "id": "final", "phase": "final", "text": text}}})
        self.dispatcher.observe({"method": "turn/completed", "params": {
            "threadId": assignment["worker_id"], "turn": {"id": assignment["turn_id"], "status": status}}})
        return library.describe(self.workspace, name)["assignment"]

    def settle(self, name="pilot"):
        assignment = library.describe(self.workspace, name)["assignment"]
        task_id, worker_id = assignment["task_id"], assignment["worker_id"]
        claim_id = library._task(self.state, "project", task_id)["claim_id"]
        receipt = {"schema": "de67.worker-result-receipt.v1", "lineage_id": "project", "task_id": task_id,
                   "claim_id": claim_id, "worker_id": worker_id, "disposition": "completed",
                   "verdict": "assigned behavior proven", "outcome": "Prove the assigned behavior",
                   "summary": "The coordinator inspected the worker's evidence", "material_changes": [],
                   "tests": ["Behavior checked"], "live_actions": [], "evidence_ceiling": ["Assigned task only"],
                   "accepted_no_replay": [], "active_work": [], "narrow_queries": [], "entrypoints": [],
                   "first_open_boundary": "", "bindings": {}, "artifacts": [], "journal_entries": [],
                   "first_divergence": None, "context_metrics": {}}
        saved = self.harness.record_worker_result_receipt("project", task_id, worker_id, receipt)
        self.harness.complete_task("project", task_id, "Assigned behavior accepted", receipt_id=saved["receipt_id"])


class WorkerLibraryTests(WorkerFixture, unittest.TestCase):
    def test_astra_low_dispatches_exact_app_server_input(self):
        worker = self.worker(model="gpt-6-astra", effort="low")
        self.assertEqual(worker["model"], "gpt-6-astra")
        self.assign()
        self.dispatcher.process_pending()
        starts = [params for method, params in self.rpc.calls if method == "thread/start"]
        self.assertEqual(starts[-1]["model"], "gpt-6-astra")
        self.assertEqual(starts[-1]["config"]["model_reasoning_effort"], "low")

    def test_coding_max_effort_reaches_worker_runtime(self):
        for model, name in (("gpt-6-luna", "luna-coder"), ("gpt-6-sol", "sol-coder")):
            self.worker(name=name, model=model, effort="max")
            self.assign(name=name, task_id=name)
            self.dispatcher.process_pending()
            start = next(params for method, params in reversed(self.rpc.calls)
                         if method == "thread/start")
            turn = next(params for method, params in reversed(self.rpc.calls)
                        if method == "turn/start")
            self.assertEqual(start["model"], model)
            self.assertEqual(start["config"]["model_reasoning_effort"], "max")
            self.assertEqual(turn["effort"], "max")

    def prepared_text(self, task_id, owner, constraint="Current constraint version one"):
        from worker_packet import standing_section
        return ("Current assigned task: " + task_id + "\nCurrent owner instructions: " + owner + "\n"
                + standing_section("common-guidance", "Standing guidance retained in the conversation. " * 8)
                + standing_section("worker-ownership", constraint)
                + "Current task mailbox: " + task_id + "\n")

    def turn_input(self):
        return [params["input"][0]["text"] for method, params in self.rpc.calls if method == "turn/start"][-1]

    def delivery_event(self, name="pilot"):
        assignment = library.describe(self.workspace, name)["assignment"]
        events = [json.loads(line) for line in Path(assignment["events_path"]).read_text(encoding="utf-8").splitlines()]
        return next(event["params"] for event in events if event.get("method") == "de67/workerDelivery/prepared")

    def test_fresh_worker_receives_whole_prepared_assignment_inline(self):
        self.worker()
        current = self.prepared_text("task-a", "Inspect the first owner constraint")
        packet = self.task(text=current)
        self.assign(packet=packet)
        self.dispatcher.process_pending()
        delivered = self.turn_input()
        self.assertIn(packet[0].read_bytes().decode("utf-8"), delivered)
        self.assertNotIn(str(packet[0]), delivered)
        self.assertNotIn(packet[1], delivered)
        self.assertNotIn("Read the complete current prepared task brief", delivered)
        event = self.delivery_event()
        self.assertEqual(event["canonical_packet"], str(packet[0]))
        self.assertEqual(event["canonical_sha256"], packet[1])
        self.assertFalse(event["comparison_available"])
        self.assertEqual(event["omitted_sections"], [])
        self.assertEqual(event["delivered_utf8_bytes"], len(packet[0].read_bytes()))

    def test_reuse_omits_identical_standing_but_delivers_changed_constraints_and_owner(self):
        self.worker()
        self.assign(packet=self.task(text=self.prepared_text("task-a", "Previous owner premise")))
        self.dispatcher.process_pending()
        previous = self.returned()
        self.settle()
        current = self.prepared_text("task-b", "Corrected owner premise", "Replacement current constraint")
        self.assign(task_id="task-b", packet=self.task("task-b", current))
        self.dispatcher.process_pending()
        delivered = self.turn_input()
        self.assertNotIn("Standing guidance retained in the conversation.", delivered)
        self.assertNotIn("Previous owner premise", delivered)
        self.assertNotIn("Current constraint version one", delivered)
        self.assertIn("Corrected owner premise", delivered)
        self.assertIn("Replacement current constraint", delivered)
        self.assertIn("Current assigned task: task-b", delivered)
        self.assertIn("Current task mailbox: task-b", delivered)
        self.assertIn("Changed standing sections below replace their earlier versions", delivered)
        event = self.delivery_event()
        self.assertEqual(event["previous_assignment_id"], previous["id"])
        self.assertEqual(event["omitted_sections"], ["common-guidance"])
        self.assertEqual(event["changed_sections"], ["worker-ownership"])
        self.assertLess(event["delivered_utf8_bytes"], event["full_utf8_bytes"])

    def test_untrusted_or_unconfirmed_prior_assignment_requires_full_delivery(self):
        for index, damage in enumerate(("altered", "missing", "uncertain-request", "missing-turn", "empty")):
            with self.subTest(damage=damage):
                name, first, second = "pilot-" + str(index), "prior-" + str(index), "current-" + str(index)
                self.worker(name)
                old_text = "" if damage == "empty" else self.prepared_text(first, "Previous owner")
                prior_packet = self.task(first, old_text)
                self.assign(name, first, prior_packet)
                self.dispatcher.process_pending()
                prior = self.returned(name)
                self.settle(name)
                if damage == "altered":
                    prior_packet[0].write_text(old_text + "Changed after delivery", encoding="utf-8")
                elif damage == "missing":
                    prior_packet[0].unlink()
                elif damage in {"uncertain-request", "missing-turn"}:
                    with closing(library._connect(self.workspace, write=True)) as db, db:
                        if damage == "uncertain-request":
                            db.execute("UPDATE requests SET status='uncertain' WHERE assignment_id=?", (prior["id"],))
                        else:
                            db.execute("UPDATE assignments SET turn_id=NULL WHERE id=?", (prior["id"],))
                current = self.prepared_text(second, "Current owner")
                current_packet = self.task(second, current)
                self.assign(name, second, current_packet)
                self.dispatcher.process_pending()
                self.assertIn(current_packet[0].read_bytes().decode("utf-8"), self.turn_input())
                self.assertFalse(self.delivery_event(name)["comparison_available"])

    def test_late_worker_return_keeps_evidence_after_sol_has_settled_task(self):
        self.worker()
        self.assign()
        self.dispatcher.process_pending()
        self.settle()
        task_before = library._task(self.state, "project", "task-a")
        assignment = self.returned(text="Late worker evidence")
        self.assertEqual(assignment["status"], "returned")
        self.assertEqual(Path(assignment["result_path"]).read_text(encoding="utf-8"), "Late worker evidence")
        self.assertEqual(library._task(self.state, "project", "task-a"), task_before)

    def test_shutdown_does_not_reopen_an_already_settled_worker_claim(self):
        self.worker()
        self.assign()
        self.dispatcher.process_pending()
        self.settle()
        task_before = library._task(self.state, "project", "task-a")
        self.dispatcher.shutdown()
        self.assertEqual(library.describe(self.workspace, "pilot")["assignment"]["status"], "interrupted")
        self.assertEqual(library._task(self.state, "project", "task-a"), task_before)

    def test_catalog_is_read_only_and_missing_adapter_is_visible(self):
        root = library._root(self.workspace)
        self.assertFalse(root.exists())
        self.assertEqual(library.catalog(self.workspace)["workers"], [])
        self.assertFalse(root.exists())
        Path(self.binding["socket"]).unlink()
        self.assertFalse(library.catalog(self.workspace)["runtime"]["available"])
        with self.assertRaisesRegex(library.WorkerLibraryError, "socket is unavailable"):
            self.worker()

    def test_named_worker_reuses_conversation_for_new_task_after_fresh_sol(self):
        self.worker()
        queued = self.assign()
        self.dispatcher.process_pending()
        self.assertEqual(library.request_status(self.workspace, queued["request_id"])["state"], "submitted")
        original = self.returned()
        self.assertIsNone(library._task(self.state, "project", "task-a")["attempt_terminal_at"])
        self.settle()
        self.dispatcher.shutdown()
        Path(self.binding["socket"]).unlink()
        self.binding = self.bind("sol-b", "run-b", "supervisor-b")
        self.dispatcher = library.WorkerDispatcher(self.workspace, self.rpc, self.binding)
        library.describe(self.workspace, "pilot", "Observe the corrected route", model="gpt-6-sol",
                         effort="medium", environment=self.env)
        self.assign(task_id="task-b")
        self.dispatcher.process_pending()
        current = library.describe(self.workspace, "pilot")
        self.assertEqual(current["thread_id"], original["worker_id"])
        self.assertEqual(current["assignment"]["task_id"], "task-b")
        resumed = [params for method, params in self.rpc.calls if method == "thread/resume"]
        self.assertEqual(resumed[-1]["model"], "gpt-6-sol")
        self.assertEqual(library.worker_owners(self.workspace, self.state, "project"), {original["worker_id"]: "sol-b"})

    def test_parallel_dispatch_binds_exact_tasks_not_queue_order(self):
        self.worker("first")
        self.worker("second")
        a, b = self.task("task-a"), self.task("task-b")
        self.assign("second", "task-b", b)
        self.assign("first", "task-a", a)
        self.dispatcher.process_pending()
        owned = library.owned_assignments(self.workspace, self.state, "project", coordinator_session_id="sol-a")
        self.assertEqual(owned, {"task-b": "worker-1", "task-a": "worker-2"})
        self.assertTrue(self.dispatcher.has_active_turns())

    def test_stale_caller_and_queued_binding_cannot_dispatch(self):
        self.worker()
        queued = self.assign()
        old_env = dict(self.env)
        self.bind("sol-b", "run-b", "supervisor-b")
        with self.assertRaisesRegex(library.WorkerLibraryError, "current Sol"):
            library.retire(self.workspace, "pilot", environment=old_env)
        self.dispatcher.process_pending()
        self.assertEqual(self.rpc.calls, [])
        self.assertEqual(library.request_status(self.workspace, queued["request_id"])["state"], "rejected")
        self.assertIsNone(library._task(self.state, "project", "task-a")["worker_id"])

    def test_registry_copy_is_not_another_workspaces_worker_library(self):
        self.worker()
        foreign = self.workspace / "foreign"
        shutil.copytree(library._root(self.workspace), library._root(foreign))
        with self.assertRaisesRegex(library.WorkerLibraryError, "different workspace"):
            library.catalog(foreign)

    def test_wrong_task_packet_and_changed_prepared_input_are_rejected(self):
        self.worker()
        a, b = self.task("task-a"), self.task("task-b")
        with self.assertRaisesRegex(library.WorkerLibraryError, "exact task"):
            self.assign(task_id="task-a", packet=b)
        queued = self.assign(task_id="task-a", packet=a)
        a[0].write_text("Changed after queue", encoding="utf-8")
        self.dispatcher.process_pending()
        self.assertEqual(library.request_status(self.workspace, queued["request_id"])["state"], "rejected")
        self.assertEqual(self.rpc.calls, [])

    def test_double_assignment_and_busy_retirement_are_rejected(self):
        self.worker()
        self.assign()
        second = self.task("task-b")
        with self.assertRaisesRegex(library.WorkerLibraryError, "unsettled"):
            self.assign(task_id="task-b", packet=second)
        with self.assertRaisesRegex(library.WorkerLibraryError, "idle"):
            library.retire(self.workspace, "pilot", environment=self.env)
        self.dispatcher.process_pending()
        self.returned()
        with self.assertRaisesRegex(library.WorkerLibraryError, "unsettled"):
            self.assign(task_id="task-b", packet=second)

    def test_same_task_steering_and_returned_continuation_do_not_create_new_claim(self):
        self.worker()
        self.assign()
        self.dispatcher.process_pending()
        library.message(self.workspace, "pilot", "Inspect the second observation", environment=self.env)
        self.dispatcher.process_pending()
        self.assertEqual(self.rpc.turn_count, 1)
        self.assertEqual(self.rpc.calls[-1][0], "turn/steer")
        self.returned()
        library.message(self.workspace, "pilot", "Continue with the existing evidence", environment=self.env)
        self.dispatcher.process_pending()
        self.assertEqual(self.rpc.turn_count, 2)
        delegated = self.harness.connection.execute("SELECT COUNT(*) FROM worker_checkpoints WHERE kind='delegated'").fetchone()[0]
        self.assertEqual(delegated, 1)

    def test_same_sol_can_resume_known_turn_after_new_server_and_supervisor(self):
        self.worker()
        self.assign()
        self.dispatcher.process_pending()
        original_worker = library.describe(self.workspace, "pilot")["thread_id"]
        self.dispatcher.shutdown("Simulated server shutdown")
        Path(self.binding["socket"]).unlink()
        self.binding = self.bind("sol-a", "run-recovery", "supervisor-recovery")
        self.dispatcher = library.WorkerDispatcher(self.workspace, self.rpc, self.binding)
        queued = library.message(self.workspace, "pilot", "Continue the same task", environment=self.env)
        self.dispatcher.process_pending()
        self.assertEqual(library.request_status(self.workspace, queued["request_id"])["state"], "submitted")
        claim = library._task(self.state, "project", "task-a")
        self.assertEqual((claim["worker_id"], claim["coordinator_session_id"], claim["supervisor_id"]),
                         (original_worker, "sol-a", "supervisor-a"))
        self.assertEqual(library.owned_assignments(self.workspace, self.state, "project", "sol-a"), {"task-a": original_worker})

    def test_new_sol_resumes_returned_open_assignment_after_review(self):
        import codex_runner
        from coordinator_supervisor import mutation_gate, active_worker_coordinator_session
        self.worker()
        self.assign()
        self.dispatcher.process_pending()
        (self.workspace / ".de67/mutation-suggestions.md").write_text(
            "## Pending suggestions\n\n- Owner-authorized [trigger]: Review now.\n")
        self.assertIsNone(mutation_gate(self.state, "project", self.workspace))
        with self.assertRaisesRegex(Exception, "worker attempt is running"):
            self.harness.retire_claim_clocks_for_mutation("project", "review")
        self.returned(text="Partial evidence; native proof remains")
        self.assertIsNone(active_worker_coordinator_session(self.state, "project"))
        before = dict(library._task(self.state, "project", "task-a"))
        before_claim = tuple(self.harness.connection.execute('SELECT * FROM worker_claims').fetchone())
        self.assertIsNone(before["attempt_terminal_at"])
        self.assertIsNotNone(mutation_gate(self.state, "project", self.workspace))
        self.harness.retire_claim_clocks_for_mutation("project", "review")
        Path(self.binding["socket"]).unlink()
        self.binding = self.bind("sol-b", "run-b", "supervisor-b")
        self.dispatcher = library.WorkerDispatcher(self.workspace, self.rpc, self.binding)
        queued = library.message(self.workspace, "pilot", "Resume the remaining native proof", environment=self.env)
        self.dispatcher.process_pending()
        runner_env = {**self.env, 'DE67_WORKSPACE': str(self.workspace),
                      'DE67_DEADLINE_STATE': str(self.state), 'DE67_LINEAGE': 'project',
                      'DE67_SUPERVISOR_PID': 'supervisor-b'}
        guard = codex_runner.CoordinatorLoopGuard(
            initial_unbound_tasks=('task-a',),
            roster_resolver=codex_runner._roster_resolver(self.workspace, runner_env),
            claim_recorder=codex_runner._claim_recorder(runner_env),
            task_terminal=codex_runner._task_terminal_resolver(runner_env),
        )
        guard.observe({'type': 'thread.started', 'thread_id': 'sol-b'})
        guard.reconcile_handoffs()
        self.assertEqual(guard.unbound_tasks, ())
        guard.observe({'type': 'item.started', 'item': {
            'type': 'collab_tool_call', 'tool': 'wait', 'status': 'in_progress'}})
        self.assertEqual(library.request_status(self.workspace, queued["request_id"])["state"], "submitted")
        self.assertEqual(active_worker_coordinator_session(self.state, "project"), "sol-b")
        after = library._task(self.state, "project", "task-a")
        for key in ("worker_id", "coordinator_session_id", "supervisor_id", "attempt_terminal_at"):
            self.assertEqual(after[key], before[key])
        self.assertEqual(self.rpc.turn_count, 2)
        self.assertEqual(self.harness.connection.execute("SELECT COUNT(*) FROM worker_claims").fetchone()[0], 1)
        record = codex_runner._claim_recorder(runner_env)
        for task, worker, coordinator in [('task-a', 'different-worker', 'sol-b'),
                                          ('unknown-task', before['worker_id'], 'sol-b'),
                                          ('task-a', before['worker_id'], 'foreign-coordinator')]:
            with self.subTest(task=task, worker=worker, coordinator=coordinator):
                with self.assertRaises(codex_runner.RunnerError):
                    record(task, worker, coordinator)
        foreign = codex_runner._claim_recorder({**runner_env, 'DE67_SUPERVISOR_PID': 'foreign-supervisor'})
        with self.assertRaises(codex_runner.RunnerError):
            foreign('task-a', before['worker_id'], 'sol-b')
        with closing(library._connect(self.workspace, write=True)) as db:
            saved_supervisor = db.execute('SELECT claim_supervisor_id FROM assignments WHERE task_id=?',
                                         ('task-a',)).fetchone()[0]
            db.execute('UPDATE assignments SET claim_supervisor_id=? WHERE task_id=?',
                       ('foreign-claim-owner', 'task-a'))
            db.commit()
        with self.assertRaises(codex_runner.RunnerError):
            record('task-a', before['worker_id'], 'sol-b')
        with closing(library._connect(self.workspace, write=True)) as db:
            db.execute('UPDATE assignments SET claim_supervisor_id=? WHERE task_id=?',
                       (saved_supervisor, 'task-a'))
            db.commit()
        record('task-a', before['worker_id'], 'sol-b')
        with closing(library._connect(self.workspace)) as db:
            original_binding = db.execute('SELECT binding FROM assignments WHERE task_id=?',
                                          ('task-a',)).fetchone()[0]
        for field in ('workspace', 'deadline_state', 'lineage'):
            changed = json.loads(original_binding)
            changed[field] = 'foreign-binding'
            with closing(library._connect(self.workspace, write=True)) as db:
                db.execute('UPDATE assignments SET binding=? WHERE task_id=?',
                           (json.dumps(changed), 'task-a'))
                db.commit()
            with self.subTest(field=field), self.assertRaises(codex_runner.RunnerError):
                record('task-a', before['worker_id'], 'sol-b')
        with closing(library._connect(self.workspace, write=True)) as db:
            db.execute('UPDATE assignments SET binding=? WHERE task_id=?', (original_binding, 'task-a'))
            db.commit()
        record('task-a', before['worker_id'], 'sol-b')
        self.assertEqual(self.harness.connection.execute("SELECT COUNT(*) FROM worker_claims").fetchone()[0], 1)
        self.assertEqual(tuple(self.harness.connection.execute('SELECT * FROM worker_claims').fetchone()), before_claim)
        self.assertEqual(self.harness.connection.execute(
            "SELECT COUNT(*) FROM worker_checkpoints WHERE kind='delegated'").fetchone()[0], 1)
        self.settle()

    def test_new_sol_cannot_adopt_running_assignment(self):
        self.worker()
        self.assign()
        self.dispatcher.process_pending()
        Path(self.binding["socket"]).unlink()
        self.bind("sol-b", "run-b", "supervisor-b")
        with self.assertRaisesRegex(library.WorkerLibraryError, "this coordinator"):
            library.message(self.workspace, "pilot", "Continue", environment=self.env)

    def test_point_of_use_context_covers_sessions_without_repeating_on_wait(self):
        self.assertIn("session_id", library.interaction_guidance("assign"))
        self.assertIn("write_stdin", library.interaction_guidance("message"))
        self.assertIsNone(library.interaction_guidance("wait"))

    def test_uncertain_turn_start_is_not_replayed_or_reassigned(self):
        self.worker()
        queued = self.assign()
        self.rpc.fail_next = ("turn/start", TimeoutError("Lost RPC receipt"))
        self.dispatcher.process_pending()
        count = len(self.rpc.calls)
        self.dispatcher.process_pending()
        self.assertEqual(len(self.rpc.calls), count)
        self.assertEqual(library.request_status(self.workspace, queued["request_id"])["state"], "uncertain")
        self.assertEqual(library.describe(self.workspace, "pilot")["status"], "uncertain")
        with self.assertRaises(library.WorkerLibraryError):
            library.message(self.workspace, "pilot", "Try again", environment=self.env)
        self.dispatcher.shutdown()
        self.assertEqual(library.describe(self.workspace, "pilot")["status"], "interrupted")
        self.assertEqual(library.request_status(self.workspace, queued["request_id"])["state"], "uncertain")

    def test_rejected_queue_does_not_bind_worker_to_another_workers_claim(self):
        self.worker()
        request = self.assign()
        self.harness.claim_worker("project", "task-a", "other-worker", "sol-a", "supervisor-a")
        self.dispatcher.process_pending()
        self.assertEqual(library.request_status(self.workspace, request["request_id"])["state"], "rejected")
        self.assertEqual(library.describe(self.workspace, "pilot")["status"], "idle")
        self.assign(task_id="task-b")
        self.dispatcher.process_pending()
        self.assertEqual(library._task(self.state, "project", "task-a")["worker_id"], "other-worker")
        self.assertEqual(library._task(self.state, "project", "task-b")["worker_id"], "worker-1")

    def test_owner_correction_during_thread_load_rejects_before_claim_or_generation(self):
        owner = self.workspace / ".de67/WEC.md"
        owner.write_text("<!-- DE67:OWNER-CONTRACT:BEGIN -->\nOriginal owner instruction\n<!-- DE67:OWNER-CONTRACT:END -->\n", encoding="utf-8")
        self.worker()
        request = self.assign()
        original_call = self.rpc.call

        def owner_changes_during_load(method, params):
            result = original_call(method, params)
            if method == "thread/start":
                owner.write_text("<!-- DE67:OWNER-CONTRACT:BEGIN -->\nCorrected owner instruction\n<!-- DE67:OWNER-CONTRACT:END -->\n", encoding="utf-8")
            return result

        with patch.object(self.rpc, "call", side_effect=owner_changes_during_load):
            self.dispatcher.process_pending()
        status = library.request_status(self.workspace, request["request_id"])
        self.assertEqual(status["state"], "rejected")
        self.assertIn("Owner instructions changed", status["error"])
        self.assertIsNone(library._task(self.state, "project", "task-a")["worker_id"])
        self.assertIsNone(library.describe(self.workspace, "pilot")["thread_id"])
        self.assertEqual(self.rpc.turn_count, 0)

    def test_definite_turn_rejection_keeps_known_claim_resumable(self):
        self.worker()
        self.assign()
        self.rpc.fail_next = ("turn/start", Rejected("Rejected before start"))
        self.dispatcher.process_pending()
        self.assertEqual(library.describe(self.workspace, "pilot")["status"], "failed")
        queued = library.message(self.workspace, "pilot", "The issue is corrected; continue", environment=self.env)
        self.dispatcher.process_pending()
        self.assertEqual(library.request_status(self.workspace, queued["request_id"])["state"], "submitted")
        self.assertEqual(self.rpc.turn_count, 1)

    def test_worker_result_and_usage_are_preserved_outside_sol_context(self):
        self.worker()
        self.assign()
        self.dispatcher.process_pending()
        assignment = library.describe(self.workspace, "pilot")["assignment"]
        self.dispatcher.observe({"method": "thread/tokenUsage/updated", "params": {
            "threadId": assignment["worker_id"], "tokenUsage": {"total": {"inputTokens": 123}}}})
        final = self.returned(text="A precise worker result")
        self.assertEqual(Path(final["result_path"]).read_text(), "A precise worker result")
        events = Path(library.describe(self.workspace, "pilot")["assignment"]["events_path"]).read_text()
        self.assertIn("tokenUsage", events)
        self.assertIsNone(library._task(self.state, "project", "task-a")["attempt_terminal_at"])
        checkpoint = self.harness.connection.execute("SELECT evidence FROM worker_checkpoints WHERE kind='worker-return'").fetchone()[0]
        self.assertFalse(json.loads(checkpoint)["terminal_authority"])
        from agent_mailbox import pending
        notice = pending(self.workspace, "coordinator")[-1][1]
        self.assertIn(final["result_path"], notice["text"])
        self.assertNotIn("A precise worker result", notice["text"])
        self.dispatcher.shutdown()
        self.assertEqual(library.describe(self.workspace, "pilot")["status"], "returned")


if __name__ == "__main__":
    unittest.main()
