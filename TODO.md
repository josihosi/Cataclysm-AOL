# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active target: `Bandit camp-map risk/reward dispatch planning packet v0`.

Current state: the cannibal night-raid live slice is checkpointed green for the named scenarios; the optional bandit-control contrast is non-blocking unless product review explicitly reopens it. The bandit camp-map lane is active; the latest deterministic ecology slice is implemented and validated as code evidence only.

Known footing:

- Product source: `doc/bandit-camp-map-ecology-source-of-truth-2026-04-28.md`.
- Contract: `doc/bandit-camp-map-risk-reward-dispatch-planning-packet-v0-2026-04-28.md`.
- Andi lane: `doc/bandit-camp-map-risk-reward-dispatch-andi-lane-v0-2026-04-28.md`.
- Live matrix: `doc/bandit-live-product-playtest-matrix-v0-2026-04-28.md`.
- Existing deterministic/code support covers a camp-owned `camp_intelligence_map`, serialization/deserialization, active target OMT persistence, scout-return writeback into the source camp map, live signal marks writing camp-map signal leads, legacy scalar migration fallback, remembered camp-map lead selection through the real live dispatch cadence/report path, two-OMT ordinary scout stand-off, half-day ordinary scout return clock in the live aftermath seam, roster/reserve dispatch capacity for 2/4/5/7/10 living-member camps, active-outside dogpile blocking, wounded/unready and killed-member dispatch shrinkage, bounded stockpile-pressure willingness, high-threat/poor-reward non-escalation, prior defender-loss pressure, prior bandit-loss cooling, larger-than-scout stalk sizing when risk/reward justifies it, no-opening hold/return in the camp-map decision seam, and green named feature-path proof for no-opening stalk-pressure, high-threat/low-reward hold, and active-outside dogpile-block guardrails; it is not full product closure.

Next narrow work queue:

1. Move to the next named downstream bandit matrix proof, likely shakedown/toll control regression or repeatability/fixture-bias review before broader product closure.
2. Keep variable-roster product closure honest: 2/5/10 sizing variants, no-opening stalk-pressure, high-threat/low-reward hold, and active-outside dogpile-block are green for their guardrails, but not full product closure.
3. Keep product proof downstream: no live/harness closure until named matrix scenarios observe the live dispatch/writeback bridge and report/log fields.

Latest active-outside boundary: `bandit.active_outside_dogpile_block_live` is green feature-path evidence in `.userdata/dev-harness/harness_runs/20260428_200434/`. It proves current-runtime `./cataclysm-tiles --version = 6206131b31`, clean startup/runtime gate, saved preflight existing active outside stalk pressure (`active_outside=2`, three ready at home), green 30-minute wait ledger, same-run `bandit_live_world dispatch rejected:` with `candidate_reason=remembered_camp_map_lead`, `opening_state=opening_present_or_not_required`, `opening_available=yes`, roster line `living=5 ready_at_home=3 wounded_or_unready=0 active_outside=2 reserve=2 dispatchable=1`, `hold: unresolved active outside group/contact blocks dogpile`, and saved unchanged two-active/three-ready state.

Latest high-threat boundary: `bandit.high_threat_low_reward_holds` is green feature-path evidence in `.userdata/dev-harness/harness_runs/20260428_200600/`. It proves current-runtime `./cataclysm-tiles --version = 6206131b31`, clean startup/runtime gate, green 30-minute wait ledger, same-run `bandit_live_world dispatch rejected:` with `candidate_reason=remembered_camp_map_lead`, `opening_state=opening_present_or_not_required`, `opening_available=yes`, `hold: high threat or poor reward does not escalate by itself`, exact rendered `selected=hold / chill`, and saved no-active-outside/no-member-dispatch state (`active_outside=0`, five ready at home).

Latest no-opening boundary: `bandit.stalk_pressure_waits_for_opening` is green feature-path evidence in `.userdata/dev-harness/harness_runs/20260428_195617/`. It proves only active remembered pressure -> no-opening rejection -> stale/decayed lead: saved preflight active stalk-pressure lead footing, a green 30-minute cadence wait, same-run `bandit_live_world dispatch rejected:` with `candidate_reason=remembered_camp_map_lead`, `signal_packet=none`, `opening_state=no_opening_after_bounded_stalk_window`, `opening_available=no`, held-pressure notes, no active outside group, and saved stale/decayed lead state (`last_outcome=no_opening_return_hold_decay`, confidence `2`). It is not opening-present escalation or broad product closure.

Latest variable-roster boundary: tiny, medium, and large/cooled sizing variants are green feature-path evidence: `bandit.variable_roster_tiny_dispatch_sizing_live` in `.userdata/dev-harness/harness_runs/20260428_202044/` proves a scout-confirmed two-member camp commits the buddy pair as two-member stalk pressure with reserve zero; `bandit.variable_roster_dispatch_sizing_live` in `.userdata/dev-harness/harness_runs/20260428_192059/` proves a five-member camp preserves reserve two and dispatches a two-member stalk; `bandit.variable_roster_large_cooled_dispatch_sizing_live` in `.userdata/dev-harness/harness_runs/20260428_193621/` proves a ten-member camp with prior bandit losses keeps reserve five and dispatches a cooled two-member stalk. Latest vanished-signal boundary: `bandit.camp_map_vanished_signal_redispatch` is green feature-path evidence in `.userdata/dev-harness/harness_runs/20260428_185947/`. Latest sight-avoid boundary: `bandit.scout_stalker_sight_avoid_live` is green feature-path evidence in `.userdata/dev-harness/harness_runs/20260428_173626/`. These close only their named guardrail slices; the tiny buddy-pair update supersedes old `20260428_193433` scout/reserve-one evidence for current tiny expectations, and no-opening remains no-opening decision/decay proof only, not opening-present escalation or broad product closure.

Proof discipline:

- Code changes need `git diff --check`, the narrow touched-object build, and focused `[bandit][live_world]` / `[bandit][live_world][camp_map]` tests before checkpoint.
- Deterministic evidence can close deterministic sub-slices only; live game claims need named harness/product scenarios from the matrix.
- Do not reopen fuel, Smart Zone, or cannibal optional-control seams unless canon or Josef/Schani explicitly does so.
