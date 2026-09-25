from pathlib import Path
import sys,json,sqlite3
sys.path.insert(0,'/Users/josefhorvath/.codex/skills/de67/de-67-3/scripts')
from deadline_harness import DeadlineHarness
p=Path(__file__).resolve().parent;s=p.parents[1]/'state/deadlines.sqlite3';lineage='semantic-surface-cockpit';evidence='.de67/task-logs/review-random-cycle-20/report.md; empty owner queue; guarded method/FS/guideline no-op, validated recovery handoff and preserved70 acceptances; Sol candidate tooling and Luna runtime recovery remain unproved.'
def close(path):
 with DeadlineHarness(path) as h:
  c=h.connection.execute('select due_task_id,resolution_evidence,universal_required from random_mutation_cycles where lineage_id=? and cycle_number=20',(lineage,)).fetchone()
  assert tuple(c)==('R-LAUNCHER-OLLAMA-SETUP-closure-setup-proof-binding-001',None,0),tuple(c)
  n=h.connection.execute('select count(*) from coordinator_restart_requests').fetchone()[0]
  before={t:h.connection.execute('select * from '+t).fetchall() for t in ['tasks','claim_acceptances','worker_claims','closure_gaps']}
  r=h.resolve_random_mutation(lineage,20,evidence)
  assert h.connection.execute('select count(*) from coordinator_restart_requests').fetchone()[0]==n+1
  for t,rows in before.items():assert rows==h.connection.execute('select * from '+t).fetchall(),t
  return r
copy=p/'closeout-reproduction.sqlite3'
with sqlite3.connect(s) as a,sqlite3.connect(copy) as b:a.backup(b)
r=close(copy);(p/'closeout-reproduction.json').write_text(json.dumps(r,indent=2))
if '--apply' in sys.argv:
 r=close(s);(p/'closeout.json').write_text(json.dumps(r,indent=2))
print(json.dumps(r))
