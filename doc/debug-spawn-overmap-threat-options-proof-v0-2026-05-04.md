# Debug spawn overmap threat options proof v0 — 2026-05-04

Lane: `CAOL-JOSEF-LIVE-DEBUG-BATCH-v0`, Slice 9.

## Claim

The debug menu spawn-horde entry is now a small overmap-threat submenu instead of the old single dummy one-zombie horde action:

- medium zombie horde at `5` OMT north;
- medium zombie horde at `10` OMT north;
- writhing stalker at `5` OMT north;
- writhing stalker at `10` OMT north;
- zombie rider at `5` OMT north;
- zombie rider at `10` OMT north.

The medium horde option creates 30 zombie horde entries, not one monster.

## Source / behavior checkpoint

Changed source/classes:

- `src/debug_menu.cpp` / `src/debug_menu.h`: add `overmap_spawn_option`, expose option/destination helpers for tests, replace the old `SPAWN_HORDE` one-shot with `Spawn which overmap threat?`, and log selected id/group/population/distance/destination for harness proof.
- `data/json/monstergroups/misc.json`: keep `GROUP_DEBUG_EXACTLY_ONE` as legacy data and add explicit debug groups for medium horde, writhing stalker, and zombie rider.
- `tests/debug_menu_test.cpp`: deterministic coverage for submenu options, `5`/`10` OMT destination math through `coords::map_squares_per( scale::overmap_terrain )`, and monstergroup resolution/counts.
- `tools/openclaw_harness/startup_harness.py` / `tools/openclaw_harness/scenarios/harness.debug_spawn_overmap_threat_options_mcw.json`: live debug-menu shortcut path and saved horde-map audit support for count and distance bands.

## Evidence

Build/test gates after current edits:

```text
make -j4 obj/debug_menu.o tests/debug_menu_test.o tests LINTJSON=0 ASTYLE=0
./tests/cata_test "[debug_menu]" --reporter compact
python3 -m py_compile tools/openclaw_harness/startup_harness.py
python3 -m json.tool tools/openclaw_harness/scenarios/harness.debug_spawn_overmap_threat_options_mcw.json
make -j4 TILES=1 cataclysm-tiles LINTJSON=0 ASTYLE=0
git diff --check
```

Results:

```text
[debug_menu]: Passed all 4 test cases with 247 assertions.
cataclysm-tiles rebuild: exit 0 (linker emitted only existing macOS dylib-version warnings).
```

Live feature-path proof:

```text
python3 tools/openclaw_harness/startup_harness.py probe --compact-stdout harness.debug_spawn_overmap_threat_options_mcw
```

Credited run: `.userdata/dev-harness/harness_runs/20260504_124808/`.

Run summary:

```text
verdict=artifacts_matched
evidence_class=feature-path
feature_proof=true
step_ledger_status=green_step_local_proof
steps=18/18 green
runtime title=Cataclysm: Dark Days Ahead - 8ebe8fbcd5-dirty
```

Decisive live rows:

```text
debug overmap_spawn: id=medium_horde_5_omt group=GROUP_DEBUG_MEDIUM_HORDE population=30 distance_omt=5 destination_sm=(280,72,0)
debug overmap_spawn: id=medium_horde_10_omt group=GROUP_DEBUG_MEDIUM_HORDE population=30 distance_omt=10 destination_sm=(280,62,0)
debug overmap_spawn: id=writhing_stalker_5_omt group=GROUP_DEBUG_WRITHING_STALKER population=1 distance_omt=5 destination_sm=(280,72,0)
debug overmap_spawn: id=writhing_stalker_10_omt group=GROUP_DEBUG_WRITHING_STALKER population=1 distance_omt=10 destination_sm=(280,62,0)
debug overmap_spawn: id=zombie_rider_5_omt group=GROUP_DEBUG_ZOMBIE_RIDER population=1 distance_omt=5 destination_sm=(280,72,0)
debug overmap_spawn: id=zombie_rider_10_omt group=GROUP_DEBUG_ZOMBIE_RIDER population=1 distance_omt=10 destination_sm=(280,62,0)
```

Saved horde-map audit in the same run:

```text
audit_saved_hordes_after_debug_overmap_spawns.metadata.json: status=required_state_present
observed_horde_counts={'mon_robin': 1, 'mon_writhing_stalker': 2, 'mon_zombie': 60, 'mon_zombie_rider': 2}
mon_zombie distances: 30 entries in the 5-OMT band at distance 128 ms, 30 entries in the 10-OMT band at distance 250 ms
mon_writhing_stalker distances: 128 ms and 248 ms
mon_zombie_rider distances: 128 ms and 248 ms
audit_player_save_mtime_after_debug_overmap_spawn_save.metadata.json: status=required_state_present
```

## Boundary

This closes Slice 9 for debug-menu spawning and saved overmap horde-map state. It does not claim natural discovery, pursuit behavior after the spawn, positive tracking intensity, or combat/playfeel for the spawned threats; those belong to the individual stalker/rider/horde behavior slices.
