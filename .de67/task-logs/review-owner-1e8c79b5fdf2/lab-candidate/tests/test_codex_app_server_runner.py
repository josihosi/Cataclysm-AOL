from __future__ import annotations

import json
import io
import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
from contextlib import redirect_stdout
from types import SimpleNamespace

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import codex_app_server_runner as transport
import codex_runner
import mutator_session


class AppServerTransportTests(unittest.TestCase):
    def test_service_cleanup_retains_reparented_descendants_and_rechecks_birth(self):
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            socket = workspace / 'owned.sock'
            captured = {200: (100, 'server', f'codex app-server --listen unix://{socket}'),
                        300: (200, 'child', 'helper'), 400: (200, 'old', 'helper')}
            current = {300: (1, 'child', 'helper'), 400: (1, 'new', 'unrelated')}
            killed = []
            def kill(pid, signum):
                killed.append(pid)
                current.pop(pid)
            with patch.object(transport, 'process_snapshot', side_effect=lambda: dict(current)), \
                 patch.object(transport.os, 'kill', side_effect=kill), \
                 patch.object(transport, 'signal', SimpleNamespace(SIGTERM=15, SIGKILL=9)):
                transport.stop_owned_runtime(SimpleNamespace(pid=100, poll=lambda: 0),
                    workspace, workspace, {}, captured_rows=captured, owned_socket=socket)
            self.assertEqual(killed, [300])
            self.assertIn(400, current)

    def test_outer_cleanup_selects_only_owned_tree_even_after_adapter_death(self):
        for adapter_dead in (False, True):
            with self.subTest(adapter_dead=adapter_dead), tempfile.TemporaryDirectory() as directory:
                workspace = Path(directory)
                run_dir = workspace / 'run'
                env = {'CODEX_HOME': str(workspace / 'codex')}
                socket = transport.socket_path(run_dir, env)
                socket.parent.mkdir(parents=True)
                socket.touch()
                address = workspace / '.de67/state/coordinator-input.json'
                transport.atomic_json(address, {'runner_pid': 100, 'socket': str(socket)})
                rows = {
                    100: (1, 'adapter-birth', 'adapter'),
                    200: (1 if adapter_dead else 100, 'server-birth', f'codex app-server --listen unix://{socket}'),
                    300: (200, 'helper-birth', 'owned-mcp'),
                    400: (1, 'unrelated-birth', 'unrelated'),
                }
                killed = []
                def kill(pid, signum):
                    killed.append(pid)
                    rows.pop(pid, None)
                process = SimpleNamespace(pid=100, poll=lambda: 0 if adapter_dead or 100 not in rows else None)
                with patch.object(transport, 'process_snapshot', side_effect=lambda: dict(rows)), \
                     patch.object(transport.os, 'kill', side_effect=kill), \
                     patch.object(transport, 'signal', SimpleNamespace(SIGTERM=15, SIGKILL=9)):
                    transport.stop_owned_runtime(process, workspace, run_dir, env)
                self.assertEqual(set(killed), {200, 300} if adapter_dead else {100, 200, 300})
                self.assertIn(400, rows)
                self.assertFalse(address.exists())
                self.assertFalse(socket.exists())

    def test_role_launch_reset_resume_compaction_and_cleanup(self):
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            run_dir = workspace / 'run'
            run_dir.mkdir()
            calls = []
            servers = []

            class Server:
                pid = 999
                stopped = False
                def __init__(self, argv, **kwargs):
                    Path(argv[-1].removeprefix('unix://')).touch()
                    servers.append(self)
                def poll(self): return 0 if self.stopped else None
                def terminate(self): self.stopped = True
                def wait(self, **kwargs): return 0

            class Client:
                notifications = []
                def __init__(self, socket):
                    self.notifications = []
                def send(self, message): pass
                def call(self, method, params):
                    calls.append((method, params))
                    if method in ('thread/start', 'thread/resume'):
                        return {'thread': {'id': params.get('threadId', 'fresh')}}
                    if method == 'turn/start':
                        self.notifications = [
                            {'method': 'item/completed', 'params': {'threadId': params['threadId'],
                                'item': {'type': 'contextCompaction', 'id': 'compact'}}},
                            {'method': 'turn/completed', 'params': {'threadId': params['threadId'],
                                'turn': {'id': 'turn', 'status': 'completed'}}},
                        ]
                        return {'turn': {'id': 'turn'}}
                def close(self): pass

            for role, resume in [('coordinator', ''), ('mutation-reviewer', ''), ('coordinator', 'saved')]:
                calls.clear()
                env = {'DE67_RUNNER_ACTIVE_DIR': str(run_dir), 'CODEX_HOME': str(workspace / 'codex'),
                       'DE67_PROCESS_ROLE': role, 'DE67_COORDINATOR_RUN_ID': 'run',
                       'DE67_COORDINATOR_RESUME_SESSION': resume,
                       'DE67_COORDINATOR_MODEL': 'gpt-6-astra' if role == 'mutation-reviewer' else 'gpt-6-sol',
                       'DE67_COORDINATOR_REASONING_EFFORT': 'ultra' if role == 'mutation-reviewer' else 'low'}
                with patch.dict(os.environ, env, clear=True), patch.object(transport.sys, 'platform', 'darwin'), \
                     patch.object(transport.signal, 'signal'), patch.object(transport.subprocess, 'Popen', Server), \
                     patch.object(transport, 'Rpc', Client), redirect_stdout(io.StringIO()):
                    self.assertEqual(transport.run('codex', workspace, 'Current role prompt'), 0)
                launch = next((method, params) for method, params in calls if method.startswith('thread/'))
                self.assertEqual(launch[0], 'thread/resume' if resume else 'thread/start')
                self.assertEqual(launch[1]['approvalPolicy'], 'never')
                self.assertEqual(launch[1]['sandbox'], 'danger-full-access')
                self.assertEqual(launch[1]['model'], env['DE67_COORDINATOR_MODEL'])
                self.assertTrue(servers[-1].stopped)
                self.assertFalse(list((workspace / '.de67/state').glob('*-input.json')))
                self.assertFalse(list((workspace / 'codex/state/de67-input').glob('*.sock')))

            transport.atomic_json(workspace / '.de67/state/workspace.json', {'persistent_mutator': True})
            env.update(DE67_PROCESS_ROLE='mutation-reviewer', DE67_COORDINATOR_MODEL='gpt-6-astra',
                       DE67_COORDINATOR_RESUME_SESSION='', DE67_LINEAGE='lineage',
                       DE67_MUTATION_GATE_JSON=json.dumps({'kind': 'random', 'identity': 'cycle 1',
                                                           'selected_lane': 'lane'}))
            # Owner messages and successive reviews retain one conversation.
            for expected in ('thread/start', 'thread/resume'):
                calls.clear()
                with patch.dict(os.environ, env, clear=True), patch.object(transport.sys, 'platform', 'darwin'), \
                     patch.object(transport.signal, 'signal'), patch.object(transport.subprocess, 'Popen', Server), \
                     patch.object(transport, 'Rpc', Client), patch.object(mutator_session.MutatorSession, 'acquire'), \
                     redirect_stdout(io.StringIO()):
                    self.assertEqual(transport.run('codex', workspace, 'Current review'), 0)
                launch = next((method, params) for method, params in calls if method.startswith('thread/'))
                self.assertEqual(launch[0], expected)
                self.assertTrue(launch[1]['config']['features.context_management.experimental_mode'])
                persisted = json.loads((workspace / '.de67/state/mutator-session.json').read_text())
                self.assertEqual(persisted['thread_id'], 'fresh')
                self.assertEqual(persisted['state'], 'idle')

            # The persistent mutator must not turn coordinator resets into resumes.
            env.update(DE67_PROCESS_ROLE='coordinator', DE67_COORDINATOR_MODEL='gpt-6-sol')

            with patch.dict(os.environ, env, clear=True), patch.object(transport.sys, 'platform', 'darwin'), \
                 patch.object(transport.signal, 'signal'), patch.object(transport.subprocess, 'Popen', Server), \
                 patch.object(transport, 'Rpc', Client), patch.object(Client, 'close', side_effect=RuntimeError('closed')), \
                 redirect_stdout(io.StringIO()):
                with self.assertRaisesRegex(RuntimeError, 'closed'):
                    transport.run('codex', workspace, 'Current role prompt')
            self.assertTrue(servers[-1].stopped)
            self.assertFalse(list((workspace / 'codex/state/de67-input').glob('*.sock')))

    def test_reviews_and_owner_input_resume_original_owner_without_gate_metadata(self):
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            run_dir = workspace / 'run'
            run_dir.mkdir()
            transport.atomic_json(workspace / '.de67/state/workspace.json', {'persistent_mutator': True})
            calls = []

            class Server:
                pid = 999
                stopped = False
                def __init__(self, argv, **kwargs):
                    Path(argv[-1].removeprefix('unix://')).touch()
                def poll(self): return 0 if self.stopped else None
                def terminate(self): self.stopped = True
                def wait(self, **kwargs): return 0

            class Client:
                def __init__(self, socket): self.notifications = []
                def send(self, message): pass
                def call(self, method, params):
                    calls.append((method, params))
                    if method in {'thread/start', 'thread/resume'}:
                        return {'thread': {'id': params.get('threadId', 'new-thread')}}
                    if method == 'turn/start':
                        self.notifications = [{'method': 'turn/completed', 'params': {
                            'threadId': params['threadId'], 'turn': {'id': 'turn', 'status': 'completed'}}}]
                        return {'turn': {'id': 'turn'}}
                def close(self): pass

            env = {'DE67_RUNNER_ACTIVE_DIR': str(run_dir), 'CODEX_HOME': str(workspace / 'codex'),
                   'DE67_PROCESS_ROLE': 'mutation-reviewer', 'DE67_COORDINATOR_RUN_ID': 'retry-run',
                   'DE67_COORDINATOR_MODEL': 'gpt-6-astra', 'DE67_COORDINATOR_REASONING_EFFORT': 'medium',
                   'DE67_LINEAGE': 'lineage'}
            session = mutator_session.MutatorSession(workspace)
            # Migration must retain the owner, never select the separate review thread.
            transport.atomic_json(session.path, {'workspace': str(workspace.resolve()), 'model': 'gpt-6-astra',
                'thread_id': 'owner-thread', 'owner': {'thread_id': 'owner-thread'},
                'review': {'thread_id': 'obsolete-review', 'state': 'interrupted', 'gate': {'identity': 'old'}}})
            for gate_value in (None, '{broken', '{"kind":"random","identity":"different"}'):
                calls.clear()
                review_env = dict(env)
                if gate_value is not None:
                    review_env['DE67_MUTATION_GATE_JSON'] = gate_value
                with patch.dict(os.environ, review_env, clear=True), patch.object(transport.sys, 'platform', 'darwin'), \
                     patch.object(transport.signal, 'signal'), patch.object(transport.subprocess, 'Popen', Server), \
                     patch.object(transport, 'Rpc', Client), redirect_stdout(io.StringIO()):
                    self.assertEqual(transport.run('codex', workspace, 'Current review instructions'), 0)
                launches = [(method, params) for method, params in calls if method.startswith('thread/')]
                self.assertEqual(len(launches), 1)
                self.assertEqual((launches[0][0], launches[0][1]['threadId']), ('thread/resume', 'owner-thread'))
                turn = next(params for method, params in calls if method == 'turn/start')
                self.assertEqual(turn['input'], [{'type': 'text', 'text': 'Current review instructions'}])
                saved = json.loads(session.path.read_text())
                self.assertEqual(saved['thread_id'], 'owner-thread')
                self.assertEqual(saved['result'], 'completed')

            calls.clear()
            owner_input = workspace / 'owner-input.json'
            receipt = workspace / 'owner-receipt.json'
            transport.atomic_json(owner_input, {'input': [{'type': 'text', 'text': 'User Message: retain me'}],
                                                'client_id': 'owner:1', 'receipt_path': str(receipt)})
            owner_env = {**env, 'DE67_COORDINATOR_RUN_ID': 'owner-run',
                         'DE67_INITIAL_INPUT_PATH': str(owner_input)}
            with patch.dict(os.environ, owner_env, clear=True), patch.object(transport.sys, 'platform', 'darwin'), \
                 patch.object(transport.signal, 'signal'), patch.object(transport.subprocess, 'Popen', Server), \
                 patch.object(transport, 'Rpc', Client), redirect_stdout(io.StringIO()):
                self.assertEqual(transport.run('codex', workspace, 'owner guidance'), 0)
            launch = next((method, params) for method, params in calls if method.startswith('thread/'))
            self.assertEqual((launch[0], launch[1]['threadId']), ('thread/resume', 'owner-thread'))
            turn = next(params for method, params in calls if method == 'turn/start')
            self.assertEqual(turn['clientUserMessageId'], 'owner:1')
            self.assertEqual(turn['input'], [{'type': 'text', 'text': 'User Message: retain me'}])
            self.assertEqual(json.loads(receipt.read_text())['state'], 'submitted')

            # A genuinely new owner conversation still receives the bootstrap.
            session.path.unlink()
            calls.clear()
            with patch.dict(os.environ, owner_env, clear=True), patch.object(transport.sys, 'platform', 'darwin'), \
                 patch.object(transport.signal, 'signal'), patch.object(transport.subprocess, 'Popen', Server), \
                 patch.object(transport, 'Rpc', Client), redirect_stdout(io.StringIO()):
                self.assertEqual(transport.run('codex', workspace, 'owner guidance'), 0)
            turn = next(params for method, params in calls if method == 'turn/start')
            self.assertEqual(turn['input'], [{'type': 'text', 'text': 'owner guidance'},
                                             {'type': 'text', 'text': 'User Message: retain me'}])

    def test_lock_prevents_a_second_mutation_owner(self):
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            owner = mutator_session.MutatorSession(workspace)
            owner.acquire(lambda: False)
            owner.record('owner-thread', state='active', turn_id='owner-turn')
            contender = mutator_session.MutatorSession(workspace)
            attempts = 0
            def stopped():
                nonlocal attempts
                attempts += 1
                return attempts > 1
            try:
                with self.assertRaisesRegex(RuntimeError, 'another invocation owned it'):
                    contender.acquire(stopped)
                saved = json.loads((workspace / '.de67/state/mutator-session.json').read_text())
                self.assertEqual(saved['thread_id'], 'owner-thread')
                self.assertEqual(saved['turn_id'], 'owner-turn')
            finally:
                contender.close()
                owner.close()

    def test_review_startup_failure_keeps_shared_conversation_recoverable(self):
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            run_dir = workspace / 'run'
            run_dir.mkdir()
            transport.atomic_json(workspace / '.de67/state/workspace.json', {'persistent_mutator': True})
            env = {'DE67_RUNNER_ACTIVE_DIR': str(run_dir), 'CODEX_HOME': str(workspace / 'codex'),
                   'DE67_PROCESS_ROLE': 'mutation-reviewer', 'DE67_COORDINATOR_RUN_ID': 'review-run',
                   'DE67_COORDINATOR_MODEL': 'gpt-6-astra', 'DE67_COORDINATOR_REASONING_EFFORT': 'medium',
                   'DE67_LINEAGE': 'lineage', 'DE67_MUTATION_GATE_JSON': json.dumps(
                       {'kind': 'random', 'identity': 'failure', 'selected_lane': None})}
            mutator_session.MutatorSession(workspace).record('owner-thread', state='idle')

            class Server:
                pid = 777
                stopped = False
                def __init__(self, argv, **kwargs): Path(argv[-1].removeprefix('unix://')).touch()
                def poll(self): return 0 if self.stopped else None
                def terminate(self): self.stopped = True
                def wait(self, **kwargs): return 0

            class Client:
                def __init__(self, socket): self.notifications = []
                def send(self, message): pass
                def call(self, method, params):
                    if method == 'thread/resume': return {'thread': {'id': params['threadId']}}
                    if method == 'turn/start': raise transport.RpcError('turn start rejected', -32600)
                def close(self): pass

            with patch.dict(os.environ, env, clear=True), patch.object(transport.sys, 'platform', 'darwin'), \
                 patch.object(transport.signal, 'signal'), patch.object(transport.subprocess, 'Popen', Server), \
                 patch.object(transport, 'Rpc', Client), redirect_stdout(io.StringIO()):
                with self.assertRaisesRegex(transport.RpcError, 'turn start rejected'):
                    transport.run('codex', workspace, 'review prompt')
            saved = json.loads((workspace / '.de67/state/mutator-session.json').read_text())
            self.assertEqual(saved['thread_id'], 'owner-thread')
            self.assertEqual(saved['result'], 'interrupted')
            self.assertFalse((workspace / '.de67/state/mutator-input.json').exists())

    def test_native_command_still_drives_the_existing_handoff_guard(self):
        guard = codex_runner.CoordinatorLoopGuard()
        guard.observe({"type": "item.completed", "item": transport.normalize_item({
            "type": "commandExecution", "status": "completed", "exitCode": 0,
            "aggregatedOutput": json.dumps({"action": "spawn_worker", "worker_spawns": [
                {"task_id": "T-live"}]}),
        })})
        self.assertEqual(guard.unbound_tasks, ("T-live",))
        guard.observe({"type": "item.completed", "item": transport.normalize_item({
            "type": "collabAgentToolCall", "tool": "spawnAgent", "status": "completed",
            "receiverThreadIds": ["worker-1"],
        })})
        self.assertEqual(guard.unbound_tasks, ())

    def test_native_followup_does_not_reassign_an_existing_worker(self):
        guard = codex_runner.CoordinatorLoopGuard(initial_unbound_tasks=("T1", "T2"))
        item = {"type": "collabAgentToolCall", "tool": "spawnAgent", "status": "completed",
                "receiverThreadIds": ["worker-1"]}
        guard.observe({"type": "item.completed", "item": transport.normalize_item(item)})
        item["tool"] = "followupTask"
        guard.observe({"type": "item.completed", "item": transport.normalize_item(item)})
        self.assertEqual(guard.unbound_tasks, ("T2",))

    def test_workspace_opt_in_preserves_cli_default_and_explicit_override(self):
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            self.assertEqual(codex_runner._command("codex", workspace, {})[:2], ["codex", "exec"])
            binding = workspace / ".de67/state/workspace.json"
            transport.atomic_json(binding, {"agent_transport": "app-server"})
            with patch("codex_runner.sys.platform", "darwin"):
                command = codex_runner._command("codex", workspace, {})
                self.assertTrue(command[1].endswith("codex_app_server_runner.py"))
            self.assertEqual(codex_runner._command("codex", workspace,
                             {"DE67_AGENT_TRANSPORT": "cli"})[:2], ["codex", "exec"])

    def test_unknown_transport_is_not_silently_ignored(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(codex_runner.RunnerError, "Unsupported agent transport"):
                codex_runner._command("codex", Path(directory), {"DE67_AGENT_TRANSPORT": "unknown"})


if __name__ == "__main__":
    unittest.main()
