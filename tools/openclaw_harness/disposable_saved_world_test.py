#!/usr/bin/env python3
"""Explicit saved-copy custody and the actual closed-owner cleanup caller."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent))
import startup_harness as h


class DisposableSavedWorldTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name).resolve()
        self.root = self.base / 'profile'
        self.save = self.root / 'save'
        self.source = self.base / 'source'
        self.source.mkdir()
        (self.source / 'master.gsav').write_bytes(b'original world')
        self.source_hash = h.sha256_tree(self.source)
        self.patches = [mock.patch.object(h, 'userdir_for_profile', return_value=self.root),
                        mock.patch.object(h, 'save_dir_for_profile', return_value=self.save)]
        for patch in self.patches:
            patch.start()
            self.addCleanup(patch.stop)

    def create_closed_copy(self, *, disposable=True, existing=False, replace=False,
                           launch=True, run_id='run-1'):
        if existing:
            (self.save / 'World').mkdir(parents=True)
            (self.save / 'World' / 'master.gsav').write_bytes(b'different' if replace else b'original world')
        code = '''
import sys
from pathlib import Path
sys.path.insert(0, sys.argv[1])
import startup_harness as h
root, source = Path(sys.argv[2]), Path(sys.argv[3])
h.userdir_for_profile = lambda profile: root
h.save_dir_for_profile = lambda profile: root / 'save'
fake = {'schema': 'caol-owned-process-generation-v1', 'pid': 99999999 if sys.argv[7] == 'run-1' else 99999998, 'alive': True, 'birth_identity': 'test-generation', 'command_sha256': 'a' * 64}
def inspect(pid):
    return fake if pid == fake['pid'] else h.process_generation_snapshot(pid)
with h.own_writable_root(root, sys.argv[7], inspect, h.process_generation_matches) as owner:
    result = h.install_saved_world_snapshot('profile', 'World', source, replace=sys.argv[5] == 'yes', disposable_run_id=sys.argv[7] if sys.argv[4] == 'yes' else '')
    print(result['status'], flush=True)
    if sys.argv[6] == 'yes':
        owner.begin_launch()
        owner.bind_game(fake)
'''
        p = subprocess.run([sys.executable, '-c', code, str(Path(h.__file__).parent), str(self.root),
                            str(self.source), 'yes' if disposable else 'no', 'yes' if replace else 'no',
                            'yes' if launch else 'no', run_id],
                           capture_output=True, text=True, timeout=20)
        self.assertEqual(p.returncode, 0, p.stderr)
        return p.stdout.strip()

    def test_fresh_copy_closed_owner_removes_only_destination(self):
        self.assertEqual(self.create_closed_copy(), 'green_saved_world_snapshot_installed')
        self.assertEqual(h.sha256_tree(self.save / 'World'), self.source_hash)
        # Model the native save producer's world-specific complete generation.
        generation = self.save / '.save-continuity' / 'World' / 'complete' / 'world'
        generation.mkdir(parents=True)
        (generation / 'master.gsav').write_bytes(b'ordinary saved generation')
        foreign = self.save / '.save-continuity' / 'Other' / 'complete'
        foreign.mkdir(parents=True)
        (foreign / 'keep').write_bytes(b'foreign')
        (self.save / 'Keep').mkdir()
        result = h.cleanup_disposable_saved_world('profile', 'World', 'run-1')
        self.assertEqual(result['status'], 'removed')
        self.assertFalse((self.save / 'World').exists())
        self.assertFalse(generation.exists())
        self.assertEqual((foreign / 'keep').read_bytes(), b'foreign')
        self.assertTrue((self.save / 'Keep').is_dir())
        self.assertEqual(h.sha256_tree(self.source), self.source_hash)

    def test_existing_continuity_is_never_adopted_for_fresh_copy(self):
        from types import SimpleNamespace
        foreign = self.save / '.save-continuity' / 'World' / 'complete'
        foreign.mkdir(parents=True)
        (foreign / 'keep').write_bytes(b'foreign')
        with mock.patch.object(h, 'current_owner', return_value=SimpleNamespace(run_id='run-1')):
            with self.assertRaises(FileExistsError):
                h.install_saved_world_snapshot('profile', 'World', self.source, disposable_run_id='run-1')
        self.assertEqual((foreign / 'keep').read_bytes(), b'foreign')
        self.assertFalse((self.save / 'World').exists())
        self.assertEqual(h.sha256_tree(self.source), self.source_hash)

    def test_replaced_or_symlink_continuity_refuses_before_world_removal(self):
        self.create_closed_copy()
        continuity = self.save / '.save-continuity' / 'World'
        old = continuity.with_name('original')
        continuity.rename(old)
        continuity.mkdir()
        (continuity / 'keep').write_bytes(b'foreign')
        with self.assertRaises(h.WritableRootConflict):
            h.cleanup_disposable_saved_world('profile', 'World', 'run-1')
        self.assertEqual((continuity / 'keep').read_bytes(), b'foreign')
        self.assertTrue((self.save / 'World').exists())
        h.shutil.rmtree(continuity)
        continuity.symlink_to(old, target_is_directory=True)
        with self.assertRaises(h.WritableRootConflict):
            h.cleanup_disposable_saved_world('profile', 'World', 'run-1')
        self.assertTrue((self.save / 'World').exists())

    def test_continuity_failure_after_world_removal_retries(self):
        self.create_closed_copy()
        continuity = self.save / '.save-continuity' / 'World'
        (continuity / 'complete').mkdir()
        real = h.shutil.rmtree
        def fail_continuity(path, *args, **kwargs):
            if path == continuity:
                raise OSError('controlled continuity removal failure')
            return real(path, *args, **kwargs)
        with mock.patch.object(h.shutil, 'rmtree', side_effect=fail_continuity):
            with self.assertRaisesRegex(OSError, 'controlled continuity'):
                h.cleanup_disposable_saved_world('profile', 'World', 'run-1')
        self.assertFalse((self.save / 'World').exists())
        self.assertTrue((continuity / 'complete').exists())
        self.assertEqual(h.cleanup_disposable_saved_world('profile', 'World', 'run-1')['status'], 'removed')
        self.assertFalse(continuity.exists())
        self.assertEqual(h.sha256_tree(self.source), self.source_hash)

    def test_old_record_does_not_adopt_unowned_continuity(self):
        self.create_closed_copy()
        custody = next((self.root / '.saved-world-custody').glob('*.json'))
        record = json.loads(custody.read_text())
        del record['continuity_device'], record['continuity_inode']
        custody.write_text(json.dumps(record))
        continuity = self.save / '.save-continuity' / 'World'
        (continuity / 'foreign').write_bytes(b'keep')
        with self.assertRaises(h.WritableRootConflict):
            h.cleanup_disposable_saved_world('profile', 'World', 'run-1')
        self.assertEqual((continuity / 'foreign').read_bytes(), b'keep')
        self.assertTrue((self.save / 'World').exists())

    def test_later_closed_profile_run_does_not_strand_original_copy(self):
        self.create_closed_copy()
        self.assertEqual(self.create_closed_copy(run_id='run-2'), 'green_saved_world_snapshot_reused')
        before = (self.root / '.harness-owner.lock').read_bytes()
        latest = json.loads(before)['game_generation']
        real_inspect = h.process_generation_snapshot
        def live_latest(pid):
            return {**latest, 'alive': True} if pid == latest['pid'] else real_inspect(pid)
        with mock.patch.object(h, 'process_generation_snapshot', side_effect=live_latest):
            with self.assertRaises(h.WritableRootConflict):
                h.cleanup_disposable_saved_world('profile', 'World', 'run-1')
        self.assertTrue((self.save / 'World').exists())
        self.assertEqual((self.root / '.harness-owner.lock').read_bytes(), before)
        self.assertEqual(h.cleanup_disposable_saved_world('profile', 'World', 'run-1')['status'], 'removed')
        self.assertEqual((self.root / '.harness-owner.lock').read_bytes(), before)
        self.assertEqual(h.sha256_tree(self.source), self.source_hash)

    def test_closed_no_launch_can_clean_but_uncertain_launch_cannot(self):
        self.create_closed_copy(launch=False)
        lock = self.root / '.harness-owner.lock'
        prior = json.loads(lock.read_text())
        prior['launch_state'] = 'launching'
        lock.write_text(json.dumps(prior))
        before = h.sha256_tree(self.save / 'World')
        with self.assertRaises(h.WritableRootConflict):
            h.cleanup_disposable_saved_world('profile', 'World', 'run-1')
        self.assertEqual(h.sha256_tree(self.save / 'World'), before)
        prior['launch_state'] = 'not_started'
        lock.write_text(json.dumps(prior))
        self.assertEqual(h.cleanup_disposable_saved_world('profile', 'World', 'run-1')['status'], 'removed')

    def test_later_closed_no_launch_does_not_strand_original_copy(self):
        self.create_closed_copy()
        self.assertEqual(self.create_closed_copy(run_id='run-2', launch=False),
                         'green_saved_world_snapshot_reused')
        self.assertEqual(h.cleanup_disposable_saved_world('profile', 'World', 'run-1')['status'], 'removed')

    def test_reuse_retained_and_replacement_never_gain_disposal_authority(self):
        for existing, replace, disposable in [(True, False, True), (True, True, True), (False, False, False)]:
            with self.subTest(existing=existing, replace=replace, disposable=disposable):
                # One isolated fixture per subtest; owner lock records are not recycled.
                self.tearDown()
                self.doCleanups()
                self.setUp()
                self.create_closed_copy(disposable=disposable, existing=existing, replace=replace)
                before = h.sha256_tree(self.save / 'World')
                with self.assertRaises(h.WritableRootConflict):
                    h.cleanup_disposable_saved_world('profile', 'World', 'run-1')
                self.assertEqual(h.sha256_tree(self.save / 'World'), before)
                self.assertEqual(h.sha256_tree(self.source), self.source_hash)

    def test_mismatch_unsafe_and_live_or_unknown_are_refused(self):
        self.create_closed_copy()
        before = h.sha256_tree(self.save / 'World')
        with self.assertRaises(h.WritableRootConflict):
            h.cleanup_disposable_saved_world('profile', 'World', 'foreign-run')
        with self.assertRaises(h.WritableRootConflict):
            h.cleanup_disposable_saved_world('profile', '../World', 'run-1')
        real = h.process_generation_snapshot
        for state in (True, None):
            def inspect(pid):
                result = real(pid)
                if pid != os.getpid():
                    result['alive'] = state
                return result
            with self.subTest(alive=state), mock.patch.object(h, 'process_generation_snapshot', side_effect=inspect):
                with self.assertRaises(h.WritableRootConflict):
                    h.cleanup_disposable_saved_world('profile', 'World', 'run-1')
            self.assertEqual(h.sha256_tree(self.save / 'World'), before)
        moved = self.save / 'other'
        (self.save / 'World').rename(moved)
        (self.save / 'World').symlink_to(moved, target_is_directory=True)
        with self.assertRaises(h.WritableRootConflict):
            h.cleanup_disposable_saved_world('profile', 'World', 'run-1')
        self.assertEqual(h.sha256_tree(moved), before)

    def test_replaced_directory_and_foreign_custody_are_refused(self):
        self.create_closed_copy()
        destination = self.save / 'World'
        moved = self.save / 'original'
        destination.rename(moved)
        destination.mkdir()
        (destination / 'master.gsav').write_bytes(b'foreign world')
        with self.assertRaises(h.WritableRootConflict):
            h.cleanup_disposable_saved_world('profile', 'World', 'run-1')
        self.assertEqual((destination / 'master.gsav').read_bytes(), b'foreign world')
        self.assertEqual(h.sha256_tree(moved), self.source_hash)

    def test_start_caller_passes_explicit_purpose_under_run_lease(self):
        args = h.build_parser().parse_args([
            'start', '--profile', 'profile', '--world', 'World',
            '--saved-world-snapshot', str(self.source), '--disposable-saved-world',
            '--harness-run-id', 'run-1'])
        class ReachedInstall(Exception):
            pass
        def observe(profile, world, source, **kwargs):
            self.assertEqual((profile, world, source), ('profile', 'World', str(self.source)))
            self.assertEqual(kwargs['disposable_run_id'], 'run-1')
            self.assertEqual(h.current_owner(self.root).run_id, 'run-1')
            raise ReachedInstall()
        with mock.patch.object(h, 'load_profile_config', return_value={}), \
                mock.patch.object(h, 'install_saved_world_snapshot', side_effect=observe):
            with self.assertRaises(ReachedInstall):
                h.run_startup(args)

    def test_controlled_probe_rejects_snapshot_and_disposable_before_action(self):
        from types import SimpleNamespace
        cases = [({'saved_world_snapshot': str(self.source)}, {}),
                 ({'disposable_saved_world': True}, {}),
                 ({}, {'saved_world_snapshot': str(self.source)})]
        for fields, scenario_extra in cases:
            args = SimpleNamespace(scenario='controlled', **fields)
            with self.subTest(fields=fields), \
                    mock.patch.object(h, 'load_scenario', return_value={'controlled_camp_setup': {}, **scenario_extra}), \
                    mock.patch.object(h, 'run_controlled_camp_setup_probe') as controlled, \
                    mock.patch.object(h, 'own_writable_root') as lease:
                with self.assertRaisesRegex(SystemExit, 'does not support saved-world'):
                    h.run_probe_mode(args)
                controlled.assert_not_called()
                lease.assert_not_called()

    def test_foreign_world_appearing_during_copy_is_not_removed(self):
        import types
        copytree = h.shutil.copytree
        def race(source, staging, **kwargs):
            result = copytree(source, staging, **kwargs)
            foreign = self.save / 'World'
            foreign.mkdir()
            (foreign / 'keep').write_bytes(b'foreign')
            return result
        owner = types.SimpleNamespace(run_id='run-1')
        with mock.patch.object(h, 'current_owner', return_value=owner), \
                mock.patch.object(h.shutil, 'copytree', side_effect=race):
            with self.assertRaises(FileExistsError):
                h.install_saved_world_snapshot('profile', 'World', self.source,
                                               disposable_run_id='run-1')
        self.assertEqual((self.save / 'World' / 'keep').read_bytes(), b'foreign')
        self.assertEqual(list((self.root / '.saved-world-custody').glob('*.json')), [])

    def test_custody_creation_failures_never_publish_world_and_can_retry(self):
        from types import SimpleNamespace
        import tempfile as temp_module
        real_mkdir, real_mkstemp, real_fdopen = Path.mkdir, temp_module.mkstemp, os.fdopen
        for failure in ('mkdir', 'open', 'fdopen', 'write', 'close', 'publish'):
            with self.subTest(failure=failure):
                self.tearDown()
                self.doCleanups()
                self.setUp()
                def mkdir(path, *args, **kwargs):
                    if path.name == '.saved-world-custody':
                        raise OSError('controlled custody mkdir failure')
                    return real_mkdir(path, *args, **kwargs)
                def mkstemp(*args, **kwargs):
                    if str(kwargs.get('prefix', '')).startswith('.custody-'):
                        raise OSError('controlled custody open failure')
                    return real_mkstemp(*args, **kwargs)
                class FailClose:
                    def __init__(self, stream): self.stream = stream
                    def __enter__(self): return self.stream
                    def __exit__(self, *args):
                        self.stream.close()
                        raise OSError('controlled custody close failure')
                def fdopen(fd, *args, **kwargs):
                    return FailClose(real_fdopen(fd, *args, **kwargs))
                failure_patch = {
                    'mkdir': mock.patch.object(Path, 'mkdir', mkdir),
                    'open': mock.patch.object(temp_module, 'mkstemp', side_effect=mkstemp),
                    'fdopen': mock.patch.object(h.os, 'fdopen', side_effect=OSError('controlled custody fdopen failure')),
                    'write': mock.patch.object(h.json, 'dump', side_effect=OSError('controlled custody write failure')),
                    'close': mock.patch.object(h.os, 'fdopen', side_effect=fdopen),
                    'publish': mock.patch.object(h.os, 'rename' if os.name == 'nt' else 'link',
                                                 side_effect=OSError('controlled custody publish failure')),
                }[failure]
                with mock.patch.object(h, 'current_owner', return_value=SimpleNamespace(run_id='run-1')):
                    with failure_patch, self.assertRaisesRegex(OSError, 'controlled custody'):
                        h.install_saved_world_snapshot('profile', 'World', self.source, disposable_run_id='run-1')
                    self.assertFalse((self.save / 'World').exists())
                    self.assertEqual(list((self.root / '.saved-world-custody').glob('*')), [])
                    self.assertEqual(h.sha256_tree(self.source), self.source_hash)
                    result = h.install_saved_world_snapshot('profile', 'World', self.source, disposable_run_id='run-1')
                    self.assertEqual(result['status'], 'green_saved_world_snapshot_installed')
                    self.assertEqual(h.sha256_tree(self.save / 'World'), self.source_hash)

    @unittest.skipIf(os.name == 'nt', 'POSIX hard-link publication boundary')
    def test_temp_unlink_failure_after_publication_rolls_back_and_retries(self):
        from types import SimpleNamespace
        original_unlink = Path.unlink
        observed = []
        def fail_once(path, *args, **kwargs):
            if path.name.startswith('.custody-') and not observed:
                published = list(path.parent.glob('*.json'))
                self.assertEqual(len(published), 1)
                self.assertTrue(os.path.samefile(path, published[0]))
                observed.append(True)
                raise OSError('controlled post-publication unlink failure')
            return original_unlink(path, *args, **kwargs)
        with mock.patch.object(h, 'current_owner', return_value=SimpleNamespace(run_id='run-1')):
            with mock.patch.object(Path, 'unlink', fail_once):
                with self.assertRaisesRegex(OSError, 'controlled post-publication'):
                    h.install_saved_world_snapshot('profile', 'World', self.source, disposable_run_id='run-1')
            self.assertEqual(observed, [True])
            self.assertFalse((self.save / 'World').exists())
            self.assertEqual(list((self.root / '.saved-world-custody').glob('*')), [])
            self.assertEqual(h.sha256_tree(self.source), self.source_hash)
            result = h.install_saved_world_snapshot('profile', 'World', self.source, disposable_run_id='run-1')
            self.assertEqual(result['status'], 'green_saved_world_snapshot_installed')
            self.assertEqual(h.sha256_tree(self.save / 'World'), self.source_hash)

    @unittest.skipIf(os.name == 'nt', 'POSIX hard-link publication boundary')
    def test_post_publication_rollback_preserves_replaced_foreign_record(self):
        from types import SimpleNamespace
        original_unlink = Path.unlink
        changed = []
        def replace_then_fail(path, *args, **kwargs):
            if path.name.startswith('.custody-') and not changed:
                canonical = next(path.parent.glob('*.json'))
                original_unlink(canonical)
                canonical.write_text('foreign custody')
                changed.append(canonical)
                raise OSError('controlled post-publication replacement')
            return original_unlink(path, *args, **kwargs)
        with mock.patch.object(h, 'current_owner', return_value=SimpleNamespace(run_id='run-1')), \
                mock.patch.object(Path, 'unlink', replace_then_fail):
            with self.assertRaisesRegex(OSError, 'controlled post-publication'):
                h.install_saved_world_snapshot('profile', 'World', self.source, disposable_run_id='run-1')
        self.assertEqual(changed[0].read_text(), 'foreign custody')
        self.assertFalse((self.save / 'World').exists())
        self.assertEqual(h.sha256_tree(self.source), self.source_hash)

    def test_cleanup_removal_failure_preserves_owner_and_can_retry(self):
        self.create_closed_copy()
        lock = self.root / '.harness-owner.lock'
        before = lock.read_bytes()
        with mock.patch.object(h.shutil, 'rmtree', side_effect=OSError('controlled removal failure')):
            with self.assertRaisesRegex(OSError, 'controlled removal failure'):
                h.cleanup_disposable_saved_world('profile', 'World', 'run-1')
        self.assertEqual(lock.read_bytes(), before)
        self.assertTrue((self.save / 'World').exists())
        self.assertEqual(h.cleanup_disposable_saved_world('profile', 'World', 'run-1')['status'], 'removed')
        self.assertEqual(lock.read_bytes(), before)

    def test_record_unlink_failure_retries_as_authenticated_already_absent(self):
        self.create_closed_copy()
        lock = self.root / '.harness-owner.lock'
        before = lock.read_bytes()
        real_unlink = Path.unlink
        def fail_record(path, *args, **kwargs):
            if path.parent.name == '.saved-world-custody':
                raise OSError('controlled record failure')
            return real_unlink(path, *args, **kwargs)
        with mock.patch.object(Path, 'unlink', fail_record):
            with self.assertRaisesRegex(OSError, 'controlled record failure'):
                h.cleanup_disposable_saved_world('profile', 'World', 'run-1')
        self.assertFalse((self.save / 'World').exists())
        self.assertEqual(lock.read_bytes(), before)
        self.assertEqual(h.cleanup_disposable_saved_world('profile', 'World', 'run-1')['status'], 'already_absent')
        self.assertEqual(lock.read_bytes(), before)
        # A bare absent path without the producer record is not an authenticated outcome.
        with self.assertRaises(h.WritableRootConflict):
            h.cleanup_disposable_saved_world('profile', 'World', 'run-1')

    def test_windows_owner_preserving_lease_keeps_previous_record(self):
        import socket
        from types import SimpleNamespace
        import windows_native_process
        from writable_run_owner import WindowsWritableRunOwner
        self.root.mkdir()
        generation = lambda pid: {'schema': 'caol-owned-process-generation-v1', 'pid': pid,
                                  'birth_identity': 'previous-generation', 'command_sha256': 'a' * 64,
                                  'executable_path': 'python.exe'}
        prior = {'schema': 'caol-writable-run-owner-v1', 'host': socket.gethostname(),
                 'userdir': str(self.root), 'run_id': 'run-1', 'launcher_state': 'returned',
                 'launch_state': 'bound', 'launcher_generation': generation(99999998),
                 'game_generation': generation(99999999)}
        marker = self.root / '.harness-owner.lock'
        marker.write_text(json.dumps(prior))
        before = marker.read_bytes()
        def inspect(pid):
            if pid in (99999998, 99999999):
                return {'alive': False}
            return h.process_generation_snapshot(pid)
        seen = []
        with mock.patch.object(os, 'O_BINARY', 0, create=True), \
                mock.patch.object(windows_native_process, 'WindowsDirectoryLease',
                                  return_value=SimpleNamespace(close=lambda: None)):
            owner = WindowsWritableRunOwner(self.root, 'cleanup', inspect, h.process_generation_matches,
                                            prior_validator=seen.append, preserve_prior=True)
            try:
                self.assertEqual(seen[0]['run_id'], 'run-1')
                self.assertEqual(marker.read_bytes(), before)
                with self.assertRaises(h.WritableRootConflict):
                    owner.begin_launch()
            finally:
                owner.close()
        self.assertEqual(marker.read_bytes(), before)



if __name__ == '__main__':
    unittest.main()
