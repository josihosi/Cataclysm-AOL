# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active target: `Bandit camp-map risk/reward dispatch planning packet v0`.

Current state: the cannibal night-raid live slice is checkpointed green for the named scenarios; the optional bandit-control contrast is non-blocking unless product review explicitly reopens it. The bandit camp-map lane is now the next unblocked implementation/proof target.

Known footing:

- Product source: `doc/bandit-camp-map-ecology-source-of-truth-2026-04-28.md`.
- Contract: `doc/bandit-camp-map-risk-reward-dispatch-planning-packet-v0-2026-04-28.md`.
- Andi lane: `doc/bandit-camp-map-risk-reward-dispatch-andi-lane-v0-2026-04-28.md`.
- Live matrix: `doc/bandit-live-product-playtest-matrix-v0-2026-04-28.md`.
- Existing deterministic/code support already covers a camp-owned `camp_intelligence_map`, serialization/deserialization, active target OMT persistence, scout-return writeback into the source camp map, and legacy scalar migration fallback; it is not live product proof.

Next narrow work queue:

1. Inspect the current `bandit_live_world` hooks before coding: site roster/member state, dispatch cadence, scout-return writeback, local sight-avoidance, serialization, and reports/logs.
2. Extend deterministic proof for the next camp-map ecology slice: two-OMT ordinary scout stand-off, half-day scout-watch timeout/return, roster/reserve sizing for 2/4/5/7/10 member camps, active-outside dogpile blocking, and high-threat non-escalation.
3. Keep product proof downstream: no live/harness closure until the deterministic seams and live dispatch/writeback bridge are explicit.

Proof discipline:

- Code changes need `git diff --check`, the narrow touched-object build, and focused `[bandit][live_world]` / `[bandit][live_world][camp_map]` tests before checkpoint.
- Deterministic evidence can close deterministic sub-slices only; live game claims need named harness/product scenarios from the matrix.
- Do not reopen fuel, Smart Zone, or cannibal optional-control seams unless canon or Josef/Schani explicitly does so.
