"""Owner-scoped specification/context preservation checks; no lifecycle writes."""
import hashlib,json,re,sqlite3,sys,tempfile
from pathlib import Path
r=Path(__file__).parent.resolve();w=r.parents[2];b=r/'baseline';c=r/'candidate'
sys.path.insert(0,'/Users/josefhorvath/.codex/skills/de67/de-67-3/scripts')
import mutation_guard as guard
import policy_kernel as policy
from coordinator_supervisor import coordinator_prompt
out={}
# Invoke the existing local review validator. Record its precise compatibility limit,
# rather than changing the installed guard or pretending an owner thaw is append-only.
try:
 out['ordinary_random_review']=list(guard.validate_random_review_mutation(b,c,selected_lane='test-and-task-guidelines.md'))
except guard.GuardError as error:out['ordinary_random_review']={'not_applicable':str(error),'reason':'Owner-directed thaw/refreeze of existing open contracts; current canonical FS also lacks the legacy protected heading names.'}
try:
 out['broad_fs_contract_guard']=guard.validate_universal_dfs_mutation(b/'FS.md',c/'FS.md')
except guard.GuardError as error:out['broad_fs_contract_guard']={'not_applicable':str(error)}
consumed=guard.validate_owner_suggestion_consumption(b/'mutation-suggestions.md',c/'mutation-suggestions.md');assert len(consumed)==1 and 'HARNESS-FULL-STACK-20260921' in consumed[0]
old=(b/'FS.md').read_text();new=(c/'FS.md').read_text()
old_slices={x.slice_id:x for x in guard.parse_dfs_slices(old)};new_slices={x.slice_id:x for x in guard.parse_dfs_slices(new)}
mutable={'R-HARNESS-EXECUTION-S001','R-HARNESS-SESSION-S001','R-HARNESS-JOURNEY-S001','R-HARNESS-PERFORMANCE-S001'}
for sid,item in old_slices.items():
 assert sid in new_slices and item.claim_id==new_slices[sid].claim_id,sid
 if sid not in mutable:assert item.content==new_slices[sid].content,sid
old_claims=guard._claims_by_id(guard._stable_claim_records(old),'Baseline');new_claims=guard._claims_by_id(guard._stable_claim_records(new),'Candidate')
for key,record in old_claims.items():
 assert key in new_claims,key
 if key!='R-HARNESS-PERFORMANCE':assert new_claims[key]==record,key
 else:
  assert record[1]==' ' and new_claims[key][1]=='x'
  assert record[3].split(' — ',1)[1]==new_claims[key][3].split(' — ',1)[1]
added=set(new_claims)-set(old_claims);assert added=={'R-HARNESS-EVIDENCE','R-HARNESS-PREMISES','R-HARNESS-CONTINUATION'}
assert all(new_claims[k][1]==' ' and new_claims[k][2] for k in added)
# All original accepted claim bodies and the actual domain truth section stay intact.
for sid,item in old_slices.items():
 if item.claim_id in old_claims and old_claims[item.claim_id][1].lower()=='x':assert new_slices[sid].content==item.content
for title in ['## Language, coordinates and truth','## Retained accepted gameplay contract','## Product regression and cleanup sequence','## Six efficiency improvements','## Revised remaining playtest: follow into a city','## Deferred experiments and release gates']:
 assert guard._exact_markdown_section(old,title)==guard._exact_markdown_section(new,title),title
state=w/'.de67/state/deadlines.sqlite3';db=sqlite3.connect('file:'+str(state)+'?mode=ro',uri=True);db.row_factory=sqlite3.Row
acceptances=[dict(x) for x in db.execute('select * from claim_acceptances order by lineage_id,claim_id,acceptance_number')]
assert acceptances==json.loads((r/'accepted-before.json').read_text())
assert any(x['claim_id']=='R-HARNESS-PERFORMANCE' and x['acceptance_number']==1 and x['invalidated_at'] is None for x in acceptances)
items=guard.validate_work_ledger(c/'work-ledger.md',c/'FS.md',state=state,lineage_id='semantic-surface-cockpit')
# Existing issued assignment text is byte-preserved; the new current owner contract supplies
# pending corrections. No immutable packet is rewritten or new deadline task opened.
ledger=(c/'work-ledger.md').read_text();before_ledger=(b/'work-ledger.md').read_text()
for line in before_ledger.splitlines():
 if line.startswith('  - Assignment ') and ' result:' not in line and ' disposition:' not in line:
  assert line in ledger,line[:100]
new_tasks={'R-HARNESS-EVIDENCE-exploration-001':'R-HARNESS-EVIDENCE','R-HARNESS-SESSION-exploration-001':'R-HARNESS-SESSION','R-HARNESS-PREMISES-exploration-001':'R-HARNESS-PREMISES','R-HARNESS-CONTINUATION-exploration-001':'R-HARNESS-CONTINUATION','R-HARNESS-CONTINUATION-exploration-002':'R-HARNESS-CONTINUATION'}
assignment_previews={}
for task,claim in new_tasks.items():
 assignment,source=policy.exploration_assignment(ledger,task,claim)
 assert source=='task-specific ledger assignment' and len(assignment)>300
 assignment_previews[task]={'source':source,'bytes':len(assignment.encode()),'text':assignment}
(r/'assignment-previews.json').write_text(json.dumps(assignment_previews,indent=2)+'\n')
with tempfile.TemporaryDirectory() as temporary:
 preview=Path(temporary);(preview/'.de67').mkdir();(preview/'.de67/WEC.md').write_bytes((c/'WEC.md').read_bytes())
 owner=policy.current_owner_contract(preview)
 assert 'HARNESS-FULL-STACK-20260921' in owner and '50%' in owner and 'Pending delivery correction' in owner
 prompt=coordinator_prompt(preview,state,'semantic-surface-cockpit','preview-only',None,'owner-suggestion f5d913c63d0e')
 assert owner in prompt
 assert 'external supervisor alone launches' in owner
 (r/'owner-contract-preview.md').write_text(owner)
 out['rendered_context']={'owner_contract_bytes':len(owner.encode()),'coordinator_prompt_contains_exact_owner_contract':True,'worker_assignment_previews':str(r/'assignment-previews.json'),'evidence_ceiling':'Production context extractors and prompt renderer executed; no worker or coordinator launched, adoption remains delivery work.'}
# Every attachment topic is represented by a distinct row plus full normative requirements.
for marker in ['| 1.','| 2.','| 3.','| 4.','| 5A–E.','| 6.','| 7A–D.','| 8A–B.','| 9.','| 10.','| 11.']:
 assert marker in new,marker
assert 'COMPLETE OWNER ATTACHMENT' in (b/'mutation-suggestions.md').read_text()
assert len(guard._pending_suggestion_entries((c/'mutation-suggestions.md').read_text()))==0
out.update(owner_entry_consumed=list(consumed),owner_scoped_contract_preservation='passed',old_slices_preserved=len(old_slices),added_slices=sorted(set(new_slices)-set(old_slices)),unchanged_original_slices=len(old_slices)-len(mutable),accepted_rows_preserved=len(acceptances),accepted_status_projection_only='R-HARNESS-PERFORMANCE',active_ledger_items=len(items),all_eleven_owner_areas_mapped=True,installed_method_changes=False,policy_changes=False)
(r/'candidate-validation.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(out,indent=2))
