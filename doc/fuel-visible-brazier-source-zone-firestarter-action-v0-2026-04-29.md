# Fuel visible deployed-brazier source-zone firestarter action v0 (2026-04-29)

## Verdict

Green feature-path proof for the normal player action from visible deployed brazier + source firewood to same-run saved `fd_fire`.

The old `fuel_writeback_source_zone_v0_2026-04-29` proof surface remains retired/broken and is not credited. This proof uses a new nested-lighter fixture and the normal Apply-inventory product path:

1. start on normal map UI;
2. select `brazier` from normal Apply inventory;
3. prove `Deploy where?` and place east/right;
4. select charged `lighter` from normal Apply inventory;
5. prove exact `Light where?` before targeting;
6. target east/right into the visibly deployed brazier/log/source-zone tile;
7. accept the case-sensitive `SOURCE_FIREWOOD` prompt with uppercase `Y`;
8. require recognizable ignition OCR before save/writeback;
9. save, prove player-save mtime advanced, and audit saved target tile for `f_brazier` + `fd_fire`.

## Fixture and scenario

Fixture:

- `fuel_visible_brazier_deploy_source_zone_nested_lighter_v0_2026-04-29`
- path: `tools/openclaw_harness/fixtures/saves/live-debug/fuel_visible_brazier_deploy_source_zone_nested_lighter_v0_2026-04-29/manifest.json`
- source fixture: `bandit_basecamp_playtest_kit_v0_2026-04-22`
- adds a wielded `brazier`, one live-accessible worn-pocket `lighter` with `charges=100` plus nested `butane` `charges=100`, 100 `log` items east of the player, and `SOURCE_FIREWOOD` zone `OpenClaw visible brazier source firewood proof zone`.
- does **not** inject `fd_fire`.

Scenario:

- `bandit.live_world_nearby_camp_visible_brazier_source_zone_firestarter_action_mcw`
- path: `tools/openclaw_harness/scenarios/bandit.live_world_nearby_camp_visible_brazier_source_zone_firestarter_action_mcw.json`
- command: `python3 tools/openclaw_harness/startup_harness.py probe bandit.live_world_nearby_camp_visible_brazier_source_zone_firestarter_action_mcw`

## Green evidence

Run:

- `.userdata/dev-harness/harness_runs/20260429_153253/`
- verdict: `artifacts_matched`
- evidence class: `feature-path`
- feature proof: `true`
- step ledger: 31/31 green, `green_step_local_proof`
- cleanup: launched `cataclysm-tiles --userdir .userdata/dev-harness/ --world McWilliams`, then auto-terminated with `SIGTERM` after artifact capture.

Top-level claim-scoped artifact patterns matched the normal UI trace path:

- filtered Apply inventory returned `brazier`;
- `Deploy where?` consumed `RIGHT` / `dx=1 dy=0 dz=0`;
- filtered Apply inventory returned `lighter`;
- exact `direction_prompt event=open message="Light where?"` occurred before targeting;
- `Light where?` consumed `RIGHT` / `dx=1 dy=0 dz=0`.

Precondition metadata:

- `.userdata/dev-harness/harness_runs/20260429_153253/audit_saved_inventory_has_brazier_and_lighter_before_apply.metadata.json`
  - live-accessible `brazier` count 1 at `player.weapon`;
  - live-accessible `lighter` count 1 with `charges=100` at worn-pocket path;
  - live-accessible `butane` present.
- `.userdata/dev-harness/harness_runs/20260429_153253/audit_saved_source_firewood_zone_after_visible_brazier_deploy.metadata.json`
  - required `SOURCE_FIREWOOD` / `your_followers` zone present after visible deploy.

Prompt and ignition OCR gates:

- `.userdata/dev-harness/harness_runs/20260429_153253/target_direction_east_to_visible_brazier_logs.after.screen_text.json`
  - matched `burn your firewood source` from OCR line `you really want to burn your firewood source? (Case Sensitive)`.
- `.userdata/dev-harness/harness_runs/20260429_153253/confirm_burn_source_firewood_prompt_after_visible_brazier.after.screen_text.json`
  - matched ignition guard `a fir` from OCR line `Dau successfillu linht a fire` (OCR fallback for `You successfully light a fire.`).
- `.userdata/dev-harness/harness_runs/20260429_153253/request_direct_save_after_visible_brazier_ignition.after.screen_text.json`
  - matched `Save and quit` before the uppercase save confirmation.

Same-run writeback:

- `.userdata/dev-harness/harness_runs/20260429_153253/audit_player_save_mtime_after_visible_brazier_ignition_save.metadata.json`
  - `status=required_state_present`;
  - `mtime_changed_since_required_label=true`;
  - `save_mtime_iso=2026-04-29T15:34:04.648315`.
- `.userdata/dev-harness/harness_runs/20260429_153253/audit_saved_target_tile_for_visible_brazier_fd_fire.metadata.json`
  - `status=required_state_present`;
  - `observed_furniture=[f_brazier]`;
  - `observed_field_ids=[fd_fire]`;
  - `missing_required_fields=[]`, `missing_required_furniture=[]`.

## Boundary

This closes the source-zone fuel repair proof for normal player ignition and same-run saved `fd_fire` on the visibly deployed brazier/log tile.

It does **not** prove bandit signal response, horde response, roof/elevated fire, long-duration smoke/light behavior, or `fd_smoke`. Those remain separate playtest-stack items. The next honest stack target is `Player-lit fire and bandit signal verification packet v0`, starting from this green real-fire proof or a fresher equivalent.
