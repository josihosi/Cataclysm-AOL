"""Native lease identity tests; kernel handles are doubles, not game proof."""
from __future__ import annotations

from contextlib import ExitStack
from copy import deepcopy
import ctypes
from dataclasses import replace
import json
import os
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parent))

from certification_process_lease import ProcessSnapshot, process_command_identity
import startup_harness
import windows_native_process as native
from writable_run_owner import WindowsWritableRunOwner


class LeaseKernel:
    """Only the native handle API is replaced; producer and owner are real."""

    def __init__(self):
        self.paths = {}
        self.next_handle = 100
        self.opened_pids = []
        self.duplicates = []
        self.closed = []

    def file_handle(self, path):
        self.next_handle += 1
        self.paths[self.next_handle] = os.path.normcase(os.path.normpath(str(path)))
        return self.next_handle

    def CreateFileW(self, path, *_):
        return self.file_handle(path)

    def GetFinalPathNameByHandleW(self, handle, buffer, *_):
        buffer.value = self.paths[getattr(handle, "value", handle)]
        return len(buffer.value)

    def OpenProcess(self, access, inherit, pid):
        self.opened_pids.append(pid)
        return 9000

    def GetCurrentProcess(self):
        return 9001

    def DuplicateHandle(self, source, handle, destination, output, *_):
        duplicate = self.file_handle(self.paths[handle])
        ctypes.cast(output, ctypes.POINTER(native.w.HANDLE)).contents.value = duplicate
        self.duplicates.append(duplicate)
        return True

    def CloseHandle(self, handle):
        self.closed.append(handle)
        return True


class NativeLeaseAdoptionTest(unittest.TestCase):
    def setUp(self):
        self.stack = ExitStack()
        self.addCleanup(self.stack.close)
        temporary = self.stack.enter_context(tempfile.TemporaryDirectory())
        self.root = Path(temporary).resolve() / "profile"
        self.root.mkdir()
        self.kernel = LeaseKernel()
        self.snapshot = ProcessSnapshot(os.getpid(), True, "owned-python.exe",
                                        "windows-filetime:123456789", "owned-launcher --test")
        self.inspector = SimpleNamespace(inspect=lambda pid: self.snapshot)
        self.stack.enter_context(patch.object(native, "lease_kernel", return_value=self.kernel))
        self.stack.enter_context(patch.object(native, "WindowsProcessInspector",
                                              return_value=self.inspector))
        self.stack.enter_context(patch.object(os, "O_BINARY", getattr(os, "O_BINARY", 0),
                                              create=True))
        self.stack.enter_context(patch.dict(sys.modules, {
            "msvcrt": SimpleNamespace(get_osfhandle=lambda fd:
                                      self.kernel.file_handle(self.root / ".harness-owner.lock")),
        }))
        # This is the actual startup caller's public generation producer.
        inspect = lambda pid: startup_harness.process_generation_snapshot(
            pid, inspector=self.inspector)
        self.owner = WindowsWritableRunOwner(self.root, "native-test", inspect,
                                             startup_harness.process_generation_matches)
        self.stack.callback(self.owner.close)
        self.owner.begin_launch()
        self.export = self.owner.export_native_lease()

    def adopt(self, export=None, run_id="native-test"):
        return native.adopt_native_lease(export or self.export, run_id=run_id)

    def assert_identity_rejected(self):
        with self.assertRaisesRegex(OSError, "launcher identity unavailable or changed"):
            self.adopt()
        self.assertEqual(self.kernel.opened_pids, [])
        self.assertEqual(self.kernel.duplicates, [])

    def test_public_producer_owner_export_adopts_exact_hashed_generation(self):
        persisted = json.loads(self.owner.path.read_text())["launcher_generation"]
        self.assertNotIn("command", persisted)
        self.assertEqual(persisted["command_sha256"],
                         process_command_identity({"command": self.snapshot.command}))
        self.assertEqual(self.export["owner_generation"], persisted)
        held = self.adopt()
        self.addCleanup(lambda: [self.kernel.CloseHandle(handle) for handle in held])
        self.assertEqual(len(held), len(self.export["handles"]))
        self.assertEqual(self.kernel.opened_pids, [os.getpid()])
        self.assertIn(9000, self.kernel.closed)

    def test_pid_reuse_birth_executable_command_or_dead_launcher_rejected(self):
        original = self.snapshot
        changes = ({"pid": original.pid + 1}, {"birth_identity": "windows-filetime:other"},
                   {"executable_path": "other-python.exe"}, {"command": "different-launcher"},
                   {"alive": False}, {"birth_identity": ""}, {"executable_path": ""})
        for change in changes:
            with self.subTest(change=change):
                self.snapshot = replace(original, **change)
                self.assert_identity_rejected()

    def test_wrong_missing_or_malformed_command_fingerprint_rejected(self):
        for digest in ("0" * 64, "", "not-a-digest", None):
            with self.subTest(digest=digest):
                self.export["owner_generation"]["command_sha256"] = digest
                self.assert_identity_rejected()

    def test_wrong_host_run_or_root_alias_rejected_before_handle_adoption(self):
        for key, value in (("host", "other-host"), ("run_id", "other-run"),
                           ("userdir", str(self.root / ".." / "profile"))):
            with self.subTest(key=key):
                export = deepcopy(self.export)
                export[key] = value
                with self.assertRaises(OSError):
                    self.adopt(export)
                self.assertEqual(self.kernel.opened_pids, [])
                self.assertEqual(self.kernel.duplicates, [])

    def test_reservation_path_and_generation_still_must_match(self):
        marker = json.loads(self.owner.path.read_text())
        for change in ({"userdir": str(self.root.parent)}, {"launch_state": "bound"},
                       {"launcher_generation": {"pid": os.getpid()}}):
            with self.subTest(change=change):
                self.owner.path.write_text(json.dumps({**marker, **change}))
                with self.assertRaisesRegex(OSError, "reservation mismatch"):
                    self.adopt()
                self.assertEqual(self.kernel.opened_pids, [])

    def test_wrong_transferred_path_closes_duplicate_and_process_handles(self):
        export = deepcopy(self.export)
        export["paths"][0] += "-other"
        with self.assertRaisesRegex(OSError, "handle path mismatch"):
            self.adopt(export)
        self.assertEqual(len(self.kernel.duplicates), 1)
        self.assertTrue(set(self.kernel.duplicates + [9000]).issubset(self.kernel.closed))

    def test_identity_change_during_transfer_closes_all_adopted_handles(self):
        snapshots = iter((self.snapshot, replace(self.snapshot,
                                                birth_identity="windows-filetime:reused")))
        self.inspector.inspect = lambda pid: next(snapshots)
        with self.assertRaisesRegex(OSError, "launcher changed during handle transfer"):
            self.adopt()
        self.assertEqual(len(self.kernel.duplicates), len(self.export["handles"]))
        self.assertTrue(set(self.kernel.duplicates + [9000]).issubset(self.kernel.closed))


class ExplicitGameDisposalTest(unittest.TestCase):
    def test_eof_disposal_finalizer_and_bridge_require_confirmed_exit(self):
        import io
        from unittest.mock import Mock
        from cockpit_file_bridge import FileBackedCockpitBridge
        import cockpit_live_session_test as fixtures
        import scenario_registry_cli
        import hashlib

        session = self.root / 'bridge-finalizer'
        session.mkdir()
        owned = {'pid': 123, 'status': 'alive', 'expected': self.expected}
        trace = self.run / 'native.log'
        trace.write_text('')
        manifest = self.run / 'fixture.json'
        manifest.write_text('{}')
        digest = hashlib.sha256(manifest.read_bytes()).hexdigest()
        frame = {**fixtures.frame(1, 100), 'run_id': 'owned-run', 'frame_id': 'owned-run:1'}
        observed = {'pid': 123, 'alive': True, 'birth_identity': self.expected['birth_identity'],
                    'executable_path': self.expected['executable_path'], 'command': 'owned game command'}
        for case in ('observed', 'unobserved', 'late_exit', 'changed_birth', 'ambiguous_absence'):
            with self.subTest(case=case):
                (self.run / 'cockpit.player_quit.json').unlink(missing_ok=True)
                self.wait_result = 0 if case == 'observed' else 258
                late_expected = dict(self.expected)
                if case == 'changed_birth':
                    late_expected['birth_identity'] = 'windows-filetime:999'
                later = {'pid': 123, 'status': 'exited', 'expected': late_expected,
                         'observed': {'pid': 123, 'alive': None if case == 'ambiguous_absence' else False}}
                identities = [owned, later if case in ('late_exit', 'changed_birth', 'ambiguous_absence') else owned]
                hook = Mock()
                report = {'mode': 'probe', 'steps': [], 'contract': {'profile': 'fixture'},
                          'scenario_manifest': {'source': {'path': str(manifest), 'sha256': digest}},
                          'proof_classification': {'status': 'amber'}}
                with patch.dict(os.environ, {'OPENCLAW_COCKPIT_BRIDGE_SESSION_DIR': str(session),
                                            'OPENCLAW_COCKPIT_BRIDGE_BINDING_ID': 'native-binding'}), \
                     patch.object(startup_harness, 'current_owned_process_generation', return_value=owned) as identity, \
                     patch.object(startup_harness, '_process_generation_snapshot', return_value=observed), \
                     patch.object(native, 'WindowsProcessInspector', return_value=self.inspector), \
                     patch.object(startup_harness, 'semantic_step_source_trace', return_value=trace), \
                     patch.object(startup_harness, 'current_semantic_step_frame', return_value=frame), \
                     patch('process_performance.ProcessPerformance', side_effect=ValueError('fixture')), \
                     patch('sys.stdout', new_callable=io.StringIO):
                    service = startup_harness.open_cockpit_game_service(profile='test', run_dir=self.run,
                        run_id='owned-run', trace_start_offset=0, pid=123, session_id='test', live_session=True,
                        cleanup_on_finish=False)
                    try:
                        self.termination_ok = False
                        incoming = io.StringIO(json.dumps({'action': 'game.observe'}) + '\n' +
                                               json.dumps({'action': 'run.quit', 'abort': True}) + '\n')
                        output = io.StringIO()
                        self.assertEqual(startup_harness.serve_cockpit_live(service, incoming, output), 1)
                        replies = [json.loads(line) for line in output.getvalue().splitlines()]
                        self.assertEqual(replies[-1]['error'], 'client_disconnected')
                        self.assertEqual(replies[-2]['result']['cleanup']['status'], 'retained_explicit_abort_refused')
                    finally:
                        service.live_channel.archive.close()
                    self.termination_ok = True
                    identity.side_effect = identities
                    startup_harness.finalize_probe_report(self.run, report, cleanup_pid=123,
                                                         post_finalize_hook=hook)
                accepted = case in ('observed', 'late_exit')
                cleanup = report['cleanup']
                self.assertEqual(cleanup['status'], {'observed': 'explicit_abort_terminated',
                    'late_exit': 'already_exited'}.get(case, 'explicit_abort_exit_unobserved_process_retained'))
                if case == 'observed':
                    self.assertTrue(cleanup['os_exit_observed'])
                for credit in ('native_exit_credit', 'native_save_credit', 'normal_exit_credit'):
                    self.assertFalse(cleanup[credit])
                self.assertEqual(hook.call_count, int(accepted))
                migration = scenario_registry_cli._migration_report_terminal(
                    {'source_path': str(manifest.resolve()), 'source_sha256': digest, 'profile': 'fixture'},
                    self.run / 'probe.report.json', report)
                self.assertEqual(migration is not None, accepted)
                signal = json.loads((session / 'cockpit.bridge.safe_to_cleanup.json').read_text())
                self.assertEqual(signal['state'], 'safe_to_cleanup' if accepted else 'cleanup_unconfirmed')
                bridge = FileBackedCockpitBridge(session, ['unused'], binding_id='native-binding')
                bridge._child = Mock(returncode=0, stdout=SimpleNamespace(buffer=io.BytesIO()))
                bridge._terminal_request_id = 'finish'
                self.assertEqual(bridge._await_scenario_terminalization(), 0 if accepted else 1)
                self.assertEqual(json.loads((session / 'status.json').read_text())['state'],
                                 'safe_to_cleanup' if accepted else 'terminalization_failed')
        self.assertFalse(startup_harness.cleanup_is_accepted({'status': 'explicit_abort_terminated'}))

    def setUp(self):
        import socket
        self.stack = ExitStack()
        self.addCleanup(self.stack.close)
        self.root = Path(self.stack.enter_context(tempfile.TemporaryDirectory())).resolve() / 'profile'
        self.run = self.root / 'harness_runs' / 'run'
        self.run.mkdir(parents=True)
        self.expected = {'pid': 123, 'birth_identity': 'windows-filetime:123456789',
                         'executable_path': str(self.root / 'game.exe'),
                         'command_sha256': process_command_identity({'command': 'owned game command'})}
        self.snapshot = ProcessSnapshot(123, True, self.expected['executable_path'],
                                        self.expected['birth_identity'], 'owned game command')
        self.inspector = SimpleNamespace(inspect=lambda pid: self.snapshot)
        self.host = socket.gethostname()
        # Actual outer start() caller overwrites launch_game's earlier record;
        # write_startup_process_record preserves mode/run/generation, not transport.
        startup_harness.write_startup_process_record(self.run, SimpleNamespace(), {
            'pid':123, 'run_id':'owned-run', 'host':self.host,
            'userdir':str(self.root), 'mode':'terminal', 'process_generation':self.expected})
        self.write('terminal.owner.json', {'schema': 'caol-curses-terminal-owner-v1', 'run_id': 'owned-run',
            'host': self.host, 'game_pid': 123, 'game_process_generation': self.expected,
            'run_owner_path': str(self.run / 'terminal.owner.json'), 'transport':'windows_conpty'})
        (self.root / '.harness-owner.lock').write_text(json.dumps({'schema': 'caol-writable-run-owner-v1',
            'host': self.host, 'run_id': 'owned-run', 'userdir': str(self.root), 'launch_state': 'bound',
            'game_generation': self.expected}))
        self.write('cockpit.player_quit.json', {'schema': 'caol-cockpit-player-quit-v1',
            'pid': 123, 'run_id': 'owned-run', 'binding_id': 'native-binding',
            'command_sha256': self.expected['command_sha256'], 'explicit_player_abort': True})
        self.terminated, self.closed, self.opened = [], [], []
        self.handle_birth, self.handle_image = 123456789, self.expected['executable_path']
        self.wait_result, self.termination_ok = 0, True
        def open_handle(access, inherit, pid):
            self.opened.append((access, inherit, pid))
            return 999
        def terminate(handle, code):
            self.terminated.append((handle, code))
            return self.termination_ok
        def exit_code(handle, output):
            ctypes.cast(output, ctypes.POINTER(native.w.DWORD)).contents.value = 1
            return True
        self.kernel = SimpleNamespace(OpenProcess=open_handle, TerminateProcess=terminate,
            WaitForSingleObject=lambda h, ms: self.wait_result,
            GetExitCodeProcess=exit_code, CloseHandle=lambda h: self.closed.append(h) or True)
        self.stack.enter_context(patch.object(native, 'kernel', return_value=self.kernel))
        self.stack.enter_context(patch.object(native, 'handle_birth', side_effect=lambda k,h: self.handle_birth))
        self.stack.enter_context(patch.object(native, 'handle_image', side_effect=lambda k,h: self.handle_image))
        self.stack.enter_context(patch.object(native, 'handle_alive', return_value=True))
        def checked(value):
            if not value:
                raise OSError('controlled kernel refusal')
        self.stack.enter_context(patch.object(native, 'checked', side_effect=checked))
        # Windows caller branch only; do not change pathlib's host semantics.
        self.stack.enter_context(patch.object(startup_harness, 'os', SimpleNamespace(**{**vars(os), 'name':'nt'})))

    def write(self, name, value):
        (self.run / name).write_text(json.dumps(value))

    def cleanup(self, **changes):
        arguments = dict(explicit_quit=True, explicit_abort=True, run_dir=self.run,
                         expected_process_generation=self.expected, inspector=self.inspector)
        arguments.update(changes)
        return startup_harness.cleanup_game_process(123, **arguments)

    def test_exact_explicit_abort_caller_uses_minimal_reverified_handle_without_save_credit(self):
        self.assertNotIn('transport',json.loads((self.run/'process.json').read_text()))
        result = self.cleanup()
        self.assertEqual(result['status'], 'explicit_abort_terminated')
        self.assertEqual(self.opened, [(0x00101001, False, 123)])
        self.assertEqual(self.terminated, [(999, 1)])
        self.assertEqual(self.closed, [999])
        self.assertTrue(result['os_exit_observed'])
        self.assertEqual(result['exit_code'], 1)
        for key in ('native_exit_credit', 'native_save_credit', 'normal_exit_credit'):
            self.assertFalse(result[key])
        self.assertNotIn('command', result['observed_process_generation'])

    def test_implicit_or_graceful_quit_retains_and_never_opens_termination_handle(self):
        for changes in ({'explicit_abort':False}, {'explicit_quit':False}, {'run_dir':None}):
            result = self.cleanup(**changes)
            self.assertIn(result['status'], ('graceful_quit_unconfirmed_process_retained', 'retained_waiting_for_player_quit'))
        self.assertEqual(self.opened, [])
        self.assertEqual(self.terminated, [])

    def test_foreign_run_reservation_or_launch_ownership_refuses_before_handle(self):
        paths = [('process.json','run_id','foreign'), ('process.json','userdir',str(self.root.parent)),
                 ('terminal.owner.json','host','foreign-host'), ('terminal.owner.json','run_id','foreign'),
                 ('cockpit.player_quit.json','run_id','foreign'), ('cockpit.player_quit.json','command_sha256','0'*64)]
        for name,key,value in paths:
            path = self.run / name
            raw = path.read_bytes()
            changed = json.loads(raw); changed[key] = value; self.write(name,changed)
            self.assertEqual(self.cleanup()['status'], 'retained_explicit_abort_refused')
            path.write_bytes(raw)
        self.assertEqual(self.opened, [])
        self.assertEqual(self.terminated, [])

    def test_authoritative_terminal_owner_transport_wrong_or_missing_is_retained(self):
        path=self.run/'terminal.owner.json';original=json.loads(path.read_text())
        for value in ('other_transport',None):
            changed=dict(original)
            if value is None:
                changed.pop('transport')
            else:
                changed['transport']=value
            self.write('terminal.owner.json',changed)
            self.assertEqual(self.cleanup()['status'],'retained_explicit_abort_refused')
        self.assertEqual(self.opened,[]);self.assertEqual(self.terminated,[])

    def test_birth_reuse_executable_and_command_hash_mismatch_cannot_dispose(self):
        original = self.snapshot
        for change in ({'birth_identity':'windows-filetime:999'}, {'executable_path':'foreign.exe'},
                       {'command':'foreign command'}, {'alive':False}):
            self.snapshot = replace(original, **change)
            result = self.cleanup()
            self.assertNotEqual(result['status'], 'explicit_abort_terminated')
        self.snapshot = original
        self.assertEqual(self.opened, [])
        self.assertEqual(self.terminated, [])

    def test_handle_identity_race_and_owner_mutation_close_without_termination(self):
        for field,value in (('handle_birth',999), ('handle_image','foreign.exe')):
            old = getattr(self,field); setattr(self,field,value)
            result = self.cleanup()
            self.assertEqual(result['status'],'retained_explicit_abort_refused')
            setattr(self,field,old)
        original = self.kernel.OpenProcess
        path = self.root / '.harness-owner.lock'; raw=path.read_bytes()
        def race(*args):
            value=json.loads(raw); value['run_id']='foreign'; path.write_text(json.dumps(value))
            return original(*args)
        self.kernel.OpenProcess=race
        self.assertEqual(self.cleanup()['status'],'retained_explicit_abort_refused')
        path.write_bytes(raw)
        self.assertEqual(self.terminated, [])
        self.assertEqual(self.closed, [999,999,999])

    def test_termination_failure_and_exit_timeout_close_handle_without_exit_credit(self):
        self.termination_ok=False
        self.assertEqual(self.cleanup()['status'],'retained_explicit_abort_refused')
        self.termination_ok=True;self.wait_result=258
        result=self.cleanup()
        self.assertEqual(result['status'],'explicit_abort_exit_unobserved_process_retained')
        self.assertNotIn('os_exit_observed',result)
        self.assertFalse(result['native_exit_credit'])
        self.assertEqual(self.closed,[999,999])

    def test_exit_code_read_failure_after_termination_does_not_report_refusal(self):
        self.kernel.GetExitCodeProcess = lambda handle, output: False
        result = self.cleanup()
        self.assertEqual(result['status'], 'explicit_abort_exit_unobserved_process_retained')
        self.assertEqual(self.terminated, [(999, 1)])
        self.assertIn('exit_observation_error', result)
        self.assertNotIn('os_exit_observed', result)
        self.assertFalse(result['native_exit_credit'])
        self.assertFalse(result['native_save_credit'])

    def test_explicit_boolean_without_owning_abort_receipt_is_insufficient(self):
        quit=json.loads((self.run/'cockpit.player_quit.json').read_text());quit.pop('explicit_player_abort')
        self.write('cockpit.player_quit.json',quit)
        self.assertEqual(self.cleanup()['status'],'retained_explicit_abort_refused')
        self.write('probe.report.json',{'steps':[{'cockpit_live_session':{'final':{
            'run_id':'owned-run','binding_id':'native-binding','closure_kind':'ordinary',
            'termination_requested':True,'stop_detail':{'explicit_player_abort':False}}}}]})
        self.assertEqual(self.cleanup()['status'],'retained_explicit_abort_refused')
        self.assertEqual(self.opened,[]);self.assertEqual(self.terminated,[])

    def test_command_fingerprint_changed_while_handle_is_held_is_retained(self):
        snapshots=iter([self.snapshot,self.snapshot,replace(self.snapshot,command='changed during handle bind')])
        self.inspector.inspect=lambda pid:next(snapshots)
        self.assertEqual(self.cleanup()['status'],'retained_explicit_abort_refused')
        self.assertEqual(self.terminated,[]);self.assertEqual(self.closed,[999])

    def test_pre_fix_explicit_abort_report_can_authorize_disposal_without_rewriting_it(self):
        quit=json.loads((self.run/'cockpit.player_quit.json').read_text());quit.pop('explicit_player_abort')
        self.write('cockpit.player_quit.json',quit)
        final={'run_id':'owned-run','binding_id':'native-binding','closure_kind':'explicit_abort',
               'termination_requested':True,'stop_detail':{'explicit_player_abort':True}}
        self.write('probe.report.json',{'steps':[{'cockpit_live_session':{'final':final}}]})
        raw=(self.run/'probe.report.json').read_bytes()
        self.assertEqual(self.cleanup()['status'],'explicit_abort_terminated')
        self.assertEqual((self.run/'probe.report.json').read_bytes(),raw)

    def test_public_abort_persists_authority_before_windows_disposal(self):
        import cockpit_live_session_test as fixtures
        (self.run / 'cockpit.player_quit.json').unlink()
        session = self.root / 'bridge'
        session.mkdir()
        trace = self.run / 'native.log'
        trace.write_text('')
        frame = {**fixtures.frame(1, 100), 'run_id': 'owned-run', 'frame_id': 'owned-run:1'}
        owned = {'pid': 123, 'status': 'alive', 'expected': self.expected}
        observed = {'pid': 123, 'alive': True, 'birth_identity': self.expected['birth_identity'],
                    'executable_path': self.expected['executable_path'], 'command': 'owned game command'}
        with patch.dict(os.environ, {'OPENCLAW_COCKPIT_BRIDGE_SESSION_DIR': str(session),
                                    'OPENCLAW_COCKPIT_BRIDGE_BINDING_ID': 'native-binding'}), \
             patch.object(startup_harness, 'semantic_step_source_trace', return_value=trace), \
             patch.object(startup_harness, 'current_semantic_step_frame', return_value=frame), \
             patch.object(startup_harness, 'current_owned_process_generation', return_value=owned), \
             patch.object(startup_harness, '_process_generation_snapshot', return_value=observed), \
             patch.object(native, 'WindowsProcessInspector', return_value=self.inspector), \
             patch('process_performance.ProcessPerformance', side_effect=ValueError('fixture')):
            service = startup_harness.open_cockpit_game_service(profile='test', run_dir=self.run,
                run_id='owned-run', trace_start_offset=0, pid=123, session_id='test', live_session=True,
                cleanup_on_finish=False)
            self.addCleanup(service.live_channel.archive.close)
            self.assertTrue(service.call({'action': 'game.observe'})['ok'])
            with patch.object(startup_harness, 'write_cockpit_player_quit_authority',
                              side_effect=OSError('controlled authority write failure')):
                refusal = service.call({'action': 'run.quit', 'abort': True})
            self.assertFalse(refusal['ok'])
            self.assertEqual(self.opened, [])
            self.assertEqual(self.terminated, [])
            self.assertFalse((self.run / 'cockpit.player_quit.json').exists())
            self.assertFalse((self.run / 'cockpit.live.final.json').exists())
            self.termination_ok = False
            refused = service.call({'action': 'run.quit', 'abort': True,
                                    'stop_reason': 'first explicit attempt'})
            self.assertTrue(refused['ok'], refused)
            self.assertEqual(refused['result']['cleanup']['status'], 'retained_explicit_abort_refused')
            self.assertTrue(refused['result']['retry_allowed'])
            self.assertEqual(self.terminated, [(999, 1)])
            self.assertTrue(service.call({'action': 'game.observe'})['ok'])
            self.assertEqual(self.terminated, [(999, 1)])  # Observation never retries disposal.
            self.assertEqual(service.call({'action': 'game.act', 'observation_id': 'owned-run:1',
                                           'action_id': 'world.wait'})['error'],
                             'abort_exit_observation_pending')
            self.snapshot = replace(self.snapshot, birth_identity='windows-filetime:999')
            drift = service.call({'action': 'run.quit', 'abort': True,
                                  'stop_reason': 'retry with changed identity'})
            self.assertEqual(drift['result']['cleanup']['status'], 'retained_explicit_abort_refused')
            self.assertEqual(self.terminated, [(999, 1)])
            self.snapshot = replace(self.snapshot, birth_identity=self.expected['birth_identity'])
            self.termination_ok = True
            result = service.call({'action': 'run.quit', 'abort': True,
                                   'stop_reason': 'owned disposal test'})
        self.assertTrue(result['ok'], result)
        self.assertEqual(result['result']['state'], 'exit_observation_pending')
        self.assertEqual(result['result']['cleanup']['status'], 'explicit_abort_terminated')
        self.assertEqual(self.terminated, [(999, 1), (999, 1)])
        authority = json.loads((self.run / 'cockpit.player_quit.json').read_text())
        self.assertTrue(authority['explicit_player_abort'])
        self.assertEqual((authority['run_id'], authority['binding_id'], authority['pid']),
                         ('owned-run', 'native-binding', 123))
        self.assertEqual(authority['command_sha256'], self.expected['command_sha256'])
        self.assertEqual(authority['stop_reason'], 'owned disposal test')
        self.assertFalse(result['result']['native_exit_credit'])
        self.assertFalse(result['result']['native_save_credit'])
        self.assertFalse((self.run / 'cockpit.live.final.json').exists())


if __name__ == "__main__":
    unittest.main()
