# Andi handoff: Smart Zone live coordinate-label proof

## Current canon state

`Smart Zone Manager live coordinate-label proof v0` is **CLOSED / GREEN LIVE COORDINATE-LABEL PROOF**.

Green run: `.userdata/smart-zone-audit-live-20260429e/harness_runs/20260429_225644/`, scenario `smart_zone.live_coordinate_label_proof_v0_tmp9` (stabilized as `tools/openclaw_harness/scenarios/smart_zone.live_coordinate_label_proof_v0.json`). Report: `feature_proof=true`, `evidence_class=feature-path`, `verdict=artifacts_matched`, 18/18 green step-local rows.

Proof chain: load gameplay -> open real `Zones manager` -> add `Basecamp: Storage` -> accept Smart Zone Manager prompt with **Yes** -> result `placed_zones=23` -> close/save-changes -> reopen Zone Manager -> audit generated row coordinate labels and absolute coordinates. Decisive evidence is the live `zone_manager_row` redraw trace (`visible_label`, `compact_label`, `start_abs`, `end_abs`, `center_abs`), with screenshot/OCR as fallback UI checkpoint evidence.

Verdict: green for the live coordinate-label/lumping claim. The generated layout spans distinct relative labels/coordinates and is not the one-tile lumping bug. Some related rows intentionally share labels/tiles; that is not by itself treated as collapse.

Boundary: this proves live create/inspect/close-save/reopen, not a separate full process-reload disk-persistence audit after SIGTERM cleanup.

## Completed actual-playtest stack boundary

Preserve these completed proofs as canon, not as rerun invitations:

- source-zone player fire: `.userdata/dev-harness/harness_runs/20260429_153253/`, `bandit.live_world_nearby_camp_visible_brazier_source_zone_firestarter_action_mcw`; real normal Apply inventory/deploy/lighter path, saved `f_brazier` + `fd_fire`;
- player-lit fire -> bandit signal: `.userdata/dev-harness/harness_runs/20260429_162100/`, `bandit.player_lit_fire_signal_wait_mcw`; bounded wait, live signal/dispatch, saved active scout response;
- roof/elevated fire source: `.userdata/dev-harness/harness_runs/20260429_172847/`; normal player-created roof `t_tile_flat_roof` + `f_brazier` + `fd_fire`;
- roof-fire horde split proof: `.userdata/dev-harness/harness_runs/20260429_180239/`, `bandit.roof_fire_horde_split_wait_from_player_fire_mcw`; live roof-fire horde signal plus saved horde retarget/movement-budget response;
- Smart Zone Manager live layout: green coordinate-label proof at `.userdata/smart-zone-audit-live-20260429e/harness_runs/20260429_225644/`.

Evidence boundaries to preserve:

- source-zone fire proof is not roof proof;
- roof proof is the split-run packet from the saved player-created roof-fire world;
- roof-horde `tracking_intensity` remained `0`, so the credited response is retarget/movement-budget metadata after live roof-fire horde signaling, not positive tracking-intensity proof;
- Smart Zone deterministic geometry remains support; the live coordinate-label claim is now closed by the real Zone Manager proof, not by deterministic tests alone.

## Active work

No active execution target is currently promoted in repo canon. Next work should be promoted by Schani/Josef plan aux rather than inferred from old watchlist items.

## Future-only watchlist unless Schani/Josef promotes it

- natural three/four-site player-pressure behavior;
- true zero-site idle baseline;
- stricter roof-horde positive `tracking_intensity` proof;
- full Smart Zone process-reload disk-persistence audit, if Josef wants stricter persistence proof;
- GitHub normal-download release packet, which needs explicit promotion from current canon before Andi treats it as the active lane.

## Canonical anchors

- `Plan.md`
- `TODO.md`
- `TESTING.md`
- `SUCCESS.md`
- Smart Zone contract: `doc/smart-zone-manager-harness-audit-retry-packet-v0-2026-04-28.md`
- stack contract: `doc/c-aol-actual-playtest-verification-stack-v0-2026-04-27.md`
- roof-fire closure proof: `doc/roof-fire-horde-detection-proof-v0-2026-04-29.md`
- green signal proof: `doc/player-lit-fire-bandit-signal-verification-v0-2026-04-29.md`
- green fire proof: `doc/fuel-visible-brazier-source-zone-firestarter-action-v0-2026-04-29.md`

## Non-goals/cautions

- Do not rerun solved Smart Zone, source-zone fire, bandit signal, or roof-fire horde proofs as ritual.
- Do not use `fuel_writeback_source_zone_v0_2026-04-29`.
- Do not debug-inject the credited fire/light/smoke for product proof.
- Do not promote watchlist items or release work from this file alone.
