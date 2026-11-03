#!/usr/bin/env python3
"""Exact owner-reference first-run authority; manufactured setup stays zero-credit."""
import contextlib
import copy
import hashlib
import io
import json
import os
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
            selection = store.execute_registry_query(connection, request, drafts_root=root / 'drafts')
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
                    with mock.patch.object(store, '_current_verified_route', return_value=route):
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

if __name__ == '__main__':
    unittest.main()
