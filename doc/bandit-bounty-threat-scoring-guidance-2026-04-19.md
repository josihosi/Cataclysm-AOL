# Bandit bounty/threat scoring guidance v1 (2026-04-19)

## Status

This is a **parked concept-chain guidance doc**, not an implementation greenlight.
Its job is to make the bandit overmap idea more concrete without prematurely opening an active coding lane.

Use it for:
- deterministic scoring design
- later Andi-facing technical guidance
- keeping scoring logic separate from still-open visibility and handoff questions

Do **not** treat this doc by itself as permission to implement the whole bandit system now.

---

## Purpose

Bandit camps need a deterministic way to decide what is worth doing.
This v1 guidance defines two core overmap-side scores:
- **bounty**: how attractive a mark or target is
- **threat**: how dangerous that mark or target looks

The bandit decision layer should not run on vibes, LLM improvisation, or hand-authored one-off encounter magic.
It should score remembered marks, camp state, and job templates deterministically.

---

## Scope

This guidance currently covers:
- camp ledger inputs that drift over time
- mark types and mark decay
- deterministic bounty scoring
- deterministic threat scoring
- simple job desirability scoring
- a clean separation between hard state and temperament modifiers

---

## Explicit non-goals

This guidance does **not** yet define:
- exact smoke/light/sound visibility math
- exact signal-ingestion cadence
- exact overmap-to-bubble handoff rules
- player/basecamp visibility or concealment logic
- detailed local tactical AI
- faction diplomacy systems

Those remain later parked concept items.

---

## Deterministic law

The scoring layer should be deterministic given the same inputs.
That means:
- no LLM interpretation
- no hidden random action choice inside scoring
- no invisible human-authored "because it felt right" jumps

Randomness may still exist later in:
- world generation
- signal appearance
- mission variation
- local combat outcomes

But the overmap scoring pass itself should be repeatable from the same state.

---

## Core data model

### 1. Camp ledger

Each bandit camp should maintain a small deterministic ledger.

Suggested v1 fields:
- `stockpile_food`
- `stockpile_ammo`
- `stockpile_med`
- `food_pressure`
- `ammo_pressure`
- `med_pressure`
- `manpower_available`
- `confidence`
- `aggression`
- `recent_losses`
- `recent_gains`
- `job_load`

### Meaning
- stockpile buckets move by explicit haul, daily camp upkeep, and mission-side provisioning/recovery costs, not by hidden passive decay
- pressure values increase urgency for relevant jobs
- manpower limits which jobs are even valid
- confidence affects willingness to take risks
- recent losses raise perceived threat and caution
- recent gains can reinforce greed/confidence
- job load prevents a camp from acting infinitely busy without cost

---

### 2. Map marks

Each camp should maintain rough remembered marks rather than perfect knowledge.

Suggested v1 fields per mark:
- `mark_type`
- `location`
- `age`
- `confidence`
- `bounty_base`
- `threat_base`
- `soft_decay_rate`
- `last_refresh_turn`
- `last_visibility_confirmation_turn`

### Example mark types
- `smoke_seen`
- `light_seen`
- `gunfire_heard`
- `traveler_route`
- `weak_target`
- `defended_site`
- `zombie_heavy_zone`
- `previous_loss_site`
- `good_ambush_road`
- `likely_loot`

Marks should be:
- approximate
- selectively decaying, not uniformly melting
- refreshable
- mergeable when nearby and same-kind
- usable even when partly stale

Important rule:
- soft bounty/attention components may fade
- confirmed threat should mostly stay frozen until real observation updates it

---

## Hard state vs soft temperament

Keep these separate.

### Hard state
Inputs that should score directly:
- supplies and pressures
- manpower
- recent losses/gains
- mark location and age
- bounty/threat estimates
- job cooldown/load

### Soft temperament
Camp personality knobs that bias choices but do not replace the hard math:
- `greed`
- `caution`
- `revenge_bias`
- `opportunism`
- `territoriality`

This allows two camps to read the same mark differently without turning the model into mush.

---

## Bounty score

## Goal
Bounty answers:

> How worthwhile does this mark look right now?

### Suggested v1 components
- `loot_value`
- `target_weakness`
- `traffic_value`
- `signal_freshness_bonus`
- `camp_need_bonus`
- `distance_cost`

### Suggested formula shape

```text
bounty_score =
    loot_value
  + target_weakness
  + traffic_value
  + signal_freshness_bonus
  + camp_need_bonus
  - distance_cost
```

### Component notes
- `loot_value`: expected material gain, whether goods, supplies, captives, or easy robbery
- `target_weakness`: how exploitable the mark looks
- `traffic_value`: whether a road/route seems worth intercepting repeatedly
- `signal_freshness_bonus`: new information is more actionable than stale rumor
- `camp_need_bonus`: food/ammo/med pressure raises value of matching targets
- `distance_cost`: farther targets cost time, risk, and manpower attention

Useful v1 asymmetry:
- bounty should often be easier to perceive at overmap scale than threat
- terrain/building class, visible light/smoke, and direct human sightings can create promising bounty reads even when the real local danger is still unclear
- smoke/light should therefore usually write a **bounty-first** mark, with stronger threat sharpening later through approach, contact, searchlight-style evidence, patrol behavior, fortification cues, or prior losses
- this is good, because it incentivizes scouting/stalking/exploration instead of giving camps perfect threat truth for free

---

## Threat score

## Goal
Threat answers:

> How dangerous does this mark look right now?

### Suggested v1 components
- `defender_strength`
- `fortification_value`
- `recent_losses_penalty`
- `zombie_pressure`
- `uncertainty_penalty`
- `distance_penalty`
- `target_distraction_bonus`

### Suggested formula shape

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

### Component notes
- `defender_strength`: perceived armed human danger
- `fortification_value`: static difficulty of attacking a location
- `recent_losses_penalty`: camps should remember getting bloodied
- `zombie_pressure`: monsters increase chaos and danger around the mark; v1 does not require explicit bandit-versus-zombie tactical simulation for this
- `uncertainty_penalty`: unknowns are threat, not free mystery meat
- `distance_penalty`: farther operations are harder to support and retreat from
- `target_distraction_bonus`: if the target is already stressed or entangled, effective threat drops

Useful v1 asymmetry:
- threat should usually have a smaller confident radius than bounty
- broad overmap promise can exist without equally broad threat certainty
- later implementation can plausibly reuse the existing overmap-scale hostile/zombie visibility precedent as a rough upper bound for how far confirmed threat should project

---

## Important opportunism rule

Zombie chaos should not only raise ambient danger.
It can also reduce the target's **effective defensive coherence**.

Meaning:
- a strong player/survivor group may still be dangerous in absolute terms
- but if they are wounded, overextended, distracted, or fighting zombies
- bandits may judge them as more attackable

So the model should allow:
- `zombie_pressure` to increase raw environmental danger
- `target_distraction_bonus` to lower effective target threat

Current simplification for v1:
- if bandits decide the opportunity is worth the risk, the model may simply assume they manage the approach well enough to attempt it
- this does not imply persistent city farming, because structural bounty can deplete and bad outcomes should write back sticky threat memory

This tension is good.
It creates opportunistic ambush logic instead of flat power comparison.

---

## Job desirability score

Camps should not pick marks directly.
They should score **jobs** against marks.

### Suggested v1 jobs
- `hold / chill`
- `scout`
- `scavenge`
- `toll`
- `stalk`
- `steal`
- `raid`
- `reinforce`

### Job template fields
Each job type should define:
- `required_min_manpower`
- `preferred_mark_types`
- `max_risk_tolerance`
- `max_travel_radius`
- `reward_profile`
- `cooldown`
- `base_return_clock`

### Suggested formula shape

```text
job_score =
    bounty_score
  - threat_score
  + temperament_modifiers
  + camp_need_modifiers
  + job_type_bonus
  - active_pressure_penalty
```

A job is valid only if:
- manpower is sufficient
- travel radius is allowed
- job cooldown is clear
- current load allows dispatch

`active_pressure_penalty` is where v1 can damp silly multi-camp pile-ons.
If a region already carries fresh disruption, recent failed pressure, or evidence that someone else already made it hot, that should reduce the score for blindly piling in again.

`hold / chill` should be treated as an always-available score-`0` baseline with no mark requirement, no travel, and no dispatch cost.
Highest valid outward job wins only if it scores above that baseline, while ties or worse should collapse to `hold / chill` instead of speculative wandering.
This is still per-camp scoring, not coalition strategy logic.
Occasional overlap can happen, but v1 should not behave like several camps are sharing one omniscient attack planner.

## Important revenge / persistence rule

Stalking pressure is desirable.
Infinite revenge spirals are not.

So revenge bias should mostly do things like:
- keep a painful target region warm in memory longer
- make follow-up stalking/probing more likely than total forgetfulness
- modestly resist immediate total de-prioritization after a bad contact

But revenge bias should **not** override:
- a raid group's return clock
- severe wound accumulation
- panic / morale collapse
- obviously worsening threat math

Good v1 read:
- a bloodied group may stalk, shadow, or take another look later
- the same bloodied group should still become more eager to reroute home as wounds and losses mount
- revenge can help preserve pressure at the camp level even while the specific outing cuts short

---

## Mark refresh and selective pruning

Marks should not all get weaker the same way over time.
And v1 should prefer **overwrite / refresh / selective pruning** over a costly fantasy where every remembered thing is constantly re-decayed in detail.

### Daily drift idea
Once per day, cheap cleanup can:
- lose confidence on weak unsupported soft marks
- gain uncertainty when a soft read was not refreshed
- delete low-value clutter that has been superseded, visibility-cleared, or left unsupported

But:
- moving bounty should be cleared or rewritten by source change, negative recheck, or stronger replacement evidence, not numerically shaved down by an idle timer
- confirmed threat should not passively decay away just because nobody looked
- threat rewrites should stay source-specific: closer harder danger evidence or bad outcomes raise the read, repeated corroboration confirms it, and only close contradictory observation or successful passage lowers it
- the implementation should keep this cheap, with important marks persisting until overwritten or honestly revised rather than numerically melted every tick

### Refresh / rewrite rule
A mark can be refreshed or updated by:
- a repeat signal
- a scout pass
- a successful raid/toll/stalk observation
- a team visibility pass that actually sees the region
- a negative recheck that honestly clears the prior moving-bounty read
- a threat recheck that sharpens, confirms, or lowers one specific danger source
- a new related mark merging into it

Strong v1 default:
- important marks stay until refreshed, overwritten, or visibility-confirmed otherwise
- cheap cadence mostly updates what changed and prunes weak stale clutter
- forgetting should be the exception for low-value noise, not the default behavior for meaningful threat memory

This keeps the bandit map lively without turning it into perfect memory or implausible amnesia.

---

## Camp ledger drift

Daily camp bookkeeping can do things like:
- apply the once-per-day mouths/basic-upkeep stockpile drain
- raise food/ammo/med pressure when those daily costs are hard to cover
- reduce confidence after losses
- reduce aggression if manpower is too low
- raise greed/opportunism pressure when supplies run short
- lower job load as missions end

Mission-side stockpile costs should stay explicit and event-driven:
- dispatch pays ration/ammo/med issue when a group actually leaves
- return pays rearm/treatment/restock when a group comes home having spent them

This is where camps slowly become more desperate, cautious, or bold.

---

## Suggested v1 deterministic sequence

For each camp on its strategic tick:

1. update camp ledger drift
2. decay/refresh marks
3. generate candidate jobs from current valid marks
4. compute bounty score for each job/mark pair
5. compute threat score for each job/mark pair
6. compute final job desirability
7. discard invalid jobs
8. choose highest-scoring valid job
9. dispatch a lightweight abstract group only if the best outward job beats `hold / chill`; otherwise stay home

This whole pass should be deterministic from current stored state.

---

## Implementation guidance shape for Andi (later)

When this parked item is eventually greenlit, the implementation guidance should stay narrow:
- represent camp ledger state explicitly
- represent marks explicitly
- keep formulas visible and inspectable
- prefer small named helper functions over one giant mystery scorer
- make scoring debug-printable
- keep player/basecamp visibility out of the first scoring slice

Suggested helper shape:
- `compute_mark_bounty( camp, mark )`
- `compute_mark_threat( camp, mark )`
- `compute_job_score( camp, job_template, mark )`
- `refresh_or_overwrite_mark( mark, observation )`
- `decay_mark( mark )`
- `apply_daily_camp_drift( camp )`

That makes the math testable instead of mythological.

---

## Good first parked-chain follow-ups

After this scoring guidance, the next parked concept items likely are:
1. exact visibility/signal math
2. signal cadence/persistence model
3. overmap-to-bubble handoff seam
4. player/basecamp visibility and concealment

This order is intentional.
Scoring should not quietly absorb visibility logic that belongs to later slices.

---

## Why this item is parked

This is a concept-enrichment step.
It makes the larger bandit design more concrete and technically legible.
But it should remain parked until the broader bandit concept chain is coherent enough to greenlight as a real implementation backlog item.
