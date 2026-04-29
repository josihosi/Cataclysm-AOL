# Fuel writeback repair via wood source zone packet v0 (2026-04-29)

## Status

JOSEF-REOPENED STEP-SCREENSHOT ACTION AUDIT under `C-AOL actual playtest verification stack v0`.

Imagination source: `doc/fuel-writeback-repair-via-wood-source-zone-imagination-source-of-truth-2026-04-29.md`.

## Why this exists

The scoped normal brazier deploy gate is already green: selected `brazier` -> `Deploy where?` -> east/right key consumed -> guarded save/writeback -> saved east-tile `f_brazier`.

The old continuation is red at `blocked_untrusted_drop_filter_or_inventory_visibility`: the Multidrop `plank` filter redraw exposed no selectable `typeid="2x4"` fuel row, so there is no credited count selection, confirm-return, post-fuel save/writeback, lighter, or `fd_fire` proof.

Josef's repair direction is to bypass that brittle exact inventory/drop selector seam: spawn an oversized local firewood supply, preferably logs first and planks/`2x4` only if logs fail the intended firewood path; place a broad source-firewood zone over/around it; then prove the normal fire-starting path can see enough real in-world fuel.

## Scope

### In scope

1. Reuse or rerun the already-green normal brazier deployment primitive only as the brazier footing.
2. Create an oversized in-world wood source near the brazier: target shape is thousands of logs or planks so fuel visibility is impossible to miss.
3. Create/verify a broad source-firewood zone over the wood source using the most reliable current primitive. If the exact zone type/key name is uncertain, inspect live game metadata/zone JSON rather than guessing.
4. Prove saved-world footing before ignition: `f_brazier`, real wood items in the expected nearby/source area, and source-firewood zone metadata or visible zone evidence.
5. Run the normal player fire-start/lighter path, not a debug `fd_fire` placement shortcut. Capture the narrow in-game message/log line for the lighting action immediately after targeting/turn advance if one is emitted; it is bridge evidence that the fire came from normal play before save/`fd_fire` closure is attempted, not a substitute for saved-state proof.
6. If cheap to stage, winter/cold exposure plus character-warmth log lines may be used as secondary liveliness evidence that the fire is still active after time passes. Treat warmth messages as adjunct evidence only, because clothing, weather, shelter, distance, and elapsed-time interruptions can all confound them.
7. Prove guarded save/writeback after ignition with saved `fd_fire`/smoke/light state and then continue into the existing player-lit fire -> bandit signal matrix if green.

### Out of scope

- Crediting the old Multidrop filtered-`2x4` path without new proof.
- Closing player-lit fire from debug map-editor `fd_fire` or synthetic loaded-map fields.
- Reopening roof-fire horde proof before real player-lit fire is green.
- Broad zone-manager proof. This packet may use a zone only as a practical firewood source footing; it does not claim Smart Zone Manager closure.

## Success state

- [x] Current runtime is fresh enough for the probe and the run path is named in the report: final changed probe `.userdata/dev-harness/harness_runs/20260429_091438/`.
- [ ] Deployed brazier footing is proven or reused with saved east/nearby `f_brazier` evidence.
- [x] Oversized firewood source is real in saved-world metadata: `.userdata/dev-harness/harness_runs/20260429_091438/` green `audit_saved_source_tile_for_logs` on east-tile `log` items.
- [x] Broad source-firewood zone or equivalent real game fuel-source footing is proven by saved zone metadata: `.userdata/dev-harness/harness_runs/20260429_091438/` green `audit_saved_source_firewood_zone_for_logs` for `SOURCE_FIREWOOD` / `your_followers` over the log source.
- [ ] Normal player fire-start/lighter action is audited with screenshots/OCR or exact metadata after every meaningful action boundary, and any wrong menu/prompt/target/activity mismatch is named instead of pressed through.
- [ ] Normal player fire-start/lighter action reaches the brazier/fuel path without the old `blocked_untrusted_drop_filter_or_inventory_visibility` blocker, with any decisive in-game message/log line captured as narrow action-path bridge evidence.
- [ ] Optional winter/warmth adjunct, if cheap and clean, records character-warmth log lines after bounded time passage as secondary evidence that the fire is still live; this must not become a blocker or replace saved-state proof.
- [ ] Guarded post-ignition save/writeback proves actual `fd_fire` plus smoke/light-relevant state on the expected tile/area.
- [ ] If ignition is green, bounded wait evidence shows the relevant bandit live-signal response or a clearly classified no-response outcome.

## Validation notes


Current reopened audit boundary (2026-04-29): the scenario/fixture stages `fuel_writeback_source_zone_v0_2026-04-29`, preflight-wields a charged `lighter` with nested `butane`, and adds `audit_player_message_log_contains` immediately after targeting/turn advance so only decisive normal-lighting message lines are captured before save closure. The final changed agent probe `.userdata/dev-harness/harness_runs/20260429_091438/` stayed non-green for feature proof: setup/preflight rows were green, but `audit_player_save_mtime_after_source_zone_ignition_save` was red because the saved-player mtime did not advance, the player message log stayed at baseline with no `You successfully light a fire.`-style line, and copied saved-map inspection found no `fd_fire`/`fd_smoke`. Josef suspects a wrong action somewhere in the macro, so the current retry is not another blind run: it must add screenshots/OCR or exact metadata after every meaningful action boundary and stop where the path diverges.

Use the proof-freeze discipline from `doc/c-aol-harness-proof-freeze-matrix-v0-2026-04-28.md`: setup helpers are footing only. Feature proof needs clean startup, green step-local rows, claim-scoped artifacts, guarded save/writeback, and no yellow/blocked wait ledger for the claimed behavior. In-game message/debug log lines are useful bridge evidence for the player action path; extract only the decisive line(s) and pair them with saved-state proof instead of dumping broad logs.

After two materially different attempts hit the same blocker, consult Frau Knackal before attempt 3. After four unresolved attempts, package the manual recipe and evidence boundary for Josef instead of looping.
