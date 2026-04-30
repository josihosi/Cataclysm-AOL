# Andi handoff: CAOL-ROOF-HORDE-NICE-FIRE-v0

## Current canon state

`CAOL-ROOF-HORDE-NICE-FIRE-v0` is **CLOSED / CHECKPOINTED GREEN V0**.

Authoritative receipts: `Plan.md`, `SUCCESS.md`, `TESTING.md`, `doc/work-ledger.md`, and closure proof `doc/roof-fire-horde-nice-roof-fire-proof-v0-2026-04-30.md`. If this file ever disagrees with those, repair this file from canon.

`CAOL-WRITHING-STALKER-v0` remains closed and must not be reopened. The old mixed-hostile horde caveat was promoted into this focused roof-fire proof and is now closed for v0 under the caveats below.

## Green proof

- Scenario: `bandit.roof_fire_horde_nice_roof_fire_mcw`
- Run: `.userdata/dev-harness/harness_runs/20260430_191556/`
- Source roof-fire footing: `.userdata/dev-harness/harness_runs/20260429_172847/`
- Prior split footing/proof: `.userdata/dev-harness/harness_runs/20260429_180239/`
- Proof doc: `doc/roof-fire-horde-nice-roof-fire-proof-v0-2026-04-30.md`

Credited behavior:
1. Saved roof/elevated fire before/after wait is `t_tile_flat_roof` + `f_brazier` + `fd_fire`, tied to the normal player-created roof-fire source chain.
2. Horde footing before wait is setup-only: `mon_zombie` at offset `[0,-120,0]`, destination self, `tracking_intensity=0`, `last_processed=0`, `moves=0`.
3. Bounded wait advances `300` turns (`5266942` -> `5267242`).
4. Same-run live roof-fire horde signal fires: `source_omt=(140,41,1) horde_signal_power=20 ... elevated_exposure_extended=yes`.
5. Saved horde response after wait: destination retargets to `[3360,984,1]`, `last_processed=5267242`, `moves=8400`.
6. Cost/stability: wall-clock `2:34.72`, `14/14` step rows green, `1/1` wait rows green, no runtime warnings/abort; horde-specific timing `not instrumented`.

## Caveats / do not overclaim

- `tracking_intensity` remained `0`; do not claim positive tracking-intensity behavior.
- Horde-specific micro-timing is not instrumented separately.
- This closes focused roof-fire signal/retarget/move-budget response, not broad horde combat/pathfinding or natural multi-day discovery.
- Do not rerun this proof, mixed-hostile soup, or writhing-stalker work unless Schani/Josef explicitly promote a stricter follow-up.

## Next executor state

No active target is currently promoted. Wait for Schani/Josef to promote the next lane.
