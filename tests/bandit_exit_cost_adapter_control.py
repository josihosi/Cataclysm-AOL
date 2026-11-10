#!/usr/bin/env python3
"""Check the real homeward adapter against its exhaustive reference."""
import os
import re
import subprocess
import sys
import unittest


class ExitAdapterReference(unittest.TestCase):
    def test_homeward_selection_and_repeated_block(self):
        env = os.environ.copy()
        env['CAOL_EXIT_COST_REFERENCE_CHECK'] = '1'
        env.pop('CAOL_EXIT_TRANSFER_MODE', None)
        binary = os.environ.get('CAOL_EXIT_TEST_BINARY') or (
            'tests/cata_test.exe' if sys.platform == 'win32' else 'tests/cata_test')
        command = [binary, 'hostile_camp_local_handoff_binds_the_complete_pair_transactionally',
                   '--section', 'homeward materialization binds both physical camp routes',
                   '--rng-seed', '48151']
        with self.subTest(mode='reference'):
            proc = subprocess.run(
                command,
                env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False, timeout=120)
            self.assertEqual(proc.returncode, 0, proc.stdout[-5000:])
            rows = re.findall(r'exit_cost_reference reference_match=(\d+) repeated_match=(\d+) '
                              r'exact_pairs=1 verdict=(H[01]) pairs=(\d+) route_solves=(\d+)', proc.stdout)
            self.assertEqual(len(rows), 2, proc.stdout[-10000:])
            self.assertEqual([row[:3] for row in rows], [('1', '1', 'H1'), ('1', '1', 'H0')])
            parity = re.findall(r'exit_cost_diagnostic_parity slots=(\d+) owner=(\d+) verdict=(H[01])',
                                proc.stdout)
            self.assertEqual(parity, [('1', '1', 'H1'), ('1', '1', 'H0')])
            self.assertTrue('exit_cost_rank_mismatch matched=1 eligibility_changed=1' in proc.stdout, proc.stdout[-5000:])
            self.assertTrue('exit_cost_first_safe matched=1 alternate=1' in proc.stdout, proc.stdout[-5000:])
            self.assertTrue('exit_cost_survivor recorded=1 survivor_selected=1 unresolved_refused=1' in proc.stdout, proc.stdout[-5000:])
            self.assertTrue('exit_cost_invalidation applied=1 refused=1 owner_preserved=1' in proc.stdout, proc.stdout[-5000:])
            self.assertGreater(int(rows[1][3]), 0)
            self.assertGreater(int(rows[1][4]), 0)
            print('adapter exhaustive/repeated controls:', rows)
            for line in proc.stdout.splitlines():
                if any(tag in line for tag in ('exit_cost_rank_mismatch', 'exit_cost_first_safe',
                                               'exit_cost_survivor', 'exit_cost_invalidation', 'exit_cost_transfer')):
                    print(line[line.index('exit_cost_'):])
        for mode in ('route', 'survivor'):
            with self.subTest(mode=mode):
                isolated_env = env.copy()
                isolated_env['CAOL_EXIT_TRANSFER_MODE'] = mode
                isolated = subprocess.run(command, env=isolated_env, text=True,
                                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                          check=False, timeout=120)
                self.assertEqual(isolated.returncode, 0, isolated.stdout[-5000:])
                self.assertTrue(f'exit_cost_isolated mode={mode} status=0' in isolated.stdout,
                                isolated.stdout[-5000:])
                print(f'isolated actual consumer: {mode} status=0')


if __name__ == '__main__':
    unittest.main()
