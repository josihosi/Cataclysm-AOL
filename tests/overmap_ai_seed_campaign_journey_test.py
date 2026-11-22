#!/usr/bin/env python3
"""Connected-scenario controls for data pinning and reached-event accounting."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

import overmap_ai_seed_campaign as campaign


class JourneyControls(unittest.TestCase):
    def test_data_identity_guards_and_observed_miss(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            binary = root / 'native'
            binary.write_text('#!' + sys.executable + '\n'
                              'print(\'CAMPAIGN_JOURNEY {"watch_minute":850,"departure_fired":0,'
                              '"return_minute":1140,"return_generation":1,"generation":1,'
                              '"physical_returns":2}\')\n')
            binary.chmod(0o755)
            directory = root / 'campaign'
            with patch.object(campaign, 'data_digest', return_value='data-a'):
                campaign.initialize(directory, binary, [0], [124], campaign.JOURNEY)
            with patch.object(campaign, 'data_digest', return_value='data-b'):
                with self.assertRaisesRegex(RuntimeError, 'game data changed'):
                    campaign.run(directory)
            attempt = json.loads((directory / 'campaign.json').read_text())['attempts'][0]
            self.assertEqual(attempt['status'], 'pending')
            with patch.object(campaign, 'data_digest', side_effect=['data-a', 'data-b']):
                with self.assertRaisesRegex(RuntimeError, 'changed during attempt'):
                    campaign.run(directory)
            attempt = json.loads((directory / 'campaign.json').read_text())['attempts'][0]
            self.assertEqual(attempt['status'], 'claimed')
            self.assertNotIn('result', attempt)
            with patch.object(campaign, 'data_digest', return_value='data-a'):
                result = campaign.run(directory)
            self.assertEqual(result['transitions']['departure'], {'reached': 0, 'missed': 1})
            self.assertEqual(result['transitions']['physical_return'], {'reached': 1, 'missed': 0})


if __name__ == '__main__':
    unittest.main()
