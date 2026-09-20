import json
from pathlib import Path
import tempfile
import unittest
from work_context_provider import (session_context, select_current_results,
                                    export_current_results, prepare_current_results,
                                    continuation_note, CurrentResultsError)
import hashlib

class SessionContextTests(unittest.TestCase):
    def test_exact_sessions_missing_status_and_reentry_freshness(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);session=root/'.userdata/openclaw_harness/bridge-sessions/branch-a'
            session.mkdir(parents=True);status=session/'status.json'
            evidence={'entrypoints':[str(status)], 'receipts':[{'bindings':{'session':str(session.parent/'historical-b')}}]}
            missing=session_context(root,evidence)['sessions'][0]
            self.assertFalse(missing['files']['status.json']['available'])
            status.write_text(json.dumps({'state':'ready','session_generation':0}))
            first=session_context(root,evidence)['sessions'][0]
            status.write_text(json.dumps({'state':'ready','session_generation':1,'session_descriptor':{'run_id':'new'}}))
            (session/'active-request.json').write_text('{"request_id":"pending-7"}')
            next=session_context(root,evidence)['sessions'][0]
            self.assertEqual(next['recorded_status']['session_generation'],1)
            self.assertEqual(next['recorded_status']['pending_request']['request_id'],'pending-7')
            self.assertNotEqual(first['files']['status.json']['sha256'],next['files']['status.json']['sha256'])
            self.assertEqual(next['current_continuation']['session_generation'], 1)
            self.assertEqual(next['current_continuation']['pending_request_id'], 'pending-7')
            self.assertIn('liveness',next['evidence_limit'])
            self.assertEqual(len(session_context(root,evidence)['sessions']),1)
            status.write_text('corrupt')
            self.assertFalse(session_context(root,evidence)['sessions'][0]['files']['status.json']['available'])

    def test_current_results_requires_exact_ids_and_preserves_independent_results(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d); source = root / 'premise.md'; source.write_text('premise-v1')
            digest = hashlib.sha256(source.read_bytes()).hexdigest()
            evidence = {'receipts': [
                {'receipt_id': 'gen0-save-exit', 'claim': 'persistence', 'status': 'observed'},
                {'receipt_id': 'same-claim-independent', 'claim': 'persistence', 'status': 'observed'},
            ]}
            selected = select_current_results(
                evidence, ['gen0-save-exit', 'same-claim-independent'],
                ['generation-0 save/exit is preserved'],
                ['generation-1 reload/continuation remains outstanding'],
                {str(source): digest})
            output = root / 'current-results.md'
            first = export_current_results(selected, output)
            self.assertEqual(first['receipt_ids'], ['gen0-save-exit', 'same-claim-independent'])
            self.assertIn('gen0-save-exit', output.read_text())
            with self.assertRaises(CurrentResultsError):
                select_current_results(evidence, ['missing-receipt'])
            source.write_text('premise-v2')
            with self.assertRaises(CurrentResultsError):
                select_current_results(evidence, ['gen0-save-exit'], dependencies={str(source): digest})

    def test_prepare_replaces_current_projection_but_keeps_old_revision(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d); source = root / 'premise.md'; source.write_text('premise')
            digest = hashlib.sha256(source.read_bytes()).hexdigest()
            evidence = {'receipts': [{'receipt_id': 'gen0', 'state': 'save-exit'}]}
            first = select_current_results(evidence, ['gen0'], ['old conclusion'], [], {str(source): digest})
            output = root / 'current.md'
            one = prepare_current_results(root, 'worker', first, output, 'dispatch brief')
            second = select_current_results(evidence, ['gen0'], ['new conclusion'], ['reload outstanding'], {str(source): digest})
            two = prepare_current_results(root, 'worker', second, output, 'dispatch brief')
            self.assertNotEqual(one['revision'], two['revision'])
            from importlib.util import spec_from_file_location, module_from_spec
            spec = spec_from_file_location('context_library_test', '/Users/josefhorvath/.codex/skills/de67/de-67-3/scripts/context_library.py')
            library = module_from_spec(spec); spec.loader.exec_module(library)
            self.assertIn('old conclusion', library.revision(root, one['revision'])['text'])
            assembled = library.selected_context(root, 'worker')
            self.assertIn('new conclusion', assembled)
            self.assertNotIn('old conclusion', assembled)

    def test_current_continuation_note_is_replaceable_and_explicit(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d); output = root / 'current-results.md'
            note = continuation_note(
                session_generation=3, process_generation=7, binding='bound-a',
                pending_request_id='play-pending-1',
                retained_frame_handles=['frame:old'],
                retained_evidence_handles=['receipt:old'],
                unresolved_question='Did the resumed owner accept the wait?',
                next_decision='Collect play-pending-1 before any new action.')
            evidence = {'receipts': [{'receipt_id': 'accepted-old', 'status': 'accepted'}]}
            selected = select_current_results(
                evidence, ['accepted-old'],
                ['Old accepted receipt remains at its original scope.'],
                ['Current generation must be validated before reuse.'],
                continuation=note)
            exported = export_current_results(selected, output)
            text = output.read_text()
            self.assertEqual(exported['receipt_ids'], ['accepted-old'])
            for value in ('session_generation', 'process_generation', 'bound-a',
                          'play-pending-1', 'frame:old', 'receipt:old',
                          'Collect play-pending-1 before any new action.'):
                self.assertIn(value, text)
            replacement = select_current_results(
                evidence, ['accepted-old'], ['Resumed current result.'],
                ['Refresh required.'], continuation=continuation_note(
                    session_generation=4, process_generation=8, binding='bound-a',
                    pending_request_id=None, retained_frame_handles=['frame:new'],
                    retained_evidence_handles=['receipt:old'],
                    unresolved_question='None', next_decision='Use fresh look.'))
            export_current_results(replacement, output)
            replaced = output.read_text()
            self.assertNotIn('play-pending-1', replaced)
            self.assertIn('frame:new', replaced)
            self.assertIn('receipt:old', replaced)

if __name__=='__main__':unittest.main()
