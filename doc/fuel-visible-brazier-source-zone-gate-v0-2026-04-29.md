# Fuel visible deployed-brazier source-zone gate v0 (2026-04-29)

## Verdict

The earlier source-zone fuel footing had saved metadata for `f_brazier`, logs, and `SOURCE_FIREWOOD`, but that is not enough player-facing product proof. Josef flagged the right risk: saved `observed_furniture=[f_brazier]` does not by itself prove the player can see/use a deployed brazier surface.

A new visible deployed-brazier gate is now green. It proves the normal product path up to deployed brazier + logs + source zone, and still sends **no lighter/fire keys**.

## New fixture and scenario

Fixture:

- `fuel_visible_brazier_deploy_source_zone_v0_2026-04-29`
- path: `tools/openclaw_harness/fixtures/saves/live-debug/fuel_visible_brazier_deploy_source_zone_v0_2026-04-29/manifest.json`
- source fixture: `bandit_live_world_nearby_camp_real_fire_exact_items_v0_2026-04-27`
- this is a fresher clean player-action fixture path, not the broken old `fuel_writeback_source_zone_v0_2026-04-29` proof surface.
- fixture adds only real saved `log` items and a `SOURCE_FIREWOOD` zone near/east of the player.
- fixture does **not** inject `f_brazier` or `fd_fire`.

Scenario:

- `bandit.live_world_nearby_camp_visible_brazier_source_zone_gate_mcw`
- path: `tools/openclaw_harness/scenarios/bandit.live_world_nearby_camp_visible_brazier_source_zone_gate_mcw.json`
- command: `python3 tools/openclaw_harness/startup_harness.py probe bandit.live_world_nearby_camp_visible_brazier_source_zone_gate_mcw`

## Green evidence

Run:

- `.userdata/dev-harness/harness_runs/20260429_144805/`
- verdict: `artifacts_matched`
- feature proof: `true`
- step ledger: 20/20 green, `green_step_local_proof`

Menu / placement screenshots:

- apply inventory opened: `.userdata/dev-harness/harness_runs/20260429_144805/open_apply_inventory_for_brazier_text_guard.after.png`
- brazier filter typed: `.userdata/dev-harness/harness_runs/20260429_144805/type_brazier_filter_for_apply_text_guard.after.png`
- filtered brazier selected / deploy prompt transition: `.userdata/dev-harness/harness_runs/20260429_144805/select_filtered_brazier_expect_deploy_prompt.after.png`
- east/right placement after `Deploy where?`: `.userdata/dev-harness/harness_runs/20260429_144805/place_prompted_brazier_east.after.png`

Player-facing deployed-brazier inspection:

- look opened after placement: `.userdata/dev-harness/harness_runs/20260429_144805/open_look_after_prompted_brazier_deploy.after.png`
- look/OCR artifact: `.userdata/dev-harness/harness_runs/20260429_144805/open_look_after_prompted_brazier_deploy.after.screen_text.json`
- east target named/described: `.userdata/dev-harness/harness_runs/20260429_144805/look_east_at_deployed_brazier.after.png`
- OCR artifact: `.userdata/dev-harness/harness_runs/20260429_144805/look_east_at_deployed_brazier.after.screen_text.json`
- OCR fallback text includes `hrazıer` (OCR for brazier) and `burn things`, plus the description fragment `raised metal dish`, proving the player-facing inspected target is the brazier surface rather than metadata-only furniture.

Saved-state re-audit after visible deployment:

- `.userdata/dev-harness/harness_runs/20260429_144805/audit_saved_target_tile_for_visible_brazier_logs.metadata.json`
  - `observed_furniture=[f_brazier]`
  - `observed_items=[log]`
- `.userdata/dev-harness/harness_runs/20260429_144805/audit_saved_source_firewood_zone_after_visible_brazier_deploy.metadata.json`
  - `required_zone_type=SOURCE_FIREWOOD`
  - `required_faction=your_followers`
  - `required_name_contains=OpenClaw visible brazier source firewood proof zone`

## Boundary

This gate proves visible/product-path brazier deployment plus logs/source-zone footing. It does not prove lighter activation, `Light where?`, ignition, save mtime after ignition, `fd_fire`, `fd_smoke`, smoke/light, or bandit signal response.

Do not continue fire proof from metadata-only `f_brazier` confidence. Any future fire proof must cite this visible deployed-brazier gate or a fresher equivalent, then separately prove the charged lighter/apply-wielded path and `Light where?` before any source-firewood confirmation or ignition claim.
