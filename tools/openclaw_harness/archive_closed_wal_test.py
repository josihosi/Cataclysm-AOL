"""Closed WAL evidence remains readable without changing retained sources."""
import hashlib
import json
from pathlib import Path
import sqlite3
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

import cockpit_archive
from cockpit_archive import Archive, resolve_wire
from cockpit_evidence import select
from cockpit_file_bridge import FileBackedCockpitBridge as Bridge
import evidence_events
from evidence_display import retain


class ClosedWalTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name).resolve()
        pid = int(subprocess.check_output([sys.executable, '-c', 'import os; print(os.getpid())']))
        (self.directory / 'status.json').write_text(json.dumps({
            'binding_id': 'binding', 'state': 'safe_to_cleanup',
            'child_exit_code': 0, 'bridge_pid': pid}))
        self.path = self.directory / 'cockpit-evidence.sqlite'
        archive = Archive(self.path, run_id='run', binding_id='binding')
        sequence = archive.sequence()
        sequence.append({'kind': 'observation', 'value': {'run_id': 'run', 'game_turn': 42,
            'surface': {'facts': {'visible_entities': [
                {'identity': {'id': 17}, 'name': 'Scout', 'pos': [2, 3, 0]}]}}}})
        self.wire = archive.wire({'ok': True, 'result': {'action_observation_sequence': sequence}})
        archive.connection.execute('PRAGMA wal_checkpoint(TRUNCATE)').fetchall()
        archive.close()
        # Reproduce the retained post-close artifact even on SQLite builds
        # that persist empty sidecars after the final connection closes.
        Path(str(self.path) + '-wal').unlink(missing_ok=True)
        Path(str(self.path) + '-shm').unlink(missing_ok=True)
        self.assertFalse(Path(str(self.path) + '-wal').exists())
        responses = self.directory / 'responses'
        responses.mkdir()
        self.response_path = responses / 'request.json'
        raw = json.dumps(self.wire).encode()
        self.response_path.write_bytes(raw)
        self.digest = hashlib.sha256(raw).hexdigest()
        (responses / 'request.receipt.json').write_text(json.dumps({
            'binding_id': 'binding', 'request_id': 'request', 'response_sha256': self.digest,
            'response_artifact': 'responses/request.json'}))

    def force_readonly_failure(self):
        connect = sqlite3.connect
        class Unreadable:
            def execute(self, *args):
                error = sqlite3.OperationalError('unable to open database file')
                raise error
            def close(self):
                pass
        def open_connection(database, **kwargs):
            if database == self.path.as_uri() + '?mode=ro':
                return Unreadable()
            return connect(database, **kwargs)
        return patch.object(cockpit_archive.sqlite3, 'connect', side_effect=open_connection)

    def query(self):
        with patch.object(evidence_events, 'retain',
                          side_effect=lambda value: retain(value, self.directory / 'display')):
            return evidence_events.query([
                {'path': str(self.response_path), 'producer': 'cockpit', 'response': True}],
                {'run_id': 'run', 'actor_id': 17}, selectors=['game_time.turn', 'payload.pos'])

    def test_closed_archive_projection_matches_exact_recovery_and_cleans_snapshot(self):
        before = self.path.read_bytes()
        with self.force_readonly_failure():
            result = Bridge.response_artifact(self.directory, 'request', self.digest)
            self.assertTrue(result['ok'], result)
            sequence = result['response']['result']['action_observation_sequence']
            snapshot = Path(sequence.archive._readonly_snapshot.name)
            self.assertTrue(snapshot.exists())
            self.assertEqual(select(sequence[0], 'value.game_turn'), 42)
            query = self.query()
            self.assertEqual(query['status'], 'matched')
            self.assertEqual(query['matched'], 1)
            self.assertEqual(query['rows'][0]['fields'],
                             {'game_time.turn': 42, 'payload.pos': [2, 3, 0]})
            self.assertEqual(query['rows'][0]['source']['sha256'], self.digest)
            sequence.archive.close()
            self.assertFalse(snapshot.exists())
        self.assertEqual(self.path.read_bytes(), before)
        self.assertFalse(Path(str(self.path) + '-wal').exists())

    def test_non_posix_does_not_signal_a_process(self):
        with patch.object(cockpit_archive.os, 'name', 'nt'), patch.object(cockpit_archive.os, 'kill') as kill:
            self.assertFalse(cockpit_archive._closed_bridge_archive(self.path, 'binding'))
            kill.assert_not_called()

    def test_live_or_unproven_lifecycle_does_not_enable_fallback(self):
        status_path = self.directory / 'status.json'
        closed = json.loads(status_path.read_bytes())
        import os
        for changes in ({'bridge_pid': os.getpid()}, {'state': 'ready'},
                        {'binding_id': 'other'}, {'child_exit_code': None}):
            with self.subTest(changes=changes):
                status_path.write_text(json.dumps({**closed, **changes}))
                with self.force_readonly_failure():
                    result = self.query()
                self.assertEqual(result['status'], 'partial')
                self.assertTrue(result['unavailable_sources'])

    def test_existing_wal_is_never_ignored(self):
        writer = Archive(self.path, run_id='run', binding_id='binding')
        try:
            writer.sequence().append({'new': True})
            self.assertTrue(Path(str(self.path) + '-wal').exists())
            with self.force_readonly_failure():
                result = self.query()
            self.assertEqual(result['status'], 'partial')
            self.assertTrue(result['unavailable_sources'])
        finally:
            writer.close()

    def test_wal_appearing_during_copy_rejects_snapshot(self):
        copyfile = cockpit_archive.shutil.copyfile
        wal = Path(str(self.path) + '-wal')
        def copy_then_wal(source, destination):
            copyfile(source, destination)
            wal.write_bytes(b'concurrent writer')
        with self.force_readonly_failure(), patch.object(cockpit_archive.shutil, 'copyfile', copy_then_wal):
            result = self.query()
        self.assertEqual(result['status'], 'partial')
        self.assertTrue(result['unavailable_sources'])

    def test_missing_and_corrupt_archives_are_incomplete_not_no_match(self):
        for state in ('missing', 'corrupt'):
            with self.subTest(state=state):
                if state == 'missing':
                    self.path.unlink()
                else:
                    self.path.write_bytes(b'not a SQLite database')
                result = self.query()
                self.assertEqual(result['status'], 'partial')
                self.assertEqual(result['matched'], 0)
                self.assertTrue(result['unavailable_sources'])

    def test_changed_binding_and_changed_stream_still_refuse(self):
        with self.force_readonly_failure():
            with self.assertRaisesRegex(ValueError, 'binding_mismatch'):
                resolve_wire(self.wire, directory=self.directory, binding_id='other')
        writer = Archive(self.path, run_id='run', binding_id='binding')
        with writer.connection:
            writer.connection.execute("UPDATE records SET value='42'")
        writer.connection.execute('PRAGMA wal_checkpoint(TRUNCATE)').fetchall()
        writer.close()
        Path(str(self.path) + '-wal').unlink(missing_ok=True)
        Path(str(self.path) + '-shm').unlink(missing_ok=True)
        with self.force_readonly_failure():
            result = self.query()
        self.assertEqual(result['status'], 'partial')
        self.assertIn('digest_mismatch', str(result['unavailable_sources']))


if __name__ == '__main__':
    unittest.main()
