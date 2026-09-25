"""Compare read-only overmap NPC payloads from archived source and task fixture."""
import hashlib, json, shutil, sys
from pathlib import Path
sys.path.insert(0, str(Path('tools/openclaw_harness').resolve()))
import startup_harness as sh
root=Path.cwd()
sources={
 'owner_source':root/'.userdata/reference-saves/josef-basecamp-closed-windows-20260924-122642/save/TestSetup00',
 'task_fixture':root/'tools/openclaw_harness/fixtures/saves/live-debug/first_smoke_smoke-first_testsetup00_v1/save/TestSetup00',
}
work=root/'.de67/task-logs/R-CAOL-FIRST-SMOKE-exploration-001/overmap-audit-tmp'
records=[]
for label,world in sources.items():
 temp=work/label; temp.mkdir(parents=True,exist_ok=True)
 for name in ('worldoptions.json','overmaps.dict','master.gsav','mmr.dict'):
  if (world/name).exists(): shutil.copy2(world/name,temp/name)
 (temp/'overmaps').mkdir(exist_ok=True)
 copied=temp/'overmaps/o.0.0.zzip'; shutil.copy2(world/'overmaps/o.0.0.zzip',copied)
 plain,version,payload=sh.extract_overmap_payload(copied)
 try:
  npcs=payload.get('npcs',[])
  canonical=json.dumps(npcs,ensure_ascii=False,sort_keys=True,separators=(',',':')).encode()
  rows=[]
  for npc in npcs:
   if not isinstance(npc,dict): continue
   activity=npc.get('activity',{}); actor=activity.get('actor',{}) if isinstance(activity,dict) else {}; data=actor.get('actor_data',{}) if isinstance(actor,dict) else {}; refs=[]
   for field in ('picked_up_stuff','other_activity_items'):
    items=data.get(field,[])
    for item in items if isinstance(items,list) else []:
     if isinstance(item,dict): refs.append({'list':field,'item_uid':item.get('uid'),'parent':item.get('parent')})
   if refs: rows.append({'npc_id':npc.get('id'),'name':npc.get('name'),'activity':activity.get('type'),'actor_type':actor.get('actor_type'),'item_location_refs':refs})
  records.append({'label':label,'source_path':str(sources[label].relative_to(root)),'overmap_sha256':hashlib.sha256((world/'overmaps/o.0.0.zzip').read_bytes()).hexdigest(),'npc_count':len(npcs),'canonical_npc_subtree_sha256':hashlib.sha256(canonical).hexdigest(),'npc_activity_owner_refs':rows})
 finally: sh.cleanup_extracted_overmap(plain,keep=False)
result={'schema':'R-CAOL-FIRST-SMOKE-npc-reference-compare-v1','method':'Copied only each overmaps/o.0.0.zzip plus worldoptions.json/overmaps.dict/master.gsav/mmr.dict into task-log scratch; decoded via startup_harness.extract_overmap_payload; compared canonical sorted-key JSON of unchanged npcs subtrees; scratch removed after artifact write.','source_npc_subtree_equals_fixture':records[0]['canonical_npc_subtree_sha256']==records[1]['canonical_npc_subtree_sha256'],'records':records,'interpretation':'The source and fixture NPC payloads are identical. The observed startup exception occurs while overmap::unserialize loads NPC id 2 Tilda Wray ACT_MOVE_LOOT and its item_location references whose parent owner is character id 2. This localizes the observed failure to loading the saved NPC activity/reference in this source-derived world; it does not establish whether the underlying defect is stale source state or current loader ordering.'}
out=root/'.de67/task-logs/R-CAOL-FIRST-SMOKE-exploration-001/smoke-first-npc-reference-audit.json'
out.write_text(json.dumps(result,indent=2)+'\n')
shutil.rmtree(work,ignore_errors=True)
print(out)
print(hashlib.sha256(out.read_bytes()).hexdigest())
