from contextlib import closing
from datetime import datetime, timezone
import json
from pathlib import Path
import sqlite3
import sys
import tempfile
import unittest
from unittest.mock import patch

SCRIPTS = Path(__file__).resolve().parents[1] / 'scripts'
sys.path.insert(0, str(SCRIPTS))
import codex_runner
import coordinator_supervisor
from deadline_harness import DeadlineHarness
from work_context import token_usage_view


def command_completed(result):
    return {'type': 'item.completed', 'item': {
        'type': 'command_execution', 'status': 'completed', 'exit_code': 0,
        'command': 'python worker_library.py assign',
        'aggregated_output': json.dumps(result),
    }}


def followup_completed(worker):
    return {'type': 'item.completed', 'item': {
        'type': 'collab_tool_call', 'tool': 'followup_task',
        'status': 'completed', 'receiver_thread_ids': [worker],
    }}


def rollout(identity, model, timestamps):
    events = [{'type': 'turn_context', 'payload': {'turn_id': 'turn', 'model': model}}]
    for index, epoch in enumerate(timestamps):
        events.append({
            'type': 'token_usage_record',
            'timestamp': datetime.fromtimestamp(epoch, timezone.utc).isoformat(),
            'payload': {
                'thread_id': identity, 'turn_id': 'turn', 'response_id': f'response-{index}',
                'usage': {'input_tokens': 10, 'cached_input_tokens': 4,
                          'output_tokens': 2, 'total_tokens': 12},
            },
        })
    return ''.join(json.dumps(event) + '\n' for event in events)


class NamedWorkerIntegrationTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.workspace = self.root / 'workspace'
        self.workspace.mkdir()
        self.state = self.root / 'deadlines.sqlite3'
        self.runtime = self.root / 'runtime'
        self.runtime.mkdir()
        self.native_state = self.runtime / 'state_5.sqlite'
        self.environment = {
            'DE67_CODEX_STATE': str(self.native_state),
            'DE67_DEADLINE_STATE': str(self.state),
            'DE67_LINEAGE': 'project',
        }

    def tearDown(self):
        self.tmp.cleanup()

    def test_runner_cli_dispatch_binds_exact_second_task_without_spawn_edge(self):
        ownership = {}
        recorded = []

        def owned(workspace, state, lineage, *, coordinator_session_id):
            self.assertEqual((workspace, state, lineage), (self.workspace, self.state, 'project'))
            return dict(ownership) if coordinator_session_id == 'current-coordinator' else {}

        guard = codex_runner.CoordinatorLoopGuard(
            initial_unbound_tasks=('first-task', 'second-task'),
            roster_resolver=codex_runner._roster_resolver(self.workspace, self.environment),
            roster_validator=codex_runner._roster_validator(self.workspace, self.environment),
            claim_recorder=lambda *binding: recorded.append(binding),
            clock=lambda: 100,
        )
        self.assertFalse(self.native_state.exists())
        with patch('worker_library.owned_assignments', side_effect=owned):
            guard.observe({'type': 'thread.started', 'thread_id': 'current-coordinator'})
            # A command's text/receiver is insufficient before durable ownership exists.
            dispatch = command_completed({'task_id': 'second-task', 'worker_id': 'persistent-worker',
                                          'state': 'assigned'})
            guard.observe(dispatch)
            self.assertEqual(guard.unbound_tasks, ('first-task', 'second-task'))
            self.assertEqual(recorded, [])
            ownership['second-task'] = 'persistent-worker'
            guard.observe(dispatch)
            self.assertEqual(guard.unbound_tasks, ('first-task',))
            self.assertEqual(recorded, [('second-task', 'persistent-worker', 'current-coordinator')])
            guard.observe({'type': 'turn.completed'})
            self.assertEqual(len(recorded), 1)

    def test_runner_library_validation_rejects_other_task_coordinator_and_used_worker(self):
        def owned(_workspace, _state, _lineage, *, coordinator_session_id):
            return {'exact-task': 'persistent-worker'} if coordinator_session_id == 'owner' else {}

        resolve = codex_runner._roster_resolver(self.workspace, self.environment)
        validate = codex_runner._roster_validator(self.workspace, self.environment)
        with patch('worker_library.owned_assignments', side_effect=owned):
            self.assertEqual(resolve('exact-task', 100, 'owner', frozenset()), 'persistent-worker')
            self.assertIsNone(resolve('other-task', 100, 'owner', frozenset()))
            self.assertIsNone(resolve('exact-task', 100, 'other-coordinator', frozenset()))
            self.assertIsNone(resolve('exact-task', 100, 'owner', frozenset({'persistent-worker'})))
            self.assertTrue(validate('persistent-worker', 'owner', 'exact-task'))
            self.assertFalse(validate('persistent-worker', 'owner', 'other-task'))
            self.assertFalse(validate('persistent-worker', 'other-coordinator', 'exact-task'))
            self.assertFalse(validate('unrelated-worker', 'owner', 'exact-task'))

    def test_persistent_followup_receiver_cannot_take_first_queued_task(self):
        recorded = []
        guard = codex_runner.CoordinatorLoopGuard(
            initial_unbound_tasks=('first-task', 'second-task'),
            roster_resolver=codex_runner._roster_resolver(self.workspace, self.environment),
            roster_validator=codex_runner._roster_validator(self.workspace, self.environment),
            claim_recorder=lambda *binding: recorded.append(binding),
        )
        with patch('worker_library.owned_assignments', return_value={'second-task': 'persistent-worker'}):
            guard.observe({'type': 'thread.started', 'thread_id': 'owner'})
            guard.observe(followup_completed('persistent-worker'))
            self.assertEqual(guard.unbound_tasks, ('first-task', 'second-task'))
            self.assertEqual(recorded, [])
            guard.observe({'type': 'turn.completed'})
            self.assertEqual(guard.unbound_tasks, ('first-task',))
            self.assertEqual(recorded, [('second-task', 'persistent-worker', 'owner')])

    def test_supervisor_preserves_verified_library_owners_without_native_database(self):
        self.assertFalse(self.native_state.exists())
        with patch('worker_library.worker_owners', return_value={'persistent-worker': 'current'}) as owners:
            result = coordinator_supervisor.runtime_worker_owners(
                self.workspace, self.environment, state_path=self.state, lineage_id='project')
        owners.assert_called_once_with(self.workspace, self.state, 'project')
        self.assertEqual(result, {'persistent-worker': 'current'})

    def test_supervisor_merges_library_owners_with_native_edges_and_current_owner_wins(self):
        with closing(sqlite3.connect(self.native_state)) as db, db:
            db.executescript('CREATE TABLE threads(id TEXT,cwd TEXT,model TEXT); '
                             'CREATE TABLE thread_spawn_edges(parent_thread_id TEXT,child_thread_id TEXT);')
            db.executemany('INSERT INTO threads VALUES (?,?,?)', [
                ('native-worker', str(self.workspace), 'gpt-6-luna'),
                ('persistent-worker', str(self.workspace), 'gpt-6-sol'),
                ('unrelated-worker', str(self.root / 'elsewhere'), 'gpt-6-luna'),
            ])
            db.executemany('INSERT INTO thread_spawn_edges VALUES (?,?)', [
                ('native-parent', 'native-worker'), ('old-parent', 'persistent-worker'),
                ('unrelated-parent', 'unrelated-worker'),
            ])
        with patch('worker_library.worker_owners', return_value={'persistent-worker': 'current-parent'}):
            result = coordinator_supervisor.runtime_worker_owners(
                self.workspace, self.environment, state_path=self.state, lineage_id='project')
        self.assertEqual(result, {'native-worker': 'native-parent', 'persistent-worker': 'current-parent'})

    def usage_fixture(self):
        with DeadlineHarness(self.state) as harness:
            harness.start_task('project', 'prior-task', 'prior-claim', 1000, now=9)
            harness.claim_worker('project', 'prior-task', 'persistent-worker', 'old-coordinator', 'supervisor', now=10)
            harness.release_worker_claim('project', 'prior-task', 'persistent-worker', 'finished prior assignment', now=60)
            harness.start_task('project', 'first-task', 'first-claim', 1000, now=99)
            harness.claim_worker('project', 'first-task', 'persistent-worker', 'current-coordinator', 'supervisor', now=100)
            harness.release_worker_claim('project', 'first-task', 'persistent-worker', 'finished first assignment', now=200)
            harness.start_task('project', 'second-task', 'second-claim', 1000, now=299)
            harness.claim_worker('project', 'second-task', 'persistent-worker', 'current-coordinator', 'supervisor', now=300)
        sessions = [
            ('old-coordinator', 'gpt-6-sol', [50]),
            ('current-coordinator', 'gpt-6-sol', [250]),
            ('native-helper', 'native-helper-model', [250]),
            ('persistent-worker', 'gpt-6-sol', [50, 150, 250, 350]),
            ('worker-helper', 'gpt-6-luna', [50, 150, 250, 350]),
            ('nested-helper', 'nested-helper-model', [50, 150, 250, 350]),
            ('unrelated-worker', 'unrelated-model', [150]),
        ]
        with closing(sqlite3.connect(self.native_state)) as db, db:
            db.executescript('CREATE TABLE threads(id TEXT,rollout_path TEXT); '
                             'CREATE TABLE thread_spawn_edges(parent_thread_id TEXT,child_thread_id TEXT);')
            for identity, model, timestamps in sessions:
                trace = self.runtime / (identity + '.jsonl')
                trace.write_text(rollout(identity, model, timestamps), encoding='utf-8', newline='')
                db.execute('INSERT INTO threads VALUES (?,?)', (identity, str(trace)))
            db.executemany('INSERT INTO thread_spawn_edges VALUES (?,?)', [
                ('current-coordinator', 'native-helper'),
                ('persistent-worker', 'worker-helper'), ('worker-helper', 'nested-helper'),
            ])

    def test_usage_discovers_independent_worker_and_helpers_with_only_selected_claim_windows(self):
        self.usage_fixture()
        result = token_usage_view(self.workspace, self.state, 'project', details=True, codex_home=self.runtime)
        self.assertTrue(result['complete'], result)
        self.assertEqual(result['root_session_id'], 'current-coordinator')
        self.assertEqual(result['session_count'], 5)
        self.assertEqual(result['by_model']['gpt-6-sol']['total_tokens'], 36)
        self.assertEqual(result['by_model']['native-helper-model']['total_tokens'], 12)
        self.assertNotIn('unrelated-model', result['by_model'])
        sources = {source['session_id']: source for source in result['sources']}
        for worker, model in [('persistent-worker', 'gpt-6-sol'),
                              ('worker-helper', 'gpt-6-luna'), ('nested-helper', 'nested-helper-model')]:
            self.assertEqual(result['by_model'][model]['total_tokens'], 36 if model == 'gpt-6-sol' else 24)
            self.assertEqual(sources[worker]['usage_windows'], [{'start': 100, 'end': 200},
                                                               {'start': 300, 'end': None}])
            self.assertEqual(sources[worker]['response_count'], 2)
            self.assertEqual(sources[worker]['source_response_count'], 4)
        self.assertNotIn('usage_windows', sources['native-helper'])
        self.assertEqual(token_usage_view(self.workspace, self.state, 'project',
                                         codex_home=self.runtime)['new_source_bytes_read'], 0)
        prior = token_usage_view(self.workspace, self.state, 'project', task='prior-task',
                                 details=True, codex_home=self.runtime)
        self.assertTrue(prior['complete'], prior)
        self.assertEqual(prior['root_session_id'], 'old-coordinator')
        self.assertEqual(prior['by_model']['gpt-6-sol']['total_tokens'], 24)
        self.assertEqual(prior['by_model']['gpt-6-luna']['total_tokens'], 12)
        self.assertNotIn('native-helper-model', prior['by_model'])
        selected = token_usage_view(self.workspace, self.state, 'project', task='second-task',
                                    codex_home=self.runtime)
        self.assertEqual(selected['by_model']['gpt-6-sol']['total_tokens'], 36)
        self.assertEqual(selected['new_source_bytes_read'], 0)

    def test_usage_marks_unavailable_durable_worker_helper_as_gap(self):
        self.usage_fixture()
        with closing(sqlite3.connect(self.native_state)) as db, db:
            db.execute('DELETE FROM threads WHERE id=?', ('nested-helper',))
        result = token_usage_view(self.workspace, self.state, 'project', details=True, codex_home=self.runtime)
        self.assertFalse(result['complete'])
        self.assertIn('nested-helper', result['unavailable_thread_ids'])
        self.assertEqual(result['by_model']['gpt-6-sol']['total_tokens'], 36)
        self.assertEqual(result['by_model']['gpt-6-luna']['total_tokens'], 24)


if __name__ == '__main__':
    unittest.main()
