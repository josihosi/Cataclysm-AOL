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
- `group_id`
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
- `goal_stickiness`
- `goal_preemption_posture`
- `return_clock`
- `anchored_identity_slots`
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
- whether the current goal is sticky, fragile, or easy to preempt
- any anchored recurring identities that should survive the handoff

Not divine revelation.

## Identity continuity across the seam
The overmap group itself should be the primary persistent actor.
That continuity is mandatory.

But v1 should **not** require every individual bandit to exist as a fully persistent tracked person across every overmap step and every handoff.
That would bloat the system for little gain.

The better split is:
- persistent **group continuity**
- a small number of **anchored individuals** that survive handoffs directly
- fungible remainder for the rest of the group

Good v1 anchors are things like:
- the group leader
- a bandit the player has encountered or recognized before
- a mission-special actor worth remembering

Good v1 cap:
- roughly **1 to 3 anchored individuals** per active group

So if the player kills or wounds an anchored individual, that exact result can survive the return trip.
If an unnamed grunt dies, the return path can usually treat that as reduced strength or generic casualty instead of demanding full biography continuity.

Useful continuity rule:
- the player should mostly be re-meeting the same **group problem**, not rolling a fresh unrelated encounter every time the seam toggles
- the group should carry forward its source camp, current job, surviving anchored faces, fresh losses, current cargo, and recent target memory
- anchored individuals should reappear on later handoffs when they honestly survived and remained attached to that group
- replacement of dead fungible members is fine later, but it should read as the same bandit outfit limping home, regrouping, and returning, not as instant total cast amnesia

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

## Goal sustainability and preemption
The current target or mark should be treated as a **mission lead**, not an inviolable tile oath.
If the group is moving toward smoke, a mark, or a suspected victim site, local observations on the route should be allowed to revise that goal.

Good v1 outcomes are:
- **continue**: the current lead still looks worth following
- **probe**: the current lead is still nearby/plausible, but one bounded close look is needed before either committing or clearing it
- **shadow / delay**: a live moving target is promising, but patience is smarter than charging now
- **divert**: a materially better immediate prey/opportunity appears on the same route or immediate intercept envelope
- **abort / reroute**: route danger, zombie pressure, burdened return-clock pressure, or collapsing confidence make the current lead not worth forcing

This is where goal sustainability lives.
Goals should persist long enough to matter, but not so rigidly that the group behaves like a missile homing on stale smoke.
These outcomes should all spend the same outing's remaining movement and leash. None of them should silently mint a fresh mission clock or a broad off-route strategic replan.

Useful v1 inputs:
- current goal value
- current goal confidence
- prey quality on sight
- local zombie pressure
- route safety
- mission posture
- retreat bias

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
- `anchored_identity_updates`

Follow-through persistence boundary note:
- later micro-item 28 now freezes save/load as **camp ledger + mark ledger + active abstract group state + bounded anchored identities + carried cargo/burden/leash + minimal bubble-owned join keys**
- exact loaded-bubble truth such as precise NPC positions, HP, inventories, and current combat state should stay in the ordinary game save rather than being duplicated in a second bandit schema
- once a bubble return packet is applied, keep the resulting durable camp/group/mark state, not a second ghost copy of already-consumed deltas

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

## Mission leash / return clock

Stalking is good.
Endless local fixation is not.

Raid, probe, or stalking groups should carry a **return clock**.
That clock says how long the current outing remains worth extending before the group reroutes toward home or safe withdrawal.

Good v1 behavior:
- groups may stalk, shadow, or circle a target for a while instead of charging immediately
- wounds, morale damage, cargo burden, and mounting local danger should shorten the remaining clock
- good local opportunity may consume some clock while still justifying continued pressure
- when the clock expires, the default should be reroute home or disengage, not immortal orbiting

This gives v1 a nice middle ground:
- not goldfish bandits who forget immediately
- not Terminator raiders who pursue forever because revenge exists

## Collapse-back rules

The seam needs explicit rules for when local actors collapse back into overmap abstraction.

Suggested v1 triggers:
- the group exits the relevant local area
- the group successfully disengages and is no longer in meaningful local contact
- the group completes cargo acquisition and transitions to escape/return
- the group's return clock expires and it reroutes home or to safe disengagement
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
6. local observations may continue, divert, shadow, or abort the current mission lead
7. when collapse-back trigger is met, build return payload
8. write return payload into overmap group/camp state
9. route surviving abstract group toward retreat/home/next goal as appropriate

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
