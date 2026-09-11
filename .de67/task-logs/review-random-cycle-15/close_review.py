from pathlib import Path
import sqlite3,json,sys
sys.path.insert(0,'/Users/josefhorvath/.codex/skills/de67/de-67-3/scripts')
from deadline_harness import DeadlineHarness
from mutation_guard import method_tree_digest
r=Path(__file__).resolve().parents[3];b=r/'.de67/state/review-random-cycle-15';s=r/'.de67/state/deadlines.sqlite3';report='.de67/task-logs/review-random-cycle-15/report.md';lineage='semantic-surface-cockpit'
def close(path):
 with DeadlineHarness(path) as h:
  cycle=h.connection.execute('SELECT due_task_id,resolution_evidence,universal_required FROM random_mutation_cycles WHERE lineage_id=? AND cycle_number=15',(lineage,)).fetchone();assert tuple(cycle)==('R-037-coverage-manifest-reconciliation-005',None,0),tuple(cycle)
  return h.resolve_random_mutation(lineage,15,report+'; Guarded method/FS no-op. Task004/005 demonstrate use and honest proof ceilings. Isolated acceptance probes refute stale maintenance metadata blockers; current ledger replaces those blockers and obsolete R037 frontier. Existing deferred supervisor-loop adoption check remains conditional on a future authorized external start.')
copy=b/'closeout-reproduction.sqlite3'
with sqlite3.connect(s) as source,sqlite3.connect(copy) as destination:source.backup(destination)
result=close(copy);(b/'closeout-reproduction.json').write_text(json.dumps(result,indent=2))
if '--apply' in sys.argv:
 with sqlite3.connect(b/'before.sqlite3') as before,sqlite3.connect(s) as current:
  for table in ('tasks','claim_clocks','claim_deadline_generations','claim_acceptances','worker_claims','worker_checkpoints','closure_gaps'):
   assert before.execute('SELECT * FROM '+table).fetchall()==current.execute('SELECT * FROM '+table).fetchall(),table
 assert method_tree_digest()=='dd806e69181e1296c99b1ffaf17438986f643f350469feb14235d3700749975e'
 result=close(s);(Path(__file__).parent/'closeout.json').write_text(json.dumps(result,indent=2))
print(json.dumps(result))
