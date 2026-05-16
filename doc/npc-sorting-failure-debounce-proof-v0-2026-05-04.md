# NPC sorting failure debounce proof v0 — 2026-05-04

Lane: `CAOL-JOSEF-LIVE-DEBUG-BATCH-v0`, Slice 8.

## Claim

NPC camp/basecamp sorting failures now cool down instead of immediately recreating the same impossible `ACT_MOVE_LOOT` job every scan:

- a no-progress NPC zone-sort activity records an `ACT_MOVE_LOOT` job cooldown;
- job scans skip blocked activities until the cooldown expires;
- successful sorting clears the block path by recording actual moved items;
- cooldown state survives job-data serialization;
- ordinary successful sorting coverage still passes.

## Source / behavior checkpoint

Changed source classes:

- `src/activity_actor.cpp` / `src/activity_actor_definitions.h`: `zone_sort_activity_actor` tracks whether it moved at least one item. When an NPC sort activity finishes with no progress, it blocks `ACT_MOVE_LOOT` for `30_minutes`; when it moved items, it clears that job block.
- `src/npc.cpp` / `src/npc.h`: `job_data` now owns activity cooldown helpers separate from task priority.
- `src/npcmove.cpp`: NPC job scans expire old cooldowns and skip still-blocked jobs instead of recreating the same activity every scan.
- `src/savegame_json.cpp`: job activity cooldowns serialize/deserialize alongside task priorities and fetch history.
- `tests/clzones_test.cpp`: direct NPC activity-loop and job-scan recovery coverage for blocked sort jobs.
- `tests/faction_camp_test.cpp`: serialization coverage for cooldown persistence and expiry semantics.

## Evidence

Local gate run after current edits:

```text
git diff --check
make -j4 obj/activity_actor.o obj/npc.o obj/npcmove.o obj/savegame_json.o tests/clzones_test.o tests/faction_camp_test.o LINTJSON=0 ASTYLE=0
make -j4 tests/clzones_test.o tests/faction_camp_test.o tests LINTJSON=0 ASTYLE=0
./tests/cata_test "[zones][items][activities][sorting][npc],[camp][jobs]" --reporter compact
./tests/cata_test "[zones][items][activities][sorting]" --reporter compact
```

Results:

```text
Passed all 3 test cases with 15 assertions.
Passed all 28 test cases with 200 assertions.
```

Representative assertions now prove:

```text
npc_zone_sorting_no_progress_blocks_move_loot_job
npc_blocked_move_loot_job_is_not_recreated_until_cooldown_expires
job_data_serializes_activity_cooldowns
```

## Boundary

This closes Slice 8 as a focused deterministic/source-path checkpoint for the actual NPC activity/job loop. It does not redesign zone sorting, remove NPC sorting, or claim every possible full/unreachable/source-specific fixture shape has its own distinct cooldown key; the implemented block is job-level and expires normally so fixed setups can retry.
