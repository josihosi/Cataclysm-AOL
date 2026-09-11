from pathlib import Path
import sqlite3,json,sys
sys.path.insert(0,'/Users/josefhorvath/.codex/skills/de67/de-67-3/scripts')
from deadline_harness import DeadlineHarness
r=Path(__file__).resolve().parents[3];b=r/'.de67/state/review-r026-generation-006';s=r/'.de67/state/deadlines.sqlite3';report='.de67/task-logs/review-r026-generation-006/report.md'
verdict='Provenance composition finished on time; whole-package acceptance remains dependent on unresolved R-037 coverage.'
diagnosis=report+'; R026 generation6 deadline1789084156.396244 expired without whole-claim acceptance. Its source task completed1789083040.1754072 and gap closed1789083040.2413828. Preserve completed proof. R037 task003 now has a finding and a separate pending generation2 review; no new method defect is established by this whole-package miss. Reconcile stale ledger status without replay, invented acceptance or API credential access.'
def close(path):
 with DeadlineHarness(path) as h:
  row=h.connection.execute("SELECT generation,source_task_id,reviewed_at FROM claim_deadline_generation_incidents WHERE lineage_id=? AND claim_id=? ORDER BY generation DESC LIMIT 1",('semantic-surface-cockpit','R-026')).fetchone();assert tuple(row)==(6,'R-026-closure-r031-provenance-composition-001',None),tuple(row)
  task_before=tuple(h.connection.execute("SELECT * FROM tasks WHERE task_id='R-026-closure-r031-provenance-composition-001'").fetchone())
  h.diagnose_claim_deadline('semantic-surface-cockpit','R-026',verdict,diagnosis)
  micro=h.resolve_deadline_mutation('semantic-surface-cockpit','R-026','micro',report+'; Refresh ledger to preserve completed compositions and distinguish the separately pending R037 finding/review from active work.')
  macro=h.resolve_deadline_mutation('semantic-surface-cockpit','R-026','macro',report+'; No method change required: whole-claim deadline and completed component task are distinct; existing continuation and evidence contracts suffice. No rule added for a dependency delay.',no_change_required=True)
  assert task_before==tuple(h.connection.execute("SELECT * FROM tasks WHERE task_id='R-026-closure-r031-provenance-composition-001'").fetchone())
  return {'micro':micro,'macro':macro,'source_task_preserved':True}
copy=b/'closeout-reproduction.sqlite3'
with sqlite3.connect(s) as source,sqlite3.connect(copy) as destination:source.backup(destination)
result=close(copy);(b/'closeout-reproduction.json').write_text(json.dumps(result,indent=2))
if '--apply' in sys.argv:
 result=close(s);(Path(__file__).parent/'closeout.json').write_text(json.dumps(result,indent=2))
print(json.dumps({'macro':result['macro']['disposition'],'restart':result['macro']['coordinator_restart'],'source_task_preserved':True}))
