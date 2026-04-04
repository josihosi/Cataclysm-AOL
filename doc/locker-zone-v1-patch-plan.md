# Locker Zone v1 patch plan

This document is the detailed auxiliary patch plan for the greenlit Locker Zone v1 feature in `Plan.md`.
It is intentionally implementation-shaped.
The goal is to make future work concrete enough that the cron/workloop can advance it without rediscovering the same design every night.

Status: **greenlit design, not the current top queue item**.
Current queue still finishes the active Basecamp bark/feel pass first.

---

## Product goal

Locker Zone v1 is a **camp-managed loadout maintenance system**.
It is not fashion AI and not a full military quartermaster simulation.

Desired player-facing behavior:
- The player defines a **Zone Manager locker zone** containing approved loadout items.
- The player sets a simple **camp-wide locker policy** for which categories NPCs should maintain.
- Camp NPCs quietly re-check their loadout over time, one at a time, instead of all stampeding at once.
- NPCs keep one best item for each enabled category, remove duplicates, and equip obvious upgrades from the locker zone.
- Human-facing bark stays light/informal; no giant equipment report is needed for v1.

Desired system behavior:
- low churn
- deterministic selection
- no whole-camp dogpile on the same item
- no giant overlap solver
- no ammo/magazine logistics yet

---

## Explicit v1 scope

### Included in v1
- Zone Manager locker zone type
- camp-wide locker policy
- one best item per managed category
- duplicate cleanup for managed categories
- simple category-specific score logic
- wake-up dirty + sequential service queue
- temporary item reservation while one NPC is being serviced
- melee weapon slot
- ranged weapon slot (selection only, not reload logistics)
- deterministic tests for category selection, queueing, and duplicate cleanup

### Not included in v1
- seasonal loadouts
- per-NPC overrides
- role presets
- advanced layering / combinatorial outfit solving
- full ammo/magazine/reload maintenance
- job-launch logic through the locker system
- trying to classify every weird wearable in the game

If an item is weird or ambiguous, v1 should skip it rather than be confidently wrong.

---

## Managed categories and v1 classifier basis

The v1 categories are intentionally fixed and boring:
- underwear
- socks
- shoes
- pants
- shirt
- vest
- body armor
- helmet
- glasses
- bag
- melee weapon
- ranged weapon

### Classifier strategy
Use **metadata-first, ordered first-match-wins classification**.
Do not use name/string matching as the primary mechanism.
Use item metadata such as:
- `is_armor()` / `is_gun()` / melee weapon checks
- covered body parts
- flags such as `SKINTIGHT`, `OUTER`, `BELTED`, `POCKETS`
- storage values
- armor/protection profile

Use a small explicit override/skip table only for weird exceptions.
Ambiguous items become `unmanaged` in v1.

### Proposed v1 classifier table

#### socks
Match if item:
- is armor
- covers both feet
- is `SKINTIGHT`
- does not also look like full clothing/armor

#### shoes
Match if item:
- is armor
- covers both feet
- is not `SKINTIGHT`
- is clearly footwear rather than socks

#### underwear
Match if item:
- is armor
- is `SKINTIGHT`
- covers pelvis / underlayer area
- is not foot-only and not head-only

#### pants
Match if item:
- is armor
- covers both legs
- is not `SKINTIGHT`
- is not obviously a full-body weird suit

#### shirt
Match if item:
- is armor
- covers torso
- is not body armor
- is not `BELTED`
- is not bag-like storage gear
- is not a vest-only torso piece

#### vest
Match if item:
- is armor
- covers torso
- is torso-focused with limited/no arm coverage
- is not bag-like
- is not body armor

#### body armor
Match if item:
- is armor
- covers torso
- has protection/encumbrance profile more like armor than clothing
- is not bag-like

#### helmet
Match if item:
- is armor
- covers head
- is not just eyewear
- helmet liners / odd underlayers may be skipped in v1

#### glasses
Match if item:
- is armor
- covers eyes / eyewear slot
- is not broad head armor

#### bag
Match if item:
- is armor
- is `BELTED` or back-hanging torso gear
- is storage-first equipment
- is not body armor

#### melee weapon
Match if item:
- is a weapon
- is not a gun

#### ranged weapon
Match if item:
- is a gun

### Explicit v1 simplification
Ignore overlap for now.
If an item could plausibly be both bag and armor or both shirt and full-body nonsense, skip it.
We are not solving onesies in v1.

---

## Trigger and service model

### Intended feel
This should be low-key camp maintenance.
Not a military muster.
The intended vibe is: people wake up, notice they need to sort their gear out, and the camp processes that gradually.

### Trigger model
NPCs become **locker-dirty** when:
- they wake up / start the day
- they are missing a managed category
- the player changes locker policy
- the locker zone gains new eligible gear
- they lose/drop important managed gear

### Service model
Do not let all NPCs process immediately.
Use a camp-level **locker service queue**:
- dirty NPC enters queue
- one NPC at a time is serviced
- if nothing changes, the pass is quick/cheap
- if swaps are needed, it takes longer
- then the next NPC gets serviced

### Reservation model
To avoid dogpiles on the same best item:
- when an NPC begins a locker pass, selected candidate items become temporarily reserved
- later NPCs treat reserved items as unavailable
- reservation clears on success, failure, or timeout

### Priority inside queue
Suggested v1 order:
1. missing essential gear
2. invalid/duplicate managed gear state
3. obvious upgrade available
4. routine wake-up maintenance

This keeps “no shoes” above “could maybe upgrade jeans.”

---

## Player control / UI

### v1 policy shape
Use one **camp-wide locker policy**.
The player should be able to enable/disable broad categories such as:
- armor
- bag
- ranged weapon
- glasses
- etc.

The zone contents remain the actual supply source.
So:
- policy = what kinds of gear are allowed
- locker zone = what concrete gear exists

### UI placement
This can be implemented as a simple Basecamp configuration/menu entry.
It does not need a giant new UI.
A checkbox list is enough for v1.

### Serialization
Store this policy in camp/basecamp state, not as a temporary menu variable.
It should survive save/load.

---

## Better-item evaluation (v1)

Do not attempt a universal “best item in CDDA” function.
Use simple per-category scores.

### Suggested score principles

#### socks / shoes
Favor:
- valid category match
- adequate coverage
- warmth
- low encumbrance
- condition

#### underwear
Favor:
- valid category match
- basic coverage
- low encumbrance
- condition

#### pants / shirt / vest
Favor:
- coverage
- useful protection
- warmth (lightly)
- storage (lightly/moderately)
- low encumbrance
- condition

#### body armor
Favor:
- protection / coverage strongly
- condition
- accept some encumbrance, but not absurd amounts

#### helmet
Favor:
- protection
- condition
- acceptable encumbrance

#### glasses
Mostly binary utility choice.
Pick one valid eyewear item if enabled.

#### bag
Favor:
- storage strongly
- condition
- acceptable encumbrance

#### melee weapon
Favor:
- obvious usability
- damage / quality
- condition
- avoid junk that an NPC should clearly not prefer

#### ranged weapon
For v1, keep this simple:
- pick based on obvious gun usefulness
- do not yet solve full ammo-readiness
- if the weapon is clearly unusable or nonsense, skip it

### Upgrade threshold
Use a small “meaningfully better” threshold.
Do not churn because one item is microscopically better on paper.

---

## Execution flow for one locker pass

For one NPC chosen from the queue:

1. Build the enabled category set from camp policy.
2. Read current worn/wielded loadout.
3. Classify currently equipped managed items.
4. Read locker-zone candidate items.
5. Classify eligible candidates.
6. Reserve the best candidate items for this NPC.
7. For each managed category:
   - if multiple current items exist, keep the best and schedule the rest for removal
   - if missing category, equip best candidate if one exists
   - if current item exists but a meaningfully better candidate exists, schedule upgrade
8. Execute removals/swaps/equips.
9. Return displaced managed items to locker storage if possible; otherwise use sane fallback handling.
10. Clear dirty state / release reservations / set next maintenance state.

### No-op case
If the NPC already satisfies policy, the pass should finish quickly and clear dirty without churn.

---

## Real code implementation outline

This section is intentionally concrete enough for future work.
Exact file names may drift, but these are the likely touchpoints.

### Patch 1 — data model and zone plumbing
Goal: create the locker concept in code without behavior yet.

Likely work:
- add a new Zone Manager zone type for locker supply
- add camp/basecamp locker policy data structure
- add save/load serialization for locker policy
- add simple UI/menu entry for toggling categories

Likely touchpoints:
- zone manager / zone type definitions
- basecamp/camp data structures
- basecamp UI/menu code

Acceptance:
- player can define a locker zone
- player can enable/disable categories
- policy survives save/load

### Patch 2 — classifier and candidate enumeration
Goal: make the game understand what counts as socks/shoes/etc.

Likely work:
- add `locker_slot` enum
- add `classify_locker_item( const item & )`
- add helper to scan locker-zone items and bucket them by slot
- add `unmanaged` fallback

Acceptance:
- representative test items classify correctly
- weird overlap items are skipped, not misclassified

### Patch 3 — score helpers
Goal: choose obvious upgrades deterministically.

Likely work:
- add per-category score helpers
- add "meaningfully better" threshold logic
- add candidate comparison logic

Acceptance:
- obvious upgrades beat weak items
- tiny differences do not cause churn

### Patch 4 — NPC locker state and queue
Goal: stop all-NPC dogpiles.

Likely work:
- add dirty flag / last-pass state to NPC or camp-maintenance state
- add camp locker service queue
- add one-at-a-time service selection
- add temporary reservation of chosen items

Acceptance:
- multiple dirty NPCs do not all service simultaneously
- item reservation prevents duplicate target selection during one pass window

### Patch 5 — execution of a locker pass
Goal: actually swap/equip/clean up gear.

Likely work:
- inspect current worn/wielded managed items
- remove duplicates
- equip missing categories
- perform obvious upgrades
- return replaced managed items to storage when practical

Acceptance:
- NPC can go from bad/simple outfit to better locker-approved one
- duplicate managed items are cleaned up
- no-op pass leaves a good outfit alone

### Patch 6 — wake-up dirty and maintenance timing
Goal: hook the feature into believable camp rhythm.

Likely work:
- mark NPCs dirty on wake-up/daily cycle
- support additional dirty triggers for lost gear / changed policy / new items
- integrate with existing NPC/camp update loop

Acceptance:
- wake-up dirty works
- sequential maintenance works over time
- no constant reequip spam

### Patch 7 — tests and smoke packet
Goal: make the feature trustworthy before fancy extensions.

Deterministic tests should cover:
- classifier table representative items
- duplicate cleanup
- upgrade choice
- disabled category policy
- dirty queue ordering
- one-at-a-time service
- reservation behavior
- no-op pass behavior

Later smoke packet should cover:
- one NPC improves outfit from locker zone
- multiple NPCs do not dogpile the same gear
- policy toggles change outcomes

---

## Known awkward edges to consciously ignore in v1

Do not let these balloon the patch:
- onesies / full-body weird suits
- blanket wearables
- exoskeleton oddities
- highly nuanced armor + bag + vest conflict solving
- advanced ammo/magazine logic
- personal fashion or role-based taste

If something is weird, let v1 skip it.
That is the right call.

---

## V2 / V3 parked follow-ups

### V2 parked
Ranged readiness and locker-based reload support:
- pick up to two compatible magazines
- reload from locker-zone ammo supply
- basic ranged-readiness logic

### V3 parked
Seasonal dressing and per-NPC nuance:
- winter vs summer preferences
- coats/blankets/shorts logic
- per-NPC overrides

These stay parked until Locker Zone v1 survives real play.

---

## Greenlight summary

Locker Zone v1 is green because the broad idea and the important implementation details are now stable enough:
- zone type is defined
- categories are defined
- trigger rhythm is defined
- dogpile prevention is defined
- scoring direction is defined
- explicit non-goals are defined
- tests are known in advance

The remaining uncertainty is implementation detail, not feature identity.
That is exactly where a greenlit plan should be.
