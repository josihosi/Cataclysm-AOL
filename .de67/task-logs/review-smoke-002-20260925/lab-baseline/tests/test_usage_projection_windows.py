from datetime import datetime, timezone
import json
from pathlib import Path
import sqlite3
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from usage_projection import usage_projection


def stamp(epoch):
    return datetime.fromtimestamp(epoch, timezone.utc).isoformat().replace('+00:00', 'Z')


def event(kind, payload, timestamp=None):
    result = {'type': kind, 'payload': payload}
    if timestamp is not None:
        result['timestamp'] = timestamp
    return json.dumps(result) + '\n'


def context(turn='turn', model='gpt-6-sol'):
    return event('turn_context', {'turn_id': turn, 'model': model})


def response(rid, epoch, n=10, *, turn='turn', identity='worker'):
    return event('token_usage_record', {
        'thread_id': identity, 'turn_id': turn, 'response_id': rid,
        'usage': {'input_tokens': n, 'cached_input_tokens': 4,
                  'output_tokens': 2, 'total_tokens': n + 2},
        'thread_token_usage': {'input_tokens': 10000, 'cached_input_tokens': 4000,
                               'output_tokens': 2000, 'total_tokens': 12000},
    }, stamp(epoch) if isinstance(epoch, (int, float)) else epoch)


class UsageWindowTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.path = Path(self.tmp.name) / 'worker.jsonl'
        self.db = sqlite3.connect(':memory:')
        self.record = {'id': 'worker', 'rollout_path': str(self.path),
                       'usage_windows': [{'start': 100, 'end': 200}]}

    def tearDown(self):
        self.db.close()
        self.tmp.cleanup()

    def view(self):
        return usage_projection(self.db, {'records': [self.record]},
                                root_id='new-coordinator', details=True)

    def write(self, text):
        self.path.write_text(text, encoding='utf-8', newline='')

    def test_old_worker_lifetime_excluded_and_own_current_models_preserved(self):
        self.write(context('old', 'gpt-6-luna') + response('old', 50, 100, turn='old')
                   + context() + response('new', 150, 20)
                   + response('rollup', 160, 1000, identity='helper')
                   + context('sol', 'gpt-6-sol') + response('new-sol', 170, 30, turn='sol'))
        result = self.view()
        self.assertTrue(result['complete'])
        self.assertNotIn('gpt-6-luna', result['by_model'])
        self.assertEqual(result['by_model']['gpt-6-sol']['total_tokens'], 54)
        self.assertEqual(result['by_model']['gpt-6-sol']['uncached_input_plus_output_tokens'], 46)
        source = result['sources'][0]
        self.assertEqual(source['usage_windows'], self.record['usage_windows'])
        self.assertEqual(source['response_count'], 2)
        self.assertEqual(source['source_response_count'], 3)
        self.assertEqual(source['excluded_response_count'], 1)
        self.assertEqual(source['window_started_at'], stamp(100))
        self.assertEqual(source['counters_observed_at'], stamp(170))
        self.assertEqual(self.view()['new_source_bytes_read'], 0)

    def test_disjoint_overlapping_windows_and_duplicate_response_count_union_once(self):
        self.record['usage_windows'] = [
            {'start': 100, 'end': 200}, {'start': 150, 'end': 180},
            {'start': 300, 'end': 400},
        ]
        self.write(context() + response('first', 100) + response('overlap', 160)
                   + response('overlap', 160) + response('gap', 250, 1000)
                   + response('second', 350) + response('end', 400, 1000))
        result = self.view()
        self.assertTrue(result['complete'])
        self.assertEqual(result['by_model']['gpt-6-sol']['total_tokens'], 36)
        self.assertEqual(result['by_model']['gpt-6-sol']['session_count'], 1)
        self.assertEqual(result['sources'][0]['response_count'], 3)
        self.assertEqual(result['sources'][0]['excluded_response_count'], 2)

    def test_reprojection_uses_cached_records_and_open_window_ingests_only_suffix(self):
        self.write(context() + response('old', 50) + response('current', 150))
        self.assertEqual(self.view()['by_model']['gpt-6-sol']['total_tokens'], 12)
        self.record['usage_windows'] = [{'start': 0, 'end': 100}]
        result = self.view()
        self.assertEqual(result['new_source_bytes_read'], 0)
        self.assertEqual(result['by_model']['gpt-6-sol']['total_tokens'], 12)
        self.assertEqual(result['sources'][0]['counters_observed_at'], stamp(50))
        self.record['usage_windows'] = [{'start': 100, 'end': None}]
        suffix = response('next', 300, 20)
        with self.path.open('a', encoding='utf-8', newline='') as stream:
            stream.write(suffix)
        result = self.view()
        self.assertTrue(result['complete'])
        self.assertEqual(result['new_source_bytes_read'], len(suffix.encode('utf-8')))
        self.assertEqual(result['by_model']['gpt-6-sol']['total_tokens'], 34)

    def test_unknown_missing_invalid_or_naive_timestamps_are_excluded_gaps(self):
        self.write(context() + response('known', 150) + response('missing', None, 100)
                   + response('invalid', 'not-a-time', 100)
                   + response('naive', '1970-01-01T00:02:30', 100))
        result = self.view()
        self.assertFalse(result['complete'])
        self.assertEqual(result['by_model']['gpt-6-sol']['total_tokens'], 12)
        self.assertEqual(result['sources'][0]['response_count'], 1)
        self.assertEqual(result['sources'][0]['unknown_timestamp_response_count'], 3)
        self.assertFalse(result['sources'][0]['complete'])

    def test_timezone_offset_and_half_open_bounds_use_epoch_comparison(self):
        self.write(context() + response('before', 99) + response('start', 100)
                   + response('offset', '1970-01-01T01:02:30+01:00')
                   + response('end', 200))
        result = self.view()
        self.assertTrue(result['complete'])
        self.assertEqual(result['by_model']['gpt-6-sol']['total_tokens'], 24)
        self.assertEqual(result['sources'][0]['response_count'], 2)

    def test_invalid_window_is_a_gap_without_unbounded_fallback(self):
        self.record['usage_windows'] = [{'start': 100}, {'start': 200, 'end': 100}]
        self.write(context() + response('old', 50) + response('current', 150))
        result = self.view()
        self.assertFalse(result['complete'])
        self.assertEqual(result['by_model'], {})
        self.assertEqual(result['sources'][0]['invalid_usage_windows'], 2)
        self.assertEqual(result['sources'][0]['response_count'], 0)

    def test_native_record_without_windows_keeps_lifetime_accounting(self):
        del self.record['usage_windows']
        self.write(context() + response('old', 50, 20) + response('new', 150))
        result = self.view()
        self.assertTrue(result['complete'])
        self.assertEqual(result['by_model']['gpt-6-sol']['total_tokens'], 34)
        self.assertEqual(result['sources'][0]['response_count'], 2)
        self.assertNotIn('usage_windows', result['sources'][0])


if __name__ == '__main__':
    unittest.main()
