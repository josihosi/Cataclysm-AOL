#!/usr/bin/env python3
"""Payment campaign data binding is checked before and after child execution."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


class PaymentDataBinding(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        (self.root / 'tests').mkdir()
        (self.root / 'data').mkdir()
        self.data = self.root / 'data' / 'input.json'
        self.data.write_text('original')
        (self.root / 'tests' / 'bandit_live_world_test.cpp').write_text('fixture')
        source = Path(os.environ.get('CAOL_BINDING_TEST_RUNNER',
                                    Path(__file__).with_name('overmap_ai_seed_campaign.py')))
        self.runner = self.root / 'tests' / 'overmap_ai_seed_campaign.py'
        shutil.copyfile(source, self.runner)
        subprocess.run(['git', 'init', '-q', str(self.root)], check=True)
        subprocess.run(['git', '-C', str(self.root), '-c', 'user.name=Test',
                        '-c', 'user.email=test@example.invalid', 'commit', '-q',
                        '--allow-empty', '-m', 'test'], check=True)
        self.marker = self.root / 'child-started'
        self.binary = self.root / 'child'
        self.binary.write_text('#!/bin/sh\ntouch ' + str(self.marker) + '\n'
                               'if [ -f ' + str(self.root / 'mutate') + ' ]; then\n'
                               '  echo changed > ' + str(self.data) + '\nfi\n'
                               'echo "CUSTODY payment item=1"\n'
                               'echo "CUSTODY boundary settled=1 credit=1"\n'
                               'echo "CUSTODY repeat credit=1 decision=cooldown"\n')
        self.binary.chmod(0o755)

    def cli(self, *args):
        return subprocess.run([sys.executable, str(self.runner), *map(str, args)],
                              cwd=self.root, text=True, capture_output=True)

    def init(self, scenario):
        directory = self.root / scenario
        result = self.cli('init', directory, '--binary', self.binary, '--scenario', scenario,
                          '--setup-seeds', 0, '--game-seeds', 123)
        self.assertEqual(result.returncode, 0, result.stderr)
        return directory

    def test_changed_data_rejected_before_claim_or_child(self):
        for scenario in ('bandit-payment-custody-v1', 'bandit-payment-authenticated-v1'):
            with self.subTest(scenario=scenario):
                self.marker.unlink(missing_ok=True)
                self.data.write_text('original')
                directory = self.init(scenario)
                self.data.write_text('changed')
                result = self.cli('run', directory, '--limit', 1)
                self.assertNotEqual(result.returncode, 0)
                self.assertFalse(self.marker.exists(), 'native child started despite changed data')
                self.assertIn('game data changed; create a new campaign', result.stderr)
                attempt = json.loads((directory / 'campaign.json').read_text())['attempts'][0]
                self.assertEqual(attempt['status'], 'pending')
                self.assertNotIn('launches', attempt)

    def test_matching_data_runs_to_completion(self):
        for scenario in ('bandit-payment-custody-v1', 'bandit-payment-authenticated-v1'):
            with self.subTest(scenario=scenario):
                directory = self.init(scenario)
                result = self.cli('run', directory, '--limit', 1)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertTrue(self.marker.exists())
                attempt = json.loads((directory / 'campaign.json').read_text())['attempts'][0]
                self.assertEqual(attempt['status'], 'completed')
                self.marker.unlink()

    def test_matching_data_launches_and_post_run_change_is_rejected(self):
        for scenario in ('bandit-payment-custody-v1', 'bandit-payment-authenticated-v1'):
            with self.subTest(scenario=scenario):
                self.data.write_text('original')
                directory = self.init(scenario)
                (self.root / 'mutate').touch()
                result = self.cli('run', directory, '--limit', 1)
                self.assertNotEqual(result.returncode, 0)
                self.assertTrue(self.marker.exists())
                self.assertIn('game data changed during attempt', result.stderr)
                attempt = json.loads((directory / 'campaign.json').read_text())['attempts'][0]
                self.assertEqual(attempt['status'], 'claimed')
                self.assertNotIn('result', attempt)
                self.marker.unlink()
                (self.root / 'mutate').unlink()


if __name__ == '__main__':
    unittest.main()
