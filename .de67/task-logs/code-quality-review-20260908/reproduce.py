from pathlib import Path
import sys,json,tempfile,hashlib,subprocess
r=Path(__file__).resolve().parent;w=r.parents[2]
sys.path.insert(0,str(w/'tools/openclaw_harness'))
from evidence_search_index import EvidenceIndex
from evidence_search_query import EvidenceSearch
sources=['tools/openclaw_harness/evidence_search_index.py','tools/openclaw_harness/evidence_search_query.py','tools/openclaw_harness/evidence_search_index_test.py','tools/openclaw_harness/evidence_search_query_test.py']
hashes={p:hashlib.sha256((w/p).read_bytes()).hexdigest() for p in sources}
out={'head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=w,text=True).strip(),'sources':hashes}
with tempfile.TemporaryDirectory(prefix='code-quality-') as temp:
 root=Path(temp)
 a=root/'a.jsonl';b=root/'b.jsonl'
 a.write_text(json.dumps({'event':'note','run_id':'A','request_id':'a'})+'\n')
 b.write_text(json.dumps({'event':'note','run_id':'B','request_id':'b'})+'\n')
 idx=EvidenceIndex(root/'multi.sqlite3',model_id='fixture',model_version='1')
 idx.ingest(a);idx.ingest(b)
 before=[{'path':Path(x['path']).name,'request':x['request_id']} for x in idx.occurrences()]
 retry=idx.ingest(b)
 after=[{'path':Path(x['path']).name,'request':x['request_id']} for x in idx.occurrences()]
 assert before==[{'path':'a.jsonl','request':'a'},{'path':'b.jsonl','request':'b'}]
 assert not any(x['request']=='a' for x in after), 'Counterexample no longer reproduces'
 out['multi_source_idempotence']={'expected':'Re-ingesting unchanged B preserves current A and B exactly once.','before':before,'retry':{k:v for k,v in retry.items() if k!='path'},'after':after,'reproduced':True}
 idx.close()
 p=root/'filter.jsonl';p.write_text(json.dumps({'event':'note','run_id':'run-c','feature':'crafting','topic':'recipe'})+'\n')
 idx=EvidenceIndex(root/'filter.sqlite3',model_id='fixture',model_version='1');idx.ingest(p)
 search=EvidenceSearch(idx)
 full=search.query('');filtered=search.query('',filters={'feature':'crafting'})
 assert len(full['rows'])==1 and len(filtered['rows'])==0
 out['original_field_filter']={'expected':'The existing original record with feature=crafting remains discoverable with that explicit filter.','unfiltered_rows':len(full['rows']),'filtered_rows':len(filtered['rows']),'filtered_status':filtered['status'],'reproduced':True}
 idx.close()
assert hashes=={p:hashlib.sha256((w/p).read_bytes()).hexdigest() for p in sources},'Source changed during diagnosis'
(r/'reproduction.json').write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps(out,indent=2))
