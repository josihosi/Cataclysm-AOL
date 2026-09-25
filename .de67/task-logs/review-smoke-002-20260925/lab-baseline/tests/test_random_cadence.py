from pathlib import Path
import json
import sqlite3
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from deadline_harness import DeadlineHarness, RANDOM_INTERVAL_MIN, RANDOM_INTERVAL_MAX, CADENCE_VERSION

class RandomCadenceTests(unittest.TestCase):
    def complete(self, h, count):
        for n in range(count):
            h.start_task('fixture', f't-{n}', f'R-{n}', 100, now=0)
            h.complete_task('fixture', f't-{n}', 'fixture completion', now=1)

    def test_inclusive_draws(self):
        self.assertEqual((RANDOM_INTERVAL_MIN, RANDOM_INTERVAL_MAX, CADENCE_VERSION), (20, 50, 3))
        for offset, expected in [(0, 20), (30, 50)]:
            with patch('deadline_harness.secrets.randbelow', side_effect=[offset, 0]) as draw:
                self.assertEqual(DeadlineHarness._draw_random_cycle()[0], expected)
                self.assertEqual(draw.call_args_list[0].args, (31,))

    def test_v2_pending_cycle_keeps_progress_and_is_not_redrawn(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'state.sqlite3'
            with patch('deadline_harness.secrets.randbelow', side_effect=[0, 0]), DeadlineHarness(p) as h:
                self.complete(h, 5)
                h.connection.execute("UPDATE random_mutation_cycles SET interval_windows=11,due_after_terminal_windows=11,cadence_version=2")
                h.connection.commit()
                before=[tuple(r) for r in h.connection.execute('SELECT * FROM tasks ORDER BY task_id')]
            # Exercise the actual old version constraint, not only an old row in a new schema.
            with sqlite3.connect(p) as c:
                sql=c.execute("SELECT sql FROM sqlite_master WHERE name='random_mutation_cycles'").fetchone()[0]
                sql=sql.replace('CREATE TABLE random_mutation_cycles', 'CREATE TABLE old_shape')
                sql=sql.replace('cadence_version IN (1, 2, 3)', 'cadence_version IN (1, 2)').replace('DEFAULT 3', 'DEFAULT 2')
                c.execute('PRAGMA foreign_keys=OFF');c.execute(sql)
                c.execute('INSERT INTO old_shape SELECT * FROM random_mutation_cycles')
                c.execute('DROP TABLE random_mutation_cycles');c.execute('ALTER TABLE old_shape RENAME TO random_mutation_cycles')
            with patch('deadline_harness.secrets.randbelow', side_effect=AssertionError('no redraw')), DeadlineHarness(p) as h:
                row=h.connection.execute('SELECT * FROM random_mutation_cycles').fetchone()
                self.assertEqual((row['cycle_number'],row['interval_windows'],row['due_after_terminal_windows'],row['cadence_version']), (1,20,20,3))
                self.assertEqual(h._terminal_window_count('fixture'),5)
                self.assertEqual(before,[tuple(r) for r in h.connection.execute('SELECT * FROM tasks ORDER BY task_id')])
                self.assertEqual(h.connection.execute('PRAGMA foreign_key_check').fetchall(),[])
            with patch('deadline_harness.secrets.randbelow', side_effect=AssertionError('no redraw')), DeadlineHarness(p) as h:
                self.assertEqual(h.list_tasks(now=2)['random_mutation']['due_after_terminal_windows'],20)

    def test_due_or_resolved_old_cycle_is_unchanged(self):
        for due, resolved in [(True,False),(False,True)]:
            with self.subTest(due=due), tempfile.TemporaryDirectory() as d:
                p=Path(d)/'state.sqlite3'
                with patch('deadline_harness.secrets.randbelow',side_effect=[0,0]), DeadlineHarness(p) as h:
                    self.complete(h,1)
                    h.connection.execute('UPDATE random_mutation_cycles SET interval_windows=10,due_after_terminal_windows=10,cadence_version=2,due_task_id=?,resolution_evidence=?,ordinary_resolution_evidence=?',('t-0' if due else None,'historical resolution' if resolved else None,'historical resolution' if resolved else None))
                    h.connection.commit();before=tuple(h.connection.execute('SELECT * FROM random_mutation_cycles').fetchone())
                with DeadlineHarness(p) as h:
                    self.assertEqual(tuple(h.connection.execute('SELECT * FROM random_mutation_cycles').fetchone()),before)

    def test_elapsed_old_boundary_without_due_marker_is_not_postponed(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'state.sqlite3'
            with patch('deadline_harness.secrets.randbelow',side_effect=[0,0]), DeadlineHarness(p) as h:
                self.complete(h,10)
                h.connection.execute('UPDATE random_mutation_cycles SET interval_windows=10,due_after_terminal_windows=10,cadence_version=2,due_task_id=NULL')
                h.connection.commit()
            with DeadlineHarness(p) as h:
                row=h.connection.execute('SELECT interval_windows,due_after_terminal_windows,cadence_version FROM random_mutation_cycles').fetchone()
                self.assertEqual(tuple(row),(10,10,2))
                self.assertTrue(h.list_tasks(now=2)['random_mutation']['due'])

if __name__ == '__main__': unittest.main()
