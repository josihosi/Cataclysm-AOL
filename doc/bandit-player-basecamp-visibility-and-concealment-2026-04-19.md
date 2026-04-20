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

It also does **not** assume we need a wholly new light-vector system.
The better default is to reuse the existing local directional-light footing and only add a coarse overmap adapter layer.

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
A directly discovered camp or obviously durable occupied site can imply:
- high bounty
- high threat
- stable repeated activity
- meaningful route/return value

These locations should feel more legible than one-off campfires because direct discovery and repetition matter.
But ordinary survivor routine should **not** harden into a special settlement-signature class.
Repeated site-centered smoke/light/traffic should usually just raise bounty and confidence on the map.
Then, when bandit camp needs and cadence make the region worth approaching, groups investigate the accumulated read instead of relying on a separate magical "settlement signature" rule.

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
- searchlights or other obvious scanning beams

General meaning:
- if distant light is actually visible, it usually implies current occupation, current use, or a meaningful active event
- not all visible light should mean the same thing, because a sweeping searchlight reads very differently from a static lamp, fire, or lit window

Interpretation tendency:
- stronger at night or in dark conditions
- often implies immediate occupation more than old smoke does
- **ordinary light** such as lamps, fires, lit interiors, and routine camp lighting should lean toward **bounty / occupancy** interpretation
- **searchlights** or other obvious scanning / patrol-style beams should lean toward **threat** interpretation first, not generic bounty

Important laws:
- daylight should suppress distant light usefulness essentially completely, matching the real game footing rather than letting light become a daytime world-map beacon
- visible light is stronger evidence of active occupancy than smoke alone, but still not perfect identity truth
- smoke plus visible light in the same area should count as strong corroboration of meaningful active occupancy, not merely vague ambience
- moving or scanning light should be classified separately from ordinary static light so bandits do not score a watch beam like a cozy lamp

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

### 4. Repeated site-centered activity
Typical examples:
- repeated smoke/light at the same region
- traffic around one site
- stable occupation pattern

General meaning:
- this is not random noise, it may be a durable target or pressure site

Interpretation tendency:
- strong reinforcement of mark confidence
- rising bounty and stronger revisit value
- threat should rise mainly when other cues suggest defenders, patrols, searchlights, prior losses, or other hard danger

Important law:
- this should usually remain an aggregation of repeated ordinary signals, not a separate settlement-signal family

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
- rough law now is enough; exact constants and edge cases should be left to fitting-based tuning later
- this should stay coarse and overmap-oriented here, not turn into exact tile-stealth law

Useful directional rule:
- a lit site may be legible from one side and concealed from another
- closed houses, curtained windows, walls, stoves or other contained indoor sources, and forest cover can reduce or block outward light leakage on some approaches while leaving other directions more exposed
- trees/forest should likely block or suppress outward light strongly
- buildings should also mask light, but probably in a different and tunable way rather than by pretending every wall behaves like a tree line
- later implementation should expect some fitting-based balancing here instead of pretending the first constants will be holy scripture
- repo recon now suggests this should lean on the existing local light model, which already has directional arcs and quadrant-aware apparent-light handling, then summarize that into a cheap overmap-facing exposure result
- later implementation should therefore prefer a cheap directional exposure adapter, not expensive tile-perfect world-map ray theater

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
- repeated mixed signals do **not** need to become a separate settlement-signature class to matter
- they can simply become a stronger bounty/confidence region that later gets approached when bandit needs and cadence make it worth the risk
- smoke/light usually create **bounty-first** legibility; sharper threat should mostly come later from closer observation, prior losses, searchlights, patrol behavior, fortification cues, or actual contact

Examples:
- one night light: likely current occupancy or current use, but not yet automatic settlement truth
- one roadside light source: maybe a vehicle stop, police-style road event, or another dangerous temporary contact
- smoke two days in a row: stronger suspicion of recurring human activity
- smoke plus visible night light at one site: strong corroboration that something meaningful is active there now
- a couple of ordinary scavenging passes: still human/route activity, not automatic special-site truth
- recurring smoke + light + traffic pattern: stronger site-centered bounty region, more likely to attract a later approach when supplies or opportunity pressure push action

This should integrate with the existing mark/heatmap doc instead of replacing it.

---

## Concealment levers

The player and a basecamp should have understandable ways to reduce exposure.
Much of this is already ordinary survivor logic, not a brand-new subsystem we need to invent from scratch.

### Fire discipline
- fewer fires
- shorter burns
- more careful burn timing
- avoiding obvious repeated plume behavior
- avoiding smoky daytime burns when possible

### Light discipline
- less open nighttime lighting
- reducing exposed bright sources
- not broadcasting occupancy for free
- containing or masking night light instead of letting it spill outward

### Route discipline
- avoiding overly repetitive movement patterns
- not creating the same obvious path or disturbance signature every day

### Weather exploitation
- using poor visibility conditions when movement or signaling should be harder to read
- accepting that this cuts both ways and may also reduce your own legibility to allies
- accepting that the same conditions may also create better approach or escape windows for hostile groups

### Optional player-facing feedback
If later implementation wants a small usability helper, a good candidate is:
- let the player notice when their activity is creating a meaningful overmap-readable smoke signal
- possibly show that as a coarse overmap-style smoke indication, perhaps one z-level above ground, rather than pretending the player should infer everything blindly

This should be treated as player feedback / readability help, not as a requirement to invent a separate smoke law.

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

## Directional light exposure adapter sketch

If later implementation wants a practical v1 for visible night light, the cleanest shape now looks like this:

### Input stance
- do **not** infer light exposure from overmap terrain class alone
- do **not** treat every indoor lamp as automatically overmap-visible
- do reuse the existing local lightmap/light-arc truth as the physical footing

### Suggested cheap summary shape
For a sampled active site or local region, derive a coarse exposure packet such as:
- `north_exposure`
- `east_exposure`
- `south_exposure`
- `west_exposure`
- `peak_exposure`
- `contained_light`
- `light_class`
- `scan_signature`

Where each side bucket means roughly:
- how much light meaningfully leaks outward from that side under current conditions
- not exact tactical visibility, just coarse outward legibility

And where the extra light fields mean roughly:
- `light_class`: ordinary occupancy light versus obvious searchlight / threat-style light
- `scan_signature`: whether the visible light pattern looks static, intermittent, or sweeping enough to imply active scanning rather than passive illumination

### Practical derivation rule
A good v1 implementation shape would be:
1. only evaluate this for relevant candidate sites or active signal regions, not for the whole world continuously
2. suppress the whole pass in daylight unless something unusually bright explicitly deserves exception
3. read local light truth after normal lightmap generation, rather than rebuilding fake light logic in bandit code
4. collapse that local truth into four coarse side buckets or quadrants
5. classify the visible pattern into ordinary light versus searchlight-like threat light when the source / motion / beam shape makes that distinction legible
6. reduce the result by current weather / visibility penalties
7. emit a light-family mark only if one or more buckets clears a meaningful threshold

### Why this shape is attractive
It preserves the useful truths we actually want:
- a house can leak light more on one side than another
- curtains, walls, forest edge, or contained indoor sources can keep some light internal
- a bright source can be noticeable without yielding clean tactical truth
- a sweeping beam can read as defensive threat while a static lamp reads as occupancy or bounty
- overmap logic stays cheap and inspectable instead of pretending to be a second tile-light simulator

### What the adapter should not do
- do not recompute full tile-perfect ray behavior for distant bandit AI every cadence pass
- do not give exact actor identity or exact room knowledge from light alone
- do not let one weak transient light instantly harden into settlement truth
- do not flatten all visible light into one generic score when searchlights and ordinary lamps obviously mean different things
- do not bypass weather, daylight, or containment just because a source exists

This keeps the implementation promising without making it computational theater.

---

## Smoke exposure adapter sketch

Smoke wants a slightly different adapter than light.
It should care less about side-specific leak buckets and more about plume strength, persistence, and weather shaping.

### Input stance
- do **not** treat all fires as equal smoke signals
- do **not** assume local smoke field visibility and overmap smoke legibility are the same question
- do reuse the current fire/smoke/wind footing as the physical substrate

### Suggested cheap summary shape
For a sampled active site or recent burn region, derive a coarse smoke packet such as:
- `smoke_strength`
- `smoke_persistence`
- `smoke_height_bias`
- `smoke_spread_bias`
- `smoke_confidence`

Where these mean roughly:
- how much smoke is being or was recently produced
- whether it is a brief puff or a repeating / sustained plume
- whether the smoke likely rises into a more legible plume shape
- how much wind/shelter fuzzes, smears, or displaces the signal
- how believable the resulting overmap smoke mark should be

### Practical derivation rule
A good v1 implementation shape would be:
1. sample only relevant active fires / burn sites / other smoke-producing sources, not arbitrary empty terrain
2. derive smoke weight from existing smoke-production footing rather than a fixed "fire means smoke" rule
3. raise confidence for sustained or repeated smoke production, not one tiny transient puff
4. modify the result by shelter, wind, and weather so the mark can become clearer, fuzzier, or displaced
5. allow the resulting smoke-family overmap mark to project farther than ordinary local sight range when conditions support a meaningful plume
6. keep the mark coarse and uncertain, not a magical exact column locator

### Important current-engine caveat
At the moment, the physical smoke source itself appears to live in the **reality bubble** world.
Current fire processing and field processing run on the loaded map bubble, not as a global offscreen smoke simulation.

That means the later bandit system should probably **not** depend on literal offscreen smoke fields existing forever outside the bubble.
The cleaner design is:
- bubble-local fire/smoke behavior produces or refreshes abstract smoke marks
- those marks can persist or be read at overmap scale
- the overmap mark is allowed to outlive the immediate local field simulation

So yes, a smoke mark may legitimately matter at a broader scale than local vision range, even if the literal smoke field is only ever simulated while the source area is loaded.

### Why this shape is attractive
It preserves the useful truths we actually want:
- larger / dirtier burns should matter more than tiny clean burns
- repeated smoke should feel more settlement-like than one accidental puff
- wind and shelter should shape the read instead of acting like decorative flavor text
- smoke can be legible from far away without requiring infinite global gas simulation

### What the adapter should not do
- do not require a literal smoke field to be continuously simulated outside the reality bubble
- do not make every campfire a world-map beacon
- do not give exact room or actor identity from smoke alone
- do not ignore that wind may blur the source location while still preserving the fact that "something is burning over there"

This keeps smoke strong as a strategic signal without tying the concept to a non-existent full-world plume sim.

---

## Starter numeric packet for v1 tuning

These numbers are a **starter packet**, not sacred doctrine.
But the point is to stop hand-waving and freeze something testable.

### Engine-grounded scale facts
- **1 overmap tile = 24 map tiles**
- current reality-bubble view radius is about **60 map tiles**, which is about **2.5 overmap tiles**
- for coarse overmap reasoning, the clean round number is therefore a **3 overmap tile radius** around the player for **bubble-local / high-relevance** logic
- but that local bubble-fed zone should **not** be mistaken for the whole abstract bandit overmap play-space
- the **strategic bandit theater** needs to be much larger, or bandits collapse into a dumb tripwire near the player
- current preferred starter lean: **about 60 overmap tiles radius** for the main abstract theater when computationally affordable
- if that turns out too expensive in real profiling, something around **48 OMT** is the fallback, not a return to tiny tripwire scale

### Starter sight/read ranges
- **ordinary bounty visibility range:** start at **10 overmap tiles** in clear conditions
  - this matches current code footing surprisingly well: `Character::overmap_sight_range()` effectively lands at base `6` plus perception contribution, which is **10 OMT** for the default `PER 8` survivor
- **confident threat visibility range:** start at **6 overmap tiles**
  - this is intentionally tighter than bounty so camps can notice "something profitable may be there" before they feel they truly understand danger there
- **searchlight / hard-danger threat extension:** allow sharper threat reads to stretch to about **8 overmap tiles** when there is corroboration such as searchlights, patrol behavior, prior losses, or other explicit danger evidence
- **meaningful smoke-plume range:** allow sustained smoke to project out to about **15 overmap tiles** as a coarse outer cap
  - weak or brief smoke should often read at less than that
  - this deliberately makes smoke legible farther than ordinary sight without turning every tiny fire into a world-map beacon

### Movement-budget consequence
Daily movement budget should stay much smaller than the full strategic theater.
The current follow-through canon freezes this starter envelope by outing type:
- local forage skim / camp-edge opportunism -> **1 OMT/day**
- scout / cautious probe / short shadow -> **2 OMT/day**
- toll setup / convoy hit / route ambush -> **3 OMT/day**
- ordinary scavenge / steal run -> **4 OMT/day**
- raid / hard reinforce / committed strike -> **5 OMT/day**
- rare explicit theater reposition / emergency redeploy -> **6 OMT/day**

That keeps the wide strategic theater useful without implying same-day nonsense:
- routine local jobs stay local
- planned haul or raid jobs can reach farther, but still should not cross half the county in one day
- the 6-OMT ceiling is a rare top-end reposition packet, not the default daily economy loop
- later cadence-spend, distance-burden, and return-clock law can narrow or chunk this, but should not mint free extra travel because the AI woke often

---

## Open questions that can stay parked

These are real questions, but they do not block parking the concept.

- how strongly repeated site-centered signals should amplify bounty/confidence before a camp starts taking real approach interest
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
