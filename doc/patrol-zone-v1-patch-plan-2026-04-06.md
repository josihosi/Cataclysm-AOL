# Patrol Zone v1 patch plan (2026-04-06)

Status: greenlit implementation plan for Andi.

## Goal

Implement a **Zone Manager patrol zone** that becomes a real Basecamp/camp job with legible NPC guard behavior.

This is **not** a smart-zone-manager thesis and **not** a tactical AI overhaul.
Patrol Zone v1 should be brutally simple, deterministic, and explainable.

## Product contract

### Patrol Zone v1
- Player can paint patrol-zone squares in Zone Manager.
- Patrol squares are grouped into **4-way connected clusters**.
- Each cluster is treated as one patrol assignment unit.
- The camp job menu gets a distinct **Patrol** task with its own priority.
- The patrol pool is NPCs with patrol priority > 0.
- Active guards are chosen at **shift boundaries** using a **2-shift model**:
  - day
  - night
- Within a cluster:
  - if active guards >= square count, guards hold distinct squares
  - if active guards < square count, guards cycle through the cluster in a fixed loop
- Allocation rule: give each cluster one active guard first when possible, then add extra coverage to larger clusters.
- On-shift patrol is a **sticky commitment** for the shift.
- Routine camp work does **not** steal an on-shift guard.
- Urgent disruption **can** break patrol:
  - combat / immediate threat
  - severe injury
  - collapse-level needs
  - explicit player reassignment
- If an active guard is lost mid-shift:
  - backfill from patrol reserve if available
  - otherwise tolerate the coverage gap until later recompute
- Do **not** globally reshuffle the rest of the roster mid-shift.

## V1 policy constants to pin down early
These are small but important. Nail them down explicitly instead of letting them drift into quiet freestyle:
- **connectivity rule**: 4-way only
- **shift model**: exactly 2 shifts (`day`, `night`)
- **day/night cut**: choose explicit hours and keep them stable for v1
- **loop dwell time**: choose one simple fixed dwell duration per square/post for v1
- **reserve backfill timing**: choose whether reserve replacement is immediate on true guard loss or only on a coarse patrol check; keep it deterministic
- **off-shift posture**: decide whether off-shift guards idle near the patrol area or return to ordinary camp life; do not let this stay accidental

## Explicit non-goals for v1
- custom shift counts
- diagonal connectivity cleverness
- threat-aware or perimeter-aware weighting
- dynamic constant re-optimization
- path-cost optimal route generation
- per-NPC preferred sleep schedules
- smart reserve behavior beyond simple reserve/off-shift
- player-configurable dwell times
- mixed-priority post importance
- smart zone manager ideas

## Recommended implementation slices

### Slice 1 — zone surface + topology spine
Deliverables:
- add a new Zone Manager zone type for patrol squares
- expose a stable internal patrol-zone lookup path
- implement 4-way connected-component clustering for patrol squares
- add narrow deterministic tests for topology splitting/merging

Suggested evidence:
- narrow deterministic tests only

### Slice 2 — patrol job surface + planner contract
Deliverables:
- add Patrol as a camp/basecamp job with its own priority entry
- define the patrol pool as NPCs with patrol priority > 0
- implement a deterministic planner that:
  - picks active guards at shift boundaries
  - uses one-per-cluster coverage first
  - records the intended staffing/allocation shape without claiming on-map behavior proof yet
- encode the reference staffing/topology cases explicitly:
  - 1 NPC / 4 disconnected posts
  - 4 NPCs / 4 disconnected posts
  - mixed clusters => one-per-cluster coverage first
  - 16 NPCs / 4 connected squares stays explainable for later hold-vs-loop behavior

Suggested evidence:
- deterministic planner tests only

### Slice 3 — sticky shift roster + interrupt rules
Deliverables:
- make on-shift patrol block routine work for the duration of the shift
- define an explicit interrupt whitelist
- support reserve backfill without whole-roster reshuffle
- tolerate uncovered gaps when reserve is absent

Suggested evidence:
- deterministic tests for:
  - shift roster formation
  - stickiness vs routine chores
  - interruptibility
  - reserve backfill
  - no global recompute on single mid-shift loss

### Slice 4 — on-map behavior
Deliverables:
- understaffed connected cluster -> fixed patrol loop
- fully staffed cluster -> hold distinct squares
- disconnected single-tile clusters behave as separate posts
- basic mid-shift continuation behavior stays legible

Suggested evidence:
- narrow deterministic scheduling/path intent tests where practical
- then one honest live probe/harness packet once enough visible behavior exists

### Slice 5 — live proof + packaging
Deliverables:
- capture one honest live scenario for each visually important mode:
  - lone guard on disconnected posts
  - staffed connected cluster with hold positions
- package the evidence with explicit:
  - screen
  - tests
  - artifacts/logs
- update repo docs/ledgers to current truth only

## Reference cases that must stay explainable

### Case A — 1 NPC, 4 disconnected posts
- one day-shift patrol route
- fixed visit order
- partial daily coverage only

### Case B — 4 NPCs, 4 disconnected posts
- 2 active by day
- 2 active by night
- each active guard covers 2 posts
- no fake full 24h occupancy claim

### Case C — 16 NPCs, 4 connected squares
- 4 active by day
- 4 active by night
- active guards hold distinct squares
- extras remain reserve/off-shift

## Deterministic test packet to aim for
- topology:
  - 4 orthogonally touching squares => 1 cluster
  - diagonal-only touching squares => separate clusters
- staffing:
  - 1 NPC + 4 isolated posts => 1 route covering all 4 on one shift
  - 4 NPCs + 4 isolated posts => 2 day active / 2 night active / 2 posts each
  - 1 NPC + connected 4-tile cluster => 1 loop within the cluster
  - 16 NPCs + connected 4-tile cluster => 4 day holders / 4 night holders / extras reserve
- allocation:
  - mixed clusters with too few active guards => one-per-cluster first, then extras
- stickiness:
  - routine non-patrol work does not steal an on-shift guard
- interruptibility:
  - urgent disruption removes a guard mid-shift
  - reserve backfills if present
  - otherwise coverage gap remains
- recompute timing:
  - shift-boundary recompute yes
  - constant availability-change thrash no

## Player-legibility bar for v1
Even if the underlying scheduling is correct, Patrol Zone v1 still fails if the player cannot roughly tell:
- why an NPC is standing still vs walking a loop
- why a tile/post is uncovered right now
- why connected clusters behave differently from disconnected posts
- why some guards are off-shift / reserve instead of dogpiling the same square

Treat this as a real success criterion, not decorative UX garnish.

## Hallucination / fake-progress traps for Andi
Do **not** claim success just because:
- a zone type exists but no planner uses it
- a planner exists but routine work still steals guards immediately
- hold-vs-loop behavior only exists in prose, not code/tests
- mid-shift reserve behavior is hand-waved
- a live probe looks plausible on-screen but deterministic tests for the contract do not exist yet
- docs sound coherent while the interrupt whitelist is still undefined

When reporting progress, separate clearly:
- implemented deterministic contract
- packaged live evidence
- remaining missing behavior

## Recommendation to Andi
Build this in order:
1. zone + clustering
2. patrol job + planner contract
3. sticky roster + interrupt whitelist
4. on-map hold/loop behavior
5. live proof/packaging

Do not skip ahead to “smart” behavior. If the v1 is not explainable in one paragraph, it is already too clever.
