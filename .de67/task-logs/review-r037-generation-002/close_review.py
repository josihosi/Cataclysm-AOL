from pathlib import Path
import sqlite3,json,sys
sys.path.insert(0,'/Users/josefhorvath/.codex/skills/de67/de-67-3/scripts')
from deadline_harness import DeadlineHarness
r=Path(__file__).resolve().parents[3];b=r/'.de67/state/review-r037-generation-002';s=r/'.de67/state/deadlines.sqlite3';report='.de67/task-logs/review-r037-generation-002/report.md';lineage='semantic-surface-cockpit';task='R-037-native-consequence-fixture-repair-003'
verdict='Partial fixture and native proof preserved; secondary item response-to-NPC consumer application remains unproved.'
diagnosis=report+'; Existing terminal pickup instrumentation is present. The final run-bound stream has two unrelated intent events and no pickup status; shared runner selection and inspected inventory absence do not establish native application. Delivered task003 already authorized observation/fixture repair. Trace response identity, current-NPC queue/pending state and ordinary consumer before another unchanged run. No specific final dispatch defect or missing logger is proven. Mandatory API controls retain the independent safe owner-provisioning boundary.'
def close(path):
 with DeadlineHarness(path) as h:
  row=h.connection.execute('SELECT generation,source_task_id,reviewed_at FROM claim_deadline_generation_incidents WHERE lineage_id=? AND claim_id=? ORDER BY generation DESC LIMIT 1',(lineage,'R-037')).fetchone();assert tuple(row)==(2,task,None),tuple(row)
  before=tuple(h.connection.execute('SELECT * FROM tasks WHERE lineage_id=? AND task_id=?',(lineage,task)).fetchone())
  h.diagnose_claim_deadline(lineage,'R-037',verdict,diagnosis)
  micro=h.resolve_deadline_mutation(lineage,'R-037','micro',report+'; Current ledger commissions task004 for exact response/queue/consumer correlation and causally necessary repair, retaining task003 terminal history and proof ceilings. Existing repair authority applies; no new owner gate.')
  macro=h.resolve_deadline_mutation(lineage,'R-037','macro',report+'; No method change required. Existing contracts already authorize bounded repository observation/repair and preserve nonterminal evidence. Repair the current handoff; do not add a rule for a final dispatch cause not established by the evidence.',no_change_required=True)
  assert before==tuple(h.connection.execute('SELECT * FROM tasks WHERE lineage_id=? AND task_id=?',(lineage,task)).fetchone())
  return {'micro':micro,'macro':macro,'source_task_preserved':True}
copy=b/'closeout-reproduction.sqlite3'
with sqlite3.connect(s) as source,sqlite3.connect(copy) as destination:source.backup(destination)
result=close(copy);(b/'closeout-reproduction.json').write_text(json.dumps(result,indent=2))
if '--apply' in sys.argv:
 result=close(s);(Path(__file__).parent/'closeout.json').write_text(json.dumps(result,indent=2))
print(json.dumps({'macro':result['macro']['disposition'],'restart':result['macro']['coordinator_restart'],'source_task_preserved':True}))
