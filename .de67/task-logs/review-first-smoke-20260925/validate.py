from pathlib import Path
import json,sys,tempfile,shutil
p=Path(__file__).resolve().parent
sys.path.insert(0,'/Users/josefhorvath/.codex/skills/de67/de-67-3/scripts')
from mutation_guard import validate_work_ledger
from context_library import prepare,selected_context
from policy_kernel import current_owner_contract
r={}
for k,f in [('baseline',p/'baseline/work-ledger.md'),('candidate',p/'work-ledger.candidate.md')]:
 try:
  claims=validate_work_ledger(f,Path('.de67/FS.md'),state=Path('.de67/state/deadlines.sqlite3'),lineage_id='semantic-surface-cockpit')
  r[k]={'pass':True,'claims':len(claims)}
 except Exception as e:r[k]={'error':str(e)}
if 'error' in r['candidate']:assert r['candidate']==r['baseline']
old=(p/'baseline/work-ledger.md').read_text().splitlines();new=(p/'work-ledger.candidate.md').read_text().splitlines();from collections import Counter;assert not (Counter(old)-Counter(new))
with tempfile.TemporaryDirectory() as d:
 w=Path(d);(w/'.de67').mkdir();shutil.copy2(p/'work-ledger.candidate.md',w/'.de67/work-ledger.md');shutil.copy2('.de67/WEC.md',w/'.de67/WEC.md')
 rendered=[]
 for task in ['R-CAOL-FIRST-SMOKE-exploration-002','R-CAOL-FIRST-SMOKE-exploration-003','R-CAOL-CAMP-WAIT-COST-exploration-003']:
  assignment=next(x for x in new if x.startswith('  - Assignment '+task+':'))
  prepare(w,task,assignment,handoff=current_owner_contract(w));text=selected_context(w,task);assert assignment.strip() in text;rendered.append(text)
 assert 'world.level_up' in rendered[0] and 'No debug teleport' in rendered[0]
 assert 'authenticated exact native turn' in rendered[1]
 assert 'GPT-6 Astra implements' in rendered[2]
 (p/'handoff-reproduction.txt').write_text('\n\n'.join(rendered))
r.update(old_ledger_lines_preserved=True,real_context_render_pass=True,live_roof_proof=False)
(p/'validation.json').write_text(json.dumps(r,indent=2));print(json.dumps(r))
