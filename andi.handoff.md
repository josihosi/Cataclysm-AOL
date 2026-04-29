# Andi handoff: fuel source-zone fire proof after green normal-map entry

## Active target

`Fuel normal-map entry primitive packet v0` is now complete/green under `C-AOL actual playtest verification stack v0`; the active execution slice is `Fuel writeback repair via wood source zone packet v0`, reopened from the green normal-map gate.

Josef greenlit the normal-map primitive on 2026-04-29 after the source-zone fire proof correctly stopped at the normal-map gate instead of typing through stale raw JSON. That primitive is now proven, not fire proof: run `.userdata/dev-harness/harness_runs/20260429_140645/` reached feature-path green with 5/5 green step-local ledger, saved charged wielded `lighter`, saved `f_brazier` + real `log` items, saved `SOURCE_FIREWOOD`, and screenshot `normal_map_entry_gate_before_activation.png` with OCR fallback including `Wield:` and standalone `YOU`. No activation/targeting/fire/lighter action key was sent by the primitive.

Canonical anchors:

- `Plan.md`
- `TODO.md`
- `TESTING.md`
- `SUCCESS.md`
- stack contract: `doc/c-aol-actual-playtest-verification-stack-v0-2026-04-27.md`
- completed normal-map packet: `doc/fuel-normal-map-entry-primitive-packet-v0-2026-04-29.md`
- completed normal-map imagination source: `doc/fuel-normal-map-entry-primitive-imagination-source-of-truth-2026-04-29.md`
- active downstream fuel repair packet: `doc/fuel-writeback-repair-via-wood-source-zone-packet-v0-2026-04-29.md`

## Current work

Resume the downstream source-zone fire proof from the green normal-map entry gate:

1. Start from the green normal-map entry primitive evidence; do not repeat stale raw-JSON/item-info cleanup tricks.
2. Before activation, keep the screenshot/OCR normal-map gate honest; raw JSON/item-info/stale/debug/internal screens are blocker evidence and stop before action keys.
3. Press activation only from normal map UI and require the visible/OCR boundary for `Light where?`.
4. Target east/source only after the prompt is proven, and require the source-firewood confirmation prompt before any unsafe confirmation key.
5. After target/advance, require a real ignition message/OCR and red-block depleted-lighter/no-ignition text before any save/`fd_fire` audit.
6. Only after a real ignition line, prove guarded save mtime and saved `fd_fire` plus `fd_smoke`/smoke-light-relevant state; then continue into bounded bandit signal/no-response proof if green.

## Evidence expectations

Minimum packet for the reopened fire slice:

- command/scenario and run directory;
- green normal-map entry artifact reference (`20260429_140645`) or a fresh equivalent gate;
- screenshot artifact path plus named visible fact for every action boundary;
- OCR/metadata explicitly labeled as fallback if direct image inspection is unavailable;
- `Light where?`, source-firewood confirmation, and ignition/no-ignition message excerpts;
- save mtime plus saved `fd_fire`/`fd_smoke` proof only after a real ignition line;
- blocker screenshot path and narrow OCR excerpt if raw JSON/item-info/stale/debug screen appears.

## Non-goals/cautions

- This primitive is not player-lit fire proof by itself.
- Do not credit debug map-editor `fd_fire`, synthetic loaded-map fields, setup metadata, or stale-window screenshots as fire proof.
- Do not keep pressing through wrong screens to see what happens.
- Do not reopen inventory-selector/wield UI proof; the charged wielded lighter is setup footing only.
- Do not rerun completed performance rows as ritual.
- If two materially different normal-map entry attempts hit the same blocker, consult Frau before attempts 3-4; after four unresolved attempts, package the exact manual recipe/blocker instead of looping.
