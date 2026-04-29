# Andi handoff: Smart Zone live coordinate-label proof

## Current canon state

`Smart Zone Manager live coordinate-label proof v0` is **ACTIVE / GREENLIT AGENT-SIDE PROBE**.

Josef's correction: this should be straightforward. Set up/load a usable Basecamp zone, open the real Zone Manager, invoke Smart Zone generation, press **Yes**, then screenshot/OCR the Zone Manager list coordinate labels for each generated zone. Do not park just because the old clean-profile/key-delivery attempt failed; use a materially working UI route.

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

## Active work

1. Start from the smallest current-runtime loadable basecamp/profile/fixture that reaches gameplay and can open Zone Manager.
2. Prove `Zones manager` is actually open before sending generation keys. The old `action_menu` trace remains a blocker if it recurs.
3. Invoke Smart Zone generation through the real UI and accept the prompt with **Yes**.
4. Inspect/reopen the generated Zone Manager list and capture screenshot/OCR of zone names plus visible coordinate labels (`2E`-style offsets).
5. Extract saved/reopened zone metadata where available: zone names/types/options/absolute coordinates.
6. Classify the result directly: same coordinate labels mean the lumping bug still exists; distinct expected labels plus matching metadata means live layout proof is green.

If code changes are needed to repair UI entry/keybinding or harness observation, make the smallest such repair. If the probe proves the product is still wrong, do not redesign silently; report the exact visible failure and metadata.

## Future-only watchlist unless Schani/Josef promotes it

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
- Do not promote watchlist items or release work from this file alone.
