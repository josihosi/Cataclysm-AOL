# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

`C-AOL actual playtest verification stack v0` advanced again: the player-lit fire / bandit signal response packet is green. Preserve the old boundary: `fuel_writeback_source_zone_v0_2026-04-29` remains broken/retired and must not be used as a fire/lighter proof surface.

Green real-fire + signal proof to preserve:

- source real-fire run: `.userdata/dev-harness/harness_runs/20260429_153253/`, scenario `bandit.live_world_nearby_camp_visible_brazier_source_zone_firestarter_action_mcw`
- signal fixture: `player_lit_fire_bandit_signal_wait_v0_2026-04-29`
- signal scenario: `bandit.player_lit_fire_signal_wait_mcw`
- signal run: `.userdata/dev-harness/harness_runs/20260429_162100/`
- result: `verdict=artifacts_matched`, `evidence_class=feature-path`, `feature_proof=true`, step ledger 14/14 green, wait ledger green
- path proven: saved real player-created `f_brazier` + `fd_fire` -> mineral water plus `AUTO_DRINK` support zone -> real wait UI 30-minute choice -> saved turn delta `1800` after guarded save/writeback -> same-run live smoke/fire signal scan/maintenance -> matched `bandit_camp` -> dispatch plan `candidate_reason=live_signal` -> saved active `bandit_camp` scout response targeting `player@140,41,0` with `known_recent_marks` including `live_smoke@140,41,0`
- decisive files: `probe.report.json`, `probe.step_ledger.json`, `wait_30_minutes_for_player_lit_fire_bandit_signal.wait_menu.screen_text.json`, `audit_saved_turn_after_player_lit_fire_signal_wait.metadata.json`, `audit_player_lit_fire_signal_dispatch_artifact.log_delta.txt`, `audit_saved_bandit_live_world_after_player_lit_signal_save.metadata.json`
- proof doc: `doc/player-lit-fire-bandit-signal-verification-v0-2026-04-29.md`

Still preserve earlier gates as scoped support:

- clean normal-map fixture: `fuel_source_zone_clean_normal_map_v0_2026-04-29`, run `.userdata/dev-harness/harness_runs/20260429_143149/`
- visible deployed-brazier/source-zone gate without lighter keys: `.userdata/dev-harness/harness_runs/20260429_144805/`, `bandit.live_world_nearby_camp_visible_brazier_source_zone_gate_mcw`
- normal player ignition/writeback: `.userdata/dev-harness/harness_runs/20260429_153253/`, `bandit.live_world_nearby_camp_visible_brazier_source_zone_firestarter_action_mcw`
- prior failed action diagnostic: `.userdata/dev-harness/harness_runs/20260429_142257/` remains non-proof for ignition because `A` did not show `Light where?`

Canonical anchors:

- Stack contract: `doc/c-aol-actual-playtest-verification-stack-v0-2026-04-27.md`
- Green normal-map entry: `doc/fuel-normal-map-entry-primitive-packet-v0-2026-04-29.md`
- Visible deployed-brazier gate: `doc/fuel-visible-brazier-source-zone-gate-v0-2026-04-29.md`
- Green firestarter action proof: `doc/fuel-visible-brazier-source-zone-firestarter-action-v0-2026-04-29.md`
- Green player-lit fire bandit signal proof: `doc/player-lit-fire-bandit-signal-verification-v0-2026-04-29.md`
- Fuel repair packet: `doc/fuel-writeback-repair-via-wood-source-zone-packet-v0-2026-04-29.md`
- Save-transform corruption audit: `doc/fuel-source-zone-save-transform-corruption-audit-v0-2026-04-29.md`

Next narrow work queue:

1. `Roof-fire horde detection proof packet v0` is closed green via the materially different split-run route. Preserve source player-action roof-fire writeback run `.userdata/dev-harness/harness_runs/20260429_172847/`, credited split-run proof `.userdata/dev-harness/harness_runs/20260429_180239/`, scenario `bandit.roof_fire_horde_split_wait_from_player_fire_mcw`, fixture `roof_fire_horde_split_wait_from_player_fire_v0_2026-04-29`, and proof note `doc/roof-fire-horde-detection-proof-v0-2026-04-29.md`.
2. Preserve the exact roof-horde boundary: saved roof/elevated `t_tile_flat_roof` + `f_brazier` + `fd_fire` survived split-run load; bounded wait advanced `observed_delta_turns=300`; same-run live roof-fire horde light signal logged `horde_signal_power=20`; staged `mon_zombie` horde retargeted from `[3367,874,1]` to roof-fire source submap `[3360,984,1]` with `moves=8400` / `last_processed=5267242`. Do not overclaim positive `tracking_intensity`; it remained `0`.
3. Preserve the prior partial-boundary/postmortem doc `doc/roof-fire-horde-player-action-boundary-v0-2026-04-29.md` as the reason split-run was required, not as the final state.
4. Reproducible horde setup remains artifacted separately: fixture `roof_fire_horde_staged_horde_v0_2026-04-29`, scenario `bandit.roof_fire_horde_staged_horde_audit_mcw`, run `.userdata/dev-harness/harness_runs/20260429_170116/`, proof note `doc/roof-fire-horde-staged-horde-footing-v0-2026-04-29.md`. This is horde setup/metadata support only.
5. Keep natural three/four-site player-pressure behavior and true zero-site idle baseline as decision/watchlist items unless Schani/Josef explicitly promotes them.

Proof discipline:

- OCR/metadata are fallback evidence unless the image was directly inspected.
- Do not launder setup metadata, stale screenshots, debug `fd_fire`, synthetic loaded-map fields, or item-info text into product proof.
- For roof/horde, the only new green product slice is roof/elevated player-created `fd_fire` writeback. Horde detection/response still needs a fresh, materially different wait/response proof path or Josef evidence.
