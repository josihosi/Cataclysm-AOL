# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Josef reopened one narrow check in the `C-AOL actual playtest verification stack v0`: verify whether the brazier is actually deployed/usable through the player-facing product path. That visible deployed-brazier/source-zone gate is now green; the old fuel/source-zone proof surface remains broken, so do **not** use `fuel_writeback_source_zone_v0_2026-04-29` as a fire/lighter proof surface.

Clean fixture replacement is now available and first-load green:

- fixture: `fuel_source_zone_clean_normal_map_v0_2026-04-29`
- source: captured from `.userdata/dev-harness/harness_runs/20260429_140645/saved_world/McWilliams` after the green normal-map entry run and before any activation/targeting/fire/lighter key
- clean gate probe: `.userdata/dev-harness/harness_runs/20260429_143149/`, scenario `bandit.live_world_nearby_camp_source_zone_clean_normal_map_entry_mcw`
- visible deployed-brazier/source-zone gate: `.userdata/dev-harness/harness_runs/20260429_144805/`, scenario `bandit.live_world_nearby_camp_visible_brazier_source_zone_gate_mcw`, 20/20 green, normal Apply->brazier->Deploy where?->east, look/OCR `hrazıer` + `burn things`, saved `f_brazier` + `log`, saved `SOURCE_FIREWOOD`, no lighter/fire keys
- result: feature-path green, 5/5 green step-local ledger, load-finalized artifact match
- first screenshot: `.userdata/dev-harness/harness_runs/20260429_143149/normal_map_entry_gate_before_activation.png`
- named visible fact: normal gameplay map UI, OCR fallback matched `Wield:` and `YOU`; no raw `pocket_type` / `contents` / `specific energy` guard fired
- setup footing rechecked: saved wielded `lighter` charge 100, saved `f_brazier`, real saved `log`, saved `SOURCE_FIREWOOD`

Canonical anchors:

- Save-transform corruption audit: `doc/fuel-source-zone-save-transform-corruption-audit-v0-2026-04-29.md`.
- Visible deployed-brazier gate: `doc/fuel-visible-brazier-source-zone-gate-v0-2026-04-29.md`.
- Stack contract: `doc/c-aol-actual-playtest-verification-stack-v0-2026-04-27.md`.
- Completed normal-map entry packet: `doc/fuel-normal-map-entry-primitive-packet-v0-2026-04-29.md`.
- Packaged fuel repair packet: `doc/fuel-writeback-repair-via-wood-source-zone-packet-v0-2026-04-29.md`.
- Manual package entry: `/Users/josefhorvath/.openclaw/workspace/runtime/josef-playtest-package.md#2026-04-29--fuel-writeback-source-zone-repair-v0`.

Next narrow work queue:

1. Do **not** rerun fire/lighter proof on `fuel_writeback_source_zone_v0_2026-04-29`.
2. Treat `.userdata/dev-harness/harness_runs/20260429_144805/` as the green visible deployed-brazier/source-zone gate: normal Apply->brazier->Deploy where?->east, look/OCR `hrazıer` + `burn things`, saved `f_brazier` + `log`, saved `SOURCE_FIREWOOD`, no lighter/fire keys.
3. If fuel fire proof continues, use only the visible deployed-brazier gate or a fresher equivalent: start from normal map UI, prove real brazier deployment/visibility, then select/activate the charged lighter through a product path and require `Light where?` before any direction/confirmation keys.
4. If a visibly deployed/usable brazier plus charged lighter still fails to open `Light where?`, stop and classify it as a firestarter/action primitive bug rather than laundering the failure into fuel/fire proof.
5. If a brand-new clean fixture ever opens to raw JSON/item-info soup, stop and classify it as harness/game-code load bug for review; do not poke through it with keys.
6. Keep `Player-lit fire and bandit signal verification packet v0` blocked behind real player-lit fire proof/manual evidence.
7. Keep `Roof-fire horde detection proof packet v0` blocked behind real player-created fire/light/smoke.

Proof discipline:

- The broken-save postmortem is not fire proof; it is to stop repeating the backend manipulation that created the poisoned fixture/UI state.
- OCR/metadata are fallback evidence, not visual inspection, unless the image was directly inspected.
- Do not launder setup metadata, stale screenshots, debug `fd_fire`, synthetic loaded-map fields, or item-info text into product proof.
- If the same broken-save symptom reappears on the green fixture path, stop and classify harness/load or game-code investigation before pressing onward.
