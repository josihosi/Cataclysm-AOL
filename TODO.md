# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active target: `Fuel writeback repair via wood source zone packet v0` is **NEXT / UNBLOCKED FOR REOPENED FIRE PROOF, STILL UNPROVEN** now that `Fuel normal-map entry primitive packet v0` is green.

Normal-map entry evidence: `.userdata/dev-harness/harness_runs/20260429_140645/`, scenario `bandit.live_world_nearby_camp_source_zone_normal_map_entry_mcw`, 5/5 green step-local ledger, feature-path classification, load-finalized artifact match, saved charged wielded `lighter`, saved `f_brazier` + real `log` items, saved `SOURCE_FIREWOOD`, screenshot `normal_map_entry_gate_before_activation.png`, OCR fallback includes `Wield:` and standalone `YOU`. No activation/targeting/fire/lighter action key is sent by that primitive.

Canonical anchors:

- Stack contract: `doc/c-aol-actual-playtest-verification-stack-v0-2026-04-27.md`.
- Completed normal-map entry packet: `doc/fuel-normal-map-entry-primitive-packet-v0-2026-04-29.md`.
- Completed normal-map imagination source: `doc/fuel-normal-map-entry-primitive-imagination-source-of-truth-2026-04-29.md`.
- Active downstream fuel repair packet: `doc/fuel-writeback-repair-via-wood-source-zone-packet-v0-2026-04-29.md`.
- Active downstream fuel imagination source: `doc/fuel-writeback-repair-via-wood-source-zone-imagination-source-of-truth-2026-04-29.md`.

Next narrow work queue:

1. Before fire proof continues, investigate the broken-save incident that produced the stale raw-JSON/item-info startup surface: compare `fuel_writeback_source_zone_v0_2026-04-29` against the green normal-map fixture path and identify the backend save edit, transform, or harness resume step that broke UI state, or state explicitly that the exact culprit is no longer recoverable.
2. Preserve the broken-run evidence (`20260429_093118`, `20260429_093509`, `20260429_095021`, `20260429_122807`, `20260429_122955`) as blocker/postmortem evidence only; do not use those saves as the proof surface.
3. Reopen the source-zone fire proof from the green normal-map entry gate; do not repeat stale raw-JSON/item-info cleanup tricks.
4. Guard activation with a screenshot/OCR boundary for `Light where?`; abort if normal map is not visible first.
5. Guard east targeting/source-firewood confirmation before any unsafe confirmation key.
6. Require post-target ignition/no-ignition player-message/OCR before any save or `fd_fire` audit; red-block depleted-lighter/no-ignition text.
7. Only after a real ignition line, prove save mtime plus saved `fd_fire`/`fd_smoke`; then proceed to downstream bandit signal/no-response.

Proof discipline:

- The broken-save postmortem is not fire proof; it is to stop repeating the backend manipulation that created the poisoned fixture/UI state.
- OCR/metadata are fallback evidence, not visual inspection, unless the image was directly inspected.
- Do not launder setup metadata, stale screenshots, debug `fd_fire`, synthetic loaded-map fields, or item-info text into product proof.
- If the same broken-save symptom reappears on the green fixture path, stop and classify harness/load or game-code investigation before pressing onward.
