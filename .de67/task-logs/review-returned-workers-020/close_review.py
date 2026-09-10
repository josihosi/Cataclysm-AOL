from pathlib import Path
import json,sqlite3,sys
sys.path.insert(0,'/Users/josefhorvath/.codex/skills/de67/de-67-3/scripts')
from deadline_harness import DeadlineHarness
import mutation_guard as guard
root=Path(__file__).resolve().parents[3]
state=root/'.de67/state/deadlines.sqlite3'
base=root/'.de67/state/review-returned-workers-020'
lineage='semantic-surface-cockpit';task='R-029-closure-sound-information-semantics-001'
report='.de67/task-logs/review-returned-workers-020/report.md'
validation=json.loads((base/'validation.json').read_text())
assert guard.method_tree_digest()==validation['method_digest']
assert guard.protected_method_digest()==validation['protected_digest']
verdict='Returned partial work is valid; unfinished task rows incorrectly blocked review and mandatory Git checkpoints stopped continuation.'
diagnosis=report+'; Owner authorized removal of these method restrictions. Same-coordinator resume already works; fresh-coordinator continuation and quiet-turn review are now regression-tested. No investigation of why workers returned. Native sound/natural route remains incomplete; preserve generation20 miss and normalized prior attempt evidence. Live supervisor-loop adoption remains explicitly deferred.'

def finish(target):
 with DeadlineHarness(target) as h:
  row=h.connection.execute('SELECT generation,source_task_id,reviewed_at FROM claim_deadline_generation_incidents WHERE lineage_id=? AND claim_id=? ORDER BY generation DESC LIMIT 1',(lineage,'R-029')).fetchone()
  assert row['generation']==20 and row['source_task_id']==task and row['reviewed_at'] is None,dict(row)
  h.diagnose_incident(lineage,task,'deadline_miss',verdict,diagnosis)
 receipt=guard.persist_normal_method_receipt(target,lineage,task,'deadline_miss',validation['method_digest'],tuple(validation['method_changed']),validation['protected_digest'],validation['method_digest'])
 with DeadlineHarness(target) as h:
  micro=h.resolve_deadline_mutation(lineage,'R-029','micro',report+'; Preserve partial results, normalized administrative history and completed activation proof; current ledger carries separate continuation assignments.')
  macro=h.resolve_deadline_mutation(lineage,'R-029','macro',report+'; Owner-authorized broader method validation and regression proof; source repair installed, live-parent adoption remainder is explicitly queued.',receipt_id=receipt)
  return {'receipt':receipt,'micro':micro,'macro':macro}
copy=base/'closeout-reproduction.sqlite3'
with sqlite3.connect(state) as source,sqlite3.connect(copy) as destination:source.backup(destination)
reproduction=finish(copy)
(base/'closeout-reproduction.json').write_text(json.dumps(reproduction,indent=2))
if '--apply' in sys.argv:
 result=finish(state)
 (Path(__file__).parent/'closeout.json').write_text(json.dumps(result,indent=2))
 print(json.dumps({'receipt':result['receipt'],'restart':result['macro']['coordinator_restart']}))
else:print(json.dumps({'reproduction_receipt':reproduction['receipt'],'restart':reproduction['macro']['coordinator_restart']}))
