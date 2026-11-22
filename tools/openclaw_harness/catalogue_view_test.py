#!/usr/bin/env python3
"""Discovery and source-bound selection checks without native gameplay."""
import contextlib
import json
import io
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from types import SimpleNamespace
from unittest import mock
import cockpit
import scenario_registry_store as store
import scenario_registry_cli as cli
import startup_harness
from catalogue_view import family_view, saved_camp_opportunity
import scenario_registry_cli_test as cli_fixtures

HERE = Path(__file__).resolve().parent

class CatalogueViewTest(unittest.TestCase):
    def cli(self, *args):
        result = subprocess.run([sys.executable, '-B', str(HERE / 'startup_harness.py'), *args], text=True, capture_output=True, check=True)
        return json.loads(result.stdout)

    def test_curated_library_has_complete_distinct_recipes(self):
        rows = startup_harness.list_scenarios()
        self.assertNotIn("caol.scratch_world", {row["name"] for row in rows})
        repo = HERE.parents[1]
        paths = subprocess.check_output(["git", "ls-files", "tools/openclaw_harness/scenarios/*.json"],
                                        cwd=repo, text=True).splitlines()
        tracked = {declaration["name"] for path in paths
                   for declaration in [json.loads((repo / path).read_bytes())]
                   if declaration.get("catalogue", {}).get("kind") in {"maintained", "diagnostic"}}
        self.assertTrue(tracked)
        curated = [row for row in rows if row["name"] in tracked]
        self.assertCountEqual([row["name"] for row in curated], tracked)
        questions = set()
        for row in curated:
            recipe = row['playtest_recipe']
            with self.subTest(scenario=row['name']):
                for field in ('title', 'question', 'starting_state', 'exercise', 'controls',
                              'batching', 'success', 'failure', 'inconclusive', 'evidence',
                              'finish', 'reuse', 'references'):
                    self.assertTrue(recipe.get(field), field)
                self.assertNotIn(recipe['question'], questions)
                questions.add(recipe['question'])
                for reference in recipe['references']:
                    self.assertTrue((HERE.parents[1] / reference).is_file(), reference)
                declaration = json.loads(Path(row['path']).read_text())
                bootstrap = declaration.get('native_semantic_bootstrap', {})
                self.assertNotIn('world.move', bootstrap.get('required_actions', []))

    def test_default_expansion_and_real_fixture_links(self):
        families = {family['id']: family for family in self.cli('list-scenarios')['families']}
        for name in ('migo-tower-journey', 'human-signal-journeys', 'stalker-pursuit'):
            self.assertIn(name, families)
        self.assertNotIn('mixed-load-controls', families)
        soak = families['natural-play-endurance']
        self.assertEqual(soak['variants'], [])
        self.assertEqual(soak['starting_save'], 'Starting save not yet defined')
        self.assertTrue(soak['recipe'].endswith('/long-running-soak.md'))
        refused = subprocess.run([sys.executable, '-B', str(HERE / 'startup_harness.py'),
                                  'list-scenarios', '--scenario-id', 'natural-play-endurance'],
                                 text=True, capture_output=True)
        self.assertNotEqual(refused.returncode, 0)
        self.assertIn('missing or ambiguous', refused.stderr)
        variant = self.cli('list-scenarios', '--family', 'migo-tower-journey')['families'][0]['variants'][0]
        self.assertEqual(variant['name'], 'migo.r050_tower_remote_initial')
        self.assertEqual(variant['fixture']['name'], 'r050_migo_tower_remote_initial_v1')
        self.assertTrue(variant['fixture']['manifest_present'])
        self.assertEqual(variant['fixture']['qualification'], 'unknown')
        self.assertEqual(variant['scenario_id'], store.manifest_identity(Path(variant['path'])))
        fixtures = self.cli('list-fixtures', '--profile', 'live-debug')['fixtures']
        linked = next(x for x in fixtures if x['fixture'] == variant['fixture']['name'])
        self.assertIn(variant['scenario_id'], [x['scenario_id'] for x in linked['declared_variants']])
        self.assertIn('mixed-load-controls', {x['id'] for x in self.cli('list-scenarios', '--diagnostic')['families']})

    def test_invalid_declaration_is_reported_without_rewriting_it(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'invalid.json'
            raw = json.dumps({'name': 'invalid', 'manifest_version': 1, 'steps': []})
            path.write_text(raw)
            with mock.patch.object(startup_harness, 'scenarios_root', return_value=Path(tmp)):
                row = startup_harness.list_scenarios()[0]
                self.assertEqual(row['scenario_manifest']['validation']['status'], 'invalid')
                self.assertEqual(path.read_text(), raw)
                with self.assertRaises(SystemExit):
                    startup_harness.load_scenario('invalid', source_path=path)

    def test_family_conflict_and_malformed_membership(self):
        m = dict(kind='maintained', family_id='f', family_title='One', family_purpose='Purpose', variant='A')
        with self.assertRaises(ValueError):
            family_view([{'catalogue': m}, {'catalogue': dict(m, family_title='Other')}])
        invalid = family_view([{'name': 'bad', 'catalogue': {'kind': 'maintained'}}], kind='all')
        self.assertEqual(invalid['other_scenarios'][0]['membership'], 'unknown')
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'bad.json'
            path.write_text(json.dumps({'name': 'bad', 'catalogue': {'kind': 'maintained'}}))
            with self.assertRaises(SystemExit):
                startup_harness.load_scenario('bad', source_path=path)

    def test_saved_camp_opportunity_reports_source_and_earliest_expiry(self):
        site = {'schema_version': 12, 'site_id': 'camp:one',
                'camp_decision': {'schema_version': 2, 'state': 'report_awaiting_assessment'},
                'next_routine_dispatch_eligible_minutes': 9100,
                'current_scout_report': {'schema_version': 5, 'revision': 1, 'source_generation': 2,
                    'source_activity_id': 'a', 'application_key': 'k', 'delivered_minutes': 8000,
                    'assessment': {'schema_version': 3, 'threshold_class': 'site_signal',
                                   'site_signal_observed_minutes': 7900, 'site_signal_source_id': 'fire'}}}
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'dimension_data.gsav'
            def write():
                path.write_text('# version 39\n' + json.dumps({'overmapbuffer': {'bandit_live_world': {'schema_version': 7, 'sites': [site]}}}))
            write()
            result = saved_camp_opportunity(path)
            self.assertEqual(result['status'], 'known')
            self.assertEqual(result['sites'][0]['decision'], 'report_awaiting_assessment')
            self.assertEqual(result['sites'][0]['routine_next_eligible_minutes']['value'], 9100)
            self.assertEqual(result['sites'][0]['report']['authorization_expiry_minutes'], 10780)
            first = result['source']['sha256']
            site['current_scout_report']['assessment']['site_signal_source_id'] = ''
            write()
            second = saved_camp_opportunity(path)
            self.assertIsNone(second['sites'][0]['report']['authorization_expiry_minutes'])
            self.assertNotEqual(first, second['source']['sha256'])
            site['camp_decision']['schema_version'] = 99
            write()
            self.assertIsNone(saved_camp_opportunity(path)['sites'][0]['decision'])
            path.write_text('# version 999\n{}')
            self.assertEqual(saved_camp_opportunity(path)['status'], 'unknown')
            path.unlink()
            self.assertEqual(saved_camp_opportunity(path)['status'], 'unknown')

    def test_saved_camp_changed_during_read_is_unknown(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'dimension_data.gsav'
            path.write_text('# version 39\n' + json.dumps({'overmapbuffer': {'bandit_live_world': {'schema_version': 7, 'sites': []}}}))
            import os
            original = os.fstat
            count = 0
            def changed(fd):
                nonlocal count
                count += 1
                result = original(fd)
                if count == 2:
                    return SimpleNamespace(st_dev=result.st_dev, st_ino=result.st_ino, st_size=result.st_size,
                                           st_mtime_ns=result.st_mtime_ns + 1, st_ctime_ns=result.st_ctime_ns)
                return result
            with mock.patch('catalogue_view.os.fstat', side_effect=changed):
                self.assertEqual(saved_camp_opportunity(path)['reason'], 'saved dimension changed during inspection')

    def test_prepared_sources_and_recipe_are_visible_in_real_discovery(self):
        family = self.cli('list-scenarios', '--family', 'prepared-camp-fire-visibility')['families'][0]
        self.assertEqual(len(family['variants']), 2)
        for variant in family['variants']:
            recipe = variant['playtest_recipe']
            self.assertTrue(recipe['starting_state']['saved_world_snapshot'])
            self.assertIn('play controls', ' '.join(recipe['controls']))
            self.assertTrue(recipe['success'])
            exact = self.cli('list-scenarios', '--scenario-id', variant['scenario_id'])['scenarios'][0]
            self.assertEqual(exact['playtest_recipe'], recipe)

    def test_changed_fixture_chain_is_unknown(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            fixture = root / 'p' / 'f'
            world = fixture / 'save' / 'world'
            world.mkdir(parents=True)
            (fixture / 'manifest.json').write_text('{}')
            (world / 'dimension_data.gsav').write_text('# version 39\n' + json.dumps(
                {'overmapbuffer': {'bandit_live_world': {'schema_version': 7, 'sites': []}}}))
            resolved = {'save_src': fixture / 'save', 'source_chain': [('p', 'f')], 'save_transforms': []}
            changed = dict(resolved, save_transforms=[{'kind': 'player_items'}])
            with mock.patch.object(startup_harness, 'fixtures_root', return_value=root), \
                 mock.patch.object(startup_harness, 'resolve_fixture_payload', side_effect=[resolved, changed]):
                result = startup_harness.catalogue_fixture_opportunity('f', 'p', {})
            self.assertEqual(result['status'], 'unknown')
            self.assertEqual(result['reason'], 'fixture source chain changed during inspection')

    def test_missing_fixture_is_unknown_without_retaining_a_broken_scenario(self):
        result = startup_harness.catalogue_fixture_opportunity('nonexistent-fixture', 'nonexistent-profile', {})
        self.assertEqual(result['status'], 'unknown')

    def test_metadata_change_does_not_authorize_old_token(self):
        with tempfile.TemporaryDirectory() as tmp:
            helper = cli_fixtures.ScenarioRegistryCliTest()
            registry, scenarios, token = helper.issue_selection_token(Path(tmp))
            manifest = scenarios / 'cli.json'
            connection = store.open_registry(str(registry))
            try:
                self.assertTrue(store.reload_selection_token_for_launch(connection, token).accepted)
                declaration = json.loads(manifest.read_text())
                declaration['catalogue'] = dict(kind='maintained', family_id='f', family_title='F', family_purpose='P', variant='V')
                manifest.write_text(json.dumps(declaration))
                self.assertFalse(store.reload_selection_token_for_launch(connection, token).accepted)
                store.rebuild_manifest_projection(connection, scenarios)
                self.assertFalse(store.reload_selection_token_for_launch(connection, token).accepted)
            finally:
                connection.close()
            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                status = cli.main(['--json', '--registry', str(registry), 'registry-query', '--query-json',
                                   json.dumps({'requirements': [], 'preferences': []})])
            self.assertEqual(status, 0)
            result = json.loads(out.getvalue())['result']
            candidate = next(x for x in result['candidates'] if x['name'] == declaration['name'])
            self.assertEqual(candidate['catalogue']['family_id'], 'f')
            self.assertNotEqual(result.get('token_id'), token)

    def test_cockpit_select_uses_requested_exact_identity(self):
        candidate = SimpleNamespace(scenario_id='other', facts={}, lifecycle_state='active', explanation={})
        target = SimpleNamespace(scenario_id='wanted', facts={}, lifecycle_state='active', explanation={})
        execution = SimpleNamespace(evaluation=SimpleNamespace(candidates=[candidate, target]), selection_id='selection', selected_scenario_id='wanted')
        conn = mock.MagicMock()
        conn.__enter__.return_value = conn
        conn.execute.return_value.fetchone.return_value = {'fit_reason': 'matched'}
        with mock.patch.object(cockpit, 'open_registry', return_value=conn), mock.patch.object(cockpit, 'execute_registry_query', return_value=execution) as query:
            result = cockpit.CockpitService().call({'action': 'scenario.select', 'scenario_id': 'wanted'})
        self.assertTrue(result['ok'])
        self.assertEqual(query.call_args.kwargs['scenario_id'], 'wanted')
        self.assertEqual(result['result']['selection']['scenario_id'], 'wanted')

    def test_real_cockpit_registry_selection_and_refusal(self):
        with tempfile.TemporaryDirectory() as tmp:
            registry, scenarios, _ = cli_fixtures.ScenarioRegistryCliTest().issue_selection_token(Path(tmp))
            identity = store.manifest_identity(scenarios / 'cli.json')
            query = {'action': 'scenario.select', 'scenario_id': identity,
                     'requirements': [{'key': 'player.injured', 'op': 'eq', 'value': False,
                                       'minimum_evidence': 'declared'}]}
            result = cockpit.CockpitService(str(registry)).call(query)
            self.assertTrue(result['ok'])
            self.assertEqual(result['result']['selection']['scenario_id'], identity)
            query['scenario_id'] = 'missing-identity'
            rejected = cockpit.CockpitService(str(registry)).call(query)
            self.assertFalse(rejected['ok'])
            self.assertIn('missing or ambiguous', rejected['error'])

    def test_browse_variant_selects_same_real_registry_source(self):
        class AnnotatedFixture(cli_fixtures.ScenarioRegistryCliTest):
            def strict_manifest(self):
                declaration = super().strict_manifest()
                declaration['catalogue'] = dict(kind='maintained', family_id='test-family',
                    family_title='Test family', family_purpose='Exercise exact selection', variant='Test start')
                return declaration
        with tempfile.TemporaryDirectory() as tmp:
            registry, scenarios, _ = AnnotatedFixture().issue_selection_token(Path(tmp))
            with mock.patch.object(cockpit.startup_harness, 'scenarios_root', return_value=scenarios):
                service = cockpit.CockpitService(str(registry))
                browse = service.call({'action': 'scenario.browse', 'family': 'test-family'})
                identity = browse['result']['families'][0]['variants'][0]['scenario_id']
                self.assertEqual(identity, store.manifest_identity(scenarios / 'cli.json'))
                result = service.call({'action': 'scenario.select', 'scenario_id': identity,
                    'requirements': [{'key': 'player.injured', 'op': 'eq', 'value': False,
                                      'minimum_evidence': 'declared'}]})
                self.assertTrue(result['ok'])
                self.assertEqual(result['result']['selection']['scenario_id'], identity)

    def test_cockpit_family_browse_and_search(self):
        with tempfile.TemporaryDirectory() as tmp:
            service = cockpit.CockpitService(str(Path(tmp) / 'registry.sqlite3'))
            default = service.call({'action': 'scenario'})
            self.assertTrue(default['ok'])
            families = {row['id']: row for row in default['result']['families']}
            self.assertIn('migo-tower-journey', families)
            self.assertNotIn('mixed-load-controls', families)
            expanded = service.call({'action': 'scenario.browse', 'family': 'migo-tower-journey'})
            variant = expanded['result']['families'][0]['variants'][0]
            self.assertTrue(variant['scenario_id'])
            self.assertEqual(variant['fixture']['qualification'], 'unknown')
            diagnostic = service.call({'action': 'scenario.browse', 'view': 'diagnostic'})
            self.assertIn('mixed-load-controls', {row['id'] for row in diagnostic['result']['families']})
            search = service.call({'action': 'scenario.browse', 'view': 'all', 'search': 'Firelight through open windows'})
            self.assertTrue(search['ok'])
            self.assertEqual(search['result']['families'][0]['variants'][0]['playtest_recipe']['title'],
                             'Firelight through open windows')

    def test_real_discovery_shows_quarantine_stale_and_missing_index(self):
        class Annotated(cli_fixtures.ScenarioRegistryCliTest):
            def strict_manifest(self):
                d = super().strict_manifest()
                d.update({'fixture': 'fixture-a', 'fixture_profile': 'dev-harness',
                          'catalogue': dict(kind='maintained', family_id='family', family_title='Family',
                                            family_purpose='Exercise selection', variant='Active')})
                return d
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            registry, scenarios, _ = Annotated().issue_selection_token(root)
            quarantine_path = scenarios / 'quarantined.json'
            declaration = Annotated().strict_manifest()
            declaration['name'] = 'quarantined'
            declaration['catalogue']['variant'] = 'Quarantined'
            quarantine_path.write_text(json.dumps(declaration))
            fixture_root = root / 'fixture-root'
            (fixture_root / 'fixture-a').mkdir(parents=True)
            (fixture_root / 'fixture-a' / 'manifest.json').write_text('{}')
            connection = store.open_registry(str(registry))
            try:
                store.rebuild_manifest_projection(connection, source_paths=[quarantine_path])
                bad_id = store.manifest_identity(quarantine_path)
                connection.execute('INSERT INTO quarantine_history(manifest_id,route_key,quarantine_kind,details_json) VALUES(?,?,?,?)',
                                   (bad_id, 'test-route', 'contradicted', '{}'))
                with self.assertRaises(store.ScenarioRegistryStoreError):
                    store.execute_registry_query(connection, store.parse_registry_query_request({'requirements': [], 'preferences': []}), scenario_id=bad_id)
            finally:
                connection.close()
            def invoke(*argv):
                output = io.StringIO()
                with mock.patch.object(sys, 'argv', ['startup_harness.py', *argv]), \
                     mock.patch.object(startup_harness, 'scenarios_root', return_value=scenarios), \
                     mock.patch.object(startup_harness, 'profile_fixture_root', return_value=fixture_root), \
                     contextlib.redirect_stdout(output):
                    self.assertEqual(startup_harness.main(), 0)
                return json.loads(output.getvalue())
            default = invoke('list-scenarios', '--registry', str(registry))
            variants = {x['name']: x for x in default['families'][0]['variants']}
            self.assertEqual(variants['quarantined']['registry_status']['availability'], 'Unavailable: quarantined')
            self.assertFalse(variants['quarantined']['registry_status']['token_eligible'])
            self.assertEqual(variants['cli.registry.fixture']['registry_status']['lifecycle'], 'active')
            self.assertTrue(variants['cli.registry.fixture']['registry_status']['token_eligible'])
            expanded = invoke('list-scenarios', '--family', 'family', '--registry', str(registry))
            row = next(x for x in expanded['families'][0]['variants'] if x['name'] == 'quarantined')
            self.assertEqual(row['registry_status']['lifecycle'], 'quarantined')
            self.assertEqual(row['fixture']['qualification'], 'unknown')
            with mock.patch.object(startup_harness, 'scenarios_root', return_value=scenarios), \
                 mock.patch.object(startup_harness, 'profile_fixture_root', return_value=fixture_root):
                service = cockpit.CockpitService(str(registry))
                cockpit_default = service.call({'action': 'scenario'})['result']['families'][0]
                cockpit_row = next(x for x in cockpit_default['variants'] if x['name'] == 'quarantined')
                self.assertEqual(cockpit_row['registry_status']['availability'], 'Unavailable: quarantined')
                cockpit_expanded = service.call({'action': 'scenario.browse', 'family': 'family'})
                self.assertEqual(next(x for x in cockpit_expanded['result']['families'][0]['variants']
                                      if x['name'] == 'quarantined')['registry_status']['lifecycle'], 'quarantined')
            fixtures = invoke('list-fixtures', '--profile', 'dev-harness', '--registry', str(registry))
            self.assertTrue(any(x['registry_status']['availability'] == 'Unavailable: quarantined'
                                for x in fixtures['fixtures'][0]['declared_variants']))
            no_index = root / 'missing' / 'registry.sqlite3'
            unknown = invoke('list-scenarios', '--registry', str(no_index))
            self.assertEqual(unknown['families'][0]['variants'][0]['registry_status']['lifecycle'], 'unknown')
            self.assertEqual(unknown['families'][0]['variants'][0]['registry_status']['availability'],
                             'Registry status unknown: index unavailable')
            self.assertFalse(no_index.exists())
            with mock.patch.object(startup_harness, 'scenarios_root', return_value=scenarios):
                self.assertTrue(cockpit.CockpitService(str(no_index)).call({'action': 'scenario'})['ok'])
            self.assertFalse(no_index.exists())
            declaration['description'] = 'source changed without projection'
            quarantine_path.write_text(json.dumps(declaration))
            stale = invoke('list-scenarios', '--family', 'family', '--registry', str(registry))
            row = next(x for x in stale['families'][0]['variants'] if x['name'] == 'quarantined')
            self.assertEqual(row['registry_status']['lifecycle'], 'unknown')
            self.assertEqual(row['registry_status']['availability'], 'Registry status unknown: source not current in registry')

if __name__ == '__main__':
    unittest.main()
