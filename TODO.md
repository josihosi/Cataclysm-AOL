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
- Existing deterministic/code support covers a camp-owned `camp_intelligence_map`, serialization/deserialization, active target OMT persistence, scout-return writeback into the source camp map, live signal marks writing camp-map signal leads, legacy scalar migration fallback, remembered camp-map lead selection through the real live dispatch cadence/report path, two-OMT ordinary scout stand-off, half-day ordinary scout return clock in the live aftermath seam, roster/reserve dispatch capacity for 2/4/5/7/10 living-member camps, active-outside dogpile blocking, wounded/unready and killed-member dispatch shrinkage, bounded stockpile-pressure willingness, high-threat/poor-reward non-escalation, prior defender-loss pressure, prior bandit-loss cooling, larger-than-scout stalk sizing when risk/reward justifies it, and no-opening hold/return in the camp-map decision seam; it is not live product proof.

Next narrow work queue:

1. Extend variable-roster live proof beyond the green five-member/medium case to tiny and large/large-cooled variants, keeping fixture footing and saved post-dispatch roster state explicit.
2. Prove stalk/pressure wait-for-opening/no-opening return through a named live/product scenario.
3. Keep product proof downstream: no live/harness closure until named matrix scenarios observe the live dispatch/writeback bridge and report/log fields.

Latest variable-roster boundary: `bandit.variable_roster_dispatch_sizing_live` is green medium-roster feature-path evidence in `.userdata/dev-harness/harness_runs/20260428_192059/`: saved preflight proves a fixture-shaped five-member camp with a persisted scout-confirmed remembered lead, the long-wait ledger is green for a bounded 30-minute cadence action, same-run logs show `candidate_reason=remembered_camp_map_lead`, `signal_packet=none`, `living=5`, `ready_at_home=5`, `wounded_or_unready=0`, `active_outside=0`, `reserve=2`, `dispatchable=3`, and `members=2`, and saved state persists an active `stalk` dispatch with two concrete active members plus three ready at home. Latest vanished-signal boundary: `bandit.camp_map_vanished_signal_redispatch` is green feature-path evidence in `.userdata/dev-harness/harness_runs/20260428_185947/`. Latest sight-avoid boundary: `bandit.scout_stalker_sight_avoid_live` is green feature-path evidence in `.userdata/dev-harness/harness_runs/20260428_173626/`. These close only their named guardrail slices; tiny/large variable-roster variants and stalk/no-opening live proof remain open.

Proof discipline:

- Code changes need `git diff --check`, the narrow touched-object build, and focused `[bandit][live_world]` / `[bandit][live_world][camp_map]` tests before checkpoint.
- Deterministic evidence can close deterministic sub-slices only; live game claims need named harness/product scenarios from the matrix.
- Do not reopen fuel, Smart Zone, or cannibal optional-control seams unless canon or Josef/Schani explicitly does so.
