from pathlib import Path
import sys,json,sqlite3,hashlib,os,tempfile
p=Path(__file__).resolve().parent;w=p.parents[2];live=Path('/Users/josefhorvath/.codex/skills/de67/de-67-3');state=w/'.de67/state/deadlines.sqlite3';lineage='semantic-surface-cockpit'
sys.path.insert(0,str(live/'scripts'))
from mutation_guard import validate_method_mutation,validate_owner_suggestion_consumption
from coordinator_supervisor import mutation_gate,pending_mutation_suggestions
from deadline_harness import DeadlineHarness
assert mutation_gate(state,lineage,w).identity=='cc464a7ab66d'
updates=[dict(target=str(w/'.de67'/f),baseline=str(p/'baseline'/f),candidate=str(p/'candidate'/f)) for f in ['FS.md','WEC.md','work-ledger.md']]
for e in updates:assert Path(e['target']).read_bytes()==Path(e['baseline']).read_bytes(),e['target']
queue=w/'.de67/mutation-suggestions.md';assert queue.read_bytes()==(p/'baseline/mutation-suggestions.md').read_bytes()
consumed=(p/'candidate/mutation-suggestions.md').read_text()
assert len(validate_owner_suggestion_consumption(p/'baseline/mutation-suggestions.md',p/'candidate/mutation-suggestions.md'))==1
exec(compile((p/'validate.py').read_text(),str(p/'validate.py'),'exec'))
reason='Owner-suggestion cc464a7ab66d dispositioned: Windows ordinary-wait camp crash is FIRST delivery priority, R-CAOL-WINDOWS-DISPATCH-CRASH-exploration-001. Sol diagnose/repair, Luna live proof; read .de67/task-logs/review-owner-cc464a7ab66d/report.md and marked WEC. Windows/WSL owner reservation remains; start authorized Mac diagnosis. Cause and Windows regression remain open; preserve all accepted/returned work. Request one fresh coordinator; external supervisor alone launches, any owner stop remains binding.'
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
receipt=dict(gate='cc464a7ab66d',invocation='mutation-990799a543b54300948b1b49df74aee6',resolved=True,owner_entries_consumed=1,accepted_records=len(before['claim_acceptances']),tasks_and_claims_preserved=True,published_sha256=hashes,restart=result,coordinator_launched=False)
(p/'closeout.json').write_text(json.dumps(receipt,indent=2));print(json.dumps({k:v for k,v in receipt.items() if k!='published_sha256'}))
