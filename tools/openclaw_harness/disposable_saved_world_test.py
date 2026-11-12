#!/usr/bin/env python3
"""Explicit saved-copy custody and the actual closed-owner cleanup caller."""
import json
import os
import hashlib
import io
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
from contextlib import redirect_stdout, redirect_stderr

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

    def fixture_payload(self):
        fixture = self.base / 'fixture'
        source = fixture / 'save' / 'World'
        source.mkdir(parents=True, exist_ok=True)
        (source / 'master.gsav').write_bytes(b'fixture source')
        (source / 'worldoptions.json').write_text(json.dumps([{'name': 'TEST_OPTION', 'value': 'old'}]))
        return {'fixture': 'fixture', 'fixture_profile': 'live-debug', 'fixture_dir': fixture,
                'save_src': fixture / 'save', 'save_transforms': [
                    {'kind': 'world_option_overrides', 'overrides': {'TEST_OPTION': 'new'}}],
                'source_chain': [('live-debug', 'fixture')], 'manifest': {}}

    def test_disposable_multiworld_fixture_touches_only_selected_world(self):
        from types import SimpleNamespace
        payload = self.fixture_payload()
        sibling = payload['save_src'] / 'Sibling'
        sibling.mkdir()
        (sibling / 'master.gsav').write_bytes(b'fixture sibling')
        before = h.sha256_tree(payload['save_src'])
        for replace in (False, True):
            for exists in (False, True):
                with self.subTest(replace=replace, sibling_exists=exists), tempfile.TemporaryDirectory() as tmp:
                    root = Path(tmp).resolve()
                    dest = root / 'save' / 'Sibling'
                    if exists:
                        dest.mkdir(parents=True)
                        (dest / 'keep').write_bytes(b'foreign sibling')
                    with mock.patch.object(h, 'userdir_for_profile', return_value=root), \
                            mock.patch.object(h, 'save_dir_for_profile', return_value=root / 'save'), \
                            mock.patch.object(h, 'resolve_fixture_payload', return_value=payload), \
                            mock.patch.object(h, 'fixture_source_binding', return_value={}), \
                            mock.patch.object(h, 'current_owner', return_value=SimpleNamespace(run_id='run')):
                        result = h.install_fixture('profile', 'fixture', replace, 'live-debug',
                                                   disposable_world='World', disposable_run_id='run')
                    self.assertEqual(result['installed_worlds'], ['World'])
                    self.assertEqual(dest.exists(), exists)
                    if exists:
                        self.assertEqual((dest / 'keep').read_bytes(), b'foreign sibling')
        self.assertEqual(h.sha256_tree(payload['save_src']), before)

    def test_nondisposable_multiworld_fixture_still_installs_siblings(self):
        payload = self.fixture_payload()
        sibling = payload['save_src'] / 'Sibling'
        sibling.mkdir()
        (sibling / 'master.gsav').write_bytes(b'fixture sibling')
        with mock.patch.object(h, 'resolve_fixture_payload', return_value=payload), \
                mock.patch.object(h, 'fixture_source_binding', return_value={}), \
                mock.patch.object(h, 'apply_fixture_save_transforms', return_value=[]):
            result = h.install_fixture('profile', 'fixture', False, 'live-debug')
        self.assertEqual(set(result['installed_worlds']), {'World', 'Sibling'})
        self.assertEqual((self.save / 'Sibling' / 'master.gsav').read_bytes(), b'fixture sibling')

    def snapshot_with_foreign_metadata(self):
        root = self.base / 'snapshots'
        snapshot = root / 'snapshot'
        snapshot.mkdir(parents=True)
        (snapshot / 'config').mkdir()
        (snapshot / 'config' / 'options.json').write_text('{}')
        (snapshot / '.harness-owner.lock').write_text('foreign owner')
        (snapshot / '.saved-world-custody').mkdir()
        (snapshot / '.saved-world-custody' / 'sibling.json').write_text('foreign custody')
        (snapshot / 'manifest.json').write_text('{}')
        return root, snapshot

    def test_snapshot_resolution_and_install_protect_owner_metadata(self):
        snapshots, snapshot = self.snapshot_with_foreign_metadata()
        self.root.mkdir()
        lock = self.root / '.harness-owner.lock'
        lock.write_text('active owner')
        custody = self.root / '.saved-world-custody'
        custody.mkdir()
        (custody / 'sibling.json').write_text('existing custody')
        lock_inode = lock.stat().st_ino
        before = h.sha256_tree(snapshot)
        with mock.patch.object(h, 'profile_snapshot_root', return_value=snapshots), \
                mock.patch.object(h, 'profile_snapshot_source_binding', return_value={}):
            resolved = h.resolve_profile_snapshot_payload('snapshot', 'profile')
            self.assertEqual([entry.name for entry in resolved['copyable_entries']], ['config'])
            # The consumer must also reject protected entries if a producer supplies them.
            resolved['copyable_entries'].extend([snapshot / '.harness-owner.lock', snapshot / '.saved-world-custody'])
            with mock.patch.object(h, 'resolve_profile_snapshot_payload', return_value=resolved):
                result = h.install_profile_snapshot('profile', 'snapshot')
        self.assertEqual(result['copied_entries'], ['config'])
        self.assertEqual(lock.stat().st_ino, lock_inode)
        self.assertEqual(lock.read_text(), 'active owner')
        self.assertEqual((custody / 'sibling.json').read_text(), 'existing custody')
        self.assertEqual(h.sha256_tree(snapshot), before)

    def test_snapshot_preserves_prior_disposable_cleanup_authority(self):
        self.create_closed_copy()
        snapshots, snapshot = self.snapshot_with_foreign_metadata()
        record = self.root / '.saved-world-custody' / (hashlib.sha256(b'World').hexdigest() + '.json')
        before_record = record.read_bytes()
        before_source = h.sha256_tree(snapshot)
        code = '''
import sys
from pathlib import Path
sys.path.insert(0, sys.argv[1])
import startup_harness as h
root, snapshots = Path(sys.argv[2]), Path(sys.argv[3])
h.userdir_for_profile = lambda profile: root
h.profile_snapshot_root = lambda profile: snapshots
h.profile_snapshot_source_binding = lambda *args, **kwargs: {}
with h.own_writable_root(root, 'later-run', h.process_generation_snapshot, h.process_generation_matches):
    h.install_profile_snapshot('profile', 'snapshot')
'''
        process = subprocess.run([sys.executable, '-c', code, str(Path(h.__file__).parent),
                                  str(self.root), str(snapshots)], capture_output=True, text=True, timeout=20)
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertEqual(record.read_bytes(), before_record)
        self.assertEqual(h.cleanup_disposable_saved_world('profile', 'World', 'run-1')['status'], 'removed')
        self.assertFalse((self.save / 'World').exists())
        self.assertEqual(h.sha256_tree(snapshot), before_source)

    def test_fixture_fresh_transform_has_closed_run_custody_and_real_cleanup(self):
        payload = self.fixture_payload()
        code = '''
import json, sys
from pathlib import Path
sys.path.insert(0, sys.argv[1])
import startup_harness as h
root, fixture = Path(sys.argv[2]), Path(sys.argv[3])
h.userdir_for_profile = lambda profile: root
h.save_dir_for_profile = lambda profile: root / 'save'
h.resolve_fixture_payload = lambda name, profile: {'fixture':name, 'fixture_profile':profile,
    'fixture_dir':fixture, 'save_src':fixture/'save', 'save_transforms':[
        {'kind':'world_option_overrides', 'overrides':{'TEST_OPTION':'new'}}],
    'source_chain':[(profile,name)], 'manifest':{}}
h.fixture_source_binding = lambda *args: {}
with h.own_writable_root(root, 'fixture-run', h.process_generation_snapshot, h.process_generation_matches):
    result = h.install_fixture('profile', 'fixture', False, 'live-debug',
        disposable_world='World', disposable_run_id='fixture-run')
    print(json.dumps(result), flush=True)
'''
        process = subprocess.run([sys.executable, '-c', code, str(Path(h.__file__).parent),
                                  str(self.root), str(payload['fixture_dir'])],
                                 capture_output=True, text=True, timeout=20)
        self.assertEqual(process.returncode, 0, process.stderr)
        result = json.loads(process.stdout)
        installed = self.save / 'World' / 'master.gsav'
        self.assertEqual(installed.read_bytes(), b'fixture source')
        self.assertEqual(result['applied_save_transforms'][0]['world'], 'World')
        self.assertEqual(json.loads((self.save / 'World' / 'worldoptions.json').read_text())[0]['value'], 'new')
        self.assertEqual(json.loads((payload['save_src'] / 'World' / 'worldoptions.json').read_text())[0]['value'], 'old')
        self.assertEqual((payload['save_src'] / 'World' / 'master.gsav').read_bytes(), b'fixture source')

        self.assertNotEqual(result['disposable_world_install']['source_sha256'],
                            result['disposable_world_install']['installed_sha256'])
        (self.save / '.save-continuity' / 'World' / 'complete').mkdir()
        with self.assertRaises(h.WritableRootConflict):
            h.cleanup_disposable_saved_world('profile', 'World', 'foreign-run')
        launcher = json.loads((self.root / '.harness-owner.lock').read_text())['launcher_generation']['pid']
        inspect = h.process_generation_snapshot
        def live_owner(pid):
            observed = inspect(pid)
            if pid == launcher:
                observed['alive'] = True
            return observed
        with mock.patch.object(h, 'process_generation_snapshot', side_effect=live_owner):
            with self.assertRaises(h.WritableRootConflict):
                h.cleanup_disposable_saved_world('profile', 'World', 'fixture-run')
        self.assertTrue(installed.exists())
        self.assertEqual(h.cleanup_disposable_saved_world('profile', 'World', 'fixture-run')['status'], 'removed')
        self.assertFalse(installed.exists())
        self.assertFalse((self.save / '.save-continuity' / 'World').exists())
        self.assertEqual((payload['save_src'] / 'World' / 'master.gsav').read_bytes(), b'fixture source')

    def test_real_r008_fixture_chain_installs_transforms_and_cleans_without_game(self):
        try:
            resolved = h.resolve_fixture_payload('r008_field_player_145_41_v1', 'live-debug')
        except SystemExit:
            self.skipTest('the optional R008 reusable fixture payload is unavailable')
        try:
            h.zzip_binary()
        except SystemExit:
            self.skipTest('the fixture compression helper is unavailable')
        source = resolved['save_src'] / 'McWilliams'
        if not source.is_dir():
            self.skipTest('the optional R008 reusable fixture payload is unavailable')
        before = h.sha256_tree(source)
        self.assertFalse(before[1])
        code = '''
import json, sys
from pathlib import Path
sys.path.insert(0, sys.argv[1])
import startup_harness as h
root = Path(sys.argv[2])
h.userdir_for_profile = lambda profile: root
h.save_dir_for_profile = lambda profile: root / 'save'
with h.own_writable_root(root, 'real-fixture-run', h.process_generation_snapshot, h.process_generation_matches):
    result = h.install_fixture('profile', 'r008_field_player_145_41_v1', True, 'live-debug',
        disposable_world='McWilliams', disposable_run_id='real-fixture-run')
    print(json.dumps(result), flush=True)
'''
        process = subprocess.run([sys.executable, '-c', code, str(Path(h.__file__).parent), str(self.root)],
                                 capture_output=True, text=True, timeout=120)
        self.assertEqual(process.returncode, 0, process.stderr)
        result = json.loads(process.stdout)
        self.assertEqual(result['installed_worlds'], ['McWilliams'])
        self.assertGreaterEqual(len(result['applied_save_transforms']), 1)
        self.assertEqual(result['disposable_world_install']['status'], 'green_saved_world_snapshot_installed')
        self.assertEqual(h.cleanup_disposable_saved_world('profile', 'McWilliams', 'real-fixture-run')['status'], 'removed')
        self.assertEqual(h.sha256_tree(source), before)

    def test_fixture_refuses_existing_missing_owner_and_wrong_world(self):
        from types import SimpleNamespace
        payload = self.fixture_payload()
        with mock.patch.object(h, 'resolve_fixture_payload', return_value=payload), \
                mock.patch.object(h, 'fixture_source_binding', return_value={}):
            for owner in (None, SimpleNamespace(run_id='foreign')):
                with self.subTest(owner=owner), mock.patch.object(h, 'current_owner', return_value=owner):
                    with self.assertRaisesRegex(SystemExit, 'exact writable run owner'):
                        h.install_fixture('profile', 'fixture', True, 'live-debug',
                                          disposable_world='World', disposable_run_id='fixture-run')
            with self.assertRaisesRegex(SystemExit, 'not an exact fixture world'):
                h.install_fixture('profile', 'fixture', True, 'live-debug',
                                  disposable_world='../World', disposable_run_id='fixture-run')
            destination = self.save / 'World'
            destination.mkdir(parents=True)
            (destination / 'worldoptions.json').write_bytes(
                (payload['save_src'] / 'World' / 'worldoptions.json').read_bytes())
            for contents in (b'foreign', b'fixture source'):
                (destination / 'master.gsav').write_bytes(contents)
                with self.subTest(existing=contents), \
                        mock.patch.object(h, 'current_owner', return_value=SimpleNamespace(run_id='fixture-run')):
                    with self.assertRaisesRegex(SystemExit, 'fresh world'):
                        h.install_fixture('profile', 'fixture', True, 'live-debug',
                                          disposable_world='World', disposable_run_id='fixture-run')
                self.assertEqual((destination / 'master.gsav').read_bytes(), contents)
            self.assertFalse((self.root / '.saved-world-custody').exists())

    def test_fixture_rejects_symlink_foreign_continuity_and_foreign_record(self):
        from types import SimpleNamespace
        payload = self.fixture_payload()
        with mock.patch.object(h, 'resolve_fixture_payload', return_value=payload), \
                mock.patch.object(h, 'fixture_source_binding', return_value={}), \
                mock.patch.object(h, 'current_owner', return_value=SimpleNamespace(run_id='fixture-run')):
            self.save.mkdir(parents=True)
            destination = self.save / 'World'
            destination.symlink_to(payload['save_src'] / 'World', target_is_directory=True)
            with self.assertRaises(SystemExit):
                h.install_fixture('profile', 'fixture', False, 'live-debug',
                                  disposable_world='World', disposable_run_id='fixture-run')
            destination.unlink()
            continuity = self.save / '.save-continuity' / 'World'
            continuity.mkdir(parents=True)
            (continuity / 'keep').write_bytes(b'foreign')
            with mock.patch.object(h, 'apply_fixture_save_transforms', return_value=[]):
                with self.assertRaises(FileExistsError):
                    h.install_fixture('profile', 'fixture', False, 'live-debug',
                                      disposable_world='World', disposable_run_id='fixture-run')
            self.assertEqual((continuity / 'keep').read_bytes(), b'foreign')
            (continuity / 'keep').unlink()
            continuity.rmdir()
            custody = self.root / '.saved-world-custody'
            custody.mkdir(exist_ok=True)
            record = custody / (hashlib.sha256(b'World').hexdigest() + '.json')
            record.write_text('foreign record')
            with self.assertRaisesRegex(SystemExit, 'custody already exists'):
                h.install_fixture('profile', 'fixture', False, 'live-debug',
                                  disposable_world='World', disposable_run_id='fixture-run')
            self.assertEqual(record.read_text(), 'foreign record')
            self.assertFalse(destination.exists())

    def test_fixture_without_disposable_purpose_does_not_create_custody(self):
        payload = self.fixture_payload()
        with mock.patch.object(h, 'resolve_fixture_payload', return_value=payload), \
                mock.patch.object(h, 'fixture_source_binding', return_value={}), \
                mock.patch.object(h, 'apply_fixture_save_transforms', return_value=[]):
            h.install_fixture('profile', 'fixture', False, 'live-debug')
        self.assertEqual((self.save / 'World' / 'master.gsav').read_bytes(), b'fixture source')
        self.assertFalse((self.root / '.saved-world-custody').exists())

    def test_fixture_transform_receipt_paths_refer_to_installed_world(self):
        from types import SimpleNamespace
        payload = self.fixture_payload()
        def transform(world, transforms):
            return [{'path': str(world / 'master.gsav'), 'nested': [str(world / 'worldoptions.json')]}]
        with mock.patch.object(h, 'resolve_fixture_payload', return_value=payload), \
                mock.patch.object(h, 'fixture_source_binding', return_value={}), \
                mock.patch.object(h, 'current_owner', return_value=SimpleNamespace(run_id='fixture-run')), \
                mock.patch.object(h, 'apply_fixture_save_transforms', side_effect=transform):
            result = h.install_fixture('profile', 'fixture', False, 'live-debug',
                                       disposable_world='World', disposable_run_id='fixture-run')
        report = result['applied_save_transforms'][0]
        self.assertEqual(report['path'], str(self.save / 'World' / 'master.gsav'))
        self.assertEqual(report['nested'], [str(self.save / 'World' / 'worldoptions.json')])

    def test_fixture_failed_transform_or_race_preserves_source_and_foreign_world(self):
        from types import SimpleNamespace
        payload = self.fixture_payload()
        source = payload['save_src'] / 'World' / 'master.gsav'
        with mock.patch.object(h, 'resolve_fixture_payload', return_value=payload), \
                mock.patch.object(h, 'fixture_source_binding', return_value={}), \
                mock.patch.object(h, 'current_owner', return_value=SimpleNamespace(run_id='fixture-run')):
            with mock.patch.object(h, 'apply_fixture_save_transforms', side_effect=ValueError('bad transform')):
                with self.assertRaisesRegex(ValueError, 'bad transform'):
                    h.install_fixture('profile', 'fixture', False, 'live-debug',
                                      disposable_world='World', disposable_run_id='fixture-run')
            self.assertFalse((self.save / 'World').exists())
            self.assertFalse((self.root / '.saved-world-custody').exists())
            def race(world, transforms):
                foreign = self.save / 'World'
                foreign.mkdir()
                (foreign / 'keep').write_bytes(b'foreign')
                return []
            with mock.patch.object(h, 'apply_fixture_save_transforms', side_effect=race):
                with self.assertRaises(FileExistsError):
                    h.install_fixture('profile', 'fixture', True, 'live-debug',
                                      disposable_world='World', disposable_run_id='fixture-run')
            self.assertEqual((self.save / 'World' / 'keep').read_bytes(), b'foreign')
            self.assertEqual(source.read_bytes(), b'fixture source')
            self.assertEqual(list((self.root / '.saved-world-custody').glob('*.json')), [])

    def test_fixture_staging_parent_removal_failure_rolls_back_and_retries(self):
        from types import SimpleNamespace
        payload = self.fixture_payload()
        original = Path.rmdir
        failed = []
        def fail_once(path):
            if path.name.startswith('.snapshot-') and not failed:
                failed.append(path)
                raise OSError('controlled staging parent failure')
            return original(path)
        with mock.patch.object(h, 'resolve_fixture_payload', return_value=payload), \
                mock.patch.object(h, 'fixture_source_binding', return_value={}), \
                mock.patch.object(h, 'current_owner', return_value=SimpleNamespace(run_id='fixture-run')):
            with mock.patch.object(Path, 'rmdir', fail_once):
                with self.assertRaisesRegex(OSError, 'controlled staging parent failure'):
                    h.install_fixture('profile', 'fixture', False, 'live-debug',
                                      disposable_world='World', disposable_run_id='fixture-run')
            self.assertFalse((self.save / 'World').exists())
            self.assertFalse((self.save / '.save-continuity' / 'World').exists())
            self.assertEqual(list((self.root / '.saved-world-custody').glob('*.json')), [])
            self.assertEqual((payload['save_src'] / 'World' / 'master.gsav').read_bytes(), b'fixture source')
            result = h.install_fixture('profile', 'fixture', False, 'live-debug',
                                       disposable_world='World', disposable_run_id='fixture-run')
            self.assertEqual(result['installed_worlds'], ['World'])
        self.assertEqual(len(failed), 1)
        self.assertFalse(failed[0].exists())

    def test_fixture_staging_failure_preserves_replaced_foreign_destination(self):
        from types import SimpleNamespace
        payload = self.fixture_payload()
        original = Path.rmdir
        changed = []
        def replace_then_fail(path):
            if path.name.startswith('.snapshot-') and not changed:
                destination = self.save / 'World'
                destination.rename(self.save / 'original')
                destination.mkdir()
                (destination / 'keep').write_bytes(b'foreign')
                changed.append(path)
                raise OSError('controlled replaced destination')
            return original(path)
        with mock.patch.object(h, 'resolve_fixture_payload', return_value=payload), \
                mock.patch.object(h, 'fixture_source_binding', return_value={}), \
                mock.patch.object(h, 'current_owner', return_value=SimpleNamespace(run_id='fixture-run')), \
                mock.patch.object(Path, 'rmdir', replace_then_fail):
            with self.assertRaisesRegex(OSError, 'controlled replaced destination'):
                h.install_fixture('profile', 'fixture', False, 'live-debug',
                                  disposable_world='World', disposable_run_id='fixture-run')
        self.assertEqual((self.save / 'World' / 'keep').read_bytes(), b'foreign')
        self.assertTrue((self.save / 'original' / 'master.gsav').exists())
        self.assertEqual(len(list((self.root / '.saved-world-custody').glob('*.json'))), 1)

    def test_start_fixture_flag_reaches_real_install_under_exact_owner(self):
        payload = self.fixture_payload()
        snapshots, snapshot = self.snapshot_with_foreign_metadata()
        sibling = self.root / '.saved-world-custody' / 'sibling.json'
        sibling.parent.mkdir(parents=True)
        sibling.write_text('existing custody')
        source_hash = h.sha256_tree(snapshot)
        install_snapshot = h.install_profile_snapshot
        def checked_snapshot(*args, **kwargs):
            owner = h.current_owner(self.root)
            lock = self.root / '.harness-owner.lock'
            inode, content = lock.stat().st_ino, lock.read_bytes()
            result = install_snapshot(*args, **kwargs)
            self.assertIs(h.current_owner(self.root), owner)
            self.assertEqual((lock.stat().st_ino, lock.read_bytes()), (inode, content))
            self.assertEqual(sibling.read_text(), 'existing custody')
            return result
        args = h.build_parser().parse_args([
            'start', '--profile', 'profile', '--world', 'World', '--fixture', 'fixture',
            '--fixture-profile', 'live-debug', '--profile-snapshot', 'snapshot',
            '--disposable-saved-world', '--harness-run-id', 'fixture-run'])
        class Installed(Exception):
            pass
        def transform(world, transforms):
            self.assertEqual(world.name, 'World')
            (world / 'master.gsav').write_bytes(b'transformed')
            return []
        def stop_after_install(profile):
            self.assertEqual(h.current_owner(self.root).run_id, 'fixture-run')
            raise Installed()
        with mock.patch.object(h, 'load_profile_config', return_value={}), \
                mock.patch.object(h, 'profile_snapshot_root', return_value=snapshots), \
                mock.patch.object(h, 'profile_snapshot_source_binding', return_value={}), \
                mock.patch.object(h, 'install_profile_snapshot', side_effect=checked_snapshot), \
                mock.patch.object(h, 'apply_profile_option_overrides', return_value={}), \
                mock.patch.object(h, 'resolve_fixture_payload', return_value=payload), \
                mock.patch.object(h, 'fixture_source_binding', return_value={}), \
                mock.patch.object(h, 'apply_fixture_save_transforms', side_effect=transform), \
                mock.patch.object(h, 'purge_profile_flexbuffer_cache', side_effect=stop_after_install):
            with self.assertRaises(Installed):
                h.run_startup(args)
        self.assertEqual((self.save / 'World' / 'master.gsav').read_bytes(), b'transformed')
        record = json.loads((self.root / '.saved-world-custody' / (hashlib.sha256(b'World').hexdigest() + '.json')).read_text())
        self.assertEqual((record['world'], record['run_id']), ('World', 'fixture-run'))
        self.assertEqual(h.sha256_tree(snapshot), source_hash)

    def test_start_refuses_existing_fixture_before_profile_setup(self):
        destination = self.save / 'World'
        destination.mkdir(parents=True)
        (destination / 'keep').write_bytes(b'foreign')
        args = h.build_parser().parse_args([
            'start', '--profile', 'profile', '--world', 'World', '--fixture', 'fixture',
            '--profile-snapshot', 'snapshot', '--disposable-saved-world',
            '--replace-existing-worlds', '--harness-run-id', 'fixture-run'])
        with mock.patch.object(h, 'load_profile_config', return_value={}), \
                mock.patch.object(h, 'install_profile_snapshot') as snapshot, \
                mock.patch.object(h, 'apply_profile_option_overrides') as options, \
                mock.patch.object(h, 'install_fixture') as fixture:
            with self.assertRaisesRegex(SystemExit, 'fresh world destination'):
                h.run_startup(args)
        snapshot.assert_not_called()
        options.assert_not_called()
        fixture.assert_not_called()
        self.assertEqual((destination / 'keep').read_bytes(), b'foreign')

    def test_registry_selected_probe_reaches_fixture_producer_with_exact_run(self):
        import scenario_registry_cli_test as registry_tests
        payload = self.fixture_payload()
        scenario = self.base / 'selected.json'
        declaration = registry_tests.ScenarioRegistryCliTest().strict_manifest()
        declaration.update({'name': 'fixture.selected', 'profile': 'profile', 'world': 'World',
                            'fixture': 'fixture', 'fixture_profile': 'live-debug'})
        scenario.write_text(json.dumps(declaration))
        args = h.build_parser().parse_args(['probe', 'fixture.selected', '--profile', 'profile'])
        args.registry_selected_source_path = str(scenario)
        args.registry_selected_source_sha256 = hashlib.sha256(scenario.read_bytes()).hexdigest()
        class Installed(Exception):
            pass
        def stop_after_install(profile):
            self.assertEqual(h.current_owner(self.root).run_id, 'fixture-run')
            raise Installed()
        with mock.patch.object(h, 'registry_authority_transition_run_id', return_value='fixture-run'), \
                mock.patch.object(h, 'detect_executable', return_value=self.base / 'fake-game'), \
                mock.patch.object(h, 'load_profile_config', return_value={}), \
                mock.patch.object(h, 'apply_profile_option_overrides', return_value={}), \
                mock.patch.object(h, 'resolve_fixture_payload', return_value=payload), \
                mock.patch.object(h, 'fixture_source_binding', return_value={}), \
                mock.patch.object(h, 'apply_fixture_save_transforms', return_value=[]), \
                mock.patch.object(h, 'purge_profile_flexbuffer_cache', side_effect=stop_after_install):
            with self.assertRaises(Installed):
                h.run_probe_mode(args)
        record = json.loads(next((self.root / '.saved-world-custody').glob('*.json')).read_text())
        self.assertEqual((record['world'], record['run_id']), ('World', 'fixture-run'))
        self.assertEqual((self.save / 'World' / 'master.gsav').read_bytes(), b'fixture source')

    def test_selected_nondisposable_fixture_does_not_invent_purpose(self):
        import scenario_registry_cli_test as registry_tests
        declaration = registry_tests.ScenarioRegistryCliTest().strict_manifest()
        declaration.update({'name': 'fixture.selected', 'profile': 'profile', 'world': 'World',
                            'fixture': 'fixture', 'fixture_profile': 'live-debug'})
        declaration['runtime_contract']['disposable_copy'] = False
        scenario = self.base / 'selected.json'
        scenario.write_text(json.dumps(declaration))
        args = h.build_parser().parse_args(['probe', 'fixture.selected', '--profile', 'profile'])
        args.registry_selected_source_path = str(scenario)
        args.registry_selected_source_sha256 = hashlib.sha256(scenario.read_bytes()).hexdigest()
        class ReachedStart(Exception):
            pass
        def capture(command, **kwargs):
            self.assertNotIn('--disposable-saved-world', command)
            raise ReachedStart()
        with mock.patch.object(h, 'detect_executable', return_value=self.base / 'fake-game'), \
                mock.patch.object(h, 'run_startup_in_process', side_effect=capture):
            with self.assertRaises(ReachedStart):
                h.run_probe_mode(args)

    def test_registry_snapshot_declared_disposable_reaches_owned_installer(self):
        import scenario_registry_cli_test as registry_tests
        declaration = registry_tests.ScenarioRegistryCliTest().strict_manifest()
        declaration.update({'name': 'snapshot.selected', 'profile': 'profile', 'world': 'World'})
        scenario = self.base / 'selected.json'
        scenario.write_text(json.dumps(declaration))
        args = h.build_parser().parse_args([
            'probe', 'snapshot.selected', '--profile', 'profile',
            '--saved-world-snapshot', str(self.source)])
        args.registry_selected_source_path = str(scenario)
        args.registry_selected_source_sha256 = hashlib.sha256(scenario.read_bytes()).hexdigest()
        class Installed(Exception):
            pass
        def stop_after_install(profile):
            self.assertEqual(h.current_owner(self.root).run_id, 'snapshot-run')
            raise Installed()
        with mock.patch.object(h, 'registry_authority_transition_run_id', return_value='snapshot-run'), \
                mock.patch.object(h, 'detect_executable', return_value=self.base / 'fake-game'), \
                mock.patch.object(h, 'load_profile_config', return_value={}), \
                mock.patch.object(h, 'apply_profile_option_overrides', return_value={}), \
                mock.patch.object(h, 'purge_profile_flexbuffer_cache', side_effect=stop_after_install):
            with self.assertRaises(Installed):
                h.run_probe_mode(args)
        record = json.loads(next((self.root / '.saved-world-custody').glob('*.json')).read_text())
        self.assertEqual((record['world'], record['run_id']), ('World', 'snapshot-run'))
        self.assertEqual(h.sha256_tree(self.source), self.source_hash)
        # This test's owner process is still alive; cleanup must not erase its world.
        with self.assertRaises(h.WritableRootConflict):
            h.cleanup_disposable_saved_world('profile', 'World', 'snapshot-run')
        self.assertTrue((self.save / 'World').is_dir())

    def test_registry_snapshot_without_disposable_declaration_is_not_claimed(self):
        import scenario_registry_cli_test as registry_tests
        declaration = registry_tests.ScenarioRegistryCliTest().strict_manifest()
        declaration.update({'name': 'snapshot.selected', 'profile': 'profile', 'world': 'World'})
        declaration['runtime_contract']['disposable_copy'] = False
        scenario = self.base / 'selected.json'
        scenario.write_text(json.dumps(declaration))
        args = h.build_parser().parse_args([
            'probe', 'snapshot.selected', '--profile', 'profile',
            '--saved-world-snapshot', str(self.source)])
        args.registry_selected_source_path = str(scenario)
        args.registry_selected_source_sha256 = hashlib.sha256(scenario.read_bytes()).hexdigest()
        class ReachedStart(Exception):
            pass
        def capture(command, **kwargs):
            self.assertNotIn('--disposable-saved-world', command)
            raise ReachedStart()
        with mock.patch.object(h, 'detect_executable', return_value=self.base / 'fake-game'), \
                mock.patch.object(h, 'run_startup_in_process', side_effect=capture):
            with self.assertRaises(ReachedStart):
                h.run_probe_mode(args)

    def test_selected_declared_snapshot_continuation_creates_fresh_custody(self):
        import scenario_registry_cli as registry_cli
        import scenario_registry_cli_test as registry_tests
        declaration = registry_tests.ScenarioRegistryCliTest().strict_manifest()
        declaration.update({'name': 'snapshot.selected', 'profile': 'profile', 'world': 'World',
                            'saved_world_snapshot': str(self.source)})
        declaration['steps'] = [{'label': 'bootstrap', 'kind': 'native_semantic_bootstrap'},
                                {'label': 'live', 'kind': 'cockpit_live_session'}]
        declaration['proof_route'] = {key: ['live'] for key in declaration['proof_route']}
        scenario = self.base / 'selected.json'
        scenario.write_text(json.dumps(declaration))
        with mock.patch.object(registry_cli, '_load_selected_scenario', return_value=declaration):
            continuation, snapshot, profile = registry_cli._selected_saved_world_defaults(
                object(), post_relaunch_continuation=False, saved_world_snapshot='',
                profile_override='profile')
        self.assertTrue(continuation)
        args = h.build_parser().parse_args([
            'probe', 'snapshot.selected', '--profile', profile, '--post-relaunch-continuation',
            '--saved-world-snapshot', snapshot])
        args.registry_selected_source_path = str(scenario)
        args.registry_selected_source_sha256 = hashlib.sha256(scenario.read_bytes()).hexdigest()
        class Installed(Exception):
            pass
        def stop_after_install(profile):
            self.assertEqual(h.current_owner(self.root).run_id, 'snapshot-run')
            raise Installed()
        with mock.patch.object(h, 'registry_authority_transition_run_id', return_value='snapshot-run'), \
                mock.patch.object(h, 'detect_executable', return_value=self.base / 'fake-game'), \
                mock.patch.object(h, 'load_profile_config', return_value={}), \
                mock.patch.object(h, 'apply_profile_option_overrides', return_value={}), \
                mock.patch.object(h, 'purge_profile_flexbuffer_cache', side_effect=stop_after_install):
            with self.assertRaises(Installed):
                h.run_probe_mode(args)
        record = json.loads(next((self.root / '.saved-world-custody').glob('*.json')).read_text())
        self.assertEqual((record['world'], record['run_id'], record['source_sha256']),
                         ('World', 'snapshot-run', self.source_hash[0]))
        self.assertEqual(h.sha256_tree(self.source), self.source_hash)

    def test_declared_continuation_does_not_claim_existing_or_other_source(self):
        import scenario_registry_cli_test as registry_tests
        declaration = registry_tests.ScenarioRegistryCliTest().strict_manifest()
        declaration.update({'name': 'snapshot.selected', 'profile': 'profile', 'world': 'World',
                            'saved_world_snapshot': str(self.source)})
        declaration['steps'] = [{'label': 'bootstrap', 'kind': 'native_semantic_bootstrap'},
                                {'label': 'live', 'kind': 'cockpit_live_session'}]
        declaration['proof_route'] = {key: ['live'] for key in declaration['proof_route']}
        scenario = self.base / 'selected.json'
        scenario.write_text(json.dumps(declaration))
        other = self.base / 'other'
        other.mkdir()
        (other / 'master.gsav').write_bytes(b'other')
        for existing, source in [(True, self.source), (False, other)]:
            with self.subTest(existing=existing), \
                    mock.patch.object(h, 'detect_executable', return_value=self.base / 'fake-game'):
                destination = self.save / 'World'
                if existing:
                    destination.mkdir(parents=True)
                    (destination / 'master.gsav').write_bytes(b'original world')
                args = h.build_parser().parse_args([
                    'probe', 'snapshot.selected', '--profile', 'profile', '--post-relaunch-continuation',
                    '--saved-world-snapshot', str(source)])
                args.registry_selected_source_path = str(scenario)
                args.registry_selected_source_sha256 = hashlib.sha256(scenario.read_bytes()).hexdigest()
                class ReachedStart(Exception):
                    pass
                def capture(command, **kwargs):
                    self.assertNotIn('--disposable-saved-world', command)
                    raise ReachedStart()
                with mock.patch.object(h, 'run_startup_in_process', side_effect=capture):
                    with self.assertRaises(ReachedStart):
                        h.run_probe_mode(args)
                if existing:
                    (destination / 'master.gsav').unlink()
                    destination.rmdir()

    def test_registry_disposable_snapshot_reuse_keeps_existing_world_unclaimed(self):
        import scenario_registry_cli_test as registry_tests
        declaration = registry_tests.ScenarioRegistryCliTest().strict_manifest()
        declaration.update({'name': 'snapshot.selected', 'profile': 'profile', 'world': 'World'})
        scenario = self.base / 'selected.json'
        scenario.write_text(json.dumps(declaration))
        destination = self.save / 'World'
        destination.mkdir(parents=True)
        (destination / 'master.gsav').write_bytes(b'original world')
        args = h.build_parser().parse_args([
            'probe', 'snapshot.selected', '--profile', 'profile',
            '--saved-world-snapshot', str(self.source)])
        args.registry_selected_source_path = str(scenario)
        args.registry_selected_source_sha256 = hashlib.sha256(scenario.read_bytes()).hexdigest()
        class ReachedExisting(Exception):
            pass
        with mock.patch.object(h, 'registry_authority_transition_run_id', return_value='snapshot-run'), \
                mock.patch.object(h, 'detect_executable', return_value=self.base / 'fake-game'), \
                mock.patch.object(h, 'load_profile_config', return_value={}), \
                mock.patch.object(h, 'apply_profile_option_overrides', return_value={}), \
                mock.patch.object(h, 'purge_profile_flexbuffer_cache', side_effect=ReachedExisting):
            with self.assertRaises(ReachedExisting):
                h.run_probe_mode(args)
        self.assertEqual(h.sha256_tree(destination), self.source_hash)
        self.assertEqual(h.sha256_tree(self.source), self.source_hash)
        self.assertFalse((self.root / '.saved-world-custody').exists())

    def test_relaunch_snapshot_does_not_inherit_disposable_purpose(self):
        import scenario_registry_cli_test as registry_tests
        declaration = registry_tests.ScenarioRegistryCliTest().strict_manifest()
        declaration.update({'name': 'snapshot.selected', 'profile': 'profile', 'world': 'World'})
        declaration['steps'] = [{'label': 'bootstrap', 'kind': 'native_semantic_bootstrap'},
                                {'label': 'live', 'kind': 'cockpit_live_session'}]
        declaration['proof_route'] = {key: ['live'] for key in declaration['proof_route']}
        scenario = self.base / 'selected.json'
        scenario.write_text(json.dumps(declaration))
        args = h.build_parser().parse_args([
            'probe', 'snapshot.selected', '--profile', 'profile',
            '--saved-world-snapshot', str(self.source), '--post-relaunch-continuation'])
        args.registry_selected_source_path = str(scenario)
        args.registry_selected_source_sha256 = hashlib.sha256(scenario.read_bytes()).hexdigest()
        class ReachedStart(Exception):
            pass
        def capture(command, **kwargs):
            self.assertNotIn('--disposable-saved-world', command)
            raise ReachedStart()
        with mock.patch.object(h, 'detect_executable', return_value=self.base / 'fake-game'), \
                mock.patch.object(h, 'run_startup_in_process', side_effect=capture):
            with self.assertRaises(ReachedStart):
                h.run_probe_mode(args)

    def test_registry_selected_launch_only_handoff_owns_fixture_before_launch(self):
        import scenario_registry_cli_test as registry_tests
        payload = self.fixture_payload()
        snapshots, snapshot = self.snapshot_with_foreign_metadata()
        sibling = self.root / '.saved-world-custody' / 'sibling.json'
        sibling.parent.mkdir(parents=True)
        sibling.write_text('existing custody')
        source_hash = h.sha256_tree(snapshot)
        install_snapshot = h.install_profile_snapshot
        def checked_snapshot(*args, **kwargs):
            owner = h.current_owner(self.root)
            lock = self.root / '.harness-owner.lock'
            inode, content = lock.stat().st_ino, lock.read_bytes()
            result = install_snapshot(*args, **kwargs)
            self.assertIs(h.current_owner(self.root), owner)
            self.assertEqual((lock.stat().st_ino, lock.read_bytes()), (inode, content))
            self.assertEqual(sibling.read_text(), 'existing custody')
            return result
        scenario = self.base / 'selected.json'
        declaration = registry_tests.ScenarioRegistryCliTest().strict_manifest()
        declaration.update({'name': 'fixture.selected', 'profile': 'profile', 'world': 'World',
                            'fixture': 'fixture', 'fixture_profile': 'live-debug',
                            'profile_snapshot': 'snapshot'})
        scenario.write_text(json.dumps(declaration))
        args = h.build_parser().parse_args(['handoff', 'fixture.selected', '--profile', 'profile', '--launch-only'])
        args.registry_selected_source_path = str(scenario)
        args.registry_selected_source_sha256 = hashlib.sha256(scenario.read_bytes()).hexdigest()
        class Installed(BaseException):
            pass
        run_dir = self.root / 'harness_runs' / 'fixture-run'
        plan = h.StartupPlan('profile', str(self.root), 'fake-game', 'continue', 'test',
                             'World', [], 'fixture', str(run_dir))
        def stop_before_game(profile, world, directory, **kwargs):
            self.assertEqual(h.current_owner(self.root).run_id, 'fixture-run')
            self.assertEqual(kwargs['transition_event_run_id'], 'fixture-run')
            binding = json.loads((directory / h.TRANSITION_EVENT_BINDING_FILENAME).read_text())
            self.assertEqual(binding['run_id'], 'fixture-run')
            raise Installed()
        with mock.patch.object(h, 'registry_authority_transition_run_id', return_value='fixture-run'), \
                mock.patch.object(h, 'profile_snapshot_root', return_value=snapshots), \
                mock.patch.object(h, 'profile_snapshot_source_binding', return_value={}), \
                mock.patch.object(h, 'install_profile_snapshot', side_effect=checked_snapshot), \
                mock.patch.object(h, 'apply_profile_option_overrides', return_value={}), \
                mock.patch.object(h, 'resolve_fixture_payload', return_value=payload), \
                mock.patch.object(h, 'fixture_source_binding', return_value={}), \
                mock.patch.object(h, 'apply_fixture_save_transforms', return_value=[]), \
                mock.patch.object(h, 'purge_profile_flexbuffer_cache', return_value={}), \
                mock.patch.object(h, 'build_plan', return_value=plan), \
                mock.patch.object(h, 'load_checkpoint_contract', return_value={
                    'normalized_contract': declaration, 'source': {}, 'validation': {}}), \
                mock.patch.object(h, 'run_checkpoint_contract_preflight', return_value={'accepted': True}), \
                mock.patch.object(h, 'config_dir_for_profile', return_value=self.root / 'config'), \
                mock.patch.object(h, 'launch_game', side_effect=stop_before_game):
            with self.assertRaises(Installed):
                h.run_probe_mode(args, handoff=True)
        record = json.loads((self.root / '.saved-world-custody' / (hashlib.sha256(b'World').hexdigest() + '.json')).read_text())
        self.assertEqual((record['world'], record['run_id']), ('World', 'fixture-run'))
        self.assertEqual(h.sha256_tree(snapshot), source_hash)

    def test_launch_only_refuses_existing_fixture_before_profile_setup(self):
        import scenario_registry_cli_test as registry_tests
        declaration = registry_tests.ScenarioRegistryCliTest().strict_manifest()
        declaration.update({'name': 'fixture.selected', 'profile': 'profile', 'world': 'World',
                            'fixture': 'fixture', 'profile_snapshot': 'snapshot'})
        scenario = self.base / 'selected.json'
        scenario.write_text(json.dumps(declaration))
        destination = self.save / 'World'
        destination.mkdir(parents=True)
        (destination / 'keep').write_bytes(b'foreign')
        args = h.build_parser().parse_args(['handoff', 'fixture.selected', '--profile', 'profile', '--launch-only'])
        args.registry_selected_source_path = str(scenario)
        args.registry_selected_source_sha256 = hashlib.sha256(scenario.read_bytes()).hexdigest()
        with mock.patch.object(h, 'registry_authority_transition_run_id', return_value='fixture-run'), \
                mock.patch.object(h, 'install_profile_snapshot') as snapshot, \
                mock.patch.object(h, 'apply_profile_option_overrides') as options, \
                mock.patch.object(h, 'install_fixture') as fixture:
            with self.assertRaisesRegex(SystemExit, 'fresh world destination'):
                h.run_probe_mode(args, handoff=True)
        snapshot.assert_not_called()
        options.assert_not_called()
        fixture.assert_not_called()
        self.assertEqual((destination / 'keep').read_bytes(), b'foreign')

    def test_public_registry_launch_installs_selected_disposable_fixture(self):
        import scenario_registry_cli as cli
        import scenario_registry_cli_test as registry_tests
        payload = self.fixture_payload()
        helper = registry_tests.ScenarioRegistryCliTest()
        declaration = helper.strict_manifest()
        declaration.update({'profile': 'profile', 'world': 'World', 'fixture': 'fixture',
                            'fixture_profile': 'live-debug'})
        with mock.patch.object(helper, 'strict_manifest', return_value=declaration):
            registry, scenarios, token = helper.issue_selection_token(self.base)
        executable = self.base / 'cataclysm-tiles'
        runtime = {'ok': True, 'schema': 1, 'executable_path': str(executable),
                   'executable_sha256': 'selected-test-binary', 'runtime_source_sha256': 'selected-test-source'}
        reached = []
        def stop_after_install(profile):
            reached.append(h.current_owner(self.root).run_id)
            raise RuntimeError('stopped after fixture install, before game launch')
        with mock.patch.object(h, 'detect_executable', return_value=executable), \
                mock.patch.object(h, 'scenarios_root', return_value=scenarios), \
                mock.patch.object(cli, '_current_source_executable_readiness', return_value={'status': 'ready'}), \
                mock.patch.object(h, 'build_runtime_binding', return_value=runtime), \
                mock.patch.object(h, 'registry_run_binding', return_value={}), \
                mock.patch.object(h, 'load_profile_config', return_value={}), \
                mock.patch.object(h, 'apply_profile_option_overrides', return_value={}), \
                mock.patch.object(h, 'resolve_fixture_payload', return_value=payload), \
                mock.patch.object(h, 'fixture_source_binding', return_value={}), \
                mock.patch.object(h, 'apply_fixture_save_transforms', return_value=[]), \
                mock.patch.object(h, 'purge_profile_flexbuffer_cache', side_effect=stop_after_install), \
                redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            self.assertEqual(cli.main(['--json', '--registry', str(registry), 'registry-launch', token]), 1)
        self.assertEqual(reached, [token])
        record = json.loads(next((self.root / '.saved-world-custody').glob('*.json')).read_text())
        self.assertEqual((record['world'], record['run_id']), ('World', token))

    def test_public_registry_launch_installs_real_r008_fixture_chain(self):
        import scenario_registry_cli as cli
        import scenario_registry_cli_test as registry_tests
        try:
            source = h.resolve_fixture_payload('r008_field_player_145_41_v1', 'live-debug')['save_src'] / 'McWilliams'
            h.zzip_binary()
        except SystemExit:
            self.skipTest('optional R008 fixture or compression helper is unavailable')
        before = h.sha256_tree(source)
        self.assertFalse(before[1])
        helper = registry_tests.ScenarioRegistryCliTest()
        declaration = helper.strict_manifest()
        declaration.update({'profile': 'profile', 'world': 'McWilliams',
                            'fixture': 'r008_field_player_145_41_v1', 'fixture_profile': 'live-debug'})
        with mock.patch.object(helper, 'strict_manifest', return_value=declaration):
            registry, scenarios, token = helper.issue_selection_token(self.base)
        executable = self.base / 'cataclysm-tiles'
        runtime = {'ok': True, 'schema': 1, 'executable_path': str(executable),
                   'executable_sha256': 'selected-test-binary', 'runtime_source_sha256': 'selected-test-source'}
        reached = []
        def stop_after_install(profile):
            reached.append(h.current_owner(self.root).run_id)
            raise RuntimeError('stopped after fixture install, before game launch')
        with mock.patch.object(h, 'detect_executable', return_value=executable), \
                mock.patch.object(h, 'scenarios_root', return_value=scenarios), \
                mock.patch.object(cli, '_current_source_executable_readiness', return_value={'status': 'ready'}), \
                mock.patch.object(h, 'build_runtime_binding', return_value=runtime), \
                mock.patch.object(h, 'registry_run_binding', return_value={}), \
                mock.patch.object(h, 'load_profile_config', return_value={}), \
                mock.patch.object(h, 'apply_profile_option_overrides', return_value={}), \
                mock.patch.object(h, 'purge_profile_flexbuffer_cache', side_effect=stop_after_install), \
                redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            self.assertEqual(cli.main(['--json', '--registry', str(registry), 'registry-launch', token]), 1)
        self.assertEqual(reached, [token])
        record = json.loads(next((self.root / '.saved-world-custody').glob('*.json')).read_text())
        self.assertEqual((record['world'], record['run_id']), ('McWilliams', token))
        self.assertTrue((self.save / 'McWilliams').is_dir())
        self.assertEqual(h.sha256_tree(source), before)

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
