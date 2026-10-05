"""Original R6 save/menu receipts: source-reader and one-shot caller controls."""
import copy
import gzip
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import cockpit
import startup_harness as h

FIXTURE = Path(__file__).parent / 'fixtures/controls/r067_closeout_successors.json.gz'

def original():
    return json.loads(gzip.decompress(FIXTURE.read_bytes()))

class CloseoutSuccessorTest(unittest.TestCase):
    def dispatch(self, case, split=False, missing=False, ended=False):
        before, after, receipt = (case[k] for k in ('before', 'after', 'receipt'))
        action = receipt['action_id']
        target = next((a['stable_id'] for a in before['valid_actions']
                       if a['id'] == action and a['stable_id']), None)
        rows = [r['event'] for r in original()['rows'] if r['event']['sequence'] <=
                max(receipt['sequence'], after['sequence'])]
        rows = [r for r in rows if not missing or r.get('frame_id') != after['frame_id']]
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source = root / 'original.log'
            published = rows
            pending = []
            if split:
                # Exercise both native orders: descriptor->deferred receipt,
                # and immediate receipt->confirmation descriptor.
                boundary = min(receipt['sequence'], after['sequence'])
                published = [r for r in rows if r['sequence'] <= boundary]
                pending = [r for r in rows if r['sequence'] > boundary]
            def encode(records):
                return ''.join(h.SEMANTIC_STEP_PREFIX + json.dumps(r) + '\n' for r in records)
            source.write_text(encode(published))
            polls = 0
            def publish_later(_delay):
                nonlocal polls
                polls += 1
                if pending:
                    with source.open('a') as stream:
                        stream.write(encode(pending))
                    pending.clear()
            with patch.object(h, 'semantic_step_source_trace', return_value=source), \
                 patch.object(h, 'semantic_wake_pipe_contract', return_value={'status':'bound','path':'test'}), \
                 patch.object(h, 'write_semantic_wake_pipe', return_value=1) as wake, \
                 patch.object(h.time, 'sleep', side_effect=publish_later):
                result = h.execute_semantic_act(run_dir=root, profile='none', run_id=before['run_id'],
                    trace_start_offset=0, pid=79088, session_id=receipt['request_id'].split(':')[1],
                    frame_id=before['frame_id'], action_id=action, stable_id=target, observed_frame=before,
                    transition_timeout_seconds=.02, observe_interval_seconds=.001,
                    read_process_state=lambda: {'alive': not ended, 'pid':79088, 'run_id':before['run_id']})
            self.assertEqual(wake.call_count, 1)
            self.assertEqual(len((root/'semantic.requests.jsonl').read_text().splitlines()), 1)
            return result, polls

    def test_original_three_successors_same_and_split_poll(self):
        for case in original()['cases']:
            for split in (False, True):
                with self.subTest(action=case['receipt']['action_id'], split=split):
                    result, polls = self.dispatch(case, split=split)
                    self.assertTrue(result['accepted'], result)
                    self.assertEqual(result['native_receipt']['request_id'], case['receipt']['request_id'])
                    self.assertEqual(result['next_frame']['frame_id'], case['after']['frame_id'])
                    self.assertEqual(result['next_frame']['valid_actions'], case['after']['valid_actions'])
                    if split:
                        self.assertGreater(polls, 0)

    def test_alive_quit_without_confirmation_remains_incomplete(self):
        result, _ = self.dispatch(original()['cases'][-1], missing=True)
        self.assertFalse(result['accepted'])
        self.assertEqual(result['reason'], 'native_surface_successor_timeout')
        self.assertIsNone(result['next_frame'])

    def test_exact_process_exit_needs_no_invented_successor(self):
        result, _ = self.dispatch(original()['cases'][-1], missing=True, ended=True)
        self.assertTrue(result['accepted'], result)
        self.assertIsNone(result['next_frame'])

    def test_original_missing_receipt_is_uncertain_and_cannot_replay(self):
        fixture = original()
        for case, response in zip(fixture['cases'], fixture['missing_receipt_responses']):
            before = case['before']
            with self.subTest(action=case['receipt']['action_id']):
                dispatches = []
                def dispatch(*args):
                    dispatches.append(args)
                    return copy.deepcopy(response['receipt'])
                channel = cockpit.CockpitRunChannel(lambda: before, dispatch)
                observation = channel.observe()
                action = case['receipt']['action_id']
                target = next((a['stable_id'] for a in before['valid_actions']
                               if a['id'] == action and a['stable_id']), None)
                result = channel.act(observation_id=observation['observation_id'], action_id=action, stable_id=target)
                self.assertEqual(result['error'], 'native_surface_receipt_timeout')
                self.assertEqual(result['failure']['detail']['action_outcome'], 'unknown')
                self.assertIsNone(result['receipt']['native_receipt'])
                from gameplay_display import plain_player_output
                shown = plain_player_output(result)
                self.assertIn('action outcome is unknown', shown)
                self.assertNotIn('Rejected:', shown)
                replay = channel.act(observation_id=observation['observation_id'], action_id=action, stable_id=target)
                self.assertEqual(replay['error'], 'duplicate_submission')
                self.assertEqual(len(dispatches), 1)
                # A reread of the same native descriptor is not new authority.
                reread = channel.observe()
                replay = channel.act(observation_id=reread['observation_id'], action_id=action, stable_id=target)
                self.assertEqual(replay['error'], 'duplicate_submission')
                self.assertEqual(len(dispatches), 1)

    def test_archived_unknown_frame_stays_consumed_but_fresh_owner_is_usable(self):
        from cockpit_archive import Archive
        case = original()['cases'][-1]
        current = [copy.deepcopy(case['before'])]
        response = copy.deepcopy(original()['missing_receipt_responses'][0]['receipt'])
        response['surface_request'] = {'run_id':current[0]['run_id'],
            'surface_id':current[0]['surface_id'], 'frame_id':current[0]['frame_id'],
            'action_id':'main_menu.quit'}
        dispatches = []
        def dispatch(*args):
            dispatches.append(args)
            return response
        with tempfile.TemporaryDirectory() as temp:
            archive = Archive(Path(temp)/'evidence.sqlite3', run_id=current[0]['run_id'], binding_id='bound')
            try:
                channel = cockpit.CockpitRunChannel(lambda: current[0], dispatch, archive=archive)
                observed = channel.observe()
                channel.act(observation_id=observed['observation_id'], action_id='main_menu.quit')
                reread = channel.observe()
                self.assertEqual(channel.act(observation_id=reread['observation_id'], action_id='main_menu.quit')['error'],
                                 'duplicate_submission')
                self.assertEqual(len(dispatches), 1)
                current[0] = case['after']
                fresh = channel.observe()
                self.assertNotEqual(fresh['observation_id'], observed['observation_id'])
                self.assertFalse(channel._observations[fresh['observation_id']]['used'])
            finally:
                archive.close()

    def test_explicit_native_rejection_is_distinct(self):
        case = original()['cases'][-1]
        before, receipt = case['before'], copy.deepcopy(case['receipt'])
        receipt.update(accepted=False, rejection_reason='wrong_surface')
        channel = cockpit.CockpitRunChannel(lambda: before,
                    lambda *args: {'native_receipt': receipt, 'accepted':False})
        observed = channel.observe()
        result = channel.act(observation_id=observed['observation_id'], action_id='main_menu.quit')
        self.assertEqual(result['error'], 'native_action_rejected')
        self.assertEqual(result['receipt']['native_receipt']['rejection_reason'], 'wrong_surface')
        self.assertEqual(channel.act(observation_id=observed['observation_id'], action_id='main_menu.quit')['error'],
                         'duplicate_submission')

if __name__ == '__main__':
    unittest.main()
