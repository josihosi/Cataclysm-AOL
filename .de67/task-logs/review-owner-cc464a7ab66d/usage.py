from pathlib import Path
import json,sqlite3,datetime
out=Path(__file__).parent; now=datetime.datetime.now(datetime.timezone.utc); start=now-datetime.timedelta(hours=10)
db=sqlite3.connect('.de67/state/deadlines.sqlite3')
bindings=db.execute('select coordinator_session_id,worker_id from worker_claims').fetchall(); roots={x for r in bindings for x in r if x}
audit='01a0c060-a6f4-7ae2-ae1b-db23fb312be4';rows={};seen=set();rates=[];events=[]
for day in range(20,25):
 for f in Path('/Users/josefhorvath/.codex/sessions/2026/09/'+str(day)).glob('*.jsonl'):
  with f.open() as h:
   m=json.loads(next(h))['payload'];sid=m['id'];src=m.get('source');sp=src.get('subagent',{}).get('thread_spawn',{}) if isinstance(src,dict) else {}
   row=dict(id=sid,parent=sp.get('parent_thread_id'),agent=sp.get('agent_path'),cwd=m.get('cwd'),source=str(f),fresh=0,cached=0,output=0,responses=0);rows[sid]=row;turn=None
   for line in h:
    try:d=json.loads(line)
    except ValueError:continue
    p=d.get('payload',{});ts=datetime.datetime.fromisoformat(d['timestamp'].replace('Z','+00:00'))
    if d.get('type')=='turn_context':turn=p.get('turn_id');row['latest_turn']=turn
    if not start<=ts<=now:continue
    if d.get('type')=='token_usage_record':
     key=(p.get('thread_id'),p.get('response_id'))
     if key in seen:continue
     seen.add(key);u=p['usage'];v=dict(fresh=u['input_tokens']-u['cached_input_tokens'],cached=u['cached_input_tokens'],output=u['output_tokens'],responses=1)
     for k,x in v.items():row[k]+=x
     events.append((sid,p.get('turn_id'),v))
    if d.get('type')=='event_msg' and p.get('type')=='token_count':
     for r in (p.get('rate_limits') or {}).values():
      if isinstance(r,dict) and r.get('window_minutes')==10080:
       elapsed=(ts.timestamp()-r['resets_at']+604800)/604800
       rates.append(dict(timestamp=d['timestamp'],used_percent=r['used_percent'],resets_at=r['resets_at'],pace_estimate=r['used_percent']/100/elapsed if elapsed>0 else None))
def descend(ids):
 ids=set(ids)
 while True:
  n=ids|{s for s,r in rows.items() if r['parent'] in ids}
  if n==ids:return ids
  ids=n
keys=['fresh','cached','output','responses']
def total(ids):return {k:sum(rows[s][k] for s in ids if s in rows) for k in keys}
tree=descend(roots);helper={s for s,r in rows.items() if r['agent']=='/root/dispatch_coverage_gap'};review={k:0 for k in keys}
for s,t,v in events:
 if s in helper or (s==audit and t==rows[audit].get('latest_turn')):
  for k in keys:review[k]+=v[k]
rates.sort(key=lambda r:r['timestamp']);other={s for s,r in rows.items() if r['cwd']==str(Path.cwd())}-tree-descend({audit})
result=dict(start=start.isoformat(),snapshot=now.isoformat(),campaign=total(tree),review_including_helper=review,other_workspace=total(other),campaign_threads=sorted([r for s,r in rows.items() if s in tree and r['responses']],key=lambda r:r['fresh'],reverse=True),helper_threads=[rows[s] for s in helper],weekly=dict(first=rates[0] if rates else None,latest=rates[-1] if rates else None,samples=len(rates),pace_range=[min(r['pace_estimate'] for r in rates),max(r['pace_estimate'] for r in rates)] if rates else []),limitations=['Local Sep20-24-created logs only; older active or remote logs missing.','Durable worker/coordinator bindings plus native descendants; unrelated same-workspace usage separated.','Account-wide pace estimated from elapsed week; samples do not establish continuous usage or campaign allowance share.','Cached tokens separate from fresh; no monetary equivalence assumed.','Review snapshot excludes later closeout responses.'])
(out/'usage.json').write_text(json.dumps(result,indent=2));print(json.dumps({k:v for k,v in result.items() if k not in ['campaign_threads','helper_threads']},indent=2));print('Top campaign:',[(r['id'],r['fresh'],r['cached'],r['output'],r['responses']) for r in result['campaign_threads'][:8]])
