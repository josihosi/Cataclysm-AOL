# Bandit overmap-to-bubble handoff seam v1 (2026-04-19)

## Status

This is a **parked concept-chain guidance doc**, not an implementation greenlight.
Its job is to define how abstract overmap bandit groups enter local play, act long enough to matter, and then collapse back into overmap state with useful consequences carried home.

Use it for:
- designing the overmap-to-bubble seam
- reusing existing routing/pursuit footing where possible
- defining return-path state from local encounters back into overmap abstraction
- keeping the seam separate from player/basecamp visibility work

Do **not** treat this doc by itself as permission to implement the full bandit system.

---

## Purpose

The parked chain now has:
- a broad concept vessel
- deterministic bounty/threat scoring
- overmap mark-generation and heatmap logic

The next missing piece is:

> how an abstract overmap group becomes a local encounter, and how local outcomes get written back into overmap state afterward

This doc defines that seam in both directions.

---

## Scope

This guidance currently covers:
- overmap -> bubble entry modes
- what state crosses the seam on entry
- local mission-state categories that matter for re-abstraction
- bubble -> overmap return-path state
- when a local group should collapse back into abstraction
- reuse of existing pursuit/noise-routing footing where possible

---

## Explicit non-goals

This guidance does **not** yet define:
- player/basecamp visibility and concealment rules
- exact smoke/light/sound math
- exact local combat AI/tactics
- exact encounter balance tuning
- exact cargo itemization rules
- full faction diplomacy behavior

This is a seam doc, not the whole war.

---

## Core principle

The handoff seam is **bidirectional**.

It is not enough to define:
- overmap group enters local play

It must also define:
- what local outcomes survive the return trip back to overmap state

If that return path is not modeled, then cargo, wounds, panic, deaths, and changed threat/bounty knowledge disappear into smoke.
That would be stupid.

---

## Best first anchor case: pursuit / investigation

The cleanest first handoff case is not a raid.
It is **pursuit / investigation**.

Why:
- there is already existing routing-to-noise-source style footing in the game
- it gives an honest first case for: signal -> route -> local pursuit state
- it is narrower and less theatrical than inventing ambush/raid logic from thin air first

So v1 seam design should treat pursuit/investigation as the anchor case and build other modes around that footing.

---

## Handoff modes

### 1. Pursuit / investigation handoff

Use when:
- a signal or mark leads a bandit group toward a point of interest
- the group is trying to inspect, track, or chase rather than attack immediately

Overmap role:
- route abstract group toward rough source/mark
- carry target confidence and urgency

Bubble role:
- instantiate a local group that searches, tracks, or pursues
- resolve immediate local uncertainty, contact, and chase pressure

### 2. Ambush handoff

Use when:
- the group is not simply chasing a noise source
- the group is moving to a favorable intercept or trap position

Overmap role:
- route group toward ambush-worthy region or approach line
- carry intent to conceal, wait, or intercept

Bubble role:
- instantiate local encounter from prepared position rather than direct pursuit

### 3. Raid / probe handoff

Use when:
- the group reaches a specific target region
- the intent is intimidation, probing, theft, or attack

Overmap role:
- select target region and mission weight
- carry job intent and acceptable risk

Bubble role:
- instantiate contact/probe/raid behavior
- resolve whether the group presses, steals, backs off, or gets bloodied

### 4. Retreat / disengage handoff

Use when:
- local danger spikes
- morale breaks
- cargo is sufficient and continued contact is not worth it

Overmap role:
- receive the returning state and route the group away/home

Bubble role:
- let the group break contact and leave local play without pretending the mission vanished

---

## Entry-state payload

When an abstract group becomes local, it should carry a small explicit payload.

Suggested v1 fields:
- `source_camp_id`
- `job_type`
- `group_strength`
- `confidence`
- `panic_threshold`
- `cargo_capacity`
- `current_target_or_mark`
- `current_threat_estimate`
- `current_bounty_estimate`
- `mission_urgency`
- `retreat_bias`
- `known_recent_marks`

This payload should stay small and inspectable.
It is enough to explain why the group is here and what it thinks it is doing.

---

## What the seam should not assume on entry

Do **not** let the overmap layer smuggle in fake certainty.

The local group should not arrive already knowing:
- exact tile truth
- exact inventory truth
- exact defender count unless already honestly learned
- perfect line-of-sight knowledge

The seam should transfer:
- goals
- confidence
- rough target knowledge
- mission posture

Not divine revelation.

---

## Local mission state categories

While the group is local, the seam should track only the mission-state categories needed for re-abstraction later.

Suggested categories:
- `contact_none`
- `contact_searching`
- `contact_tracking`
- `contact_observing`
- `contact_engaged`
- `contact_looting`
- `contact_retreating`
- `contact_broken`

These are not full tactical AI. They are seam-relevant state buckets.

---

## Bubble -> overmap return-path state

When the local encounter ends or drops below relevance, the group should not disappear empty-handed.
It should carry back a result packet.

Suggested v1 return fields:
- `survivors_remaining`
- `wound_level`
- `panic_or_morale_state`
- `cargo_acquired`
- `mission_result`
- `updated_threat_estimate`
- `updated_bounty_estimate`
- `new_marks_learned`
- `loss_site_if_any`
- `retreat_destination_or_home_bias`

---

## Return-path examples

### Cargo acquired
Effects:
- camp supplies/confidence may rise
- local bounty may drop because the area was harvested
- a repeatable-route or rich-target mark may be reinforced

### Wounds taken
Effects:
- manpower available drops
- med pressure rises
- caution rises
- future aggressive jobs may score worse temporarily

### Panic / morale break
Effects:
- local threat memory rises
- retreat/disengage becomes more likely
- the same area may be deprioritized unless revenge bias overrides it later

### Heavy losses
Effects:
- create or strengthen a strong threat/loss mark
- lower confidence
- maybe raise revenge pressure, depending on camp temperament

### Target stronger than expected
Effects:
- update threat upward
- update target confidence downward
- future raids/probes score worse

### Target weaker or richer than expected
Effects:
- update bounty upward
- maybe lower effective threat
- future return missions become more attractive

---

## Collapse-back rules

The seam needs explicit rules for when local actors collapse back into overmap abstraction.

Suggested v1 triggers:
- the group exits the relevant local area
- the group successfully disengages and is no longer in meaningful local contact
- the group completes cargo acquisition and transitions to escape/return
- the group breaks and flees
- the encounter cools below relevance and no immediate re-contact is occurring

This avoids the fake binary of either:
- staying local forever
- or vanishing without consequences

---

## Pursuit anchor reuse

Because existing route-to-noise-source style logic already exists, the pursuit/investigation seam should reuse that footing where possible.

Practical idea:
- use current routing/pursuit machinery as the first honest bridge from overmap interest to local pursuit
- define what extra bandit-specific payload must be layered on top
- avoid pretending the entire seam has to be invented from scratch

This keeps the first handoff mode grounded in real repo footing instead of fantasy architecture.

---

## Deterministic law

The seam itself should be deterministic from stored state.

That means:
- entry payload comes from known overmap state
- return payload comes from known local outcome state
- collapse rules are explicit
- re-abstraction writes back to camp/group state visibly

Randomness may still exist in local outcomes, but the seam rules should not be mush.

---

## Suggested v1 deterministic sequence

### Entry side
1. overmap group selects or receives target/mark
2. overmap group routes toward target region
3. when a handoff trigger is met, build local entry payload
4. instantiate local group in the correct handoff mode

### Exit side
5. local encounter accumulates seam-relevant result state
6. when collapse-back trigger is met, build return payload
7. write return payload into overmap group/camp state
8. route surviving abstract group toward retreat/home/next goal as appropriate

---

## Good implementation guidance shape for later

When this parked item is eventually greenlit, prefer a small explicit seam contract.

Suggested helper shape:
- `build_local_entry_payload( overmap_group )`
- `instantiate_local_group_from_payload( payload )`
- `record_local_mission_state( group_state )`
- `build_return_payload_from_local_result( local_group )`
- `apply_return_payload_to_overmap_state( payload, camp_or_group )`
- `should_collapse_back_to_overmap( local_group )`

Keep it inspectable.
Do not hide the seam in six unrelated systems and call that design.

---

## Suggested relation to the parked chain

This doc sits downstream of:
- scoring guidance
- mark-generation and heatmap guidance

Because those explain:
- why a group chooses a mission
- what information shaped that mission

This doc explains:
- how that mission enters local play
- how it returns to overmap state afterward

Related parked-chain sub-items:
- `doc/bandit-bounty-threat-scoring-guidance-2026-04-19.md`
- `doc/bandit-mark-generation-and-heatmap-model-2026-04-19.md`

---

## Good next parked-chain follow-up after this

After this item, the next strongest parked concept item is:
- player/basecamp visibility and concealment

That should sit on top of:
- scoring
- heatmaps/marks
- the bidirectional seam

Then the top-level concept paper can be rewritten as the cleaner chemistry paper later.

---

## Why this item is parked

This is concept-enrichment work.
It makes the bandit packet more concrete and implementation-legible.
But it should remain parked until the whole bandit concept chain is coherent enough to reconsider for actual greenlight.
