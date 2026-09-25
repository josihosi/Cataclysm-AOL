"""Zero-credit cannibal_camp footprint transplant into task disposable copies only."""
import hashlib, json, shutil, sys
from pathlib import Path
sys.path.insert(0, str(Path('tools/openclaw_harness').resolve()))
import startup_harness as sh
root=Path.cwd()
log=root/'.de67/task-logs/R-CAOL-FIRST-SMOKE-exploration-001/camp-transplant.json'
src=root/'tools/openclaw_harness/fixtures/saves/live-debug/tmp_bandit_live_world_local_contact_raw_2026-04-23/save/McWilliams'
placements=sh.load_bandit_special_placements(root, src, {'bandit_camp'})
p=next(x for x in placements if sh.anchor_from_special_points([q for q in x.points if q[2]<=0]) == (140,51,0))
source_anchor=sh.anchor_from_special_points([q for q in p.points if q[2]<=0])
source_path=src/'overmaps'/p.overmap_file
source_plain,_,source_payload=sh.extract_overmap_payload(source_path)
try:
    src_layers=source_payload['layers']; src_cache={}; terrain={}
    for point in p.points:
        li=sh.overmap_layer_index(point[2])
        layer=src_cache.get(li)
        if layer is None: layer=sh.decode_overmap_layer(src_layers[li],context=f'{source_path} z={point[2]}'); src_cache[li]=layer
        terrain[point]=layer[sh.overmap_flat_index(point)]
finally: sh.cleanup_extracted_overmap(source_plain,keep=not bool(source_payload.get('_created_plain',False)))
records=[]
for arm in ('smoke-first','first-light'):
    world=root/'.userdata/first-smoke-exploration-001'/arm/'TestSetup00'
    before={str(f.relative_to(world)):hashlib.sha256(f.read_bytes()).hexdigest() for f in world.rglob('*') if f.is_file()}
    abs_target=(135,137,0)
    ox,oy,target_anchor=sh.overmap_file_coords_from_abs_omt(abs_target)
    target_path=world/'overmaps'/f'o.{ox}.{oy}.zzip'
    plain,version,payload=sh.extract_overmap_payload(target_path)
    try:
        layers=payload['layers']; cache={}; translated=[]; previous={}
        for point in p.points:
            new=(target_anchor[0]+point[0]-source_anchor[0],target_anchor[1]+point[1]-source_anchor[1],target_anchor[2]+point[2]-source_anchor[2])
            if new[0]//sh.OMAPX != 0 or new[1]//sh.OMAPY != 0: raise RuntimeError(f'footprint crosses overmap: {new}')
            li=sh.overmap_layer_index(new[2]); layer=cache.get(li)
            if layer is None: layer=sh.decode_overmap_layer(layers[li],context=f'{target_path} z={new[2]}'); cache[li]=layer
            idx=sh.overmap_flat_index(new); previous[new]=layer[idx]; layer[idx]=terrain[point]; translated.append(new)
        for li,layer in cache.items(): layers[li]=sh.encode_overmap_layer(layer)
        sh.upsert_special_placement(payload,special_id='cannibal_camp',placement_origin=target_anchor,placement_points=translated)
        sh.write_overmap_payload(plain,version,payload)
    finally: sh.cleanup_extracted_overmap(plain,keep=False)
    after={str(f.relative_to(world)):hashlib.sha256(f.read_bytes()).hexdigest() for f in world.rglob('*') if f.is_file()}
    changes={k:{'before':before.get(k),'after':after.get(k)} for k in sorted(set(before)|set(after)) if before.get(k)!=after.get(k)}
    if set(changes)!={f'overmaps/{target_path.name}'}: raise RuntimeError(f'unexpected changed paths: {changes.keys()}')
    fixture=root/'tools/openclaw_harness/fixtures/saves/live-debug'/f'first_smoke_{arm}_testsetup00_v1'
    fixture.mkdir(parents=True,exist_ok=True)
    payload_dir=fixture/'save'
    if payload_dir.exists(): shutil.rmtree(payload_dir)
    shutil.copytree(world,payload_dir)
    records.append({'arm':arm,'source_copy':str(world.relative_to(root)),'fixture':str(fixture.relative_to(root)),'source_target_omt':list(abs_target),'offset_from_player':list([4,-6,0]),'donor_fixture':'bandit_basecamp_playtest_kit_v0_2026-04-22 -> tmp_bandit_live_world_local_contact_raw_2026-04-23','donor_special':'bandit_camp','donor_anchor':[140,51,0],'donor_points':[list(x) for x in p.points],'target_anchor_local':list(target_anchor),'target_points':[list(x) for x in translated],'old_terrain':{str(k):v for k,v in previous.items()},'new_terrain':{str(k):terrain[x] for k,x in zip(translated,p.points)},'changed_files':changes,'tree_sha256':sh.compute_save_tree_digest(payload_dir) if hasattr(sh,'compute_save_tree_digest') else None})
log.write_text(json.dumps({'schema':'task-first-smoke-camp-transplant-v1','owner_source_untouched':True,'records':records},indent=2)+'\n')
print(log)
print(json.dumps(records,indent=2))
