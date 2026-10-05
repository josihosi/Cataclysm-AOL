"""R067 saved handoff and recursive evidence controls; never launch a game."""
import argparse
import copy
import gzip
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

import cockpit_evidence as evidence
import play_cli
import scenario_registry_cli as cli
import startup_harness as harness

ROOT = Path(__file__).resolve().parents[2]
TEMPLATE = ROOT / 'build_logs/first-smoke-067/camp-npc-adjacent-omt-r051-20261004/revision-2/registry-scenarios/bandit.r_caol_first_smoke_051_r067_camp_npc_receiver_adjacent_omt_8820_20261004.json'


class HarnessFollowupTest(unittest.TestCase):
    def selection(self, root, snapshot=True):
        scenario = json.loads(TEMPLATE.read_text())
        scenario['name'] = 'r067.followup'
        scenario['profile'] = str(root / 'profile')
        if snapshot:
            scenario['saved_world_snapshot'] = str(root / 'snapshot')
            (root / 'snapshot').mkdir()
            (root / 'snapshot' / 'master.gsav').write_text('exact preserved source')
        path = root / 'r067.followup.json'
        path.write_text(json.dumps(scenario))
        selection = cli.RegistryBootstrapToken('token', True, 'current', 'r067.followup', str(path), {},
                                               hashlib.sha256(path.read_bytes()).hexdigest())
        return selection, scenario, path

    def test_selected_declaration_supplies_snapshot_profile_and_continuation(self):
        with tempfile.TemporaryDirectory() as tmp:
            selection, scenario, path = self.selection(Path(tmp))
            args = argparse.Namespace()
            cli._preflight_selected_save_setup(selection, args)
            for adapter in (cli._registry_launch_probe_namespace, cli._registry_bootstrap_probe_namespace,
                            cli._registry_repair_probe_namespace):
                namespace = adapter(selection)
                self.assertEqual(namespace.saved_world_snapshot, scenario['saved_world_snapshot'])
                self.assertEqual(namespace.profile, scenario['profile'])
                self.assertTrue(namespace.post_relaunch_continuation)
                self.assertEqual(namespace.registry_selected_source_sha256, selection.source_sha256)
            path.write_text(path.read_text() + ' ')
            with self.assertRaises(cli.ScenarioRegistryStoreError):
                cli._preflight_selected_save_setup(selection, args)

    def test_explicit_snapshot_and_profile_override_selected_defaults(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            selection, _, _ = self.selection(root)
            explicit = root / 'explicit'
            explicit.mkdir()
            args = argparse.Namespace(saved_world_snapshot=str(explicit), profile=str(root / 'other'))
            cli._preflight_selected_save_setup(selection, args)
            self.assertEqual(args.saved_world_snapshot, str(explicit.resolve()))
            self.assertEqual(args.profile, str(root / 'other'))

    def test_missing_handoff_refuses_before_bridge_or_token_claim(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            selection, _, _ = self.selection(root, snapshot=False)
            args = argparse.Namespace(command='registry-bootstrap-detached-launch', bootstrap_token='token',
                                      session_dir=str(root / 'session'), post_relaunch_continuation=True)
            with mock.patch.object(cli, 'open_registry', return_value=mock.Mock()), \
                 mock.patch.object(cli, 'reload_bootstrap_token_for_launch', return_value=selection), \
                 mock.patch.object(harness, 'compare_runtime_binding', return_value={'status': 'matched'}), \
                 mock.patch.object(cli, 'subprocess') as processes, \
                 mock.patch.object(cli, 'claim_bootstrap_token_for_launch') as claim, \
                 mock.patch.object(cli, '_write_result') as output:
                self.assertEqual(cli._launch_bootstrap_file_bridge(args, root / 'registry.sqlite3'), 1)
                self.assertIn('saved_world_snapshot', output.call_args.args[0]['error'])
                processes.run.assert_not_called()
                claim.assert_not_called()
                self.assertFalse((root / 'session').exists())

    def test_populated_profile_refuses_different_bytes_and_reuses_exact_copy(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            selection, scenario, _ = self.selection(root)
            save_root = root / 'profile-save'
            destination = save_root / scenario['world']
            destination.mkdir(parents=True)
            master = destination / 'master.gsav'
            master.write_text('unsaved current contents')
            with mock.patch.object(harness, 'save_dir_for_profile', return_value=save_root):
                with self.assertRaisesRegex(cli.ScenarioRegistryStoreError, 'different data'):
                    cli._preflight_selected_save_setup(selection, argparse.Namespace())
                with self.assertRaises(SystemExit):
                    harness.install_saved_world_snapshot(scenario['profile'], scenario['world'], root / 'snapshot')
                self.assertEqual(master.read_text(), 'unsaved current contents')
                result = harness.install_saved_world_snapshot(scenario['profile'], scenario['world'],
                                                               root / 'snapshot', replace=True)
                self.assertEqual(result['source_sha256'], result['installed_sha256'])
                stat = master.stat()
                cli._preflight_selected_save_setup(selection, argparse.Namespace())
                result = harness.install_saved_world_snapshot(scenario['profile'], scenario['world'], root / 'snapshot')
                self.assertEqual(result['status'], 'green_saved_world_snapshot_reused')
                self.assertEqual(master.stat().st_ino, stat.st_ino)
                self.assertEqual(master.stat().st_mtime_ns, stat.st_mtime_ns)

    def test_recursive_worn_pockets_unknowns_and_pointers(self):
        value = {'actors': [{'id': 4, 'name': 'Mason', 'inv': [], 'worn': [
            {'typeid': 'pants', 'uid': 93, 'contents': {'contents': [
                {'typeid': 'coin_gold', 'uid': 101, 'owner': 'hells_raiders'}]}}]}]}
        before = copy.deepcopy(value)
        result = evidence.recursive_view(value, 'actors', 'items', contains='coin_gold', source_role='final')
        self.assertEqual(result['matched_rows'], 1)
        row = result['rows'][0]
        self.assertEqual(row['json_pointer'], '/0/worn/0/contents/contents/0')
        self.assertEqual(row['actor']['id'], 4)
        self.assertEqual(row['containers'][0]['uid'], 93)
        self.assertFalse(row['fields']['charges']['available'])
        self.assertEqual(result['source_role_provenance'], 'caller_label_not_inferred_from_path_or_contents')
        self.assertEqual(value, before)
        self.assertFalse(evidence.recursive_view(value, 'missing', 'items')['available'])
        actor = evidence.recursive_view(value, 'actors', 'actors')['rows'][0]
        self.assertFalse(actor['fields']['sleep']['available'])

    def test_actual_167_133_custody_rows_without_old_to_new_uid_mapping(self):
        fixture = Path(__file__).parent / 'fixtures/controls/r067_coin_custody.json.gz'
        value = json.loads(gzip.decompress(fixture.read_bytes()))
        for group, count in [('o.0.0', 167), ('#VGVzdDAw.sav', 133)]:
            rows = value['saved_coin_rows'][group]
            view = evidence.recursive_view(rows, '', 'items', contains='coin_gold', limit=500)
            self.assertEqual(view['matched_rows'], count)
            self.assertEqual({row['fields']['uid']['value'] for row in view['rows']}, {row['uid'] for row in rows})
            self.assertEqual({row['retained_source_json_pointer'] for row in view['rows']},
                             {row['json_pointer'] for row in rows})
        self.assertEqual(value['native_advertised_coin_ids_count'], 300)
        self.assertTrue(value['native_remaining_complement_matches_saved_player'])

    def test_json_string_decode_pagination_and_monster_not_item(self):
        value = {'facts': {'items': json.dumps([{'typeid': 'rock', 'uid': i} for i in range(4)]),
                           'monster': {'type_id': 'mon_zombie'}}}
        result = evidence.recursive_view(value, 'facts', 'items', offset=1, limit=2)
        self.assertEqual(result['matched_rows'], 4)
        self.assertEqual(result['next_offset'], 3)
        self.assertEqual([row['fields']['uid']['value'] for row in result['rows']], [1, 2])
        self.assertFalse(evidence.recursive_view(value, '', 'items', limit=0)['ok'])

    def test_recursive_inspect_authenticates_hash_binding_without_input(self):
        client = play_cli.PlayerClient.__new__(play_cli.PlayerClient)
        client.state = {'last_request_id': 'retained'}
        client.binding = 'bound'
        client.session = Path('/not-a-game')
        receipt = {'ok': True, 'receipt': {'binding_id': 'bound', 'response_sha256': 'exact'}}
        response = {'observation': {'surface': {'facts': {'npc': json.dumps(
            {'npc_id': 4, 'name': 'Mason', 'dead': False})}}}}
        with mock.patch.object(play_cli.Bridge, 'response_status', return_value=receipt), \
             mock.patch.object(play_cli.Bridge, 'response_artifact', return_value={'ok': True, 'response': response}) as artifact, \
             mock.patch.object(client, 'submit') as submit:
            result = client.inspect('observation.surface.facts.npc', 0, None, None, view='actors')
            self.assertEqual(result['projection']['rows'][0]['fields']['id']['value'], 4)
            artifact.assert_called_once_with(client.session, 'retained', 'exact')
            invalid = client.inspect('', 0, 0, None, view='items')
            self.assertFalse(invalid['ok'])
            self.assertEqual(invalid['projection']['error'], 'invalid_recursive_view_arguments')
            submit.assert_not_called()
        receipt['receipt']['binding_id'] = 'foreign'
        with mock.patch.object(play_cli.Bridge, 'response_status', return_value=receipt), \
             mock.patch.object(play_cli.Bridge, 'response_artifact') as artifact:
            self.assertEqual(client.inspect('', 0, None, None, view='items')['error'], 'response_binding_mismatch')
            artifact.assert_not_called()


class SurfaceRelativeInspectTest(unittest.TestCase):
    def test_surface_alias_terminal_and_explicit_raw_paths(self):
        for envelope, raw in [('result', 'result.surface.facts'),
                              ('observation', 'observation.surface.facts'),
                              ('result.terminal_observation', 'result.terminal_observation.surface.facts')]:
            value = {'surface': {'facts': {'empty': [], 'false': False, 'items': json.dumps([{'typeid': 'coin', 'uid': 7}])}}}
            for key in reversed(envelope.split('.')):
                value = {key: value}
            before = copy.deepcopy(value)
            alias = evidence.selected_view(value, ['surface.facts.false', 'surface.facts.missing'])['selectors']
            self.assertEqual(alias['surface.facts.false']['value'], False)
            self.assertEqual(alias['surface.facts.false']['resolved_selector'], raw + '.false')
            self.assertFalse(alias['surface.facts.missing']['available'])
            self.assertTrue(evidence.select_optional(value, raw + '.empty')['available'])
            view = evidence.recursive_view(value, 'surface.facts.items', 'items')
            self.assertEqual(view['resolved_selector'], raw + '.items')
            self.assertEqual(view['rows'][0]['fields']['uid']['value'], 7)
            self.assertEqual(value, before)

    def test_site_scoped_inspect_uses_exact_site_and_retained_response_authority(self):
        site = 'overmap_special:bandit_camp@129,149,0'
        selector = 'surface.facts.site_outing_owners.by_site.' + site
        catalog = {'by_site': {
            'overmap_special:bandit_cabin@103,167,0': {'outing': {'member_ids': [7, 8]}},
            site: {'site_id': site, 'outing': {'member_ids': [4, 5]},
                   'contact': {'present': False}}}}
        client = play_cli.PlayerClient.__new__(play_cli.PlayerClient)
        client.state = {'last_request_id': 'retained'}
        client.binding = 'bound'
        client.session = Path('/not-a-game')
        receipt = {'ok': True, 'receipt': {'binding_id': 'bound', 'response_sha256': 'exact'}}
        for envelope in ('result', 'observation'):
            value = {envelope: {'surface': {'facts': {'site_outing_owners': json.dumps(catalog)}}}}
            with mock.patch.object(play_cli.Bridge, 'response_status', return_value=receipt), \
                 mock.patch.object(play_cli.Bridge, 'response_artifact', return_value={'ok': True, 'response': value}) as artifact, \
                 mock.patch.object(client, 'submit') as submit:
                result = client.inspect(selector, 0, None, None,
                                        selected_fields=['site_id', 'outing.member_ids', 'contact.present'])
                rows = result['projection']['selectors']
                self.assertEqual(rows[selector + '.outing.member_ids']['value'], [4, 5])
                self.assertEqual(rows[selector + '.site_id']['resolved_selector'],
                                 envelope + '.' + selector + '.site_id')
                self.assertIs(rows[selector + '.contact.present']['value'], False)
                missing = client.inspect('surface.facts.site_outing_owners.by_site.unknown',
                                         0, None, None, selected_fields=['site_id'])
                self.assertFalse(next(iter(missing['projection']['selectors'].values()))['available'])
                artifact.assert_called_with(client.session, 'retained', 'exact')
                submit.assert_not_called()
        receipt['receipt']['binding_id'] = 'foreign'
        with mock.patch.object(play_cli.Bridge, 'response_status', return_value=receipt), \
             mock.patch.object(play_cli.Bridge, 'response_artifact') as artifact:
            self.assertEqual(client.inspect(selector, 0, None, None, selected_fields=['site_id'])['error'],
                             'response_binding_mismatch')
            artifact.assert_not_called()

    def test_absent_ambiguous_and_explicit_raw_selection(self):
        self.assertEqual(evidence.select_optional({'result': {}}, 'surface.facts')['reason'], 'surface_unavailable')
        value = {'result': {'surface': {'facts': {'balance': 1}}},
                 'observation': {'surface': {'facts': {'balance': 2}}}}
        alias = evidence.select_optional(value, 'surface.facts.balance')
        self.assertFalse(alias['available'])
        self.assertEqual(alias['reason'], 'surface_ambiguous')
        self.assertEqual(alias['candidate_selectors'], ['observation.surface.facts.balance', 'result.surface.facts.balance'])
        self.assertEqual(evidence.select(value, 'result.surface.facts.balance'), 1)
        self.assertEqual(evidence.select(value, 'observation.surface.facts.balance'), 2)
        self.assertEqual(evidence.recursive_view(value, 'surface.facts', 'items')['reason'], 'surface_ambiguous')

    def test_slice_select_view_and_messages_use_same_resolver(self):
        client = play_cli.PlayerClient.__new__(play_cli.PlayerClient)
        client.session = Path('/not-a-game'); client.binding = 'bound'
        client.state = {'last_request_id': 'exact', 'observation_request_id': 'exact'}
        value = {'result': {'terminal_observation': {'observation_id': 'terminal-frame',
                 'surface': {'facts': {'messages': [{'text': 'heard'}], 'items': [{'typeid': 'coin', 'uid': 8}]}}}}}
        receipt = {'ok': True, 'receipt': {'binding_id': 'bound', 'response_sha256': 'exact-sha'}}
        with mock.patch.object(play_cli.Bridge, 'response_status', return_value=receipt), \
             mock.patch.object(play_cli.Bridge, 'response_artifact', return_value={'ok': True, 'response': value}), \
             mock.patch.object(client, 'submit') as send:
            sliced = client.inspect('surface.facts.messages', 0, 1, None)
            self.assertEqual(sliced['resolved_selector'], 'result.terminal_observation.surface.facts.messages')
            self.assertEqual(sliced['response_sha256'], 'exact-sha')
            missing = client.inspect('surface.facts.missing', 0, None, None)
            self.assertEqual(missing['error'], 'selected_response_slice_is_unavailable')
            self.assertEqual(missing['resolved_selector'], 'result.terminal_observation.surface.facts.missing')
            fields = client.inspect('surface.facts', 0, None, None, selected_fields=['messages'])
            self.assertEqual(fields['projection']['selectors']['surface.facts.messages']['value'], [{'text': 'heard'}])
            view = client.inspect('surface.facts.items', 0, 1, None, view='items')
            self.assertEqual(view['projection']['rows'][0]['fields']['uid']['value'], 8)
            self.assertEqual(client.messages(0, 1, None)['observation_id'], 'terminal-frame')
            value['observation'] = {'surface': {'facts': {}}}
            self.assertEqual(client.inspect('surface.facts', 0, None, None)['error'], 'surface_ambiguous')
            self.assertEqual(client.messages(0, 1, None)['error'], 'surface_ambiguous')
            send.assert_not_called()

    def test_stale_binding_blocks_every_mode_before_resolution(self):
        client = play_cli.PlayerClient.__new__(play_cli.PlayerClient)
        client.session = Path('/not-a-game'); client.binding = 'current'
        client.state = {'last_request_id': 'old', 'observation_request_id': 'old'}
        with mock.patch.object(play_cli.Bridge, 'response_status', return_value={
             'ok': True, 'receipt': {'binding_id': 'foreign', 'response_sha256': 'old-sha'}}), \
             mock.patch.object(play_cli.Bridge, 'response_artifact') as artifact:
            for options in ({}, {'selected_fields': ['balance']}, {'view': 'items'}):
                self.assertEqual(client.inspect('surface.facts', 0, 1, None, **options)['error'], 'response_binding_mismatch')
            self.assertEqual(client.messages(0, 1, None)['error'], 'response_binding_mismatch')
            artifact.assert_not_called()


if __name__ == '__main__':
    unittest.main()
