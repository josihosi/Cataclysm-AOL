# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

`C-AOL actual playtest verification stack v0` advanced: the source-zone fuel repair now has green normal player-action ignition proof. The old `fuel_writeback_source_zone_v0_2026-04-29` proof surface remains broken/retired; do **not** use it as a fire/lighter proof surface.

Green fuel/fire proof to preserve:

- scenario: `bandit.live_world_nearby_camp_visible_brazier_source_zone_firestarter_action_mcw`
- fixture: `fuel_visible_brazier_deploy_source_zone_nested_lighter_v0_2026-04-29`
- run: `.userdata/dev-harness/harness_runs/20260429_153253/`
- result: `verdict=artifacts_matched`, `evidence_class=feature-path`, `feature_proof=true`, step ledger 31/31 green
- path proven: normal map UI -> normal Apply inventory `brazier` -> `Deploy where?` -> east/right -> normal Apply inventory `lighter` -> exact UI trace `Light where?` -> east/right target -> `SOURCE_FIREWOOD` prompt -> uppercase `Y` -> recognizable ignition OCR -> save prompt -> mtime advance -> saved target tile `f_brazier` + `fd_fire`
- decisive files: `probe.report.json`, `probe.step_ledger.json`, `target_direction_east_to_visible_brazier_logs.after.screen_text.json`, `confirm_burn_source_firewood_prompt_after_visible_brazier.after.screen_text.json`, `audit_player_save_mtime_after_visible_brazier_ignition_save.metadata.json`, `audit_saved_target_tile_for_visible_brazier_fd_fire.metadata.json`
- proof doc: `doc/fuel-visible-brazier-source-zone-firestarter-action-v0-2026-04-29.md`

Still preserve earlier gates as scoped support:

- clean normal-map fixture: `fuel_source_zone_clean_normal_map_v0_2026-04-29`, run `.userdata/dev-harness/harness_runs/20260429_143149/`
- visible deployed-brazier/source-zone gate without lighter keys: `.userdata/dev-harness/harness_runs/20260429_144805/`, `bandit.live_world_nearby_camp_visible_brazier_source_zone_gate_mcw`
- prior failed action diagnostic: `.userdata/dev-harness/harness_runs/20260429_142257/` remains non-proof for ignition because `A` did not show `Light where?`

Canonical anchors:

- Stack contract: `doc/c-aol-actual-playtest-verification-stack-v0-2026-04-27.md`
- Green normal-map entry: `doc/fuel-normal-map-entry-primitive-packet-v0-2026-04-29.md`
- Visible deployed-brazier gate: `doc/fuel-visible-brazier-source-zone-gate-v0-2026-04-29.md`
- Green firestarter action proof: `doc/fuel-visible-brazier-source-zone-firestarter-action-v0-2026-04-29.md`
- Fuel repair packet: `doc/fuel-writeback-repair-via-wood-source-zone-packet-v0-2026-04-29.md`
- Save-transform corruption audit: `doc/fuel-source-zone-save-transform-corruption-audit-v0-2026-04-29.md`

Next narrow work queue:

1. Start `Player-lit fire and bandit signal verification packet v0` from the green real-fire proof above or a fresher equivalent; add bounded wait/time passage and bandit signal response/metadata.
2. Before long waiting, make the waiting setup survivable and non-noisy: spawn/provide mineral water and set up a Smart Zone / auto-drink zone around the player so thirst does not become the whole proof.
3. For the long-wait path, verify before/after time/turns. If time does not pass, capture a screenshot of the blocking warning/prompt and classify that exact prompt; do not summarize it as “time did not pass” without the visible blocker.
4. When wait-interruption prompts offer `I` or `N`, prefer `I` over `N`; be prepared to handle the same prompt class multiple times during one long wait, with screenshot evidence for each distinct prompt class before responding.
5. Do not claim bandit signal from the source-zone ignition proof alone; it proves real player fire + saved `fd_fire`, not signal response.
6. Do not rerun fire/lighter proof on `fuel_writeback_source_zone_v0_2026-04-29`.
7. Keep `Roof-fire horde detection proof packet v0` separate: it still needs roof/elevated-position plus real player-created roof fire/light/smoke and horde response metadata.

Proof discipline:

- OCR/metadata are fallback evidence unless the image was directly inspected.
- Do not launder setup metadata, stale screenshots, debug `fd_fire`, synthetic loaded-map fields, or item-info text into product proof.
- For the next signal probe, require the real fire source plus bounded wait/time passage and claim-scoped bandit artifact/metadata; startup/load success alone is not signal proof.
