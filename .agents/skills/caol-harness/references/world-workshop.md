# Build a playtest world

Run these commands from the repository root with Python 3. A world can be a closed save, a materialized fixture, or the directory produced by `prepare`. Use `--player-save FILENAME` when a save contains several player characters.

## Read the overmap

```sh
python tools/openclaw_harness/startup_harness.py world inspect tools/openclaw_harness/fixtures/saves/live-debug/r050_migo_tower_remote_initial_v1
python tools/openclaw_harness/startup_harness.py world inspect WORLD --zoom --map
python tools/openclaw_harness/startup_harness.py world inspect WORLD --center 145 134 --z 1 --map --json
```

The default view covers an 81 × 81 tile square, a 40-OMT radius around the saved player, with the surface displayed at z=0. The 20-OMT zoom shows the central 41 × 41 tiles. Center on another location or select a different level as needed. Distances in the summary are horizontal Chebyshev distances from the view center to the nearest footprint tile. AI eligibility uses the player and the site anchor or surviving party members. Brackets on the map show the saved local map bounds. Unknown tiles have no saved terrain in the inspected area. Inspection reads existing files without generating terrain or advancing time.

Use `--json` for site identities, member positions, the saved terrain counts and NPC records. Combine it with `--map` for the exact terrain ID at each coordinate, ordered north to south and west to east. A footprint, a population record and a physically identifiable member each answer a different setup question. Inspect the selected save again after native initialization when staffing matters.

## Prepare a world

For a quick setup, supply just the parent, a fresh destination, and the changes you want.

```sh
python tools/openclaw_harness/startup_harness.py world prepare live-debug:r050_migo_tower_remote_initial_v1 .userdata/test-brazier --give brazier 1 --give log 3 --give lighter 1
```

Use `--home cannibal_camp 20 0 -1` to place an additional camp 20 OMT east and one level below the saved player. For a reusable experiment, put the conditions in a JSON recipe.

Choose a parent close to the intended setup. Save the following as a task-local recipe, for example `camp.json` for the R050 Mi-go fixture. The player starts on a roof, so the east camp offset drops one level to reach the surface.

```json
{
  "name": "forest-camp",
  "locations": {
    "east_camp": {"from": "player", "offset_omt": [20, 0, -1]}
  },
  "specials": [{"id": "cannibal_camp", "at": "east_camp"}],
  "items": [
    {"id": "brazier", "count": 1, "offset_ms": [0, 0, 0]},
    {"id": "log", "count": 3, "offset_ms": [0, 0, 0]},
    {"id": "lighter", "count": 1, "charges": 50, "offset_ms": [0, 0, 0]}
  ],
  "requires": [{"staffed": "cannibal_camp", "at": "east_camp"}]
}
```

```sh
python tools/openclaw_harness/startup_harness.py world prepare tools/openclaw_harness/fixtures/saves/live-debug/r050_migo_tower_remote_initial_v1 camp.json .userdata/playtest-east-camp
python tools/openclaw_harness/startup_harness.py world inspect .userdata/playtest-east-camp --zoom --map
```

Use a new destination for each preparation. Absolute locations are `[x,y,z]` in OMT. Terrain edits use `{"id":"forest","at":"name"}` in the `terrain` array. Add `"to":"destination"` for a straight or diagonal corridor. The offline builder places fixed home footprints in saved overmap regions on eligible terrain. Choose an ungenerated local tile, or use the native overmap editor for a new region or a special that needs native placement effects. Item offsets are local map squares relative to the player. The result records the parent, applied recipe and saved readback. Read its requirements before assigning play. A newly placed home can still need native initialization and a saved roster. A visit during setup is appropriate when testing downstream behavior. For a remote first-contact test, record any player contact or knowledge that visit creates. Explicit unmet conditions stay visible while the scratch world is available for that setup.

After a native save, `world inspect CLOSED_WORLD --check PATH/TO/preparation.json` rechecks the requirements at the original locations, even if the avatar moved during play. An explicit check returns exit code 2 when a condition is unmet or unknown. A successful preparation still leaves its world available to resolve those conditions.

## Start a scratch session

Luna can open an ordinary native cockpit on a disposable copy. Use `--dry-run` to check the selected build and source. An independently published build can be selected with `--build-receipt PATH`. If the default build is stale, scratch checks for a current installed one and reports available pinned products for an explicitly versioned test.

```sh
python tools/openclaw_harness/startup_harness.py world scratch .userdata/playtest-east-camp --question "Can the player place and light the brazier using the ordinary controls?" --family fire
```

Use the returned session handle for native controls, save, exit and finish. Each scratch launch has its own writable profile, so two players can test the same starting conditions at the same time. A prepared world opens on the character named in its recipe readback. For a raw world with multiple characters, pass `--player-save FILENAME`. A build blocker carries the next action, and a scratch attempt has a searchable run ID.

## Keep an example

```sh
python tools/openclaw_harness/startup_harness.py world history brazier
python tools/openclaw_harness/startup_harness.py world keep RUN_ID --reason "Player placed the brazier and lit it through Examine" --family fire
python tools/openclaw_harness/startup_harness.py world history --kept --family fire
```

History and retained checkpoints live beside the scenario registry on the machine that ran the test. Use a reason that describes what the run actually showed. A failed approach or unusual encounter can make a good example. Add `--checkpoint CLOSED_WORLD` when the exact state is worth retaining. To replay it, find the `checkpoint` path with `world history --kept --family FAMILY --json` and pass that path to `world scratch`. `world unkeep RUN_ID` releases the retained copy and keeps the searchable run result.

Use the same family for retries and variations in distance, weather or seed. Each run has its own result and can be kept independently. Add a catalogue family when a genuinely new gameplay question needs one.
