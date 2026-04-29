# Andi handoff: fuel source-zone fire proof after green normal-map entry

## Active target

`Fuel normal-map entry primitive packet v0` is complete/green under `C-AOL actual playtest verification stack v0`. Josef has now diagnosed the old fuel/source-zone surface as a broken save/fixture and reopened one narrow check: verify whether the brazier is actually deployed/usable through the player-facing product path. Use the clean replacement fixture only, and do not run fire/lighter proof on `fuel_writeback_source_zone_v0_2026-04-29`.

Josef greenlit the normal-map primitive on 2026-04-29 after the source-zone fire proof correctly stopped at the normal-map gate instead of typing through stale raw JSON. The clean replacement fixture is now `fuel_source_zone_clean_normal_map_v0_2026-04-29`, copied from `.userdata/dev-harness/harness_runs/20260429_140645/saved_world/McWilliams` after that green normal-map run and before any activation/targeting/fire/lighter key. Clean proof `.userdata/dev-harness/harness_runs/20260429_143149/` is feature-path green with 5/5 green step-local ledger, saved charged wielded `lighter`, saved `f_brazier` + real `log` items, saved `SOURCE_FIREWOOD`, and screenshot `normal_map_entry_gate_before_activation.png` with OCR fallback including `Wield:` and standalone `YOU`.

Canonical anchors:

- `Plan.md`
- `TODO.md`
- `TESTING.md`
- `SUCCESS.md`
- stack contract: `doc/c-aol-actual-playtest-verification-stack-v0-2026-04-27.md`
- completed normal-map packet: `doc/fuel-normal-map-entry-primitive-packet-v0-2026-04-29.md`
- completed normal-map imagination source: `doc/fuel-normal-map-entry-primitive-imagination-source-of-truth-2026-04-29.md`
- active downstream fuel repair packet: `doc/fuel-writeback-repair-via-wood-source-zone-packet-v0-2026-04-29.md`
- save-transform corruption audit: `doc/fuel-source-zone-save-transform-corruption-audit-v0-2026-04-29.md`

## Current work

No fire/lighter proof should run on the broken old fixture. The horse is fixed only insofar as a clean first-load fixture exists and proves normal map UI; fire proof remains unproven. Josef's current suspicion is that saved/spawned `f_brazier` metadata may not equal the player-facing deployed brazier state the action path expects.

1. Use `fuel_source_zone_clean_normal_map_v0_2026-04-29` for this check, or build a fresher clean fixture that first proves normal map UI.
2. Do **not** point fire scenarios back at `fuel_writeback_source_zone_v0_2026-04-29`.
3. Check whether the brazier is visibly/operationally deployed, not merely present as saved metadata. If it is only spawned/saved, that is not enough.
4. If needed, add a real deploy primitive from normal map UI: use the `a` inventory activation/deploy path and screenshot the deploy/placement menu, or use `*` construction/build menu only if that is the actual product path; then screenshot the final deployed brazier state on the map.
5. If a clean fixture opens to raw JSON/item-info soup, classify it as harness/game-code load bug and stop for review; do not poke through it with keys.
6. Preserve `20260429_093118`, `20260429_093509`, `20260429_095021`, `20260429_122807`, and `20260429_122955` as blocker/postmortem evidence only.
7. Preserve `20260429_142257` as prior action-boundary non-proof: normal map/setup green, but apply-wielded `A` did not produce `Light where?`.
8. Only after the visible/operational deployed-brazier gate is green, require the full guarded fire chain from clean normal map: activation -> `Light where?`; east/source target -> source-firewood confirmation; target/advance -> real ignition message/OCR and no depleted-lighter/no-ignition text; only then save mtime and saved `fd_fire`/`fd_smoke`.

## Evidence expectations

Minimum packet for clean-fixture / any later reopened fire slice:

- command/scenario and run directory;
- clean fixture name/path (`fuel_source_zone_clean_normal_map_v0_2026-04-29`) and first-load artifact reference (`20260429_143149`) or a fresh equivalent clean gate;
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
