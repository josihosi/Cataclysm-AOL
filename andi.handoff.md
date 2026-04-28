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

Deterministic/code support now covers a camp-owned `camp_intelligence_map`, normal serialize/deserialize support, active target OMT persistence, scout-return writeback into the source camp map, live signal marks writing camp-map signal leads, legacy scalar remembered-memory migration only as fallback, remembered camp-map lead selection through the real live dispatch cadence/report path, two-OMT ordinary scout stand-off, half-day ordinary scout timeout/return through the live aftermath seam, roster/reserve dispatch capacity for 2/4/5/7/10 living-member camps, active-outside dogpile blocking, wounded/unready and killed-member dispatch shrinkage, bounded stockpile-pressure willingness, high-threat/poor-reward non-escalation, prior defender-loss pressure, prior bandit-loss cooling, larger-than-scout stalk sizing when risk/reward justifies it, and no-opening hold/return in the camp-map decision seam.

Green feature-path evidence now exists for these named guardrails:

- `bandit.scout_stalker_sight_avoid_live` in `.userdata/dev-harness/harness_runs/20260428_173626/`: saved active scout/member footing, saved turn delta >= 2, same-run `local_gate ... live_existing_active_group=yes`, and claim-scoped `sight_avoid: exposed -> repositioned ... distance=1`.
- `bandit.camp_map_vanished_signal_redispatch` in `.userdata/dev-harness/harness_runs/20260428_185947/`: saved remembered lead, green bounded 30-minute cadence wait, same-run no-live-signal remembered-lead stalk plan, local-gate `active_job=stalk`, and saved active stalk state with four concrete members.
- `bandit.variable_roster_tiny_dispatch_sizing_live` in `.userdata/dev-harness/harness_runs/20260428_193433/`: two-member camp keeps reserve one and dispatches one scout, with saved one-active/one-ready split.
- `bandit.variable_roster_dispatch_sizing_live` in `.userdata/dev-harness/harness_runs/20260428_192059/`: five-member camp keeps reserve two and dispatches a two-member stalk, with saved two-active/three-ready split.
- `bandit.variable_roster_large_cooled_dispatch_sizing_live` in `.userdata/dev-harness/harness_runs/20260428_193621/`: ten-member camp with prior bandit losses keeps reserve five and dispatches a cooled two-member stalk, with saved two-active/eight-ready split.

Current validation:

- `python3 -m py_compile tools/openclaw_harness/startup_harness.py`
- `python3 -m json.tool` on the relevant variable-roster scenario/fixture files
- `python3 tools/openclaw_harness/proof_classification_unit_test.py`
- `git diff --check`
- `make -j4 tests LINTJSON=0 ASTYLE=0`
- `./tests/cata_test "[bandit][live_world][camp_map]" --success`
- the three variable-roster harness probes named above

These close their named guardrail slices only. Do **not** inflate them into broader product closure: stalk/no-opening return still needs live proof, and the variable-roster matrix row still needs live high-threat/low-reward plus active-outside dogpile guardrails if Schani wants that row fully closed.

## Next implementation/proof target

Continue the camp-map ecology contract by proving stalk/pressure wait-for-opening and no-opening return through the real path before attempting product closure.

Do **not** rerun the green medium/five-member case. Do **not** rerun sight-avoid, vanished-signal, tiny-roster, or large/cooled-roster proofs unless Schani/Josef/Frau explicitly reopen them. Follow `Plan.md` / `TODO.md` / `TESTING.md` toward stalk/no-opening return next, while keeping remaining variable-roster guardrails explicit.

## Queued after this lane

`Smart Zone Manager harness-audit retry packet v0` is greenlit/queued, not an interruption to the active bandit camp-map lane unless Schani/Josef explicitly promotes it. Imagination source: `doc/smart-zone-manager-harness-audit-retry-imagination-source-of-truth-2026-04-28.md`; canonical contract: `doc/smart-zone-manager-harness-audit-retry-packet-v0-2026-04-28.md`. This is a bounded proof retry only: repair/stage a loadable fixture if needed, relink/rebuild to current runtime, prove live Zone Manager open -> Smart Zone generation -> generated/reopened layout, and prefer structured saved-zone metadata / coordinates / options / save-reopen state over screenshots/OCR where stronger. Do not close from `smart-zone-audit-live-20260428a` run `20260428_151053`: it is `feature_proof=false` due to runtime mismatch and 25/25 yellow rows, and its only zone evidence is a temp `ZONE_START_POINT`, not generated Smart Zone layout.

`Cannibal camp confidence-push live playtest packet v0` is greenlit/queued after the relevant harness-review boundary, not an interruption to the active bandit camp-map lane unless Schani/Josef explicitly promotes it. Imagination source: `doc/cannibal-camp-confidence-push-live-playtest-imagination-source-of-truth-2026-04-28.md`; canonical contract: `doc/cannibal-camp-confidence-push-live-playtest-packet-v0-2026-04-28.md`. This is confidence uplift for the closed cannibal behavior: wandering day pressure, night mistake/contact, reload brain, different-seed repeat, and bandit contrast control.

`Generic clean-code boundary review packet v0` is greenlit as a report-only boundary review after the active bandit camp-map lane reaches a real checkpoint, not an interruption to current implementation. Canonical contract: `doc/generic-clean-code-boundary-review-packet-v0-2026-04-28.md`; imagination source: `doc/generic-clean-code-boundary-review-imagination-source-of-truth-2026-04-28.md`. First pass should only report canon drift, stale TODOs, obvious build/test/lint hazards, and bounded cleanup risks; classify findings as fix now / queue / ignore-watch; do not edit files or reorder priorities without Schani/Josef review.

`C-AOL live AI performance audit packet v0` is greenlit as a bottom-of-stack performance audit, not an interruption to the active bandit camp-map lane unless Schani/Josef explicitly promotes it. Canonical contract: `doc/c-aol-live-ai-performance-audit-packet-v0-2026-04-28.md`; imagination source: `doc/c-aol-live-ai-performance-imagination-source-of-truth-2026-04-28.md`.
