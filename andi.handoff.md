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
- `bandit.variable_roster_tiny_dispatch_sizing_live` in `.userdata/dev-harness/harness_runs/20260428_202044/`: scout-confirmed two-member camp commits the buddy pair as stalk pressure (`reserve=0`, `dispatchable=2`, `selected_members=2`), with saved two-active/zero-ready split.
- `bandit.variable_roster_dispatch_sizing_live` in `.userdata/dev-harness/harness_runs/20260428_192059/`: five-member camp keeps reserve two and dispatches a two-member stalk, with saved two-active/three-ready split.
- `bandit.variable_roster_large_cooled_dispatch_sizing_live` in `.userdata/dev-harness/harness_runs/20260428_193621/`: ten-member camp with prior bandit losses keeps reserve five and dispatches a cooled two-member stalk, with saved two-active/eight-ready split.
- `bandit.stalk_pressure_waits_for_opening` in `.userdata/dev-harness/harness_runs/20260428_195617/`: **narrow no-opening decision/decay guardrail only** — active remembered pressure -> no-opening rejection -> stale/decayed lead. Concretely: saved active remembered stalk-pressure lead, green 30-minute cadence, same-run `opening_state=no_opening_after_bounded_stalk_window` / `opening_available=no` rejection, held-pressure notes, no spawned outside group, and saved stale/decayed lead (`last_outcome=no_opening_return_hold_decay`, confidence `2`). This is **not** opening-present escalation proof and not broad product closure.
- `bandit.high_threat_low_reward_holds` in `.userdata/dev-harness/harness_runs/20260428_200600/`: clean current runtime `6206131b31`, green 30-minute wait, same-run high-threat hold rejection with `opening_state=opening_present_or_not_required`, `opening_available=yes`, exact `selected=hold / chill`, and saved no-active-outside/no-member-dispatch state.
- `bandit.active_outside_dogpile_block_live` in `.userdata/dev-harness/harness_runs/20260428_200434/`: clean current runtime `6206131b31`, green 30-minute wait, saved preflight existing active outside stalk pressure (`active_outside=2`, three ready at home), same-run dogpile-block rejection with `hold: unresolved active outside group/contact blocks dogpile`, and saved unchanged two-active/three-ready state.
- `bandit.extortion_first_demand_fight_mcw` in `.userdata/dev-harness/harness_runs/20260428_204454/`: shakedown/toll-control guardrail only — same-run `camp_style` / `open_shakedown` local-gate artifact, first-demand `demanded_toll=15797` pay/fight surface, and distinct fight branch logging.
- `bandit.extortion_first_demand_pay_mcw` in `.userdata/dev-harness/harness_runs/20260428_204630/`: shakedown/toll-control guardrail only — same first-demand surface, pay branch logging, and saved no-active-group/all-14-home writeback.
- `bandit.extortion_reopened_demand_mcw` in `.userdata/dev-harness/harness_runs/20260428_204813/`: shakedown/toll-control guardrail only — reopened defender-loss `demanded_toll=22116` bounded higher demand with pay/fight options.

Current validation:

- `./cataclysm-tiles --version` -> `d70b8ce014` after the shakedown/toll-control commit/rebuild; the three shakedown proof runs themselves were dirty-runtime-at-proof from source `72e67bdcb9`
- `python3 -m py_compile tools/openclaw_harness/startup_harness.py`
- `python3 -m json.tool` on the relevant scenario/fixture files
- `python3 tools/openclaw_harness/proof_classification_unit_test.py`
- `git diff --check`
- `make -j4 TILES=1 cataclysm-tiles LINTJSON=0 ASTYLE=0`
- `./tests/cata_test "[bandit_live_world]"`
- `./tests/cata_test "[bandit][live_world][camp_map]" --success`
- the named harness probes above, including `bandit.high_threat_low_reward_holds`, `bandit.active_outside_dogpile_block_live`, `bandit.extortion_first_demand_fight_mcw`, `bandit.extortion_first_demand_pay_mcw`, and `bandit.extortion_reopened_demand_mcw`

These close their named guardrail slices only. Shakedown/toll-control guardrails are green, dirty-runtime-at-proof is noted, and this is **not** broad product closure. Do **not** inflate no-opening into opening-present escalation or inflate the roster/high-threat/active-outside/shakedown guardrails into broad product closure.

## Next implementation/proof target

Continue the camp-map ecology contract by moving to the next named downstream proof target from canon, likely repeatability/fixture-bias review or optional empty-camp live sanity before broader product closure. Do not treat shakedown/toll control as the next unrun target; it is green as scoped guardrail evidence from `20260428_204454`, `20260428_204630`, and `20260428_204813`.

Do **not** rerun the green tiny, medium/five-member, no-opening, high-threat, active-outside, or shakedown/toll-control proofs unless Schani/Josef/Frau explicitly reopen them. Keep `bandit.stalk_pressure_waits_for_opening` labeled narrowly as no-opening decision/decay only.

## Queued after this lane

`Smart Zone Manager harness-audit retry packet v0` is greenlit/queued, not an interruption to the active bandit camp-map lane unless Schani/Josef explicitly promotes it. Imagination source: `doc/smart-zone-manager-harness-audit-retry-imagination-source-of-truth-2026-04-28.md`; canonical contract: `doc/smart-zone-manager-harness-audit-retry-packet-v0-2026-04-28.md`. This is a bounded proof retry only: repair/stage a loadable fixture if needed, relink/rebuild to current runtime, prove live Zone Manager open -> Smart Zone generation -> generated/reopened layout, and prefer structured saved-zone metadata / coordinates / options / save-reopen state over screenshots/OCR where stronger. Do not close from `smart-zone-audit-live-20260428a` run `20260428_151053`: it is `feature_proof=false` due to runtime mismatch and 25/25 yellow rows, and its only zone evidence is a temp `ZONE_START_POINT`, not generated Smart Zone layout.

`Cannibal camp confidence-push live playtest packet v0` is greenlit/queued after the relevant harness-review boundary, not an interruption to the active bandit camp-map lane unless Schani/Josef explicitly promotes it. Imagination source: `doc/cannibal-camp-confidence-push-live-playtest-imagination-source-of-truth-2026-04-28.md`; canonical contract: `doc/cannibal-camp-confidence-push-live-playtest-packet-v0-2026-04-28.md`. This is confidence uplift for the closed cannibal behavior: wandering day pressure, night mistake/contact, reload brain, different-seed repeat, and bandit contrast control.

`Generic clean-code boundary review packet v0` is greenlit as a report-only boundary review after the active bandit camp-map lane reaches a real checkpoint, not an interruption to current implementation. Canonical contract: `doc/generic-clean-code-boundary-review-packet-v0-2026-04-28.md`; imagination source: `doc/generic-clean-code-boundary-review-imagination-source-of-truth-2026-04-28.md`. First pass should only report canon drift, stale TODOs, obvious build/test/lint hazards, and bounded cleanup risks; classify findings as fix now / queue / ignore-watch; do not edit files or reorder priorities without Schani/Josef review.

`C-AOL live AI performance audit packet v0` is greenlit as a bottom-of-stack performance audit, not an interruption to the active bandit camp-map lane unless Schani/Josef explicitly promotes it. Canonical contract: `doc/c-aol-live-ai-performance-audit-packet-v0-2026-04-28.md`; imagination source: `doc/c-aol-live-ai-performance-imagination-source-of-truth-2026-04-28.md`.
