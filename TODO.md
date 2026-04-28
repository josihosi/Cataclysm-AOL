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

1. Prove the remaining behavior through named live/product scenarios: stalk/pressure wait-for-opening/no-opening return and variable-roster sizing through the real path.
2. Keep product proof downstream: no live/harness closure until named matrix scenarios observe the live dispatch/writeback bridge and report/log fields.

Latest vanished-signal boundary: `bandit.camp_map_vanished_signal_redispatch` is now green feature-path evidence in `.userdata/dev-harness/harness_runs/20260428_185947/`: saved preflight proves a persisted scout-confirmed remembered lead, the long-wait ledger is green for bounded 30-minute menu/cadence action plus claim-scoped post-wait artifact delta, same-run logs show `candidate_reason=remembered_camp_map_lead`, `signal_packet=none`, `reward=13`, `risk=1`, `margin=12`, `members=4`, `reserve=5`, `dispatchable=8`, and saved state persists an active `stalk` dispatch with four concrete active members. Latest sight-avoid boundary: `bandit.scout_stalker_sight_avoid_live` is green feature-path evidence in `.userdata/dev-harness/harness_runs/20260428_173626/`. These close those matrix guardrails only; stalk/no-opening and variable-roster live scenarios remain open.

Proof discipline:

- Code changes need `git diff --check`, the narrow touched-object build, and focused `[bandit][live_world]` / `[bandit][live_world][camp_map]` tests before checkpoint.
- Deterministic evidence can close deterministic sub-slices only; live game claims need named harness/product scenarios from the matrix.
- Do not reopen fuel, Smart Zone, or cannibal optional-control seams unless canon or Josef/Schani explicitly does so.
