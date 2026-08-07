# Plan

This is the sole task description and roadmap for the active Mac hostile-camp ecology lane.
`SUCCESS.md` defines proof of completion, `TODO.md` names the current necessary claim, and
`TESTING.md` records only the proof policy and current evidence.

## Contract

On `dev`, make naturally generated bandit and cannibal camps feel like physical factions rather
than omniscient event generators. Camps dispatch coherent two-person scouts; scouts discover
targets through ordinary bounded perception, travel and stalk physically, react coherently when
exposed, carry only surviving evidence home, and let their camp choose the faction-specific
consequence. Bandits rob through a shakedown; cannibals mount a night raid.

Josef and the harness must be able to observe the authoritative state and collect compact causal
evidence without save archaeology, OCR dependence, a second simulation, or debug code that writes
the behavior being proved.

The smallest completion contract is the outcome ledger in `SUCCESS.md`: authoritative observer,
one natural scout-to-decision incident, one physical bandit shakedown lifecycle, one physical
cannibal night-raid lifecycle, and release-relevant performance/save/platform qualification.

## Proven state

- The `DEBUG_CLAIRVOYANCE` ecology observer reads the authoritative camp/dispatch projection for
  UI, compact JSON, selection/follow, deltas, watches, incident capture, and labelled intervention.
  Its disabled-cost and save-neutrality proof is checkpointed at `117857f551`; authoritative
  casualty intervention and reconciliation are checkpointed at `1e6a0924e7`.
- Authority, persistence, exact-pair roster ownership, perception, route/handoff, cohesion,
  physical return, report, decision, resource, fairness, and loss controls have focused test
  coverage. These components do not substitute for one live end-to-end incident.
- Recent live runs exposed and closed concrete local ownership, scout/shakedown intent, cohesion
  motor, and relative-wait observer seams. No run yet proves the complete natural survivor return,
  final report, and camp decision chain.
- `7af4127ade2b` is the current clean source checkpoint for the southwest observer footing. Run
  `20260807_074745` naturally dispatched and handed off the exact pair, held stable cohesion, and
  advanced the production clock from scheduler hour 142 through 148. It ended with the outing
  still active, zero observations, and zero members returned. Verdict:
  `blocked_scout_to_decision_physical_return_not_reached_in_initial_window`; no lifecycle credit.

## Active claim

Using the structured state from run `20260807_074745`, identify the first missing authoritative
transition after stable local assembly. Establish whether production still legitimately owns a
longer watch state or whether the local owner failed to advance observation/egress. Change nothing
until that distinction is proved from the existing artifact and code path. Then fix only the
reproducible contract-breaking seam and rerun the same causal contract.

Do not extend a deadline or change geometry merely to force success. Once the natural incident is
green, the next claim is to continue the decided physical owner through the bandit shakedown
lifecycle.

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

## Held decisions

Upstream refresh, Windows free play, packaging, release, and integration through `master` and the
porting orchestrator remain held until the active ecology contract reaches its qualification gate.
Historical details remain recoverable from Git and named run artifacts; they are not active
roadmap material.
