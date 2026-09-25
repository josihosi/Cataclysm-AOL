from __future__ import annotations

import hashlib
import json
import sqlite3
import time
import unittest
from contextlib import closing
from unittest.mock import patch

from test_worker_library import WorkerFixture, library
import policy_kernel


class WorkerOwnerFreshnessTests(WorkerFixture, unittest.TestCase):
    def setUp(self):
        super().setUp()
        self.worker()
        self.owner = self.workspace / '.de67/WEC.md'
        self.owner.write_text('<!-- DE67:OWNER-CONTRACT:BEGIN -->\nObserve the route without changing source.\n<!-- DE67:OWNER-CONTRACT:END -->\n', encoding='utf-8')
        (self.workspace / '.de67/FS.md').write_text(
            '<!-- DE67:DFS-SLICE:BEGIN id=R-008-S001 claim=R-008 -->\n'
            '- [ ] R-008 — Observe the route at its available evidence ceiling.\n'
            '<!-- DE67:DFS-SLICE:END id=R-008-S001 claim=R-008 -->\n', encoding='utf-8')
        (self.workspace / '.de67/work-ledger.md').write_text(
            '- [ ] R-008 — Observe the route.\n'
            '  - DFS slices: `R-008-S001`\n'
            '  - Assignment owner-task: Interpret the supplied route evidence.\n', encoding='utf-8')
        self.harness.start_task('project', 'owner-task', 'R-008', 3600, now=time.time())

    def prepare(self):
        calls = policy_kernel.unbound_worker_spawns(self.workspace, self.state, 'project')
        self.assertEqual([call['task_id'] for call in calls], ['owner-task'])
        packet = calls[0]['dispatch_packet']
        return self.workspace / packet['path'], packet['sha256']

    def correct_owner(self):
        self.owner.write_text('<!-- DE67:OWNER-CONTRACT:BEGIN -->\nStop source changes. Use only the corrected evidence.\n<!-- DE67:OWNER-CONTRACT:END -->\n', encoding='utf-8')

    def test_owner_change_before_queue_rejects_old_packet_and_accepts_regenerated_one(self):
        packet = self.prepare()
        self.correct_owner()
        with self.assertRaisesRegex(library.WorkerLibraryError, '[Oo]wner'):
            self.assign(task_id='owner-task', packet=packet)
        self.assertEqual(self.rpc.calls, [])
        self.assign(task_id='owner-task', packet=self.prepare())
        self.dispatcher.process_pending()
        delivered = next(params['input'][0]['text'] for method, params in self.rpc.calls
                         if method == 'turn/start')
        self.assertIn('Stop source changes. Use only the corrected evidence.', delivered)

    def test_owner_change_after_queue_rejects_before_loading_or_claiming_worker(self):
        queued = self.assign(task_id='owner-task', packet=self.prepare())
        self.correct_owner()
        self.dispatcher.process_pending()
        self.assertEqual(library.request_status(self.workspace, queued['request_id'])['state'], 'rejected')
        self.assertIsNone(library._task(self.state, 'project', 'owner-task')['worker_id'])
        self.assertEqual(self.rpc.calls, [])
        self.assign(task_id='owner-task', packet=self.prepare())
        self.dispatcher.process_pending()
        self.assertEqual(self.rpc.turn_count, 1)

    def test_recorded_revision_describes_rendered_owner_text_even_if_owner_changes_during_preparation(self):
        rendered_owner = policy_kernel.current_owner_contract(self.workspace)
        record = policy_kernel.record_dispatch

        def changed_before_record(*args, **kwargs):
            self.correct_owner()
            return record(*args, **kwargs)

        with patch.object(policy_kernel, 'record_dispatch', side_effect=changed_before_record):
            packet = self.prepare()
        self.assertIn(rendered_owner, packet[0].read_text(encoding='utf-8'))
        with closing(sqlite3.connect(self.workspace / '.de67/state/work-context.sqlite3')) as db:
            metadata = json.loads(db.execute('SELECT context_json FROM dispatches WHERE task=?',
                                            ('owner-task',)).fetchone()[0])
        self.assertEqual(metadata['owner_contract_sha256'], hashlib.sha256(rendered_owner.encode()).hexdigest())
        with self.assertRaisesRegex(library.WorkerLibraryError, '[Oo]wner'):
            self.assign(task_id='owner-task', packet=packet)

    def test_repreparing_identical_packet_enriches_legacy_owner_metadata(self):
        packet = self.prepare()
        index = self.workspace / '.de67/state/work-context.sqlite3'
        with closing(sqlite3.connect(index)) as db, db:
            metadata = json.loads(db.execute('SELECT context_json FROM dispatches WHERE task=? AND digest=?',
                                            ('owner-task', packet[1])).fetchone()[0])
            expected_owner = metadata.pop('owner_contract_sha256')
            db.execute('UPDATE dispatches SET context_json=? WHERE task=? AND digest=?',
                       (json.dumps(metadata), 'owner-task', packet[1]))
        with self.assertRaisesRegex(library.WorkerLibraryError, '[Oo]wner'):
            self.assign(task_id='owner-task', packet=packet)
        regenerated = self.prepare()
        self.assertEqual(regenerated, packet)
        with closing(sqlite3.connect(index)) as db:
            rows = db.execute('SELECT context_json FROM dispatches WHERE task=? AND digest=?',
                              ('owner-task', packet[1])).fetchall()
        self.assertEqual(len(rows), 1)
        self.assertEqual(json.loads(rows[0][0])['owner_contract_sha256'], expected_owner)
        queued = self.assign(task_id='owner-task', packet=regenerated)
        self.dispatcher.process_pending()
        self.assertEqual(library.request_status(self.workspace, queued['request_id'])['state'], 'submitted')
        self.assertEqual(self.rpc.turn_count, 1)


if __name__ == '__main__':
    unittest.main()
