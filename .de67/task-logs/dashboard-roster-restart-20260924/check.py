from pathlib import Path
from unittest.mock import patch
import tempfile,sqlite3,json,sys,importlib.util
p=Path(__file__).parent
files=[Path('/Users/josefhorvath/Library/Application Support/DE67 Dashboard/de67_dashboard.py'),Path('/Users/josefhorvath/.codex/skills/de67/integrations/dashboard/de67_dashboard.py'),Path('/Volumes/CodexBulk/Schanigarten/workspaces/de67-lab/integrations/dashboard/de67_dashboard.py')]
results=[]
for i,f in enumerate(files):
 sp=importlib.util.spec_from_file_location('dash_check'+str(i),f);m=importlib.util.module_from_spec(sp);sys.modules[sp.name]=m;sp.loader.exec_module(m)
 with tempfile.TemporaryDirectory() as tmp:
  w=Path(tmp);sessions=w/'sessions';sessions.mkdir();db=sqlite3.connect(w/'state_5.sqlite');db.executescript('CREATE TABLE threads(id TEXT,rollout_path TEXT);CREATE TABLE thread_spawn_edges(parent_thread_id TEXT,child_thread_id TEXT);')
  for sid,parent,model in [('new',None,'sol'),('old',None,'sol'),('worker',None,'luna'),('helper','worker','luna'),('released','old','luna')]:
   ftrace=sessions/(sid+'.jsonl');ftrace.write_text(''.join(json.dumps(r)+'\n' for r in [{'type':'session_meta','payload':{'id':sid,'parent_thread_id':parent,'cwd':str(w)}},{'type':'turn_context','payload':{'model':'gpt-6-'+model,'effort':'medium'}},{'type':'event_msg','payload':{'type':'task_started'}}]));db.execute('INSERT INTO threads VALUES (?,?)',(sid,str(ftrace)))
  db.executemany('INSERT INTO thread_spawn_edges VALUES (?,?)',[('worker','helper'),('old','released')]);db.commit();db.close()
  with patch.object(m,'_active_mutator',return_value=None),patch.object(m,'_active_coordinator_id',return_value='new'),patch.object(m,'_active_worker_claims',return_value={'worker':'old'}):
   result=m.worker_state(w,sessions);assert result['available'],result;assert result['counts']['luna']['medium']==2,result
 live=m.worker_state(Path.cwd(),Path('/Users/josefhorvath/.codex/sessions'));assert live['available'],live;results.append({'path':str(f),'retained_owner_and_helper_regression':'passed','live':live})
(p/'verification.json').write_text(json.dumps(results,indent=2));print(json.dumps(results))
