#!/usr/bin/env python3
"""First-run admission and native-session authority for the focused patrol scene."""
import contextlib
import copy
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
from scenario_registry import ManifestValidationError, validate_manifest
from scenario_registry_store import (execute_registry_query, open_registry,
    parse_registry_query_request, rebuild_manifest_projection, reload_selection_token_for_launch)

SCENE = HARNESS / 'scenarios/caol.patrol_sound_focused_mcw.json'
CHARTER = HARNESS / 'charters/caol-patrol-sound-focused.json'
KEY = 'capabilities.caol.patrol_sound'
VALUE = 'native_personal_rule_duty_sound_release_reload'

class PatrolSoundRouteTest(unittest.TestCase):
    def test_first_run_selection_is_exact_and_binding_changes_are_rejected(self):
        declaration = json.loads(SCENE.read_text())
        self.assertEqual(validate_manifest(declaration, path=SCENE)['validation']['status'], 'valid')
        self.assertFalse(declaration['runtime_contract']['setup_only_debug'])
        self.assertTrue(declaration['runtime_contract']['grants_gameplay_proof'])
        self.assertEqual(declaration['fixture'], 'r032_prepared_camp_patrol_v1')
        self.assertEqual(declaration['profile'], 'patrol-sound-exploration-001-luna')
        self.assertEqual(declaration['runtime_contract']['profile'], declaration['profile'])
        self.assertEqual(declaration['runtime_contract']['requirements']['profile'], declaration['profile'])
        request = parse_registry_query_request({'requirements': [
            {'key': KEY, 'op': 'eq', 'value': VALUE, 'minimum_evidence': 'declared'}], 'preferences': []})
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            scenarios = root / 'scenarios'
            scenarios.mkdir()
            scene = scenarios / SCENE.name
            scene.write_bytes(SCENE.read_bytes())
            connection = open_registry(str(root / 'registry.sqlite3'))
            try:
                rebuild_manifest_projection(connection, scenarios)
                result = execute_registry_query(connection, request, drafts_root=root / 'drafts')
                self.assertIsNotNone(result.token_id)
                self.assertEqual(connection.execute('SELECT COUNT(*) FROM verification_history').fetchone()[0], 0)
                selected = reload_selection_token_for_launch(connection, result.token_id)
                self.assertTrue(selected.accepted, selected.reason)
                self.assertEqual(selected.scenario, declaration['name'])
                self.assertEqual(selected.reason, 'first_run_bootstrap')
                scene.write_text(scene.read_text() + '\n')
                self.assertFalse(reload_selection_token_for_launch(connection, result.token_id).accepted)
            finally:
                connection.close()
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            scenarios = root / 'scenarios'
            scenarios.mkdir()
            wrong = copy.deepcopy(declaration)
            wrong['capabilities']['runtime.fixture'] = 'unrelated-fixture'
            (scenarios / SCENE.name).write_text(json.dumps(wrong))
            connection = open_registry(str(root / 'registry.sqlite3'))
            try:
                rebuild_manifest_projection(connection, scenarios)
                denied = execute_registry_query(connection, request, drafts_root=root / 'drafts')
                self.assertIsNone(denied.token_id)
            finally:
                connection.close()
        other = copy.deepcopy(declaration)
        other['name'] = 'caol.unrelated_patrol_sound_mcw'
        with self.assertRaises(ManifestValidationError):
            validate_manifest(other, path=SCENE)

    def test_first_run_registry_launch_issues_focused_wec_without_prior_evidence(self):
        from scenario_registry_cli_test import ScenarioRegistryCliTest
        declaration = json.loads(SCENE.read_text())
        request = parse_registry_query_request({'requirements': [
            {'key': KEY, 'op': 'eq', 'value': VALUE, 'minimum_evidence': 'declared'}], 'preferences': []})
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            scenarios = root / 'scenarios'
            scenarios.mkdir()
            (scenarios / SCENE.name).write_bytes(SCENE.read_bytes())
            registry = root / 'registry.sqlite3'
            connection = open_registry(str(registry))
            try:
                rebuild_manifest_projection(connection, scenarios)
                selected = execute_registry_query(connection, request, drafts_root=root / 'drafts')
                self.assertIsNotNone(selected.token_id)
                self.assertEqual(connection.execute('SELECT COUNT(*) FROM verification_history').fetchone()[0], 0)
            finally:
                connection.close()
            with mock.patch.dict(os.environ, {'OPENCLAW_COCKPIT_BRIDGE_BINDING_ID': 'test-bridge'}):
                result, probe = ScenarioRegistryCliTest().run_registry_launch(registry, scenarios, selected.token_id)
            self.assertEqual(result, 23)
            receipt = json.loads(probe.call_args.args[0].registry_launch_receipt)
            self.assertEqual(receipt['wec_authority']['evidence_class'], 'focused feature proof')
            self.assertEqual(receipt['wec_authority']['run_id'], selected.token_id)
            self.assertEqual(receipt['wec_authority']['source_sha256'], receipt['source_sha256'])
            self.assertFalse(declaration['runtime_contract']['setup_only_debug'])

    def test_actual_live_descriptor_preserves_first_run_authority_and_setup_ceiling(self):
        scene = json.loads(SCENE.read_text())
        charter = json.loads(CHARTER.read_text())
        step = scene['steps'][1]
        for authority, expected_credit, expected_ceiling in (
                ('focused feature proof', True, 'focused'), ('setup support', False, 'setup-only')):
            with self.subTest(authority=authority), tempfile.TemporaryDirectory() as temp:
                run = Path(temp)
                (run / startup_harness.TRANSITION_EVENT_BINDING_FILENAME).write_text(json.dumps({'run_id':'test-run'}))
                service = SimpleNamespace(run_channel=SimpleNamespace(_binding_id='test-binding'))
                with contextlib.redirect_stdout(io.StringIO()), \
                     mock.patch.object(startup_harness, 'semantic_step_trace_start_for_source', return_value=0), \
                     mock.patch.object(startup_harness, 'open_cockpit_game_service', return_value=service) as opened, \
                     mock.patch.object(startup_harness, 'active_playtest_witness_charter', return_value=charter), \
                     mock.patch.object(startup_harness, 'serve_cockpit_live', return_value=0), \
                     mock.patch.object(startup_harness, 'read_completed_cockpit_final', return_value={'state':'finished'}):
                    reports = startup_harness.execute_probe_steps(123, run, [step], profile='test', world='McWilliams',
                        scenario_identity=scene['name'], grants_gameplay_proof=True,
                        registry_authority={'evidence_class': authority})
                descriptor = json.loads((run / (step['label'] + '.cockpit_live_session.json')).read_text())
                self.assertEqual(descriptor['gameplay_credit'], expected_credit)
                self.assertEqual(reports[0]['metadata']['gameplay_credit'], expected_credit)
                self.assertEqual(opened.call_args.kwargs['witness_evidence_ceiling'], expected_ceiling)
                self.assertEqual(descriptor['witness']['outcome'], charter['claim'])

if __name__ == '__main__':
    unittest.main()
