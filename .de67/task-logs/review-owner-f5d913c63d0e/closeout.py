"""Publish the validated owner-scoped FS/context and request one successor; never launch."""
import hashlib,json,os,sqlite3,sys,tempfile
from pathlib import Path
R=Path(__file__).parent.resolve(); W=R.parents[2]
STATE=Path('/Volumes/CodexBulk/Schanigarten/workspaces/Cataclysm-AOL/.de67/state/deadlines.sqlite3')
LINEAGE='semantic-surface-cockpit';GATE='f5d913c63d0e'
sys.path.insert(0,'/Users/josefhorvath/.codex/skills/de67/de-67-3/scripts')
from deadline_harness import DeadlineHarness
from coordinator_supervisor import mutation_gate,pending_mutation_suggestions
import mutation_guard
from worker_library import returned_assignments

REASON=('Owner-suggestion f5d913c63d0e resolved; evidence .de67/task-logs/review-owner-f5d913c63d0e/report.md. '
        'Deliver refrozen FS S-HARNESS/H-ERGONOMICS: first ingest/resume returned R-HARNESS-EXECUTION-exploration-003 '
        'with preserved fixture repairs and current semantic-readiness correction; independently commission the exact-evidence '
        'repair when ownership permits. The full-stack owner outcome covers session/display, scenario premises, evidence/witness, '
        'continuation/build and varied native campaign proof, not merely the wait patch. Preserve 64 accepted records and all '
        'prior work/evidence ceilings. Read current marked WEC and concrete ledger assignments; Sol implements and validates, '
        'the mutator has changed specification/context only. One fresh coordinator; external supervisor alone launches. '
        'No inherited deadline or duplicate worker input; 50% savings and broader native reliability remain unproved.')

def records(path,table):
    connection=sqlite3.connect('file:'+str(path)+'?mode=ro',uri=True);connection.row_factory=sqlite3.Row
    result=[dict(row) for row in connection.execute('select * from '+table+' order by rowid')];connection.close();return result

def digest(value):return hashlib.sha256(json.dumps(value,sort_keys=True).encode()).hexdigest()

def atomic(path,data):
    fd,temp=tempfile.mkstemp(prefix='.'+path.name+'-',dir=path.parent)
    try:
        with os.fdopen(fd,'wb') as out:out.write(data);out.flush();os.fsync(out.fileno())
        os.replace(temp,path)
    finally:
        if os.path.exists(temp):os.unlink(temp)

if __name__=='__main__':
    gate=mutation_gate(STATE,LINEAGE,W)
    assert gate is not None and gate.kind=='owner-suggestion' and gate.identity==GATE,gate
    assert returned_assignments(W,STATE,LINEAGE)=={'R-HARNESS-EXECUTION-exploration-003'}
    validation=json.loads((R/'candidate-validation.json').read_text())
    assert validation['owner_scoped_contract_preservation']=='passed' and validation['accepted_rows_preserved']==64
    assert (R/'report.md').is_file() and (R/'handoff-reproduction.json').is_file()
    for name in ['FS.md','WEC.md','work-ledger.md','mutation-suggestions.md','test-and-task-guidelines.md']:
        assert (W/'.de67'/name).read_bytes()==(R/'baseline'/name).read_bytes(),('live context changed',name)
    before=json.loads((R/'baseline-state.json').read_text())
    for name,expected in before['dirty_file_hashes'].items():
        if name not in ['.de67/WEC.md','.de67/work-ledger.md','.de67/mutation-suggestions.md']:
            assert hashlib.sha256((W/name).read_bytes()).hexdigest()==expected,('pre-existing work changed',name)
    acceptance_before=records(STATE,'claim_acceptances');tasks_before=records(STATE,'tasks');claims_before=records(STATE,'worker_claims')
    assert len(acceptance_before)==64
    mutation_guard.validate_work_ledger(R/'candidate/work-ledger.md',R/'candidate/FS.md',state=STATE,lineage_id=LINEAGE)
    mutation_guard.validate_owner_suggestion_consumption(R/'baseline/mutation-suggestions.md',R/'candidate/mutation-suggestions.md')
    # Rehearse only the required durable transition on a copy; no copied task routing or launches.
    rehearsal=R/'closeout-rehearsal.sqlite3'
    assert not rehearsal.exists(),'rehearsal already exists; inspect rather than overwrite'
    source=sqlite3.connect('file:'+str(STATE)+'?mode=ro',uri=True);target=sqlite3.connect(rehearsal);source.backup(target);target.close();source.close()
    with DeadlineHarness(rehearsal) as harness:
        first=harness.request_coordinator_restart(LINEAGE,REASON)
        repeated=harness.request_coordinator_restart(LINEAGE,REASON)
    assert first['created'] is True and repeated['created'] is False
    assert first['coordinator_restart']['generation']==repeated['coordinator_restart']['generation']
    assert records(rehearsal,'claim_acceptances')==acceptance_before
    assert records(rehearsal,'tasks')==tasks_before and records(rehearsal,'worker_claims')==claims_before
    (R/'closeout-rehearsal.json').write_text(json.dumps({'first':first,'repeated':repeated,'acceptance_and_tasks_preserved':True},indent=2)+'\n')
    published={}
    for name in ['FS.md','WEC.md','work-ledger.md']:
        data=(R/'candidate'/name).read_bytes();atomic(W/'.de67'/name,data);published[name]=hashlib.sha256(data).hexdigest()
    # Gate consumption is last, after all concrete contract/handoff artifacts are published.
    data=(R/'candidate/mutation-suggestions.md').read_bytes();atomic(W/'.de67/mutation-suggestions.md',data)
    published['mutation-suggestions.md']=hashlib.sha256(data).hexdigest()
    assert pending_mutation_suggestions(W)==()
    assert mutation_gate(STATE,LINEAGE,W) is None
    with DeadlineHarness(STATE) as harness:result=harness.request_coordinator_restart(LINEAGE,REASON)
    assert result['created'] is True
    assert records(STATE,'claim_acceptances')==acceptance_before
    assert records(STATE,'tasks')==tasks_before and records(STATE,'worker_claims')==claims_before
    final={'invocation':'mutation-48bcebae6cd84750aecc249a942956f5','gate':GATE,'resolved':True,
        'owner_entry':'HARNESS-FULL-STACK-20260921','pending_entries':0,'published_sha256':published,
        'accepted_count':len(acceptance_before),'acceptance_rows_sha256':digest(acceptance_before),
        'tasks_and_claims_unchanged':True,'returned_task_preserved':'R-HARNESS-EXECUTION-exploration-003',
        'restart':result,'launched_coordinator':False,'product_or_method_code_changed':False}
    (R/'closeout.json').write_text(json.dumps(final,indent=2)+'\n');print(json.dumps(final,indent=2))
