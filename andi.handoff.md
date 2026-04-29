# Andi handoff: actual-playtest verification stack

## Current canon state

`Roof-fire horde detection proof packet v0` is **COMPLETE / GREEN SPLIT-RUN FEATURE PROOF**.

Do not run another roof/horde retry as ritual. The green proof is canonized in `Plan.md`, `TODO.md`, `TESTING.md`, `SUCCESS.md`, and `doc/roof-fire-horde-detection-proof-v0-2026-04-29.md`.

## Roof-fire horde closure packet

Credited proof:

- source player-created roof-fire run: `.userdata/dev-harness/harness_runs/20260429_172847/`
- split-run proof: `.userdata/dev-harness/harness_runs/20260429_180239/`
- fixture: `roof_fire_horde_split_wait_from_player_fire_v0_2026-04-29`
- scenario: `bandit.roof_fire_horde_split_wait_from_player_fire_mcw`
- proof doc: `doc/roof-fire-horde-detection-proof-v0-2026-04-29.md`
- prior partial-boundary/postmortem: `doc/roof-fire-horde-player-action-boundary-v0-2026-04-29.md`

Evidence boundary:

- saved roof/elevated fire survived split-run load: `t_tile_flat_roof` + `f_brazier` + `fd_fire` at `[3368,994,1]`;
- bounded in-game wait advanced turn delta `300` (`5266942 -> 5267242`);
- same-run live roof-fire horde light signal artifact captured `horde_signal_power=20`, `elevated_roof_exposed=yes`, `vertical_sightline=yes`;
- saved horde response metadata changed: staged `mon_zombie` at `[3367,874,1]` retargeted `destination_ms` to `[3360,984,1]`, with `moves=8400` and `last_processed=5267242`.

Product/code seam:

- `src/do_turn.cpp` adds `live_bandit_fire_source_is_elevated_roof_exposed( const map &here, const tripoint_bub_ms &p )`.
- Elevated `t_flat_roof` / `t_tile_flat_roof` fire sources now count as exposed roof signals for live light packets.
- The light packet sets exposed/open treatment, `vertical_sightline=true`, `elevation_bonus=2`, and emits `elevated_roof_exposed=yes` in debug notes when appropriate.

Important caveat:

- `tracking_intensity` remained `0`, so the credited horde response is **retarget/movement-budget metadata after live roof-fire light signaling**, not positive tracking-intensity proof.

## What is green and must be preserved

Previous source-zone signal proof remains complete/green and should not be rerun as ritual:

- source real-fire run: `.userdata/dev-harness/harness_runs/20260429_153253/`
- signal fixture: `player_lit_fire_bandit_signal_wait_v0_2026-04-29`
- signal scenario: `bandit.player_lit_fire_signal_wait_mcw`
- signal run: `.userdata/dev-harness/harness_runs/20260429_162100/`
- result: `verdict=artifacts_matched`, `evidence_class=feature-path`, `feature_proof=true`, step ledger 14/14 green, wait ledger green
- canonical proof doc: `doc/player-lit-fire-bandit-signal-verification-v0-2026-04-29.md`

Roof-fire horde proof is now also green, with the caveat above. Do not downgrade it back to partial roof-fire writeback proof.

## Current open/watch items

- Smart Zone Manager remains implemented-but-unproven / Josef playtest package unless explicitly reopened with a repaired UI-entry primitive or Josef manual evidence.
- Natural three/four-site player-pressure behavior and true zero-site idle baseline remain decision/watchlist items, not current requirements.
- GitHub normal-download release packet remains held behind Josef's debug-correction/product-proof stack until Schani/Josef promotes it.

## Canonical anchors

- `Plan.md`
- `TODO.md`
- `TESTING.md`
- `SUCCESS.md`
- stack contract: `doc/c-aol-actual-playtest-verification-stack-v0-2026-04-27.md`
- roof-fire closure proof: `doc/roof-fire-horde-detection-proof-v0-2026-04-29.md`
- roof-fire partial-boundary postmortem: `doc/roof-fire-horde-player-action-boundary-v0-2026-04-29.md`
- green signal proof: `doc/player-lit-fire-bandit-signal-verification-v0-2026-04-29.md`
- green fire proof: `doc/fuel-visible-brazier-source-zone-firestarter-action-v0-2026-04-29.md`

## Non-goals/cautions

- Do not rerun solved player-lit source-zone fire -> bandit signal proof.
- Do not rerun solved roof-fire horde split-run proof unless Josef asks for a stricter evidence class.
- Do not use `fuel_writeback_source_zone_v0_2026-04-29`.
- Do not claim positive `tracking_intensity` for roof-fire horde proof.
- Do not debug-inject the credited roof fire/light/smoke.
- Do not call source-zone proof roof proof; the roof proof is the split-run packet above.
