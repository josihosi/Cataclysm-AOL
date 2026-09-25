from pathlib import Path
import sqlite3,json,sys,os,tempfile
p=Path(__file__).resolve().parent;w=p.parents[2];state=w/'.de67/state/deadlines.sqlite3';lineage='semantic-surface-cockpit';claim='R-CAOL-FIRST-SMOKE'
sys.path.insert(0,'/Users/josefhorvath/.codex/skills/de67/de-67-3/scripts')
from deadline_harness import DeadlineHarness
from coordinator_supervisor import mutation_gate,pending_mutation_suggestions
from mutation_guard import validate_work_ledger
assert mutation_gate(state,lineage,w).identity=='R-CAOL-FIRST-SMOKE-exploration-001'
assert not pending_mutation_suggestions(w)
for name in ['FS.md','WEC.md','work-ledger.md','mutation-suggestions.md']:
 assert (w/'.de67'/name).read_bytes()==(p/'baseline'/name).read_bytes(),name
validate_work_ledger(p/'work-ledger.candidate.md',w/'.de67/FS.md',state=state,lineage_id=lineage)
tables=['tasks','worker_claims','worker_checkpoints','claim_acceptances','closure_gaps']
def retained(path):
 with sqlite3.connect(path) as db:return {t:db.execute('select * from '+t+' order by rowid').fetchall() for t in tables}
before=retained(state);assert before==retained(p/'before.sqlite3')
short='Smoke fire proved; incomplete light/continuation proof, log flood and setup detours; native returned-turn routing gap held review.'
evidence='.de67/task-logs/review-first-smoke-20260925/report.md; exact checkpoints, native return at physical line9729, quiescence counterexample and rendered successor briefs retained. No new acceptance. Ordinary ladder route and saved minute7994 continuation retained; Astra log repair first for closed-run replay. Existing roster/reminder fix reused; native return method candidate commissioned for Sol.'
micro='Validated concrete recovery handoff and ledger ownership projection: same saved-world continuation and ordinary ladder route instead of failed teleports; named Luna returned-turn route while Sol stages native-turn reconciliation. Preserve partial proof and all accepted work. '+evidence
macro='No additional installed method change required in this review: reuse validated Astra roster/setup and supervisor/Q1 reminder correction from astra-winddown-20260925. Do not attribute Q1 to historical released rows; verified native-return gap needs the commissioned authenticated transport candidate, not speculative state clearing. '+evidence

def resolve(path):
 with DeadlineHarness(path) as h:
  diagnosis=h.diagnose_claim_deadline(lineage,claim,short,evidence)
  a=h.resolve_deadline_mutation(lineage,claim,'micro',micro)
  b=h.resolve_deadline_mutation(lineage,claim,'macro',macro,no_change_required=True)
  # Resolving the last component itself requests the one semantic restart.
  return {'diagnosis':diagnosis,'micro':a,'macro':b}
copy=p/'rehearsal.sqlite3'
with sqlite3.connect(state) as src,sqlite3.connect(copy) as dst:src.backup(dst)
trial=resolve(copy);assert retained(copy)==before
with sqlite3.connect(copy) as c:
 generations=c.execute('select generation from coordinator_restart_requests where lineage_id=?',(lineage,)).fetchall()
with sqlite3.connect(state) as c:
 original=c.execute('select generation from coordinator_restart_requests where lineage_id=?',(lineage,)).fetchall()
assert len(generations)==len(original)+1
(p/'rehearsal.json').write_text(json.dumps(trial,indent=2))
# Publish the validated context before resolving the gate.
target=w/'.de67/work-ledger.md';fd,tmp=tempfile.mkstemp(dir=target.parent,prefix='.work-ledger-review-')
with os.fdopen(fd,'wb') as f:f.write((p/'work-ledger.candidate.md').read_bytes());f.flush();os.fsync(f.fileno())
os.chmod(tmp,target.stat().st_mode & 0o777);os.replace(tmp,target)
result=resolve(state);assert retained(state)==before
remaining = mutation_gate(state,lineage,w)
assert remaining is None or remaining.identity != 'R-CAOL-FIRST-SMOKE-exploration-001'
with sqlite3.connect(state) as c:
 restart=c.execute('select generation,reason,requested_at from coordinator_restart_requests where lineage_id=? order by generation desc limit 1',(lineage,)).fetchone()
 assert c.execute('select count(*) from coordinator_restart_requests where lineage_id=?',(lineage,)).fetchone()[0]==len(original)+1
receipt={'invocation':'mutation-1bd9283b6edc4a4a8a1de949e28d7376','gate':'R-CAOL-FIRST-SMOKE-exploration-001','resolved':True,'owner_pending_entries':0,'accepted_records_preserved':len(before['claim_acceptances']),'retained_tasks_claims_checkpoints_gaps':True,'restart':restart,'coordinator_launched':False,'lifecycle':result}
(p/'closeout.json').write_text(json.dumps(receipt,indent=2));print(json.dumps({k:v for k,v in receipt.items() if k!='lifecycle'}))
