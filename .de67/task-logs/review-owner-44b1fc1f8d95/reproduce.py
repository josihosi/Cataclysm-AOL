from pathlib import Path
import json,sys,tempfile,shutil,subprocess
W=Path(__file__).resolve().parents[3]
R=W/'.de67/state/owner-approvals-20260908'
C=R/'candidate-method/scripts'
sys.path.insert(0,str(C))
from specification import compatibility_pointer,resolve
from mutation_guard import validate_random_review_mutation,GuardError
out={}
with tempfile.TemporaryDirectory() as td:
 t=Path(td)
 for label in ['before','after']:
  d=t/label;d.mkdir()
  for name in ['FS.md','DFS.md','orchestrator-guidelines.md','test-and-task-guidelines.md']:
   src=R/'workspace-candidate/.de67'/name
   if not src.exists():src=W/'.de67'/name
   shutil.copy2(src,d/name)
 # The exact legacy CLI guard paths must inspect canonical content and identity.
 after=t/'after';f=after/'FS.md';f.write_text(f.read_text()+'\nA changed functional contract.\n')
 try:out['stale_pointer_guard']={'result':list(validate_random_review_mutation(t/'before',after,selected_lane='DFS.md')),'rejected':False}
 except GuardError as e:out['stale_pointer_guard']={'rejected':True,'error':str(e)}
 try:resolve(after)
 except Exception as e:out['resolver_same_input']={'rejected':True,'error':str(e)}
 (after/'DFS.md').write_text(compatibility_pointer(f))
 try:out['valid_pointer_changed_contract_guard']={'result':list(validate_random_review_mutation(t/'before',after,selected_lane='DFS.md')),'rejected':False}
 except GuardError as e:out['valid_pointer_changed_contract_guard']={'rejected':True,'error':str(e)}
# Old supervisor keeps its imported DeadlineHarness: exercise that exact projection
# implementation against the already-isolated staged workspace DB, with no writes.
for label,method in [('running_supervisor_code',R/'method-baseline/scripts'),('fresh_candidate_code',C)]:
 code='import sys,json;from pathlib import Path;sys.path.insert(0,sys.argv[1]);from deadline_harness import DeadlineHarness\nwith DeadlineHarness(Path(sys.argv[2])) as h:\n print(json.dumps(h.synchronize_dfs_statuses(persist=False)))'
 p=subprocess.run([sys.executable,'-c',code,str(method),str(R/'workspace-candidate/.de67/state/deadlines.sqlite3')],capture_output=True,text=True)
 out[label]={'exit_code':p.returncode,'stdout':p.stdout,'stderr':p.stderr}
s=resolve(R/'workspace-candidate/.de67').text
out['functional_consult_slice']=s[s.index('<!-- DE67:DFS-SLICE:BEGIN id=R-MAINT-CONSULT-S001'):s.index('<!-- DE67:DFS-SLICE:END id=R-MAINT-CONSULT-S001')]
assert out['stale_pointer_guard']['rejected'] is False
assert out['resolver_same_input']['rejected'] is True
assert out['running_supervisor_code']['exit_code']!=0
assert out['fresh_candidate_code']['exit_code']==0
Path(__file__).with_name('reproduction.json').write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps(out,indent=2))
