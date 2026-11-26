import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parent.parent
import overmap_ai_seed_campaign as campaign


def output(reload=True, mismatch=False):
    return '\n'.join([
        'MIGO_CLOCK phase=loaded step=1 world=1 elapsed=1 delta=1',
        'MIGO_CLOCK phase=active_unloaded step=1 world=2 elapsed=%s delta=1' % (1 if mismatch else 2),
        'MIGO_CLOCK phase=frozen step=1 world=3 elapsed=2 delta=0',
        'MIGO_CLOCK phase=loaded_again step=1 world=4 elapsed=3 delta=1',
        'MIGO_CYCLE earned=604800 birth_sequence=0 world=604800',
        *(['MIGO_RELOAD pending=604800 source=pod resident=1'] if reload else []),
        'MIGO_CYCLE placed=1 roster=2 native_bodies=1 new_member=2',
    ])


class BroodCampaign(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.directory = Path(self.tmp.name) / 'campaign'
        binary = Path(self.tmp.name) / 'binary'
        binary.write_text('controlled child')
        self.data = mock.patch.object(campaign, 'data_digest', return_value='data')
        self.data.start()
        self.addCleanup(self.data.stop)
        campaign.initialize(self.directory, binary, [1, 2], [11, 12], campaign.BROOD_RELOAD)

    def state(self):
        return json.loads((self.directory / 'campaign.json').read_text())

    def run_child(self, text):
        return mock.patch.object(campaign.subprocess, 'run', return_value=mock.Mock(returncode=0, stdout=text))

    def test_stop_resume_preserves_first(self):
        with self.run_child(output()) as child:
            campaign.run(self.directory, 1)
            first = self.state()['attempts'][0]
            self.assertEqual([a['status'] for a in self.state()['attempts']], ['completed', 'pending'])
            campaign.run(self.directory, 1)
            state = self.state()
            self.assertEqual(first, state['attempts'][0])
            self.assertEqual(child.call_count, 2)
            self.assertEqual([a['status'] for a in state['attempts']], ['completed', 'completed'])
            self.assertEqual(campaign.coverage(state)['transitions']['world_reload']['reached'], 2)
            self.assertEqual(child.call_args.kwargs['env']['CAOL_MIGO_WORLD_RELOAD'], '1')
            self.assertEqual(state['native_source_scope'], 'working-checkout-only')
            self.assertIsNone(state['compiled_fixture_sha256'])

    def test_success_exit_with_clock_divergence_has_no_credit_and_stops(self):
        with self.run_child(output(mismatch=True)):
            campaign.run(self.directory)
        state = self.state()
        result = state['attempts'][0]['result']
        self.assertEqual(result['outcome'], 'invariant_failure')
        self.assertEqual(result['first_failure']['world_turn'], 2)
        self.assertEqual(result['first_failure']['production_delta'], 0)
        self.assertTrue(all(not event['reached'] for event in result['events']))
        self.assertEqual(state['attempts'][1]['status'], 'pending')

    def test_native_failure_retains_earliest_clock_discrepancy(self):
        # The native process failed after reporting its first usable clock mismatch.
        text = ('MIGO_CLOCK phase=loaded step=1 world=86403 elapsed=1 delta=1\n'
                'MIGO_CLOCK phase=active_unloaded step=1 world=86404 elapsed=1 delta=1\n'
                'MIGO_CLOCK phase=active_unloaded step=2 world=86405 elapsed=1 delta=1')
        with mock.patch.object(campaign.subprocess, 'run',
                               return_value=mock.Mock(returncode=4, stdout=text)) as child:
            campaign.run(self.directory)
        state = self.state()
        result = state['attempts'][0]['result']
        self.assertEqual((result['exit'], result['outcome']), (4, 'native_failure'))
        self.assertEqual(result['first_failure']['world_turn'], 86404)
        self.assertEqual(result['first_failure']['production_delta'], 0)
        self.assertEqual(result['first_failure']['activity_delta'], 1)
        self.assertTrue(all(not event['reached'] and not event['fired'] for event in result['events']))
        self.assertEqual(state['attempts'][1]['status'], 'pending')
        self.assertEqual(child.call_count, 1)

    def test_first_discrepancy_precedes_missing_later_marker(self):
        with self.run_child(output(reload=False, mismatch=True)):
            campaign.run(self.directory)
        result = self.state()['attempts'][0]['result']
        self.assertEqual(result['outcome'], 'invariant_failure')
        self.assertEqual(result['first_failure']['world_turn'], 2)
        self.assertTrue(all(not event['reached'] for event in result['events']))

    def test_missing_reload_is_incomplete_and_gets_no_credit(self):
        with self.run_child(output(reload=False)):
            with self.assertRaisesRegex(RuntimeError, 'missing requested reload'):
                campaign.run(self.directory)
        state = self.state()
        self.assertEqual(state['attempts'][0]['status'], 'claimed')
        self.assertNotIn('result', state['attempts'][0])
        self.assertEqual(campaign.coverage(state)['transitions']['world_reload']['reached'], 0)

    def test_no_child_does_not_complete(self):
        with mock.patch.object(campaign.subprocess, 'run', side_effect=OSError('child unavailable')):
            with self.assertRaises(OSError):
                campaign.run(self.directory)
        self.assertEqual(self.state()['attempts'][0]['status'], 'claimed')
        self.assertNotIn('result', self.state()['attempts'][0])

    def test_changed_data_refused_before_claim(self):
        with mock.patch.object(campaign, 'data_digest', return_value='changed'):
            with mock.patch.object(campaign.subprocess, 'run') as child:
                with self.assertRaisesRegex(RuntimeError, 'game data changed'):
                    campaign.run(self.directory)
                child.assert_not_called()
        self.assertEqual(self.state()['attempts'][0]['status'], 'pending')

if __name__ == '__main__':
    unittest.main()
