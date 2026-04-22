# Hostile site profile layer packet v0 (2026-04-22)

Status: greenlit backlog contract.

## Why this exists

Once multiple hostile owners can coexist, the next risk is just as stupid in a different direction:
all later hostile-site work could still be secretly hardcoded to current bandit-camp assumptions.
Then the repo would technically support more than one owner, but only by teaching every new hostile family to wear a bandit hat.

This packet exists to put one explicit **hostile site profile layer** on top of the multi-owner scheduler.
The goal is not a giant generic faction-AI cathedral.
The goal is one bounded shared substrate where camp-style and smaller hostile sites can differ in clean visible ways without forking the whole system.

## Scope

1. Define one shared hostile owner/cadence/persistence substrate with explicit per-profile rules instead of hardcoding current bandit-camp assumptions into every call site.
2. Support at minimum:
   - one camp-style hostile profile
   - one smaller hostile-site profile that is still not the singleton stalker case
3. Make profile-driven differences explicit and reviewer-readable, including at least:
   - dispatch capacity / reserve rule
   - threat posture bias
   - return-clock / persistence lean
   - writeback expectations after contact or loss
4. Preserve the same bounded live-world/writeback contract instead of reopening broad hostile-human architecture arguments.
5. Keep the layer honest enough that later adopters can say `this is a new hostile-site profile on the shared substrate` rather than `this is bandit code copied three times with different nouns`.

## Non-goals

- a giant generic faction-AI framework
- writhing-stalker singleton behavior in this same packet
- broad diplomacy, social-horror, or extortion systems
- magical stealth, sight-avoidance, or omniscience behavior
- every hostile-human family in the repo
- replacing the active bandit live-world lane with an abstract framework sermon

## Success shape

This packet is good enough when:
1. one shared hostile owner/cadence/persistence substrate exists with explicit profile rules instead of hardcoded bandit-camp-only assumptions
2. at minimum one camp-style profile and one smaller hostile-site profile can coexist on that substrate reviewer-cleanly
3. dispatch, threat posture, return-clock, and writeback differences are profile-driven and readable rather than site-kind spaghetti
4. the multi-site scheduler can consume those profiles without regressing the already-honest bandit live-owner footing
5. the packet stays bounded and does not widen into giant generic hostile-human architecture or singleton stalker weirdness

## Validation expectations

- prefer deterministic or tightly controlled proof that profile differences are explicit in state and behavior rather than only implied by comments
- make at least one side-by-side comparison readable enough that review can tell why two hostile-site profiles behaved differently on the same substrate
- keep the proof focused on substrate/profile correctness, not on full live encounter theatrics
- broaden to heavier live proof only if the profile split cannot be judged honestly from narrower evidence

## Dependency note

This queued packet sits behind `doc/multi-site-hostile-owner-scheduler-packet-v0-2026-04-22.md`.
It is the bridge from one-family bandit machinery to a reusable hostile-site substrate.
`Writhing stalker` deliberately stays out of this packet; if the layer is still mush, singleton pressure work will just hide the mush under cooler prose.
