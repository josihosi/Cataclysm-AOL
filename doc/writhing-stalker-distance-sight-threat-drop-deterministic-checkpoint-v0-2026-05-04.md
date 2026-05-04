# Writhing stalker distance / sight / threat-drop deterministic checkpoint v0 — 2026-05-04

Active lane: `CAOL-JOSEF-LIVE-DEBUG-BATCH-v0`, Slice 7.

## Claim

Deterministic/live-planner seams now cover the Slice 7 correction shape before a staged/live timing row is promoted:

- high-threat / plainly observed stalkers now write back a cautious `5` OMT stalk distance instead of the older close `3` OMT footing;
- post-burst spent-disengage memory also writes back the same far-watch distance so a finished strike does not immediately re-loiter close;
- a sighted stalker with a cover/edge route chooses a LoS-breaking shadow response and persists shadowing memory;
- a sighted stalker with no safe cover/edge route withdraws and writes spent-disengage cooldown instead of direct forced visible approach;
- low zombie-distraction pressure (`zombie_pressure >= 2`) can trigger the close dark-square pounce when there is believable evidence, preserving the no-evidence/no-omniscience guard;
- after the burst budget is spent, the live planner boots out to withdraw/spent-disengage and keeps the `5` OMT stalk distance.

## Changed source / tests

- `src/writhing_stalker_ai.cpp`
  - adds `cautious_stalk_distance_omt = 5` and uses it for overmatched, spent-disengage, and opportunity-withdraw writeback;
  - lowers the close dark-square zombie-distraction pounce threshold from `3` to `2` when evidence exists and no sight/bright counterpressure is active;
  - adds explicit target-focus handling: cover/edge routes shadow with a sight-break reason, while no safe route withdraws instead of visible direct-forced stalking.
- `tests/writhing_stalker_test.cpp`
  - updates the former `3` OMT overmatched expectations to `5` OMT;
  - adds sighted-stalk cover/withdraw coverage;
  - adds low-distraction pounce then boot-out coverage.

## Evidence

```text
make -j4 tests LINTJSON=0 ASTYLE=0
./tests/cata_test "[writhing_stalker]" --reporter compact
```

Result:

```text
Passed all 25 test cases with 283 assertions.
```

## Boundary / remaining proof

This is a deterministic/source-path checkpoint, not the full Slice 7 closure. Remaining Slice 7 evidence is a staged/live timing row that shows the player-facing rhythm: distant/hidden stalk, sight response, threat-drop pounce/short strike, then boot-out/recover without high-threat retreat or no-omniscience regressions.
