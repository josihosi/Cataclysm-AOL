from __future__ import annotations

import importlib.util
from pathlib import Path
import sys
import unittest

SCRIPT = Path('/Users/josefhorvath/.codex/skills/de67/de-67-3/scripts')
sys.path.insert(0, str(SCRIPT))
HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('worker_library_candidate', HERE / 'worker_library.candidate.py')
candidate = importlib.util.module_from_spec(spec)
spec.loader.exec_module(candidate)

TESTS = Path('/Users/josefhorvath/.codex/skills/de67/de-67-3/tests')
sys.path.insert(0, str(TESTS))
import test_worker_library as original
original.library = candidate


class ReleasedRecovery(original.WorkerFixture, unittest.TestCase):
    def prepare_released(self, turns, status='idle'):
        self.worker(model='gpt-6-luna', effort='medium')
        self.assign()
        self.dispatcher.process_pending()
        assignment = candidate.describe(self.workspace, 'pilot')['assignment']
        worker_id = assignment['worker_id']
        self.harness.release_worker_claim('project', 'task-a', worker_id, 'retired owner')
        Path(self.binding['socket']).unlink()
        self.binding = self.bind('sol-b', 'run-b', 'supervisor-b')
        old_call = self.rpc.call

        def call(method, params):
            if method == 'thread/read':
                return {'thread': {'id': worker_id, 'cwd': str(self.workspace),
                                   'status': {'type': status}, 'turns': turns}}
            if method == 'thread/unsubscribe':
                return {}
            return old_call(method, params)

        self.rpc.call = call
        self.dispatcher = candidate.WorkerDispatcher(self.workspace, self.rpc, self.binding)
        return assignment

    def test_completed_exact_turn_is_archived_without_replay(self):
        old = self.prepare_released([])
        turn = {'id': old['turn_id'], 'status': 'completed', 'items': [
            {'type': 'agentMessage', 'id': 'final', 'phase': 'final', 'text': 'Observed camp UI'}]}
        self.rpc.call = self._with_turn(self.rpc.call, turn)
        previous_starts = self.rpc.turn_count
        self.dispatcher.reconcile()
        current = candidate.describe(self.workspace, 'pilot')['assignment']
        self.assertEqual(current['status'], 'returned')
        self.assertIn('Observed camp UI', Path(current['result_path']).read_text())
        self.assertEqual(self.rpc.turn_count, previous_starts)
        self.assertIsNotNone(candidate._task(self.state, 'project', 'task-a')['released_at'])
        self.dispatcher.reconcile()
        self.assertEqual(candidate.describe(self.workspace, 'pilot')['assignment']['result_path'], current['result_path'])

    def test_ambiguous_unfinished_turn_is_preserved_without_replay(self):
        old = self.prepare_released([])
        self.rpc.call = self._with_turn(self.rpc.call, {'id': old['turn_id'], 'status': 'inProgress', 'items': []})
        previous_starts = self.rpc.turn_count
        self.dispatcher.reconcile()
        current = candidate.describe(self.workspace, 'pilot')['assignment']
        self.assertEqual(current['status'], 'interrupted')
        self.assertIsNone(current['result_path'])
        self.assertIn('uncertain', current['error'])
        self.assertEqual(self.rpc.turn_count, previous_starts)

    def test_live_old_server_is_not_adopted(self):
        old = self.prepare_released([])
        old_socket = self.workspace / 'run-a.sock'
        old_socket.touch()
        import unittest.mock
        with unittest.mock.patch.object(candidate, '_old_server_present', return_value=True):
            self.dispatcher.reconcile()
        current = candidate.describe(self.workspace, 'pilot')['assignment']
        self.assertEqual(current['status'], 'running')
        self.assertEqual(current['turn_id'], old['turn_id'])

    def test_unreleased_live_owner_is_not_adopted(self):
        self.worker(model='gpt-6-luna', effort='medium')
        self.assign()
        self.dispatcher.process_pending()
        old = candidate.describe(self.workspace, 'pilot')['assignment']
        Path(self.binding['socket']).unlink()
        self.binding = self.bind('sol-b', 'run-b', 'supervisor-b')
        self.dispatcher = candidate.WorkerDispatcher(self.workspace, self.rpc, self.binding)
        self.dispatcher.reconcile()
        current = candidate.describe(self.workspace, 'pilot')['assignment']
        self.assertEqual(current['status'], 'running')
        self.assertEqual(current['turn_id'], old['turn_id'])
        self.assertIsNone(current['result_path'])

    @staticmethod
    def _with_turn(call, turn):
        def wrapped(method, params):
            if method == 'thread/read':
                thread = call(method, params)['thread']
                thread['turns'] = [turn]
                return {'thread': thread}
            return call(method, params)
        return wrapped


if __name__ == '__main__':
    unittest.main()
