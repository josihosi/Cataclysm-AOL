# Bandit and Cannibal Hostile-Camp AI Specification

Status: normative design contract and implementation audit for the isolated Mac `dev` lane.
Comparison baseline: `port/cdda-master` at `660057ff728b` versus `dev` at `1844bc8324a3`.

This document defines what the feature must do in play, how the implementation is divided between
game systems, and which parts of the current `dev` diff are complete, partial, or absent. It is not
a second task roadmap: `Plan.md` and `SUCCESS.md` still determine execution order.

## Document authority and supersession

- `Plan.md` is the sole roadmap, this file is the normative functional/technical contract,
  `SUCCESS.md` is the outcome ledger, `TODO.md` is only the next execution claim, and `TESTING.md`
  owns current proof policy/evidence.
- Older dated bandit/cannibal packet, audit, and proof documents are implementation archaeology.
  They may explain why a seam exists, but they cannot add requirements, declare success, or
  override this contract and current source evidence.
- `TechnicalTome.md` is chronological mechanic history, not a second current-state specification.
  If its historical wording conflicts with this file, this file and the inspected game path win.
- Do not recreate a phase ledger or parallel roadmap. Close work by crossing the matching red item
  here and the corresponding outcome in `SUCCESS.md`, then reduce `TODO.md` to the next necessary
  claim.

Status markers:

- `[x]` is present in the game path and has proportionate source/test evidence.
- `[ ] 🔴` is wrong, incomplete, or unproved. When closed, replace it with `[x]` and add the proof.
- A unit-tested state structure is not considered implemented if no production game path invokes it.

## Functional contract

A naturally generated bandit or cannibal camp is a persistent physical faction. It sends a
two-person scout party to explore and scavenge, learns only what the scouts can plausibly perceive,
withdraws coherently when discovered, receives only information carried home by survivors, and
decides whether it has enough strength and opportunity to act. A favorable bandit decision creates
a new shakedown party. A favorable cannibal decision creates a new attack party which waits for
true darkness. Neither faction knows the avatar's coordinates or camp contents by fiat.

The player-facing success path is:

```mermaid
flowchart LR
    A["Living hostile camp"] --> B["Reserve exact two-person scout party"]
    B --> C["Travel and search physical OMT route"]
    C --> D["Observe target from watch ring"]
    D -->|"not exposed"| E["Complete bounded assessment"]
    D -->|"burned"| F["Gain close-contact evidence and withdraw"]
    E --> G["Survivors physically return"]
    F --> G
    G --> H["Camp receives survivor-scoped report"]
    H --> I["Camp compares opportunity, risk, strength, and reserve"]
    I -->|"hold / rescout / abandon"| A
    I -->|"bandit favorable"| J["Reserve fresh shakedown party"]
    I -->|"cannibal favorable"| K["Reserve fresh raid party"]
    J --> L["Open demand, payment / refusal / combat, aftermath"]
    K --> M["Wait for true darkness, attack loaded defenders, aftermath"]
```

## Required behavior and current implementation

### 1. Camp, roster, and dispatch ownership

- [x] Bandit and cannibal camps use the same routine exploration machinery. Cannibals do not need
  a separate supernatural hunt trigger.
- [x] Routine scouts are exactly two people. A one-person camp cannot dispatch; a two-person camp
  sends both; larger camps send two while retaining an at-home reserve where their roster permits.
  The number two is the agreed product rule, not a tuning guess.
- [x] The camp owns one serialized roster whose members are unambiguously at home, reserved,
  outbound, materialized, returning, dead, missing, or retired. A member cannot be simultaneously
  available to camp jobs and an outing.
- [x] Routine scouting, returned assessment, and hostile response are different operations with
  stable IDs, generations, reservation members, and idempotent application keys. A scout mission
  does not silently turn into a raid.
- [x] A camp has at most one active external operation. A response cannot reserve members while a
  scout or another response still owns outside pressure for that camp.
- [x] A deterministic surviving member replaces a dead leader; the pair retains its shared route
  and operation identity.

### 2. World truth, private knowledge, and bounty

- [x] Finite structural/ground bounty is global world truth. Each camp stores only its private,
  possibly stale estimate. Two camps may believe a site is rich, but only the first valid claimant
  consumes its remaining resource.
- [x] Bandit and cannibal scouts both search for and collect finite bounty as part of routine
  scouting.
- [x] Camp leads carry provenance, observation time, confidence/uncertainty, threat, bounty,
  approximate position, and aging. Active operations pin referenced evidence so ordinary pruning
  cannot invalidate a live mission.
- [ ] 🔴 Player-camp opportunity is not yet a renewable authoritative value. Repeated shakedowns
  must require both the existing cooldown and genuinely renewed camp value from stored goods,
  population, or activity. A timer alone must not regenerate loot or authorize repeated demands.

### 3. Perception and discovery

- [x] The production branch's exact-avatar radar (`direct_player_range`, ten OMTs) is absent from
  the active `dev` path. The remaining `legacy_radar` value is compatibility vocabulary for old
  saves, not a live sensor.
- [x] Scouts acquire facts only from a bounded route/frontier query. The ordinary baseline is
  roughly three OMTs in clear day, two in degraded light/dusk, and one in unlit night, with terrain,
  weather, elevation, optics, and the actual NPC's sight affecting the result. These numbers are
  the agreed visibility rule.
- [x] Optics improve credible observation and assessment; they do not grant arbitrary map-wide
  player or camp tracking.
- [x] Smoke, visible light, searchlights, alarms, gunfire, and explosions create approximate leads.
  A signal may cause investigation, but it does not reveal the avatar's identity, exact inventory,
  or current coordinates.
- [x] Terrain supplies a prior danger cost. Live hostile population is sampled only where a scout
  can legitimately observe it; the camp does not query unseen zombie populations as omniscient
  route data.
- [x] Danger has soft and hard effects: risk can increase route cost, while an observed overwhelming
  threat can reroute, abort, or force immediate self-defense.

### 4. Physical movement, stalking, and exposure

- [x] The strategic operation has one simulation owner at a time: abstract overmap or local loaded
  NPCs. Handoff epoch and generation checks reject stale or duplicate advancement.
- [x] A scout pair has a leader, escort, common route, assigned staging tiles, cohesion rules,
  regroup behavior, bounded recovery, and casualty-aware continuation.
- [x] The target camp footprint and a watch position remain distinct. Normal stalking observes from
  a three-OMT radius—two empty OMTs between the scouts and the camp—and falls farther back when
  terrain or exposure requires it.
- [x] Reciprocal ordinary visual contact burns the party. Being burned adds useful close-contact
  evidence and target alertness, then commits the scouts to egress. It is not deliberately farmed
  as a scouting tactic.
- [x] While searching, watching, or withdrawing covertly, scouts are neutral to the player and
  allied defenders. Generic faction hostility and misleading “gets angry” presentation resume only
  after attack, refusal/escalation, or committed combat.
- [x] A burned party has a persistent route out of the target OMT. It is not constrained to pace
  between adjacent visible tiles while its strategic owner wants to leave.
- [ ] 🔴 The natural local-to-abstract return handoff is not complete. Run `20260807_152913` proves
  exact two-member staging, forward travel, watch completion, and a later valid `returning_home`
  handoff, but the pair still fails to cross a homeward bubble boundary and dematerialize at camp.
  This is the current first gameplay blocker.

### 5. Report, assessment, and response decision

- [x] A camp learns no useful target dossier until a survivor physically returns. Two dead scouts
  yield only overdue/missing state. One survivor yields a partial/provisional report restricted to
  evidence available to that survivor; a later survivor may revise it.
- [x] Reports identify their source operation, member(s), evidence revisions, timestamps, target,
  uncertainty, defender bounds, coarse visible equipment, opportunity cues, route risk, exposure,
  and losses. Applying the same return/report/cargo packet twice is a no-op.
- [x] Camp assessment compares pessimistic target strength, uncertainty, alertness, route risk,
  opportunity, ready camp power, and the required home reserve. It may hold, rescout, abandon, or
  prepare one faction-specific follow-on.
- [x] A follow-on response selects and reserves a fresh party from current survivors rather than
  reusing the scout reservation. The current planner preserves the report revision and operation
  generation.
- [ ] 🔴 The production scheduler never calls `plan_hostile_operation`; current calls are confined
  to tests. `transition_hostile_operation_phase` is likewise exercised by tests and origin-recall
  cleanup, not by a complete live response lifecycle. The follow-on owner is therefore scaffolding,
  not an implemented player-facing feature.

### 6. Faction-specific consequences

Bandits and cannibals share exploration and assessment. They diverge only after a returned report
authorizes a response.

- [ ] 🔴 **Bandit shakedown:** reserve a fresh response party, travel physically, rally outside the
  camp at a plausible two-to-three-OMT planning distance, approach openly, suppress premature
  patrol combat, demand a share of currently reachable camp storage, and resolve payment, refusal,
  player attack, withdrawal, casualties, and return.
  The parley-neutrality hook exists, but no natural scout-to-shakedown run proves the lifecycle.
- [ ] 🔴 **Cannibal raid:** reserve a fresh attack party sized against pessimistic camp strength,
  travel physically, rally in concealment at a plausible two-to-three-OMT planning distance, wait
  for true local darkness, and attack the avatar plus all loaded camp defenders. Cannibals never
  open the payment interface. No invisible offscreen defender deaths are permitted. The state
  vocabulary exists, but the live lifecycle is not wired or proved.
- [ ] 🔴 Aftermath must update the attacking camp's casualties, readiness, target alertness,
  outcome memory, payment/plunder, and future eligibility. A bandit may repeat only after cooldown
  plus renewed target opportunity; a cannibal may reassess survivors rather than replaying an
  obsolete report.
- [x] Autonomous inter-camp war is outside this version. Other hostile camps contribute route risk;
  they do not trigger a second unspecced faction-war simulation.

### 7. Persistence, performance, and proof

- [x] Camps, private leads, finite resources, outings, reservations, reports, decisions, casualties,
  ownership epochs, and application watermarks have serialization and focused compatibility tests.
- [x] `DEBUG_CLAIRVOYANCE` provides a read-only ecology view, selection, bounded watches, compact
  deltas, incident capture, and one labelled casualty intervention through the authoritative death
  route. The observer is not a gameplay owner.
- [ ] 🔴 Save/load at each live lifecycle boundary—including local/abstract handoff, split return,
  shakedown contact, and cannibal darkness wait—must be proved with the changed executable.
- [ ] 🔴 Mac performance/save measurements for the observer and early ecology are not final
  qualification for the completed feature. Before integration, measure the full production path
  on macOS, Linux/WSL, and Windows, including scheduler cost, loaded-NPC cost, save-size growth, and
  save/load latency. Do not run global work every avatar turn merely to satisfy a test.
- [ ] 🔴 Release qualification remains blocked until the natural scout-to-decision incident,
  bandit shakedown, cannibal night raid, persistence boundaries, and relevant platform routes are
  green. `port/cdda-master` remains untouched meanwhile.

## Competing AI systems and override direction

The feature does not replace CDDA NPC AI. It supplies a strategic owner and a narrow movement
intent while an ecology operation is active. Existing survival and combat owners keep precedence
where their facts are more immediate.

```mermaid
flowchart TD
    E["Shared physical facts: terrain, light, smoke, sound, visible threats"] --> S["Hostile-camp strategic owner"]
    S --> H["Abstract/local handoff adapter"]
    H --> M["Loaded scout or response-party movement intent"]
    M --> N["Ordinary npc::move when no ecology order owns this action"]

    X["Field, trap, fire, or adjacent unrelated attacker"] -->|"one-action survival override"| M
    P["Player/allied attack, refusal, or committed contact"] -->|"end covert state"| C["Generic NPC combat/faction hostility"]
    C --> N
    R["Camp patrol alarm"] -->|"active shakedown parley stays neutral"| N
    O["DEBUG_CLAIRVOYANCE"] -. "read only" .-> S

    E --> G["Horde response policy"]
    E --> Z["Zombie-rider light memory/policy"]
    E --> W["Writhing-stalker local policy"]
```

### Ownership and precedence table

| Situation | Authoritative owner | Override rule |
|---|---|---|
| Camp memory, dispatch eligibility, report, decision, reservation | `bandit_live_world` strategic state | No generic NPC behavior may create or advance these facts. |
| Unloaded travel/search/watch/return | Abstract outing cursor | Only the matching operation generation and owner epoch advances. |
| Materialization/dematerialization | `do_turn.cpp` handoff adapter plus overmap NPC storage | Transfer ownership atomically; never let local and abstract loops move the same member. |
| Loaded route/cohesion/egress | Ecology movement intent for the exact reserved members | Own only the next action needed by the persisted operation; fall through when no valid order exists. |
| Fire, field, trap, impassable tile, adjacent unrelated threat | Existing immediate-survival/combat behavior | May preempt one action without deleting the strategic route or inventing a phase transition. |
| Player/allied attack, bandit refusal, committed raid/contact | Existing NPC combat and faction attitude | Explicitly terminates covert neutrality; combat becomes authoritative until contact resolves. |
| Generic faction hostility during covert stalking | Derived covert relationship in `npc::attitude_to` | Suppress hostility only for active, correctly targeted ecology members; never persist a fake faction change. |
| Player-camp patrol during bandit parley | Camp patrol AI | Treat only the active shakedown party as neutral until escalation. Other hostiles remain hostiles. |
| Ordinary inactive NPC travel | Existing `overmap_npc_move` / NPC `omt_path` | Must yield for members owned by an active ecology operation to prevent double travel. |
| Ordinary loaded NPC needs, missions, behavior tree, and optional LLM intent | Existing `npc::move` path | The exact reserved member's valid ecology order owns that action first; ordinary AI resumes when no ecology order applies or covert state ends. LLM output never creates strategic ecology truth. |
| Debug UI/harness | Read-only observer and labelled authoritative casualty intervention | May reveal state or kill a selected member through the real death route; may not set discovery, phase, report, decision, payment, or raid outcome. |

`bandit_dry_run` and `bandit_pursuit_handoff` are also potential duplicate owners inside the
feature. Their permitted roles are deliberately narrow: `bandit_dry_run` may evaluate or explain a
candidate, and `bandit_pursuit_handoff` may validate/transport a transition packet. Neither owns a
second camp memory, reservation, lifecycle clock, or result. Persisted truth remains in
`bandit_live_world` and the normal NPC/overmap stores.

### Shared primitives versus separate policies

Hordes, zombie riders, writhing stalkers, bandits, and cannibals may consume the same physical
emitter primitives. They must not share behavioral memory or ownership merely because they noticed
the same event.

| Primitive or state | Bandit/cannibal camps | Hordes | Zombie riders | Writhing stalkers |
|---|---|---|---|---|
| Physical light/smoke/sound observation | Shared input | Shared input | Shared input | Shared input where its local/overmap contract permits |
| Terrain and ordinary visibility | Shared engine primitive | Own policy | Own policy | Own policy |
| Camp intelligence map / dossiers | Own, private per camp | Never | Never | Never |
| Finite bounty and shakedown value | Own hostile-camp system | Never | Never | Never |
| Scout/report/response reservations | Own hostile-camp system | Never | Never | Never |
| Movement and reality-bubble handoff | Exact camp-operation members only | Existing horde owner | Rider owner | Stalker owner |
| Debug projection | May share observer presentation | May be displayed | May be displayed | May be displayed |

Direction: introduce or retain a small read-only **physical observation** interface at the producer
boundary, then let each consumer decide what it means. Do not introduce a universal “ecology AI”
brain, universal target registry, shared pursuit memory, or generic phase setter. For any actor that
could be claimed by two movement systems, the deciding key is the actor's stable identity plus the
active owner's generation/epoch; the loser yields without mutating the winner's state.

## Conformance summary: `dev` versus `port/cdda-master`

What is right in `dev`:

- [x] It removes the production branch's ten-OMT avatar radar and replaces it with bounded,
  provenance-bearing perception.
- [x] It creates persistent private camp knowledge, global finite-resource truth, two-person
  scouting, stable reservations, abstract/local ownership, cohesive movement, exposure egress,
  survivor-scoped reports, camp assessment, and honest observer tooling.
- [x] It separates a returned scouting operation from a fresh hostile response operation and keeps
  bandit/cannibal consequences distinct at the policy level.
- [x] It adds derived covert neutrality instead of permanently editing faction relations, and it
  gives camp patrols a narrow shakedown-parley exception.

What is incomplete or currently wrong:

- [ ] 🔴 The local/abstract ownership seam still strands a real returning pair before camp
  dematerialization.
- [ ] 🔴 The fresh hostile-operation planner and phase machine are not invoked by the production
  scheduler, so bandit and cannibal consequences remain test-only scaffolding.
- [ ] 🔴 Renewable player-camp opportunity and complete aftermath/repeat rules are absent.
- [ ] 🔴 Full live save/load, performance/save-growth, and three-platform qualification remain
  open and must follow the completed gameplay path rather than isolated helper success.

## Acceptance ledger

The feature is complete when these user-visible contracts are crossed off with changed-executable
evidence. Existing focused tests may support a row but cannot replace its stated live route.

- [ ] 🔴 One natural bandit camp dispatches its exact pair, watches or burns, physically returns at
  least one survivor, applies a final report, and enters the matching camp decision without a
  teleported or abstract-jump return.
- [ ] 🔴 A burned visible pair gains evidence, exits the target OMT without dancing or false anger,
  and preserves its route/report identities.
- [ ] 🔴 Killing both scouts prevents an informed response; one survivor produces only a partial
  report; a later survivor revises rather than duplicates it.
- [ ] 🔴 A decided bandit response reserves a fresh party, reaches the camp, performs a real
  shakedown, and resolves payment, refusal/combat, return, and aftermath.
- [ ] 🔴 A decided cannibal response reserves a fresh party, waits for true darkness, attacks all
  loaded defenders without a payment UI, and causes no offscreen defender deaths.
- [ ] 🔴 Two camps cannot double-harvest one finite site; a repeated bandit shakedown requires
  cooldown plus demonstrably renewed player-camp opportunity.
- [ ] 🔴 Quiet play inside the old radar radius remains undiscovered; clear day/dusk/unlit night,
  forest/weather/optics, decoy signals, target relocation, and zombie-heavy route cases follow the
  bounded perception contract.
- [ ] 🔴 Save/load preserves authority and causality at every phase; full-feature performance and
  save growth remain acceptable on macOS, Linux/WSL, and Windows.

## Agent-coordination projection (DE67 v0.3 Prompt A)

This is a coordination projection of the red contracts above, not another roadmap. It is bound to
`dev` at `9753b78eb966ab149a295e29c7fb26cf2f0ff713`, uses one xhigh contract-first coordinator,
and has a ten-slot ceiling. Why now: the specification has enough owner evidence to divide later
implementation without creating duplicate gameplay owners. The intended player path remains
natural scout discovery -> physical return -> survivor report -> camp decision -> physical faction
response. A helper-green or debug-written outcome is false completion; the decisive current edge is
the valid two-member `returning_home` handoff that still fails before the next homeward boundary and
camp dematerialization. No material owner choice is left open by this projection.

Stable IDs below only name existing red claims; they add no requirements:

| Red ID | Existing unresolved contract |
|---|---|
| `HC-R01` | Natural local/abstract physical return and the unchanged scout-to-decision incident. |
| `HC-R02` | Ordinary-play bounded-discovery fairness and absence of hidden-state radar. |
| `HC-R03` | Burned-pair evidence, coherent egress, covert neutrality, and identity continuity. |
| `HC-R04` | Dead/missing/split-survivor knowledge, report revision, and mission-slot release. |
| `HC-R05` | Production scheduling of a fresh response and arbitration against duplicate movement owners. |
| `HC-R06` | Complete physical bandit shakedown lifecycle. |
| `HC-R07` | Complete physical cannibal darkness-raid lifecycle. |
| `HC-R08` | Authoritative aftermath, finite-resource contention, renewable opportunity, and replay-safe repeat. |
| `HC-R09` | Changed-executable save/load at every named live lifecycle boundary. |
| `HC-R10` | Full-path performance, three-platform runtime/package qualification, and release gate. |

`W` is the likely write set, `O` the authoritative owner set, and `P` mutable proof artifacts.
Unknown intersections serialize. Tests and review belong to the slot whose contract they prove;
there is no generic test or review slot.

| Slot / dependencies / red IDs | Primary production owner; bounded worker job; profile | Likely `W`; authoritative `O`; mutable `P` | Parallel condition | Necessary merge proof and coordinator checkpoint |
|---|---|---|---|---|
| `T01` / none / `HC-R01` | `do_turn.cpp` local/abstract handoff adapter. Isolate the first post-`returning_home` owner seam, repair only that transition, and preserve the scenario's geometry and timing. **Terra high.** | `W`: `src/do_turn.cpp`, the implicated handoff contract in `src/bandit_live_world.{cpp,h}` or `src/overmapbuffer.{cpp,h}`, and its focused owning test only. `O`: outing cursor, handoff epoch, overmap NPC storage, local ecology movement, canonical return/report owner. `P`: changed Mac executable, unchanged McWilliams fixture/scenario working copy, focused-test and incident artifacts. | Round 1: only this slot. Later parallelism requires disjoint `W/O/P` re-proof. | Focused proof must isolate the next boundary/dematerialization transition; the unchanged `bandit.scout_to_decision_observer_live_mcw` incident must then order paired physical boundary -> camp dematerialization -> canonical return -> final report -> matching decision. Both are necessary: the former distinguishes the owner fix from harness luck; the latter distinguishes gameplay from helper or abstract-jump success. Coordinator accepts only the first-seam diff and that event/identity chain, otherwise returns `T01`. |
| `T02` / `T01` / `HC-R02` | `bandit_live_world` bounded perception/frontier and private-lead owner. Close only missing ordinary-play fairness evidence or the first production seam it reveals. **Luna high.** | `W`: `src/bandit_live_world.{cpp,h}`, a physical-emitter producer only if causally implicated, and owning natural-test/scenario files; harness code only for a proved visibility gap. `O`: physical observations, bounded visibility/risk query, private camp lead. `P`: changed executable and the existing quiet/visibility/signal/relocation route fixtures and run artifacts. | Not with any slot sharing `bandit_live_world`, the executable, fixture mutations, or harness output; unknown producer overlap serializes. | Retain only controls that distinguish an explicit contract variable: quiet inside the old radius versus a credible signal; clear day/dusk/unlit night; road versus forest/weather with and without optics; decoy versus real signal; relocation; unseen versus legitimately observed zombie danger. Coordinator checks that no exact avatar, inventory, defender, storage, or hidden-zombie fact enters the owner; duplicate matrix cells fail MSW and are removed. |
| `T03` / `T01` / `HC-R03` | Loaded ecology movement/derived-attitude owner for the exact reserved pair. Close burned egress without creating a faction-state shortcut. **Terra high.** | `W`: `src/do_turn.cpp`, `src/npc.{cpp,h}`, `src/npcmove.cpp`, implicated route/evidence code in `src/bandit_live_world.*`, and the owning burn proof. `O`: reciprocal visual contact, persisted egress route, next-action ecology intent, ordinary survival/combat precedence. `P`: changed executable, visible-burn fixture, incident artifacts. | Not with `T01`, `T02`, `T04`, or response slots unless all movement, state, binary, and fixture intersections are proved empty. | One natural visible-burn incident plus a quiet/unattacked control is necessary to distinguish burned evidence/committed egress from ordinary covert neutrality; it must show no pacing, false anger, or route/report identity replacement. Coordinator returns any diff that persists a fake faction change or lets debug state create the burn. |
| `T04` / `T01` / `HC-R04` | `bandit_live_world` casualty reconciliation and report/application-watermark owner. Close survivor-scoped knowledge and slot release through the authoritative death route. **Luna high.** | `W`: `src/bandit_live_world.{cpp,h}`, `src/do_turn.cpp` only if death reconciliation is implicated, and the owning casualty/report proof. `O`: roster state, outing resolution, survivor evidence, report revision, application keys. `P`: changed executable, casualty fixture copies, incident artifacts. | Serialize against any slot sharing roster/report state, the executable, casualty fixtures, or harness output. | The minimal discriminating packet is: both dead -> no informed response and no wedged slot; one survivor -> partial/provisional report; later survivor -> revision rather than duplicate. Each branch distinguishes an explicit knowledge outcome, so deleting one leaves `HC-R04` unproved. Coordinator verifies stable operation/member/report identities and authoritative deaths before closure. |
| `T05` / `T01` / `HC-R05` | Hourly hostile-camp scheduler plus fresh-operation reservation owner. Wire `plan_hostile_operation` and the first real phase transition without duplicating strategic or NPC movement. **Terra high.** | `W`: `src/do_turn.cpp`, `src/bandit_live_world.{cpp,h}`, `src/npc.cpp`/`src/npcmove.cpp` only for a proved yield seam, and owning scheduler tests/probe. `O`: camp decision, fresh reservation, generation/epoch cursor, ordinary NPC/optional-LLM yield. `P`: changed executable and natural decision-to-reservation artifacts. | Only after `T01`; serialize with slots sharing scheduler, reservation, movement, executable, or fixture state. | A focused owner control must reject stale/duplicate generation and reuse of the scout reservation; a changed-executable incident must naturally turn the matching final decision into one fresh response and advance it through its first production transition. The two proofs distinguish owner arbitration from test-only planner scaffolding. Coordinator accepts only one strategic owner and returns double advancement or LLM-created ecology truth. |
| `T06` / `T05` / `HC-R06` | Bandit policy within `active_hostile_operation`; ordinary dialogue/combat/storage remain subordinate contact owners. Complete rally, open demand, outcome, physical return, and writeback. **Terra high.** | `W`: `src/bandit_live_world.*`, `src/do_turn.cpp`, implicated `src/npc*`, `src/npctalk.cpp`, `src/basecamp.*`, and owning shakedown proofs. `O`: shakedown phase/reservation, narrow parley neutrality, reachable storage/payment, committed combat, return. `P`: changed executable, shakedown fixtures, paid and refusal/combat incident artifacts. | After `T05`; not parallel with other response/aftermath slots while strategic, movement, contact, binary, or fixture sets intersect. | One paid branch and one refusal-or-attack branch are the minimum controls that distinguish a real demand/payment path from premature combat and distinguish escalation/combat/return from dialogue-only success. Both must rally physically, close casualties/survivors, return, and write back exactly once. Coordinator returns teleport, invisible payment, broad patrol neutrality, or missing replay-safe closure. |
| `T07` / `T05` / `HC-R07` | Cannibal policy within `active_hostile_operation`; ordinary loaded combat owns committed contact. Complete physical rally, true-dark wait, attack, return, and writeback. **Terra high.** | `W`: `src/bandit_live_world.*`, `src/do_turn.cpp`, implicated `src/npc*`, and owning raid proofs; no payment UI path. `O`: raid reservation/phase, local darkness, loaded defender set, committed combat, return. `P`: changed executable, cannibal fixture, day-wait and darkness-attack artifacts. | After `T05`; serialize with response/aftermath slots sharing strategic, movement, combat, executable, or fixture state. | A pre-darkness hold and later true-dark attack in the same causal route are both necessary to distinguish a darkness policy from elapsed-time attack. The incident must engage the avatar and all loaded defenders, expose no payment UI, cause no offscreen defender death, and physically reconcile survivors/casualties. Coordinator returns any bandit-policy leakage or debug-triggered contact. |
| `T08` / `T06`,`T07` / `HC-R08` | `bandit_live_world` aftermath/resource/opportunity and idempotent application owner. Implement authoritative renewal and faction-specific future eligibility. **Terra high.** | `W`: `src/bandit_live_world.{cpp,h}`, implicated `src/basecamp.*` opportunity producer, and owning resource/repeat proofs. `O`: global finite bounty, private estimates, outcome memory, payment/plunder, cooldown plus renewed opportunity, application watermarks. `P`: changed executable, two-camp contention and before/after-renewal fixtures/artifacts. | Only after both response policies settle their outcome packets; serialize with slots sharing aftermath, resources, opportunity, binary, or fixtures. | Two camps contesting one site distinguishes global consumption from duplicated private belief; repeat attempts before and after a real stored-goods/population/activity renewal distinguish cooldown-only replay from renewed opportunity. Faction aftermath must apply once. Coordinator rejects timer-created value, stale-report replay, or duplicate writeback. |
| `T09` / `T01`,`T06`,`T07`,`T08` / `HC-R09` | `bandit_live_world` serializers plus the active production owner on each boundary. Prove, and repair only if first-seam evidence requires, save/load causality at the named live boundaries. **Sol medium.** | `W`: `src/bandit_live_world.{cpp,h}`, implicated `src/game_io.cpp`, and the owning boundary tests/scenarios/fixtures only. `O`: serialized camp/operation identity and local/abstract, split-return, shakedown-contact, and darkness-wait owners. `P`: changed executable, exact pre/post-save fixture copies and artifacts. | Dependencies must be green; cross-boundary persistence, executable, and fixture intersections otherwise serialize. | One save/reload at each explicitly named boundary is necessary because each distinguishes a different owner transfer: local/abstract handoff, split return, shakedown contact, cannibal darkness wait. Each must preserve generation/epoch/member/application identity and resume via production. Coordinator returns schema-only or raw-save-rewrite evidence. |
| `T10` / `T01`-`T09` / `HC-R10` | Platform runtime/package qualification and, only with new owner authority, the porting-orchestrator release owner. Measure the completed path. **Sol medium.** | `W`: no gameplay change unless a measured failure reopens its owning slot; only implicated portable build/package/workflow files. `O`: macOS, Linux/WSL, Windows scheduler/loaded cost and save behavior; project packaging and release-branch flow. `P`: exact per-platform binaries, metrics, saves, packages, and candidate-playtest artifacts. | Final dependency gate; no concurrency with an open implementation or persistence slot. | The same completed production path needs named CPU/scheduler, retained-memory, save-size/load-latency, runtime, and packaging evidence on each required platform; deleting a platform route leaves the platform contract unproved. `S.A.integrated_proof` is not named, so T10 adds no final live run, adversarial review, or full-diff review beyond the explicitly named specification and platform proofs. Final promotion must use the reviewed orchestrator path and requires fresh explicit authority before touching `port/cdda-master`. Coordinator closes only a clean, identity-matched candidate with no red predecessor. |

### Round-1 concurrency and worker-follow protocol

The evidence-derived maximum safe round-1 concurrency is **one**, and the only qualifying slot is
`T01`. It is the active physical-return blocker. Every other slot either depends on it or has a
known/unknown intersection with its `W`, authoritative movement/report owners in `O`, or its Mac
executable/harness/fixture artifacts in `P`. Thus
`parallel(i,j)` is allowed only when `W_i ∩ W_j = ∅`, `O_i ∩ O_j = ∅`, and
`P_i ∩ P_j = ∅`; unknown intersections serialize. Recompute this before every later launch.

Worker follow is finite and same-slot: assignment receipt -> bounded progress evidence -> returned
diff and artifacts -> coordinator checks the slot's contract-specific proof -> same-slot remediation
or closure. A finding, test, or review enters only when deleting it would leave that slot materially
unproved; it never creates task eleven or a detached test-writing program.

At every slot closure, the coordinator applies this proof-integrity gate:
exact source, binary, and fixture identities match; staged setup ends before the asserted behavior;
no helper, mock, raw-save transform, debug setter, teleport, or handwritten artifact manufactures
the outcome; the transition comes from the authoritative production owner; positive/negative
controls distinguish the claimed mechanism; and test-only code is never credited as gameplay.
Under MSW, only an identity or evidence-integrity finding whose deletion would leave the contract
falsely green reopens the same slot.
