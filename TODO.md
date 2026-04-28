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
- Existing deterministic/code support covers a camp-owned `camp_intelligence_map`, serialization/deserialization, active target OMT persistence, scout-return writeback into the source camp map, live signal marks writing camp-map signal leads, legacy scalar migration fallback, remembered camp-map lead selection through the real live dispatch cadence/report path, two-OMT ordinary scout stand-off, half-day ordinary scout return clock in the live aftermath seam, roster/reserve dispatch capacity for 2/4/5/7/10 living-member camps, active-outside dogpile blocking, wounded/unready and killed-member dispatch shrinkage, bounded stockpile-pressure willingness, high-threat/poor-reward non-escalation, prior defender-loss pressure, prior bandit-loss cooling, larger-than-scout stalk sizing when risk/reward justifies it, no-opening hold/return in the camp-map decision seam, and green named feature-path proof for the no-opening stalk-pressure guardrail; it is not full product closure.

Next narrow work queue:

1. Prove live high-threat/low-reward and active-outside dogpile guardrails as separate named scenarios.
2. Keep variable-roster product closure honest: 2/5/10 sizing variants and no-opening stalk-pressure are green for their guardrails, but not full row/product closure.
3. Keep product proof downstream: no live/harness closure until named matrix scenarios observe the live dispatch/writeback bridge and report/log fields.

Latest no-opening boundary: `bandit.stalk_pressure_waits_for_opening` is green feature-path evidence in `.userdata/dev-harness/harness_runs/20260428_195617/`. It proves saved preflight active stalk-pressure lead footing, a green 30-minute cadence wait, same-run `bandit_live_world dispatch rejected:` with `candidate_reason=remembered_camp_map_lead`, `signal_packet=none`, `opening_state=no_opening_after_bounded_stalk_window`, `opening_available=no`, held-pressure notes, no active outside group, and saved stale/decayed lead state (`last_outcome=no_opening_return_hold_decay`, confidence `2`).

Latest variable-roster boundary: tiny, medium, and large/cooled sizing variants are green feature-path evidence: `bandit.variable_roster_tiny_dispatch_sizing_live` in `.userdata/dev-harness/harness_runs/20260428_193433/` proves a two-member camp preserves one reserve and dispatches a one-member scout instead of an impossible stalk pair; `bandit.variable_roster_dispatch_sizing_live` in `.userdata/dev-harness/harness_runs/20260428_192059/` proves a five-member camp preserves reserve two and dispatches a two-member stalk; `bandit.variable_roster_large_cooled_dispatch_sizing_live` in `.userdata/dev-harness/harness_runs/20260428_193621/` proves a ten-member camp with prior bandit losses keeps reserve five and dispatches a cooled two-member stalk. Latest vanished-signal boundary: `bandit.camp_map_vanished_signal_redispatch` is green feature-path evidence in `.userdata/dev-harness/harness_runs/20260428_185947/`. Latest sight-avoid boundary: `bandit.scout_stalker_sight_avoid_live` is green feature-path evidence in `.userdata/dev-harness/harness_runs/20260428_173626/`. These close only their named guardrail slices; live high-threat/active-outside variable-roster guardrails remain open.

Proof discipline:

- Code changes need `git diff --check`, the narrow touched-object build, and focused `[bandit][live_world]` / `[bandit][live_world][camp_map]` tests before checkpoint.
- Deterministic evidence can close deterministic sub-slices only; live game claims need named harness/product scenarios from the matrix.
- Do not reopen fuel, Smart Zone, or cannibal optional-control seams unless canon or Josef/Schani explicitly does so.
