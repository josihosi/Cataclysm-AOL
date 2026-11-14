"""Mac lane refusals and launch/input integration using harmless native PTYs."""
import argparse
import json
import os
from pathlib import Path
import socket
import sys
import tempfile
import time
import unittest
from unittest.mock import patch
import startup_harness as harness
from curses_terminal_transport import dispatcher_status, dispatch_input
from writable_run_owner import own_writable_root, WritableRootConflict


class TerminalModeIsolationTest(unittest.TestCase):
    @unittest.skipUnless(os.name == 'posix', 'POSIX owning wrapper integration')
    def test_startup_and_probe_own_real_roots_with_fingerprinted_processes(self):
        import subprocess
        from writable_run_owner import current_owner
        calls = []
        def body(args, **kwargs):
            root = harness.userdir_for_profile(args.profile)
            owner = current_owner(root)
            self.assertIsNotNone(owner)
            self.assertNotIn('command', owner.record['launcher_generation'])
            owner.begin_launch()
            child = subprocess.Popen([sys.executable, '-c', 'import sys; sys.stdin.buffer.readline()'],
                stdin=subprocess.PIPE, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                pass_fds=owner.inherited_fds, start_new_session=True)
            try:
                generation = harness.public_process_generation(harness.stable_terminal_process_generation(child.pid))
                owner.bind_game(generation)
                self.assertEqual(owner.record['launch_state'], 'bound')
                self.assertTrue(harness.process_generation_matches(generation,
                    harness.process_generation_snapshot(child.pid)))
                calls.append(args.profile)
            finally:
                child.stdin.write(b'exit\n'); child.stdin.flush()
                self.assertEqual(child.wait(timeout=5), 0)
                child.stdin.close()
            return 0
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder).resolve()
            (root / '.userdata').mkdir()
            scenario = {'name': 'fixture', 'profile': 'probe-fixture',
                        'runtime_contract': {'playtest_mode': 'terminal', 'terminal_transport': 'pty'}}
            with patch.object(harness, 'repo_root', return_value=root), \
                 patch.object(harness, '_run_startup', side_effect=body), \
                 patch.object(harness, '_run_probe_mode', side_effect=body), \
                 patch.object(harness, 'load_scenario', return_value=scenario), \
                 patch.object(harness, 'terminal_mode_readiness', return_value={'status': 'ready'}):
                for index in range(2):
                    # Second entry reconciles the persisted returned owner and
                    # actually exited child instead of bypassing ownership.
                    args = argparse.Namespace(profile='startup-fixture', executable=sys.executable,
                        harness_run_id=f'startup-{index}', dry_run=False, scenario_contract_path='')
                    self.assertEqual(harness.run_startup(args), 0)
                    probe = argparse.Namespace(scenario='fixture', profile='probe-fixture',
                        executable=sys.executable, dry_run=False)
                    self.assertEqual(harness.run_probe_mode(probe), 0)
            self.assertEqual(calls, ['startup-fixture', 'probe-fixture'] * 2)
            for profile in set(calls):
                record = json.loads((root / '.userdata' / profile / '.harness-owner.lock').read_text())
                self.assertEqual(record['launcher_state'], 'returned')
                self.assertNotIn('command', record['game_generation'])

    @unittest.skipUnless(os.name == 'posix', 'POSIX adapter')
    def test_real_startup_profiles_and_reusable_roots_refuse_overlap_before_writes(self):
        from writable_run_owner import WritableRunOwner
        from writable_run_owner_test import start_waiting_child
        child = None
        with tempfile.TemporaryDirectory() as raw:
            base = Path(raw).resolve()
            userdata = base / '.userdata'; userdata.mkdir()
            active = userdata / 'active'; active.mkdir()
            (active / 'saved-bytes').write_bytes(b'other run retained save')
            try:
                with own_writable_root(active, 'active-owner', harness.process_generation_snapshot,
                                       harness.process_generation_matches) as owner:
                    child = start_waiting_child(owner)
                # The launcher has returned; actual child descriptors are the
                # authority, not ACTIVE or a caller-supplied no-owner answer.
                before = {str(p.relative_to(userdata)): p.read_bytes()
                          for p in userdata.rglob('*') if p.is_file()}
                inode = (active / '.harness-owner.lock').stat().st_ino
                generation = harness.process_generation_snapshot(child.pid)
                # Independently inspect actual inherited inode locks after
                # the launcher closes, bypassing marker-based preflight.
                import fcntl
                for path, mode in [(active, fcntl.LOCK_SH), (userdata, fcntl.LOCK_EX)]:
                    fd = os.open(path, os.O_RDONLY | os.O_DIRECTORY)
                    try:
                        with self.assertRaises(BlockingIOError):
                            fcntl.flock(fd, mode | fcntl.LOCK_NB)
                    finally:
                        os.close(fd)
                refusals = []
                with patch.object(harness, 'repo_root', return_value=base), \
                     patch.object(harness, '_run_startup', side_effect=AssertionError('profile writes reached')) as body:
                    for profile in ['.', '..', ' . ', ' .. ']:
                        args = argparse.Namespace(profile=profile, harness_run_id='contender', dry_run=False,
                                                  scenario_contract_path='')
                        with self.assertRaises((ValueError, SystemExit)):
                            harness.run_startup(args)
                        refusals.append('startup profile '+repr(profile))
                    body.assert_not_called()
                    # Ordinary profile names and legacy slash sanitization
                    # continue to resolve to direct sibling run roots.
                    for profile in ['ordinary.v2', 'branch/name', '../active']:
                        resolved = harness.userdir_for_profile(harness.resolve_profile_name(profile))
                        self.assertEqual(resolved.parent, userdata)
                        self.assertNotEqual(resolved, active)
                for root in [userdata, active / 'nested']:
                    with self.assertRaises(WritableRootConflict):
                        WritableRunOwner(root, 'overlap', harness.process_generation_snapshot,
                                         harness.process_generation_matches)
                    refusals.append('reusable root '+str(root))
                after = {str(p.relative_to(userdata)): p.read_bytes()
                         for p in userdata.rglob('*') if p.is_file()}
                self.assertEqual(after, before)
                self.assertEqual((active / '.harness-owner.lock').stat().st_ino, inode)
                self.assertFalse((active / 'nested').exists())
                self.assertFalse((userdata / '.harness-owner.lock').exists())
                self.assertTrue(harness.process_generation_matches(generation, harness.process_generation_snapshot(child.pid)))
                # A genuine sibling remains available while the first child
                # holds its full inherited directory and file exclusion.
                with own_writable_root(userdata / 'ordinary.v2', 'sibling', harness.process_generation_snapshot,
                                       harness.process_generation_matches):
                    pass
                child.stdin.write(b'exit\n'); child.stdin.flush(); child.wait(timeout=5)
                self.assertFalse(harness.process_generation_snapshot(child.pid)['alive'])
                control = {'control': 'actual_startup_overlap_refusals', 'host': socket.gethostname(),
                           'child_process_generation': generation, 'refusals': refusals,
                           'other_run_bytes_and_lock_inode_unchanged': True,
                           'launcher_returned_child_lock_retained': True, 'inherited_directory_locks_independently_verified': True,
                           'ordinary_sibling_accepted': True,
                           'child_exit': child.returncode, 'temporary_root': str(base)}
            finally:
                if child is not None:
                    if child.poll() is None:
                        child.stdin.write(b'exit\n'); child.stdin.flush(); child.wait(timeout=5)
                    child.stdin.close()
        self.assertFalse(base.exists())
        control['temporary_root_removed'] = True
        print(json.dumps(control))

    def test_windows_inspector_selects_read_only_backend_without_signalling(self):
        from certification_process_lease import SystemProcessInspector, ProcessSnapshot
        from types import SimpleNamespace
        from unittest.mock import Mock
        backend = Mock()
        backend.inspect.return_value = ProcessSnapshot(42, True, "C:/dummy.exe", "windows-filetime:1", "dummy")
        module = SimpleNamespace(WindowsProcessInspector=Mock(return_value=backend))
        with patch.dict("sys.modules", {"windows_native_process": module}), \
                patch("certification_process_lease.os.name", "nt"), \
                patch("certification_process_lease.os.kill") as kill:
            self.assertEqual(SystemProcessInspector().inspect(42), backend.inspect.return_value)
            backend.inspect.assert_called_once_with(42)
            with self.assertRaisesRegex(OSError, "native Windows"):
                SystemProcessInspector().signal(42, 0)
            kill.assert_not_called()

    def test_lane_defaults_and_explicit_refusals(self):
        self.assertEqual(harness.scenario_playtest_mode({}), 'tiles')
        with self.assertRaises(ValueError):
            harness.scenario_playtest_mode({'runtime_contract': {'playtest_mode': 'terminal'}})
        scenario = {'runtime_contract': {'playtest_mode': 'terminal', 'terminal_transport': 'pty'}}
        with patch.object(harness.sys, 'platform', 'win32'):
            self.assertEqual(harness.terminal_mode_readiness(scenario, Path('game'))['status'], 'unsupported_terminal_mode')
        with patch.object(harness.sys, 'platform', 'darwin'):
            for change in ({'capture_world_after': True}, {'steps': [{'kind': 'keyboard'}]}):
                self.assertEqual(harness.terminal_mode_readiness({**scenario, **change}, Path('game'))['status'], 'unsupported_terminal_mode')
            with patch.object(harness, 'validate_selected_product_build', return_value=(None, 'receipt mismatch')):
                self.assertEqual(harness.terminal_mode_readiness(scenario, Path('game'))['reason'], 'receipt mismatch')
            with patch.object(harness, 'validate_selected_product_build', return_value=({'build_configuration': {'renderer': 'tiles'}}, '')):
                self.assertEqual(harness.terminal_mode_readiness(scenario, Path('game'))['status'], 'unsupported_terminal_mode')

    def test_unsupported_direct_startup_does_not_create_profile(self):
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw).resolve() / 'not-created'
            args = argparse.Namespace(profile='a', harness_run_id='run-a', dry_run=False,
                                      scenario_contract_path='a.json', scenario_identity='a', executable='game')
            with patch.object(harness, 'userdir_for_profile', return_value=root), \
                 patch.object(harness, 'load_scenario', return_value={'runtime_contract': {'playtest_mode': 'terminal', 'terminal_transport': 'pty'}}), \
                 patch.object(harness, 'terminal_mode_readiness', return_value={'status': 'unsupported_terminal_mode'}):
                with self.assertRaises(SystemExit): harness.run_startup(args)
            self.assertFalse(root.exists())

    def test_incomplete_existing_root_refused_before_saved_bytes_change(self):
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw).resolve()
            save = root/'save'; save.mkdir()
            (save/'sentinel').write_bytes(b'retained save')
            (root/'config-sentinel').write_bytes(b'retained config')
            before = {str(p.relative_to(root)): p.read_bytes() for p in root.rglob('*') if p.is_file()}
            with self.assertRaises(WritableRootConflict): harness.require_managed_terminal_root(root)
            after = {str(p.relative_to(root)): p.read_bytes() for p in root.rglob('*') if p.is_file()}
            self.assertEqual(before, after)
            self.assertFalse((root/'.harness-owner.lock').exists())

    def test_unowned_direct_terminal_launch_refuses_before_artifacts(self):
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw).resolve()
            with patch.object(harness, 'userdir_for_profile', return_value=root), patch.object(harness.subprocess, 'Popen') as popen:
                with self.assertRaises(WritableRootConflict):
                    harness.launch_game('unowned', 'world', root, executable=sys.executable, terminal_transport='pty')
                popen.assert_not_called()
            self.assertEqual(list(root.iterdir()), [])

    def test_terminal_command_pins_every_writable_root_and_tiles_stays_default(self):
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw).resolve()
            with patch.object(harness, 'userdir_for_profile', return_value=root):
                command = harness.build_game_command(Path('/bin/game'), 'a', 'world', isolated_writable=True)
            for option, target in [('userdir', root), ('configdir', root/'config'), ('savedir', root/'save'), ('memorialdir', root/'memorial')]:
                self.assertEqual(command[command.index('--'+option)+1], str(target)+'/')
            original = harness.build_game_command(Path('/bin/game'), 'a', 'world')
            self.assertNotIn('--configdir', original)
            self.assertIn('.userdata/a/', original)

    def test_bridge_wrong_host_or_birth_cannot_write_input_or_cleanup(self):
        from cockpit_file_bridge import FileBackedCockpitBridge
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            (root/'requests').mkdir(); (root/'controls').mkdir()
            actual = harness.process_generation_snapshot(os.getpid())
            status = {'state': 'ready', 'binding_id': 'bound', 'bridge_pid': os.getpid(),
                      'host': socket.gethostname(), 'bridge_process_generation': actual}
            for altered in ({**status, 'host': 'wrong'},
                            {**status, 'bridge_process_generation': {**actual, 'birth_identity': 'stale'}}):
                (root/'status.json').write_text(json.dumps(altered))
                self.assertFalse(FileBackedCockpitBridge.send_request(root, request_id='r', binding_id='bound', request={'action': 'game.observe'})['ok'])
                self.assertFalse(FileBackedCockpitBridge.cleanup(root, 'bound')['ok'])
                self.assertEqual(list((root/'requests').iterdir()), [])
                self.assertEqual(list((root/'controls').iterdir()), [])

    def test_registry_preflight_uses_new_roots_but_preserves_explicit_profile(self):
        import scenario_registry_cli as cli
        declaration = {'profile': 'terminal-mechanics', 'world': 'world',
                       'runtime_contract': {'playtest_mode': 'terminal', 'terminal_transport': 'pty'}}
        with patch.object(cli, '_load_selected_scenario', return_value=declaration):
            a = argparse.Namespace(profile='')
            b = argparse.Namespace(profile='')
            cli._preflight_selected_save_setup(object(), a)
            cli._preflight_selected_save_setup(object(), b)
            self.assertNotEqual(a.profile, b.profile)
            explicit = argparse.Namespace(profile='selected-owned-root')
            cli._preflight_selected_save_setup(object(), explicit)
            self.assertEqual(explicit.profile, 'selected-owned-root')

    def test_outer_startup_record_retains_exact_term_without_inheriting_stale_record(self):
        import ast
        import inspect
        from types import SimpleNamespace
        tree = ast.parse(inspect.getsource(harness._run_startup))
        # Execute the actual outer metadata publication without launching a game.
        statement = next(node for node in ast.walk(tree) if isinstance(node, ast.Expr)
                         and isinstance(node.value, ast.Call)
                         and any(isinstance(arg, ast.Dict) and any(
                             isinstance(key, ast.Constant) and key.value == 'semantic_step_trace_start_offset'
                             for key in arg.keys) for arg in node.value.args))
        module = ast.fix_missing_locations(ast.Module(body=[statement], type_ignores=[]))
        with tempfile.TemporaryDirectory() as raw:
            run = Path(raw)
            for selected_term in ('xterm-256color', None):
                (run/'process.json').write_text('{"pid": 99, "TERM": "stale"}')
                process = SimpleNamespace(pid=100, args=['bound-process'])
                if selected_term is not None:
                    process._openclaw_launch_term = selected_term
                generation = {'pid': 100, 'birth_identity': 'current',
                              'command_sha256': __import__('hashlib').sha256(b'bound-process').hexdigest()}
                context = dict(vars(harness), run_dir=run, proc=process,
                    process_generation=generation,
                    transition_binding={'run_id': 'current-run', 'event_path': 'events'},
                    userdir_for_profile=lambda _: run, profile='current',
                    terminal_native_transport=selected_term is not None,
                    plan=SimpleNamespace(executable=sys.executable, harness_new_world=False, harness_raw_seed=False,
                                         harness_bandit_feasibility=False),
                    child_environment={}, wait_diagnostic=False, debug_size=123,
                    killed_pids=[], certification_lease=None)
                exec(compile(module, 'production-outer-process-writer', 'exec'), context)
                record = json.loads((run/'process.json').read_text())
                self.assertEqual(record.get('TERM'), selected_term)
                self.assertEqual(record['pid'], 100)
                self.assertEqual(record['executable_path'], sys.executable)
                self.assertEqual(record['run_id'], 'current-run')
                self.assertEqual(record['process_generation'], generation)
                self.assertEqual(record['semantic_step_trace_start_offset'], 123)
                self.assertEqual(record['mode'], 'terminal' if selected_term else 'tiles')

    def test_startup_identity_rejects_missing_or_wrong_birth(self):
        observed = {'pid': 42, 'alive': True, 'birth_identity': 'actual', 'command': '/bin/game', 'executable_path': '/bin/game'}
        with patch.object(harness, 'process_generation_snapshot', return_value=observed), \
             patch.object(harness, 'pid_command', return_value='/bin/game'), \
             patch.object(harness, 'pid_is_alive', return_value=True), \
             patch.object(harness, 'compare_runtime_binding', return_value={'status': 'matched'}):
            for expected in (None, {**observed, 'birth_identity': 'stale'}):
                self.assertFalse(harness.terminal_native_startup_identity(42, Path('/bin/game'), {}, expected)['ok'])
            self.assertTrue(harness.terminal_native_startup_identity(42, Path('/bin/game'), {}, observed)['ok'])

    def test_startup_identity_uses_one_birth_bound_snapshot_not_auxiliary_command(self):
        executable = Path(sys.executable).resolve()
        observed = {'pid': 42, 'alive': True, 'birth_identity': 'actual',
                    'command': f'{executable} --world TestSetup00', 'executable_path': str(executable)}
        with patch.object(harness, 'process_generation_snapshot', return_value=observed) as snapshot, \
             patch.object(harness, 'pid_command', return_value='') as auxiliary, \
             patch.object(harness, 'pid_is_alive', return_value=True), \
             patch.object(harness, 'compare_runtime_binding', return_value={'status': 'matched'}):
            identity = harness.terminal_native_startup_identity(42, executable, {}, observed)
            self.assertTrue(identity['ok'], identity)
            self.assertEqual(identity['observed_executable'], str(executable))
            self.assertNotIn('command', identity['process_generation'])
            snapshot.assert_called_once_with(42)
            auxiliary.assert_not_called()

    def test_startup_identity_refuses_incomplete_changed_dead_wrong_executable_and_binding(self):
        executable = Path(sys.executable).resolve()
        expected = {'pid': 42, 'alive': True, 'birth_identity': 'actual', 'command': str(executable), 'executable_path': str(executable)}
        cases = (
            ({**expected, 'command': ''}, expected, 'matched',
             'terminal_native_process_command_unavailable'),
            ({**expected, 'birth_identity': 'reused'}, expected, 'matched',
             'terminal_native_process_generation_unproven'),
            ({**expected, 'alive': False}, expected, 'matched',
             'terminal_native_process_not_alive'),
            ({**expected, 'command': '/bin/wrong', 'executable_path': '/bin/wrong'}, {**expected, 'command': '/bin/wrong', 'executable_path': '/bin/wrong'}, 'matched',
             'terminal_native_process_command_mismatch'),
            (expected, expected, 'mismatched', 'terminal_native_runtime_binding_unproven'),
            (expected, None, 'matched', 'terminal_native_process_generation_unproven'),
        )
        for observed, recorded, binding, error in cases:
            with self.subTest(error=error), \
                 patch.object(harness, 'process_generation_snapshot', return_value=observed), \
                 patch.object(harness, 'pid_command', return_value=str(executable)) as auxiliary, \
                 patch.object(harness, 'pid_is_alive', return_value=True), \
                 patch.object(harness, 'compare_runtime_binding', return_value={'status': binding}):
                identity = harness.terminal_native_startup_identity(42, executable, {}, recorded)
                self.assertFalse(identity['ok'])
                self.assertEqual(identity['error'], error)
                auxiliary.assert_not_called()

    def test_startup_identity_result_drives_actual_bridge_descriptor_admission(self):
        import io
        from types import SimpleNamespace
        from cockpit_file_bridge import FileBackedCockpitBridge
        executable = Path(sys.executable).resolve()
        expected = {'pid': 42, 'alive': True, 'birth_identity': 'actual', 'command': str(executable), 'executable_path': str(executable)}
        for current_command in (str(executable), ''):
            with self.subTest(current_command=current_command), tempfile.TemporaryDirectory() as raw, \
                 patch.object(harness, 'process_generation_snapshot',
                              return_value={**expected, 'command': current_command}), \
                 patch.object(harness, 'pid_command', return_value=''), \
                 patch.object(harness, 'pid_is_alive', return_value=True), \
                 patch.object(harness, 'compare_runtime_binding', return_value={'status': 'matched'}):
                identity = harness.terminal_native_startup_identity(42, executable, {}, expected)
                descriptor = {'schema': 'caol-cockpit-live-session-v1',
                              'entry_mode': 'cockpit_live_session', 'run_id': 'run-a',
                              'binding_id': 'native-a', 'bridge_binding_id': 'bound-a'}
                envelope = ({'cockpit_live_session': descriptor} if identity['ok'] else
                            {'ok': False, 'startup': {'reason': 'terminal_native_identity_unproven',
                                                      'focus': identity}})
                bridge = FileBackedCockpitBridge(Path(raw) / 'session', [sys.executable, '-c', 'pass'],
                                                binding_id='bound-a',
                                                require_session_ready=True)
                bridge.prepare()
                bridge._child = SimpleNamespace(stdout=io.StringIO(), poll=lambda: None)
                with patch.object(bridge, '_read_complete_response', return_value=json.dumps(envelope)):
                    if current_command:
                        self.assertEqual(bridge._await_session_descriptor(), descriptor)
                        self.assertFalse(bridge._startup_failure)
                    else:
                        with self.assertRaisesRegex(ValueError, 'terminal_native_identity_unproven'):
                            bridge._await_session_descriptor()
                        self.assertEqual(bridge._startup_failure['reason'],
                                         'terminal_native_identity_unproven')
                self.assertTrue((bridge.session_dir / 'child.startup.stdout.jsonl').is_file())

    @unittest.skipUnless(os.name == 'posix', 'POSIX adapter')
    def test_actual_launch_pair_retains_lease_and_routes_only_its_input(self):
        code = 'import os,sys,tty; assert os.environ["TERM"] == "xterm-256color"; tty.setraw(0);\nwhile True:\n c=os.read(0,1); os.write(1,b"echo:"+c);\n if c==b"q": break\n'
        children = []
        observations = []
        with tempfile.TemporaryDirectory() as raw:
            base = Path(raw).resolve()
            try:
                with patch.object(harness, 'repo_root', return_value=base), \
                     patch.object(harness, 'userdir_for_profile', side_effect=lambda name: base/name), \
                     patch.object(harness, 'build_game_command', return_value=[sys.executable, '-c', code]):
                    for name in ('a', 'b'):
                        run = base/name/'harness_runs'/('run-'+name)
                        run.mkdir(parents=True)
                        with own_writable_root(base/name, 'run-'+name, harness.process_generation_snapshot, harness.process_generation_matches):
                            child = harness.launch_game(name, 'world', run, executable=sys.executable, terminal_transport='pty', transition_event_run_id='run-'+name, child_environment={**os.environ, "TERM": "dumb"})
                        children.append((name, run, child))
                        self.assertEqual(json.loads((run/'process.json').read_text())['TERM'], 'xterm-256color')
                        initial = json.loads((run/'process.json').read_text())
                        # The production outer probe supplies additional facts after launch.
                        outer = {key: value for key, value in initial.items() if key != 'TERM'}
                        outer['harness_raw_seed'] = False
                        harness.write_startup_process_record(run, child, outer)
                        updated = json.loads((run/'process.json').read_text())
                        self.assertEqual(updated, {**outer, 'TERM': 'xterm-256color'})
                        self.assertNotIn('TERM', outer)
                        owner = json.loads((run/'terminal.owner.json').read_text())
                        self.assertEqual(owner['host'], socket.gethostname())
                        self.assertTrue(owner['game_process_generation']['birth_identity'])
                        self.assertTrue(owner['broker_process_generation']['birth_identity'])
                        self.assertGreater(owner['lease_fd_count'], 1)
                        observations.append({key: owner[key] for key in ['host','run_id','game_process_generation','broker_process_generation','endpoint']})
                        harness.register_terminal_native_input(child.pid, str(child._openclaw_terminal_input_endpoint), 'run-'+name, run)
                        with self.assertRaises(WritableRootConflict):
                            with own_writable_root(base/name, 'collision', harness.process_generation_snapshot, harness.process_generation_matches): pass
                    a, b = children
                    self.assertTrue(harness.terminal_native_press_sequence(a[2].pid, ['a'])['ok'])
                    with self.assertRaises(RuntimeError):
                        dispatch_input(Path(a[2]._openclaw_terminal_input_endpoint), run_id='run-b', pid=b[2].pid, keys=['q'])
                    self.assertTrue(harness.terminal_native_press_sequence(a[2].pid, ['q'])['ok'])
                    a[2].wait(timeout=5)
                    self.assertIsNone(b[2].poll())
                    self.assertEqual(dispatcher_status(Path(b[2]._openclaw_terminal_input_endpoint))['game_status'], 'alive')
                    self.assertTrue(harness.terminal_native_press_sequence(b[2].pid, ['b', 'q'])['ok'])
                    b[2].wait(timeout=5)
            finally:
                for name, run, child in children:
                    if child.poll() is None:
                        dispatch_input(Path(child._openclaw_terminal_input_endpoint), run_id='run-'+name, pid=child.pid, keys=['q'])
                        child.wait(timeout=5)
                    harness.TERMINAL_NATIVE_INPUTS.pop(child.pid, None)
                    endpoint = Path(child._openclaw_terminal_input_endpoint)
                    deadline = time.monotonic()+5
                    while endpoint.parent.exists() and time.monotonic()<deadline: time.sleep(.05)
                    self.assertFalse(endpoint.parent.exists(), str(endpoint.parent))
            print(json.dumps({"control": "actual_launch_pair", "owners": observations, "children_exited": True, "private_directories_removed": True}))





class TerminalGenerationProbeTest(unittest.TestCase):
    def test_public_probe_finalizes_generation_before_live_world_gate(self):
        from test_harness_new_world import generation_scenario, generation_startup
        scenario = generation_scenario()
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw).resolve(); run = root/"run"; run.mkdir()
            args = harness.build_parser().parse_args(["probe", scenario["name"], "--profile", "offline"])
            args._loaded_scenario = scenario
            start = generation_startup(run)
            args.registry_launch_receipt = '{"runtime_binding":{"run_id":"offline-run"}}'
            def startup(command, **kwargs):
                self.assertEqual(command[command.index("--harness-new-world")+1], "ordinary-offline")
                self.assertEqual(command[command.index("--harness-raw-seed")+1], "830205385")
                self.assertEqual(command[command.index("--harness-new-world-scenario")+1], "evacuee")
                self.assertFalse(kwargs["live_session"])
                return 0, start, "owned generator stub", ""
            with patch.object(harness, "repo_root", return_value=root), \
                 patch.object(harness, "load_scenario", return_value=scenario), \
                 patch.object(harness, "validate_selected_product_build", return_value=({
                     "build_configuration": {"renderer": "curses", "make_variables": ["SOUND=0"]}}, "")), \
                 patch.object(harness, "scenario_manifest_binding", return_value={"source": {"sha256": "offline"}}), \
                 patch.object(harness, "registry_authority_transition_run_id", return_value="offline-run"), \
                 patch.object(harness, "detect_executable", return_value=root/"game"), \
                 patch.object(harness, "run_startup_in_process", side_effect=startup) as called, \
                 patch.object(harness, "finalize_scenario_report") as finalize, \
                 patch.object(harness, "startup_actions_ready", side_effect=AssertionError("generation required live World")), \
                 patch.object(harness, "launch_game", side_effect=AssertionError("offline test launched native game")):
                self.assertEqual(harness.run_probe_mode(args), 0)
            called.assert_called_once(); finalize.assert_called_once()
            report = finalize.call_args.args[1]
            self.assertTrue(report["ok"]); self.assertFalse(report["feature_proof"])
            self.assertEqual(report["world"], "ordinary-offline")
            self.assertEqual(finalize.call_args.kwargs["cleanup_pid"], 101)
            self.assertTrue((root/".userdata/offline/.harness-owner.lock").is_file())
            self.assertFalse((root/".userdata/offline/save").exists())

    def test_declared_generation_failure_is_finalized_without_live_or_relaunch(self):
        from test_harness_new_world import generation_scenario, generation_startup
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw).resolve(); run = root/"run"; run.mkdir()
            args = harness.build_parser().parse_args(["probe", "offline", "--profile", "offline"])
            args._loaded_scenario = generation_scenario()
            args.registry_launch_receipt = '{"runtime_binding":{"run_id":"offline-run"}}'
            start = generation_startup(run); start.update(ok=False, exit_code=1, reason="harness_world_process_failed")
            with patch.object(harness, "repo_root", return_value=root), \
                 patch.object(harness, "scenario_manifest_binding", return_value={}), \
                 patch.object(harness, "registry_authority_transition_run_id", return_value="offline-run"), \
                 patch.object(harness, "detect_executable", return_value=root/"game"), \
                 patch.object(harness, "run_startup_in_process", return_value=(1, start, "", "")), \
                 patch.object(harness, "finalize_scenario_report") as finalize, \
                 patch.object(harness, "launch_game", side_effect=AssertionError("native launch")):
                self.assertEqual(harness._run_probe_mode(args), 1)
            report = finalize.call_args.args[1]
            self.assertFalse(report["ok"]); self.assertFalse(report["feature_proof"])
            self.assertEqual(report["screen"]["status"], "not_required_generation_only")


    def test_invalid_generation_refuses_before_profile_or_native_launch(self):
        from test_harness_new_world import generation_scenario
        scenario = generation_scenario(); scenario["fixture"] = "legacy-fixture"
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw).resolve()
            args = harness.build_parser().parse_args(["probe", "offline", "--profile", "offline"])
            with patch.object(harness, "repo_root", return_value=root), \
                 patch.object(harness, "load_scenario", return_value=scenario), \
                 patch.object(harness, "registry_authority_transition_run_id", return_value="offline-run"), \
                 patch.object(harness, "detect_executable", return_value=root/"game"), \
                 patch.object(harness, "run_startup_in_process") as startup:
                with self.assertRaisesRegex(SystemExit, "generation cannot include"):
                    harness.run_probe_mode(args)
            startup.assert_not_called()
            self.assertFalse((root/".userdata").exists())

if __name__ == '__main__': unittest.main()
