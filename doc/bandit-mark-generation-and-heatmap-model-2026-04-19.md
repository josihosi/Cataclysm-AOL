# Bandit overmap mark-generation and heatmap model v1 (2026-04-19)

## Status

This is a **parked concept-chain guidance doc**, not an implementation greenlight.
Its job is to define how bandit camps create, refresh, decay, and interpret overmap-side marks and broad threat/bounty heatmaps before any local bubble seam is designed.

Use it for:
- overmap-only signal/mark generation design
- threat and bounty heatmap design
- later technical guidance for implementation planning
- keeping mark creation separate from later handoff and visibility-to-player work

Do **not** treat this doc by itself as permission to implement the full bandit system.

---

## Purpose

The bandit concept now has:
- a broad overmap concept vessel
- deterministic bounty/threat scoring logic

The next missing piece is:

> how marks get created, refreshed, decayed, and folded into overmap-side threat and bounty fields in the first place

This doc defines that missing writer-side logic.

---

## Scope

This guidance currently covers:
- overmap-only mark generation
- broad threat and bounty heatmaps
- mark refresh, selective decay, and threat freezing
- additive and subtractive threat contributions from players, monsters, and remembered events
- the cadence family for mark and heatmap upkeep
- the distinction between typed marks and broad heat pressure

---

## Explicit non-goals

This guidance does **not** yet define:
- exact overmap-to-bubble handoff rules
- player/basecamp visibility and concealment logic
- local NPC sight/tactics/combat behavior
- exact local encounter generation
- diplomacy or social negotiation systems

This is strictly an **overmap-side** concept item.

---

## Overmap-only law

Everything in this doc should live on the overmap layer alone.
That means:
- camps observe or remember overmap-relevant inputs
- marks and heatmaps are updated without local tactical simulation
- no bubble-side certainty is assumed here
- no local NPC tactical behavior is defined here

This doc is about what camps *think they know* over broad space, not what a nearby NPC sees tile-by-tile.

---

## Core mental model

Bandits should not rely on a single list of marks.
They should maintain both:

### 1. Typed marks
Discrete remembered notes such as:
- smoke seen here
- gunfire heard here
- defended site here
- previous loss here
- likely traffic route here
- weak target here

### 2. Broad heatmaps
Continuous-ish overmap-side gradients for:
- **threat**
- **bounty**

Heatmaps are the broad pressure field.
Marks are the memorable pins written on top of that field.

This split gives a better result than either system alone.

---

## Heatmap categories

### Threat heatmap
Represents how dangerous a region feels to bandits.

Threat can come from:
- player/survivor activity
- armed groups
- defended camps/settlements
- prior bandit losses
- dense monster pressure
- uncertain zones
- bad retreat geometry or long distance from support

### Bounty heatmap
Represents how attractive or profitable a region feels to bandits.

Bounty can come from:
- visible or inferred supplies
- repeated route traffic
- weak settlements
- smoke/light/noise implying human activity
- known scavenging value
- easy robbery opportunity

Useful v1 terrain split:
- ordinary open streets, meadows, and fields should contribute little or no structural bounty by themselves
- forests may contribute a little background bounty
- houses, buildings, and denser urban structure should contribute meaningfully more structural bounty than open empty terrain
- direct human/NPC sightings should inject strong mobile bounty even when threat is still uncertain

---

## Important monster-pressure rule

Monster pressure should not be treated as only one thing.
It can affect threat in two directions.

### Additive threat
Monsters increase ambient danger:
- more chaos
- more uncertainty
- harder movement
- harder retreat

### Subtractive target coherence
Monsters can reduce how coherent or prepared a target seems:
- distracted by zombies
- wounded or exhausted
- forced into noise/light/fire decisions
- ambushable while already busy surviving

So the system should allow both:
- `monster_pressure_add` to raise environmental threat
- `target_coherence_subtract` to lower effective target resistance

This matches the earlier opportunism rule instead of flattening it into one number.

Current v1 simplification:
- zombies contribute to scoring and mark shape here
- they do **not** require explicit bandit-versus-zombie tactical simulation yet
- if a bandit group accepts the risk, the model can simply assume they manage the approach well enough to attempt the opportunistic action

---

## Cadence family

This should use the same coarse cadence family as the rest of the overmap actor concept.

### Nearby active relevance
- every **20 minutes**

Use for:
- active nearby groups
- hot marks under current interest
- refreshing strong local signal zones

### Inactive / distant relevance
- every **2 hours**

Use for:
- distant inactive groups
- stale regional fields
- low-priority mark aging

### Daily drift / cleanup
- once per **day**

Use for:
- pressure decay
- confidence aging
- mark deletion/collapse
- storage/consumption drift
- long-horizon cleanup

### Event-driven creation
Signals or events may create a mark immediately, but they should still be re-evaluated later on the cadence family above.

This keeps the model responsive without forcing constant global simulation.

---

## What creates marks

Marks should be created from events or accumulated field pressure.

### Signal-driven marks
Examples:
- smoke
- light
- sound

These create attention marks when they cross some meaningful overmap threshold.

### World-state marks
Examples:
- known road traffic
- known defended place
- prior successful robbery site
- prior bandit loss site
- known zombie-heavy zone

### Mission-result marks
Examples:
- target was stronger than expected
- target was rich
- route was good for ambush
- route was a waste of time
- city approach was too dangerous
- horde-choked route caused retreat

These should refresh or alter later scoring instead of disappearing into history fog.

---

## Typed mark model

Suggested v1 fields per mark:
- `mark_type`
- `location`
- `strength`
- `confidence`
- `age`
- `last_refresh_turn`
- `threat_add`
- `bounty_add`
- `monster_pressure_add`
- `target_coherence_subtract`
- `decay_rate`

### Notes
- `strength` is how intense/recent the source feels
- `confidence` is how much the camp trusts the mark
- `threat_add` and `bounty_add` can feed the heatmaps
- `monster_pressure_add` and `target_coherence_subtract` preserve the two-sided monster rule cleanly

---

## Heatmap update model

Heatmaps should be affected by both discrete marks and coarse regional conditions.

### Threat heatmap update
Threat should rise from:
- defended/armed activity
- recent losses
- monster pressure
- uncertainty
- long distance from camp support

Threat should **not** passively melt just because time passed and nobody looked.
Confirmed threat should mostly freeze until later observation updates it.
It should also usually project with a **shorter confident radius** than bounty.

Threat should fall mainly when:
- scouts or travelling groups get visibility-confirmed evidence that danger is lower
- a close revisit or successful passage downgrades the area honestly
- uncertainty resolves downward through real observation rather than idle forgetting

### Bounty heatmap update
Bounty should rise from:
- repeated activity
- likely supplies
- strong signal evidence of humans/resources
- previous success
- route traffic

Bounty should fall when:
- marks go stale
- targets are already harvested/emptied
- repeated failures suggest low value
- camps no longer need that resource as urgently

---

## Typed marks vs heatmaps

### Heatmaps are for gradients
Use heatmaps when the information is broad, regional, or fuzzy.
Examples:
- this valley feels risky
- this road region feels lucrative
- this town edge is monster-heavy

### Typed marks are for remembered notes
Use marks when the information is memorable or action-shaping.
Examples:
- strong smoke seen here
- we lost people here
- repeated gunfire heard here
- reliable ambush road here

Typed marks should stay more conservative than broad heatmaps.
Heat can blur neighboring pressure.
Typed marks should preserve distinct practical stories when the signals may belong to different roads, camps, city edges, or actors.

### Conversion rule
A useful principle:
- immediate events update local heatmaps first
- events become typed marks when they are strong enough, repeated enough, or mission-relevant enough

This avoids writing a permanent note for every tiny blip.

---

## Refresh model

Marks and heatmaps should refresh from:
- new matching signals
- scout passes
- job outcomes
- nearby repeated activity
- repeated heat buildup crossing a threshold

But refresh should still respect provenance.
A camp should not treat its own routine recon traffic, its own logistics churn, or the aftermath of its own uninteresting local movement as fresh hostile-contact evidence by default.
Self-generated activity should only reinforce a mark when later evidence says it really revealed something external or mission-relevant.

### Merge rule
Nearby marks of the same type may merge when:
- their locations are close enough
- their ages are compatible
- they reflect the same practical point of interest

Do **not** merge just because two hot things are nearby on the overmap.
If the signals could plausibly represent different actors, different approach corridors, different camp footprints, or different city-edge trouble, they should remain separate typed marks even if the surrounding heatmaps blur together.

Merged marks should generally:
- increase strength
- reset freshness somewhat
- raise confidence modestly

---

## Decay and freeze model

Not everything should behave the same way over time.
Transient signal and bounty can fade.
Confirmed threat should mostly freeze until updated.

Preferred v1 principle:
- do **not** build an expensive always-melting world map where every mark is continuously re-decayed just to feel alive
- strong threat/loss memory should usually just **stay on the map** until overwritten or visibility-confirmed otherwise
- cheap cadence passes should mostly refresh, rewrite, smooth weak soft pressure, and prune obvious clutter

That keeps the system computationally sane and avoids goldfish bandits who repeat the same mistakes because passive forgetting ate their memory.

### Short cadence update
At the 20-minute and 2-hour passes:
- ingest queued events and refresh or overwrite affected marks
- reduce transient signal strength only for soft, weak, unreinforced pressure
- weaken confidence on unrefreshed soft marks
- smooth unconfirmed heat spikes downward if not reinforced
- do **not** cheaply remote-rewrite a strong recent threat/loss mark into safety just because time passed

### Daily cleanup
At daily cleanup:
- remove weak stale soft marks
- reduce broad bounty residues and transient attention clutter
- convert unresolved but unconfirmed pressure into lower-confidence background memory when appropriate
- keep serious threat/loss memory sticky unless later close-range experience genuinely downgrades it

This should stay cheap and tiered.
The point is not to numerically babysit every remembered mark every pass.
The point is to stop clutter from growing forever while letting important reads persist until replaced or honestly rechecked.

### Visibility-confirmed threat updates
Threat should be updated by what bandit groups actually see inside their practical visibility bubble, not by passive forgetting.
If a team gets line-of-travel or close-range confirmation that tiles/areas are safer or still dangerous, that observation should rewrite the local threat state.
If nobody looked, the scary read should mostly stay frozen.

### Proximity-gated reevaluation
If a group leaves an area because it read as too dangerous, that area should remain marked as scary.
Meaningful reevaluation should usually require:
- a close revisit
- a successful later passage
- a scout/team visibility pass that actually sees the area
- or some other attractive nearby mark that pulls the group back into local contact

That keeps cities and other failed-risk zones from being remotely recalculated into safe jackpot territory every few ticks.

This keeps the system from turning into immortal clutter without turning it into amnesia either.

---

## Suggested v1 deterministic sequence

For each relevant cadence pass:

1. ingest any immediate new events already queued for overmap processing
2. update local heatmap cells/regions from those events
3. create or refresh typed marks if thresholds are met
4. decay transient signal/bounty pressure based on age and cadence tier
5. preserve or revise threat state based on actual visibility-confirmed observation, not passive time alone
6. collapse or delete stale weak soft marks
7. expose the resulting marks/heat values to the bounty/threat scoring layer

This whole sequence should be deterministic from stored state.

---

## Technical guidance shape for later implementation

When this parked item is eventually greenlit, keep the implementation inspectable.

Suggested helper shape:
- `emit_mark_signal( source_event )`
- `apply_mark_to_heatmaps( mark )`
- `refresh_or_create_mark( event_or_signal )`
- `overwrite_or_confirm_mark( mark, new_observation )`
- `decay_soft_mark( mark, cadence_tier )`
- `update_heatmaps_from_visibility_confirmation( camp_or_region, scout_or_group )`
- `decay_bounty_and_transient_pressure( camp_or_region, cadence_tier )`
- `collapse_heat_to_mark_if_needed( region )`

Avoid one giant opaque updater.
Keep the arithmetic visible enough to debug and test later.

---

## Suggested relation to the scoring packet

This doc is upstream of scoring.

Relationship:
- this doc explains how marks/heatmaps come into existence
- the scoring doc explains how camps score those marks/fields into job choices

Do not quietly mix the two again.
The separation is useful.

Related parked-chain sub-item:
- `doc/bandit-bounty-threat-scoring-guidance-2026-04-19.md`

---

## Related parked-chain follow-up

The next parked-chain follow-up after this was the bidirectional seam doc:
- `doc/bandit-overmap-to-bubble-handoff-seam-2026-04-19.md`

That ordering was intentional:
- marks exist
- heatmaps exist
- scoring exists
- then the handoff can be designed on top of something real instead of vapor

---

## Why this item is parked

This is concept-enrichment work.
It makes the bandit packet more concrete and implementation-legible.
But it should remain parked until the whole bandit concept chain is coherent enough to reconsider for actual greenlight.
