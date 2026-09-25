from __future__ import annotations
import json, os, shlex, shutil, subprocess, sys, tempfile, time, unittest
from pathlib import Path
from unittest.mock import patch
SCRIPTS = Path(__file__).resolve().parents[1] / "scripts"; sys.path.insert(0, str(SCRIPTS))
if sys.platform == "win32":
    raise unittest.SkipTest("The macOS supervisor service requires POSIX process and locking APIs")
import supervisor_service  # noqa: E402
from deadline_harness import DeadlineHarness  # noqa: E402
from policy_kernel import workspace_facts  # noqa: E402

class SupervisorServiceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(); self.root = Path(self.temp.name)
        self.workspace = (self.root / "workspace").resolve(); (self.workspace / ".de67/state").mkdir(parents=True)
        self.state = self.workspace / ".de67/state/deadlines.sqlite3"
        with DeadlineHarness(self.state):
            pass
        (self.workspace / ".de67/state/workspace.json").write_text(json.dumps(
            {"workspace": str(self.workspace), "clock": {"state": str(self.state), "lineage": "lineage"}}))
        self.platform = patch("supervisor_service.sys.platform", "darwin"); self.platform.start()
        self.legacy_loaded = patch("supervisor_service._legacy_loaded", return_value=False)
        self.legacy_loaded.start()
    def tearDown(self): self.legacy_loaded.stop(); self.platform.stop(); self.temp.cleanup()

    def test_cleanup_removes_stale_binding_and_preserves_owner_conversation(self):
        with patch.dict(os.environ, {"CODEX_HOME": str(self.root / "codex")}):
            socket = self.root / "codex/state/de67-input/0123456789abcdef.sock"
            socket.parent.mkdir(parents=True)
            socket.touch()
            address = self.workspace / ".de67/state/coordinator-input.json"
            address.write_text(json.dumps({"workspace": str(self.workspace),
                                          "runner_pid": 12345, "socket": str(socket)}))
            owner = self.workspace / ".de67/state/mutator-input.json"
            owner.write_text('{"owner": true}')
            with patch("codex_app_server_runner.process_snapshot", return_value={}):
                captured = supervisor_service._capture_coordinator_runtime(self.workspace)
                supervisor_service._cleanup_coordinator_runtime(self.workspace, captured)
            self.assertFalse(socket.exists())
            self.assertFalse(address.exists())
            self.assertEqual(owner.read_text(), '{"owner": true}')
            self.assertIsNone(supervisor_service._capture_coordinator_runtime(self.workspace))

    def test_capture_rejects_socket_outside_owned_directory(self):
        address = self.workspace / ".de67/state/coordinator-input.json"
        address.write_text(json.dumps({"workspace": str(self.workspace),
                                      "runner_pid": 12345,
                                      "socket": str(self.root / "0123456789abcdef.sock")}))
        self.assertIsNone(supervisor_service._capture_coordinator_runtime(self.workspace))

    @patch("supervisor_service.shutil.which")
    def test_spec_preserves_environment_without_auto_restart(self, which):
        which.side_effect = lambda value: {"codex": "/opt/bin/codex", "tmux": "/opt/bin/tmux"}.get(value)
        spec = supervisor_service.service_spec(self.workspace); words = shlex.split(spec.shell_command)
        self.assertEqual(spec.tmux, "/opt/bin/tmux"); self.assertEqual(words[0], "exec")
        self.assertIn("DE67_CODEX=/opt/bin/codex", words); self.assertIn(f"PATH={os.environ['PATH']}", words)
        self.assertIn("DE67_COORDINATOR_SANDBOX=danger-full-access", words)
        self.assertEqual(
            len([word for word in words if word.startswith("DE67_SUPERVISOR_START_TOKEN=")]),
            1,
        )
        self.assertIn(str(self.state), words); self.assertIn("--runner", words); self.assertNotIn("launchctl", words)

    @patch("supervisor_service.shutil.which", return_value="/opt/bin/tmux")
    @patch("supervisor_service.subprocess.run")
    def test_status_and_stop_work_without_workspace_config(self, run, _which):
        run.return_value = subprocess.CompletedProcess([], 1, "", "can't find session")
        broken = self.root / "missing"; self.assertEqual(supervisor_service.status_service(broken)[1], "stopped")
        supervisor_service.stop_service(broken)

    @patch("supervisor_service.shutil.which")
    @patch("supervisor_service.subprocess.run")
    @patch("supervisor_service._normalize_explicit_start")
    def test_start_creates_and_verifies_session(self, normalize, run, which):
        which.side_effect = lambda value: "/opt/bin/tmux" if value == "tmux" else "/opt/bin/codex"
        run.side_effect = [subprocess.CompletedProcess([],1,"","can't find session"),
                           subprocess.CompletedProcess([],0,"",""),
                           subprocess.CompletedProcess([],0,"",""),
                           subprocess.CompletedProcess([],0,"",""),
                           subprocess.CompletedProcess([],0,"","")]
        spec = supervisor_service.start_service(self.workspace)
        command = run.call_args_list[1].args[0]
        self.assertEqual(command[0:2], ["/opt/bin/tmux", "-S"])
        self.assertEqual(command[3:7], ["new-session","-d","-s",spec.label])
        normalize.assert_called_once_with(self.state.resolve(), "lineage")
        self.assertIn("wait-for", command[-1])
        environment = SCRIPTS.parent / "assets/environment"
        for name in ("phase3-policy.json", "phase3-contracts.json", "phase3-policy.d67"):
            self.assertEqual(
                (self.workspace / ".de67" / name).read_bytes(),
                (environment / name).read_bytes(),
            )

    @patch("supervisor_service.shutil.which")
    @patch("supervisor_service.subprocess.run")
    @patch("supervisor_service._normalize_explicit_start")
    def test_start_rejects_immediate_exit_without_normalizing(self, normalize, run, which):
        which.side_effect = lambda value: "/opt/bin/tmux" if value == "tmux" else "/opt/bin/codex"
        run.side_effect = [subprocess.CompletedProcess([],1,"","can't find session"),
                           subprocess.CompletedProcess([],0,"",""), subprocess.CompletedProcess([],1,"","can't find session")]
        with self.assertRaisesRegex(supervisor_service.ServiceError, "exited during"):
            supervisor_service.start_service(self.workspace)
        normalize.assert_not_called()

    @patch("supervisor_service.shutil.which")
    @patch("supervisor_service.subprocess.run")
    @patch("supervisor_service._normalize_explicit_start")
    def test_start_rejects_tmux_creation_failure_without_normalizing(self, normalize, run, which):
        which.side_effect = lambda value: "/opt/bin/tmux" if value == "tmux" else "/opt/bin/codex"
        run.side_effect = [
            subprocess.CompletedProcess([], 1, "", "can't find session"),
            subprocess.CompletedProcess([], 1, "", "tmux refused session"),
        ]
        with self.assertRaisesRegex(supervisor_service.ServiceError, "tmux refused session"):
            supervisor_service.start_service(self.workspace)
        normalize.assert_not_called()

    @patch("supervisor_service.shutil.which", return_value="/opt/bin/tmux")
    @patch("supervisor_service.os.killpg")
    @patch("supervisor_service.subprocess.run")
    def test_stop_kills_exact_session_and_its_process_group(self, run, killpg, _which):
        killpg.side_effect = [None, None, None]
        run.side_effect = [
            subprocess.CompletedProcess([],0,"",""),
            subprocess.CompletedProcess([],0,"4312\n",""),
            subprocess.CompletedProcess([],0,"",""),
        ]
        spec = supervisor_service.stop_service(self.workspace)
        pane_command = run.call_args_list[1].args[0]
        self.assertEqual(pane_command[3:], [
            "list-panes", "-t", f"={spec.label}", "-F", "#{pane_pid}",
        ])
        self.assertEqual(killpg.call_args_list, [
            unittest.mock.call(4312, supervisor_service.signal.SIGTERM),
            unittest.mock.call(4312, 0),
            unittest.mock.call(4312, supervisor_service.signal.SIGKILL),
        ])
        command = run.call_args_list[2].args[0]
        self.assertEqual(command[0:2], ["/opt/bin/tmux", "-S"])
        self.assertEqual(command[3:], ["kill-session","-t",f"={spec.label}"])

    @patch("supervisor_service.os.killpg", side_effect=PermissionError)
    def test_reused_inaccessible_group_is_not_signaled(self, killpg):
        supervisor_service._kill_surviving_process_groups((4312,))
        killpg.assert_called_once_with(4312, 0)

    @patch("supervisor_service.shutil.which", return_value="/opt/bin/tmux")
    @patch("supervisor_service.subprocess.run")
    def test_missing_tmux_server_is_normal_absence(self, run, _which):
        run.return_value = subprocess.CompletedProcess([],1,"","no server running on /tmp/tmux")
        self.assertEqual(supervisor_service.status_service(self.workspace)[1], "stopped")

    @patch("supervisor_service.shutil.which", return_value="/opt/bin/tmux")
    @patch("supervisor_service.subprocess.run")
    def test_exiting_final_tmux_server_is_normal_absence(self, run, _which):
        run.return_value = subprocess.CompletedProcess([],1,"","server exited unexpectedly")
        self.assertEqual(supervisor_service.status_service(self.workspace)[1], "stopped")

    @patch("supervisor_service._remove_legacy")
    @patch("supervisor_service.shutil.which", return_value="/opt/bin/tmux")
    @patch("supervisor_service.subprocess.run")
    def test_stop_migrates_legacy_launchagent(self, run, _which, remove_legacy):
        run.return_value = subprocess.CompletedProcess([],1,"","no server running")
        spec = supervisor_service.stop_service(self.workspace)
        remove_legacy.assert_called_once()
        self.assertEqual(remove_legacy.call_args.args[0].label, spec.label)

    def test_legacy_status_does_not_require_tmux(self):
        with patch("supervisor_service._legacy_loaded", return_value=True), patch(
            "supervisor_service.shutil.which", return_value=None
        ):
            _spec, status = supervisor_service.status_service(self.workspace)
        self.assertEqual(status, "legacy-launchagent-loaded")

    @patch("supervisor_service.shutil.which")
    def test_start_preserves_live_legacy_owner(self, which):
        which.side_effect = lambda value: "/opt/bin/tmux" if value == "tmux" else "/opt/bin/codex"
        with patch("supervisor_service._legacy_loaded", return_value=True), patch(
            "supervisor_service._remove_legacy"
        ) as remove:
            with self.assertRaisesRegex(supervisor_service.ServiceError, "run stop, then start"):
                supervisor_service.start_service(self.workspace)
        remove.assert_not_called()

    def test_explicit_start_normalizes_runtime_ownership_but_preserves_project_truth(self):
        dfs = self.workspace / ".de67/FS.md"
        ledger = self.workspace / ".de67/work-ledger.md"
        mutations = self.workspace / ".de67/mutation-suggestions.md"
        dfs.write_text("frozen product truth\n")
        ledger.write_text("unfinished work projection\n")
        mutations.write_text("queued mutation truth\n")
        with DeadlineHarness(self.state) as harness:
            harness.start_task("lineage", "complete", "R-001", 3600, now=1)
            harness.complete_task("lineage", "complete", "terminal proof", now=2)
            for number in range(2, 10):
                harness.start_task(
                    "lineage", f"complete-{number}", "R-001", 3600,
                    now=number * 2,
                )
                harness.complete_task(
                    "lineage", f"complete-{number}", f"terminal proof {number}",
                    now=number * 2 + 1,
                )
            harness.start_task("lineage", "idle-claim", "R-002", 3600, now=18.5)
            harness.complete_task(
                "lineage", "idle-claim", "idle claim proof", now=19.5,
            )
            harness.start_task(
                "lineage", "legacy-normalized", "R-LEGACY", 9999, now=0,
            )
            harness.connection.execute(
                "UPDATE tasks SET terminal_at = 1, attempt_terminal_at = 1, "
                "attempt_terminal_kind = 'abandoned', abandoned_at = 1, "
                "abandonment_reason = 'external_supervisor_restart_normalization' "
                "WHERE lineage_id = 'lineage' AND task_id = 'legacy-normalized'"
            )
            harness.connection.commit()
            harness.start_task("lineage", "stale", "R-001", 3600, now=20)
            harness.claim_worker(
                "lineage", "stale", "worker-old", "coordinator-old",
                "supervisor-old", now=21,
            )
            restart = harness.request_coordinator_restart(
                "lineage", "preserve semantic restart", now=22,
            )["coordinator_restart"]
            generation = restart["generation"]
            harness.claim_coordinator_restart(
                "lineage", generation, "dead-run", now=23,
            )
            harness.connection.execute(
                "UPDATE random_mutation_cycles SET interval_windows = 11, "
                "due_after_terminal_windows = 11 WHERE lineage_id = 'lineage'"
            )
            harness.connection.executescript(
                """
                DROP TRIGGER task_terminal_kind_is_valid_on_update;
                CREATE TRIGGER task_terminal_kind_is_valid_on_update
                BEFORE UPDATE OF attempt_terminal_kind ON tasks
                WHEN NEW.attempt_terminal_kind IS NOT NULL
                 AND NEW.attempt_terminal_kind NOT IN (
                     'completed', 'abandoned', 'finding', 'integrity_breach'
                 )
                BEGIN
                    SELECT RAISE(ABORT, 'unsupported attempt terminal kind');
                END;
                """
            )
            harness.connection.commit()
            mutation_before = harness.list_tasks(now=23)["random_mutation"]
            self.assertFalse(mutation_before["due"])

        result = supervisor_service._normalize_explicit_start(
            self.state, "lineage", now=4000,
        )

        self.assertEqual(result, {"abandoned_attempts": 1, "released_restart_claim": True})
        with DeadlineHarness(self.state) as harness:
            completed = harness.connection.execute(
                "SELECT attempt_terminal_kind, completion_evidence FROM tasks "
                "WHERE lineage_id = 'lineage' AND task_id = 'complete'"
            ).fetchone()
            self.assertEqual(completed["attempt_terminal_kind"], "completed")
            self.assertEqual(completed["completion_evidence"], "terminal proof")
            stale = harness.connection.execute(
                "SELECT attempt_terminal_kind, abandonment_reason FROM tasks "
                "WHERE lineage_id = 'lineage' AND task_id = 'stale'"
            ).fetchone()
            self.assertEqual(stale["attempt_terminal_kind"], "restart_normalized")
            self.assertEqual(
                stale["abandonment_reason"],
                "external_supervisor_restart_normalization",
            )
            claim = harness.connection.execute(
                "SELECT released_at, release_reason FROM worker_claims "
                "WHERE lineage_id = 'lineage' AND task_id = 'stale'"
            ).fetchone()
            self.assertEqual(claim["released_at"], 4000)
            self.assertEqual(claim["release_reason"], "restart_normalized")
            preserved = harness.coordinator_restart_status("lineage")["coordinator_restart"]
            self.assertTrue(preserved.get("required", preserved.get("pending")))
            self.assertEqual(preserved["generation"], generation)
            self.assertIsNone(preserved["expected_run_id"])
            retired = harness.connection.execute(
                "SELECT retired_at, retirement_reason FROM claim_deadline_generations "
                "WHERE lineage_id = 'lineage' AND claim_id = 'R-001' AND generation = 1"
            ).fetchone()
            self.assertEqual(retired["retired_at"], 4000)
            self.assertEqual(
                retired["retirement_reason"],
                "external_supervisor_restart_normalization",
            )
            idle_retired = harness.connection.execute(
                "SELECT retired_at, retirement_reason FROM claim_deadline_generations "
                "WHERE lineage_id = 'lineage' AND claim_id = 'R-002' AND generation = 1"
            ).fetchone()
            self.assertEqual(idle_retired["retired_at"], 4000)
            self.assertEqual(
                harness.list_tasks(now=23)["random_mutation"], mutation_before
            )
            self.assertEqual(
                harness.connection.execute(
                    "SELECT COUNT(*) FROM incidents WHERE lineage_id = 'lineage'"
                ).fetchone()[0],
                0,
            )
            facts = workspace_facts(
                self.workspace, self.state, "lineage", now=4000,
            )
            self.assertNotIn("worker_abandoned", facts)
            self.assertNotIn("worker_completed", facts)
            self.assertNotIn("worker_restart_normalized", facts)
            self.assertNotIn("open_claim", facts)
            self.assertNotIn("deadline_expired", facts)
            harness.claim_coordinator_restart(
                "lineage", generation, "fresh-run", now=4000,
            )
            harness.acknowledge_coordinator_restart(
                "lineage", generation, "fresh-run", now=4000,
            )
            replacement = harness.start_task(
                "lineage", "replacement", "R-001", 3600, now=4001,
            )
            self.assertEqual(replacement["deadline_generation"], 2)
            self.assertEqual(replacement["deadline_at"], 7601)
            self.assertEqual(
                harness.connection.execute(
                    "SELECT supervisor_epoch_generation FROM tasks "
                    "WHERE lineage_id = 'lineage' AND task_id = 'replacement'"
                ).fetchone()[0],
                1,
            )
            fresh_facts = workspace_facts(
                self.workspace, self.state, "lineage", now=4001,
            )
            self.assertIn("open_claim", fresh_facts)
            self.assertNotIn("deadline_expired", fresh_facts)
            idle_replacement = harness.start_task(
                "lineage", "idle-replacement", "R-002", 3600, now=4002,
            )
            self.assertEqual(idle_replacement["deadline_generation"], 2)
            self.assertEqual(idle_replacement["deadline_at"], 7602)
        self.assertEqual(dfs.read_text(), "frozen product truth\n")
        self.assertEqual(ledger.read_text(), "unfinished work projection\n")
        self.assertEqual(mutations.read_text(), "queued mutation truth\n")

    @unittest.skipUnless(
        os.environ.get("DE67_RUN_SERVICE_INTEGRATION") == "1",
        "real tmux service integration is opt-in",
    )
    def test_real_tmux_service_survives_separate_control_processes(self):
        root = os.environ.get("DE67_SERVICE_TEST_ROOT")
        with tempfile.TemporaryDirectory(dir=root) as temporary:
            base = Path(temporary)
            workspace = base / "workspace"
            (workspace / ".de67/state").mkdir(parents=True)
            state = workspace / ".de67/state/deadlines.sqlite3"
            state.touch()
            (workspace / ".de67/state/workspace.json").write_text(
                json.dumps({"workspace": str(workspace.resolve()),
                            "clock": {"state": str(state.resolve()), "lineage": "smoke"}})
            )
            scripts = base / "scripts"
            scripts.mkdir()
            launcher = scripts / "supervisor_service.py"
            shutil.copy2(SCRIPTS / "supervisor_service.py", launcher)
            shutil.copy2(SCRIPTS / "deadline_harness.py", scripts / "deadline_harness.py")
            shutil.copy2(
                Path(__file__).parent / "fixtures/service_fake_supervisor.py",
                scripts / "coordinator_supervisor.py",
            )
            (scripts / "codex_runner.py").touch()
            environment = os.environ.copy()
            environment.update({"DE67_CODEX": "/usr/bin/true", "DE67_TMUX": shutil.which("tmux") or ""})
            command = [sys.executable, str(launcher)]
            try:
                started = subprocess.run(
                    [*command, "start", "--workspace", str(workspace)],
                    env=environment, text=True, capture_output=True,
                )
                self.assertEqual(started.returncode, 0, started.stderr)
                status = subprocess.run(
                    [*command, "status", "--workspace", str(workspace)],
                    env=environment, text=True, capture_output=True,
                )
                self.assertEqual(status.returncode, 0, status.stderr)
                self.assertIn("pid=", json.loads(status.stdout)["service"])
            finally:
                stopped = subprocess.run(
                    [*command, "stop", "--workspace", str(workspace)],
                    env=environment, text=True, capture_output=True,
                )
            self.assertEqual(stopped.returncode, 0, stopped.stderr)
            final_status = subprocess.run(
                [*command, "status", "--workspace", str(workspace)],
                env=environment, text=True, capture_output=True,
            )
            self.assertEqual(json.loads(final_status.stdout)["service"], "stopped")

    @unittest.skipUnless(
        os.environ.get("DE67_RUN_SERVICE_STACK_INTEGRATION") == "1",
        "real multi-round tmux stack integration is opt-in",
    )
    def test_real_tmux_service_restarts_mid_stack_then_completes_mutation_chain(self):
        root = os.environ.get("DE67_SERVICE_TEST_ROOT")
        fixture_root = Path(__file__).parent / "fixtures"
        environment_root = fixture_root / "service_stack_environment"
        with tempfile.TemporaryDirectory(dir=root) as temporary:
            base = Path(temporary)
            workspace = (base / "workspace").resolve()
            state_root = workspace / ".de67/state"
            state_root.mkdir(parents=True)
            shutil.copy2(environment_root / "FS.md", workspace / ".de67/FS.md")
            shutil.copy2(environment_root / "work-ledger.md", workspace / ".de67/work-ledger.md")
            shutil.copy2(
                environment_root / "mutation-suggestions.md",
                workspace / ".de67/mutation-suggestions.md",
            )
            shutil.copy2(environment_root / "product.txt", workspace / "product.txt")

            state = state_root / "deadlines.sqlite3"
            from deadline_harness import DeadlineHarness
            with DeadlineHarness(state) as harness:
                harness.start_task("stack-test", "bootstrap", "R-STACK", 60, now=time.time())
                harness.complete_task("stack-test", "bootstrap", "stack fixture ready")
            # A prior external supervisor may have attempted this exact durable
            # frontier. The explicit new service start must get one fresh epoch.
            from coordinator_supervisor import SupervisorJournal, supervision_fingerprint
            prior = SupervisorJournal(state, "stack-test", "prior-service")
            prior.begin(
                "coordinator",
                supervision_fingerprint(state, "stack-test", workspace),
                "prior-run",
            )
            prior.finish("prior-run", "failed", "simulated prior service death")
            (state_root / "workspace.json").write_text(json.dumps(
                {"workspace": str(workspace),
                 "clock": {"state": str(state), "lineage": "stack-test"}}
            ))

            scripts = base / "scripts"
            scripts.mkdir()
            for name in (
                "supervisor_service.py", "coordinator_supervisor.py", "codex_runner.py",
                "deadline_harness.py", "blocker_adapter.py", "policy_kernel.py",
            ):
                shutil.copy2(SCRIPTS / name, scripts / name)
            fake_codex = scripts / "service_stack_fake_codex.py"
            shutil.copy2(fixture_root / "service_stack_fake_codex.py", fake_codex)
            fake_codex.chmod(0o755)

            environment = os.environ.copy()
            environment.update({
                "DE67_CODEX": str(fake_codex),
                "DE67_TMUX": shutil.which("tmux") or "",
                "DE67_STACK_RESTART_AFTER_ROUND_ONE": "1",
            })
            command = [sys.executable, str(scripts / "supervisor_service.py")]
            try:
                started = subprocess.run(
                    [*command, "start", "--workspace", str(workspace)],
                    env=environment, text=True, capture_output=True,
                )
                self.assertEqual(started.returncode, 0, started.stderr)
                status = subprocess.run(
                    [*command, "status", "--workspace", str(workspace)],
                    env=environment, text=True, capture_output=True,
                )
                self.assertEqual(status.returncode, 0, status.stderr)
                self.assertIn("pid=", json.loads(status.stdout)["service"])

                (state_root / "service-test-release").touch()
                with DeadlineHarness(state) as harness:
                    deadline = float(harness.status_task("stack-test", "bootstrap")["deadline_at"])
                while time.time() < deadline:
                    events_path = state_root / "stack-events.jsonl"
                    if events_path.exists() and len(events_path.read_text().splitlines()) == 1:
                        break
                    time.sleep(0.05)
                else:
                    self.fail("first coordinator round did not finish before restart")

                stopped_for_restart = subprocess.run(
                    [*command, "stop", "--workspace", str(workspace)],
                    env=environment, text=True, capture_output=True,
                )
                self.assertEqual(stopped_for_restart.returncode, 0, stopped_for_restart.stderr)
                stale_log = state_root / "supervisor-service/stderr.log"
                with stale_log.open("a", encoding="utf-8") as output:
                    output.write("stale prior-epoch error retained for audit\n")
                restarted = subprocess.run(
                    [*command, "start", "--workspace", str(workspace)],
                    env=environment, text=True, capture_output=True,
                )
                self.assertEqual(restarted.returncode, 0, restarted.stderr)
                restarted_status = subprocess.run(
                    [*command, "status", "--workspace", str(workspace)],
                    env=environment, text=True, capture_output=True,
                )
                self.assertIn("pid=", json.loads(restarted_status.stdout)["service"])
                self.assertIn("stale prior-epoch error", stale_log.read_text())
                (state_root / "service-test-continue-after-restart").touch()

                # The durable bootstrap deadline is the authoritative integration bound.
                while time.time() < deadline:
                    current = subprocess.run(
                        [*command, "status", "--workspace", str(workspace)],
                        env=environment, text=True, capture_output=True,
                    )
                    self.assertEqual(current.returncode, 0, current.stderr)
                    if json.loads(current.stdout)["service"] == "stopped":
                        break
                    time.sleep(0.05)
                else:
                    self.fail("multi-round tmux stack did not finish before its bootstrap deadline")
            finally:
                stopped = subprocess.run(
                    [*command, "stop", "--workspace", str(workspace)],
                    env=environment, text=True, capture_output=True,
                )
            self.assertEqual(stopped.returncode, 0, stopped.stderr)

            events = [json.loads(line) for line in
                      (state_root / "stack-events.jsonl").read_text().splitlines()]
            self.assertEqual([event["role"] for event in events], [
                "coordinator", "coordinator", "mutation-reviewer", "coordinator",
            ])
            self.assertTrue(all(event["sandbox"] == "danger-full-access" for event in events))
            self.assertEqual(len({event["start_token"] for event in events}), 2)
            self.assertTrue(events[0]["start_token"])
            self.assertNotEqual(events[0]["start_token"], events[1]["start_token"])
            self.assertEqual(len({event["start_token"] for event in events[1:]}), 1)
            self.assertEqual(events[-1]["generation"], "1")
            with DeadlineHarness(state) as harness:
                epochs = {
                    row["task_id"]: row["supervisor_epoch_generation"]
                    for row in harness.connection.execute(
                        "SELECT task_id, supervisor_epoch_generation FROM tasks "
                        "WHERE lineage_id = 'stack-test' AND task_id IN "
                        "('round-one', 'round-two', 'post-mutation')"
                    )
                }
            self.assertEqual(epochs, {
                "round-one": 1, "round-two": 2, "post-mutation": 2,
            })
            self.assertEqual((workspace / "product.txt").read_text().splitlines(), [
                "Editable stack-test product state.",
                "round one worker result",
                "round two worker result",
                "mutation reviewer inspected editable state",
                "post-mutation worker result",
            ])
            self.assertIn("- [x] R-STACK", (workspace / ".de67/FS.md").read_text())
            self.assertNotIn("- [ ]", (workspace / ".de67/work-ledger.md").read_text())
            self.assertIn("Consumed by stack mutation reviewer", (
                workspace / ".de67/mutation-suggestions.md").read_text())

    @patch("supervisor_service.shutil.which", return_value="/opt/bin/tmux")
    @patch("supervisor_service.subprocess.run")
    def test_query_error_is_not_absence(self, run, _which):
        run.return_value = subprocess.CompletedProcess([],2,"","server failure")
        with self.assertRaisesRegex(supervisor_service.ServiceError, "server failure"):
            supervisor_service.status_service(self.workspace)

if __name__ == "__main__": unittest.main()
