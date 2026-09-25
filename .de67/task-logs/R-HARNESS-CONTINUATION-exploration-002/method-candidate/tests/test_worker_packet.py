from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from worker_packet import delivery_text, standing_section


class WorkerPacketTests(unittest.TestCase):
    def assert_full(self, current, previous=None):
        rendered, metadata = delivery_text(current, previous)
        self.assertEqual(rendered, current)
        self.assertFalse(metadata['comparison_available'])
        self.assertEqual(metadata['omitted_sections'], [])
        self.assertEqual(metadata['changed_sections'], [])
        self.assertEqual(metadata['full_utf8_bytes'], len(current.encode('utf-8')))
        self.assertEqual(metadata['delivered_utf8_bytes'], len(current.encode('utf-8')))

    def test_fresh_and_legacy_packets_are_delivered_complete(self):
        current = 'Current task.\n' + standing_section('helpers', 'Collect owned helpers before returning.')
        self.assert_full(current)
        self.assert_full(current, 'An older packet without marked sections.\n')
        self.assert_full('Current legacy packet.\n', current)
        self.assert_full('', current)

    def test_unchanged_guidance_omitted_but_new_assignment_owner_and_mailbox_preserved(self):
        standing = standing_section('helpers', 'Collect every helper before returning. ' * 12)
        prior = 'Task old.\n' + standing + 'Owner: inspect only.\nMailbox --from old-task\n'
        before = 'Task new: repair the reproduced defect.\n'
        after = 'Owner correction: implementation is now authorized.\nMailbox --from new-task\n'
        rendered, metadata = delivery_text(before + standing + after, prior)
        self.assertEqual(rendered, before + 'Standing guidance helpers unchanged.\n' + after)
        self.assertNotIn('Task old', rendered)
        self.assertNotIn('Owner: inspect only', rendered)
        self.assertEqual(metadata['omitted_sections'], ['helpers'])
        self.assertEqual(metadata['changed_sections'], [])
        self.assertTrue(metadata['comparison_available'])
        self.assertLess(metadata['delivered_utf8_bytes'], metadata['full_utf8_bytes'])

    def test_changed_mandatory_guidance_is_reinjected_verbatim(self):
        same = standing_section('outcome', 'Finish the assigned outcome.')
        old = standing_section('ownership', 'Stop owned jobs before returning.')
        changed = standing_section('ownership', 'Preserve the owner-retained session; stop only other owned jobs.')
        current = 'New task.\n' + changed + same + 'Current owner correction remains here.\n'
        rendered, metadata = delivery_text(current, old + same)
        self.assertIn(changed, rendered)
        self.assertIn('Current owner correction remains here.', rendered)
        self.assertEqual(metadata['changed_sections'], ['ownership'])
        self.assertEqual(metadata['omitted_sections'], ['outcome'])
        self.assertNotIn('Stop owned jobs before returning.', rendered)

    def test_new_section_and_equal_text_under_another_name_are_not_omitted(self):
        body = 'This mandatory contract belongs to its exact section.'
        prior = standing_section('old-name', body)
        new_section = standing_section('new-name', body)
        rendered, metadata = delivery_text(new_section, prior)
        self.assertEqual(rendered, new_section)
        self.assertEqual(metadata['changed_sections'], ['new-name'])
        self.assertEqual(metadata['omitted_sections'], [])

    def test_section_order_and_outside_text_do_not_control_comparison(self):
        first = standing_section('first', 'First standing contract.')
        second = standing_section('second', 'Second standing contract.')
        current = 'Current prefix.\n' + second + 'Current middle.\n' + first + 'Current suffix.\n'
        rendered, metadata = delivery_text(current, first + 'Old middle.\n' + second)
        self.assertEqual(rendered, 'Current prefix.\nStanding guidance second unchanged.\n'
                         'Current middle.\nStanding guidance first unchanged.\nCurrent suffix.\n')
        self.assertEqual(metadata['omitted_sections'], ['second', 'first'])

    def test_empty_current_section_replaces_previous_content(self):
        current = standing_section('fallback', '')
        rendered, metadata = delivery_text(current, standing_section('fallback', 'Previously needed fallback.'))
        self.assertEqual(rendered, current)
        self.assertEqual(metadata['changed_sections'], ['fallback'])
        self.assertNotIn('Previously needed fallback.', rendered)

    def test_malformed_or_duplicate_markers_in_either_packet_disable_all_omission(self):
        valid = standing_section('helpers', 'Always preserve live ownership.')
        begin = '<!-- worker-standing:begin name="broken" -->\n'
        end = '<!-- worker-standing:end name="broken" -->\n'
        malformed = [
            begin + 'No end marker.\n',
            end,
            begin + '<!-- worker-standing:end name="different" -->\n',
            begin + standing_section('nested', 'Nested contract.') + end,
            standing_section('duplicate', 'One.') + standing_section('duplicate', 'Two.'),
            '<!-- worker-standing:begin name="broken" -- >\n',
            '<!-- worker-standing begin name="broken" -->\n',
            '<!-- worker-standing:begin name="bad name" -->\n',
            'Inline marker ' + standing_section('inline', 'Not renderer-owned layout.'),
        ]
        for broken in malformed:
            with self.subTest(packet=broken):
                self.assert_full(valid + broken, valid)
                self.assert_full(valid, valid + broken)

    def test_metadata_counts_actual_utf8_bytes_without_claiming_token_savings(self):
        standing = standing_section('unicode', 'Überprüfe die Änderung. ' * 8)
        current = 'Current task: café ☕.\n' + standing
        rendered, metadata = delivery_text(current, standing)
        self.assertEqual(metadata['full_utf8_bytes'], len(current.encode('utf-8')))
        self.assertEqual(metadata['delivered_utf8_bytes'], len(rendered.encode('utf-8')))
        self.assertGreater(metadata['full_utf8_bytes'], len(current))
        self.assertEqual(set(metadata), {'comparison_available', 'full_utf8_bytes',
                                        'delivered_utf8_bytes', 'omitted_sections', 'changed_sections'})

    def test_markers_do_not_include_routing_cues_and_invalid_names_are_rejected(self):
        packet = standing_section('worker-helper_contract.1', 'Preserve helpers.')
        self.assertNotIn('DE67', packet)
        for name in ('', 'two words', 'name"', '<!--'):
            with self.subTest(name=name):
                with self.assertRaises(ValueError):
                    standing_section(name, 'Contract.')


if __name__ == '__main__':
    unittest.main()
