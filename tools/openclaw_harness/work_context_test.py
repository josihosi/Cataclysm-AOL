import json
import contextlib
import io
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

    def test_session_context_uses_the_actual_pending_completed_and_ended_state(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            session = root / '.userdata/openclaw_harness/bridge-sessions/current'
            session.mkdir(parents=True)
            evidence = {'entrypoints': [str(session / 'status.json')]}
            status = session / 'status.json'
            active = session / 'active-request.json'
            status.write_text(json.dumps({'state': 'awaiting_response', 'binding_id': 'bound',
                                          'session_generation': 3, 'inflight_request_id': 'request-pending'}))
            active.write_text(json.dumps({'request_id': 'request-pending'}))
            pending = session_context(root, evidence)['sessions'][0]['next_operation']
            self.assertEqual(pending['kind'], 'collect_pending')
            self.assertEqual(pending['request_id'], 'request-pending')
            self.assertEqual(pending['argv'][-3:], ['collect', '--request-id', 'request-pending'])
            self.assertNotIn('look', pending['argv'])

            active.unlink()
            status.write_text(json.dumps({'state': 'ready', 'binding_id': 'bound', 'session_generation': 3}))
            (session / 'play-client.json').write_text(json.dumps({
                'binding_id': 'bound', 'last_request_id': 'request-completed',
                'last_collected_result': {'request_id': 'request-completed', 'ok': True},
            }))
            completed = session_context(root, evidence)['sessions'][0]['next_operation']
            self.assertEqual(completed['kind'], 'replay_completed_result')
            self.assertEqual(completed['argv'][-3:], ['collect', '--request-id', 'request-completed'])

            status.write_text(json.dumps({'state': 'safe_to_cleanup', 'binding_id': 'bound'}))
            ended = session_context(root, evidence)['sessions'][0]['next_operation']
            self.assertEqual(ended['kind'], 'ended_or_failed')
            self.assertNotIn('argv', ended)

            (session / 'play-client.json').unlink()
            status.write_text(json.dumps({'state': 'ready', 'binding_id': 'bound', 'session_generation': 4}))
            stale = session_context(root, evidence)['sessions'][0]['next_operation']
            self.assertEqual(stale['kind'], 'refresh_current_owner')
            self.assertEqual(stale['argv'][-1], 'look')

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

    def test_old_continuation_note_without_executable_operation_remains_reusable(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            old_note = continuation_note(
                session_generation=1, process_generation=1, binding='bound',
                pending_request_id=None, next_decision='Use fresh look.')
            old_note.pop('next_operation')
            selected = select_current_results(
                {'receipts': [{'receipt_id': 'old-receipt'}]}, ['old-receipt'],
                continuation=old_note)
            export_current_results(selected, root / 'old.md')
            self.assertIn('next_operation', (root / 'old.md').read_text())

    def test_export_projects_receipts_to_handles_and_foregrounds_decision_state(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            output = root / 'current-results.md'
            evidence = {'receipts': [{
                'receipt_id': 'full-receipt', 'verdict': 'inconclusive',
                'evidence_ceiling': ['no live result'], 'bindings': {'run': 'r-1'},
                'artifacts': [{'path': 'evidence.json', 'sha256': 'a' * 64, 'role': 'receipt'}],
                'huge_unneeded_body': 'not worker input',
            }]}
            selected = select_current_results(
                evidence, ['full-receipt'], conclusions=['A narrow conclusion.'],
                limits=['No live result.'], current_question='Which exact request is pending?')
            export_current_results(selected, output)
            exported = output.read_text()
            self.assertIn('A narrow conclusion.', exported)
            self.assertIn('No live result.', exported)
            self.assertIn('Which exact request is pending?', exported)
            self.assertIn('evidence.json', exported)
            self.assertNotIn('not worker input', exported)

    def test_prepared_context_is_assembled_then_used_for_same_operation_replay(self):
        """The injected projection, rather than its archived bytes, drives a valid next command."""
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            session = root / '.userdata/openclaw_harness/bridge-sessions/replay'
            session.mkdir(parents=True)
            (session / 'bridge.manifest.json').write_text(json.dumps({'binding_id': 'bound'}))
            (session / 'status.json').write_text(json.dumps({
                'state': 'ready', 'binding_id': 'bound', 'session_generation': 2,
            }))
            (session / 'play-client.json').write_text(json.dumps({
                'binding_id': 'bound', 'last_request_id': 'completed-1',
                'last_collected_result': {'ok': True, 'request_id': 'completed-1', 'state': 'collected'},
            }))
            observed = session_context(root, {'entrypoints': [str(session / 'status.json')]})['sessions'][0]
            operation = observed['next_operation']
            self.assertEqual(operation['kind'], 'replay_completed_result')
            source = root / 'source.md'
            source.write_text('source-bound')
            selection = select_current_results(
                {'receipts': [{'receipt_id': 'retained-result'}]}, ['retained-result'],
                conclusions=['Completed result is replayable without new input.'],
                limits=['The synthetic session establishes replay only, not native gameplay.'],
                current_question='What was the completed operation?',
                continuation=observed['current_continuation'],
                dependencies={str(source): hashlib.sha256(source.read_bytes()).hexdigest()},
            )
            prepared = prepare_current_results(root, 'consumer', selection, root / 'current.md',
                                               'Use the injected current result and its next operation.')
            from importlib.util import spec_from_file_location, module_from_spec
            spec = spec_from_file_location('context_library_adoption',
                                           '/Users/josefhorvath/.codex/skills/de67/de-67-3/scripts/context_library.py')
            library = module_from_spec(spec); spec.loader.exec_module(library)
            injected = library.selected_context(root, 'consumer')
            self.assertIn('Completed result is replayable without new input.', injected)
            self.assertIn('completed-1', injected)
            self.assertTrue(prepared['revision'])
            import play_cli
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                exit_status = play_cli.main(operation['argv'][2:])
            replay = json.loads(output.getvalue())
            self.assertEqual(exit_status, 0)
            self.assertEqual(replay['request_id'], 'completed-1')
            self.assertTrue(replay['ok'])

if __name__=='__main__':unittest.main()
