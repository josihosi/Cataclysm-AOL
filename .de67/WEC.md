# Combined hostile-ecology and harness-registry WEC

## Preserved hostile-ecology proof rescope

Preserve R-002’s product behavior: hostile scouts must discover the player, signals, and danger fairly through bounded real perception, never hidden-state radar. Rescope only R-002’s proof burden: focused owner tests should cover the visibility/perception invariants; the smallest live negative/positive production proof should establish quiet play is not discovered by the old radar, a credible real signal can be discovered without a decoy granting exact hidden truth, relocation does not drag stale target knowledge, and unseen danger does not affect routing until legitimately observed. Remove the requirement for a bespoke natural-world/live certification route for every matrix row and remove exact per-row binary/fixture/operation/member continuity where deleting it does not leave the fairness contract unmet or unproved. Do not weaken the gameplay behavior, and leave R-001 and R-003 through R-010 product requirements unchanged.

## Harness scenario registry and canonical proof workflow

### User outcome

C-AOL should have a dependable harness where coordinators can describe gameplay conditions and proof route; find only scenarios genuinely satisfying hard prerequisites; rank valid scenarios by preferences; understand matches/failures/staleness; launch through one normalized startup, Peekaboo, input, observation and cleanup route; and receive a reviewable scenario draft when nothing qualifies. Scenario selection must stop being the recurring weakest point in gameplay proof. The stronger requested outcome includes a working inventory/migration step that tries every existing scenario and defines it in the database, preserving failures/contradictions instead of skipping them.

### Intended experience

A coordinator states requirements such as real camp, player condition, nearby-but-not-visible friendlies, hostile NPC capable of Fight, ordinary input permitted, terminal aftermath and save/reload. Harness rejects any hard mismatch, ranks valid survivors, explains evidence/freshness, identifies profile/fixture/world/executable/permissions/helpers, performs canonical preflight/launch, records actual verification or contradiction, and never credits startup/load as feature proof. No match explains every missing capability and produces a reviewable manifest draft but does not run it.

### Terminology

Scenario manifest = reviewable declared intent/requirements/capabilities/proof contract. Scenario registry = searchable SQLite index built from manifests and observed evidence. Capability = supported condition or transition. Hard requirement rejects; preference ranks valid survivors. Evidence states: declared, inspected, run-verified, contradicted, stale. Proof route is precondition through production behavior to artifact/verdict. Startup footing is never gameplay proof. Scenario draft is non-executed candidate manifest.

### Required dimensions

Player: overall/per-part health, wounds/bleeding/bites/infection/treatment, pain, thirst, hunger/calories, stamina, fatigue/sleepiness, temperature/wetness/exposure, radiation, effects/diseases, mutations/traits/bionics, movement/encumbrance, wielded/worn/carried inventory, charges/ammo/tools, relevant faction/mission/dialogue state.

Local place: forest/field/road/shelter/building/camp/etc, indoor/outdoor/weather protection, terrain/furniture/fields/traps, camp ownership/facilities, vehicles, traversability/escape, light/visibility, position relative to interaction owner.

Actors: friendly NPCs, unfriendly NPCs, monsters; presence/absence, identity/type/faction/attitude/role, count, distance/range, visible/out-of-sight/nearby, loaded vs overmap-only, health/readiness, reservation/assignment/availability, ability to reach/participate.

World/overmap: nearby terrain/specials/camps/shelters/roads/cities/hostile sites, overmap NPCs/hordes/threats, time/season/moon/light, weather/portal-storm policy, world options/mods, generation/operation/report/cursor/receipt identity, save/reload/persistence.

Capabilities: ordinary movement/waiting, dialogue, Pay/Fight/named choices, trade, combat, NPC/monster travel, local/overmap transitions, terminal aftermath, survivor return, save/reload, exactly-once, duplicate/stale/replay rejection, visible/log/persisted evidence.

Runtime: OS, branch/commit/dirty source/executable binding, profile/snapshot/world/fixture compatibility, helpers, Peekaboo capture/input permissions, PID focus, permitted/forbidden input, safe-mode/interruption, OCR, permitted debug setup vs required production behavior, cleanup/restoration/disposable-copy policy.

Evidence retains state, source manifest/fixture, source/executable binding, run/proof artifact, observed preconditions, invalidation reason, relevant verification, and proof depth (startup/interaction/terminal/persistence/replay).

### Boundaries and decisions

Manifests remain authoritative for declared intent. SQLite is rebuildable search index and verification-history store, not opaque replacement. Hard requirements cannot be ranked around. Names/descriptions are not capability evidence. Startup/load is distinct from gameplay proof. Debug-authored state is not production proof. Contradicted/stale evidence remains visible. Generated drafts require review and are not auto-launched. Rework fits existing harness/fixtures/profiles/artifact checks/Peekaboo rather than a disconnected harness. Do not choose product behavior beyond this intent. Current hostile-ecology delivery should not be blocked if safe isolation is possible. Canonical workflow covers startup, Peekaboo permissions, PID focus, input, observation, reporting, cleanup. A harness-facing skill teaches/invokes the same workflow.

### Smallest useful vertical slice

Query: real camp; player not critically injured; friendly NPC nearby but not visible; hostile shakedown NPC nearby; input allowed; visible Fight required. System filters current manifests; rejects recent Fight-contradicted scenario; explains rejection; returns only hard-valid scenario or a non-executed draft; does not launch during query. Additionally, the user requires a working migration/inventory step that processes every existing scenario into the registry and attempts/records its verification disposition.

### Acceptance

Thirsty forest observer cannot qualify for camp/Fight. Recent camp scenario cannot qualify by name alone. Contradicted capability excluded from hard matches. Preferences cannot rescue hard mismatch. Valid result explains manifest/fixture/evidence. No-match identifies unmet needs and produces non-executed draft. Selected launch uses canonical preflight and separate startup/feature verdicts. Runs strengthen/contradict/stale indexed evidence without rewriting manifest intent. Every existing scenario is represented in the database with an explicit attempted/imported/verified/failed/blocked/contradicted disposition; none silently omitted.

### Failure cases

Filename/prose similarity selection; camp implies Fight; Peekaboo/HUD implies gameplay proof; auto-running draft; retaining green after binding changes; competing declaration sources; unfilterable free-form evidence; registry explains but cannot reject; silent omission during all-scenario migration; turning this into a disconnected second harness.

### Prototype/reaction outputs for later

Concrete coordinator query/explanation, manifest capability block, SQLite result with evidence states, generated no-match draft, canonical harness-skill invocation.
