"""Native Trade projection and authenticated selection-batch caller controls."""
import copy
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

import gameplay_display as display
import play_cli


class TradeRowsTest(unittest.TestCase):
    def snapshot(self):
        rows = [{'group_uid': str(i), 'letter': chr(97 + i) if i < 20 else None,
                 'name': 'local coin ' + str(i), 'party': 'player', 'actor_id': 'character:3',
                 'available': 5, 'selected': 0, 'unit': 'items', 'unit_price': '$4.50',
                 'selected_value': '$0.00', 'enabled': True,
                 'locations': [{'root_source': 'map', 'owner_faction': 'your_followers'}]}
                for i in range(797)]
        facts = {'trade_rows': rows, 'selection_source': 'trade_selector::to_use',
                 'active_party': 'player', 'active_actor_id': 'character:3',
                 'player_name': 'Casey', 'trader_name': 'Granville', 'demand_amount': '$1396.26',
                 'balance': '-$1396.26', 'player_offer_value': '$0.00', 'trader_offer_value': '$0.00'}
        return {'owner': 'inventory', 'current': {'observation_id': 'frame:37', 'facts': facts}}

    def client(self, snapshot):
        client = object.__new__(play_cli.PlayerClient)
        client.frame = mock.Mock(return_value='frame:37')
        client.display_snapshot = mock.Mock(return_value=snapshot)
        client.act = mock.Mock(return_value={'state': 'pending', 'request_id': 'one-native-request'})
        return client

    def test_batch_validates_whole_current_pane_and_submits_once_without_commit(self):
        snapshot = self.snapshot()
        before = copy.deepcopy(snapshot)
        client = self.client(snapshot)
        self.assertEqual(client.trade_letters('abc', 1)['state'], 'pending')
        client.act.assert_called_once_with('trade.toggle_letters', None,
            {'letters': 'abc', 'party': 'player', 'actor_id': 'character:3'}, 1)
        self.assertEqual(snapshot, before)
        for letters, expected in [('aba', 'duplicate_trade_letter'), ('ab?', 'unavailable_or_ambiguous_trade_letter'),
                                  ('', 'trade_letters_unavailable')]:
            client = self.client(snapshot)
            self.assertEqual(client.trade_letters(letters, 1)['error'], expected)
            client.act.assert_not_called()
        for change, expected in [('pane', 'foreign_trade_pane'), ('ambiguous', 'unavailable_or_ambiguous_trade_letter'),
                                 ('disabled', 'unavailable_or_ambiguous_trade_letter'),
                                 ('frame', 'stale_trade_rows'), ('owner', 'current_owner_is_not_trade')]:
            altered = copy.deepcopy(snapshot)
            rows = altered['current']['facts']['trade_rows']
            if change == 'pane': rows[2]['actor_id'] = 'character:4'
            elif change == 'ambiguous': rows[3]['letter'] = 'c'
            elif change == 'disabled': rows[2]['enabled'] = False
            elif change == 'frame': altered['current']['observation_id'] = 'old'
            else: altered['owner'] = 'prompt'
            client = self.client(altered)
            self.assertEqual(client.trade_letters('abc', 1)['error'], expected)
            client.act.assert_not_called()
        client = self.client(snapshot)
        client.frame.side_effect = ValueError('request_in_flight: use collect')
        with self.assertRaisesRegex(ValueError, 'request_in_flight'):
            client.trade_letters('ab', 1)
        client.act.assert_not_called()

    def test_compact_rows_and_controls_are_paged_without_replacing_native_values(self):
        snapshot = self.snapshot()
        rows = snapshot['current']['facts']['trade_rows']
        text = display._plain_trade_rows(rows)
        self.assertIn('a | 0 | local coin 0 | 0/5 items | $4.50', text)
        self.assertNotIn('local coin 8', text)
        self.assertLess(len(text), 1500)
        actions = [{'id': 'inventory.toggle', 'stable_id': str(i), 'enabled': True} for i in range(797)]
        actions += [{'id': 'trade.toggle_letters', 'stable_id': '', 'enabled': True}]
        selected = display._trade_page_actions(actions, snapshot['current']['facts'])
        self.assertEqual(len(selected), 9)
        self.assertIn('play trade LETTERS', display._plain_controls(selected))
        response = {'observation': {'run_id': 'r', 'surface_id': 's', 'observation_id': 'frame:37',
                    'surface': {'kind': 'inventory', 'facts': snapshot['current']['facts'], 'actions': actions}}}
        view, current = display.display(response, refresh=True)
        self.assertEqual(current['current']['observation_id'], 'frame:37')
        output = display.plain_player_output({'state': 'collected', 'response': view}, snapshot=current)
        self.assertIn('OUR SIDE: Casey', output)
        self.assertIn('Demand $1396.26', output)
        self.assertIn('797 filtered groups', output)
        self.assertNotIn('local coin 796', output)
        self.assertNotIn('--target 796', output)
        self.assertLess(len(output), 2000)
        rows[0]['letter'] = None
        rows[0].pop('unit_price')
        rows[0]['locations'] = []
        text = display._plain_trade_rows(rows[:1])
        self.assertIn('- | 0', text)
        self.assertIn('unavailable', text)
        self.assertNotIn('rawprice', text)

    def test_trade_slice_uses_existing_authenticated_response_and_exact_source_path(self):
        from cockpit_file_bridge import FileBackedCockpitBridge as bridge
        rows = self.snapshot()['current']['facts']['trade_rows']
        client = object.__new__(play_cli.PlayerClient)
        client.session = Path('/unused')
        client.binding = 'bound'
        client.state = {'last_request_id': 'request-37'}
        for envelope in ('result', 'observation'):
            response = {envelope: {'surface': {'facts': {'trade_rows': json.dumps(rows)}}}}
            receipt = {'ok': True, 'receipt': {'binding_id': 'bound', 'response_sha256': 'exact-sha'}}
            with mock.patch.object(bridge, 'response_status', return_value=receipt), \
                 mock.patch.object(bridge, 'response_artifact', return_value={'ok': True, 'response': response}):
                result = client.inspect('', 1, 3, 'local coin', view='trade')
                self.assertEqual(result['response_sha256'], 'exact-sha')
                self.assertEqual(result['request_id'], 'request-37')
                self.assertEqual(result['resolved_selector'], envelope + '.surface.facts.trade_rows')
                self.assertEqual([row['group_uid'] for row in result['slice']], ['1', '2', '3'])
                full = client.inspect('surface.facts.trade_rows', 0, None, None)
                self.assertEqual(len(full['slice']), 797)
        with mock.patch.object(bridge, 'response_status', return_value={'ok': True, 'receipt': {'binding_id': 'foreign'}}), \
             mock.patch.object(bridge, 'response_artifact') as artifact:
            self.assertEqual(client.inspect('', 0, 8, None, view='trade')['error'], 'response_binding_mismatch')
            artifact.assert_not_called()


if __name__ == '__main__':
    unittest.main()
