import json
import pathlib
import shutil
import subprocess

root = pathlib.Path('.userdata/caol-bulk-item-cost-exploration-001/save')
source = root / 'La Mirada'
target = root / 'BulkB12'
if target.exists():
    shutil.rmtree(target)
shutil.copytree(source, target)
# Use the task's existing decompressor to edit the native map serialization.
subprocess.run(['./zzip', str(target / 'maps/4.4.0.zzip')], check=True,
               stdout=subprocess.DEVNULL)
map_path = target / 'maps/4.4.0/147.157.0.map'
doc = json.loads(map_path.read_text())
# Map files hold multiple 12x12 submaps. The source and wood destination
# share submap [294,315]; tools are in [295,315]. Tile coordinates are
# local to each submap, not to the 24x24 map file.
target_cells = {
    (294, 315, 0): {(11, 0): "source", (11, 1): "wood"},
    (295, 315, 0): {(0, 0): "tools"},
}
found = {name: [] for name in ("source", "wood", "tools")}
for submap in doc:
    sm_coords = tuple(submap.get('coordinates', []))
    if sm_coords not in target_cells:
        continue
    old = submap.get('items', [])
    new = []
    for x, y, items in zip(old[0::3], old[1::3], old[2::3]):
        cell_name = target_cells[sm_coords].get((x, y))
        if cell_name is None:
            new.extend((x, y, items))
        else:
            assert not found[cell_name], (cell_name, sm_coords, x, y)
            found[cell_name] = items
    submap['items'] = new
assert all(not items for items in found.values()), {name:len(items) for name,items in found.items()}
source_items = []
for i in range(128):
    source_items.append({'typeid':'2x4','uid':900000000+i,'owner':'your_followers',
                         'last_temp_check':0,'template_traits':[]})
for i in range(128):
    source_items.append({'typeid':'hammer','uid':900001000+i,'owner':'your_followers',
                         'last_temp_check':0,'template_traits':[]})
chunk = next(c for c in doc if c.get('coordinates') == [294,315,0])
chunk['items'].extend((11,0,source_items))
map_path.write_text(json.dumps(doc,separators=(',',':')))
subprocess.run(['./zzip', str(map_path)], check=True, stdout=subprocess.DEVNULL)
shutil.rmtree(target / 'maps/4.4.0')
# Install the already-audited static zones from the previous La Mirada clone.
zone_source = root / 'BulkB7'
for name in ['#SGFycmlzIFNwaXZleQ==.zones.json', '#SGFycmlzIFNwaXZleQ==.zoneszmgr-temp.json']:
    shutil.copy2(zone_source / name, target / name)
# Record hashes and fixture facts, without touching the source/reference save.
result = {'world':'BulkB12','source_world':'La Mirada',
          'source_character_save_sha256':None,
          'source_tile_ms':[3539,3780,0], 'source_map_coordinates':{'submap':[294,315,0],'tile':[11,0]},
          'destination_tiles_ms':[[3539,3781,0],[3540,3780,0]],
          'prelaunch_source_items':{'2x4':128,'hammer':128},
          'prelaunch_destination_items':{'wood':0,'tools':0},
          'fixture_coordinate_audit':{'source_submap':[294,315,0],'source_local':[11,0],
                                       'wood_submap':[294,315,0],'wood_local':[11,1],
                                       'tools_submap':[295,315,0],'tools_local':[0,0]},
          'item_uids':[900000000,900001127]}
import hashlib
save_name='#SGFycmlzIFNwaXZleQ==.sav.zzip'
result['source_character_save_sha256']=hashlib.sha256((target/save_name).read_bytes()).hexdigest()
out=pathlib.Path('.de67/task-logs/R-CAOL-BULK-ITEM-COST-exploration-001/profile-route/B11-prepared.json')
out.write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
