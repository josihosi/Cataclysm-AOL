# Plan

This is the sole task description and roadmap for the active Mac hostile-camp ecology lane.
`SUCCESS.md` defines proof of completion, `TODO.md` names the current necessary claim, and
`TESTING.md` records only the proof policy and current evidence.

## Requested outcome

On `dev`, make naturally generated bandit and cannibal camps feel like physical factions rather
than omniscient event generators. Camps dispatch coherent two-person scouts; scouts discover
targets through ordinary bounded perception, travel and stalk physically, react coherently when
exposed, carry only surviving evidence home, and let their camp choose the faction-specific
consequence. Bandits rob through a shakedown; cannibals mount a night raid.

Josef and the harness must be able to observe the authoritative state and collect compact causal
evidence without save archaeology, OCR dependence, a second simulation, or debug code that writes
the behavior being proved.

Completion is exactly the outcome ledger in `SUCCESS.md`.
The normative player-facing behavior, system ownership, and implementation audit are in
`doc/bandit-cannibal-hostile-camp-ai-spec.md`.

## Proven state

- The authoritative observer and focused owner tests cover identity, exact-pair roster ownership,
  perception, route/handoff, cohesion, physical return/report/decision, resource, fairness, and
  loss controls. Focused proof does not substitute for a natural incident.
- Exact-head natural run `20260810_004648` used
  `dev@274c4c1f239b7c68702a0b76321cb271906dd892+SDL3`, binary SHA-256
  `fbd9351d46c19b868893850b22fd32d73514469f02f82c7cfe17e52a4cea224f`, the unchanged
  `bandit.scout_to_decision_observer_live_mcw` scenario, generation 1, members 4/5, and zero
  intervention. The pair physically progressed from OMT `(164,34,0)` to camp-adjacent OMT
  `(164,38,0)`.
- Four production attempts each evaluated the same 128 complete boundary pairs with complete
  relationships and found no pair safely reachable by both scouts. The current local owner still
  returned the same independently scored unsafe pair; both routes were empty and fallback consumed
  moves without movement. No paired crossing, camp dematerialization, canonical return, report, or
  decision credit is accepted.
- The compact C4 receipt and selector prove a production selection/transition defect. They do not
  prove the unchanged world geometry genuinely entraps the pair under every valid ownership
  transition.

## Active claim — `HC-R01`

Repair the authoritative reality-bubble/overmap ownership transition without changing geometry,
fixture, scenario timing, outing/member identity, physical crossing, or nonreentry. While the exact
pair is loaded, the local owner must choose a route-reachable paired physical boundary transition
on actual loaded geometry or retain/replan ownership without physical movement, route/ownership
progress, or outcome credit. Abstract ownership resumes only from the matching committed crossing;
teleportation, direct path assignment, duplicate ownership, geometry edits, and abstract-return
shortcuts are non-credit.

Production proof must distinguish a valid alternate/recentered transfer from genuine physical
entrapment on the unchanged world, then complete the unchanged natural ordered chain: paired
physical crossing -> camp dematerialization -> canonical surviving return -> eligible final report
-> matching authoritative decision. `HC-R01` remains stable; no subordinate red ID is required.

## Boundaries

- Bandit and cannibal ecology only. Writhing-stalker, zombie-rider, flesh-raptor, and generic horde
  behavior are separate discussions.
- Gameplay truth remains with existing authoritative owners. No map-note truth, duplicate
  registry, direct discovery/phase setter, teleported outcome, or persisted observer state.
- Harness transforms may declare preconditions but stop before the claimed behavior. Every debug
  intervention is labelled and cannot be credited as a natural transition.
- Develop and verify only in the isolated Mac `dev` worktree. The production `port/cdda-master`
  checkout remains untouched until Josef explicitly promotes an integration lane.
- Keep Windows, Linux/WSL, and macOS compatibility; claim only the platform routes actually
  exercised. Qualification begins when the vertical gameplay contract is green.
- Apply the global MSW deletion rule to every proposed plan step, change, test, review finding, and
  discovered edge case. Re-proving a closed claim is not work.

## Next milestone

After the natural scout-to-decision incident satisfies the next unchecked `SUCCESS.md` outcome,
continue the decided physical bandit owner through the shakedown lifecycle. Upstream refresh,
packaging, release, and integration remain outside this lane until qualification is active.
