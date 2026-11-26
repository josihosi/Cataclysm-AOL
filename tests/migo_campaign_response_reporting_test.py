"""Check optional reload reporting against actual native event lines."""
import contextlib
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import migo_campaign_response as runner


class ReloadReporting(unittest.TestCase):
    def run_case(self, requested, observed):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / 'result.json'
            argv = ['runner', '--binary', sys.executable, '--setup-seed', '0',
                    '--game-seed', '1', '--output', str(output), '--continue-visible']
            if requested:
                argv += ['--recloak-after-contact', '--world-reload']
            lines = 'MIGO_RESPONSE boundary step=1\n'
            if observed:
                lines += 'MIGO_RESPONSE world_reload step=1 residents=3\n'
            native = mock.Mock(returncode=0, stdout=lines)
            printed = io.StringIO()
            with mock.patch.object(sys, 'argv', argv), mock.patch.object(runner, 'digest', return_value='digest'), \
                    mock.patch.object(runner, 'data_digest', return_value='data'), \
                    mock.patch.object(runner.subprocess, 'run', return_value=native), \
                    contextlib.redirect_stdout(printed):
                code = runner.main()
            return code, json.loads(printed.getvalue()), json.loads(output.read_text())

    def test_optional_reload_requires_native_event(self):
        for requested, observed, expected in ((False, False, 0), (True, False, 1), (True, True, 0)):
            with self.subTest(requested=requested, observed=observed):
                code, summary, result = self.run_case(requested, observed)
                self.assertEqual(code, expected)
                self.assertEqual(summary['world_reload'], observed)
                self.assertEqual(summary['world_reload_required'], requested)
                self.assertEqual(any('MIGO_RESPONSE world_reload ' in line
                                     for line in result['observation']), observed)


if __name__ == '__main__':
    unittest.main()
