"""A non-reload run must not report an unobserved reload."""
import contextlib
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import migo_campaign_response as runner


class ResponseReporting(unittest.TestCase):
    def test_reload_field_reflects_native_event(self):
        for observed in (False, True):
            with self.subTest(observed=observed), tempfile.TemporaryDirectory() as directory:
                output = Path(directory) / 'result.json'
                argv = ['runner', '--binary', sys.executable, '--setup-seed', '0',
                        '--game-seed', '1', '--output', str(output), '--continue-visible']
                lines = 'MIGO_RESPONSE boundary step=1\n'
                if observed:
                    lines += 'MIGO_RESPONSE world_reload step=1 residents=3\n'
                native = mock.Mock(returncode=0, stdout=lines)
                printed = io.StringIO()
                with mock.patch.object(sys, 'argv', argv), mock.patch.object(runner, 'digest', return_value='digest'), \
                        mock.patch.object(runner, 'data_digest', return_value='data'), \
                        mock.patch.object(runner.subprocess, 'run', return_value=native), \
                        contextlib.redirect_stdout(printed):
                    self.assertEqual(runner.main(), 0)
                self.assertEqual(json.loads(printed.getvalue())['world_reload'], observed)


if __name__ == '__main__':
    unittest.main()
