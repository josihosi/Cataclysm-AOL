# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active target: `Bandit camp-map risk/reward dispatch planning packet v0`.

Current state: the cannibal night-raid live slice is checkpointed green for the named scenarios; the optional bandit-control contrast is non-blocking unless product review explicitly reopens it. The bandit camp-map lane is active, with another deterministic ecology slice implemented and ready for review.

Known footing:

- Product source: `doc/bandit-camp-map-ecology-source-of-truth-2026-04-28.md`.
- Contract: `doc/bandit-camp-map-risk-reward-dispatch-planning-packet-v0-2026-04-28.md`.
- Andi lane: `doc/bandit-camp-map-risk-reward-dispatch-andi-lane-v0-2026-04-28.md`.
- Live matrix: `doc/bandit-live-product-playtest-matrix-v0-2026-04-28.md`.
- Existing deterministic/code support covers a camp-owned `camp_intelligence_map`, serialization/deserialization, active target OMT persistence, scout-return writeback into the source camp map, legacy scalar migration fallback, two-OMT ordinary scout stand-off, half-day ordinary scout return clock in the live aftermath seam, roster/reserve dispatch capacity for 2/4/5/7/10 living-member camps, and active-outside dogpile blocking; it is not live product proof.

Next narrow work queue:

1. Extend deterministic proof for the next remaining camp-map ecology slice: wounded/killed-member shrinkage, high-threat non-escalation, prior outcome pressure/cooling, and stockpile/need pressure without breaking reserve/risk gates.
2. Add deterministic coverage for larger stalk/pressure follow-up, no-opening return/hold behavior, and sight-avoid reposition/abort within at most two visible local turns without teleporting.
3. Keep product proof downstream: no live/harness closure until the deterministic seams and live dispatch/writeback bridge are explicit.

Proof discipline:

- Code changes need `git diff --check`, the narrow touched-object build, and focused `[bandit][live_world]` / `[bandit][live_world][camp_map]` tests before checkpoint.
- Deterministic evidence can close deterministic sub-slices only; live game claims need named harness/product scenarios from the matrix.
- Do not reopen fuel, Smart Zone, or cannibal optional-control seams unless canon or Josef/Schani explicitly does so.
