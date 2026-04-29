# Fuel normal-map entry primitive packet v0 — 2026-04-29

## Status

COMPLETE / GREEN under `C-AOL actual playtest verification stack v0`.

Imagination source: `doc/fuel-normal-map-entry-primitive-imagination-source-of-truth-2026-04-29.md`.

## Contract

Build and prove the smallest reusable primitive that gets the fuel/source-zone fixture from startup/load into a trusted normal gameplay map screen before any fire/lighter action key is sent.

This is not fire proof. It is the eye/keyboard gate that makes later fire proof possible.

## Scope

- Start from the existing source-zone fire footing where possible: saved `f_brazier`, real saved-world logs, saved `SOURCE_FIREWOOD`, and charged wielded lighter.
- Add or repair a dedicated normal-map entry probe/step that captures the first live screen after load without blind `Escape`, `Return`, or other cleanup keys.
- Record a screenshot artifact path plus the named visible fact that proves normal map/gameplay UI.
- Red-block stale item-info/raw-JSON/debug/internal screens before activation/targeting keys.
- If the fixture consistently opens into stale JSON/item-info, rebuild or resave the fixture from normal gameplay view instead of continuing the fire macro.
- Once normal-map entry is green, hand off to the existing source-zone fire proof guards: `Light where?`, source-firewood confirmation, post-target ignition message/OCR, save mtime, and saved `fd_fire`/`fd_smoke`.

## Non-goals

- Do not claim player-lit fire, smoke, light, or bandit signal from this primitive alone.
- Do not use debug map-editor `fd_fire` or synthetic loaded-map fields as closure.
- Do not treat OCR from raw JSON/item-info as gameplay/menu proof.
- Do not press through wrong screens to “see what happens.”
- Do not reopen inventory-selector/wield UI proof; the charged wielded lighter is setup footing only.

## Success state

- [x] A named run/probe proves exact setup footing or names the missing setup precondition.
- [x] The first live post-load proof gate captures actual normal gameplay map UI, with screenshot artifact path and named visible fact.
- [x] Raw JSON/item-info/stale/debug screens are explicit blocker outcomes and stop before action keys.
- [x] The normal-map gate is reusable by the source-zone fire scenario and documented in the step ledger.
- [x] Fixture rebuild/resave was not needed for this primitive; the fixture transform repair and guarded probe now load to normal-map UI.
- [x] Downstream fire proof is eligible only after the normal-map gate is green, and still requires visible fire/no-fire screenshot, ignition message/OCR, save mtime, and saved `fd_fire`/`fd_smoke`.

## Testing / evidence

Required evidence packet:

- command/scenario name and run directory;
- setup metadata excerpt for `f_brazier`, logs/source-firewood zone, and wielded charged lighter, or a clear setup blocker;
- screenshot artifact path for the normal-map entry gate;
- named visible fact proving normal gameplay UI;
- OCR/metadata only as fallback if direct image inspection is unavailable, explicitly labeled as fallback;
- red/blocker screenshot path and OCR excerpt if the screen is stale item-info/raw JSON;
- no fire/lighter action keys sent until the normal-map gate is green.

If green, continue into `Fuel writeback repair via wood source zone packet v0`; if red, report the exact entry blocker instead of rerunning the fire macro.

## Green evidence — 2026-04-29

Run: `.userdata/dev-harness/harness_runs/20260429_140645/`
Command: `python3 tools/openclaw_harness/startup_harness.py probe bandit.live_world_nearby_camp_source_zone_normal_map_entry_mcw`

Result: feature-path green, `verdict=artifacts_matched`, 5/5 green step-local ledger, matched artifact `game::load: Finalizing end`.

Setup footing:

- `audit_saved_player_has_wielded_charged_lighter.metadata.json`: required/observed weapon `lighter`, observed remaining charge `100`.
- `audit_saved_target_tile_for_brazier_logs.metadata.json`: required/observed `f_brazier`, required `log`, observed real saved-world `log` items.
- `audit_saved_source_firewood_zone_for_logs.metadata.json`: `SOURCE_FIREWOOD`, faction `your_followers`, `OpenClaw source firewood proof zone`, covering required points `[3368, 994, 0]` and `[3369, 994, 0]`.

Screen gate:

- screenshot: `.userdata/dev-harness/harness_runs/20260429_140645/normal_map_entry_gate_before_activation.png`
- OCR fallback: `.userdata/dev-harness/harness_runs/20260429_140645/normal_map_entry_gate_before_activation.screen_text.json` includes `Wield:` plus standalone `YOU` and ordinary gameplay/message text; no raw `pocket_type` / `contents` / `specific energy` guard fired.

Caveat: direct model image inspection was unavailable in this environment, so the visible-fact claim is screenshot path plus OCR fallback, not independent image-model inspection. This primitive does not prove player-lit fire, save mtime, `fd_fire`, smoke, light, or bandit signal response.
