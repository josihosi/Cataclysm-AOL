from pathlib import Path
import sys,json,sqlite3,hashlib,os,tempfile
p=Path(__file__).resolve().parent;w=p.parents[2];live=Path('/Users/josefhorvath/.codex/skills/de67/de-67-3');state=w/'.de67/state/deadlines.sqlite3';lineage='semantic-surface-cockpit'
sys.path.insert(0,str(live/'scripts'))
from mutation_guard import validate_method_mutation,validate_owner_suggestion_consumption
from coordinator_supervisor import mutation_gate,pending_mutation_suggestions
from deadline_harness import DeadlineHarness
assert mutation_gate(state,lineage,w).identity=='1e8c79b5fdf2'
changes=validate_method_mutation(p/'baseline',p/'candidate',universal=False)
assert list(changes)==json.loads((p/'validation.json').read_text())['method_changed']
updates=[dict(target=str(live/f),baseline=str(p/'baseline'/f),candidate=str(p/'candidate'/f)) for f in changes]
updates+=json.loads((p/'extra-edits.json').read_text())
updates.append(dict(target=str(w/'.de67/state/workspace.json'),baseline=str(p/'workspace.json'),candidate=str(p/'workspace.candidate.json')))
for e in updates:assert Path(e['target']).read_bytes()==Path(e['baseline']).read_bytes(),e['target']
queue=w/'.de67/mutation-suggestions.md';assert queue.read_bytes()==(p/'mutation-suggestions.md').read_bytes()
consumed=queue.read_text().split('## Pending suggestions')[0]+'## Pending suggestions\n'
(p/'queue.candidate.md').write_text(consumed);assert len(validate_owner_suggestion_consumption(p/'mutation-suggestions.md',p/'queue.candidate.md'))==1
reason='Owner-suggestion 1e8c79b5fdf2 resolved: GPT-6-only model guidance, capability roster and dispatch validation promoted; evidence .de67/task-logs/review-owner-1e8c79b5fdf2/report.md. Preserve live ownership and accepted proof; restored model/effort preferences are not quotas. One fresh coordinator; external supervisor owns launch.'
tables=['tasks','claim_acceptances','worker_claims','closure_gaps']
def rows(path):
 with sqlite3.connect(path) as db:return {t:db.execute('select * from '+t).fetchall() for t in tables}
before=rows(state);assert before==rows(p/'before.sqlite3')
copy=p/'closeout-rehearsal.sqlite3'
with sqlite3.connect(state) as a,sqlite3.connect(copy) as b:a.backup(b)
with DeadlineHarness(copy) as h:
 first=h.request_coordinator_restart(lineage,reason);again=h.request_coordinator_restart(lineage,reason)
assert first['created'] and not again['created'];assert rows(copy)==before
(p/'closeout-rehearsal.json').write_text(json.dumps({'first':first,'again':again,'preserved':True},indent=2))
def atomic(path,data):
 fd,tmp=tempfile.mkstemp(dir=path.parent,prefix='.'+path.name)
 try:
  with os.fdopen(fd,'wb') as f:f.write(data);f.flush();os.fsync(f.fileno())
  os.chmod(tmp,path.stat().st_mode & 0o777);os.replace(tmp,path)
 finally:
  if os.path.exists(tmp):os.unlink(tmp)
hashes={}
for e in updates:
 target=Path(e['target']);data=Path(e['candidate']).read_bytes();atomic(target,data);hashes[str(target)]=hashlib.sha256(data).hexdigest()
# Consume only after promotion; immutable original owner input remains in this receipt directory.
atomic(queue,consumed.encode());assert not pending_mutation_suggestions(w);assert mutation_gate(state,lineage,w) is None
with DeadlineHarness(state) as h:result=h.request_coordinator_restart(lineage,reason)
assert result['created'];assert rows(state)==before
receipt=dict(gate='1e8c79b5fdf2',invocation='mutation-5bf04ec679c9488fa85a372cfe56cce0',resolved=True,owner_entries_consumed=1,accepted_records=len(before['claim_acceptances']),tasks_and_claims_preserved=True,published_sha256=hashes,restart=result,coordinator_launched=False)
(p/'closeout.json').write_text(json.dumps(receipt,indent=2));print(json.dumps({k:v for k,v in receipt.items() if k!='published_sha256'}))
