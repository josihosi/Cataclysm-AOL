from pathlib import Path
import sqlite3,json,sys
p=Path(__file__).resolve().parent;w=p.parents[2];state=w/'.de67/state/deadlines.sqlite3';lineage='semantic-surface-cockpit';claim='R-CAOL-CAMP-WAIT-COST'
sys.path.insert(0,'/Users/josefhorvath/.codex/skills/de67/de-67-3/scripts')
from deadline_harness import DeadlineHarness
from coordinator_supervisor import mutation_gate,pending_mutation_suggestions
assert mutation_gate(state,lineage,w).identity=='R-CAOL-CAMP-WAIT-COST-exploration-002'
assert not pending_mutation_suggestions(w)
for name in ['FS.md','WEC.md','work-ledger.md','mutation-suggestions.md']:assert (w/'.de67'/name).read_bytes()==(p/'baseline'/name).read_bytes()
tables=['tasks','worker_claims','worker_checkpoints','claim_acceptances','closure_gaps']
def retained(path):
 with sqlite3.connect(path) as db:return {t:db.execute('select * from '+t+' order by rowid').fetchall() for t in tables}
before=retained(state);assert before==retained(p/'before.sqlite3')
short='Owner-required Astra absent after roster replacement; Sol prep returned, implementation blocked and deadline elapsed during stopped delivery.'
evidence='.de67/task-logs/review-camp-wait-20260925/report.md and validation.json. Exact result/checkpoint3 retained; installed roster/setup preservation correction unchanged and tested; 17 GPT-6 choices including Astra higher effort verified. Existing exploration-003 is the concrete Astra implementation handoff; saved minute7994 baseline and independent friendly-NPC diagnosis preserved. Product fix remains open.'
micro='Reuse validated current handoff R-CAOL-CAMP-WAIT-COST-exploration-003 with original Sol prep; Astra repairs duplicate contact/log volume and full-file reread, named Luna verifies equal-game-time saved-run behavior. Do not redo completed profiling or claim a product pass. '+evidence
macro='Reviewed no additional method change: roster-reset cause already corrected and reproduced by partial-refreeze regression; effort is a preference, not restriction. Installed candidate byte identity and current roster checked; reuse prior guard/tests and preceding review rather than introduce another policy. '+evidence

def resolve(path):
 with DeadlineHarness(path) as h:
  d=h.diagnose_claim_deadline(lineage,claim,short,evidence)
  a=h.resolve_deadline_mutation(lineage,claim,'micro',micro)
  b=h.resolve_deadline_mutation(lineage,claim,'macro',macro,no_change_required=True)
  return {'diagnosis':d,'micro':a,'macro':b}
copy=p/'rehearsal.sqlite3'
with sqlite3.connect(state) as a,sqlite3.connect(copy) as b:a.backup(b)
trial=resolve(copy);assert retained(copy)==before
with sqlite3.connect(state) as c:restart_before=c.execute('select * from coordinator_restart_requests order by generation').fetchall()
with sqlite3.connect(copy) as c:assert c.execute('select * from coordinator_restart_requests order by generation').fetchall()==restart_before
(p/'rehearsal.json').write_text(json.dumps(trial,indent=2))
result=resolve(state);assert retained(state)==before
remaining=mutation_gate(state,lineage,w);assert remaining is None or remaining.identity!='R-CAOL-CAMP-WAIT-COST-exploration-002'
with sqlite3.connect(state) as c:
 assert c.execute('select * from coordinator_restart_requests order by generation').fetchall()==restart_before
receipt={'gate':'R-CAOL-CAMP-WAIT-COST-exploration-002','invocation':'mutation-287ccffa4af64e389f32108be90c3051','resolved':True,'accepted_records_preserved':len(before['claim_acceptances']),'tasks_claims_checkpoints_gaps_preserved':True,'pending_owner_entries':0,'remaining_gate':None if remaining is None else remaining.identity,'restart_coalesced':result['macro']['coordinator_restart'],'coordinator_launched':False,'lifecycle':result}
(p/'closeout.json').write_text(json.dumps(receipt,indent=2));print(json.dumps({k:v for k,v in receipt.items() if k!='lifecycle'}))
