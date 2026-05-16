# Cannibal camp first hostile-profile adopter packet v0 (2026-04-22)

Status: checkpointed / done for now.

## Why this exists

If the hostile-site profile layer is real, it needs a second hostile family to prove it.
But jumping straight from bandit camps to `writhing stalker` would mix together two separate questions:
- does the profile layer actually work?
- does the singleton stalker design work?

That is too much mystery in one pot.

This packet uses **cannibal camp** as the first non-bandit adopter precisely because it should still read like a hostile camp/site owner rather than a one-off predator.
That keeps the proof on the profile layer first.
The repo can earn the weirder singleton case later.

## Scope

1. Land one honest cannibal-camp hostile profile on top of the shared hostile-site substrate.
2. If the current tree lacks a real cannibal-camp hostile site family, first add one **rare dedicated cannibal-camp spawn/theme** instead of pretending the new profile can attach to vapor.
3. The intended first anchor is a **new camp-derived mapgen theme**, likely copied from current bandit-camp footing and then deliberately re-skinned with cannibal names, loadouts, bloodier dressing, human-jerky / butchered-food cues, and similarly readable environmental filth.
4. Keep cannibal-camp cadence, roster, pressure, dispatch, and writeback rules explicit rather than smuggled through bandit names.
5. Make the first adopter coexist with bandit-owned sites without coalition nonsense or accidental shared-state corruption.
6. Keep the packet focused on `prove the new hostile-site profile layer can carry a second camp-shaped hostile family` rather than on a huge hostile-content expansion.

## Non-goals

- writhing-stalker singleton behavior in the same packet
- broad hidden-psychopath / social-camouflage systems
- faction politics or giant cannibal lore expansion
- every hostile-human family at once
- silent runtime mutation of ordinary bandit camps into cannibal camps as the default implementation
- turning the packet into a general content-authoring empire

## Success shape

This packet is good enough when:
1. one honest cannibal-camp hostile profile runs on the shared hostile-site substrate instead of faking reuse through bandit-only assumptions
2. a rare dedicated cannibal-camp anchor/theme exists if the current tree did not already have one, and that anchor reads like a distinct hostile site rather than an unexplained bandit rename
3. the first anchor is allowed to derive from current bandit-camp structure, but its names, loadouts, and environmental dressing are explicit enough that review can tell it is the cannibal variant
4. cannibal-camp cadence, roster, pressure, dispatch, and writeback rules are explicit enough that later behavior can be reasoned about without folklore
5. cannibal-camp owned sites can coexist with bandit-owned sites without coalition nonsense or accidental shared-state corruption
6. the slice stays bounded and does not smuggle in singleton stalker work, giant lore expansion, or whole-faction redesign

## Validation expectations

- prefer proof that the hostile-site profile layer is carrying a second hostile family honestly rather than just renamed bandit behavior
- if a new adopter anchor had to be added first, keep that content proof bounded and explicit instead of laundering it through vague lore wording
- reviewer-readable output should make it obvious which profile/site family is acting, why its behavior differs from the bandit baseline, and why the new mapgen/theme reads as cannibal rather than generic bandit clutter
- broaden to richer live-play proof only after the adoption itself is honest

## Current grounded content note

The current tree already has a little cannibal-flavored content footing, for example `lodge_cannibal_15x15` plus `cannibal_weapons` / `cannibal_food`, but it does **not** yet amount to a real hostile cannibal-camp site family.
That means packet v0 can reuse some thematic content instead of inventing every prop from scratch, while still requiring a real rare dedicated hostile-site anchor.

## Dependency note

This queued packet sits behind `doc/hostile-site-profile-layer-packet-v0-2026-04-22.md`.
`Writhing stalker` stays parked one step longer and should be revisited later as the **first singleton adopter after the profile layer exists**, not crammed into this cannibal-camp proof just because it sounds cooler.
