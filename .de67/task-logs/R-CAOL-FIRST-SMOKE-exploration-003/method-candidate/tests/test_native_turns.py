from __future__ import annotations

import json
import os
from pathlib import Path
import sqlite3
import sys
import tempfile
import time
import unittest
from unittest.mock import patch


SCRIPTS = Path(__file__).resolve().parents[1] / "scripts"
sys.path.insert(0, str(SCRIPTS))

from coordinator_supervisor import _complete_mutation_review, mutation_gate  # noqa: E402
from deadline_harness import DeadlineError, DeadlineHarness  # noqa: E402
from native_turns import quiet_assignments, returned_assignments  # noqa: E402
from policy_kernel import workspace_facts  # noqa: E402
from worker_library import _connect  # noqa: E402


class NativeReturnTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.workspace = Path(self.temporary.name) / "workspace"
        self.workspace.mkdir()
        self.workspace = self.workspace.resolve()
        self.state = self.workspace / ".de67/state/deadlines.sqlite3"
        self.codex_state = Path(self.temporary.name) / "codex.sqlite3"
        self.coordinator = "coordinator-1"
        self.started = time.time()
        self.birth = "Fri Sep 25 09:00:00 2026"
        self.known_birth: dict[int, str | None] = {31415: None}
        self.environment = patch.dict(os.environ, {"DE67_CODEX_STATE": str(self.codex_state)})
        self.environment.start()
        self.addCleanup(self.environment.stop)
        self.process = patch("native_turns._process_birth", side_effect=self.known_birth.get)
        self.process.start()
        self.addCleanup(self.process.stop)
        with sqlite3.connect(self.codex_state) as db:
            db.executescript("""
                CREATE TABLE threads (
                    id TEXT PRIMARY KEY, rollout_path TEXT, cwd TEXT, agent_path TEXT);
                CREATE TABLE thread_spawn_edges (
                    parent_thread_id TEXT, child_thread_id TEXT, status TEXT);
            """)
        self._transport(self.coordinator)
        (self.workspace / ".de67/mutation-suggestions.md").write_text(
            "## Pending suggestions\n\n- [trigger]: Review returned work.\n",
            encoding="utf-8",
        )
        (self.workspace / ".de67/FS.md").write_text(
            "# FS\n\nStatus: Frozen\n\n- [ ] R-NATIVE — Continue proof.\n",
            encoding="utf-8",
        )
        (self.workspace / ".de67/work-ledger.md").write_text(
            "# Work ledger\n\n## Active work\n\n- [ ] R-NATIVE — Continue proof.\n",
            encoding="utf-8",
        )

    def _transport(self, coordinator: str, *, status: str = "done", birth: str | None = None) -> Path:
        run = self.workspace / ".de67/state/runner-runs" / coordinator
        run.mkdir(parents=True, exist_ok=True)
        (run / "events.jsonl").write_text(
            json.dumps({"type": "thread.started", "thread_id": coordinator}) + "\n",
            encoding="utf-8",
        )
        (run / "status.json").write_text(json.dumps({
            "status": status, "session_id": coordinator, "exit_code": 0,
            "transport_kind": "cli", "transport_pid": 31415,
            "transport_birth": self.birth if birth is None else birth,
        }), encoding="utf-8")
        return run

    def _thread(self, thread: str, parent: str, path: str,
                events: list[tuple[str, str]], *, meta_parent: str | None = None) -> Path:
        rollout = Path(self.temporary.name) / "sessions" / f"rollout-{thread}.jsonl"
        rollout.parent.mkdir(exist_ok=True)
        values = [{"type": "session_meta", "payload": {
            "id": thread, "parent_thread_id": meta_parent or parent,
            "cwd": str(self.workspace), "agent_path": path, "thread_source": "subagent",
            "source": {"subagent": {"thread_spawn": {
                "parent_thread_id": meta_parent or parent, "agent_path": path,
            }}},
        }}]
        values += [{"type": "event_msg", "payload": {"type": kind, "turn_id": turn}}
                   for kind, turn in events]
        rollout.write_text("\n".join(map(json.dumps, values)) + "\n", encoding="utf-8")
        with sqlite3.connect(self.codex_state) as db:
            db.execute("INSERT INTO threads VALUES (?,?,?,?)",
                       (thread, str(rollout), str(self.workspace), path))
            db.execute("INSERT INTO thread_spawn_edges VALUES (?,?,?)",
                       (parent, thread, "open"))  # Open is historical, not turn liveness.
        return rollout

    def _task(self, task: str = "native", *, returned: bool = True,
              coordinator: str | None = None) -> tuple[str, Path]:
        coordinator = coordinator or self.coordinator
        worker = "worker-" + task
        path = "/root/task_" + task.encode().hex()
        with DeadlineHarness(self.state) as harness:
            harness.start_task("project", task, "R-NATIVE", 100, now=self.started)
            harness.claim_worker("project", task, worker, coordinator, "supervisor-1", now=self.started + 1)
        events = [("task_started", "turn-1")]
        if returned:
            events.append(("task_complete", "turn-1"))
        return worker, self._thread(worker, coordinator, path, events)

    def test_returned_partial_work_opens_review_and_retires_only_the_clock(self) -> None:
        self._task()
        self.assertEqual(returned_assignments(self.workspace, self.state, "project"), {"native"})
        facts = workspace_facts(self.workspace, self.state, "project", now=self.started + 2)
        self.assertNotIn("live_task", facts)
        self.assertEqual(mutation_gate(self.state, "project", self.workspace).kind, "owner-suggestion")
        with DeadlineHarness(self.state) as harness:
            self.assertEqual(harness.retire_claim_clocks_for_mutation("project", "review", now=self.started + 2), 1)
            task = harness._task("project", "native")
            self.assertIsNone(task["attempt_terminal_at"])
            self.assertEqual(task["claim_id"], "R-NATIVE")
            self.assertEqual(harness.connection.execute(
                "SELECT worker_id,released_at FROM worker_claims WHERE task_id='native'"
            ).fetchone()["worker_id"], "worker-native")

    def test_active_child_blocks_gate_and_retirement(self) -> None:
        worker, _ = self._task()
        self._thread("child-1", worker, "/root/task_" + b"native".hex() + "/helper",
                     [("task_started", "child-turn")])
        self.assertEqual(returned_assignments(self.workspace, self.state, "project"), set())
        self.assertIn("live_task", workspace_facts(self.workspace, self.state, "project", now=self.started + 2))
        self.assertIsNone(mutation_gate(self.state, "project", self.workspace))
        with DeadlineHarness(self.state) as harness, self.assertRaisesRegex(DeadlineError, "worker attempt is running"):
            harness.retire_claim_clocks_for_mutation("project", "review", now=self.started + 2)

    def test_ambiguous_turn_or_transport_fails_closed(self) -> None:
        _worker, rollout = self._task()
        run = self.workspace / ".de67/state/runner-runs" / self.coordinator
        status = run / "status.json"
        original = status.read_text()
        for payload in (
            {**json.loads(original), "status": "running"},
            {**json.loads(original), "transport_birth": None},
            {**json.loads(original), "exit_code": 2},
        ):
            with self.subTest(payload=payload):
                status.write_text(json.dumps(payload))
                self.assertEqual(returned_assignments(self.workspace, self.state, "project"), set())
        status.unlink()
        self.assertEqual(returned_assignments(self.workspace, self.state, "project"), set())
        status.write_text(original)
        events = run / "events.jsonl"
        saved_events = events.read_text()
        events.unlink()
        self.assertEqual(returned_assignments(self.workspace, self.state, "project"), set())
        events.write_text(saved_events)
        self.known_birth[31415] = self.birth
        self.assertEqual(returned_assignments(self.workspace, self.state, "project"), set())
        self.known_birth[31415] = None
        with rollout.open("a") as output:
            output.write(json.dumps({"type": "event_msg", "payload": {
                "type": "task_started", "turn_id": "turn-2"}}) + "\n")
        self.assertEqual(returned_assignments(self.workspace, self.state, "project"), set())

    def test_released_historical_registry_row_and_independent_assignment(self) -> None:
        self._task("native")
        with DeadlineHarness(self.state) as harness:
            harness.start_task("project", "historical", "R-HISTORY", 50, now=self.started)
            harness.claim_worker("project", "historical", "named-old", self.coordinator,
                                 "supervisor-1", now=self.started + 1)
            harness.connection.execute(
                "UPDATE worker_claims SET released_at=? WHERE task_id='historical'", (self.started + 2,))
            harness.connection.execute(
                "UPDATE tasks SET attempt_terminal_at=?, attempt_terminal_kind='restart_normalized' "
                "WHERE task_id='historical'", (self.started + 2,))
            harness.connection.commit()
        with _connect(self.workspace, write=True) as db:
            db.execute("INSERT INTO workers VALUES (?,?,?,?,?,?,?)",
                       ("old", "old", "gpt-6-luna", "low", "named-old", 1, None))
            db.execute("INSERT INTO assignments VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
                       ("old-assignment", "old", "historical", str(self.state), "project",
                        "packet", "digest", None, json.dumps({"workspace": str(self.workspace),
                        "deadline_state": str(self.state), "lineage": "project",
                        "thread_id": self.coordinator, "supervisor_id": "supervisor-1"}),
                        "running", "named-old", "supervisor-1", None, None, None, None, 1, 1))
            db.commit()
        self.assertEqual(quiet_assignments(self.workspace, self.state, "project"), {"native"})
        self.assertNotIn("live_task", workspace_facts(self.workspace, self.state,
                                                       "project", now=self.started + 3))
        self.assertIsNotNone(mutation_gate(self.state, "project", self.workspace))
        self._task("active", returned=False)
        self.assertIn("live_task", workspace_facts(self.workspace, self.state, "project", now=self.started + 5))
        self.assertIsNone(mutation_gate(self.state, "project", self.workspace))

    def test_final_text_and_aborted_child_do_not_prove_return(self) -> None:
        worker, rollout = self._task("native", returned=False)
        with rollout.open("a") as output:
            output.write(json.dumps({"type": "response_item", "payload": {
                "type": "message", "phase": "final_answer", "content": "I am done"}}) + "\n")
        self.assertEqual(returned_assignments(self.workspace, self.state, "project"), set())
        with rollout.open("a") as output:
            output.write(json.dumps({"type": "event_msg", "payload": {
                "type": "task_complete", "turn_id": "turn-1"}}) + "\n")
        child = self._thread("child-1", worker, "/root/task_" + b"native".hex() + "/helper",
                             [("task_started", "child-turn"), ("turn_aborted", "child-turn")])
        self.assertEqual(returned_assignments(self.workspace, self.state, "project"), set())
        with child.open("a") as output:
            for kind in ("task_started", "task_complete"):
                output.write(json.dumps({"type": "event_msg", "payload": {
                    "type": kind, "turn_id": "later-child-turn"}}) + "\n")
        self.assertEqual(returned_assignments(self.workspace, self.state, "project"), {"native"})

    def test_latest_completed_turn_supersedes_older_incomplete_turn(self) -> None:
        _worker, rollout = self._task("native", returned=False)
        with rollout.open("a") as output:
            for kind in ("task_started", "task_complete"):
                output.write(json.dumps({"type": "event_msg", "payload": {
                    "type": kind, "turn_id": "turn-2"}}) + "\n")
        self.assertEqual(returned_assignments(self.workspace, self.state, "project"), {"native"})

    def test_reviewer_handoff_keeps_task_open(self) -> None:
        self._task()
        with DeadlineHarness(self.state) as harness:
            harness.checkpoint_worker("project", "native", "worker-native", "partial",
                                      "Retained original evidence", now=self.started + 2)
            original = [tuple(row) for row in harness.connection.execute(
                "SELECT sequence,kind,evidence FROM worker_checkpoints WHERE task_id='native'")]
        gate = mutation_gate(self.state, "project", self.workspace)
        self.assertIsNotNone(gate)
        assert gate is not None
        fake = Path(self.temporary.name) / "reviewer.py"
        fake.write_text(
            "import os, pathlib, sys\n"
            "sys.path.insert(0, os.environ['CANDIDATE_SCRIPTS'])\n"
            "from deadline_harness import DeadlineHarness\n"
            "workspace=pathlib.Path(os.environ['DE67_WORKSPACE'])\n"
            "assert os.environ['DE67_PROCESS_ROLE']=='mutation-reviewer'\n"
            "assert 'exclusive Phase-3 mutation reviewer' in sys.stdin.read()\n"
            "(workspace/'.de67/mutation-suggestions.md').write_text('## Pending suggestions\\n\\n')\n"
            "with DeadlineHarness(os.environ['DE67_DEADLINE_STATE']) as harness:\n"
            " assert harness._task('project','native')['attempt_terminal_at'] is None\n"
            " harness.request_coordinator_restart('project','review complete')\n"
            "(workspace/'.de67/reviewer-ran').write_text('yes')\n",
            encoding="utf-8",
        )
        run_root = self.workspace / ".de67/state/coordinator-runs"
        run_root.mkdir()
        with patch.dict(os.environ, {"CANDIDATE_SCRIPTS": str(SCRIPTS)}):
            restart = _complete_mutation_review(
                [sys.executable, str(fake)], self.workspace, self.state,
                "project", run_root, gate,
                extra_env={"CANDIDATE_SCRIPTS": str(SCRIPTS)},
            )
        self.assertTrue(restart.required)
        self.assertEqual((self.workspace / ".de67/reviewer-ran").read_text(), "yes")
        with DeadlineHarness(self.state) as harness:
            self.assertIsNone(harness._task("project", "native")["attempt_terminal_at"])
            self.assertEqual(harness._task("project", "native")["claim_id"], "R-NATIVE")
            self.assertIsNone(harness.connection.execute(
                "SELECT released_at FROM worker_claims WHERE task_id='native'"
            ).fetchone()["released_at"])
            self.assertEqual([tuple(row) for row in harness.connection.execute(
                "SELECT sequence,kind,evidence FROM worker_checkpoints WHERE task_id='native'")],
                             original)


if __name__ == "__main__":
    unittest.main()
