# Bandit player/basecamp visibility and concealment v1 (2026-04-19)

## Status

This is a **parked concept-chain guidance doc**, not an implementation greenlight.
Its job is to define how player activity, NPC activity, camps, settlements, and city activity become coarse overmap-visible signals that bandit groups interpret as possible bounty, possible threat, or both.

This doc is grounded in the supporting recon note:
- `doc/bandit-visibility-physical-systems-recon-2026-04-19.md`

Do **not** treat this doc as permission to implement the full bandit visibility system.

---

## Purpose

The parked chain already has:
- the broad concept vessel
- deterministic bounty/threat scoring
- overmap mark-generation and heatmap logic
- the bidirectional overmap-to-bubble handoff seam
- a physical-systems recon note for smoke, light, sound, weather, and visibility

The missing item was:

> how activity from the player, NPCs, camps, settlements, and cities becomes coarse overmap legibility that bandits can interpret without magical certainty

This doc freezes that visibility/concealment layer.

---

## Scope

This guidance currently covers:
- signal sources that can reveal activity
- environmental filters that strengthen, blur, or suppress those signals
- interpretation outputs for bandit overmap logic
- concealment levers available to the player or a basecamp
- repeated-signal reinforcement of overmap marks
- compatibility with the scoring, heatmap, and handoff docs

---

## Explicit non-goals

This guidance does **not** yet define:
- implementation details or APIs
- exact local combat AI or tactics
- exact tile-level perception law near the player
- exact itemized loot visibility math
- a new fog-based sound dampening rule
- a separate fake weather/physics system outside the real engine footing

This is a visibility/concealment concept slice, not the whole machine.

---

## Core rule

Signals do **not** tell bandits:
- this is definitely the player
- this is definitely a basecamp
- this is definitely free loot

Signals tell bandits:
- **something active may be here**
- **there may be profit here**
- **there may be danger here**

But the two axes do **not** need the same legibility.
A good v1 asymmetry is:
- **bounty** may be legible at broader overmap scale from terrain/building class, visible activity, light, smoke, and obvious human sightings
- **threat** should usually require closer observation, local visibility, or hard mission-result evidence before it becomes confidently sharp

So the output of this layer is not truth.
It is a coarse possible-contact location with confidence plus bounty/threat hints.

---

## What can become visible

### Humans / NPC activity
Human presence can imply both:
- **bounty**, because people imply supplies, captives, route value, camps, traffic, or lootable activity
- **threat**, because people can defend themselves, have allies, or lead bandits into a bad fight

### Player activity
The player is not a special cosmic exception.
If the player creates repeated visible activity, bandits should interpret it the same broad way:
- possible bounty
- possible threat

The system should avoid magical player-targeting and stay activity-based.

### Basecamps / settlements
A real camp or repeating settlement footprint can imply:
- high bounty
- high threat
- stable repeated activity
- meaningful route/return value

These locations should feel more legible than one-off campfires because repetition matters.
But ordinary survivor routine should **not** harden into a settlement signature too quickly.
Durable settlement interpretation should usually require site-centered repetition across more than one cadence pass, or mixed corroborating signals, not one or two generic chores.

### Empty cities
Cities can also read as both:
- **bounty**, because salvage/scavenge opportunity exists
- **threat**, because zombie density, uncertainty, clutter, and failed approach risk are high

So a city should not be treated as “free loot field.”
It is structurally mixed value.

Useful bounty baseline split:
- open streets, meadows, and ordinary fields should usually contribute little or no structural bounty by themselves
- forests may contribute a little bounty because they can hide activity or support rough survivor use
- houses, buildings, and denser urban structures should contribute meaningfully more bounty than open empty terrain
- visible human/NPC sightings should be one of the strongest mobile bounty cues

---

## Signal-source families

### 1. Smoke signals
Typical examples:
- campfires
- burning structures
- industrial or repeated fire activity

General meaning:
- something active happened or is happening here
- possible shelter, cooking, defense, scavenging, or destruction

Interpretation tendency:
- moderate location signal
- possible bounty
- possible threat

Important law:
- smoke should be treated as a coarse activity sign, not a perfect identity beacon

### 2. Light signals
Typical examples:
- open flames
- lamps
- lit structures
- night travel or camp lighting
- vehicle or road-stop lighting

General meaning:
- if distant light is actually visible, it usually implies current occupation, current use, or a meaningful active event

Interpretation tendency:
- stronger at night or in dark conditions
- often implies immediate occupation more than old smoke does
- can imply danger as much as bounty, because some visible lights may belong to vehicles, road stops, defensive positions, or other high-risk contact situations

Important laws:
- daylight should suppress distant light usefulness essentially completely, matching the real game footing rather than letting light become a daytime world-map beacon
- visible light is stronger evidence of active occupancy than smoke alone, but still not perfect identity truth
- smoke plus visible light in the same area should count as strong corroboration of meaningful active occupancy, not merely vague ambience

### 3. Human / route activity
Typical examples:
- repeated travel paths
- recurring scavenging patterns
- repeated local noise or signs of movement
- visible occupation behavior around a location
- direct overmap-scale human/NPC sightings

General meaning:
- there may be people, logistics, habit, or defended use here

Interpretation tendency:
- direct human sighting should usually create strong bounty interest immediately
- threat should remain less certain unless the sighting also carries closer-range evidence of defenders, force strength, or visible hostile pressure

Important law:
- the same camp's own routine recon or logistics traffic should not be allowed to harden into fresh hostile-contact truth by default
- repeated route activity only deserves real reinforcement when the signal plausibly comes from somebody else, from a genuinely mixed/shared corridor, or from external corroboration

### 4. Settlement activity
Typical examples:
- repeated smoke/light at the same region
- traffic around one site
- stable occupation pattern

General meaning:
- this is not random noise, it may be a durable target or pressure site

Interpretation tendency:
- strong reinforcement of mark confidence
- high bounty and non-trivial threat by default

### 5. City activity
Typical examples:
- salvage fires
- movement near city edges
- repeated visible disturbance in urban zones

General meaning:
- there may be recoverable value, but the area is also inherently dangerous and uncertain

Interpretation tendency:
- city activity should usually inject threat and bounty together, not only one axis

---

## Environmental filters

These filters do not create activity on their own.
They alter how legible signals are.

### Fog / mist
Good current footing:
- fog/mist can suppress **sight** and **light legibility**
- fog/mist does **not** need to create new sound-dampening rules for this concept item

Meaning:
- smoke and light can become less reliable or lower-confidence under fog/mist
- sound can keep using existing engine footing without new romantic nonsense

### Daylight / darkness
General rule:
- light signals matter much more in darkness
- daylight suppresses distant light usefulness essentially to zero for overmap-read purposes
- darkness may also make some non-light visual cues less reliable

### Rain / storms
General rule:
- storms and heavy weather can lower visual confidence
- existing engine footing already supports weather affecting sound and visibility
- the concept layer should reuse that rather than inventing a second weather law

### Wind
General rule:
- wind can alter smoke spread, persistence, and confidence shape
- wind should mostly change how fuzzy or displaced a smoke-derived mark is, not become an entire new ideology

### Terrain / shelter
Where relevant:
- shelter, forest, walls, and terrain masking can reduce signal legibility
- for light specifically, visibility should depend on coarse **obstruction / exposure**, not merely on whether a light source exists somewhere in the structure
- this should stay coarse and overmap-oriented here, not turn into exact tile-stealth law

Useful directional rule:
- a lit site may be legible from one side and concealed from another
- closed houses, curtained windows, walls, stoves or other contained indoor sources, and forest cover can reduce or block outward light leakage on some approaches while leaving other directions more exposed
- later implementation should prefer a cheap directional exposure model, not expensive tile-perfect world-map ray theater

Important bilateral rule:
- weather and terrain concealment should not act like a player-only stealth blessing
- the same fog, rain, darkness, shelter, or clutter that reduces bandit read on player/basecamp activity can also hide approaching bandits, route harassment, or withdrawal movement
- the design goal is more mutual uncertainty, not one-sided invisibility

---

## Interpretation outputs

The visibility layer should produce a small coarse result packet.

Suggested v1 outputs:
- `location_confidence`
- `bounty_hint`
- `threat_hint`
- `signal_family`
- `persistence_bias`

### Location confidence
How likely is it that the marked region contains meaningful activity at all?
This is not certainty, only confidence.

Useful v1 implication:
- broad bounty confidence may legitimately outrun broad threat confidence
- bandits may know a place looks promising before they know whether it is safe enough to exploit

### Bounty hint
How much potential value might exist here?
Examples:
- supplies
- camp stock
- captives
- scavenging opportunity
- route value

### Threat hint
How dangerous might contact be here?
Examples:
- defenders
- zombie density
- terrain clutter
- uncertain local force strength
- evidence of repeated organized activity

### Signal family
What kind of thing created this mark?
Examples:
- smoke
- light
- human activity
- settlement pattern
- city disturbance

### Persistence bias
Should the mark fade quickly, or does it smell like a durable repeated location?

---

## Mark reinforcement and repetition

One signal should not carry the same weight as repetition.

Good rule:
- one-off signals create tentative marks
- repeated signals refresh marks and raise confidence
- repeated mixed signals at the same area can harden a weak suspicion into a meaningful target region
- settlement-class interpretation should require stronger reinforcement than generic human-activity interpretation

Examples:
- one night light: likely current occupancy or current use, but not yet automatic settlement truth
- one roadside light source: maybe a vehicle stop, police-style road event, or another dangerous temporary contact
- smoke two days in a row: stronger suspicion of recurring human activity
- smoke plus visible night light at one site: strong corroboration that something meaningful is active there now
- a couple of ordinary scavenging passes: still human/route activity, not automatic settlement truth
- recurring smoke + light + traffic pattern: stronger basecamp/settlement interpretation

This should integrate with the existing mark/heatmap doc instead of replacing it.

---

## Concealment levers

The player and a basecamp should have understandable ways to reduce exposure.

### Fire discipline
- fewer fires
- shorter burns
- more careful burn timing
- avoiding obvious repeated plume behavior

### Light discipline
- less open nighttime lighting
- reducing exposed bright sources
- not broadcasting occupancy for free

### Route discipline
- avoiding overly repetitive movement patterns
- not creating the same obvious path or disturbance signature every day

### Weather exploitation
- using poor visibility conditions when movement or signaling should be harder to read
- accepting that this cuts both ways and may also reduce your own legibility to allies
- accepting that the same conditions may also create better approach or escape windows for hostile groups

These levers should feel like understandable player choices, not hidden punishment math.

---

## Interaction with existing parked docs

### Relation to scoring guidance
This doc does not decide missions by itself.
It helps create the coarse marks and hint values that scoring logic can later consume.

### Relation to mark-generation and heatmaps
This doc explains what kinds of activity may feed marks.
The heatmap doc explains how marks are created, refreshed, decayed, and folded into broader fields.

### Relation to the handoff seam
This doc helps explain why a group becomes interested in a region in the first place.
The handoff doc explains how an abstract group enters and leaves local play once that interest becomes action.

---

## Suggested v1 structure for later implementation thinking

Keep the later implementation seam small and inspectable.

Possible conceptual flow:
1. observe coarse signal source
2. apply environmental filters
3. derive confidence + bounty hint + threat hint
4. update or refresh marks
5. let scoring logic decide whether the region is worth attention

That is enough.
No mystical omniscience engine required.

---

## Open questions that can stay parked

These are real questions, but they do not block parking the concept.

- whether settlement activity should be its own signal family or mainly an aggregation of smoke/light/human traces
- how much terrain/shelter masking should be specified here versus left to later implementation detail
- how explicit the default “cities imply both bounty and threat” language should be in later synthesis
- how coarse route-discipline evidence should be before it starts creating meaningful repeated marks

---

## Why this item is parked

This doc makes the bandit packet more concrete and player-legible.
But it should remain parked until the whole bandit concept chain is coherent enough to reconsider for actual greenlight.

Related docs:
- `doc/bandit-overmap-ai-concept-2026-04-19.md`
- `doc/bandit-bounty-threat-scoring-guidance-2026-04-19.md`
- `doc/bandit-mark-generation-and-heatmap-model-2026-04-19.md`
- `doc/bandit-overmap-to-bubble-handoff-seam-2026-04-19.md`
- `doc/bandit-visibility-physical-systems-recon-2026-04-19.md`
