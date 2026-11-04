#!/usr/bin/env python3
"""Exact owner-reference first-run authority; manufactured setup stays zero-credit."""
import contextlib
import copy
import hashlib
import io
import json
import os
import shutil
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

HARNESS = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS))
import startup_harness
import scenario_registry_cli as cli
import scenario_registry_store as store
from scenario_registry import ManifestValidationError, validate_manifest
from scenario_registry_cli import saved_world_continuation_steps

VALUE = 'native_reference_camp_equipment_patrol_physical_signal_journey'

class ReferenceCampRouteTest(unittest.TestCase):
    def scene(self, variant):
        return json.loads((HARNESS / 'scenarios' / f'caol.reference_camp_{variant}_focused_mcw.json').read_text())

    def select(self, root, variant, *, altered=None):
        declaration = self.scene(variant)
        reference = root / 'reference-saves' / Path(declaration['saved_world_snapshot']).parents[1].name / 'save' / 'TestSetup00'
        reference.mkdir(parents=True)
        declaration['saved_world_snapshot'] = str(reference)
        declaration['capabilities']['runtime.reference_save_path'] = str(reference)
        if altered:
            altered(declaration)
        scenarios = root / 'scenarios'
        scenarios.mkdir()
        scene = scenarios / (declaration['name'] + '.json')
        scene.write_text(json.dumps(declaration))
        registry = root / 'registry.sqlite3'
        connection = store.open_registry(str(registry))
        store.rebuild_manifest_projection(connection, scenarios)
        request = store.parse_registry_query_request({'requirements': [
            {'key':'capabilities.caol.reference_camp', 'op':'eq', 'value':VALUE, 'minimum_evidence':'declared'},
            {'key':'runtime.reference_variant', 'op':'eq', 'value':variant, 'minimum_evidence':'declared'}], 'preferences': []})
        original = store.path_sha256
        expected_archive = self.scene(variant)['capabilities']['runtime.reference_archive_sha256']
        expected_tree = self.scene(variant)['capabilities']['runtime.reference_save_tree_sha256']
        def path_hash(path):
            return expected_archive if Path(path).suffix == '.zip' else original(path)
        with mock.patch.object(store, 'path_sha256', side_effect=path_hash), \
             mock.patch.object(startup_harness, 'sha256_tree', return_value=(expected_tree, '')):
            try:
                selection = store.execute_registry_query(connection, request, drafts_root=root / 'drafts')
            except Exception:
                connection.close()
                raise
            return declaration, registry, scenarios, connection, selection

    def test_declarations_use_separate_isolated_save_only_sources(self):
        for variant in ('open', 'closed'):
            with self.subTest(variant=variant):
                x = self.scene(variant)
                source = HARNESS / 'scenarios' / (x['name'] + '.json')
                self.assertEqual(validate_manifest(x, path=source)['validation']['status'], 'valid')
                self.assertTrue(x['runtime_contract']['grants_gameplay_proof'])
                self.assertFalse(x['runtime_contract']['setup_only_debug'])
                self.assertEqual(x['profile'], x['runtime_contract']['profile'])
                self.assertEqual(x['profile'], x['runtime_contract']['requirements']['profile'])
                self.assertNotEqual(x['profile'], 'dev-harness')
                self.assertEqual(x['config_profile'], 'dev-harness')
                self.assertEqual(Path(x['saved_world_snapshot']).name, 'TestSetup00')
                self.assertEqual(x['capabilities']['runtime.reference_save_path'], x['saved_world_snapshot'])
                self.assertEqual(len(saved_world_continuation_steps(x['steps'], x.get('post_relaunch'))), 2)
                unrelated = copy.deepcopy(x)
                unrelated['name'] = 'caol.unrelated_reference_camp'
                with self.assertRaises(ManifestValidationError):
                    validate_manifest(unrelated, path=source)

    def test_first_run_selection_and_snapshot_mismatch_refusal(self):
        for variant in ('open', 'closed'):
            for changed in (False, True):
                with self.subTest(variant=variant, changed=changed), tempfile.TemporaryDirectory() as temp:
                    altered = (lambda x: x['capabilities'].__setitem__('runtime.reference_save_path', '/wrong')) if changed else None
                    if changed:
                        with self.assertRaisesRegex(store.ScenarioRegistryStoreError, 'reference_source_not_authorized'):
                            self.select(Path(temp), variant, altered=altered)
                        continue
                    declaration, registry, scenarios, connection, selection = self.select(Path(temp), variant, altered=altered)
                    try:
                        if changed:
                            self.assertIsNone(selection.token_id)
                        else:
                            self.assertIsNotNone(selection.token_id)
                            self.assertEqual(connection.execute('SELECT COUNT(*) FROM verification_history').fetchone()[0], 0)
                    finally:
                        connection.close()

    def test_real_registry_launch_issues_bound_focused_wec(self):
        from scenario_registry_cli_test import ScenarioRegistryCliTest
        for variant in ('open', 'closed'):
            with self.subTest(variant=variant), tempfile.TemporaryDirectory() as temp:
                declaration, registry, scenarios, connection, selection = self.select(Path(temp), variant)
                try:
                    self.assertIsNotNone(selection.token_id)
                finally:
                    connection.close()
                original = store.path_sha256
                expected_archive = self.scene(variant)['capabilities']['runtime.reference_archive_sha256']
                expected_tree = self.scene(variant)['capabilities']['runtime.reference_save_tree_sha256']
                def path_hash(path):
                    return expected_archive if Path(path).suffix == '.zip' else original(path)
                with mock.patch.object(store, 'path_sha256', side_effect=path_hash), \
                     mock.patch.object(startup_harness, 'sha256_tree', return_value=(expected_tree, '')), \
                     mock.patch.dict(os.environ, {'OPENCLAW_COCKPIT_BRIDGE_BINDING_ID':'test-bridge'}):
                    result, probe = ScenarioRegistryCliTest().run_registry_launch(registry, scenarios, selection.token_id)
                self.assertEqual(result, 23)
                receipt = json.loads(probe.call_args.args[0].registry_launch_receipt)
                self.assertEqual(receipt['wec_authority']['evidence_class'], 'focused feature proof')
                self.assertEqual(receipt['wec_authority']['run_id'], selection.token_id)
                self.assertEqual(receipt['wec_authority']['source_sha256'], receipt['source_sha256'])
                self.assertEqual(probe.call_args.args[0].saved_world_snapshot, declaration['saved_world_snapshot'])
                self.assertEqual(probe.call_args.args[0].fixture, '')

    def test_selection_reload_rejects_changed_reference_contents(self):
        for variant in ('open', 'closed'):
            for changed in ('tree', 'archive'):
                with self.subTest(variant=variant, changed=changed), tempfile.TemporaryDirectory() as temp:
                    declaration, registry, scenarios, connection, selection = self.select(Path(temp), variant)
                    self.assertIsNotNone(selection.token_id)
                    expected_archive = self.scene(variant)['capabilities']['runtime.reference_archive_sha256']
                    expected_tree = self.scene(variant)['capabilities']['runtime.reference_save_tree_sha256']
                    original = store.path_sha256
                    def path_hash(path):
                        if Path(path).suffix == '.zip':
                            return '0' * 64 if changed == 'archive' else expected_archive
                        return original(path)
                    tree = '0' * 64 if changed == 'tree' else expected_tree
                    try:
                        with mock.patch.object(store, 'path_sha256', side_effect=path_hash), \
                             mock.patch.object(startup_harness, 'sha256_tree', return_value=(tree, '')):
                            result = store.reload_selection_token_for_launch(connection, selection.token_id)
                        self.assertFalse(result.accepted)
                        self.assertEqual(result.reason, 'reference_source_not_authorized')
                    finally:
                        connection.close()

    def test_verified_selection_rechecks_reference_at_actual_launch(self):
        for changed in ('tree', 'archive'):
            with self.subTest(changed=changed), tempfile.TemporaryDirectory() as temp:
                root = Path(temp)
                declaration, registry, scenarios, connection, initial = self.select(root, 'open')
                expected_archive = self.scene('open')['capabilities']['runtime.reference_archive_sha256']
                expected_tree = self.scene('open')['capabilities']['runtime.reference_save_tree_sha256']
                original = store.path_sha256
                route = {'route_key': 'verified-reference', 'evidence_state': 'verified',
                         'internal_resolution_state': 'verified', 'details': {},
                         'bindings': ({'verification_id': 'v1', 'resolution': 'compatible'},)}
                request = store.parse_registry_query_request({'requirements': [
                    {'key':'capabilities.caol.reference_camp', 'op':'eq', 'value':VALUE, 'minimum_evidence':'declared'}], 'preferences': []})
                try:
                    manifest_id = connection.execute('SELECT manifest_id FROM manifest_current').fetchone()[0]
                    connection.execute("INSERT INTO report_ingestion_history(report_id, manifest_id, report_path, report_sha256, report_kind, ingestion_status) VALUES('r1', ?, 'report', 'a', 'probe', 'ingested')", (manifest_id,))
                    connection.execute("INSERT INTO verification_history(verification_id, manifest_id, report_id, route_key, binding_fingerprint, outcome_kind, proof_status, details_json) VALUES('v1', ?, 'r1', 'verified-reference', 'b', 'verified', 'green', '{}')", (manifest_id,))
                    def initial_hash(path):
                        return expected_archive if Path(path).suffix == '.zip' else original(path)
                    with mock.patch.object(store, '_current_verified_route', return_value=route), \
                         mock.patch.object(store, 'path_sha256', side_effect=initial_hash), \
                         mock.patch.object(startup_harness, 'sha256_tree', return_value=(expected_tree, '')):
                        selected = store.execute_registry_query(connection, request, drafts_root=root / 'drafts')
                    self.assertIsNotNone(selected.token_id)
                    issued = json.loads(connection.execute("SELECT details_json FROM token_history WHERE token_id=? AND event_kind='issued'", (selected.token_id,)).fetchone()[0])
                    self.assertEqual(issued['authority_kind'], 'query_selection')
                    def path_hash(path):
                        return expected_archive if Path(path).suffix == '.zip' else original(path)
                    with mock.patch.object(store, 'path_sha256', side_effect=path_hash), \
                         mock.patch.object(startup_harness, 'sha256_tree', return_value=(expected_tree, '')), \
                         mock.patch.object(store, '_current_route_evidence', return_value=(route,)), \
                         mock.patch.object(store, '_authoritative_current_route', return_value=route):
                        self.assertTrue(store.reload_selection_token_for_launch(connection, selected.token_id).accepted)
                    def changed_hash(path):
                        return ('0' * 64 if changed == 'archive' else expected_archive) if Path(path).suffix == '.zip' else original(path)
                    with mock.patch.object(store, 'path_sha256', side_effect=changed_hash), \
                         mock.patch.object(startup_harness, 'sha256_tree', return_value=('0' * 64 if changed == 'tree' else expected_tree, '')):
                        rejected = store.reload_selection_token_for_launch(connection, selected.token_id)
                    self.assertFalse(rejected.accepted)
                    self.assertEqual(rejected.reason, 'reference_source_not_authorized')
                    with mock.patch.object(store, 'path_sha256', side_effect=changed_hash), \
                         mock.patch.object(startup_harness, 'sha256_tree', return_value=('0' * 64 if changed == 'tree' else expected_tree, '')), \
                         mock.patch.object(cli, 'reload_selection_token_for_launch', wraps=store.reload_selection_token_for_launch) as reload_token, \
                         mock.patch.object(startup_harness, 'run_probe_mode') as probe, \
                         contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                        result = cli.main(['--json', '--registry', str(registry), 'registry-launch', selected.token_id])
                    self.assertNotEqual(result, 0)
                    reload_token.assert_called_once()
                    probe.assert_not_called()
                finally:
                    connection.close()

    def test_real_launch_rejects_snapshot_override_before_claim(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            declaration, registry, scenarios, connection, selection = self.select(root, 'open')
            connection.close()
            other = root / 'different-snapshot'; other.mkdir()
            expected_archive = self.scene('open')['capabilities']['runtime.reference_archive_sha256']
            expected_tree = self.scene('open')['capabilities']['runtime.reference_save_tree_sha256']
            original = store.path_sha256
            def path_hash(path):
                return expected_archive if Path(path).suffix == '.zip' else original(path)
            executable = root / 'executable'; executable.write_bytes(b'executable')
            binding = {'ok': True, 'schema': 1, 'executable_path': str(executable),
                       'executable_sha256': hashlib.sha256(executable.read_bytes()).hexdigest(), 'runtime_source_sha256': 'test'}
            errors = io.StringIO()
            with mock.patch.object(store, 'path_sha256', side_effect=path_hash), \
                 mock.patch.object(startup_harness, 'sha256_tree', return_value=(expected_tree, '')), \
                 mock.patch.object(startup_harness, 'scenarios_root', return_value=scenarios), \
                 mock.patch.object(startup_harness, 'detect_executable', return_value=executable), \
                 mock.patch.object(startup_harness, 'build_runtime_binding', return_value=binding), \
                 mock.patch.object(cli, '_current_source_executable_readiness', return_value={'status':'ready'}), \
                 mock.patch.dict(os.environ, {'OPENCLAW_COCKPIT_BRIDGE_BINDING_ID':'test-bridge'}), \
                 mock.patch.object(cli, '_selected_saved_world_defaults', wraps=cli._selected_saved_world_defaults) as defaults, \
                 mock.patch.object(cli, '_write_result') as output, \
                 mock.patch.object(startup_harness, 'run_probe_mode') as probe, \
                 contextlib.redirect_stdout(errors), contextlib.redirect_stderr(errors):
                result = cli.main(['--json', '--registry', str(registry), 'registry-launch', selection.token_id,
                                   '--saved-world-snapshot', str(other)])
            self.assertNotEqual(result, 0)
            self.assertTrue(defaults.called, str(output.call_args_list))
            with self.assertRaisesRegex(store.ScenarioRegistryStoreError, 'snapshot override differs from the authorized source'):
                cli._selected_saved_world_defaults(*defaults.call_args.args, **defaults.call_args.kwargs)
            probe.assert_not_called()
            connection = store.open_registry(str(registry))
            try:
                self.assertEqual(connection.execute("SELECT COUNT(*) FROM token_history WHERE token_id=? AND event_kind='verification_run'", (selection.token_id,)).fetchone()[0], 0)
            finally:
                connection.close()

    def test_native_first_run_descriptor_caps_setup_support(self):
        for variant in ('open', 'closed'):
            scene = self.scene(variant)
            charter = json.loads((HARNESS / 'charters' / f'caol-reference-camp-{variant}-focused.json').read_text())
            for authority, credit, ceiling in [('focused feature proof', True, 'focused'), ('setup support', False, 'setup-only')]:
                with self.subTest(variant=variant, authority=authority), tempfile.TemporaryDirectory() as temp:
                    run = Path(temp)
                    (run / startup_harness.TRANSITION_EVENT_BINDING_FILENAME).write_text(json.dumps({'run_id':'test-run'}))
                    service = SimpleNamespace(run_channel=SimpleNamespace(_binding_id='test-binding'))
                    with contextlib.redirect_stdout(io.StringIO()), \
                         mock.patch.object(startup_harness, 'semantic_step_trace_start_for_source', return_value=0), \
                         mock.patch.object(startup_harness, 'open_cockpit_game_service', return_value=service) as opened, \
                         mock.patch.object(startup_harness, 'active_playtest_witness_charter', return_value=charter), \
                         mock.patch.object(startup_harness, 'serve_cockpit_live', return_value=0), \
                         mock.patch.object(startup_harness, 'read_completed_cockpit_final', return_value={'state':'finished'}):
                        reports = startup_harness.execute_probe_steps(123, run, [scene['steps'][1]], profile='test', world='TestSetup00',
                            grants_gameplay_proof=True, registry_authority={'evidence_class':authority})
                    descriptor = json.loads((run / 'observe_reference_camp.cockpit_live_session.json').read_text())
                    self.assertEqual(descriptor['gameplay_credit'], credit)
                    self.assertEqual(reports[0]['metadata']['gameplay_credit'], credit)
                    self.assertEqual(opened.call_args.kwargs['witness_evidence_ceiling'], ceiling)

    def test_nearby_footprint_only_and_real_launch_binding(self):
        from scenario_registry_cli_test import ScenarioRegistryCliTest
        from bandit_live_world_audit import load_special_definitions
        source = Path(self.scene('open')['saved_world_snapshot'])
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            world = root / '.userdata/reference-camp-nearby-initial/save/TestSetup00'
            world.parent.mkdir(parents=True)
            shutil.copytree(source, world)
            seed = {'kind': 'seed_overmap_special_near_player', 'player_save': '#VGVzdDAw.sav.zzip',
                    'special_id': 'bandit_cabin', 'source_special_id': 'bandit_cabin',
                    'site_index': 1, 'offset_omt': [-10, -5, 0]}
            result = startup_harness.apply_seed_overmap_special_near_player_transform(world, seed)
            self.assertEqual(result['target_abs_omt'], [121, 138, 0])
            self.assertEqual(result['previous_target_terrain'], {'121,138,0': 'field', '121,138,1': 'open_air'})
            changed = [p.relative_to(source) for p in source.rglob('*') if p.is_file() and
                       (world / p.relative_to(source)).read_bytes() != p.read_bytes()]
            self.assertEqual(changed, [Path('overmaps/o.0.0.zzip')])
            placements = startup_harness.load_bandit_special_placements(HARNESS.parents[1], world, set(load_special_definitions(HARNESS.parents[1])))
            self.assertEqual(sorted(tuple(p.points[0]) for p in placements if p.special_id == 'bandit_cabin'),
                             [(103, 167, 0), (121, 138, 0)])
            scene = self.scene('nearby')
            scene['saved_world_snapshot'] = str(world)
            scene['capabilities']['runtime.reference_save_path'] = str(world)
            scenarios = root / 'scenarios'; scenarios.mkdir()
            (scenarios / (scene['name'] + '.json')).write_text(json.dumps(scene))
            registry = root / 'registry.sqlite3'; connection = store.open_registry(str(registry))
            store.rebuild_manifest_projection(connection, scenarios)
            request = store.parse_registry_query_request({'requirements': [
                {'key':'runtime.reference_variant', 'op':'eq', 'value':'nearby', 'minimum_evidence':'declared'}], 'preferences': []})
            expected_tree = scene['capabilities']['runtime.reference_save_tree_sha256']
            expected_archive = scene['capabilities']['runtime.reference_archive_sha256']
            original = store.path_sha256
            def path_hash(path):
                return expected_archive if Path(path).suffix == '.zip' else original(path)
            try:
                with mock.patch.object(store, 'repository_root', return_value=root), \
                     mock.patch.object(store, 'path_sha256', side_effect=path_hash), \
                     mock.patch.object(startup_harness, 'sha256_tree', return_value=(expected_tree, '')):
                    selection = store.execute_registry_query(connection, request, drafts_root=root / 'drafts')
                    self.assertIsNotNone(selection.token_id)
                    token = store.reload_selection_token_for_launch(connection, selection.token_id)
                    self.assertTrue(token.accepted)
                    with self.assertRaisesRegex(store.ScenarioRegistryStoreError, 'snapshot override differs'):
                        cli._selected_saved_world_defaults(token, post_relaunch_continuation=False,
                            saved_world_snapshot=str(root / 'other-save'), profile_override='')
                def wrong_archive(path):
                    return '0' * 64 if Path(path).suffix == '.zip' else original(path)
                with mock.patch.object(store, 'repository_root', return_value=root), \
                     mock.patch.object(store, 'path_sha256', side_effect=wrong_archive), \
                     mock.patch.object(startup_harness, 'sha256_tree', return_value=(expected_tree, '')):
                    self.assertEqual(store.reload_selection_token_for_launch(connection, selection.token_id).reason,
                                     'reference_source_not_authorized')
                with mock.patch.object(store, 'repository_root', return_value=root), \
                     mock.patch.object(store, 'path_sha256', side_effect=path_hash), \
                     mock.patch.object(startup_harness, 'sha256_tree', return_value=(expected_tree, '')):
                    selection = store.execute_registry_query(connection, request, drafts_root=root / 'drafts')
                with mock.patch.object(store, 'repository_root', return_value=root), \
                     mock.patch.object(store, 'path_sha256', side_effect=path_hash), \
                     mock.patch.object(startup_harness, 'sha256_tree', return_value=('0' * 64, '')):
                    rejected = store.reload_selection_token_for_launch(connection, selection.token_id)
                    self.assertEqual(rejected.reason, 'reference_source_not_authorized')
                with mock.patch.object(store, 'repository_root', return_value=root), \
                     mock.patch.object(store, 'path_sha256', side_effect=path_hash), \
                     mock.patch.object(startup_harness, 'sha256_tree', return_value=(expected_tree, '')):
                    selection = store.execute_registry_query(connection, request, drafts_root=root / 'drafts')
            finally:
                connection.close()
            with mock.patch.object(store, 'repository_root', return_value=root), \
                 mock.patch.object(store, 'path_sha256', side_effect=path_hash), \
                 mock.patch.object(startup_harness, 'sha256_tree', return_value=(expected_tree, '')), \
                 mock.patch.dict(os.environ, {'OPENCLAW_COCKPIT_BRIDGE_BINDING_ID':'test-bridge'}):
                result, probe = ScenarioRegistryCliTest().run_registry_launch(registry, scenarios, selection.token_id)
            self.assertEqual(result, 23)
            receipt = json.loads(probe.call_args.args[0].registry_launch_receipt)
            self.assertEqual(receipt['wec_authority']['evidence_class'], 'focused feature proof')
            self.assertEqual(probe.call_args.args[0].saved_world_snapshot, str(world))

    def test_gear_saved_start_has_exact_focused_launch_and_rejects_drift(self):
        from scenario_registry_cli_test import ScenarioRegistryCliTest
        scene = self.scene('gear_reload')
        self.assertEqual(validate_manifest(scene, path=HARNESS / 'scenarios' / (scene['name'] + '.json'))['validation']['status'], 'valid')
        self.assertEqual(len(saved_world_continuation_steps(scene['steps'], scene.get('post_relaunch'))), 2)
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            world = root / '.userdata/reference-camp-gear-initial/save/TestSetup00'
            world.mkdir(parents=True)
            scene['saved_world_snapshot'] = str(world)
            scene['capabilities']['runtime.reference_save_path'] = str(world)
            scenarios = root / 'scenarios'; scenarios.mkdir()
            (scenarios / (scene['name'] + '.json')).write_text(json.dumps(scene))
            registry = root / 'registry.sqlite3'; connection = store.open_registry(str(registry))
            store.rebuild_manifest_projection(connection, scenarios)
            request = store.parse_registry_query_request({'requirements': [
                {'key':'runtime.reference_variant', 'op':'eq', 'value':'gear_reload', 'minimum_evidence':'declared'}], 'preferences': []})
            expected_tree = scene['capabilities']['runtime.reference_save_tree_sha256']
            expected_archive = scene['capabilities']['runtime.reference_archive_sha256']
            original = store.path_sha256
            def path_hash(path):
                return expected_archive if Path(path).suffix == '.zip' else original(path)
            try:
                with mock.patch.object(store, 'repository_root', return_value=root), \
                     mock.patch.object(store, 'path_sha256', side_effect=path_hash), \
                     mock.patch.object(startup_harness, 'sha256_tree', return_value=(expected_tree, '')):
                    selected = store.execute_registry_query(connection, request, drafts_root=root / 'drafts')
                    self.assertIsNotNone(selected.token_id)
                    token = store.reload_selection_token_for_launch(connection, selected.token_id)
                    self.assertTrue(token.accepted)
                    with self.assertRaisesRegex(store.ScenarioRegistryStoreError, 'snapshot override differs'):
                        cli._selected_saved_world_defaults(token, post_relaunch_continuation=False,
                            saved_world_snapshot=str(root / 'other-save'), profile_override='')
                with mock.patch.object(store, 'repository_root', return_value=root), \
                     mock.patch.object(store, 'path_sha256', side_effect=path_hash), \
                     mock.patch.object(startup_harness, 'sha256_tree', return_value=('0' * 64, '')):
                    self.assertEqual(store.reload_selection_token_for_launch(connection, selected.token_id).reason,
                                     'reference_source_not_authorized')
                with mock.patch.object(store, 'repository_root', return_value=root), \
                     mock.patch.object(store, 'path_sha256', side_effect=path_hash), \
                     mock.patch.object(startup_harness, 'sha256_tree', return_value=(expected_tree, '')):
                    selected = store.execute_registry_query(connection, request, drafts_root=root / 'drafts')
            finally:
                connection.close()
            with mock.patch.object(store, 'repository_root', return_value=root), \
                 mock.patch.object(store, 'path_sha256', side_effect=path_hash), \
                 mock.patch.object(startup_harness, 'sha256_tree', return_value=(expected_tree, '')), \
                 mock.patch.dict(os.environ, {'OPENCLAW_COCKPIT_BRIDGE_BINDING_ID':'test-bridge'}):
                result, probe = ScenarioRegistryCliTest().run_registry_launch(registry, scenarios, selected.token_id)
            self.assertEqual(result, 23)
            receipt = json.loads(probe.call_args.args[0].registry_launch_receipt)
            self.assertEqual(receipt['wec_authority']['evidence_class'], 'focused feature proof')
            self.assertEqual(probe.call_args.args[0].saved_world_snapshot, str(world))

    def test_reference_query_rejects_drift_even_with_verified_or_coordinator_authority(self):
        for route_kind in ('verified', 'coordinator'):
            for drift in ('tree', 'archive'):
                with self.subTest(route=route_kind, drift=drift), tempfile.TemporaryDirectory() as temp:
                    root = Path(temp)
                    declaration, registry, scenarios, connection, first = self.select(root, 'open')
                    request = store.parse_registry_query_request({'requirements': [
                        {'key':'runtime.reference_variant', 'op':'eq', 'value':'open', 'minimum_evidence':'declared'}], 'preferences': []})
                    expected_archive = declaration['capabilities']['runtime.reference_archive_sha256']
                    expected_tree = declaration['capabilities']['runtime.reference_save_tree_sha256']
                    original = store.path_sha256
                    def path_hash(path):
                        return ('0' * 64 if drift == 'archive' else expected_archive) if Path(path).suffix == '.zip' else original(path)
                    route = {'route_key':'verified', 'evidence_state':'verified', 'bindings': (), 'details': {}}
                    before = connection.execute("SELECT COUNT(*) FROM token_history WHERE event_kind='issued'").fetchone()[0]
                    try:
                        with mock.patch.object(store, 'path_sha256', side_effect=path_hash), \
                             mock.patch.object(startup_harness, 'sha256_tree', return_value=('0' * 64 if drift == 'tree' else expected_tree, '')), \
                             mock.patch.object(store, '_current_verified_route', return_value=route if route_kind == 'verified' else None), \
                             mock.patch.object(store, '_coordinator_authorization', return_value={'outcome':'test'}):
                            with self.assertRaisesRegex(store.ScenarioRegistryStoreError, 'reference_source_not_authorized'):
                                store.execute_registry_query(connection, request, drafts_root=root / 'drafts',
                                    coordinator_brief={'outcome':'test'} if route_kind == 'coordinator' else None)
                        self.assertEqual(connection.execute("SELECT COUNT(*) FROM token_history WHERE event_kind='issued'").fetchone()[0], before)
                    finally:
                        connection.close()

    def test_reference_bootstrap_and_repair_issuance_reload_and_cli_refuse_drift(self):
        for authority in ('bootstrap', 'repair'):
            for drift in ('tree', 'archive'):
                with self.subTest(authority=authority, drift=drift), tempfile.TemporaryDirectory() as temp:
                    root = Path(temp)
                    declaration, registry, scenarios, connection, first = self.select(root, 'open')
                    request = store.parse_registry_query_request({'requirements': [
                        {'key':'runtime.reference_variant', 'op':'eq', 'value':'open', 'minimum_evidence':'declared'}], 'preferences': []})
                    expected_archive = declaration['capabilities']['runtime.reference_archive_sha256']
                    expected_tree = declaration['capabilities']['runtime.reference_save_tree_sha256']
                    original = store.path_sha256
                    def good_hash(path):
                        return expected_archive if Path(path).suffix == '.zip' else original(path)
                    def bad_hash(path):
                        return ('0' * 64 if drift == 'archive' else expected_archive) if Path(path).suffix == '.zip' else original(path)
                    runtime = {'schema':1, 'executable_path':str(root / 'game'), 'executable_sha256':'a'*64, 'runtime_source_sha256':'runtime'}
                    binding = {'runtime': runtime, 'fixture': {'name':'','profile':'','source_path':'','source_sha256':'a'*64},
                               'profile': {'name':'','profile':'','source_path':'','source_sha256':'b'*64}}
                    manifest_id = connection.execute('SELECT manifest_id FROM manifest_current').fetchone()[0]
                    if authority == 'repair':
                        connection.execute("INSERT INTO report_ingestion_history(report_id, manifest_id, report_path, report_sha256, report_kind, ingestion_status) VALUES('red-report', ?, 'report', 'a', 'probe', 'ingested')", (manifest_id,))
                        connection.execute("INSERT INTO verification_history(verification_id, manifest_id, report_id, route_key, binding_fingerprint, outcome_kind, proof_status, details_json) VALUES('red-v', ?, 'red-report', 'red-route', 'b', 'red', 'red', '{}')", (manifest_id,))
                    def issue():
                        if authority == 'bootstrap':
                            return store.issue_registry_bootstrap_token(connection, request, runtime_binding=runtime)
                        return store.issue_registry_repair_token(connection, request, manifest_id=manifest_id,
                            route_key='red-route', red_verification_id='red-v', binding=binding)
                    def reload(token):
                        return (store.reload_bootstrap_token_for_launch(connection, token) if authority == 'bootstrap'
                                else store.reload_repair_token_for_launch(connection, token, binding=binding))
                    try:
                        with mock.patch.object(store, '_repair_route_current', return_value=True), \
                             mock.patch.object(store, '_repair_query_matches_manifest', return_value=True), \
                             mock.patch.object(store, 'path_sha256', side_effect=bad_hash), \
                             mock.patch.object(startup_harness, 'sha256_tree', return_value=('0'*64 if drift == 'tree' else expected_tree, '')):
                            self.assertEqual(issue().reason, 'reference_source_not_authorized')
                        with mock.patch.object(store, '_repair_route_current', return_value=True), \
                             mock.patch.object(store, '_repair_query_matches_manifest', return_value=True), \
                             mock.patch.object(store, 'path_sha256', side_effect=good_hash), \
                             mock.patch.object(startup_harness, 'sha256_tree', return_value=(expected_tree, '')):
                            issued = issue()
                            self.assertTrue(issued.accepted, issued.reason)
                            self.assertTrue(reload(issued.token_id).accepted)
                        with mock.patch.object(store, '_repair_route_current', return_value=True), \
                             mock.patch.object(store, '_repair_query_matches_manifest', return_value=True), \
                             mock.patch.object(store, 'path_sha256', side_effect=bad_hash), \
                             mock.patch.object(startup_harness, 'sha256_tree', return_value=('0'*64 if drift == 'tree' else expected_tree, '')):
                            self.assertEqual(reload(issued.token_id).reason, 'reference_source_not_authorized')
                        with mock.patch.object(store, '_repair_route_current', return_value=True), \
                             mock.patch.object(store, '_repair_query_matches_manifest', return_value=True), \
                             mock.patch.object(store, 'path_sha256', side_effect=good_hash), \
                             mock.patch.object(startup_harness, 'sha256_tree', return_value=(expected_tree, '')):
                            issued = issue()
                            self.assertTrue(issued.accepted, issued.reason)
                        seen = []
                        def check(*args, **kwargs):
                            result = (store.reload_bootstrap_token_for_launch(*args, **kwargs) if authority == 'bootstrap'
                                      else store.reload_repair_token_for_launch(*args, **kwargs))
                            seen.append(result)
                            return result
                        command = 'registry-bootstrap-launch' if authority == 'bootstrap' else 'registry-repair-launch'
                        reload_name = 'reload_bootstrap_token_for_launch' if authority == 'bootstrap' else 'reload_repair_token_for_launch'
                        with mock.patch.object(store, '_repair_route_current', return_value=True), \
                             mock.patch.object(store, '_repair_query_matches_manifest', return_value=True), \
                             mock.patch.object(store, 'path_sha256', side_effect=bad_hash), \
                             mock.patch.object(startup_harness, 'sha256_tree', return_value=('0'*64 if drift == 'tree' else expected_tree, '')), \
                             mock.patch.object(cli, '_current_repair_binding', return_value=binding), \
                             mock.patch.object(cli, reload_name, side_effect=check), \
                             mock.patch.object(startup_harness, 'run_probe_mode') as probe, \
                             contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                            status = cli.main(['--json','--registry',str(registry),command,issued.token_id])
                        self.assertNotEqual(status, 0)
                        self.assertTrue(seen)
                        self.assertEqual(seen[0].reason, 'reference_source_not_authorized')
                        probe.assert_not_called()
                        with mock.patch.object(store, '_repair_route_current', return_value=True), \
                             mock.patch.object(store, '_repair_query_matches_manifest', return_value=True), \
                             mock.patch.object(store, 'path_sha256', side_effect=good_hash), \
                             mock.patch.object(startup_harness, 'sha256_tree', return_value=(expected_tree, '')):
                            issued = issue()
                            self.assertTrue(issued.accepted, issued.reason)
                        with mock.patch.object(store, '_repair_route_current', return_value=True), \
                             mock.patch.object(store, '_repair_query_matches_manifest', return_value=True), \
                             mock.patch.object(store, 'path_sha256', side_effect=good_hash), \
                             mock.patch.object(startup_harness, 'sha256_tree', return_value=(expected_tree, '')), \
                             mock.patch.object(startup_harness, 'scenarios_root', return_value=scenarios), \
                             mock.patch.object(startup_harness, 'save_dir_for_profile', return_value=root / 'fresh-profile'), \
                             mock.patch.object(startup_harness, 'compare_runtime_binding', return_value={'status':'matched'}), \
                             mock.patch.object(cli, '_current_repair_binding', return_value=binding), \
                             mock.patch.dict(os.environ, {'OPENCLAW_COCKPIT_BRIDGE_BINDING_ID':'test-bridge'}), \
                             mock.patch.object(startup_harness, 'run_probe_mode', return_value=31) as probe, \
                             contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                            status = cli.main(['--json','--registry',str(registry),command,issued.token_id])
                        self.assertEqual(status, 31)
                        probe.assert_called_once()
                        self.assertEqual(Path(probe.call_args.args[0].saved_world_snapshot).resolve(),
                                         Path(declaration['saved_world_snapshot']).resolve())
                    finally:
                        connection.close()

if __name__ == '__main__':
    unittest.main()
