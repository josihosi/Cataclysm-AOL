from contextlib import closing
import json
from pathlib import Path
import sqlite3
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from usage_projection import usage_projection
from work_context import thread_tree,token_usage_view
from deadline_harness import DeadlineHarness


def usage(n=10):
    return dict(input_tokens=n,cached_input_tokens=4,output_tokens=2,total_tokens=n+2)

def event(kind,payload):
    return json.dumps(dict(type=kind,payload=payload,timestamp='2026-09-07T06:00:00Z'))+'\n'

def response(identity,turn='t',rid='r',n=10):
    return event('token_usage_record',dict(thread_id=identity,turn_id=turn,response_id=rid,
        usage=usage(n),thread_token_usage=usage(1000)))

def context(model='gpt-5.6-luna',turn='t'):
    return event('turn_context',dict(turn_id=turn,model=model))

class UsageTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
        self.db=sqlite3.connect(':memory:');self.path=self.root/'trace.jsonl'
        self.metadata={'records':[dict(id='root',rollout_path=str(self.path))]}
    def tearDown(self): self.db.close();self.tmp.cleanup()
    def view(self):return usage_projection(self.db,self.metadata,root_id='root',details=True)
    def test_own_responses_deduplicated_rollups_excluded_model_switch(self):
        self.path.write_text(context()+response('root')+response('root')+response('child')+
            context('gpt-5.6-terra','t2')+response('root','t2','r2',20))
        v=self.view();self.assertTrue(v['complete'])
        self.assertEqual(v['by_model']['gpt-5.6-luna']['total_tokens'],12)
        self.assertEqual(v['by_model']['gpt-5.6-terra']['uncached_input_plus_output_tokens'],18)
        self.assertEqual(self.view()['new_source_bytes_read'],0)
    def test_partial_append_reads_only_suffix_and_retries_incomplete(self):
        initial=context()+response('root');self.path.write_text(initial)
        self.view();tail=response('root',rid='r2',n=20)
        with self.path.open('a', encoding='utf-8', newline='') as f:f.write(tail[:15])
        self.assertFalse(self.view()['complete'])
        with self.path.open('a', encoding='utf-8', newline='') as f:f.write(tail[15:])
        v=self.view();self.assertTrue(v['complete']);self.assertEqual(v['new_source_bytes_read'],len(tail))
        self.assertEqual(v['by_model']['gpt-5.6-luna']['total_tokens'],34)
    def test_replacement_resets_cached_records(self):
        self.path.write_text(context()+response('root'));self.view()
        replacement=self.root/'new';replacement.write_text(context('gpt-5.6-terra')+response('root',n=20))
        replacement.replace(self.path);v=self.view()
        self.assertTrue(v['sources'][0]['reset']);self.assertNotIn('gpt-5.6-luna',v['by_model'])
    def test_missing_malformed_conflicting_and_unknown_model_are_visible(self):
        self.path.write_text(response('root')+'[]\ninvalid\n'+response('root',n=20))
        v=self.view();self.assertFalse(v['complete']);self.assertIn('unattributed',v['by_model'])
        self.assertEqual(v['sources'][0]['invalid_or_conflicting_records'],3)
        self.path.unlink();self.assertFalse(self.view()['sources'][0]['available'])
    def test_conflicting_turn_models_never_silently_reassign(self):
        self.path.write_text(context()+response('root')+context('gpt-5.6-terra'))
        v=self.view();self.assertFalse(v['complete']);self.assertIn('unattributed',v['by_model'])
    def test_cumulative_fallback_is_not_added_to_responses(self):
        counter=event('event_msg',dict(type='token_count',info=dict(total_token_usage=usage(500))))
        self.path.write_text(context()+counter);self.assertFalse(self.view()['complete'])
        self.assertEqual(self.view()['by_model'],{})
        with self.path.open('a', encoding='utf-8', newline='') as f:f.write(response('root'))
        self.assertEqual(self.view()['by_model']['gpt-5.6-luna']['total_tokens'],12)
    def test_missing_required_counters_are_a_gap(self):
        self.path.write_text(context()+event('token_usage_record',dict(thread_id='root',turn_id='t',response_id='r',usage={})))
        v=self.view();self.assertFalse(v['complete']);self.assertEqual(v['sources'][0]['invalid_or_conflicting_records'],1)
    def test_missing_payload_or_freshness_never_looks_complete(self):
        self.path.write_text(context()+response('root')+event('token_usage_record',None))
        self.assertFalse(self.view()['complete'])
        self.path.write_text(context()+json.dumps(dict(type='token_usage_record',payload=dict(thread_id='root',turn_id='t',response_id='r',usage=usage())))+'\n')
        self.assertFalse(self.view()['complete'])
    def test_existing_context_index_includes_only_selected_tree_and_helpers(self):
        with closing(sqlite3.connect(self.root/'state_5.sqlite')) as db, db:
            db.executescript('CREATE TABLE threads(id TEXT,rollout_path TEXT); CREATE TABLE thread_spawn_edges(parent_thread_id TEXT,child_thread_id TEXT);')
            for identity,model in [('root','gpt-5.6-sol'),('child','gpt-5.6-terra'),('helper','gpt-5.6-luna'),('unrelated','gpt-5.6-luna')]:
                path=self.root/(identity+'.jsonl');path.write_text(context(model)+response(identity))
                db.execute('INSERT INTO threads VALUES (?,?)',(identity,str(path)))
            db.executemany('INSERT INTO thread_spawn_edges VALUES (?,?)',[('root','child'),('child','helper')])
        self.assertEqual({r['id'] for r in thread_tree('root',codex_home=self.root)['records']},{'root','child','helper'})
        state=self.root/'deadlines.sqlite'
        with DeadlineHarness(state) as h:
            h.start_task('project','task','claim',100,now=1)
            h.claim_worker('project','task','child','root','supervisor',now=2)
        v=token_usage_view(self.root,state,'project',codex_home=self.root)
        self.assertTrue(v['complete']);self.assertEqual(v['session_count'],3)
        self.assertEqual(v['by_model']['gpt-5.6-luna']['session_count'],1)
        self.assertEqual(token_usage_view(self.root,state,'project',codex_home=self.root)['new_source_bytes_read'],0)
        with closing(sqlite3.connect(self.root/'state_5.sqlite')) as db, db:db.execute('DROP TABLE thread_spawn_edges')
        self.assertFalse(token_usage_view(self.root,state,'project',codex_home=self.root)['complete'])

if __name__=='__main__':unittest.main()
