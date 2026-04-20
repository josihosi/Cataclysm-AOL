# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active lane: **Bandit concept formalization follow-through**.

Current target:
1. continue Package 1 without turning it into one giant law blob
   - micro-item 1, `Terrain bounty bucket table`, is now landed
   - micro-item 2, `Structural bounty harvest trigger rule`, is now landed
   - micro-item 3, `Structural bounty reappearance rule`, is now landed
   - micro-item 4, `Moving bounty source table`, is now landed
   - micro-item 5, `Moving bounty attachment rule`, is now landed
   - micro-item 6, `Moving bounty clear / rewrite rule`, is now landed
   - micro-item 7, `Threat source table`, is now landed
   - micro-item 8, `Threat rewrite rule`, is now landed
   - micro-item 9, `Threat-and-bounty coexistence rule`, is now landed
   - stay on micro-item 10, `Destination-only vs along-route collection rule`
2. keep the law honest and narrow
   - answer exactly whether bandits may collect value opportunistically while traveling or only at the explicit destination
   - make later implementation stop guessing whether route-side pickup is allowed, forbidden, or silently half-allowed by prose drift
   - only touch adjacent stale wording when that collection pass directly forces it
   - do not answer micro-items 10-13 at the same time just because the laws smell related
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
