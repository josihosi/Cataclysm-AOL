#!/usr/bin/env python3
"""Offline controls for the payment campaign's observed decision boundary."""

import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest import mock

import overmap_ai_seed_campaign as campaign


class CustodyDecisionReportingTest(unittest.TestCase):
    def test_next_decision_uses_post_settlement_repeat(self):
        for repeat, expected in (("CUSTODY repeat credit=100 decision=cooldown\n", True),
                                 ("CUSTODY repeat credit=100 decision=idle\n", False),
                                 ("", False)):
            with self.subTest(repeat=repeat), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                binary = root / 'native'
                binary.write_text('stand-in')
                directory = root / 'campaign'
                output = ('CUSTODY payment uid=1\n'
                          'CUSTODY boundary settled=1 credit=100 expected_delta=100 decision=cooldown\n'
                          + repeat)
                with mock.patch.object(campaign, 'data_digest', return_value='fixed'):
                    campaign.initialize(directory, binary, [0], [1], campaign.CUSTODY_AUTH)
                    with mock.patch.object(campaign.subprocess, 'run',
                                           return_value=subprocess.CompletedProcess([], 0, output)):
                        campaign.run(directory)
                attempt = json.loads((directory / 'campaign.json').read_text())['attempts'][0]
                event = next(event for event in attempt['result']['events']
                             if event['transition'] == 'next_decision')
                self.assertEqual(event['reached'], expected)
                self.assertEqual(event['fired'], expected)


if __name__ == '__main__':
    unittest.main()
