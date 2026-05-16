# Locker / basecamp follow-through work packages (2026-04-07)

This document turns the current McWilliams debug pass into a controlled follow-through packet.

The point is not to unleash one giant locker/basecamp rewrite.
The point is to preserve the now-working loop, keep the harness on the right save, and give Andi one coherent slice at a time.

## Operating rule

- **One package at a time.**
- Do not opportunistically blend packages just because they smell related.
- Revalidate the current loop before moving to the next package.
- If one package reveals a larger design problem, stop and report instead of silently widening scope.

## Current baseline

- Ordinary chat/ambient harness footing now points at the captured `McWilliams` / `Zoraida Vick` save instead of the older Sandy Creek default.
- The debug-intake pass produced a mix of:
  - one harness polish slice,
  - one basecamp toolcall-routing slice,
  - one locker outfit-hardening slice,
  - one locker policy/control-surface slice,
  - one deliberately scoped carried-item dump slice.
- Patrol sanity on the current McWilliams save already checks out: the visible patrol tiles currently resolve to **2 clusters** under 4-way connectivity.

## Package order

Default order:
1. **Harness zone-manager save-path polish**
2. **Basecamp toolcall routing fix**
3. **Locker outfit engine hardening**
4. **Locker zone policy + control-surface cleanup**
5. **Basecamp carried-item dump lane**

Current explicit override:
- Josef has now greenlit **Package 5** directly as the active lane.
- **Package 4** remains a known follow-through slice and is also explicitly greenlit backlog, but it is not the current queue.
- still do **one package at a time**; this is a conscious reprioritization, not permission to blend 4 and 5 into one blob.

Why the original order existed:
- Package 1 makes the repro/tooling packet less flaky.
- Package 2 fixes the separate wrong-snapshot bug before broader feature churn.
- Package 3 addresses the ugliest visible locker nonsense.
- Package 4 adds player control and explicit locker policy after the engine is less chaotic.
- Package 5 is the first intentionally scoped inventory/support expansion, not accidental soup.

---

## Package 1. Harness zone-manager save-path polish

### Problem
The zone-setting harness flow likely mangles the typed zone name and may fail to save zone settings when returning from Zone Manager to gameplay.

### Inputs from testing
- probable lane: locker-zone flow on McWilliams
- the custom zone name must be entered at creation time, not later through a separate edit-name pass
- closeout after returning to the zone list is a single `Esc` to open the save prompt, then uppercase `Y`
- the landed McWilliams screenshot packet also reopens Zone Manager after returning to gameplay and still sees `Probe Locker`

### Deliverable
Make the harness zone-setting flow reliably:
1. enter the intended zone name cleanly
2. return from Zone Manager to gameplay
3. confirm saving changes when prompted
4. leave screenshots/artifacts that prove the exact sequence

### Acceptance bar
- reproduced on the current McWilliams harness path
- screenshots at each relevant menu transition
- zone name no longer comes out mangled
- created/edited zone persists after returning to gameplay

### Out of scope
- locker policy semantics
- outfit logic
- inventory logic

---

## Package 2. Basecamp toolcall routing fix

### Problem
The wrong snapshot/toolcall lane may be triggered for basecamp NPCs.
A naive location-only heuristic is not acceptable because followers can also stand inside basecamp temporarily.

### Requirement
Basecamp toolcall routing must distinguish:
- true camp-duty / basecamp-assigned NPC behavior
from
- ordinary follower behavior that merely happens near or inside camp

### Deliverable
Trace and fix the routing discriminator so the basecamp snapshot/toolcall path uses the right duty/state signal rather than simple positional presence.

### Acceptance bar
- the current McWilliams repro is understood and documented
- follower-inside-basecamp does **not** automatically imply basecamp toolcall routing
- true basecamp-assigned NPC path sends the intended basecamp-aware snapshot/toolcall payload

### Out of scope
- locker outfit settings
- harness menu polish

---

## Package 3. Locker outfit engine hardening

### Problem
Current outfit replacement/cleanup is too brittle against random NPC states.
Observed or suspected issues:
- shorts + jeans at the same time
- baseball cap not coming off for soldier helmet
- damaged backpack not being replaced by a better-condition equivalent
- undergarment cleanup partly happens, but replacement rules are muddy

### Target behavior
- conflicting worn items can be removed when needed
- better-condition equivalent wins over worse-condition equivalent
- helmet can replace cap when helmet is the better choice
- one-piece / onesie armor can satisfy multiple regions
- cleanup should be selective and robust, not a daily full-strip absurdity by default

### Suggested implementation shape
Think in role buckets / outfit goals, not only isolated item slots:
- helmet
- torso armor
- arm protection
- leg protection
- backpack
- sidearm
- melee
- long gun

Trigger stronger cleanup when outfit state is clearly invalid:
- missing required role item
- conflicting nonsense worn together
- obviously inferior/damaged equivalent equipped

### Acceptance bar
- cap -> helmet replacement works
- duplicate/conflicting lower-body wear resolves sanely
- damaged backpack can be replaced by a better-condition equivalent
- onesie armor is not excluded by over-literal slot logic

### Out of scope
- full carried-item/pocket system
- follower inventory preservation rules

---

## Package 4. Locker zone policy + control-surface cleanup

### Problem
Locker semantics and player controls are underspecified.
Locker contents are curated by the player and should not quietly collapse back into generic sorting behavior.

### Policy to land
- locker items are curated stock
- locker should beat generic sorting semantics
- for locker, treat `ignore items when sorting in this zone` effectively as **YES**
- player-facing locker settings should live on the billboard / locker settings surface

### First useful setting set
- helmets
- armor
- backpacks
- sidearm
- melee
- long guns

### Armor interpretation
- roughly one protective piece for each major region when possible:
  - torso
  - arms
  - legs
- one-piece / onesie armor should count too

### Acceptance bar
- locker contents no longer get treated like ordinary unsorted stock
- player can discover and understand the first useful locker toggles
- wording is clear enough that behavior is not magical folklore

### Out of scope
- full pocket/inventory management
- grenade or advanced consumable logic

---

## Package 5. Basecamp carried-item dump lane

### Problem
Basecamp NPCs should not keep follower-style pocket junk while running the locker dressing cycle.
They should mostly empty their carried miscellany first, while preserving only a very small useful carried lane.

### Greenlit policy
Treat basecamp NPCs differently from followers:
- before or during locker dressing, basecamp NPCs dump ordinary carried misc junk out of their pockets
- curated locker stock must **not** become the dumping ground for that junk
- the dump target should be a floor / unsorted / cleanup lane outside curated locker stock
- the kept carried lane is intentionally tiny:
  - bandages
  - ammo
  - magazines

### Acceptance bar
- the carried-item cleanup runs as part of the locker dressing cycle instead of requiring separate player babysitting
- ordinary misc carried junk is dumped instead of being preserved follower-style
- bandages are kept instead of being dumped with the junk
- ammo and magazines are kept instead of being dumped with the junk
- curated locker stock does not get polluted by the dump behavior

### Greenlit proof support inside Package 5
A narrow live-restage/helper path is explicitly greenlit when the missing evidence is only the kept-lane live packet.
That means Andi may:
- seed only the exact proof items (`bandages`, ammo, one magazine, one junk control item)
- place/drop them onto the real McWilliams / `CAMP_LOCKER` live path
- use the real pickup plus locker-service seam to prove keep-vs-dump from artifacts/logs

This is still **Package 5 evidence work**, not permission to reopen Package 4 or redesign locker semantics.

### Recommended v1 exclusions
- grenades
- broad consumable logic beyond bandages
- general follower-style trinket preservation
- complicated pocket micromanagement
- arbitrary player-given-item exception logic unless clearly needed
- turning this into a full support-gear or utility-belt planning system
