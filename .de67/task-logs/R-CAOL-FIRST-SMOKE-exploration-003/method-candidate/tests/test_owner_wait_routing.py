"""Owner decision text is evidence, not an executable worker instruction."""
from pathlib import Path
import json
import sqlite3
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from deadline_harness import DeadlineHarness, DeadlineError
import policy_kernel as kernel


class OwnerWaitTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.workspace = Path(self.tmp.name)
        self.state = self.workspace / 'state.sqlite3'
        (self.workspace / '.de67').mkdir()
        (self.workspace / '.de67/FS.md').write_text('- [ ] 🔴 R-031\n- [ ] 🔴 R-029\n')
        self.ledger = self.workspace / '.de67/work-ledger.md'
        self.ledger.write_text('## Current delivery frontier\n- Active work: `G` proof boundary; required mechanism.\n')
        self.h = DeadlineHarness(self.state)
        self.addCleanup(self.h.close)
        self.h.start_task('project', 'explore', 'R-031', 1000, now=0)
        self.h.complete_task('project', 'explore', 'finite observation known', now=1)
        self.h.transition_claim_to_closure('project', 'R-031', 'explore', 'Verify provenance', 'Inspect records',
                                         gaps=[('G', 'Inspect provenance', 'Observe immutable records')], now=2)
        self.h.start_task('project', 'proof', 'R-031', 1000, phase='closure', gap_id='G', now=3)
        self.h.complete_task('project', 'proof', 'Josef must choose A or B; no replay', now=4)
        self.task_before = dict(self.h.connection.execute("SELECT * FROM tasks WHERE task_id='proof'").fetchone())

    def wait(self):
        return self.h.revise_closure_gap('project', 'R-031', 'G', 'proof', 'Josef-only A/B decision',
            'A accept the bounded result with explicit missing provenance; B authorize fresh evidence. No replay meanwhile.',
            route_kind='owner_wait', now=5)

    def facts(self):
        return kernel.workspace_facts(self.workspace, self.state, 'project', now=6)

    def test_nonempty_wait_consumes_once_and_does_not_dispatch(self):
        self.assertIn('worker_completed', self.facts())
        result = self.wait()
        self.assertEqual(result['revision'], 2)
        self.assertEqual(result['route_kind'], 'owner_wait')
        facts = self.facts()
        self.assertIn('owner_wait', facts)
        self.assertNotIn('worker_completed', facts)
        self.assertNotIn('closure_ready', facts)
        self.assertNotIn('executable_route', facts)
        with self.assertRaisesRegex(DeadlineError, 'owner decision'):
            self.h.start_task('project', 'wrong-worker', 'R-031', 1000, phase='closure', gap_id='G', now=7)
        self.assertEqual(self.task_before, dict(self.h.connection.execute("SELECT * FROM tasks WHERE task_id='proof'").fetchone()))
        self.assertEqual(self.h.list_tasks(now=7)['closure_gaps'][0]['route_kind'], 'owner_wait')

    def test_independent_active_claim_is_executable_while_owner_waits(self):
        self.wait()
        self.ledger.write_text('## Current delivery frontier\n- Active work: `G` owner decision.\n\n'
                              '- [ ] R-031 — Waiting\n  - Assignment wait: proof boundary.\n'
                              '- [ ] R-029 — Observe independent route\n  - Assignment R-029-observe: capture the independent observation.\n')
        facts = self.facts()
        self.assertIn('executable_route', facts)
        self.assertNotIn('closure_ready', facts)
        policy = json.loads((ROOT/'assets/environment/phase3-policy.json').read_text())
        self.assertEqual(kernel.decide(policy, facts).action, 'dispatch_exploration_worker')
        self.h.start_task('project', 'independent', 'R-029', 100, now=7)

    def test_retired_clock_ledger_fallback_does_not_reanimate_wait(self):
        self.wait()
        self.h.connection.execute("UPDATE claim_deadline_generations SET retired_at=5, retirement_reason='test quiet restart'")
        self.h.connection.commit()
        self.assertNotIn('executable_route', self.facts())
        self.assertNotIn('closure_ready', self.facts())

    def test_executable_observation_and_repair_keep_dispatching(self):
        self.h.revise_closure_gap('project', 'R-031', 'G', 'proof', 'Recoverable route', 'Observe source records', now=5)
        self.assertIn('executable_route', self.facts())
        self.assertIn('closure_ready', self.facts())
        self.assertNotIn('worker_completed', self.facts())
        self.h.start_task('project', 'repair', 'R-031', 1000, phase='closure', gap_id='G', now=7)
        self.h.complete_task('project', 'repair', 'changed repair route', now=8)
        self.h.revise_closure_gap('project', 'R-031', 'G', 'repair', 'Repair route', 'Repair immutable lookup then test', now=9)
        self.assertIn('executable_route', self.facts())
        self.assertIn('closure_ready', self.facts())

    def test_consumption_requires_exact_successor_identity(self):
        self.wait()
        task = self.h.connection.execute("SELECT * FROM tasks WHERE task_id='proof'").fetchone()
        self.assertTrue(kernel._terminal_result_was_consumed(self.h.connection, 'project', task))
        # Deliberately malformed read-only source projections test the consumer;
        # the real append-only lifecycle is never bypassed.
        for column, value in [('revision', 9), ('gap_id', 'other'), ('claim_id', 'other'), ('closure_sequence', 9), ('basis_task_id', 'other'), ('recorded_at', 3)]:
            with self.subTest(column=column), sqlite3.connect(':memory:') as c:
                c.row_factory = sqlite3.Row
                columns = [row[1] for row in self.h.connection.execute('PRAGMA table_info(closure_gap_revisions)')]
                c.execute('CREATE TABLE closure_gap_revisions (' + ','.join(columns) + ')')
                values = list(self.h.connection.execute('SELECT * FROM closure_gap_revisions WHERE revision=2').fetchone())
                values[columns.index(column)] = value
                c.execute('INSERT INTO closure_gap_revisions VALUES (' + ','.join('?' for _ in columns) + ')', values)
                self.assertFalse(kernel._terminal_result_was_consumed(c, 'project', task))

    def test_wait_release_requires_explicit_owner_decision_and_preserves_terminal(self):
        self.wait()
        with self.assertRaisesRegex(DeadlineError, 'explicit owner decision'):
            self.h.revise_closure_gap('project', 'R-031', 'G', 'proof', 'fresh evidence', 'run evidence', now=6)
        result = self.h.revise_closure_gap('project', 'R-031', 'G', 'proof', 'Fresh source-bound evidence',
            'Run newly authorized evidence', owner_decision='Fixture owner explicitly chose B', now=7)
        self.assertEqual(result['revision'], 3)
        self.h.start_task('project', 'authorized', 'R-031', 1000, phase='closure', gap_id='G', now=8)
        self.assertEqual(self.task_before, dict(self.h.connection.execute("SELECT * FROM tasks WHERE task_id='proof'").fetchone()))

    def test_same_gap_label_in_an_independent_claim_is_not_the_same_identity(self):
        self.wait()
        self.h.start_task('project', 'explore-other', 'R-029', 1000, now=7)
        self.h.complete_task('project', 'explore-other', 'independent observation', now=8)
        self.h.transition_claim_to_closure('project', 'R-029', 'explore-other', 'Observe independent route',
            'Run independent observation', gaps=[('G', 'Independent G', 'Observe independently')], now=9)
        self.assertIn('closure_ready', self.facts())
        self.assertIn('executable_route', self.facts())
        self.h.start_task('project', 'other-G', 'R-029', 1000, phase='closure', gap_id='G', now=10)

    def test_mixed_same_claim_gaps_preserve_independent_execution(self):
        self.wait()
        self.h.connection.execute("INSERT INTO closure_gaps (lineage_id,claim_id,closure_sequence,gap_id,opened_at,basis_task_id) VALUES ('project','R-031',2,'H',2,'explore')")
        self.h.connection.execute("INSERT INTO closure_gap_revisions (lineage_id,claim_id,closure_sequence,gap_id,revision,recorded_at,basis_task_id,description,proof_route) VALUES ('project','R-031',2,'H',1,2,'explore','Independent observation','Observe H')")
        self.h.connection.commit()
        self.assertIn('executable_route', self.facts())
        self.assertIn('closure_ready', self.facts())
        self.h.start_task('project', 'independent-gap', 'R-031', 1000, phase='closure', gap_id='H', now=7)

if __name__ == '__main__':
    unittest.main()
