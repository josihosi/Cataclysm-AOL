# NPC AI interaction coverage and next tests

Companion to [the actual control map](npc-ai-control-map.md), 2026-10-02. This specifies focused additions to existing tests, not a new framework. R061 has now exercised the combined sequence through reunion and the scheduler sleep controls; see [the verified facts and limits](npc-ai-control-map.md#established-facts-and-remaining-risks). Other proposed sequences remain unimplemented unless linked evidence says otherwise. Existing test names and source hashes are indexed in `.de67/task-logs/npc-ai-review-20261002/inventory.json`.

## Test the boundaries, not every named monster

Use a representative actor for each **different production mechanism**: adjacent melee, ranged pressure, pursuit/encirclement, field creation, knockdown/stun/grab, vertical movement and mixed friend/foe targeting. A zombie and ant sharing the same threat/movement route need not duplicate every test. A special attack that creates acid, webs or another movement effect does need an additional control. Keep species identity in the evidence.

| Interruption family | Dispatch/assembly | Travel | Watch | Contact/combat | Withdrawal/return | Reload/partial loading |
|---|---|---|---|---|---|---|
| Hostile creature or player attack | Readiness vs newly arrived threat | Survival must get control; preserve assignment | Acquisition stops when observer cannot observe | Select actual threat; record dealt/received harm | Escape/reunion and safe resumption | Same identity, wounds and intended return; no duplicate movement |
| Player plus zombies / several factions | Independent parties and knowledge | No group-level omniscient target | Site sight differs from combat target | Hostility and friendly-fire decisions | Survivor/casualty split | Local result carried once; no off-screen invented kill |
| Fire, smoke, gas, explosion | Hazard at staging slot | Escape versus route/cohesion | Exposure interrupts; signal evidence is not hazard immunity | Hazard priority vs attack/withdrawal | Safe detour or honest inability | Persist effects; don't assert full off-screen field simulation |
| Sleep, lying down, forced incapacity | Ready party vs defer dispatch | Voluntary rest policy vs forced halt | Sleeping member cannot acquire/share sight | Ordinary bedtime vs genuine collapse | Awake member/disabled partner; later resumption | Effect expiry, schedule and motor entry tested together |
| Food/water/warmth, medicine | Competing readiness and self-care | Detour competes with route | Needs interrupt finite observation honestly | Urgent care versus target action | Resume route without losing actor identity | Transient plans rebuilt; no invented consumption |
| Loose loot, corpse, equipment, harvest | Pickup is not dispatch success | Attractive item, ownership, capacity | Item action cannot count as observation | Danger interrupts pickup/forage; attack intent ≠ hit | Cargo conserved; mission restored | Missing/moved item, stale item location, interrupted backlog |
| Door, ladder, ramp, vehicle, occupied tile | Both slots actually available | Member-specific route capabilities | Physically arrive before assessment | Entry/search/fire line and occupancy | Exit from roof; blocked ladder; regroup | Active/inactive split and exact coordinate conversion |
| Command/activity change | Assignment superseded explicitly | Follow/goto/job vs group ownership | Cancel old commitment | Peaceful talk/Pay/Fight transition | Old route must not retake control | Persist authority; don't restore obsolete mission |

This matrix selects cases; it is not a mandatory full Cartesian product. For each selected cell use the actual caller and prove the observable transition. Extend it when a new mechanism changes control, not simply when a new creature name appears.

R062 now exercises finite alarm waking through actual resident and generated home/party callers, including hearing/membership, forced incapacity, expiry/reload, real flight and peaceful controls. See [bound findings](../build_logs/first-smoke-062/resumed/condition-behavior-caller-facts.json); source acceptance does not pass the [native night arms](../.agents/skills/caol-harness/references/playtest-suite.md#night-alarms-and-party-response).

R063 extends production coverage for schema9 terrain scouting through physical loaded-edge departure, outer visit, home return and report, with actual save/cleanup/load and partial-member/incapacity controls in `[frontier_journey_063]`. `[earlier_report_063]` exercises the exact saved normal report through bandit333 authorization and atomic target-claim dispatch, including wrong identities, failed application, conflicting/consumed claims and reload/replay. Final43 cases/42,507 assertions pass; five adjacent baseline fixture failures remain unchanged. [Evidence and declared fixture limits](../build_logs/first-smoke-063/result.md). Native retained terrain, hazards, demand/payment and combat remain separate proof.

R064 adds `[retained_boundary_064]` with actual12120 terrain/avatar/NPCs/ambient monsters, invoked motors, actual unload and consumed destination-first path. Reload, partial loading, incapacity and flight controls complement the controlled frontier journey. [45 cases/46,685 assertions and evidence limits](../build_logs/first-smoke-064/result.md); temporary world options/weather and native outcome remain distinct.

## Prioritized production sequences

### 1. Interrupted party across representations

Extend `tests/bandit_live_world_test.cpp` alongside `[separated_homeward_055]`, `[inactive_homeward_060]` and assembly tests. Reuse existing complete actor/world fixtures and `process_overmap_npc_move_for_test`/the actual local turn wrapper.

- Dispatch an ordinary ready pair; establish actual positions and member-specific paths.
- Introduce a real native survival interruption that separates them; one member's next path is blocked by terrain/occupancy.
- Move the bubble so only one member unloads, then serialize actual world and actor copies and load through reconciliation.
- Remove the temporary obstruction/threat by the test's declared world change; advance the real due scheduler and local motors.
- Assert identity, ownership/epoch/cursor agreement, no duplicate move/death/cargo/report application, and actual reunion or explicit supported survivor/abort return.
- Negative controls: foreign lease/member, permanent obstruction, asleep/incapacitated partner and confirmed casualty. Temporary refusal must recover after changed conditions; permanent refusal must not mint a report or teleport.

Existing individual component tests are valuable; they do not establish that this entire sequence passes. The owner separately reopened the preserved scattered-party save5274553/min8709. Keep that native arm separate from the clean R051 checkpoint5234125/min8035. The newer owner instruction supersedes older receipt wording that called8709 historical-only; preserve both histories.

### 2. Sleep through the scheduler

Extend `tests/npc_test.cpp` near `npc_sleep_preemption`, using the real local-turn wrapper rather than only `guy.move()`:

- Begin lying down but not asleep, introduce critical thirst/real threat, run effects plus motor selection, and record whether/when preemption occurs.
- Controls: actually asleep, suspended, narcosis, stun/downed and ordinary tired-but-awake. Preserve genuine incapacity; don't fix the test by deleting every effect.
- Repeat relevant duty boundary with a scheduled NPC and an active assault actor. First establish intended behavior from existing FS/owner duty scope; no blanket faction schedule inferred.
- Assert invoked branch and resulting action/effect, not only changed `committed_goal`.

### 3. Hazard priority across every direct movement entry

In existing faction tests parameterize **assembly, ingress, ordinary travel, homeward and hostile withdrawal** with: fire under actor, nearby uncontained fire, imminent explosive, then fear effect without a visible target. Use actual map fields/effects and ordinary scheduling. Verify survival gets the expected route/action, or capture the exact refusing branch. Remove hazard and prove resumption. Existing homeward fire/reunion controls and R060 flight control are reused, not replaced.

Do not weaken covert non-entry/ownership rules to make an arbitrary path pass. If survival and that rule genuinely conflict, present the concrete trapped geometry as a gameplay choice.

### 4. Loot/needs/activity interruption and resumption

Extend `tests/npc_test.cpp`, `tests/llm_intent_test.cpp` and existing activity tests:

- Valuable ground item → native pickup path → enemy/field interrupts → item moves/disappears while interrupted → safe continuation.
- Verify no phantom item/cargo, repeated impossible fetch or stale goal overriding travel. Include ally pickup disabled/ownership/capacity controls.
- Repeat with need-driven food/water and forage/harvest activity because they use different entry points. Verify real consumption and backlog/mission restoration. Use existing `npc_forage_yields_to_danger`, executor-contract and unavailable-target tests as components.
- Optional enabled LLM pending response plus hazard: test the early pause ordering directly. Keep LLM disabled control. A failing production test would support a small priority repair; source ordering alone is not an authorization to rewrite the integration.

### 5. Knowledge and actual return

Reuse `[watch_arrival_057]`, `[fresh_watch_report_058]`, `tests/scout_observation_test.cpp`: physical exact-three arrival → fresh site-bound light/smoke/exterior observation → observer interruption → reload → surviving carrier reaches home → due hourly reducer → decision. Assert unseen interior remains unknown, no old-lead substitution, deduplicated buckets and once-only report. Include no-carrier/no-current-evidence controls. Do not invent demand/attack if the actual camp lacks capability.

### 6. Vertical encounter and withdrawal

Reuse roof route, combat and withdrawal controls: ordinary ground approach → actual ladder/stairs → occupied connector/roof fire → attack or panic → descent → return. Add a ramp path only where actual terrain/path settings admit it. Native acceptance may include death from attributed combat; it need not require the raider to win. Selected action, landed damage and killer/cause are different evidence.

## Bandit encounter sequence coverage

Use the acceptance matrix in [the consolidated encounter specification](npc-bandit-encounter-spec.md#production-evidence-and-acceptance): first-sight approach, audible indoor demand, camp-NPC/remote-avatar response and local goods conservation, third-party interruption, actual attack/Fight, overmap wait-to-local activation, and reload/idempotence. These are required behavior distinctions, not claims that tests already exist or a Cartesian-product quota. R066 is the current source owner; R051 retains native ownership and compatible saves. Update evidence links as each sequence is actually proved, preserving counterexamples and unproved branches.

## Compact evidence surface

Reuse `raid_actor_action`, damage/death/sleep edges, `scout_homeward_motor`, transition events and the existing NPC inspection reader. First join existing facts by **run/build + exact turn + actor ID + site/operation/generation**. Saved turn and live observed turn must be distinct.

When a required fact is absent, add it to the existing opt-in record at its actual producer:

- **Scheduler admission:** entered/skipped and exact reason (sleep-state subtype, suspended, dead, moves, inactive or selected other motor). Emit changes or selected bounded scope, not every NPC every turn.
- **Control selection:** motor/caller, current assignment identity, actual selected action, target ID, mission/attitude/activity/committed need. Missing action row must distinguish skipped loop from scope expiry.
- **Movement result:** old/new position and z, requested next step/goal, route size, refusal category and actual known blocker. Reuse the computed route; never rerun pathfinding only to log it.
- **Resumption:** which interruption cleared and which still-valid assignment resumed, changed condition or next due cadence. Unknown remains unknown.
- **Items/knowledge:** real item identity and transfer count when relevant; evidence source/sense/time/receiver/carrier for watch/report. No inventory or world dumps by default.

The existing first-pair snapshot limitation is already assigned as deferred simultaneous-site tooling. Extend that owner’s work rather than create a competing query API. Exact operation timings remain separately assigned; do not infer simulation cost from a prompt wait or from log wall time.

## Acceptance and maintenance

For a behavior change, run the affected production sequence with the old behavior to establish failure, then the corrected implementation and adjacent negative controls. Live Luna proof follows only where native interaction is the remaining question. Preserve accepted R054–R060 evidence and current saved continuation.

For future investigations, enter through the controlling caller in the map, locate its receiving assumptions, and choose the smallest sequence that could falsify them. Update the relevant row when code changes. No extra audit gate, universal trace requirement, fixed test quota or new coordinator framework is introduced.

The initial review added documentation and indexing. R061 subsequently implemented the combined sequence through reunion and scheduler controls, then repaired the separate loader/motor gap. [The delivered regression](../build_logs/first-smoke-061/resumed/result.md) fails on old product and proves fixed-player actor movement, real reload, map boundaries, exclusions and sleep/incapacity controls on the repaired product. Home delivery and native recovery remain open. The original inventory is a dated baseline, not a hash binding for R061. No all-NPC green claim is made.

Owner-required native coverage: [waiting outside local simulation and camp-NPC contact with adjacent-OMT player](../.agents/skills/caol-harness/references/playtest-suite.md#bandit-waiting-and-camp-npc-contact). Both remain pending; source regressions alone do not close these outcomes.

The [encounter variability checklist](../.agents/skills/caol-harness/references/playtest-suite.md#encounter-variability-simple-coverage-checklist) maps bandit branches onto cannibal cases and identifies missing native outcomes. It permits shared-segment credit only with actual caller/branch/input equivalence, not shared function names; no full faction equivalence has been proved.
