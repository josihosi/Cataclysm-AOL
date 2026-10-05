"""Exact native report/observation clocks through the retained event query."""
import copy
import gzip
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import evidence_events


class EvidenceTimeTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.fixture = json.loads(gzip.decompress(
            (Path(__file__).parent / 'fixtures/controls/r067_report_native_time.json.gz').read_bytes()))
        cls.source = {'producer': 'native', 'path': 'original', 'sha256': 'bound', 'offset': 0}

    def test_original_report_provenance_is_not_reconstructed_service(self):
        record = copy.deepcopy(self.fixture['report_transition'])
        before = copy.deepcopy(record)
        row = next(evidence_events.envelopes(record, self.source))
        self.assertEqual(row['game_time']['minutes'], 8100)
        self.assertEqual(row['time_roles']['recorded_role'], 'report_delivery')
        self.assertEqual(row['time_roles']['report_delivery']['minutes'], 8100)
        self.assertFalse(row['time_roles']['service']['available'])
        self.assertIsNone(row['time_roles']['service']['minutes'])
        self.assertFalse(row['time_roles']['observation']['available'])
        self.assertEqual(record, before)
        unrelated = {**record, 'domain': 'unrelated_dialogue'}
        other = next(evidence_events.envelopes(unrelated, self.source))
        self.assertEqual(other['time_roles']['recorded_role'], 'unclassified')
        self.assertFalse(other['time_roles']['report_delivery']['available'])

    def test_actual_native_frame_is_observation_not_neighbouring_service(self):
        row = next(evidence_events.envelopes(self.fixture['native_frame'], self.source))
        self.assertEqual(row['time_roles']['observation']['minutes'], 8160)
        self.assertEqual(row['time_roles']['observation']['turn'], 5241651)
        self.assertEqual(row['time_roles']['recorded_role'], 'observation')
        self.assertFalse(row['time_roles']['service']['available'])
        self.assertFalse(row['time_roles']['report_delivery']['available'])

    def test_only_explicit_service_clock_is_published_with_unknown_type_controls(self):
        record = {**self.fixture['report_transition'], 'service_minutes': 8160,
                  'service_turn': 5241651}
        row = next(evidence_events.envelopes(record, self.source))
        self.assertEqual(row['time_roles']['service']['minutes'], 8160)
        self.assertEqual(row['time_roles']['report_delivery']['minutes'], 8100)
        for value in (True, '8160', -1, None):
            with self.subTest(value=value):
                row = next(evidence_events.envelopes(
                    {**record, 'service_minutes': value, 'service_turn': None}, self.source))
                self.assertFalse(row['time_roles']['service']['available'])
                self.assertIsNone(row['time_roles']['service']['minutes'])

    def test_query_filters_actual_observation_and_retains_exact_row_handle(self):
        with tempfile.TemporaryDirectory() as temp:
            source = Path(temp) / 'native.jsonl'
            source.write_text('\n'.join(json.dumps(self.fixture[k]) for k in
                                        ('report_transition', 'native_frame')) + '\n')
            with patch.object(evidence_events, 'retain', lambda value: {'sha256': 'query'}):
                result = evidence_events.query(
                    [{'producer': 'native', 'path': str(source)}], {'event': 'surface_descriptor'}, selectors=('time_roles', 'frame_id'),
                    predicates=(('time_roles.observation.minutes', '=', 8160),))
            self.assertEqual(result['matched'], 1)
            self.assertEqual(result['rows'][0]['fields']['time_roles']['observation']['minutes'], 8160)
            self.assertEqual(result['rows'][0]['source']['path'], str(source))
            self.assertEqual(result['rows'][0]['fields']['frame_id'], self.fixture['native_frame']['frame_id'])


if __name__ == '__main__':
    unittest.main()
