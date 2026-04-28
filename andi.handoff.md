# Andi handoff: Bandit camp-map risk/reward dispatch planning packet v0

## Active target

`Bandit camp-map risk/reward dispatch planning packet v0` is active. The cannibal night-raid live slice is checkpointed green for the required scenarios; its optional bandit-control contrast is non-blocking unless product review explicitly reopens it.

Canonical anchors:

- `Plan.md`
- `TODO.md`
- `TESTING.md`
- `SUCCESS.md`
- `doc/bandit-camp-map-ecology-source-of-truth-2026-04-28.md`
- `doc/bandit-camp-map-ecology-implementation-map-2026-04-28.md`
- `doc/bandit-camp-map-risk-reward-dispatch-planning-packet-v0-2026-04-28.md`
- `doc/bandit-camp-map-risk-reward-dispatch-andi-lane-v0-2026-04-28.md`
- `doc/bandit-live-product-playtest-matrix-v0-2026-04-28.md`

## Current evidence boundary

Deterministic/code support now covers a camp-owned `camp_intelligence_map`, normal serialize/deserialize support, active target OMT persistence, scout-return writeback into the source camp map, legacy scalar remembered-memory migration only as fallback, two-OMT ordinary scout stand-off, half-day ordinary scout timeout/return through the live aftermath seam, roster/reserve dispatch capacity for 2/4/5/7/10 living-member camps, active-outside dogpile blocking, wounded/unready and killed-member dispatch shrinkage, bounded stockpile-pressure willingness, high-threat/poor-reward non-escalation, prior defender-loss pressure, prior bandit-loss cooling, larger-than-scout stalk sizing when risk/reward justifies it, and no-opening hold/return in the camp-map decision seam.

Prior validation:

- `git diff --check`
- `make -j4 obj/bandit_live_world.o obj/do_turn.o tests/bandit_live_world_test.o tests LINTJSON=0 ASTYLE=0`
- `./tests/cata_test "[bandit][live_world][camp_map]" --success`
- `./tests/cata_test "[bandit][live_world]"`

This is deterministic/code evidence only, not live product proof.

## Next implementation/proof target

Continue the camp-map ecology contract by wiring the deterministic camp-map risk/reward decision seam through the live cadence/reporting path before attempting product closure:

- vanished-signal redispatch from remembered scout-confirmed leads;
- variable-roster sizing through the real live path, not only the helper;
- stalk/pressure wait-for-opening and no-opening return through the real path;
- sight-avoid reposition/abort within at most two visible local turns without teleporting;
- reviewer-readable reports/logs that show lead source, reward/risk inputs, intent, member count, reserve, opening state, and live-signal-vs-remembered-map driver.

## Queued after this lane

`Smart Zone Manager harness-audit retry packet v0` is greenlit/queued, not an interruption to the active bandit camp-map lane unless Schani/Josef explicitly promotes it. Imagination source: `doc/smart-zone-manager-harness-audit-retry-imagination-source-of-truth-2026-04-28.md`; canonical contract: `doc/smart-zone-manager-harness-audit-retry-packet-v0-2026-04-28.md`. This is a bounded proof retry only: repair/stage a loadable fixture if needed, relink/rebuild to current runtime, prove live Zone Manager open -> Smart Zone generation -> generated/reopened layout, and prefer structured saved-zone metadata / coordinates / options / save-reopen state over screenshots/OCR where stronger. Do not close from `smart-zone-audit-live-20260428a` run `20260428_151053`: it is `feature_proof=false` due to runtime mismatch and 25/25 yellow rows, and its only zone evidence is a temp `ZONE_START_POINT`, not generated Smart Zone layout.

`Cannibal camp confidence-push live playtest packet v0` is greenlit/queued after the relevant harness-review boundary, not an interruption to the active bandit camp-map lane unless Schani/Josef explicitly promotes it. Imagination source: `doc/cannibal-camp-confidence-push-live-playtest-imagination-source-of-truth-2026-04-28.md`; canonical contract: `doc/cannibal-camp-confidence-push-live-playtest-packet-v0-2026-04-28.md`. This is confidence uplift for the closed cannibal behavior: wandering day pressure, night mistake/contact, reload brain, different-seed repeat, and bandit contrast control.

`C-AOL live AI performance audit packet v0` is greenlit as a bottom-of-stack performance audit, not an interruption to the active bandit camp-map lane unless Schani/Josef explicitly promotes it. Canonical contract: `doc/c-aol-live-ai-performance-audit-packet-v0-2026-04-28.md`; imagination source: `doc/c-aol-live-ai-performance-imagination-source-of-truth-2026-04-28.md`.
