# Fuel writeback repair via wood source zone packet v0 (2026-04-29)

## Status

COMPLETE / GREEN NORMAL PLAYER IGNITION + SAVED `fd_fire` under `C-AOL actual playtest verification stack v0`.

Imagination source: `doc/fuel-writeback-repair-via-wood-source-zone-imagination-source-of-truth-2026-04-29.md`.

Detailed green action proof: `doc/fuel-visible-brazier-source-zone-firestarter-action-v0-2026-04-29.md`.

## Why this exists

The old Multidrop filtered-`2x4` continuation and later old fixture path did not honestly prove normal player fire. Josef diagnosed the old `fuel_writeback_source_zone_v0_2026-04-29` proof surface as a broken save/fixture; it remains retired and must not be used for future fire/lighter proof.

The repaired path uses a player-facing product sequence instead of relying on saved `f_brazier` metadata:

1. start from normal map UI;
2. deploy a `brazier` through normal Apply inventory;
3. prove `Deploy where?` and place east/right;
4. select charged `lighter` through normal Apply inventory;
5. prove exact `Light where?` before targeting;
6. target east/right into the visibly deployed brazier/log/source-zone tile;
7. accept the `SOURCE_FIREWOOD` prompt with uppercase `Y`;
8. require recognizable ignition OCR before save/writeback;
9. save and audit same-run save mtime plus saved target-tile `fd_fire`.

## Scope

### In scope

- Real normal-map/player-facing brazier deployment.
- Real in-world source firewood/log footing plus `SOURCE_FIREWOOD` zone metadata.
- Charged lighter selected through a normal product path.
- Player-message/OCR bridge evidence for source-firewood prompt and ignition.
- Guarded save/writeback after ignition with saved `fd_fire` on the expected brazier tile.

### Out of scope

- Crediting the old Multidrop filtered-`2x4` path without new proof.
- Crediting `fuel_writeback_source_zone_v0_2026-04-29` for more fire/lighter proof.
- Closing player-lit fire from debug map-editor `fd_fire`, setup metadata, or synthetic loaded-map fields.
- Claiming bandit signal response, roof-fire horde response, long-duration smoke/light behavior, or `fd_smoke`; those are separate stack items.
- Broad Zone Manager proof. This packet uses source-firewood zoning only as fuel-source footing.

## Success state

- [x] Broken-save fixture boundary is named: `fuel_writeback_source_zone_v0_2026-04-29` is retired, and `.userdata/dev-harness/harness_runs/20260429_142257/` remains prior non-proof because apply-wielded `A` did not show `Light where?`.
- [x] Clean first-load footing exists: `fuel_source_zone_clean_normal_map_v0_2026-04-29`, run `.userdata/dev-harness/harness_runs/20260429_143149/`, feature-path green normal-map entry with charged lighter, saved `f_brazier`, real `log`, and `SOURCE_FIREWOOD`.
- [x] Visible/operational deployed-brazier gate exists: `.userdata/dev-harness/harness_runs/20260429_144805/`, `bandit.live_world_nearby_camp_visible_brazier_source_zone_gate_mcw`, 20/20 green, normal Apply inventory -> `brazier` -> `Deploy where?` -> east/right, look/OCR `hrazıer` + `burn things`, saved `f_brazier` + `log`, saved `SOURCE_FIREWOOD`, no lighter/fire keys.
- [x] Corrected firestarter scenario is claim-classified: `bandit.live_world_nearby_camp_visible_brazier_source_zone_firestarter_action_mcw` has top-level artifact patterns for the normal Apply/deploy/lighter/`Light where?` UI trace path.
- [x] Normal player fire-start/lighter action reaches the visible brazier/fuel path from normal map UI: `.userdata/dev-harness/harness_runs/20260429_153253/` is feature-path green, `feature_proof=true`, 31/31 green step-local rows.
- [x] UI trace proves the decisive product path: filtered Apply inventory returned `brazier`, `Deploy where?` consumed east/right, filtered Apply inventory returned `lighter`, exact `Light where?` opened before targeting, and `Light where?` consumed east/right.
- [x] OCR bridge evidence proves source-firewood and ignition boundaries: `target_direction_east_to_visible_brazier_logs.after.screen_text.json` matched `burn your firewood source`; `confirm_burn_source_firewood_prompt_after_visible_brazier.after.screen_text.json` matched ignition guard `a fir` from OCR line `Dau successfillu linht a fire`; `request_direct_save_after_visible_brazier_ignition.after.screen_text.json` matched `Save and quit` before confirmation.
- [x] Guarded post-ignition save/writeback proves actual saved `fd_fire`: `audit_player_save_mtime_after_visible_brazier_ignition_save.metadata.json` has `mtime_changed_since_required_label=true`, and `audit_saved_target_tile_for_visible_brazier_fd_fire.metadata.json` has `observed_furniture=[f_brazier]` and `observed_field_ids=[fd_fire]` with no missing required field/furniture.
- [ ] Bounded wait evidence and bandit live-signal response remain separate under `Player-lit fire and bandit signal verification packet v0`.

## Validation notes

Green proof run: `.userdata/dev-harness/harness_runs/20260429_153253/`.

Command: `python3 tools/openclaw_harness/startup_harness.py probe bandit.live_world_nearby_camp_visible_brazier_source_zone_firestarter_action_mcw`.

Fixture: `fuel_visible_brazier_deploy_source_zone_nested_lighter_v0_2026-04-29`. This fixture provides a wielded `brazier`, a live-accessible worn-pocket `lighter` with nested `butane` charges, logs east of the player, and `SOURCE_FIREWOOD` zone metadata; it does not inject `fd_fire`.

Scenario: `tools/openclaw_harness/scenarios/bandit.live_world_nearby_camp_visible_brazier_source_zone_firestarter_action_mcw.json`.

Classifier: `verdict=artifacts_matched`, `evidence_class=feature-path`, `feature_proof=true`; step ledger: `green_step_local_proof`, `green_count=31`, `yellow_count=0`, `red_or_blocked_count=0`.

Evidence boundary: this closes normal player source-zone ignition and same-run saved `fd_fire` only. It does not prove bandit signal response, `fd_smoke`, roof/elevated fire, horde response, or long-duration smoke/light behavior. Direct image-model inspection was not used; screenshot artifacts plus OCR/metadata are named fallback evidence.

Use the proof-freeze discipline from `doc/c-aol-harness-proof-freeze-matrix-v0-2026-04-28.md`: setup helpers are footing only. Feature proof needs clean startup, green step-local rows, claim-scoped artifacts, guarded save/writeback, and no yellow/blocked wait ledger for the claimed behavior. In-game message/debug log lines are useful bridge evidence for the player action path; pair them with screenshot paths, named visible facts, save mtime, and saved-state proof instead of broad log dumps.
