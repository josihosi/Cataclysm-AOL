# Andi handoff: CAOL-ROOF-HORDE-NICE-FIRE-v0

## Current canon state

`CAOL-ROOF-HORDE-NICE-FIRE-v0` is the **ACTIVE / GREENLIT PLAYTEST PACKET**.

Authoritative canon is `Plan.md`, with `TODO.md`, `SUCCESS.md`, `TESTING.md`, and `doc/work-ledger.md` aligned downstream. This handoff is only a terse executor packet; if it ever disagrees with those files, repair this file from canon instead of treating it as truth.

`CAOL-WRITHING-STALKER-v0` is closed/checkpointed green v0. Do not reopen it. The old mixed-hostile horde caveat is now promoted into this focused roof-fire horde playtest instead of being rerun as mixed-hostile soup.

## Goal

Get the horde playtested with a nice roof fire.

Concrete meaning: run a focused feature-path proof where an honest elevated/roof fire exists, a plausible horde exists before the wait, bounded time passes, and artifacts show direct horde response plus cost/stability metrics.

## Canonical packet

- Contract: `doc/roof-fire-horde-nice-roof-fire-playtest-packet-v0-2026-04-30.md`
- Imagination source: `doc/roof-fire-horde-nice-roof-fire-imagination-source-of-truth-2026-04-30.md`
- Prior roof-fire proof: `doc/roof-fire-horde-detection-proof-v0-2026-04-29.md`
- Prior source run: `.userdata/dev-harness/harness_runs/20260429_172847/`
- Prior split proof: `.userdata/dev-harness/harness_runs/20260429_180239/`

## Next executor slice

Create/run a named focused scenario, proposed name:

- `bandit.roof_fire_horde_nice_roof_fire_mcw`

Required proof shape:

1. Prove the roof/elevated fire before waiting. Prefer reusing the previous player-created roof-fire chain if still fit; otherwise rebuild an equivalent honest player-action roof-fire path.
2. Prove horde presence before waiting at a plausible distance.
3. Run bounded in-game time passage.
4. Capture same-run roof-fire signal artifact lines.
5. Capture saved/log horde response after the wait:
   - position/offset;
   - destination/retarget;
   - `last_processed` or equivalent;
   - movement budget / moves;
   - tracking/interest metadata if exposed by the game.
6. Report metrics: waited time / sampled turns, wall-clock, available per-turn/timing summary, horde-specific timing if instrumented, and crash/stderr/debug-log status.

## Non-goals/cautions

- Do not rerun `performance.mixed_hostile_stalker_horde_mcw` as a substitute.
- Do not reopen writhing stalker v0.
- Do not claim horde setup/presence as horde behavior proof.
- Do not overclaim positive `tracking_intensity`; if it remains zero or unavailable, credit only the fields actually proven.
- Do not broaden into natural multi-day horde discovery, combat behavior, or horde AI rewrite unless this proof exposes a concrete missing bridge.
- Obey repeated-attempt discipline: after two unresolved same-item failures, consult Frau before attempt 3; after four total unresolved attempts, preserve the caveat and stop.

## Completion report must include

- scenario/run id;
- exact roof-fire footing source;
- horde before/after fields;
- signal artifact lines or decisive artifact path;
- timing/stability metrics;
- what is green vs still caveated;
- commit id if canon/code/scenario changes are committed.
