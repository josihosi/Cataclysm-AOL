from pathlib import Path
import hashlib, json, sqlite3, sys
sys.path.insert(0, '/Users/josefhorvath/.codex/skills/de67/de-67-3/scripts')
from deadline_harness import DeadlineHarness
from mutation_guard import validate_work_ledger
root = Path(__file__).resolve().parents[3]
out = Path(__file__).resolve().parent
state = root / '.de67/state/deadlines.sqlite3'
lineage = 'semantic-surface-cockpit'
task = 'R-HARNESS-JOURNEY-exploration-001'
claim = 'R-HARNESS-JOURNEY'
report = '.de67/task-logs/review-journey-001-20260923/report.md'
verdict = 'Historical journey clock expired during owner stop; refreeze retired the campaign and released the stale attempt without technical acceptance.'
diagnosis = report + '; Incident recorded during September 23 administrative release. Owner now authorizes new delivery but closed the harness campaign. Preserve all 69 acceptances and abandoned/released journey state. Empty pending owner queue. Recent usage separates stopped campaign from useful Phase-2 readiness work; latest account pace alone is not campaign waste.'
micro = report + '; Replaced stale current-worker/clock prose with historical disposition. Baseline ledger guard reproduced cross-claim leakage into the prepended launcher claim. Moving seven new claim entries unchanged to the end makes the production guard pass while preserving the exact active claim set, all assignments/subtasks, FS/WEC/policy and accepted proof. No retired journey revival.'
macro = report + '; Evidence-backed no-change: owner closure and current scopes settle the old campaign; existing method ownership/deadline contracts remain valid. No new coverage sweep, economy project or installed-method mutation. Only necessary current-context repair; successor use/savings unmeasured. A fresh coordinator must follow current red claims, not restart the retired journey despite generic clock wording.'

def acceptance(conn):
    rows = [tuple(r) for r in conn.execute('SELECT * FROM claim_acceptances ORDER BY lineage_id,claim_id,acceptance_number')]
    return {'count': len(rows), 'sha256': hashlib.sha256(json.dumps(rows, sort_keys=True).encode()).hexdigest()}

def finish(target):
    queue = (root / '.de67/mutation-suggestions.md').read_text()
    assert not queue.split('## Pending suggestions', 1)[1].strip(), 'New pending owner entry requires disposition'
    validate_work_ledger(root / '.de67/work-ledger.md', root / '.de67/FS.md', state=target, lineage_id=lineage)
    with DeadlineHarness(target) as h:
        conn = h.connection
        incident = conn.execute('SELECT source_task_id,reviewed_at,generation FROM claim_deadline_generation_incidents WHERE lineage_id=? AND claim_id=? ORDER BY generation DESC LIMIT 1', (lineage,claim)).fetchone()
        assert dict(incident) == {'source_task_id':task,'reviewed_at':None,'generation':1}, dict(incident)
        before_task = tuple(conn.execute('SELECT * FROM tasks WHERE lineage_id=? AND task_id=?',(lineage,task)).fetchone())
        before = acceptance(conn)
        saved = json.loads((out / 'accepted-before.json').read_text())
        current = [dict(r) for r in conn.execute('SELECT * FROM claim_acceptances ORDER BY lineage_id,claim_id,acceptance_number')]
        assert current == saved, 'Accepted proof changed since review baseline' 
        restart_before = conn.execute('SELECT count(*) FROM coordinator_restart_requests WHERE lineage_id=?',(lineage,)).fetchone()[0]
        diag = h.diagnose_incident(lineage,task,'deadline_miss',verdict,diagnosis)
        small = h.resolve_deadline_mutation(lineage,claim,'micro',micro)
        broad = h.resolve_deadline_mutation(lineage,claim,'macro',macro,no_change_required=True)
        after = acceptance(conn)
        assert before == after, 'Acceptance changed'
        assert before_task == tuple(conn.execute('SELECT * FROM tasks WHERE lineage_id=? AND task_id=?',(lineage,task)).fetchone()), 'Retired task changed'
        restart_after = conn.execute('SELECT count(*) FROM coordinator_restart_requests WHERE lineage_id=?',(lineage,)).fetchone()[0]
        assert restart_after == restart_before + 1, 'Expected exactly one fresh restart request'
        components = [dict(r) for r in conn.execute('SELECT component,no_change_required,receipt_id FROM deadline_generation_mutation_components WHERE lineage_id=? AND claim_id=? AND generation=1 ORDER BY component',(lineage,claim))]
        assert len(components)==2
        return {'diagnosis':diag,'micro':small,'macro':broad,'acceptance_before':before,'acceptance_after':after,'components':components,'restart_requests_added':restart_after-restart_before}

copy = out / 'closeout-reproduction.sqlite3'
assert not copy.exists(), 'Reproduction already exists; inspect instead of repeating'
with sqlite3.connect(state) as source, sqlite3.connect(copy) as destination:
    source.backup(destination)
reproduction = finish(copy)
(out / 'closeout-reproduction.json').write_text(json.dumps(reproduction, indent=2))
if '--apply' in sys.argv:
    result = finish(state)
    (out / 'closeout.json').write_text(json.dumps(result,indent=2))
    print(json.dumps({'acceptances':result['acceptance_after'],'restart_requests_added':result['restart_requests_added'],'restart':result['macro']['coordinator_restart']}))
else:
    print(json.dumps({'reproduction': 'passed', 'restart': reproduction['macro']['coordinator_restart']}))
