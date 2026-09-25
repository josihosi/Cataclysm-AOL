import datetime, hashlib, json
from pathlib import Path
OUT=Path(__file__).parent
end=datetime.datetime.now(datetime.timezone.utc); start=end-datetime.timedelta(hours=10)
roots={'01a0c0ff-2e5a-7d82-b380-188fe61ff4e6','01a0bf28-8257-7191-8506-f66994d2fda2','01a0c069-b1b6-7740-b0e5-934599578ee3','01a0c0da-bf41-7a80-9d5e-3016c5da8176'}
audit_root='01a0c060-a6f4-7ae2-ae1b-db23fb312be4'; audit_turn='01a0c1dd-806e-7350-98a0-7c24e93e18ce'
rows={};usage=[];rates=[];seen=set();errors=[]
for date in ['20','21']:
 for f in Path('/Users/josefhorvath/.codex/sessions/2026/09/'+date).glob('*.jsonl'):
  with f.open() as h:
   meta=json.loads(next(h))['payload'];sid=meta['id'];source=meta.get('source');spawn=source.get('subagent',{}).get('thread_spawn',{}) if isinstance(source,dict) else {}
   rows[sid]={'id':sid,'parent':spawn.get('parent_thread_id'),'agent_path':spawn.get('agent_path'),'source':str(f),'fresh_input':0,'cached_input':0,'output':0,'responses':0}
   for n,line in enumerate(h,2):
    try:d=json.loads(line)
    except ValueError:errors.append({'path':str(f),'line':n});continue
    p=d.get('payload',{});ts=datetime.datetime.fromisoformat(d['timestamp'].replace('Z','+00:00'))
    if not start<=ts<=end:continue
    if d.get('type')=='token_usage_record':
     key=(p['thread_id'],p['response_id'])
     if key in seen:continue
     seen.add(key);u=p['usage'];v={'fresh_input':u['input_tokens']-u['cached_input_tokens'],'cached_input':u['cached_input_tokens'],'output':u['output_tokens'],'responses':1}
     for k,x in v.items():rows[sid][k]+=x
     usage.append((sid,p.get('turn_id'),v))
    if d.get('type')=='event_msg' and p.get('type')=='token_count':
     rate=(p.get('rate_limits') or {}).get('primary')
     if rate and rate.get('window_minutes')==10080:
      elapsed=(ts.timestamp()-(rate['resets_at']-604800))/604800
      rates.append({'timestamp':d['timestamp'],**rate,'elapsed_week_pace_estimate':rate['used_percent']/100/elapsed if elapsed>0 else None})
def descendants(ids):
 result=set(ids)
 while True:
  expanded=result|{s for s,r in rows.items() if r['parent'] in result}
  if result==expanded:return result
  result=expanded
keys=['fresh_input','cached_input','output','responses']
def total(ids):return {k:sum(rows[s][k] for s in ids if s in rows) for k in keys}
tree=descendants(roots);helpers={s for s,r in rows.items() if r['parent']==audit_root and r.get('agent_path')=='/root/evidence_deadline_cause'}
audit={k:0 for k in keys}
for sid,turn,v in usage:
 if sid in helpers or (sid==audit_root and turn==audit_turn):
  for k in keys:audit[k]+=v[k]
rates.sort(key=lambda x:x['timestamp']);last=rates[-1];same_reset=[x for x in rates if x['resets_at']==last['resets_at']]
result={'window_start':start.isoformat(),'snapshot':end.isoformat(),'campaign_full_tree':total(tree),'campaign_threads':[rows[s] for s in sorted(tree) if s in rows], 'review_including_luna_helper':audit,'review_helper_threads':[rows[s] for s in sorted(helpers)],'weekly_account':{'first_sample':rates[0],'latest_sample':last,'sample_count':len(rates),'same_reset_pace_range':[min(x['elapsed_week_pace_estimate'] for x in same_reset),max(x['elapsed_week_pace_estimate'] for x in same_reset)]},'parse_errors':errors,'limitations':['Only available local Sept20/21-created session files; earlier-created active/remote sessions may be missing.','Attribution follows coordinator and child parent IDs; unrelated owner chats and external status agents are separate, not assigned campaign cost.','Cached input is not fresh work or an equivalent bill; token counts do not assign account-wide allowance.','Review snapshot excludes subsequent completion/report responses.','Pace is an estimate from used fraction divided by elapsed week, not a supplied billing multiplier; samples do not prove continuous activity.']}
(OUT/'usage.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({k:v for k,v in result.items() if k not in ['campaign_threads','review_helper_threads']},indent=2))
