from pathlib import Path
import sys,json,tempfile,shutil
p=Path(__file__).resolve().parent; w=p.parents[2]
sys.path.insert(0,'/Users/josefhorvath/.codex/skills/de67/de-67-3/scripts')
from mutation_guard import validate_work_ledger,validate_owner_suggestion_consumption,_stable_claim_records
from policy_kernel import current_owner_contract
from context_library import prepare,selected_context,task_view
b=p/'baseline';c=p/'candidate';claim='R-CAOL-WINDOWS-DISPATCH-CRASH';task=claim+'-exploration-001'
old=(b/'FS.md').read_text();new=(c/'FS.md').read_text()
assert new.startswith(old)
oldclaims=_stable_claim_records(old);newclaims=_stable_claim_records(new)
assert newclaims[:-1]==oldclaims
assert newclaims[-1][0]==claim and newclaims[-1][1]==' ' and newclaims[-1][2]
ledger=validate_work_ledger(c/'work-ledger.md',c/'FS.md',state=w/'.de67/state/deadlines.sqlite3',lineage_id='semantic-surface-cockpit')
assert ledger[0].startswith(claim)
assert len(validate_owner_suggestion_consumption(b/'mutation-suggestions.md',c/'mutation-suggestions.md'))==1
# Removing precisely the new block and priority header must restore the whole old ledger.
text=(c/'work-ledger.md').read_text();start=text.index('- [ ] '+claim);end=text.index('- [ ] R-CAOL-REFERENCE-CAMP',start)
assert (text[:start]+text[end:]).split('\n\n',1)[1]==(b/'work-ledger.md').read_text()
with tempfile.TemporaryDirectory() as d:
 root=Path(d);(root/'.de67').mkdir()
 for f in ['WEC.md','work-ledger.md']:shutil.copy(c/f,root/'.de67'/f)
 owner=current_owner_contract(root)
 assignment=next(x for x in text.splitlines() if x.startswith('  - Assignment '+task+':'))
 prepare(root,task,assignment,handoff=owner)
 rendered=selected_context(root,task)
 assert task_view(root,task)['assignment_revision']
 for phrase in ['ordinary waiting until dispatch','current Windows/WSL owner reservation','first delivery priority after this mutation','earlier natural-dispatch attempts']:
  assert phrase in rendered,phrase
 (p/'handoff-reproduction.txt').write_text(rendered)
result={'fs_exact_baseline_prefix':True,'all_prior_claims_unchanged':True,'new_red_claim':claim,'ledger_guard_pass':True,'old_ledger_exactly_preserved':True,'owner_entry_dispositioned':1,'real_context_prepare_render_pass':True,'future_worker_delivery_or_use_proven':False,'standard_owner_fs_guard':'pre-existing missing legacy Functional contract heading; explicit owner-authorized append validated by exact byte-prefix and stable-claim preservation, no installed guard modified'}
(p/'validation.json').write_text(json.dumps(result,indent=2));print(json.dumps(result))
