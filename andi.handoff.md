# Andi handoff: actual-playtest verification stack

## Current canon state

`Roof-fire horde detection proof packet v0` is **not** the next active retry. It has reached:

**PARTIAL ROOF-FIRE GREEN / HORDE RESPONSE UNPROVEN / JOSEF-SCHANI DECISION REQUIRED**

The repo canon in `Plan.md`, `TODO.md`, and `TESTING.md` wins over any older handoff wording. Do not start attempt 5 from this file.

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

## Stop rule

Do **not** run another same-shape roof/horde harness retry.

The repeated-test budget for this item is exhausted after four materially changed attempts and Frau consultation. A future retry requires Schani/Josef greenlighting a materially different method, such as:

1. split-run proof from the saved player-created roof-fire world;
2. a non-quitting save/writeback primitive so the same live session can continue into bounded wait;
3. Josef manual playtest/save evidence.

Without one of those, roof/horde stays parked as **roof fire proven / horde response unproven**.

## Next lane

No next lane is currently promoted in repo canon.

Next Andi run should start from `Plan.md` / `TODO.md` / `TESTING.md`, not from old prompt lore. If no sensible unblocked target is promoted, hold for Schani/Josef decision rather than inventing attempt 5.

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
