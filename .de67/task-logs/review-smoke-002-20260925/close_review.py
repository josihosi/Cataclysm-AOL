from pathlib import Path
import sqlite3,json,sys,os,tempfile,subprocess,hashlib
r=Path(__file__).resolve().parent;w=r.parents[2];state=w/'.de67/state/deadlines.sqlite3'
installed=Path('/Users/josefhorvath/.codex/skills/de67/de-67-3');lab=Path('/Volumes/CodexBulk/Schanigarten/workspaces/de67-lab/de-67-3')
sys.path.insert(0,str(installed/'scripts'))
from deadline_harness import DeadlineHarness
from coordinator_supervisor import mutation_gate,pending_mutation_suggestions
import mutation_guard as g
lineage='semantic-surface-cockpit';claim='R-CAOL-FIRST-SMOKE';task=claim+'-exploration-002'
assert mutation_gate(state,lineage,w).identity==task
assert len(pending_mutation_suggestions(w))==4
assert (r/'report.md').is_file()
for name in ['FS.md','WEC.md','work-ledger.md','mutation-suggestions.md']:
 assert (w/'.de67'/name).read_bytes()==(r/'baseline'/name).read_bytes(),name
assert (w/'.agents/skills/caol-harness/references/live-operation.md').read_bytes()==(r/'live-operation.before.md').read_bytes()
changed=g.validate_method_mutation(r/'method-baseline',r/'method-candidate',universal=False)
for n in changed:
 assert (installed/n).read_bytes()==(r/'method-baseline'/n).read_bytes(),n
 assert (lab/n).read_bytes()==(r/'lab-baseline'/n).read_bytes(),n
assert len(g.validate_work_ledger(r/'work-ledger.candidate.md',w/'.de67/FS.md',state=state,lineage_id=lineage))==17
queue=(r/'baseline/mutation-suggestions.md').read_text().split('## Pending suggestions')[0]+'## Pending suggestions\n\n'
(r/'mutation-suggestions.candidate.md').write_text(queue)
assert len(g.validate_owner_suggestion_consumption(r/'baseline/mutation-suggestions.md',r/'mutation-suggestions.candidate.md'))==4
snapshot=g.normal_method_candidate_snapshot(r/'method-candidate/assets/environment',r/'method-candidate')
tables=['tasks','worker_claims','worker_checkpoints','claim_acceptances','closure_gaps']
def retained(path):
 with sqlite3.connect(path) as db:return {t:db.execute('select * from '+t+' order by rowid').fetchall() for t in tables}
before=retained(state);assert before==retained(r/'before.sqlite3')
short='Full smoke ecology remains unproved: scout return/identity repair open; collection guidance and role-mixed owner context caused avoidable friction.'
evidence='.de67/task-logs/review-smoke-002-20260925/report.md; original transcripts, harness-excerpts.json, usage.json, production rendered contexts and passing candidate tests. Preserve saved original8454 and retained repair8043, partial roof proof, accepted log-cost work and all unfinished tasks. Four pending owner entries dispositioned.'
micro='Simplified exact pending-result collection guidance, clarified established named ownership, preserved current ecology-repair priority and independent proof, commissioned Sol save-result usability under smoke exploration-004. No false task completion, replay or private-save changes. '+evidence
macro='Role-scoped owner-context producer and packet freshness validated: coordinator retains assignment instructions; worker receives implementation/outcome constraints and required Luna live-operation boundary. No blanket helper ban; no native-reconciliation candidate promotion. '+evidence
# Rehearse the exact diagnosed receipt against the isolated candidate runtime and copied state.
trial=r/'rehearsal.sqlite3'
with sqlite3.connect(state) as src,sqlite3.connect(trial) as dst:src.backup(dst)
with DeadlineHarness(trial) as h: h.diagnose_claim_deadline(lineage,claim,short,evidence)
candidate_digest,protected,live,paths=snapshot
receipt=g.persist_normal_method_receipt(trial,lineage,task,'deadline_miss',candidate_digest,paths,protected,live)
config=dict(lineage=lineage,claim=claim,micro=micro,macro=macro,receipt=receipt)
(r/'resolution-input.json').write_text(json.dumps(config))
code='''from pathlib import Path
import sys,json
sys.path.insert(0,sys.argv[1])
from deadline_harness import DeadlineHarness
c=json.loads(Path(sys.argv[3]).read_text())
with DeadlineHarness(sys.argv[2]) as h:
 a=h.resolve_deadline_mutation(c['lineage'],c['claim'],'micro',c['micro'])
 b=h.resolve_deadline_mutation(c['lineage'],c['claim'],'macro',c['macro'],receipt_id=c['receipt'])
Path(sys.argv[4]).write_text(json.dumps({'micro':a,'macro':b},indent=2))
'''
subprocess.run(['/opt/homebrew/opt/python@3.14/bin/python3.14','-c',code,str(r/'method-candidate/scripts'),str(trial),str(r/'resolution-input.json'),str(r/'rehearsal.json')],check=True)
assert retained(trial)==before
with sqlite3.connect(state) as db:old_count=db.execute('select count(*) from coordinator_restart_requests where lineage_id=?',(lineage,)).fetchone()[0]
with sqlite3.connect(trial) as db:assert db.execute('select count(*) from coordinator_restart_requests where lineage_id=?',(lineage,)).fetchone()[0]==old_count+1
# Live diagnosis and guard receipt are durable before candidate promotion. No gate is resolved yet.
with DeadlineHarness(state) as h:diagnosis=h.diagnose_claim_deadline(lineage,claim,short,evidence)
live_receipt=g.persist_normal_method_receipt(state,lineage,task,'deadline_miss',candidate_digest,paths,protected,live)
assert live_receipt==receipt

def atomic(target,data):
 fd,tmp=tempfile.mkstemp(dir=target.parent,prefix='.mutation-')
 with os.fdopen(fd,'wb') as f:f.write(data);f.flush();os.fsync(f.fileno())
 os.chmod(tmp,target.stat().st_mode & 0o777);os.replace(tmp,target)
for n in changed:
 atomic(installed/n,(r/'method-candidate'/n).read_bytes())
 atomic(lab/n,(r/'lab-candidate'/n).read_bytes())
assert g.method_tree_digest()==candidate_digest
assert g.protected_method_digest()==protected
for source,target in [('WEC.candidate.md',w/'.de67/WEC.md'),('work-ledger.candidate.md',w/'.de67/work-ledger.md'),('live-operation.candidate.md',w/'.agents/skills/caol-harness/references/live-operation.md'),('mutation-suggestions.candidate.md',w/'.de67/mutation-suggestions.md')]:atomic(target,(r/source).read_bytes())
assert not pending_mutation_suggestions(w)
with DeadlineHarness(state) as h:
 a=h.resolve_deadline_mutation(lineage,claim,'micro',micro)
 b=h.resolve_deadline_mutation(lineage,claim,'macro',macro,receipt_id=receipt)
assert retained(state)==before
assert (w/'.de67/FS.md').read_bytes()==(r/'baseline/FS.md').read_bytes()
remaining=mutation_gate(state,lineage,w)
assert remaining is None or remaining.identity!=task
with sqlite3.connect(state) as db:
 assert db.execute('select count(*) from coordinator_restart_requests where lineage_id=?',(lineage,)).fetchone()[0]==old_count+1
 restart=db.execute('select generation,reason,requested_at,acknowledged_at from coordinator_restart_requests where lineage_id=? order by generation desc limit 1',(lineage,)).fetchone()
result=dict(invocation='mutation-5a250fc4b2334763826852949d9df571',gate=task,resolved=True,method_receipt=receipt,method_files=changed,owner_entries_consumed=4,accepted_records_preserved=len(before['claim_acceptances']),retained_tasks_claims_checkpoints_gaps=True,FS_unchanged=True,restart=restart,coordinator_launched=False,next_gate=None if remaining is None else remaining.identity,lifecycle=dict(diagnosis=diagnosis,micro=a,macro=b))
(r/'closeout.json').write_text(json.dumps(result,indent=2));print(json.dumps({k:v for k,v in result.items() if k!='lifecycle'},indent=2))
