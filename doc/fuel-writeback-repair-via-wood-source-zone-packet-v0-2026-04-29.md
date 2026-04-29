# Fuel writeback repair via wood source zone packet v0 (2026-04-29)

## Status

IMPLEMENTED-BUT-UNPROVEN / JOSEF PLAYTEST PACKAGE under `C-AOL actual playtest verification stack v0`.

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

- [x] Current runtime/probe path is named in the report: final post-Frau attempt `.userdata/dev-harness/harness_runs/20260429_093509/`; previous setup-green attempt `.userdata/dev-harness/harness_runs/20260429_093118/`.
- [x] Deployed brazier footing is present in saved metadata for the pre-wielded fixture path: `.userdata/dev-harness/harness_runs/20260429_093118/` green `audit_saved_target_tile_for_brazier_logs` includes saved `f_brazier`.
- [x] Oversized firewood source is real in saved-world metadata: `.userdata/dev-harness/harness_runs/20260429_093118/` green `audit_saved_target_tile_for_brazier_logs` includes saved `log` items.
- [x] Broad source-firewood zone or equivalent real game fuel-source footing is proven by saved zone metadata: `.userdata/dev-harness/harness_runs/20260429_093118/` green `audit_saved_source_firewood_zone_for_logs` for `SOURCE_FIREWOOD` / `your_followers`.
- [x] Pre-wielded charged lighter setup is proven only as setup state: `.userdata/dev-harness/harness_runs/20260429_093118/` green `audit_saved_player_has_wielded_charged_lighter` shows `required_weapon=lighter` and nested butane charge; this does **not** prove inventory selector/wield UI.
- [x] Depleted/no-ignition boundary is named instead of laundered into save/writeback: `.userdata/dev-harness/harness_runs/20260429_090634/` is blocked, not proof, because the wait/save path followed a failed or depleted-lighter action.
- [x] Wrong-screen boundary is named instead of pressed through: attempt 3/B typed through a stale item-info/raw-JSON screen; attempt 4 `.userdata/dev-harness/harness_runs/20260429_093509/` added guarded `escape` and aborted when OCR still showed `pocket_type`/`contents`/`specific energy` instead of normal map UI.
- [x] Corrected proof shape is encoded in the scenario: after targeting/advance, capture player-message/OCR, red-block depleted-lighter/no-ignition text such as `lighter has 0 charges, but needs 1`, and proceed to save/`fd_fire` audit only on a real ignition line such as `You successfully light a fire.`
- [ ] Normal player fire-start/lighter action reaches the brazier/fuel path from normal map UI, with decisive in-game message/log line captured as narrow action-path bridge evidence.
- [ ] Guarded post-ignition save/writeback proves actual `fd_fire` plus smoke/light-relevant state on the expected tile/area.
- [ ] If ignition is green, bounded wait evidence shows the relevant bandit live-signal response or a clearly classified no-response outcome.

## Validation notes


Packaged audit boundary (2026-04-29): the scenario/fixture stages `fuel_writeback_source_zone_v0_2026-04-29`, preflight-wields a charged `lighter` with nested `butane`, adds saved `f_brazier`/log/source-zone footing, and keeps `fd_fire` out of setup. Frau's 09:13 correction classifies `.userdata/dev-harness/harness_runs/20260429_090634/` as blocked, not proof: it proved log/source-zone footing but spent wait/save after failed/depleted lighter ignition, so mtime/`fd_fire` writeback could not close. Attempt 3/B `.userdata/dev-harness/harness_runs/20260429_093118/` stayed non-green for feature proof: setup/preflight rows were green, but the loaded UI was a stale item-info/raw-JSON screen (`pocket_type`, `contents`, `specific energy` OCR), `audit_player_save_mtime_after_source_zone_ignition_save` was red because saved-player mtime did not advance, and copied saved-map inspection found no `fd_fire`/`fd_smoke`. Attempt 4 `.userdata/dev-harness/harness_runs/20260429_093509/` added a guarded `escape` and aborted when the stale JSON/info screen remained. The scenario now encodes the decisive next guard: post-target/advance player-message/OCR must show real ignition and must red-block depleted-lighter/no-ignition text before any save/`fd_fire` audit. The agent-side attempt budget is now exhausted; the manual recipe and exact close condition live in `/Users/josefhorvath/.openclaw/workspace/runtime/josef-playtest-package.md#2026-04-29--fuel-writeback-source-zone-repair-v0`.

Use the proof-freeze discipline from `doc/c-aol-harness-proof-freeze-matrix-v0-2026-04-28.md`: setup helpers are footing only. Feature proof needs clean startup, green step-local rows, claim-scoped artifacts, guarded save/writeback, and no yellow/blocked wait ledger for the claimed behavior. In-game message/debug log lines are useful bridge evidence for the player action path; extract only the decisive line(s) and pair them with saved-state proof instead of dumping broad logs.

After two materially different attempts hit the same blocker, consult Frau Knackal before attempt 3. After four unresolved attempts, package the manual recipe and evidence boundary for Josef instead of looping.
