# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active lane: **Bandit concept formalization follow-through**.

Current target:
1. continue Package 3 without turning it into one giant scoring-law blob
   - micro-item 1, `Terrain bounty bucket table`, is now landed
   - micro-item 2, `Structural bounty harvest trigger rule`, is now landed
   - micro-item 3, `Structural bounty reappearance rule`, is now landed
   - micro-item 4, `Moving bounty source table`, is now landed
   - micro-item 5, `Moving bounty attachment rule`, is now landed
   - micro-item 6, `Moving bounty clear / rewrite rule`, is now landed
   - micro-item 7, `Threat source table`, is now landed
   - micro-item 8, `Threat rewrite rule`, is now landed
   - micro-item 9, `Threat-and-bounty coexistence rule`, is now landed
   - micro-item 10, `Destination-only vs along-route collection rule`, is now landed
   - micro-item 11, `Collection yield rule`, is now landed
   - micro-item 12, `Camp stockpile consumption rule`, is now landed
   - micro-item 13, `Forest yield rule`, is now landed
   - micro-item 14, `Daily movement budget rule`, is now landed
   - micro-item 15, `Cadence budget-spend rule`, is now landed
   - micro-item 16, `Distance burden rule`, is now landed
   - micro-item 17, `Return-clock rule`, is now landed
   - micro-item 18, `Cargo / wounds / panic burden rule`, is now landed
   - micro-item 19, `No-target fallback rule`, is now landed
   - micro-item 20, `No-path fallback rule`, is now landed
   - micro-item 21, `Mid-route abort / divert / shadow rule`, is now landed
   - micro-item 22, `Job candidate generation rule`, is now landed
   - micro-item 23, `Job scoring formula shape`, is now landed
   - micro-item 24, `Need-pressure override rule`, is now landed
   - stay on micro-item 25, `Threat veto vs soft-veto rule`
2. keep the law honest and narrow
   - answer exactly when danger should merely discount a job versus blocking it outright, after the newly landed need-pressure rescue but before later handoff-mode / seam work
   - keep movement budget, distance burden, return clock, burden pressure, candidate generation, the newly-landed score shape, and the newly-landed need-pressure rule frozen as already-landed law, not as a back door to re-litigate board eligibility
   - keep micro-item 25 separate from later Package 3 handoff / seam work
   - do not reopen micro-item 24 just because desperation logic and danger bars smell adjacent
3. keep this doc/spec only
   - no bandit AI code
   - no reopening locker/basecamp lanes while this bandit slice is active

Out of scope right now:
- helper workflow redesign
- bandit implementation
- multi-item bandit synthesis in one pass
- reopening the closed combat-oriented locker lane without a new contradiction
- reopening the closed Package 4 locker surface/control slice without a new contradiction
- reopening the closed organic bulletin-board speech slice without a new contradiction
- hackathon feature lanes
