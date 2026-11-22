#!/usr/bin/env python3
"""Exercise crash recovery and writer exclusion with a disposable native stand-in."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest

import overmap_ai_seed_campaign as campaign


class CampaignControls(unittest.TestCase):
    def test_interrupted_parent_keeps_child_lock_and_completed_result(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            binary = root / 'native'
            binary.write_text('#!' + sys.executable + '\n'
                              'import os, time\n'
                              'time.sleep(2)\n'
                              'print("CAMPAIGN_EVENT {\\"player\\":1,\\"follower\\":0,\\"rolling\\":" + '
                              'os.environ["CAOL_CAMPAIGN_ROLLING"] + '
                              '",\\"reached\\":1,\\"parley\\":0}")\n')
            binary.chmod(0o755)
            directory = root / 'campaign'
            campaign.initialize(directory, binary, [0, 3], [123, 123])
            campaign.run(directory, 1)
            before = json.loads((directory / 'campaign.json').read_text())
            runner = subprocess.Popen([sys.executable, str(Path(campaign.__file__)), 'run', str(directory)],
                                      stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline:
                current = json.loads((directory / 'campaign.json').read_text())
                if current['attempts'][1]['status'] == 'claimed':
                    # Wait until the inherited lock is known to belong to the child too.
                    time.sleep(.3)
                    break
                time.sleep(.02)
            else:
                runner.kill()
                runner.wait()
                self.fail('second attempt was not claimed')
            runner.kill()
            runner.wait()
            with self.assertRaisesRegex(RuntimeError, 'writer lock'):
                campaign.run(directory)
            deadline = time.monotonic() + 10
            while True:
                try:
                    lock = campaign.acquire(directory)
                    lock.close()
                    break
                except RuntimeError:
                    if time.monotonic() > deadline:
                        self.fail('child did not release writer lock')
                    time.sleep(.1)
            resumed = campaign.run(directory)
            after = json.loads((directory / 'campaign.json').read_text())
            self.assertEqual(before['attempts'][0], after['attempts'][0])
            self.assertEqual(after['attempts'][1]['launches'], 2)
            self.assertEqual(resumed['completed'], 2)
            self.assertEqual(resumed['transitions']['unreachable_control'], {'reached': 0, 'missed': 2})

    def test_known_signal_exit_stays_incomplete(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            binary = root / 'native'
            binary.write_text('#!' + sys.executable + '\nimport os, signal\nos.kill(os.getpid(), signal.SIGTERM)\n')
            binary.chmod(0o755)
            directory = root / 'campaign'
            campaign.initialize(directory, binary, [0], [123])
            with self.assertRaisesRegex(RuntimeError, 'terminated by signal'):
                campaign.run(directory)
            attempt = json.loads((directory / 'campaign.json').read_text())['attempts'][0]
            self.assertEqual(attempt['status'], 'claimed')
            self.assertLess(attempt['last_error']['exit'], 0)
            self.assertNotIn('result', attempt)

    def test_seed_resolution_is_independent(self):
        a = campaign.resolve(0, 123)
        b = campaign.resolve(3, 123)
        c = campaign.resolve(0, 124)
        self.assertNotEqual(a['setup'], b['setup'])
        self.assertEqual(a['setup'], c['setup'])
        self.assertEqual(a, campaign.resolve(0, 123))


if __name__ == '__main__':
    unittest.main()
