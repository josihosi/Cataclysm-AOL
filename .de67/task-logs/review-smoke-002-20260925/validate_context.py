from pathlib import Path
import sys,json,tempfile,shutil,difflib
r=Path(__file__).resolve().parent;w=r.parents[2]
sys.path.insert(0,str(r/'method-candidate/scripts'))
import policy_kernel as k
from coordinator_supervisor import coordinator_prompt
from deadline_harness import DeadlineHarness
import importlib.util
spec=importlib.util.spec_from_file_location("installed_guard", "/Users/josefhorvath/.codex/skills/de67/de-67-3/scripts/mutation_guard.py")
g=importlib.util.module_from_spec(spec);sys.modules[spec.name]=g;spec.loader.exec_module(g)
validate_work_ledger=g.validate_work_ledger;validate_method_mutation=g.validate_method_mutation
result={}
result['method_changed']=validate_method_mutation(r/'method-baseline',r/'method-candidate',universal=False)
result['active_ledger_claims']=len(validate_work_ledger(r/'work-ledger.candidate.md',w/'.de67/FS.md',state=w/'.de67/state/deadlines.sqlite3',lineage_id='semantic-surface-cockpit'))
# Exercise the production packet producer with the real candidate owner/ledger/specification,
# but isolated tasks, index and DB. No dispatch or provider call.
with tempfile.TemporaryDirectory() as td:
 p=Path(td);(p/'.de67').mkdir()
 for src,dest in [(r/'WEC.candidate.md','WEC.md'),(r/'work-ledger.candidate.md','work-ledger.md'),(w/'.de67/FS.md','FS.md')]:shutil.copy2(src,p/'.de67'/dest)
 state=p/'state.sqlite3'
 tasks=['R-CAOL-FIRST-SMOKE-exploration-002','R-CAOL-FIRST-SMOKE-exploration-004','R-CAOL-CAMP-WAIT-COST-exploration-003','R-CAOL-DISPATCH-VISIT-exploration-001']
 with DeadlineHarness(state) as h:
  for t in tasks:h.start_task('review',t,t.split('-exploration-')[0],100,now=1)
 coord=coordinator_prompt(p,state,'review','isolated-review',None)
 (r/'coordinator-context.txt').write_text(coord)
 assert 'Coordinator assignment authority: assign NPC wait/sleep' in coord
 assert 'Create supported repair work' in coord
 calls=k.unbound_worker_spawns(p,state,'review')
 assert len(calls)==len(tasks)
 packet_results=[]
 for call in calls:
  text=Path(call['dispatch_packet']['path']).read_text();task=next(t for t in tasks if 'work '+t+' for' in text)
  assert 'Coordinator assignment authority:' not in text
  assert 'route NPC wait/sleep implementation' not in text
  assert 'Use GPT-6 Astra for any implementation' not in text
  assert 'Coordinator may assign GPT-6 Astra' not in text
  assert 'below-100-ms' in text and 'meaningful transitions' in text
  assert 'An Astra helper remains an optional choice' in text
  assert 'GPT-6 Luna performs the full native Mac playtest' in text
  if 'CAMP-WAIT-COST' in task: assert 'implementation is assigned to GPT-6 Astra' in text
  (r/(task+'.packet.txt')).write_text(text)
  packet_results.append(dict(task=task,bytes=len(text.encode()),role_scoped=True,owner_outcome_preserved=True))
 result['packets']=packet_results
# Keep a narrow patch for the lab mirror; do not overwrite its unrelated differences.
patch=''.join(''.join(difflib.unified_diff((r/'method-baseline'/n).read_text().splitlines(True),(r/'method-candidate'/n).read_text().splitlines(True),fromfile='a/'+n,tofile='b/'+n)) for n in result['method_changed'])
(r/'method.patch').write_text(patch)
(r/'validation.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))
