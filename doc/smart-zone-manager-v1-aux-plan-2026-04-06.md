# Smart Zone Manager v1 — auxiliary plan (2026-04-06)

Status: greenlight-ready future-lane design note. Not the active lane.

## Why this exists

A future Smart Zone Manager should not start as an always-on genius goblin. It should start as a **one-off basecamp auto-layout helper** that places a small number of useful, legible zones inside the Basecamp zone.

This document exists so the later lane starts from a concrete product shape instead of a pile of good intentions.

## Product surface

Recommended v1 surface:
- explicit one-off action such as **Auto-place basecamp zones**
- optionally also offered right after placing the Basecamp inventory zone
- deterministic result
- non-destructive by default
- no ongoing auto-rearrangement after placement

## Greenlight-ready v1 contract

If this lane is greenlit, v1 should mean exactly this:
- one explicit one-off smart-zoning action inside Basecamp
- three main niches only: crafting, food/drink, equipment
- support zones only: clothing, dirty, rotten, unsorted, blanket/quilt-on-beds
- furniture/terrain chosen by general anchor flags/categories first, with floor fallback
- one niche of each type only, never a clever multi-hub layout
- custom zones allowed for only the currently intended special cases: `rotten`, `dirty`, `splintered`
- non-destructive by default
- deterministic placement on the same camp layout

If the implementation starts drifting toward patrol/locker automation, room-classification magic, or continuous self-rearrangement, that is out of v1 and should be cut or deferred.

## High-level v1 shape

### Main niches
1. **crafting**
2. **food/drink**
3. **equipment**

### Support zones
- clothing
- dirty clothing (inside, near clothing)
- rotten (outside Basecamp)
- unsorted (one larger spill/intake zone)
- blanket/quilt on beds

### Explicit custom zones currently intended
- `rotten`
- `dirty`
- `splintered`

## Important current code/context facts

### Explicit zone ids visible in `data/json/zones.json`
Relevant built-ins already visible:
- `LOOT_CUSTOM`
- `LOOT_DEFAULT`
- `LOOT_DRINK`
- `LOOT_FOOD`
- `LOOT_SEEDS`
- `LOOT_UNSORTED`
- `LOOT_WOOD`
- `SOURCE_FIREWOOD`
- `AUTO_EAT`
- `AUTO_DRINK`
- `STUDY_ZONE`
- `CAMP_STORAGE`
- plus current basecamp-specific zones like `CAMP_LOCKER` and `CAMP_PATROL`

### Practical implication
The future smart-zoning design should **not assume every desired sub-category already exists as a separate hardcoded zone id** in `zones.json`.
A lot of the detail likely has to be expressed via:
- existing loot/custom zone machinery
- stacked zone choices
- or a very small number of new custom zones

### Subcategory / custom-zone rule
Default rule for v1:
- if a useful existing zone has subcategories/options, do **not** exclude it from sorting by default just because it is more detailed
- prefer using the normal sorting/loot machinery unless a concrete conflict shows that a custom zone or explicit override is cleaner
- resolve the exact subcategory choices on the way through implementation, but keep the bias toward reuse rather than carving out needless exceptions

### Code areas likely relevant later
- `data/json/zones.json` — explicit zone ids and names
- `src/clzones.*` — zone/loot/custom handling plumbing
- `src/zone_manager_ui.cpp` — zone-manager UI/action surface

## Furniture/anchor selection strategy

### Rule
Prefer **general flags / categories** over brittle string matching.

Meaning:
- prefer furniture/terrain classes like fire source, cold storage, flat work/display surface, storage furniture, seat, bed
- only fall back to a small explicit allowlist when the generic flags are not rich enough
- do not build the first version around checking whether an id text contains `chair`, `table`, `fridge`, etc.

### Desired anchor classes
- **fire source** -> fireplace, brazier, similar fire furniture/terrain
- **water source / craft-adjacent utility** -> water pump and similar
- **cold storage** -> fridge-like furniture
- **flat equipment surface** -> table/counter/work surface
- **clothing holder** -> cupboard/dresser/wardrobe first, chair/seat second
- **bed surface** -> beds for blanket/quilt support
- **open floor fallback** -> when no suitable furniture exists

## Core placement rules

### 1. Furniture first, floor fallback
- use recognizable furniture if present
- if suitable furniture is absent, still place the zone on floor
- do not fail just because the camp is ugly

### 2. One niche only
- one crafting niche
- one food/drink niche
- one equipment niche
- not multiple distributed mini-hubs

### 3. Stay inside Basecamp unless explicitly outside-only
- smart-zoned niches stay inside Basecamp
- exception: rotten goes outside Basecamp

### 4. Non-destructive by default
- fill obvious unzoned places first
- avoid overwriting existing zones unless the player explicitly allows it later

## Detailed niche design

## A. Crafting niche

### Primary anchors
- fire source furniture/terrain
- optionally supported by nearby water source

### Corrected fire/wood local layout
This is the important corrected rule from Josef:
- the **fireplace/brazier/fire-source tile itself** should be covered by `SOURCE_FIREWOOD`
- one adjacent tile should get custom **`splintered`**
- another nearby tile should get **wood** storage

This is deliberately local and physical, not a vague crafting halo.

### Broader crafting support around that anchor
Around the fire/water anchor cluster, place nearby support zones for:
- tools
- spare parts / generic craft materials
- books/manuals / study-adjacent material
- containers
- tool magazines
- chemicals / drugs

### Chemistry/medical placement nuance
- chemicals/drugs belong **near** crafting
- but a few tiles farther away than the immediate firewood/splintered/wood knot
- close enough to feel like part of the same work corner, not so close it becomes a single heap

## B. Food/drink niche

### Primary anchor
- cold storage / fridge-like furniture

### Default zones
- `LOOT_FOOD`
- `LOOT_DRINK`

### Open design question
- `LOOT_SEEDS` may stack near food if that later feels right, but this should be decided against the actual in-game sorting behavior rather than assumed now

## C. Equipment niche

### Primary anchor
- flat work/display surface such as table-like furniture

### Intended local split
- one tile for **guns**
- one adjacent tile for **ammo + magazines** together

This is intentionally more readable than one undifferentiated weapon pile.

## D. Clothing support

### Anchor preference
1. storage furniture for clothing
2. seat/chair fallback
3. floor fallback

### Support placement
- clothing near the clothing anchor
- custom **`dirty`** zone inside, near clothing

## E. Rotten support
- custom **`rotten`** zone
- outside Basecamp
- practical dump behavior, not cleverness

## F. Unsorted support
- use `LOOT_UNSORTED`
- one larger intake/spill area
- target heuristic from Josef: roughly **+4 tiles on either side** rather than a tiny precious square
- bigger than specialty support tiles

## G. Blanket/quilt on beds
- cover beds inside the Basecamp inventory zone with a blanket/quilt support zone
- likely needs either a new custom zone or careful reuse of existing storage/custom behavior
- should be treated as support convenience, not a main niche

## Zone table — working draft

This table is intentionally conservative until the full in-game sorting/loot behavior is audited in more detail.

| Desired role | Existing zone candidate | Stack candidate | Custom zone needed? | Preferred anchor class | Notes |
|---|---|---:|---:|---|---|
| Food | `LOOT_FOOD` | maybe with seeds later | no | cold storage | likely fridge-centered |
| Drink | `LOOT_DRINK` | maybe adjacent to food | no | cold storage | likely same niche as food |
| Seeds | `LOOT_SEEDS` | maybe with food | probably no | cold storage / nearby storage | needs real in-game behavior check |
| Fire source supply | `SOURCE_FIREWOOD` | no | no | fire source tile itself | explicitly covers fireplace/brazier tile |
| Splintered wood | none obvious | no | **yes** (`splintered`) | tile adjacent to fire source | explicit custom zone |
| Wood | `LOOT_WOOD` | no | probably no | adjacent storage/floor near fire | distinct from source-firewood and splintered |
| Tools | likely loot/custom plumbing, not obvious explicit id here | maybe with parts only if needed | maybe not | crafting-adjacent storage | audit needed |
| Spare parts/materials | likely loot/custom plumbing | maybe with tools | maybe not | crafting-adjacent storage | audit needed |
| Books/manuals | `STUDY_ZONE` and/or loot/custom | maybe with manuals/study | maybe not | crafting/study-adjacent storage | needs actual zone-vocabulary pass |
| Containers | likely loot/custom plumbing | maybe with crafting supplies | maybe not | crafting-adjacent storage | audit needed |
| Tool magazines | likely loot/custom plumbing | maybe with tools | maybe not | crafting-adjacent storage | audit needed |
| Chemicals/drugs | likely loot/custom plumbing | maybe together | maybe not | a few tiles farther from fire anchor | keep near crafting but not in the central knot |
| Guns | likely loot/custom plumbing | no | maybe not | flat equipment surface | one tile |
| Ammo + magazines | likely loot/custom plumbing | yes | maybe not | adjacent flat equipment surface | one shared adjacent tile |
| Clothing | likely loot/custom plumbing | no | maybe not | clothing storage / seat fallback | keep inside |
| Dirty clothing | none obvious | no | **yes** (`dirty`) | near clothing support | inside near clothing |
| Rotten | none obvious | no | **yes** (`rotten`) | outside floor area | outside Basecamp |
| Unsorted | `LOOT_UNSORTED` | no | no | open intake floor area | larger spill zone |
| Blanket/quilt | none obvious | maybe blanket+quilt together | likely **yes** or careful custom reuse | beds | cover all beds in Basecamp inventory zone |

## Implementation order

### Slice 1 — audit and mapping
- inspect the actual in-game sorting/loot/custom zone vocabulary in more detail
- confirm which desired roles already map cleanly to existing zone behavior
- confirm which roles truly need custom zones
- confirm which roles can safely stack

### Slice 2 — anchor detection spine
- implement furniture/terrain anchor selection using general flags/categories first
- define small fallback allowlists only where the generic signals are not enough
- support floor fallback

### Slice 3 — niche planner
- choose one crafting niche
- choose one food/drink niche
- choose one equipment niche
- choose clothing/dirty support
- choose rotten outside placement
- choose unsorted spill area
- choose blanket/quilt support on beds

### Slice 4 — zone placement
- place zones deterministically
- use furniture first, floor fallback
- keep placement non-destructive by default
- preserve the corrected firewood/splintered/wood local layout around the fire anchor

### Slice 5 — packaging / UI
- explicit one-off button or follow-up prompt after placing Basecamp inventory zone
- clear wording that this is a one-time helper, not an always-on manager

## Explicit non-goals for v1
- patrol auto-placement
- locker auto-placement
- guards-at-windows behavior
- dynamic rearrangement after placement
- smart-room inference
- furniture moving
- global camp optimization
- multiple niches of the same type
- giant-base support

## Biggest v1 risks
1. **Zone-vocabulary mismatch**
   - designing subzones that the actual game does not already express well
2. **Anchor overfitting**
   - too much furniture-id/string logic instead of flag/category logic
3. **Unreadable placement**
   - technically correct zones that do not visually read as a crafting/equipment/food corner
4. **Over-excluding existing sorting behavior**
   - treating subcategory/custom-capable zones as something to dodge instead of something to reuse
5. **Too much cleverness**
   - trying to solve locker/patrol/window-guard logic in the same first version
6. **Silent destructive overwrite**
   - stomping on player zones in the name of smartness

## Deterministic test ideas
- same basecamp layout => same chosen anchors and same zone placement
- fire source tile gets `SOURCE_FIREWOOD`, not just nearby placement
- splintered tile lands adjacent to fire source when possible
- wood lands nearby but distinct from source-firewood/splintered
- fridge produces food/drink niche
- equipment table produces guns tile + adjacent ammo/mags tile
- clothing prefers storage furniture, then seat, then floor fallback
- rotten is outside Basecamp
- unsorted area is larger than niche support tiles
- no existing zones overwritten in default non-destructive mode
- bed coverage for blanket/quilt applies only to beds inside Basecamp inventory zone

## Player-legibility bar
The feature succeeds only if the player can look at the result and immediately think:
- "this is the crafting corner"
- "that fridge got food/drink"
- "that table is the equipment niche"
- "that cupboard/chair is clothing"
- "that inside nearby patch is dirty clothes"
- "that outside patch is rotten"
- "that larger spill area is unsorted intake"
- "those beds got blanket/quilt support"

If the player needs a lecture, the smartness failed.

## Greenlight recommendation
This plan is now in a state where it can be greenlit as a future lane.

The intended greenlight statement would be roughly:

> **Smart Zone Manager v1**
> Build a one-off, deterministic, non-destructive basecamp auto-layout helper that creates one crafting niche, one food/drink niche, and one equipment niche plus the support zones `dirty`, `rotten`, `unsorted`, clothing, and blanket/quilt-on-beds. Use general anchor flags/categories first, floor fallback second, preserve the explicit firewood/splintered/wood fire layout, and prefer existing sorting-zone machinery/subcategories unless a concrete conflict forces a custom zone.

## Blunt recommendation
Smart Zone Manager v1 should be a **one-off, flag-first, non-destructive basecamp auto-layout helper**.
It should create one readable crafting niche, one food/drink niche, one equipment niche, plus a few support zones, with the corrected firewood/splintered/wood fire layout preserved exactly. Audit the real zone vocabulary first, then build the planner around what truly exists instead of inventing a parallel religion of storage.
