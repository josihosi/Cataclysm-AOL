"""Every harness profile receives credentials and the native setup controls."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest import mock

import startup_harness as harness


class StartupEnvironmentTest(unittest.TestCase):
    def test_windows_dispatcher_status_projects_actual_launch_owner_without_argv(self):
        import socket
        import windows_curses_terminal_transport as transport
        command = 'C:/game.exe --password synthetic-private-value'
        game = {'pid': 42, 'alive': True, 'birth_identity': 'birth-a',
                'executable_path': 'C:/game.exe', 'command': command}
        broker = {**game, 'pid': 43, 'birth_identity': 'birth-b'}
        owner = transport._public_owner({
            'schema': transport.OWNER_SCHEMA, 'host': socket.gethostname(), 'run_id': 'run-a',
            'endpoint': 'fixture/input.pipe', 'transport': 'windows_conpty',
            'argv': ['C:/game.exe', '--password', 'synthetic-private-value'],
            'auth_key': 'synthetic-private-value', 'native_lease': {'private': 'synthetic-private-value'},
            'game_pid': 42, 'game_process_generation': game,
            'broker_pid': 43, 'broker_process_generation': broker,
            'conhost_process_generations': [broker]})
        self.assertIn('argv', owner)  # Internal legacy launch binding is intact.
        with mock.patch.object(transport, '_read_owner', return_value=owner), \
             mock.patch.object(transport, '_process_generation', side_effect=lambda pid: game if pid == 42 else broker):
            status = transport.dispatcher_status(Path('fixture/input.pipe'))
        self.assertEqual(status['game_status'], 'alive')
        self.assertEqual(status['broker_status'], 'alive')
        self.assertNotIn('argv', status['owner'])
        self.assertNotIn('synthetic-private-value', json.dumps(status))
        self.assertEqual(status['owner']['conhost_process_generations'][0]['command_sha256'],
                         harness.process_command_identity(broker))

    def test_process_diagnostics_keep_identity_without_arguments_or_environment(self):
        command = "/bin/cataclysm --api-key synthetic-private-value"
        legacy = {"pid": 42, "alive": True, "birth_identity": "birth-a", "command": command,
                  "executable_path": "/bin/cataclysm", "environment": {"API_KEY": "synthetic-private-value"}}
        inspector = mock.Mock()
        inspector.inspect.return_value = harness.ProcessSnapshot(
            pid=42, alive=True, birth_identity="birth-a", executable_path="/bin/cataclysm", command=command)
        observed = harness.process_generation_snapshot(42, inspector=inspector)
        self.assertNotIn("synthetic-private-value", json.dumps(observed))
        self.assertNotIn("command", observed)
        self.assertNotIn("environment", observed)
        self.assertTrue(harness.process_generation_matches(legacy, observed))
        from writable_run_owner import WritableRunOwner, _generation_matches as owner_matches
        self.assertTrue(WritableRunOwner._valid_generation(observed))
        self.assertTrue(owner_matches(legacy, observed))
        self.assertFalse(WritableRunOwner._valid_generation({**observed,
            "command_sha256": "invalid", "command": command}))
        from curses_terminal_transport import _same_generation as posix_matches
        from windows_curses_terminal_transport import _same_generation as windows_matches
        self.assertTrue(posix_matches(observed, legacy))
        self.assertTrue(windows_matches(observed, legacy))
        for changed in ({**legacy, "command": command + " changed"},
                        {**legacy, "birth_identity": "reused"},
                        {**observed, "command_sha256": "invalid", "command": command}):
            self.assertFalse(harness.process_generation_matches(changed, observed))
            self.assertFalse(owner_matches(changed, observed))
            self.assertFalse(posix_matches(changed, observed))
            self.assertFalse(windows_matches(changed, observed))
        self.assertFalse(windows_matches({**legacy, "executable_path": "/bin/wrong"}, observed))
        with mock.patch.object(harness, "process_generation_snapshot", return_value=observed), \
                mock.patch.object(harness, "compare_runtime_binding", return_value={"status": "matched"}):
            identity = harness.terminal_native_startup_identity(42, Path("/bin/cataclysm"), {}, legacy)
        self.assertTrue(identity["ok"], identity)
        self.assertNotIn("synthetic-private-value", json.dumps(identity))
        self.assertNotIn("observed_command", identity)
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            (root / "process.json").write_text(json.dumps({"process_generation": legacy}))
            owned = harness.current_owned_process_generation(root, inspector=inspector)
            self.assertEqual(owned["status"], "alive")
            self.assertNotIn("synthetic-private-value", json.dumps(owned))
        result = harness.cleanup_game_process(42, expected_process_generation=legacy, inspector=inspector)
        self.assertEqual(result["status"], "retained_waiting_for_player_quit")
        self.assertNotIn("synthetic-private-value", json.dumps(result))
        inspector.signal.assert_not_called()

    def test_bound_exit_and_bridge_reports_do_not_echo_process_arguments(self):
        from cockpit_file_bridge import FileBackedCockpitBridge
        command = "/bin/cataclysm --password synthetic-private-value"
        generation = {"pid": 42, "birth_identity": "birth-a", "alive": True,
                      "executable_path": "/bin/cataclysm", "command": command}
        with mock.patch.object(harness, "pid_is_alive", side_effect=[True, False]), \
                mock.patch.object(harness, "pid_command", return_value=command), \
                mock.patch.object(harness.time, "sleep"):
            result = harness.observe_bound_process_exit(42, 1, expected_command=command)
        self.assertEqual(result["status"], "native_exit")
        self.assertNotIn("synthetic-private-value", json.dumps(result))
        with mock.patch.object(harness, "pid_is_alive", return_value=True), \
                mock.patch.object(harness, "pid_command", return_value=command + " changed"):
            wrong = harness.observe_bound_process_exit(42, 0, expected_command=command)
        self.assertEqual(wrong["status"], "wrong_process_identity")
        self.assertNotIn("synthetic-private-value", json.dumps(wrong))
        with tempfile.TemporaryDirectory() as raw:
            bridge = FileBackedCockpitBridge(Path(raw) / "session",
                                            ["/bin/cataclysm", "--password", "synthetic-private-value"],
                                            binding_id="binding-a")
            bridge.prepare()
            self.assertNotIn("synthetic-private-value",
                             (bridge.session_dir / "bridge.manifest.json").read_text())
            (bridge.session_dir / "game-process.json").write_text(json.dumps({
                "pid": 42, "binding_id": "binding-a", "command": command,
                "process_generation": generation}))
            with mock.patch.object(harness, "process_generation_snapshot",
                                   return_value=harness.public_process_generation(generation)):
                evidence = bridge._owned_game_process_evidence()
            self.assertEqual(evidence["status"], "alive")
            self.assertNotIn("synthetic-private-value", json.dumps(evidence))
            with mock.patch.object(harness, "process_generation_snapshot", return_value={
                    **harness.public_process_generation(generation), "birth_identity": "reused"}):
                refused = bridge._owned_game_process_evidence()
            self.assertEqual(refused["status"], "exited_or_identity_changed")
            self.assertNotIn("synthetic-private-value", json.dumps(refused))

    def test_launch_credentials_use_child_environment_not_argv_or_process_record(self):
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            process = mock.Mock(pid=None)
            environment = {"CATA_API_KEY": "synthetic-private-value"}
            with mock.patch.object(harness, "should_use_curses_terminal", return_value=False), \
                    mock.patch.object(harness, "current_owner", return_value=None), \
                    mock.patch.object(harness, "open_semantic_wake_pipe", return_value=(-1, -1, {})), \
                    mock.patch.object(harness, "record_bridge_game_process"), \
                    mock.patch.object(harness.threading, "Thread"), \
                    mock.patch.object(harness.subprocess, "Popen", return_value=process) as launch:
                harness.launch_game("privacy-fixture", "TestSetup00", root,
                                    executable="/bin/cataclysm", child_environment=environment)
                args, options = launch.call_args
                self.assertEqual(options["env"]["CATA_API_KEY"], "synthetic-private-value")
                self.assertEqual(args[0][0], "/bin/cataclysm")
                self.assertNotIn("synthetic-private-value", json.dumps(args[0]))
                self.assertNotIn("synthetic-private-value", (root / "process.json").read_text())
                self.assertNotIn("command", json.loads((root / "process.json").read_text()))
                options["stdout"].close()
                options["stderr"].close()

    def test_semantic_free_tiles_launch_requires_matching_owner_and_removes_transport(self):
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            process = mock.Mock(pid=None)
            owner = mock.Mock(run_id="run-a", inherited_fds=())
            env = {"OPENCLAW_HARNESS_SEMANTIC_RUN_ID": "stale",
                   "OPENCLAW_HARNESS_SEMANTIC_TRACE_PATH": "/stale",
                   "OPENCLAW_HARNESS_SEMANTIC_WAKE_READ_FD": "123"}
            with mock.patch.object(harness, "should_use_curses_terminal", return_value=False), \
                    mock.patch.object(harness, "current_owner", return_value=owner), \
                    mock.patch.object(harness, "open_semantic_wake_pipe") as wake, \
                    mock.patch.object(harness, "record_bridge_game_process"), \
                    mock.patch.object(harness.threading, "Thread"), \
                    mock.patch.object(harness.subprocess, "Popen", return_value=process) as launch:
                harness.launch_game("fixture", "World", root, executable="/bin/cataclysm",
                                    child_environment=env, transition_event_run_id="run-a",
                                    diagnostic_semantic_transport=False)
                options = launch.call_args.kwargs
                self.assertEqual(options["env"]["OPENCLAW_HARNESS_RUN_ID"], "run-a")
                self.assertFalse(any(k.startswith("OPENCLAW_HARNESS_SEMANTIC_")
                                     for k in options["env"]))
                wake.assert_not_called()
                options["stdout"].close()
                options["stderr"].close()
                with self.assertRaises(harness.WritableRootConflict):
                    harness.launch_game("fixture", "World", root, executable="/bin/cataclysm",
                                        child_environment=env, transition_event_run_id="other",
                                        diagnostic_semantic_transport=False)

            with mock.patch.object(harness, "should_use_curses_terminal", return_value=False), \
                    mock.patch.object(harness, "current_owner", return_value=None):
                with self.assertRaises(harness.WritableRootConflict):
                    harness.launch_game("fixture", "World", root, executable="/bin/cataclysm",
                                        diagnostic_semantic_transport=False)

    @unittest.skipUnless(os.name == "posix", "requires POSIX executable and inherited lease")
    def test_semantic_free_tiles_child_is_owned_and_cannot_inherit_semantic_binding(self):
        with tempfile.TemporaryDirectory() as raw:
            base = Path(raw).resolve()
            root, run = base / "profile", base / "run"
            run.mkdir()
            helper = base / "game"
            helper.write_text('#!/bin/sh\nprintf "%s\\n" "${OPENCLAW_HARNESS_RUN_ID}" '
                              '"${OPENCLAW_HARNESS_SEMANTIC_RUN_ID-unset}" '
                              '"${OPENCLAW_HARNESS_SEMANTIC_TRACE_PATH-unset}"\nsleep 1\n')
            helper.chmod(0o755)
            with mock.patch.object(harness, "userdir_for_profile", return_value=root), \
                    mock.patch.object(harness, "should_use_curses_terminal", return_value=False):
                with harness.own_writable_root(root, "run-a", harness.process_generation_snapshot,
                                               harness.process_generation_matches) as owner:
                    child = harness.launch_game("fixture", "World", run, executable=str(helper),
                        child_environment={"PATH": os.environ.get("PATH", ""),
                                           "OPENCLAW_HARNESS_SEMANTIC_RUN_ID": "stale",
                                           "OPENCLAW_HARNESS_SEMANTIC_TRACE_PATH": "/stale"},
                        transition_event_run_id="run-a", diagnostic_semantic_transport=False)
                    self.assertEqual(owner.record["game_generation"]["pid"], child.pid)
                    self.assertEqual(child.wait(timeout=10), 0)
                    self.assertEqual((run / "game.stdout.log").read_text().splitlines(),
                                     ["run-a", "unset", "unset"])

    def test_registered_world_reentry_uses_explicit_metadata_without_argv(self):
        from cockpit_file_bridge import FileBackedCockpitBridge
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            userdir = root / "user"
            saved_world = userdir / "save" / "TestSetup00"
            saved_world.mkdir(parents=True)
            (saved_world / "worldoptions.json").write_text('{"saved":true}')
            bridge = FileBackedCockpitBridge(root / "session", ["unused"], binding_id="binding-a")
            bridge.prepare()
            generation = {"pid": 42, "alive": True, "birth_identity": "birth-a",
                          "command": "/bin/cataclysm --password synthetic-private-value",
                          "executable_path": "/bin/cataclysm"}
            with mock.patch.object(harness, "process_generation_snapshot",
                                   return_value=harness.public_process_generation(generation)), \
                    mock.patch.object(harness, "userdir_for_profile", return_value=userdir):
                harness.record_bridge_game_process(mock.Mock(pid=42), {
                    "OPENCLAW_COCKPIT_BRIDGE_SESSION_DIR": str(bridge.session_dir),
                    "OPENCLAW_COCKPIT_BRIDGE_BINDING_ID": "binding-a",
                    "OPENCLAW_HARNESS_PROFILE": "test",
                    "OPENCLAW_HARNESS_WORLD": "TestSetup00",
                    "OPENCLAW_HARNESS_RUN_ID": "run-a"})
            record = (bridge.session_dir / "game-process.json").read_text()
            self.assertNotIn("synthetic-private-value", record)
            self.assertEqual(json.loads(record)["world"], "TestSetup00")
            bridge._freeze_saved_world_for_reentry({
                "native_save_completion": {"completion": {"world_name": "TestSetup00"}}})
            pointer = json.loads((bridge.session_dir / "replacement_saved_world.snapshot.json").read_text())
            self.assertEqual((Path(pointer["snapshot"]) / "worldoptions.json").read_text(), '{"saved":true}')

    def test_reporting_process_reader_rejects_stale_exit_and_generation(self):
        # Execute the actual closure, with its existing OS identity probe supplied.
        import ast
        source = ast.parse(Path(harness.__file__).read_text())
        nodes = [n for n in ast.walk(source) if isinstance(n, ast.FunctionDef) and n.name == "read_process_state"]
        node = next(n for n in nodes if "current_owned_process_generation" in ast.unparse(n))
        expected = {"pid": 42, "birth_identity": "birth-new", "command": "game"}
        with tempfile.TemporaryDirectory() as raw, mock.patch.dict(os.environ, {
                "OPENCLAW_COCKPIT_BRIDGE_SESSION_DIR": raw,
                "OPENCLAW_COCKPIT_BRIDGE_BINDING_ID": "binding-a"}):
            path = Path(raw) / "game-process-exit.json"
            stale = {"pid": 42, "run_id": "run-a", "binding_id": "binding-a", "alive": False,
                     "exit_code": 0, "process_generation": {**expected, "birth_identity": "old"}}
            path.write_text(json.dumps(stale))
            probe = mock.Mock(return_value={"pid": 42, "status": "alive", "expected": expected})
            scope = dict(harness.__dict__, pid=42, run_id="run-a", run_dir=raw, performance=None,
                         current_owned_process_generation=probe, original_process_generation=expected)
            exec(compile(ast.Module(body=[node], type_ignores=[]), harness.__file__, "exec"), scope)
            read = scope["read_process_state"]
            self.assertTrue(read()["alive"])
            self.assertIsNone(read()["exit_code"])
            for status in ("identity_changed", "identity_unavailable"):
                probe.return_value = {"pid": 42, "status": status, "expected": expected}
                self.assertIsNone(read()["alive"])
            probe.return_value = {"pid": 42, "status": "exited", "expected": expected}
            self.assertFalse(read()["alive"])
            self.assertIsNone(read()["exit_code"])
            path.write_text(json.dumps({**stale, "process_generation": expected, "exit_code": 7}))
            self.assertEqual(read()["exit_code"], 7)
            probe.return_value = {"pid": 43, "status": "exited", "expected": expected}
            self.assertIsNone(read()["alive"])
            probe.return_value = {"pid": 42, "status": "exited", "expected": {**expected, "birth_identity": "another"}}
            self.assertIsNone(read()["alive"])

    def test_fresh_profile_inherits_config_before_overrides_without_mutating_source(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "configured" / "options.json"
            source.parent.mkdir()
            original = '[{"name":"LLM_INTENT_ENABLE","value":"false"}]'
            source.write_text(original)
            with mock.patch.object(harness, "config_dir_for_profile", side_effect=lambda p: root / p):
                harness.apply_profile_option_overrides(
                    "fresh", {"LLM_INTENT_ENABLE": "true"}, config_profile="configured")
                self.assertEqual(harness.load_game_options("fresh")["LLM_INTENT_ENABLE"], "true")
                harness.apply_profile_option_overrides("fresh", {}, config_profile="configured")
                self.assertEqual(harness.load_game_options("fresh")["LLM_INTENT_ENABLE"], "true")
            self.assertEqual(source.read_text(), original)

    def test_fresh_profile_without_overrides_gets_canonical_options(self):
        with tempfile.TemporaryDirectory() as directory, \
                mock.patch.object(harness, "config_dir_for_profile", return_value=Path(directory)):
            harness.apply_profile_option_overrides("fresh", {})
            self.assertIn("LLM_INTENT_ENABLE", harness.load_game_options("fresh"))

    def test_mac_tool_resolution_without_homebrew_on_path(self):
        with mock.patch.object(harness.sys, "platform", "darwin"), \
                mock.patch.object(harness.shutil, "which", return_value=None), \
                mock.patch.object(Path, "is_file", return_value=True), \
                mock.patch.object(harness.os, "access", return_value=True), \
                mock.patch.dict(os.environ, {}, clear=True):
            self.assertEqual(harness.peekaboo_binary(), "/opt/homebrew/bin/peekaboo")
            self.assertEqual(harness.harness_tool_binary("ffmpeg"), "/opt/homebrew/bin/ffmpeg")

    def test_peekaboo_explicit_override_and_path_take_precedence(self):
        with mock.patch.object(harness.shutil, "which", return_value="/custom/peekaboo"), \
                mock.patch.dict(os.environ, {}, clear=True):
            self.assertEqual(harness.peekaboo_binary(), "/custom/peekaboo")
            os.environ[harness.PEEKABOO_BINARY_ENV] = "/configured/peekaboo"
            self.assertEqual(harness.peekaboo_binary(), "/configured/peekaboo")

    def test_nonexecutable_fallback_is_not_selected(self):
        with mock.patch.object(harness.sys, "platform", "darwin"), \
                mock.patch.object(harness.shutil, "which", return_value=None), \
                mock.patch.object(Path, "is_file", return_value=True), \
                mock.patch.object(harness.os, "access", return_value=False):
            self.assertIsNone(harness.harness_tool_binary("peekaboo"))

    def test_sparse_canonical_blob_exports_exact_controls_without_restoring_worktree(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / harness.CANONICAL_KEYBINDINGS_PATH
            source.parent.mkdir(parents=True)
            raw = b'[{"id":"debug","bindings":[{"key":["}"]}]}]\n'
            source.write_bytes(raw)
            for args in (["init", "-q"], ["add", "."],
                         ["-c", "user.name=test", "-c", "user.email=test@example.invalid", "commit", "-qm", "fixture"]):
                subprocess.run(["git", "-C", str(root), *args], check=True, capture_output=True)
            source.unlink()  # The exact original Windows omission.
            target = root / "owned-profile" / "keybindings.json"
            with mock.patch.object(harness, "repo_root", return_value=root), \
                    mock.patch.object(harness, "config_dir_for_profile", return_value=target.parent), \
                    mock.patch.object(harness, "load_game_options", return_value={}), \
                    mock.patch.object(harness, "provision_llm_api_key_environment", return_value=({"status":"disabled"}, {})):
                readiness = harness.harness_runtime_dependency_readiness()
                self.assertEqual(readiness["status"], "ready")
                self.assertEqual(readiness["keybindings"]["source"], "git:HEAD")
                harness.game_child_environment("test")
                self.assertEqual(target.read_bytes(), raw)
                self.assertFalse(source.exists())
                self.assertEqual(harness.harness_runtime_dependency_readiness(), readiness)
                source.write_bytes(b"[]\n")
                harness.game_child_environment("test")
                self.assertEqual(target.read_bytes(), b"[]\n")  # Present current worktree is authoritative.

    def test_missing_or_invalid_canonical_controls_refuse_before_profile_write(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with mock.patch.object(harness, "repo_root", return_value=root), \
                    mock.patch.object(harness, "load_game_options", return_value={}), \
                    mock.patch.object(harness, "provision_llm_api_key_environment", return_value=({"status":"disabled"}, {})), \
                    mock.patch.object(harness, "config_dir_for_profile") as target:
                for contents in (None, b"not-json", b"{}"):
                    source = root / harness.CANONICAL_KEYBINDINGS_PATH
                    if contents is not None:
                        source.parent.mkdir(parents=True, exist_ok=True)
                        source.write_bytes(contents)
                    self.assertEqual(harness.harness_runtime_dependency_readiness()["status"], "unavailable")
                    with self.assertRaisesRegex(RuntimeError, "canonical harness keybindings"):
                        harness.game_child_environment("test")
                    target.assert_not_called()

    def test_selected_readiness_uses_same_runtime_dependency_loader(self):
        import scenario_registry_cli as cli
        with mock.patch.object(harness, "executable_source_readiness", return_value={"status":"ready"}), \
                mock.patch.object(harness, "harness_runtime_dependency_readiness", return_value={"status":"unavailable", "reason":"missing controls"}):
            result = cli._current_source_executable_readiness(executable="selected-game")
            self.assertEqual(result["status"], "runtime_dependency_unavailable")
            self.assertEqual(result["reason"], "missing controls")
            self.assertEqual(result["runtime_dependencies"]["status"], "unavailable")

    def test_disabled_api_still_loads_key_and_controls_on_every_launch(self):
        with tempfile.TemporaryDirectory() as directory, \
                mock.patch.object(harness, "config_dir_for_profile", return_value=Path(directory)), \
                mock.patch.object(harness, "load_game_options", return_value={}), \
                mock.patch.dict(os.environ, {"CATA_API_KEY": "test-secret"}, clear=True):
            for existing in (None, "[]"):
                target = Path(directory) / "keybindings.json"
                if existing is not None:
                    target.write_text(existing)
                environment = harness.game_child_environment("test")
                self.assertEqual(environment["CATA_API_KEY"], "test-secret")
                bindings = json.loads(target.read_text())
                debug = next(row for row in bindings if row["id"] == "debug")
                self.assertTrue(any(row["key"] == ["}"] for row in debug["bindings"]))

    def test_nonlogin_mac_recovers_key_without_returning_startup_noise(self):
        with mock.patch.dict(os.environ, {}, clear=True), \
                mock.patch.object(harness.sys, "platform", "darwin"), \
                mock.patch.object(harness, "read_secure_llm_credential", return_value=("", "macos_keychain")), \
                mock.patch.object(harness.subprocess, "run", return_value=subprocess.CompletedProcess([], 0, "startup noise\0test-secret\n", "")):
            report, environment = harness.provision_llm_api_key_environment({})
        self.assertEqual(environment, {"CATA_API_KEY": "test-secret"})
        self.assertNotIn("test-secret", json.dumps(report))
        self.assertEqual(report["source"], "login_environment")

    def test_missing_key_fails_before_controls_are_modified(self):
        with mock.patch.object(harness, "load_game_options", return_value={}), \
                mock.patch.object(harness, "provision_llm_api_key_environment", return_value=({"status": "missing", "env_var": "CATA_API_KEY", "source": ""}, {})), \
                mock.patch.object(harness, "config_dir_for_profile") as config:
            with self.assertRaisesRegex(RuntimeError, "Harness API credential is unavailable"):
                harness.game_child_environment("test")
            config.assert_not_called()


if __name__ == "__main__":
    unittest.main()
