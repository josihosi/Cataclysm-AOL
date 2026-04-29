# Andi handoff: actual-playtest verification stack

## Current canon state

`Roof-fire horde detection proof packet v0` is the active target again, but **not** as a same-shape attempt 5.

**PARTIAL ROOF-FIRE GREEN / HORDE RESPONSE UNPROVEN / MATERIAL NEW METHOD GREENLIT**

Josef explicitly loosened the latch: do the method that works instead of stopping on a technicality. The repo canon in `Plan.md`, `TODO.md`, and `TESTING.md` wins over any older handoff wording.

## What is green and must be preserved

Previous source-zone signal proof remains complete/green and should not be rerun as ritual:

- source real-fire run: `.userdata/dev-harness/harness_runs/20260429_153253/`
- signal fixture: `player_lit_fire_bandit_signal_wait_v0_2026-04-29`
- signal scenario: `bandit.player_lit_fire_signal_wait_mcw`
- signal run: `.userdata/dev-harness/harness_runs/20260429_162100/`
- result: `verdict=artifacts_matched`, `evidence_class=feature-path`, `feature_proof=true`, step ledger 14/14 green, wait ledger green
- canonical proof doc: `doc/player-lit-fire-bandit-signal-verification-v0-2026-04-29.md`

Scoped roof-fire proof now also exists and must be kept in its proper evidence class:

- fixture: `roof_fire_horde_player_action_v0_2026-04-29`
- scenario: `bandit.roof_fire_horde_player_action_mcw`
- final run: `.userdata/dev-harness/harness_runs/20260429_172847/`
- proof note: `doc/roof-fire-horde-player-action-boundary-v0-2026-04-29.md`
- green slice: setup-only roof relocation and horde footing, normal Apply→brazier deploy, normal Apply→lighter→`Light where?`→east/source-firewood confirmation, guarded save/writeback, and saved roof/elevated target tile `t_tile_flat_roof` + `f_brazier` + `fd_fire`.

This is **player-created roof-fire writeback proof only**.

## What is still unproven

`Roof-fire horde detection proof packet v0` did **not** close:

- bounded in-game wait from the proven roof-fire state;
- live roof-fire horde light-signal artifact;
- saved horde detection/response metadata after elapsed time;
- positive response or decisive no-response verdict.

The final same-shape attempt failed because the direct save/writeback gate used save-and-quit. The subsequent wait was therefore main-menu state, not a valid in-game horde wait. `audit_roof_fire_horde_light_signal_artifact` missed the required live horde-signal lines.

## Current work

Do **not** run another same-shape roof/horde harness retry. Do run the materially different method that closes the real product question.

Greenlit routes, in preferred order:

1. Split-run proof from the saved player-created roof-fire world: load the saved world after the roof fire exists, prove the roof/elevated fire state is still present, then perform the bounded in-game wait and horde response audit from gameplay state.
2. If split-run is impractical, add/use a non-quitting save/writeback primitive so the same live session can continue into bounded wait after fire creation.
3. If both are blocked, package exact Josef manual playtest/save evidence needs.

This is not a latch/permission blocker anymore. Pick the smallest route that honestly proves: roof/elevated real player-created fire -> bounded elapsed time -> horde before/after detection or response metadata. If one route hits a technical wall, switch to the next materially different route instead of parking immediately. Keep screenshots, time/turn evidence, and horde metadata boundaries tight.

## Canonical anchors

- `Plan.md`
- `TODO.md`
- `TESTING.md`
- `SUCCESS.md`
- stack contract: `doc/c-aol-actual-playtest-verification-stack-v0-2026-04-27.md`
- roof-fire boundary: `doc/roof-fire-horde-player-action-boundary-v0-2026-04-29.md`
- green signal proof: `doc/player-lit-fire-bandit-signal-verification-v0-2026-04-29.md`
- green fire proof: `doc/fuel-visible-brazier-source-zone-firestarter-action-v0-2026-04-29.md`

## Non-goals/cautions

- Do not rerun solved player-lit source-zone fire -> bandit signal proof.
- Do not use `fuel_writeback_source_zone_v0_2026-04-29`.
- Do not claim roof/horde proof from source-zone `fd_fire`, bandit signal metadata, setup-only horde metadata, or the partial roof-fire writeback proof.
- Do not debug-inject the credited roof fire/light/smoke.
- Do not call a blocked/main-menu wait a no-response result.
- Keep natural three/four-site player-pressure behavior and true zero-site idle baseline as decision/watchlist items unless Schani/Josef explicitly promotes them.
