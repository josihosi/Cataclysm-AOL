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
                generation = {'pid': 100, 'birth_identity': 'current'}
                context = dict(vars(harness), run_dir=run, proc=process,
                    process_generation=generation,
                    transition_binding={'run_id': 'current-run', 'event_path': 'events'},
                    userdir_for_profile=lambda _: run, profile='current',
                    terminal_native_transport=selected_term is not None,
                    plan=SimpleNamespace(harness_new_world=False, harness_raw_seed=False,
                                         harness_bandit_feasibility=False),
                    child_environment={}, wait_diagnostic=False, debug_size=123,
                    killed_pids=[], certification_lease=None)
                exec(compile(module, 'production-outer-process-writer', 'exec'), context)
                record = json.loads((run/'process.json').read_text())
                self.assertEqual(record.get('TERM'), selected_term)
                self.assertEqual(record['pid'], 100)
                self.assertEqual(record['run_id'], 'current-run')
                self.assertEqual(record['process_generation'], generation)
                self.assertEqual(record['semantic_step_trace_start_offset'], 123)
                self.assertEqual(record['mode'], 'terminal' if selected_term else 'tiles')

    def test_startup_identity_rejects_missing_or_wrong_birth(self):
        observed = {'pid': 42, 'alive': True, 'birth_identity': 'actual', 'command': '/bin/game'}
        with patch.object(harness, 'process_generation_snapshot', return_value=observed), \
             patch.object(harness, 'pid_command', return_value='/bin/game'), \
             patch.object(harness, 'pid_is_alive', return_value=True), \
             patch.object(harness, 'compare_runtime_binding', return_value={'status': 'matched'}):
            for expected in (None, {**observed, 'birth_identity': 'stale'}):
                self.assertFalse(harness.terminal_native_startup_identity(42, Path('/bin/game'), {}, expected)['ok'])
            self.assertTrue(harness.terminal_native_startup_identity(42, Path('/bin/game'), {}, observed)['ok'])

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


if __name__ == '__main__': unittest.main()
