# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active lane: **Bandit concept formalization follow-through**.

Current target:
1. continue Package 3 without turning it into one giant cross-layer blob
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
   - micro-item 25, `Threat veto vs soft-veto rule`, is now landed
   - micro-item 26, `Overmap-to-bubble entry-mode chooser`, is now landed
   - micro-item 27, `Bubble-to-overmap return-state packet`, is now landed
   - stay on micro-item 28, `Save/load persistence boundary`
2. keep the law honest and narrow
   - answer exactly which bandit-side state must survive save/load after the now-landed return-state packet, without silently solving starter-number cleanup or worked scenarios in the same pass
   - keep candidate generation, score shape, need-pressure rescue, threat-veto law, and the new handoff/return packet frozen as already-landed law, not as a back door to re-litigate job scoring
   - keep micro-item 28 separate from later Package 3 starter-number / scenario / invariant work
   - do not quietly solve micro-items 29-31 just because persistence edges smell related
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
