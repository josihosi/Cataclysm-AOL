# Bandit overmap AI concept (2026-04-19)

## Status

This document is a **concept vessel**, not an active implementation lane.
Its job is to retain the current bandit design cleanly, without smearing it into unrelated active work.

Use it for:
- preserving the current concept canon
- separating frozen ideas from still-hazy questions
- preparing later planning/greenlight work
- anchoring later parked-chain sub-items such as deterministic scoring guidance

Do **not** treat this doc by itself as permission to start implementation.

---

## Purpose

CDDA currently has only thin overmap-side hostile behavior.
Hordes exist, but they do not provide a rich survivor-bandit pressure loop.

The goal here is to create a bandit system that:
- acts on the **overmap**, not only inside the reality bubble
- reacts to **signals** such as smoke, light, and sound
- evaluates **threat** contextually instead of acting like a psychic encounter spawner
- hands off cleanly from coarse overmap behavior into local NPC behavior near the player
- creates readable survival gameplay, where choices like fire, noise, weather, night travel, and route discipline matter

---

## Design principles (current concept canon)

### 1. Coarse overmap, fine bubble
Bandit intent and routing should mostly live on the overmap.
Precise sight, tactics, and local combat behavior should mostly live in the reality bubble / local NPC layer.

### 2. No omniscience
Bandits should react to signals, sightings, memory, and threat estimation.
They should not act as if they have perfect world knowledge.

### 3. Eventful, not every-turn
This should not tick like a giant global brain every second.
Use coarse updates plus event-driven wakeups.

### 4. Performance is part of design
Any elegant idea that requires too much constant simulation is wrong.
Approximation, decay, caching, and distance-scaled update rates are desirable.

### 5. Player-readable consequences
The player should be able to understand why risk rose:
- visible smoke
- visible light
- loud noise
- recent sightings
- repeated travel patterns
- weakness in bad terrain / zombie pressure

### 6. Bandits are regional predators, not encounter vending machines
Fixed camps should be the source of pressure.
Roaming groups should be dispatched from those camps for specific jobs.

---

## Core model (current concept canon)

### Bandit camps
Bandit camps are overmap actor hubs.
They can:
- chill
- scout
- scavenge
- stalk
- extort / toll
- raid
- reinforce
- respond to signals

Each camp should maintain coarse state such as:
- manpower / available bandits
- local confidence / aggression
- supplies / need pressure
- known nearby opportunities
- known nearby dangers
- current mission load

### Job groups
Camps dispatch temporary overmap groups for specific jobs.
A group should have at least:
- source camp
- job type
- rough strength / headcount
- current target or interest source
- confidence / threat tolerance
- remembered information relevant to that mission

### Jobs
Initial job vocabulary should likely include:
- chill / hold
- scout
- scavenge
- toll
- stalk
- steal
- probe
- raid
- reinforce

Jobs should determine:
- how many bandits are sent
- how far they are willing to travel
- how much risk they accept
- whether they seek loot, intel, intimidation, or violence

---

## Overmap time / update model (current concept canon)

The overmap layer should not react every turn.
A coarse cadence is preferred.

Suggested shape:
- far from relevant player activity: update rarely
- near player sphere / active camp sphere: update more often
- near a fresh signal or ongoing mission: update more often still
- hourly-scale updates are a reasonable starting baseline

Preferred triggers:
- periodic coarse tick
- new signal of interest
- mission completion/failure
- threat spike
- route interruption

This allows intelligence without simulating every camp every second.

---

## Overmap signal model (current concept canon)

Bandits should be able to detect rough overmap signals and route toward or react to them.

### Smoke
Smoke should matter at overmap scale.
Relevant modifiers likely include:
- time of day
- cloud/fog/precipitation/weather visibility
- wind or other atmospheric influence if affordable
- brightness of the night, including moon conditions

Important concept note:
- smoke is especially valuable because it creates survival tradeoffs
- a fire can provide heat, cooking, morale, and safety benefits
- but it may also advertise the player or a camp to bandits

### Light
Night light should matter at overmap scale when it is unconcealed and strong enough.
Potential modifiers include:
- night brightness
- weather
- concealment / terrain roughness
- whether the light is direct, shielded, or intermittent

### Sound
Sound should also exist as a coarse overmap interest source.
Examples:
- gunfire
- explosions
- engines
- loud repeated work

This does not need exact tile-perfect truth.
A rough location, intensity, and decay over time is enough for overmap AI.

### Signal philosophy
Signals should create **points of interest**, not perfect knowledge.
Bandits should usually learn:
- something is there
- roughly where
- roughly how strong / recent it seems

Not:
- exact actor identity
- exact tile
- exact inventory

---

## Bandit knowledge / memory (current concept canon)

Bandit camps should maintain a rough remembered map of notable things.
Examples:
- smoke seen here
- night light seen here
- gunfire heard here
- likely settlement here
- repeated traffic here
- dangerous zone here
- previous defeat here
- good ambush road here

Memory entries should be:
- approximate rather than perfect
- decaying over time
- refreshable by scouts or repeat signals
- sometimes wrong or stale

This keeps bandit behavior intelligent without making it psychic.

---

## Threat model (current concept canon)

Bandits should evaluate contextual threat, not just distance.

Threat can include:
- visible armed survivors
- known defenders
- camp fortifications
- recent bandit losses
- hostile faction strength
- local monster/zombie pressure
- the bandit group's own size, morale, supplies, and job urgency

### Key opportunism concept
Local zombie chaos can reduce the player's or another survivor group's **effective threat** in context.

Meaning:
- a well-armed player may normally look dangerous
- but if that player is already entangled with zombies, wounded, distracted, or exhausted in a city
- bandits may judge the target as more attackable

This enables opportunistic ambush behavior instead of static strength comparison.

---

## Overmap-to-bubble handoff (current concept canon)

This seam is central.

### On the overmap
A bandit group should remain abstract:
- size
- job
- route
- target or interest source
- confidence / threat estimate
- memory relevant to the mission

### Near the bubble / encounter range
The group should hand off into local behavior:
- concrete NPC group or encounter
- concrete target focus
- immediate tactical behavior
- local visibility and combat logic

### Division of responsibility
The overmap should answer:
- why are they moving
- where are they going
- what do they think they are doing

The local NPC layer should answer:
- what do they see exactly now
- do they reveal themselves
- do they attack, hold, flee, flank, or negotiate

This separation avoids overloading either system.

---

## Player-facing gameplay promises (current concept canon)

The desired result is gameplay where the player has to care about:
- when to light a fire
- how much smoke it makes
- what the weather is doing
- whether night light is concealed
- how much noise they create
- whether a route is repeatedly exposed
- whether city chaos is hiding them or making them vulnerable

Bandit pressure should become something the player can read and manage, not just suffer randomly.

---

## Implementation direction (current concept canon)

The likely safest overall architecture is:
1. fixed bandit camps as regional sources
2. dispatched overmap job groups
3. signal-driven interest system
4. contextual threat evaluation
5. overmap routing to rough points of interest
6. clean handoff into local encounter behavior near relevance

This should begin with a narrow vertical slice, not a total simulated society.

---

## Suggested v1 slice (not yet greenlit, but currently favored)

A sensible first implementation slice would be:
- fixed bandit camp overmap actor
- coarse hourly update cadence
- one signal class first, likely smoke or sound
- dispatch of a simple scout group
- rough route toward signal source
- handoff into a simple near-bubble observe / harass / retreat behavior
- simple threat gate controlling whether the group presses or backs off

Why this slice:
- it proves the overmap architecture
- it proves the signal concept
- it proves the handoff seam
- it avoids prematurely solving the whole bandit ecosystem

---

## Open questions

These are the current good unresolved questions, not failures.

### 1. Exact overmap visibility math
How approximate should smoke/light/sound detection be?
- simple radius with weather modifiers?
- directional cone?
- terrain/line-of-sight approximation?
- cached signal field?

The concept wants believable intelligence, but this must stay computationally cheap.

### 2. Signal cadence and persistence
When are signals generated and how long do they matter?
- continuous while active?
- sampled on AI tick only?
- turned into decaying remembered points of interest?

### 3. Close-range visibility seam
At what exact distance or condition should overmap interest become local NPC certainty?
The concept direction is clear, but the seam still needs a crisp rule.

### 4. Threat visibility and discoverability
When should the player become aware that bandits regard them as attackable?
- only on contact?
- through rumors/tracks/signs?
- through noticed stalking behavior?

### 5. Existing CDDA hooks
Which current game systems are cheapest/honest to hook for:
- smoke visibility
- light visibility
- sound reach
- threat estimation
- overmap routing / actor updates

### 6. Bandit memory fidelity
How detailed should the remembered map be?
- rough interest points only?
- typed notes per source?
- partial decay with confidence values?

### 7. Interaction with monsters and other factions
How much should zombies, wildlife, and other NPC factions reshape bandit plans?
The current concept definitely wants zombie-chaos opportunism, but the broader interaction model is still open.

### 8. Camp-side player responses
How much basecamp-side planning belongs in v1 versus later?
Examples:
- patrols
- escorts
- scouts
- decoys
- counter-raids
- temporary concealment behavior

### 9. Save/load and persistence cost
How much group and memory state can be stored affordably without making persistence ugly or expensive?

### 10. Readability versus realism
How much warning should the player get before a dangerous ranged or ambush situation?
The concept strongly prefers readable tension over unfair invisible death.

---

## Non-goals for this concept stage

This document is not yet trying to:
- finalize exact formulas
- define every bandit personality type
- solve all local tactical AI
- greenlight implementation immediately
- replace normal NPC perception systems near the player
- build a perfect world simulation

---

## Related parked-chain sub-items

Current linked parked-chain sub-items:
- deterministic scoring guidance: `doc/bandit-bounty-threat-scoring-guidance-2026-04-19.md`
- overmap mark-generation and heatmap model: `doc/bandit-mark-generation-and-heatmap-model-2026-04-19.md`

## Why this vessel exists

This concept is a holding bowl for mixed design nectar.
It exists so later work can package, greenlight, prune, or split the idea without losing the good parts or mistaking every attractive thought for already-frozen canon.
