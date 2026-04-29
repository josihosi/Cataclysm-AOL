# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

No current unblocked fire/lighter proof remains in the `C-AOL actual playtest verification stack v0` until review/reopen. Josef explicitly diagnosed the old fuel/source-zone proof surface as a broken save/fixture, so do **not** use `fuel_writeback_source_zone_v0_2026-04-29` as a fire/lighter proof surface.

Clean fixture replacement is now available and first-load green:

- fixture: `fuel_source_zone_clean_normal_map_v0_2026-04-29`
- source: captured from `.userdata/dev-harness/harness_runs/20260429_140645/saved_world/McWilliams` after the green normal-map entry run and before any activation/targeting/fire/lighter key
- clean gate probe: `.userdata/dev-harness/harness_runs/20260429_143149/`, scenario `bandit.live_world_nearby_camp_source_zone_clean_normal_map_entry_mcw`
- result: feature-path green, 5/5 green step-local ledger, load-finalized artifact match
- first screenshot: `.userdata/dev-harness/harness_runs/20260429_143149/normal_map_entry_gate_before_activation.png`
- named visible fact: normal gameplay map UI, OCR fallback matched `Wield:` and `YOU`; no raw `pocket_type` / `contents` / `specific energy` guard fired
- setup footing rechecked: saved wielded `lighter` charge 100, saved `f_brazier`, real saved `log`, saved `SOURCE_FIREWOOD`

Canonical anchors:

- Stack contract: `doc/c-aol-actual-playtest-verification-stack-v0-2026-04-27.md`.
- Completed normal-map entry packet: `doc/fuel-normal-map-entry-primitive-packet-v0-2026-04-29.md`.
- Packaged fuel repair packet: `doc/fuel-writeback-repair-via-wood-source-zone-packet-v0-2026-04-29.md`.
- Manual package entry: `/Users/josefhorvath/.openclaw/workspace/runtime/josef-playtest-package.md#2026-04-29--fuel-writeback-source-zone-repair-v0`.

Next narrow work queue:

1. Do **not** rerun fire/lighter proof on `fuel_writeback_source_zone_v0_2026-04-29`.
2. If Schani/Josef/Frau reopen fuel fire proof, use only the clean fixture `fuel_source_zone_clean_normal_map_v0_2026-04-29` or a fresher clean fixture that first proves normal map UI.
3. If a brand-new clean fixture ever opens to raw JSON/item-info soup, stop and classify it as harness/game-code load bug for review; do not poke through it with keys.
4. Keep `Player-lit fire and bandit signal verification packet v0` blocked behind real player-lit fire proof/manual evidence.
5. Keep `Roof-fire horde detection proof packet v0` blocked behind real player-created fire/light/smoke.

Proof discipline:

- The broken-save postmortem is not fire proof; it is to stop repeating the backend manipulation that created the poisoned fixture/UI state.
- OCR/metadata are fallback evidence, not visual inspection, unless the image was directly inspected.
- Do not launder setup metadata, stale screenshots, debug `fd_fire`, synthetic loaded-map fields, or item-info text into product proof.
- If the same broken-save symptom reappears on the green fixture path, stop and classify harness/load or game-code investigation before pressing onward.
