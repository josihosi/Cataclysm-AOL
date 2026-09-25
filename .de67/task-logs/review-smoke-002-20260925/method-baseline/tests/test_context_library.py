from pathlib import Path
import json
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
import context_library as library


class ContextLibraryTests(unittest.TestCase):
    def test_task_identity_is_not_a_bundle_name_or_filesystem_path(self):
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            for task in ('R-001-prüfung', '../task with spaces', 'x' * 300, 'A', 'a'):
                library.prepare(workspace, task, 'Assignment for ' + task)
                self.assertEqual(library.task_view(workspace, task)['task'], task)
            self.assertEqual(len(list((workspace / '.de67/state/context-library/tasks').glob('*.json'))), 5)
            legacy = workspace / '.de67/state/context-library/tasks/old.json'
            value = library.task_view(workspace, 'old')
            value['brief'] = 'Preserved old selection'
            legacy.write_text(json.dumps(value), encoding='utf-8')
            self.assertIn(value['brief'], library.selected_context(workspace, 'old'))
            library.prepare(workspace, 'old', 'Updated')
            self.assertIn('Updated', library.selected_context(workspace, 'old'))

    def test_utf8_brief_survives_legacy_default_encoding(self):
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            brief = workspace / 'brief.md'
            handoff = workspace / 'handoff.md'
            text = 'Prüfe — 🔴 native evidence'
            brief.write_text(text, encoding='utf-8')
            handoff.write_text(text, encoding='utf-8')
            original = Path.read_text

            def legacy_read(path, encoding=None, **kwargs):
                return original(path, encoding=encoding or 'cp1252', **kwargs)

            with patch.object(Path, 'read_text', legacy_read):
                self.assertEqual(library.main(['--workspace', str(workspace), '--task', 'task',
                    'prepare', '--brief', str(brief), '--handoff', str(handoff)]), 0)
                self.assertEqual(library.selected_context(workspace, 'task').count(text), 2)

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.workspace = Path(self.temp.name)

    def source(self, name, content):
        path = self.workspace / name
        path.write_text(content)
        return path

    def test_selected_skill_and_current_results_without_history_inheritance(self):
        skill = self.source('skill.md', '# Run\nUse semantic observations.\n# Debug\nNo proof credit.\n')
        history = self.source('old.md', 'IRRELEVANT TRANSCRIPT')
        digest = library.put(self.workspace, 'task-a', 'run', skill, 'skill')
        library.put(self.workspace, 'task-a', 'history', history)
        library.prepare(self.workspace, 'task-a', 'Prove native camp establishment.', ['run'], 'Food proved; Patrol unknown; evidence: food.json')
        packet = library.selected_context(self.workspace, 'task-a')
        self.assertIn('Prove native camp establishment.', packet)
        self.assertIn('Use semantic observations.', packet)
        self.assertIn('Patrol unknown', packet)
        self.assertNotIn('IRRELEVANT TRANSCRIPT', packet)
        self.assertNotIn('semantic observations', json.dumps(library.catalog(self.workspace, 'task-a')))
        self.assertEqual(library.show(self.workspace, digest, 'Debug'), '# Debug\nNo proof credit.\n')
        library.prepare(self.workspace, 'task-a', 'Prove Storage.', ['run'], 'Food proved; Storage unknown; evidence: food.json')
        self.assertNotIn('Patrol unknown', library.selected_context(self.workspace, 'task-a'))
        self.assertEqual(history.read_text(), 'IRRELEVANT TRANSCRIPT')

    def test_reuse_freshness_and_immutable_old_revision(self):
        source = self.source('facts.md', 'Old premise')
        old = library.put(self.workspace, 'a', 'facts', source)
        library.reuse(self.workspace, 'b', 'shared', old)
        library.prepare(self.workspace, 'b', 'Test the premise', ['shared'])
        source.write_text('Corrected premise')
        self.assertFalse(library.catalog(self.workspace, 'b')['bundles'][0]['fresh'])
        with self.assertRaisesRegex(library.ContextError, 'Stale'):
            library.selected_context(self.workspace, 'b')
        new = library.put(self.workspace, 'a', 'facts', source)
        self.assertNotEqual(old, new)
        self.assertEqual(library.show(self.workspace, old), 'Old premise')
        library.reuse(self.workspace, 'b', 'shared', new)
        self.assertIn('Corrected premise', library.selected_context(self.workspace, 'b'))
        library.drop(self.workspace, 'b', 'shared')
        self.assertEqual(library.show(self.workspace, new), 'Corrected premise')

    def test_capacity_errors_preserve_current_selection_without_truncation(self):
        source = self.source('facts.md', 'A' * 100)
        library.put(self.workspace, 'a', 'facts', source)
        library.prepare(self.workspace, 'a', 'Assignment', ['facts'])
        before = library.selected_context(self.workspace, 'a')
        oversized = self.source('huge.md', 'Z' * 5000)
        with self.assertRaisesRegex(library.ContextError, 'capacity'):
            library.put(self.workspace, 'a', 'facts', oversized)
        self.assertEqual(library.selected_context(self.workspace, 'a'), before)
        for i in range(11):
            library.put(self.workspace, 'a', 'item' + str(i), source)
        with self.assertRaisesRegex(library.ContextError, 'capacity'):
            library.put(self.workspace, 'a', 'thirteenth', source)
        self.assertEqual(len(library.catalog(self.workspace, 'a')['bundles']), 12)
        with self.assertRaisesRegex(library.ContextError, 'distinct active'):
            library.prepare(self.workspace, 'a', 'Assignment', ['missing'])
        self.assertEqual(library.selected_context(self.workspace, 'a'), before)

    def test_selected_budget_and_explicit_limit_adjustment(self):
        source = self.source('facts.md', 'A' * 3800)
        for i in range(7): library.put(self.workspace, 'a', 'item' + str(i), source)
        names = ['item' + str(i) for i in range(7)]
        with self.assertRaisesRegex(library.ContextError, 'Selected context capacity'):
            library.prepare(self.workspace, 'a', 'Assignment', names)
        self.assertEqual(library.selected_context(self.workspace, 'a'), '')
        self.assertEqual(library.main(['--workspace', str(self.workspace), '--task', 'a', 'limits', '--selected-bytes', '30000']), 0)
        library.prepare(self.workspace, 'a', 'Assignment', names)
        self.assertIn('Selected facts bundle item6', library.selected_context(self.workspace, 'a'))

    def test_no_library_is_compatible_and_digest_corruption_is_visible(self):
        self.assertEqual(library.selected_context(self.workspace, 'legacy-task'), '')
        digest = library.put(self.workspace, 'a', 'facts', self.source('a.md', 'fact'))
        path = self.workspace / '.de67/state/context-library/revisions' / (digest + '.json')
        path.write_text('{}')
        with self.assertRaisesRegex(library.ContextError, 'digest mismatch'):
            library.show(self.workspace, digest)

    def test_dispatch_prepares_zero_bundle_task_and_replaces_automatic_handoff(self):
        packet = library.dispatch_context(self.workspace, 'task-a', 'Prove Food.', 'Food unknown')
        self.assertIn('Prove Food.', packet)
        self.assertEqual(library.catalog(self.workspace, 'task-a')['bundles'], [])
        packet = library.dispatch_context(self.workspace, 'task-a', 'Prove Storage.', 'Food proved')
        self.assertIn('Prove Storage.', packet)
        self.assertNotIn('Food unknown', packet)

    def test_changed_assignment_requires_reselection_not_silent_packet_rewrite(self):
        root = self.workspace / '.de67'
        root.mkdir()
        ledger = root / 'work-ledger.md'
        ledger.write_text('- [ ] R-1 — Camp\n  - Assignment task-a: Prove Food.\n')
        library.prepare(self.workspace, 'task-a', 'Food route')
        original = library.dispatch_context(self.workspace, 'task-a', 'Prove Food.')
        ledger.write_text(ledger.read_text() + '  - Assignment task-b: Prove Patrol.\n')
        self.assertEqual(library.dispatch_context(self.workspace, 'task-a', 'Prove Food.'), original)
        ledger.write_text(ledger.read_text().replace('Prove Food.', 'Prove Storage.'))
        with self.assertRaisesRegex(library.ContextError, 'assignment changed'):
            library.dispatch_context(self.workspace, 'task-a', 'Prove Storage.')
        self.assertEqual(library.selected_context(self.workspace, 'task-a'), original)
        library.prepare(self.workspace, 'task-a', 'Storage route')
        self.assertIn('Storage route', library.dispatch_context(self.workspace, 'task-a', 'Prove Storage.'))

    def test_underlying_dependency_invalidates_unchanged_summary_only_when_relevant(self):
        source = self.source('summary.md', 'Payment uses the active operation.')
        code = self.source('payment.cpp', 'old source')
        unrelated = self.source('other.cpp', 'old source')
        old = library.put(self.workspace, 'task-a', 'payment', source, dependencies=[code])
        library.prepare(self.workspace, 'task-a', 'Prove Pay.', ['payment'])
        unrelated.write_text('new unrelated source')
        self.assertIn('active operation', library.dispatch_context(self.workspace, 'task-a', 'Prove Pay.'))
        code.write_text('new payment source')
        self.assertFalse(library.catalog(self.workspace, 'task-a')['bundles'][0]['fresh'])
        with self.assertRaisesRegex(library.ContextError, 'dependency'):
            library.dispatch_context(self.workspace, 'task-a', 'Prove Pay.')
        self.assertEqual(library.show(self.workspace, old), source.read_text())


if __name__ == '__main__': unittest.main()
