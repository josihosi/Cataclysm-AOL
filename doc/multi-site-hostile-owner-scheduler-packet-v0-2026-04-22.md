# Multi-site hostile owner scheduler packet v0 (2026-04-22)

Status: greenlit backlog contract.

## Why this exists

The active `Bandit live-world control + playtest restage packet v0` lane is finally making one live hostile owner real inside the game.
That is necessary, but it is still only one-site proof.

The next hollow seam is obvious:
if the repo can only run one honest hostile owner at a time, then later cannibal / feral / other hostile-site work will either fake independence or quietly collapse back into one spooky shared bandit brain.

This packet exists to prove the hostile-owner substrate can run a **small set of independent hostile sites** at once and keep their state separate.
That means separate anchors, rosters, outings, remembered pressure, and writeback.
Not one giant coalition blob with better marketing.

## Scope

1. Extend the current live hostile-owner seam from one-site-at-a-time proof to `2-3` simultaneous hostile sites on one world.
2. Keep each hostile site's state independent and explicit:
   - anchor / home site
   - roster and live remaining headcount
   - active outing or local-contact state
   - remembered pressure / marks / target state
   - persisted writeback strong enough to survive save/load honestly
3. Preserve the current product rule that site-backed camps keep a home presence while losses continuously shrink later outings instead of snapping back to folklore counts.
4. Reuse existing territoriality / active-pressure / distance-burden footing to damp silly same-target pile-ons rather than inventing magical coalition coordination.
5. Keep the packet small enough that review can still answer the question `do these hostile sites behave like separate owners?` without needing a grand-strategy lecture.

## Non-goals

- faction grand strategy or coalition command behavior
- dozens of hostile families at once
- social-threat, toll, or extortion implementation
- giant whole-world omniscient hostile maps
- broad new local tactical AI
- profile-layer generalization beyond what is needed to keep several owners independent

## Success shape

This packet is good enough when:
1. the live hostile-owner seam can run `2-3` simultaneous hostile sites on one world without collapsing them into one shared fake camp brain
2. each hostile site keeps its own anchor/home site, roster/headcount, active outing/contact state, remembered pressure/marks, and persisted writeback state
3. site-backed camps still keep a home presence while losses continuously shrink later outings instead of snapping back to folklore counts
4. one site's losses, return state, or contact pressure do not silently rewrite another site's state
5. repeated same-target pressure can overlap occasionally without turning into routine multi-site dogpile coalition behavior
6. save/load stays honest for multiple hostile owners at once instead of only for the single easiest happy-path site
7. the slice stays bounded and reviewer-readable instead of widening into hostile-human empire

## Validation expectations

- prefer narrow deterministic coverage for multi-owner save/load, state separation, and independent writeback before broader live drama
- add at least one controlled proof where multiple hostile sites coexist and later divergence is visible reviewer-cleanly
- if overlap pressure is demonstrated, make the output readable enough to show `independent convergence happened` rather than `the sites secretly merged`
- broaden to live probe only when deterministic or tightly controlled proof stops being honest enough to answer the independence question

## Dependency note

This queued packet sits directly behind `doc/bandit-live-world-control-playtest-restage-packet-v0-2026-04-22.md`.
Do not pull it forward until that active lane has an honest nearby restage/writeback/perf proof, or the scheduler packet will end up grading an unproven base seam.
