import json
import pathlib
import shutil
import subprocess

root = pathlib.Path('.userdata/caol-bulk-item-cost-exploration-001/save')
source = root / 'La Mirada'
target = root / 'BulkB11'
shutil.rmtree(target)
shutil.copytree(source, target)
# Use the task's existing decompressor to edit the native map serialization.
subprocess.run(['./zzip', str(target / 'maps/4.4.0.zzip')], check=True,
               stdout=subprocess.DEVNULL)
map_path = target / 'maps/4.4.0/147.157.0.map'
doc = json.loads(map_path.read_text())
# The source tile and adjacent wood/tools destinations in the map pack's 24x24 map-tile coordinates.
coords = {(11, 12): [], (11, 13): [], (12, 12): []}
for chunk in doc:
    old = chunk.get('items', [])
    new = []
    for x, y, items in zip(old[0::3], old[1::3], old[2::3]):
        if (x, y) not in coords:
            new.extend((x, y, items))
        else:
            coords[(x, y)] = items
    chunk['items'] = new
# Ensure these are genuinely empty and add unique native item objects at the source.
assert all(not items for items in coords.values()), {p:len(items) for p,items in coords.items()}
source_items = []
for i in range(128):
    source_items.append({'typeid':'2x4','uid':900000000+i,'owner':'your_followers',
                         'last_temp_check':0,'template_traits':[]})
for i in range(128):
    source_items.append({'typeid':'hammer','uid':900001000+i,'owner':'your_followers',
                         'last_temp_check':0,'template_traits':[]})
chunk = next(c for c in doc if c.get('coordinates') == [294,314,0])
chunk['items'].extend((11,12,source_items))
map_path.write_text(json.dumps(doc,separators=(',',':')))
subprocess.run(['./zzip', str(map_path)], check=True, stdout=subprocess.DEVNULL)
shutil.rmtree(target / 'maps/4.4.0')
# Install the already-audited static zones from the previous La Mirada clone.
zone_source = root / 'BulkB7'
for name in ['#SGFycmlzIFNwaXZleQ==.zones.json', '#SGFycmlzIFNwaXZleQ==.zoneszmgr-temp.json']:
    shutil.copy2(zone_source / name, target / name)
# Record hashes and fixture facts, without touching the source/reference save.
result = {'world':'BulkB11','source_world':'La Mirada',
          'source_character_save_sha256':None,
          'source_tile_ms':[3539,3780,0], 'source_map_coordinates':[11,12],
          'destination_tiles_ms':[[3539,3781,0],[3540,3780,0]],
          'prelaunch_source_items':{'2x4':128,'hammer':128},
          'prelaunch_destination_items':{'wood':0,'tools':0},
          'item_uids':[900000000,900001127]}
import hashlib
save_name='#SGFycmlzIFNwaXZleQ==.sav.zzip'
result['source_character_save_sha256']=hashlib.sha256((target/save_name).read_bytes()).hexdigest()
out=pathlib.Path('.de67/task-logs/R-CAOL-BULK-ITEM-COST-exploration-001/profile-route/B11-prepared.json')
out.write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
