#!/usr/bin/env python3
"""Failure reporting uses the first invariant and survives interrupted publication."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

import overmap_ai_seed_campaign as campaign


class FailureControls(unittest.TestCase):
    def test_first_broken_invariant_wins(self):
        recipe = campaign.resolve(1, 124, campaign.JOURNEY)
        observation = {'watch_minute': 850, 'departure_fired': 1, 'return_minute': 849,
                       'return_generation': 1, 'generation': 1, 'physical_returns': 0}
        report = campaign.journey_invariant_result(recipe, observation)
        self.assertEqual(report['first_failure']['invariant'], 'return_after_watch')
        self.assertEqual(report['first_failure']['last_valid']['checked_invariants'],
                         ['watch_reached', 'departure_matches_schedule'])

    def test_distinct_failures_replay_and_interrupted_publication(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            binary = root / 'native'
            binary.write_text('#!' + sys.executable + '\n'
                              'print(\'CAMPAIGN_JOURNEY {"watch_minute":850,"departure_fired":1,'
                              '"return_minute":1140,"return_generation":1,"generation":1,'
                              '"physical_returns":2}\')\n')
            binary.chmod(0o755)
            directory = root / 'campaign'
            controls = ['none', 'return_before_watch', 'missing_physical_return', 'return_before_watch']
            with patch.object(campaign, 'data_digest', return_value='data'):
                campaign.initialize(directory, binary, [1, 1, 1, 1], [124, 124, 124, 124], campaign.JOURNEY, controls)
                result = campaign.run(directory)
                state = json.loads((directory / 'campaign.json').read_text())
                self.assertEqual([a['result']['outcome'] for a in state['attempts']],
                                 ['pass', 'invariant_failure', 'invariant_failure', 'invariant_failure'])
                self.assertEqual({g['key']['invariant'] for g in result['failure_groups']},
                                 {'return_after_watch', 'physical_return_committed'})
                group = next(g for g in result['failure_groups'] if g['key']['invariant'] == 'return_after_watch')
                self.assertEqual(group['attempt_ids'], [1, 3])
                for a in state['attempts'][1:]:
                    self.assertEqual(a['result']['native_summary']['return_minute'], 1140)
                    self.assertEqual(a['result']['first_failure']['classification'], 'test_control')
                tampered = root / 'tampered'
                campaign.replay(directory, 1, tampered)
                tampered_path = tampered / 'campaign.json'
                tampered_state = json.loads(tampered_path.read_text())
                tampered_state['attempts'][0]['recipe']['game_seed'] = 125
                campaign.atomic_json(tampered_path, tampered_state)
                with self.assertRaisesRegex(RuntimeError, 'recorded recipe changed'):
                    campaign.run(tampered)
                tampered_state['attempts'][0]['recipe']['game_seed'] = 124
                tampered_state['runner_sha256'] = 'different runner'
                campaign.atomic_json(tampered_path, tampered_state)
                with self.assertRaisesRegex(RuntimeError, 'campaign runner changed'):
                    campaign.run(tampered)
                replay = root / 'replay'
                campaign.replay(directory, 1, replay)
                campaign.run(replay)
                replayed = json.loads((replay / 'campaign.json').read_text())['attempts'][0]
                self.assertEqual(replayed['recipe'], state['attempts'][1]['recipe'])
                self.assertEqual(replayed['result'], state['attempts'][1]['result'])
                failing_binary = root / 'native-error'
                failing_binary.write_text(binary.read_text() + 'import sys\nsys.exit(1)\n')
                failing_binary.chmod(0o755)
                native_error = root / 'native-error-campaign'
                campaign.initialize(native_error, failing_binary, [1], [124], campaign.JOURNEY, 'return_before_watch')
                failure_coverage = campaign.run(native_error)
                failure_state = json.loads((native_error / 'campaign.json').read_text())
                self.assertEqual(failure_state['attempts'][0]['result']['outcome'], 'native_failure')
                self.assertFalse(failure_coverage['failure_groups'])
                interrupted = root / 'interrupted'
                campaign.replay(directory, 1, interrupted)
                atomic = campaign.atomic_json
                def interrupted_publish(path, obj):
                    if obj['attempts'][0]['status'] == 'completed':
                        raise OSError('injected publication interruption')
                    return atomic(path, obj)
                with patch.object(campaign, 'atomic_json', side_effect=interrupted_publish):
                    with self.assertRaisesRegex(OSError, 'publication interruption'):
                        campaign.run(interrupted)
                attempt = json.loads((interrupted / 'campaign.json').read_text())['attempts'][0]
                self.assertEqual(attempt['status'], 'claimed')
                self.assertNotIn('result', attempt)
                campaign.run(interrupted)
                attempt = json.loads((interrupted / 'campaign.json').read_text())['attempts'][0]
                self.assertEqual(attempt['result']['outcome'], 'invariant_failure')


if __name__ == '__main__':
    unittest.main()
