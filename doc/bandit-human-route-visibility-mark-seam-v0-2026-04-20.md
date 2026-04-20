# Bandit human / route visibility mark seam v0 (2026-04-20)

Status: active greenlit contract.

## Promotion audit verdict

The smoke bridge is checkpointed.
The light bridge is checkpointed.
The next narrow visibility promotion is the human / route activity bridge.

Current bandit seams already know how to reason about `moving_carrier`, `corridor`, and `site` envelopes.
What they still do not have is a bounded adapter that turns direct human sightings and repeated route-shaped activity into those envelopes without magical certainty.

This contract is the next honest bite after light.
It keeps the visibility packet moving without jumping into full concealment law, settlement-signature mythology, or a broad traffic simulator.

## Scope

1. Build a bounded human / route adapter that turns direct human sightings and repeated route-shaped activity, or equivalent deterministic route-visibility packets, into coarse overmap-readable bandit signal state.
2. Feed those packets into the current bandit mark-generation seam as `moving_carrier`, `corridor`, or bounded `site` marks when the evidence honestly supports each shape.
3. Preserve the product asymmetry that direct human sightings are strong bounty and occupancy clues, while repeated route activity should reinforce only when it plausibly belongs to somebody else, to a shared corridor, or to corroborated site traffic.
4. Preserve the anti-magic rule that the camp's own routine traffic should not harden into hostile-contact truth by default.
5. Expose the packet and resulting mark or lead path in reviewer-readable report output so later scenario and 500-turn proof work can reuse the seam instead of hand-authoring route clues again.

## Non-goals

- broad player/basecamp concealment implementation
- smoke or light adapter rework
- sound or horde-attraction expansion
- a separate magical settlement-signature class
- full route-history simulation or broad logistics persistence
- the first full 500-turn proof itself

## Success shape

This item is good enough when:
1. a bounded human / route adapter exists from direct-sighting and repeated-route footing into coarse overmap-readable bandit signal state
2. deterministic coverage proves the key bounded distinctions: direct human sighting versus repeated corridor activity, moving-carrier attachment versus site inflation, self-camp routine suppression, and at least one corroborated shared-route refresh case
3. generated human / route marks or leads feed the current bandit mark-generation, playback, and evaluator seams reviewer-cleanly
4. the slice stays visibly bounded and does not smuggle in a broader concealment system, settlement-signature rewrite, or the 500-turn proof packet

## Validation expectations

- prefer narrow deterministic bandit coverage and small route-scenario extensions on the current playback footing
- the first honest code validation should stay `make -j4 tests` and `./tests/cata_test "[bandit]"`
- keep evidence reviewer-readable; do not hide the route seam inside debugger soup or broad live-harness theater unless deterministic proof stops being enough

## Dependency note

This slice promotes the human / route portion of the parked visibility packet at:
- `doc/bandit-player-basecamp-visibility-and-concealment-2026-04-19.md`
- `doc/bandit-visibility-physical-systems-recon-2026-04-19.md`

It should reuse the already-checkpointed bandit evaluator, mark-generation, playback, handoff, smoke bridge, and light bridge seams rather than reopening them.
