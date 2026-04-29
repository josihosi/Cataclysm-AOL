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

1. Reopen the source-zone fire proof from the green normal-map entry gate; do not repeat stale raw-JSON/item-info cleanup tricks.
2. Guard activation with a screenshot/OCR boundary for `Light where?`; abort if normal map is not visible first.
3. Guard east targeting/source-firewood confirmation before any unsafe confirmation key.
4. Require post-target ignition/no-ignition player-message/OCR before any save or `fd_fire` audit; red-block depleted-lighter/no-ignition text.
5. Only after a real ignition line, prove save mtime plus saved `fd_fire`/`fd_smoke`; then proceed to downstream bandit signal/no-response.

Proof discipline:

- This item is not fire proof by itself.
- OCR/metadata are fallback evidence, not visual inspection, unless the image was directly inspected.
- Do not launder setup metadata, stale screenshots, debug `fd_fire`, synthetic loaded-map fields, or item-info text into product proof.
- If two materially different normal-map entry attempts hit the same blocker, consult Frau before attempts 3-4; after four unresolved attempts, package the exact manual recipe/blocker instead of looping.
