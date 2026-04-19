# Bandit overmap AI synthesis paper (2026-04-19)

## Status

This document is the **broad parked synthesis paper** for the bandit concept chain.
It is meant to be the first document a later reader opens.

Its job is to synthesize the current parked canon into one coherent picture without pretending the system is already greenlit.

Use it for:
- understanding the full intended bandit overmap architecture at a glance
- checking how the parked sub-items fit together
- future greenlight discussion, if that discussion is explicitly reopened later
- spotting cross-system tensions before implementation work begins

Do **not** treat this paper by itself as permission to start implementation.
This remains parked concept work.

---

## What this paper is trying to solve

CDDA already has overmap motion, hordes, noise, weather, local NPC behavior, and the basic survival pressures that make hostile human activity interesting.
What it does not yet have is a coherent survivor-bandit pressure loop that:
- lives primarily on the **overmap**
- reacts to **signals** instead of acting psychically
- evaluates **bounty** and **threat** as separate but interacting ideas
- hands off cleanly into local NPC behavior near relevance
- lets the player manage risk through legible survival choices rather than arbitrary encounter roulette

The intended result is not "random bandit spawn, now suffer."
The intended result is a regional pressure system where camps notice things, remember things, choose jobs, send groups, learn from outcomes, and create readable tension over time.

---

## Core promise

The whole concept can be reduced to one loop:

1. the world emits coarse visible/audible signs of activity
2. bandit camps turn those signs into remembered marks and regional heat pressure
3. camps score those marks into possible jobs using deterministic bounty/threat logic
4. camps dispatch abstract overmap groups for specific purposes
5. those groups route and react on the overmap
6. relevant groups hand off into local NPC behavior near the bubble
7. local outcomes collapse back into overmap state instead of vanishing
8. camps update memory, confidence, pressure, and future plans from the result

That is the machine.
Everything else is support structure.

---

## Design law

### 1. Coarse overmap, fine bubble
Bandit intent, routing, memory, and strategic choice should mostly live on the overmap.
Exact sight, tactical positioning, reveal timing, flanking, fleeing, and close combat should mostly live in the bubble / local NPC layer.

### 2. Signals, not omniscience
Bandits should react to smoke, light, sound, traffic, remembered sites, prior losses, and contextual threat.
They should not behave as if they have exact tile truth, exact inventory truth, or divine player tracking.

### 3. Determinism where it matters
Scoring, mark upkeep, camp drift, and handoff rules should be deterministic from stored state.
Randomness may still exist in world events, combat, and local chaos, but the overmap decision layer should not run on vibes.

### 4. Eventful, not every-turn global brain
The system should use cadence-based updates plus event-driven wakeups.
Anything that requires constant global second-by-second simulation is suspect.

### 5. Performance is part of design
Approximation, decay, merging, caching, and regional fields are not compromises after the fact.
They are part of the correct architecture.

### 6. Readability beats cheap cruelty
The player should be able to understand why pressure is rising.
Smoke, visible light, repeated route exposure, zombie chaos, recent attacks, and camp visibility should feel legible enough to manage.

---

## System at a glance

### Structural actors
The bandit layer should revolve around:
- **fixed bandit camps** as regional pressure hubs
- **dispatched overmap groups** as temporary mission actors
- **typed remembered marks** as notable pins in memory
- **broad bounty/threat heatmaps** as regional pressure gradients
- **deterministic camp ledgers** that track pressure, manpower, confidence, and mission load

### Structural flow
The natural flow is:
- signals and world-state create or refresh marks
- marks and regional pressure feed camp scoring
- scoring picks jobs
- jobs create abstract overmap groups
- groups travel, investigate, ambush, scavenge, or raid
- local relevance triggers bubble handoff
- outcomes return to overmap state as explicit result packets

This lets bandit behavior feel like the actions of predators with memory and logistics instead of one-off scripts.

---

## Camps, groups, and jobs

### Bandit camps
Camps are the durable strategic actors.
They are not decorative spawn anchors.
They are where regional pressure comes from.

A camp should keep coarse state such as:
- manpower available
- food/ammo/med pressure
- confidence and aggression
- recent gains and recent losses
- current job load
- known opportunities
- known dangers

These values create the camp's strategic mood without turning it into a mystical personality oracle.

Each camp should also own its own **sparse operational map** around its practical operating radius.
That map is camp-owned, not shared omniscience.
It can hold things like:
- broad terrain/building awareness
- bounty estimates
- threat estimates
- confidence
- recently checked / false lead / harvested memory

Recon groups and mission outcomes should reveal, refresh, or correct that map over time.

### Hard state vs temperament
Keep hard state separate from temperament.

Hard state should include:
- supplies and shortages
- manpower
- mission load
- remembered losses/gains
- distance and travel burden
- mark freshness and confidence

Soft temperament should bias, not replace, the hard math:
- greed
- caution
- revenge bias
- opportunism
- territoriality

That way two camps can read the same region differently without turning the model into mush.

V1 does **not** need a deliberate inter-camp coalition layer.
Camps should mostly act from their own maps, operating radii, and pressures.
Occasional overlap is fine.
Routine multi-camp dogpile behavior is not.

### Overmap groups
Camps dispatch temporary abstract groups for specific jobs.
A group should minimally carry:
- group identity
- source camp
- job type
- rough strength
- target or point of interest
- current confidence / threat estimate
- mission urgency
- retreat bias
- mission-relevant remembered marks
- goal stickiness / diversion posture

This is enough to explain why the group exists and what it thinks it is doing.
The group itself should persist across overmap travel and local handoffs.
But that does **not** mean every bandit must be a fully persistent tracked individual.
A good v1 shape is:
- persistent group identity
- at most a small anchored slice of recurring individuals
- fungible remainder for the rest of the membership

Useful identity-continuity rule:
- the player should mostly be re-encountering the same group problem, not drawing a fresh unrelated cast every time the seam toggles
- surviving anchored individuals, current job, recent losses, and current cargo should carry forward when that group reappears

As groups move, they can locally reevaluate effective threat from their surroundings without requiring the entire camp to recompute the whole world every tick.
Their current target should be a provisional mission lead, not a sacred tile commitment.

### Jobs
Initial job vocabulary should likely include:
- hold / chill
- scout
- scavenge
- toll
- stalk
- steal
- probe
- raid
- reinforce

Jobs should differ in:
- required manpower
- travel radius
- acceptable risk
- expected reward profile
- cooldown / load burden
- preferred mark types
- base return clock / outing duration

A camp should not choose a mark directly.
It should score job templates against marks and regional conditions.

That scoring should naturally discourage routine dogpiles through:
- territoriality bias
- distance burden
- depleted bounty after prior exploitation
- sticky threat memory after bad outcomes
- fresh active-pressure penalties when a region is already hot or recently disturbed

It should also avoid dumb revenge loops.
Stalking pressure is good, but groups should still carry a return clock and become more eager to disengage as wounds, panic, and losses accumulate.

---

## Signals, visibility, and concealment

The visibility layer should produce **possible contact locations**, not certainty.
That is the central law.

A mark should tell bandits something like:
- something active may be here
- there may be profit here
- there may be danger here

Not:
- this is definitely the player
- this is definitely a basecamp
- this is definitely free loot

### What can become visible
The concept currently treats all of these as legitimate coarse signal sources:
- player activity
- NPC / human activity
- directly discovered camps or obviously occupied sites
- repeated travel or route use
- city-edge disturbance or urban salvage activity
- smoke
- light
- loud sound

Important principle:
- humans can imply **bounty and threat**
- camps can imply **high bounty and high threat**
- cities can imply **bounty and threat at the same time**

A city is not a free loot field.
It is structurally mixed value.

Useful v1 asymmetry:
- **bounty** may be visible more broadly than **threat**
- terrain/building class, visible light/smoke, and direct human sightings can make a place look worth checking
- real threat sharpness should usually require closer observation, visibility-confirmed scouting, or painful mission history

Baseline awareness should stay asymmetric.
A camp may plausibly begin with rough nearby terrain/building awareness, broadly like an overmap-aware local actor.
But quiet humans, NPCs, and the player should not be immediately revealed as overmap truth just for existing.
Mobile actors should usually enter the camp map through signals, recon observation, close local discovery, or visibly repeated activity.
Smoke and light can create bounty markers.
Quiet inactivity should not.

Useful terrain/bounty split:
- open streets, meadows, and ordinary fields should not print broad bounty by default
- forests may hold a little background bounty
- buildings, houses, and denser urban structures should carry much more structural bounty than empty open ground
- direct human/NPC sightings should be among the strongest mobile bounty cues

### Environmental filters
Signal legibility should be modified by existing physical footing where possible:
- fog/mist can suppress sight and light legibility
- daylight suppresses distant light usefulness
- storms and heavy weather can lower visual confidence and already interact with sound/visibility systems
- wind can alter smoke shape, persistence, and confidence
- terrain and shelter can coarsely reduce legibility

Settled current principle:
- fog/mist do **not** need new special sound-dampening law just because it sounds atmospheric

That matters because the design should reuse real engine hooks rather than inventing a parallel decorative weather system.

## Concealment levers
The player and a basecamp should have understandable ways to reduce exposure:
- fire discipline
- light discipline
- route discipline
- weather exploitation

This is important because the design promise is not merely "bandits notice more things."
It is "the player has survival choices that affect legibility."

## Camp pressure and assault boundary
If a camp becomes legible, bandits should not collapse into either extreme:
- magical invisible camp wipes
- total passivity just because a direct raid looks costly

Sustained pressure is desirable.
That can include:
- scouting
- stalking
- route harassment
- following small outbound NPC groups
- opportunistic attacks when those groups get split, wounded, exhausted, or tangled with hordes
- waiting for camp-side weakness instead of forcing a bad direct assault

Current scope boundary:
- decisive full camp assault should resolve only when the player is present
- offscreen systems can create pressure, delays, interceptions, wounds, small losses, or missing returners, but not an invisible total camp devastation
- attack intent does not need presignaling; fairness here comes from bounded offscreen consequence scope, not courtesy telegraphs

## Existing repo footing for offscreen actors
We do not need to invent offscreen actor existence from nothing.
The current game already has a barebones overmap-NPC substrate, including:
- dormant NPC persistence in `overmap_buffer`
- overmap destinations and pathfinding
- overmap travel for travelling NPCs
- offscreen companion/basecamp NPC tracking

That is enough repo footing for later stalking/intercept slices to reuse persistence, travel, and route-following.
It is **not** enough reason to pretend the current need-driven random-NPC wandering policy is already the final hostile bandit behavior.

---

## Memory, marks, and heatmaps

Bandit camps should not rely on one giant perfect memory list.
They should maintain both:
- **typed marks** for memorable actionable notes
- **broad heatmaps** for fuzzy regional pressure

### Typed marks
Typed marks are remembered notes like:
- smoke seen here
- light seen here
- gunfire heard here
- repeated traffic here
- defended site here
- previous loss site here
- likely loot here
- good ambush road here
- zombie-heavy zone here

Useful v1 fields include:
- mark type
- location
- age
- confidence
- strength
- bounty contribution
- threat contribution
- monster-pressure contribution
- target-coherence subtraction
- soft decay rate
- last refresh turn
- last visibility confirmation turn
- threat_confidence_radius

Typed marks are for things a camp would plausibly remember.
They should be approximate, selectively decaying, refreshable, and mergeable.
Confirmed threat should mostly freeze until later observation rewrites it.

### Heatmaps
Heatmaps are not notes.
They are regional gradients.

### Threat heatmap
Threat can rise from:
- defended human activity
- recent bandit losses
- monster pressure
- uncertainty
- bad retreat geometry
- distance from support

### Bounty heatmap
Bounty can rise from:
- repeated visible activity
- route traffic
- known salvage/scavenge value
- likely supplies
- prior success
- recurring site-centered mixed signals

This split is useful because it lets bandits feel broad regional tension without requiring a million explicit remembered pins.

## Bounty classes
To keep the system from turning into an infinite suspicion fountain, bounty should not be one undifferentiated soup.

### Structural ground bounty
Ground bounty should be coarse, finite, and non-regenerating by region class.
It reflects broadly recoverable value from the place itself, not exact tile loot truth.

Good broad examples:
- low: woods, fields, sparse wilderness
- medium: houses, farms, scattered buildings
- high: dense city blocks, discovered camps, fortified or repeatedly confirmed occupied sites

Bandit exploitation should reduce this ground bounty.
It should not quietly grow back on its own.

### Mobile bounty
Mobile bounty is carried by actors and active sites, not the dirt.
Examples:
- the player current position
- NPC groups
- player camps
- discovered NPC camps or settlements

This layer can move, appear, vanish, or be refreshed by new signals and new observations.

### Route/intercept value
Roads and chokepoints should have value mainly because they are good places to catch moving bounty, not because the road itself endlessly prints treasure.

## Conversion rule
A good synthesis rule is:
- events first disturb local heat pressure
- strong, repeated, or mission-relevant pressure becomes a typed mark

That avoids writing immortal notes for every tiny blip.

## Decay, refresh, and threat freeze
Not everything should stay equally fresh forever.

At short cadence passes:
- transient signal strength weakens
- confidence on stale soft marks drops
- heat spikes smooth down if not reinforced
- empty investigations can add recently-checked / false-lead dampening
- confirmed threat should not passively melt just because time passed

At daily cleanup:
- weak stale soft marks get deleted or collapsed
- lingering bounty/transient regional pressure fades
- strong unresolved pressure can remain as lower-confidence background memory
- harvested areas can keep reduced structural bounty until new mobile activity or new signals justify renewed interest
- serious threat/loss reads should remain sticky unless later observation honestly downgrades them

Repeat signals, scout passes, mission results, and team visibility passes can refresh or rewrite marks.
But camps should not be allowed to become pseudo-psychic by counting their own routine recon traffic as fresh bounty.
This keeps camps intelligent without making them psychic.

---

## Bounty, threat, and job scoring

The scoring layer is where camps turn memory into action.
It should be deterministic.

### Bounty
Bounty answers:

> how worthwhile does this mark or region look right now?

Core influences include:
- expected loot or salvage value
- target weakness
- traffic value
- signal freshness
- camp need bonus
- distance cost

A useful v1 shape is:

```text
bounty_score =
    loot_value
  + target_weakness
  + traffic_value
  + signal_freshness_bonus
  + camp_need_bonus
  - distance_cost
```

### Threat
Threat answers:

> how dangerous does this mark or region look right now?

Core influences include:
- defender strength
- fortification value
- recent losses
- zombie pressure
- uncertainty
- distance burden
- target distraction bonus

A useful v1 shape is:

```text
threat_score =
    defender_strength
  + fortification_value
  + recent_losses_penalty
  + zombie_pressure
  + uncertainty_penalty
  + distance_penalty
  - target_distraction_bonus
```

### Job scoring
The camp then scores possible jobs against marks/regions.
A useful v1 shape is:

```text
job_score =
    bounty_score
  - threat_score
  + temperament_modifiers
  + camp_need_modifiers
  + job_type_bonus
```

A job is valid only if:
- manpower exists
- travel radius is allowed
- cooldown is clear
- job load allows dispatch

Highest valid job wins.

This is where the system stops being a mood board and starts becoming a machine.

---

## Monster pressure and opportunism

Monster pressure is not a one-axis value.
That is one of the most important ideas in the whole packet.

Zombies and chaos can:
- increase ambient danger
- increase uncertainty
- complicate travel and retreat

But they can also:
- distract a target
- wound or tire defenders
- reduce defensive coherence
- create opportunistic attack windows

So the model should preserve both:
- additive environmental danger
- subtractive target coherence

This is what allows bandits to behave opportunistically rather than simply avoiding everything dangerous.
A city fight, horde pressure, or exhausted survivor caravan can be both more dangerous and more attackable.
That tension is good.

Current v1 simplification:
- zombies matter here as threat and target-coherence pressure
- we do **not** need explicit bandit-versus-zombie tactical simulation yet
- if a group decides the opportunity is worth the risk, assume it can sneak or thread through well enough to make the attempt

That means cities may support a risky opportunistic pass.
They should **not** become infinite farming zones.
Structural bounty depletes, threat remains high, and failed or aborted approaches should leave sticky scary memory behind.

---

## Mission lifecycle

The clean mission lifecycle should look like this:

1. **signal or world-state appears**
   - smoke, light, sound, repeated traffic, prior loss, city disturbance, settlement pattern

2. **camp memory updates**
   - local heat shifts
   - typed marks are created or refreshed if thresholds are met

3. **camp strategic tick runs**
   - ledger drifts
   - stale memory decays
   - candidate jobs are scored

4. **group dispatch happens**
   - a valid job spawns an abstract overmap group

5. **overmap travel and reaction happen**
   - the group routes toward a target, observes, stalks, probes, ambushes, or raids
   - en route observations may reinforce, divert, shadow, or abort the current goal if reality changes

6. **bubble handoff happens when relevant**
   - the abstract group becomes local NPC encounter logic in the proper mode, while preserving group continuity and any anchored recurring individuals

7. **local encounter resolves**
   - search, observe, contact, engage, loot, retreat, or break

8. **result packet returns to overmap state**
   - cargo, wounds, morale, losses, revised threat/bounty, new marks, and retreat bias are written back

9. **future behavior changes**
   - the camp becomes greedier, more cautious, more desperate, more revenge-driven, or more informed depending on what happened

That loop is the synthesis backbone.

---

## Overmap-to-bubble seam

The seam between strategic abstraction and local behavior is central.
It must be **bidirectional**.

### Entry side
When a group becomes locally relevant, it should enter with a small explicit payload such as:
- source camp
- job type
- group strength
- current target or mark
- current confidence
- current bounty/threat estimate
- urgency
- panic threshold
- retreat bias
- known recent marks

The overmap layer should transfer:
- why they came
- what they think is true
- how committed they are
- whether the current goal is sticky, fragile, or easy to preempt

It should **not** transfer:
- exact tile truth
- exact inventory truth
- perfect defender counts
- magical certainty
- a fake law that the original destination must be reached no matter what appears on the route

### Handoff modes
The main conceptual modes currently are:
- pursuit / investigation
- ambush
- raid / probe
- retreat / disengage

The cleanest first anchor case remains pursuit/investigation because existing route-to-noise-source footing already exists and gives an honest first bridge.

### Return side
When the encounter ends or cools below relevance, the group should collapse back into overmap state with a result packet such as:
- survivors remaining
- wound level
- panic / morale state
- cargo acquired
- mission result
- updated threat estimate
- updated bounty estimate
- new marks learned
- loss site if any
- retreat destination or home bias

If that return path is missing, then cargo, wounds, panic, losses, and changed understanding all vanish into smoke.
That would be stupid, and worse, it would make the whole system fail to learn.

---

## Cadence and performance model

The design should use a small family of cadence tiers.
Current preferred shape:
- **every 20 minutes** for nearby active relevance
- **every 2 hours** for inactive or distant relevance
- **daily** for drift, cleanup, and memory collapse
- **immediate event creation** for new signals and mission-result marks, followed by later cadence reevaluation

This is the right kind of cheap.
It keeps the world feeling alive without demanding global constant thought.

The strategic tick should broadly do this:
1. apply ledger drift
2. refresh marks/heatmaps, selectively decay soft pressure, and freeze confirmed threat until updated
3. generate candidate jobs from current valid marks/regions
4. compute bounty and threat
5. compute final job desirability
6. discard invalid jobs
7. dispatch a lightweight abstract group if one wins

The mark writer should broadly do this:
1. ingest queued relevant events
2. update heat pressure
3. create or refresh marks if thresholds are crossed
4. decay transient pressure by cadence tier, but only revise threat downward when observation actually rechecks it
5. expose resulting state to the scoring layer

All of this should be inspectable and debug-printable later.
Mystery math is the enemy.

---

## Player-facing gameplay promises

If this concept works, the player should start caring about things like:
- whether a fire is worth the smoke
- whether night lighting is concealed
- whether a repeated route is becoming legible
- whether weather helps hide or expose activity
- whether city chaos is cover or a trap
- whether a camp looks rich but too dangerous to raid, or poor but easy to extort
- whether recent attacks or losses have probably changed enemy caution

The player should not need to know exact formulas.
But the player should feel the system's logic.
That is the standard.

---

## If this ever gets greenlit later

This paper is still parked.
But if the concept is ever reopened for actual implementation planning, the safest first vertical slice remains narrow.

Favored shape:
- fixed bandit camp as a regional actor
- coarse strategic cadence
- one real signal bridge first, likely sound or smoke
- simple scout / investigation job
- route toward a rough point of interest
- narrow pursuit/observe/retreat local handoff
- explicit return packet that writes learning back into camp state

Why this remains the right first slice:
- it proves the overmap actor model
- it proves the signal-to-mark-to-job loop
- it proves the handoff seam
- it proves the return path
- it avoids prematurely solving every raid/settlement/social edge at once

---

## Remaining open questions

The concept is much cleaner now, but some honest questions remain parked.

### 1. Visibility granularity
How coarse should smoke/light/sound detection stay before it becomes too dumb or too expensive?

### 2. Site-interest threshold law
How strongly should repeated site-centered smoke/light/human repetition amplify bounty/confidence before camps start making real approach decisions from it?

### 3. Terrain masking specificity
How much terrain/shelter masking belongs in the concept versus later implementation details?

### 4. Persistence cost
How much bandit memory and group state is affordable across save/load without becoming ugly or expensive?

### 5. Readability versus exploitation
How much ambient pressure readability is enough to feel fair without making stalking and attacks trivial to cheese every time?

### 6. Camp-side responses
How much basecamp-side agency belongs in any future v1 versus later layers?
Patrols, escorts, scouts, decoys, and concealment behavior are all tempting, and all dangerous to bloat.

These are not signs that the packet failed.
They are the remaining honest seams.

---

## Non-goals at this concept stage

This paper is still **not** trying to:
- finalize exact formulas for every case
- define every bandit personality archetype
- solve full local tactical AI
- replace ordinary local NPC perception
- build a perfect whole-world simulation
- greenlight implementation automatically

The point is coherence, not false completion.

---

## Related parked-chain docs

Supporting synthesis docs:
- deterministic scoring guidance: `doc/bandit-bounty-threat-scoring-guidance-2026-04-19.md`
- overmap mark-generation and heatmap model: `doc/bandit-mark-generation-and-heatmap-model-2026-04-19.md`
- bidirectional overmap-to-bubble handoff seam: `doc/bandit-overmap-to-bubble-handoff-seam-2026-04-19.md`
- player/basecamp visibility and concealment: `doc/bandit-player-basecamp-visibility-and-concealment-2026-04-19.md`
- physical-systems recon note: `doc/bandit-visibility-physical-systems-recon-2026-04-19.md`

This paper should be the reader-facing synthesis.
The other docs are the narrower feeder papers that support it.
