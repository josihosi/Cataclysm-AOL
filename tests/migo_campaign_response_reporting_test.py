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

    def test_working_source_is_not_presented_as_compiled(self):
        code, _, result = self.run_case(False, False)
        self.assertEqual(code, 0)
        self.assertIsNone(result['binding']['fixture_sha256'])
        self.assertEqual(result['binding']['fixture_binding'], 'working-source-only')
        self.assertIsNotNone(result['binding']['working_fixture_sha256'])

    def test_build_receipt_binds_fixture_and_rejects_other_executable(self):
        record = {'schema': 'de67.worker-result-receipt.v1', 'disposition': 'completed',
                  'bindings': {'binary_sha256': 'a' * 64, 'fixture_sha256': 'b' * 64}}
        with tempfile.TemporaryDirectory() as directory:
            receipt = Path(directory) / 'build-receipt.json'
            receipt.write_text(json.dumps(record))
            output = Path(directory) / 'result.json'
            argv = ['runner', '--binary', sys.executable, '--setup-seed', '41', '--game-seed', '1',
                    '--output', str(output), '--continue-visible', '--recloak-after-contact',
                    '--world-reload', '--build-receipt', str(receipt)]
            native = mock.Mock(returncode=0, stdout='MIGO_RESPONSE world_reload step=1 residents=3\n'
                               'MIGO_RESPONSE boundary step=2\n')
            real_digest = runner.digest
            def bound_digest(path):
                if Path(path).resolve() == Path(sys.executable).resolve():
                    return record['bindings']['binary_sha256']
                return real_digest(path)
            with mock.patch.object(sys, 'argv', argv), mock.patch.object(runner, 'digest', side_effect=bound_digest), \
                    mock.patch.object(runner, 'data_digest', return_value='data'), \
                    mock.patch.object(runner.subprocess, 'run', return_value=native), \
                    contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(runner.main(), 0)
            binding = json.loads(output.read_text())['binding']
            self.assertEqual(binding['fixture_sha256'], record['bindings']['fixture_sha256'])
            self.assertEqual(binding['fixture_binding'], 'receipt-declared')
            self.assertEqual(binding['build_receipt_sha256'], real_digest(receipt))
            output.unlink()
            with mock.patch.object(sys, 'argv', argv), \
                    mock.patch.object(runner.subprocess, 'run') as child, \
                    contextlib.redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit):
                    runner.main()
                child.assert_not_called()
            self.assertFalse(output.exists())

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
