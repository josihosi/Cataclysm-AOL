from contextlib import closing
import concurrent.futures
import json
from pathlib import Path
import sqlite3
import sys
import tempfile
import unittest
SCRIPTS=Path(__file__).resolve().parents[1]/'scripts'
sys.path.insert(0,str(SCRIPTS))
from deadline_harness import DeadlineHarness
from worker_receipt import receipt_envelope
from work_context import context_view, record_dispatch, record_run, thread_records, ContextError, dispatch_evidence_index
import policy_kernel


def receipt(task, worker, session):
    return {'schema':'de67.worker-result-receipt.v1','lineage_id':'project','task_id':task,
        'claim_id':'R-CONT','worker_id':worker,'disposition':'checkpoint','verdict':'partial',
        'outcome':'Observe native ecology','summary':f'{task} established distinct evidence',
        'material_changes':[],'tests':[],'live_actions':[], 'journal_entries':[], 'artifacts':[],
        'evidence_ceiling':[f'{task} preparation only'], 'bindings':{'session':session},
        'first_divergence':None,'accepted_no_replay':[], 'active_work':['Observe response'],
        'first_open_boundary':'Observe independent response','narrow_queries':[], 'entrypoints':[session]}


class WorkContextTests(unittest.TestCase):
    def setup_state(self, root):
        state=root/'deadline.sqlite3'
        with DeadlineHarness(state) as h:
            for i,task in enumerate(('branch-a','branch-b')):
                h.start_task('project',task,'R-CONT',100,now=1+i)
                h.claim_worker('project',task,'worker-'+task,'coordinator','supervisor',now=3+i)
        return state

    def test_concurrent_contributions_restart_and_exact_branch_context(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);state=self.setup_state(root)
            def record(task):
                with DeadlineHarness(state) as h:
                    return h.record_worker_result_receipt('project',task,'worker-'+task,
                        receipt(task,'worker-'+task,'sessions/'+task),now=5 if task=='branch-a' else 6)
            with concurrent.futures.ThreadPoolExecutor(2) as pool:
                results=list(pool.map(record,('branch-a','branch-b')))
            def view():return context_view(root,state,'project',claim='R-CONT',route='Continue sessions/branch-a')
            v=view()
            self.assertEqual({r['task_id'] for r in v['related_results']},{'branch-a','branch-b'})
            self.assertEqual([r['receipt']['task_id'] for r in v['selected_context']],['branch-a'])
            self.assertEqual(v['selected_context'][0]['receipt']['evidence_ceiling'],['branch-a preparation only'])
            # Separate process/connections and a new coordinator session preserve both contributions.
            record_dispatch(root,state,'project','successor',root/'packet.md','abc',
                            {'evidence_receipts':[results[0]['receipt_id']]})
            with closing(sqlite3.connect(state)) as db, db:
                db.execute("UPDATE worker_claims SET coordinator_session_id='fresh-coordinator'")
            again=view()
            self.assertEqual(again['newly_indexed'],0)
            self.assertEqual(again['related_results'],v['related_results'])
            self.assertEqual({t['coordinator_session_id'] for t in again['tasks']},{'fresh-coordinator'})
            # A new unrelated latest result must not replace either branch.
            with DeadlineHarness(state) as h:
                value=receipt('branch-b','worker-branch-b','sessions/branch-b')
                value['summary']='new unrelated observation'
                h.record_worker_result_receipt('project','branch-b','worker-branch-b',value,now=7)
            self.assertEqual([r['receipt']['task_id'] for r in view()['selected_context']],['branch-a'])
            history=context_view(root,state,'project',claim='R-CONT',contains='branch-b',full=True)
            self.assertEqual(len(history['receipts']),2)
            self.assertEqual(len(history['related_results']),2)
            explicit=context_view(root,state,'project',claim='R-CONT',route=results[1]['receipt_id'])
            self.assertIn(results[1]['receipt_id'],[r['receipt']['receipt_id'] for r in explicit['selected_context']])
            self.assertEqual(context_view(root,state,'project',task='branch-a')['claim'],'R-CONT')

    def test_dispatch_index_preserves_boundaries_and_exact_history_retrieval(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);state=self.setup_state(root)
            value=receipt('branch-a','worker-branch-a','sessions/a')
            value['first_divergence']={'class':'rejection','summary':'wrong_surface'}
            value['accepted_no_replay']=['scroll repair proved']
            with DeadlineHarness(state) as h:
                stored=h.record_worker_result_receipt('project','branch-a','worker-branch-a',value,now=5)
            view=context_view(root,state,'project',claim='R-CONT',route='sessions/a')
            index=dispatch_evidence_index(view)
            self.assertEqual(len(index),1)
            self.assertNotIn('first_divergence',index[0])
            detailed=dispatch_evidence_index(view,detailed=True)
            self.assertEqual(detailed[0]['first_divergence'],value['first_divergence'])
            self.assertEqual(index[0]['original_scope']['accepted_no_replay'],value['accepted_no_replay'])
            self.assertNotIn('artifact_refs',index[0])
            self.assertNotIn('journal_entry_ids',index[0])
            argv=detailed[0]['retrieve_argv']
            self.assertEqual(argv[-3:],['--receipt',stored['receipt_id'],'--full'])
            recovered=context_view(root,state,'project',receipt_id=argv[-2],full=True)
            self.assertEqual(receipt_envelope(recovered['receipts'][0]['receipt'])['receipt_id'],stored['receipt_id'])
            self.assertEqual(recovered['receipts'][0]['receipt']['bindings'],value['bindings'])

    def test_parallel_index_refresh_missing_receipt_and_source_corruption(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);state=self.setup_state(root)
            v=context_view(root,state,'project',claim='R-CONT')
            self.assertEqual(len(v['tasks']),2);self.assertEqual(v['related_results'],[])
            with DeadlineHarness(state) as h:
                h.record_worker_result_receipt('project','branch-a','worker-branch-a',
                    receipt('branch-a','worker-branch-a','sessions/a'),now=5)
            with concurrent.futures.ThreadPoolExecutor(2) as pool:
                views=list(pool.map(lambda _:context_view(root,state,'project',claim='R-CONT'),range(2)))
            self.assertEqual([v['receipt_count'] for v in views],[1,1])
            # Index is disposable; rebuilding from authoritative receipts preserves identity.
            path=root/'.de67/state/work-context.sqlite3';path.unlink()
            rebuilt=context_view(root,state,'project',claim='R-CONT')
            self.assertEqual(rebuilt['related_results'],views[0]['related_results'])

    def test_worker_packet_preserves_both_branches_and_current_ledger(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);state=self.setup_state(root)
            with DeadlineHarness(state) as h:
                for i,task in enumerate(('branch-a','branch-b')):
                    h.record_worker_result_receipt('project',task,'worker-'+task,
                        receipt(task,'worker-'+task,'sessions/'+task),now=5+i)
                h.normalize_external_supervisor_start('project',now=7)
                h.start_task('project','successor','R-CONT',100,now=8)
            de67=root/'.de67';de67.mkdir(exist_ok=True)
            (de67/'work-ledger.md').write_text('- [ ] R-CONT — Observe ecology.\n'
                '  - DFS slices: `R-CONT-S001`\n'
                '  - Known footing: independently accepted branch-a work.\n'
                '  - Current handoff: Continue `sessions/branch-a`.\n')
            (de67/'FS.md').write_text('<!-- DE67:DFS-SLICE:BEGIN id=R-CONT-S001 claim=R-CONT -->\n'
                'Observe independent natural response.\n<!-- DE67:DFS-SLICE:END id=R-CONT-S001 claim=R-CONT -->\n')
            call=policy_kernel.unbound_worker_spawns(root,state,'project')[0]
            packet=Path(call['dispatch_packet']['path']).read_text()
            self.assertIn('branch-a established distinct evidence',packet)
            self.assertIn('branch-b',packet)  # Unselected full context remains at the indexed history handle.
            self.assertIn('independently accepted branch-a work',packet)
            self.assertIn('sessions/branch-a',packet)
            view=context_view(root,state,'project',task='successor')
            self.assertEqual(view['dispatches'][0]['context']['related_tasks'],['branch-a','branch-b'])
            self.assertEqual(len(view['dispatches'][0]['context']['evidence_receipts']),1)

    def test_exact_thread_metadata_and_corrupt_index_are_visible(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);runtime=root/'runtime';runtime.mkdir();trace=runtime/'trace.jsonl'
            trace.write_text('full trace deliberately not parsed')
            with closing(sqlite3.connect(runtime/'state_5.sqlite')) as db, db:
                db.execute('CREATE TABLE threads(id,rollout_path,source,model)')
                db.execute('INSERT INTO threads VALUES (?,?,?,?)',('known',str(trace),'subagent','gpt-5.6-luna'))
            metadata=thread_records([{'task_id':'task','worker_id':'known','coordinator_session_id':'absent'}],codex_home=runtime)
            self.assertEqual(metadata['unavailable_thread_ids'],['absent'])
            self.assertEqual(metadata['records'][0]['trace']['bytes'],trace.stat().st_size)
            state=self.setup_state(root)
            with DeadlineHarness(state) as h:
                h.record_worker_result_receipt('project','branch-a','worker-branch-a',
                    receipt('branch-a','worker-branch-a','sessions/a'),now=5)
            context_view(root,state,'project',claim='R-CONT')
            with closing(sqlite3.connect(root/'.de67/state/work-context.sqlite3')) as db, db:
                db.execute('INSERT INTO receipts SELECT source,lineage,task,999,claim,receipt_id,recorded_at,payload,search_text FROM receipts')
            # An index row outside the source snapshot (future concurrent commit or
            # stale restored cache) cannot leak into this query's evidence.
            self.assertEqual(context_view(root,state,'project',claim='R-CONT')['receipt_count'],1)
            with closing(sqlite3.connect(root/'.de67/state/work-context.sqlite3')) as db, db:
                db.execute("UPDATE receipts SET payload=json_set(payload,'$.summary','forged')")
            with self.assertRaises(ContextError):context_view(root,state,'project',claim='R-CONT')
            self.assertEqual(context_view(root,state,'project',claim='R-CONT',rebuild=True)['receipt_count'],1)

    def test_runner_metadata_links_without_reading_trace(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);state=self.setup_state(root);run=root/'run';run.mkdir()
            (run/'events.jsonl').write_text('not valid JSON, deliberately never parsed')
            (run/'prompt.txt').write_text('preserved full prompt')
            env={'DE67_PROCESS_ROLE':'coordinator','DE67_LINEAGE':'project','DE67_COORDINATOR_RUN_ID':'run-1'}
            record_run(root,run,env,session_id='coordinator')
            v=context_view(root,state,'project',claim='R-CONT')
            self.assertEqual(v['runner_records'][0]['role'],'coordinator')
            self.assertTrue(v['runner_records'][0]['metadata']['events.jsonl']['available'])
            self.assertFalse(v['runner_records'][0]['metadata']['status.json']['available'])

    def test_live_task_exposes_exact_optional_checkpoint_argv_without_mutation(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d); state=self.setup_state(root)
            before=state.read_bytes()
            view=context_view(root,state,'project',task='branch-a')
            template=view['checkpoint_worker_template']
            self.assertTrue(template['optional'])
            self.assertEqual(template['argv'], [
                sys.executable, str(SCRIPTS/'deadline_harness.py'), '--state',
                str(state.resolve()), 'checkpoint-worker', '--lineage', 'project',
                '--task', 'branch-a', '--worker', 'worker-branch-a'])
            self.assertEqual(template['bindings'], {
                'state': str(state.resolve()), 'lineage': 'project',
                'task': 'branch-a', 'worker': 'worker-branch-a'})
            self.assertNotIn('--kind', template['argv'])
            self.assertNotIn('--evidence', template['argv'])
            self.assertEqual(state.read_bytes(), before)

    def test_checkpoint_template_is_absent_for_unclaimed_and_terminal_tasks(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d); state=root/'deadline.sqlite3'
            with DeadlineHarness(state) as h:
                h.start_task('project','unclaimed','R-CONT',100,now=1)
                h.start_task('project','terminal','R-CONT',100,now=2)
                h.claim_worker('project','terminal','worker-terminal','coordinator','supervisor',now=3)
                terminal_receipt=receipt('terminal','worker-terminal','sessions/terminal')
                terminal_receipt['disposition']='abandoned'
                stored=h.record_worker_result_receipt('project','terminal','worker-terminal',
                    terminal_receipt,now=4)
                h.abandon_attempt('project','terminal','done',receipt_id=stored['receipt_id'],now=5)
            self.assertNotIn('checkpoint_worker_template',
                             context_view(root,state,'project',task='unclaimed'))
            self.assertNotIn('checkpoint_worker_template',
                             context_view(root,state,'project',task='terminal'))

if __name__=='__main__':unittest.main()
