# Andi handoff: no promoted execution target

## Current canon state

`C-AOL actual playtest verification stack v0` is **CLOSED / CHECKPOINTED READY FOR SCHANI REVIEW**.

Current compact canon says there is no active or greenlit Andi execution target. Do not chase the now-solved roof/fire lane, and do not reopen any proof from this handoff.

Schani plans-aux is the next owner/action: accept the closed actual-playtest checkpoint, explicitly promote a future lane, or ask Josef for a product decision if stricter proof is wanted.

## Completed actual-playtest stack boundary

Preserve these completed proofs as canon, not as rerun invitations:

- source-zone player fire: `.userdata/dev-harness/harness_runs/20260429_153253/`, `bandit.live_world_nearby_camp_visible_brazier_source_zone_firestarter_action_mcw`; real normal Apply inventory/deploy/lighter path, saved `f_brazier` + `fd_fire`;
- player-lit fire -> bandit signal: `.userdata/dev-harness/harness_runs/20260429_162100/`, `bandit.player_lit_fire_signal_wait_mcw`; bounded wait, live signal/dispatch, saved active scout response;
- roof/elevated fire source: `.userdata/dev-harness/harness_runs/20260429_172847/`; normal player-created roof `t_tile_flat_roof` + `f_brazier` + `fd_fire`;
- roof-fire horde split proof: `.userdata/dev-harness/harness_runs/20260429_180239/`, `bandit.roof_fire_horde_split_wait_from_player_fire_mcw`; live roof-fire horde signal plus saved horde retarget/movement-budget response;
- Smart Zone Manager live layout: implemented-but-unproven / Josef playtest package, not agent-side green.

Evidence boundaries to preserve:

- source-zone fire proof is not roof proof;
- roof proof is the split-run packet from the saved player-created roof-fire world;
- roof-horde `tracking_intensity` remained `0`, so the credited response is retarget/movement-budget metadata after live roof-fire horde signaling, not positive tracking-intensity proof;
- Smart Zone deterministic geometry remains support only and does not prove the live Zone Manager coordinate-label claim.

## Future-only watchlist unless Schani/Josef promotes it

- Smart Zone Manager live UI coordinate-label proof, but only with a materially repaired UI-entry primitive or Josef manual evidence;
- natural three/four-site player-pressure behavior;
- true zero-site idle baseline;
- stricter roof-horde positive `tracking_intensity` proof;
- GitHub normal-download release packet, which needs explicit promotion from current canon before Andi treats it as the active lane.

## Canonical anchors

- `Plan.md`
- `TODO.md`
- `TESTING.md`
- `SUCCESS.md`
- stack contract: `doc/c-aol-actual-playtest-verification-stack-v0-2026-04-27.md`
- roof-fire closure proof: `doc/roof-fire-horde-detection-proof-v0-2026-04-29.md`
- green signal proof: `doc/player-lit-fire-bandit-signal-verification-v0-2026-04-29.md`
- green fire proof: `doc/fuel-visible-brazier-source-zone-firestarter-action-v0-2026-04-29.md`

## Non-goals/cautions

- Do not rerun solved source-zone fire, bandit signal, or roof-fire horde proofs as ritual.
- Do not use `fuel_writeback_source_zone_v0_2026-04-29`.
- Do not debug-inject the credited fire/light/smoke for product proof.
- Do not promote watchlist items, release work, or Smart Zone reruns from this file alone.
