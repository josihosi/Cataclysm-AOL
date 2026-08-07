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

## Proven state

- The `DEBUG_CLAIRVOYANCE` ecology observer reads the authoritative camp/dispatch projection for
  UI, compact JSON, selection/follow, deltas, watches, incident capture, and labelled intervention.
  Its disabled-cost and save-neutrality proof is checkpointed at `117857f551`; authoritative
  casualty intervention and reconciliation are checkpointed at `1e6a0924e7`.
- Focused tests cover authority, persistence, exact-pair roster ownership, perception,
  route/handoff, cohesion, physical return/report/decision, resource, fairness, and loss controls.
  They do not substitute for a live incident.
- Committed checkpoints through `25be8f51c8` make frontier watch outings advance, route an exact
  pair through transactional staging, latch completed assembly during forward ingress, and expose
  movement orders, route results, and path steps through the existing observer event.
- Run `20260807_094606` on clean `25be8f51c8+SDL3` naturally dispatched and assembled the exact pair
  with two successful staging routes and 21 path steps. The pair then split under the local ingress
  owner: saved NPC `4` remained at staging while NPC `5` returned to camp, although both retained
  the same watch goal. No watch arrival, observation, survivor return, final report, or camp
  decision was proved.
- The loaded forward-ingress owner now takes motor priority over generic NPC behavior for both
  reserved members after assembly. The focused `[local_handoff]` integration gate passed 833
  assertions in two cases and proved both members physically enter the selected watch OMT before
  the ordinary overmap cadence commits the transactional arrival.

## Active claim

Rerun `bandit.scout_to_decision_observer_live_mcw` unchanged on the executable containing the
loaded forward-ingress owner. The natural incident must carry the same exact pair through watch,
egress, survivor return, final report, and authoritative camp decision; the focused owner test does
not substitute for that outcome.

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

## Next milestone

After the natural scout-to-decision incident satisfies the next unchecked `SUCCESS.md` outcome,
continue the decided physical bandit owner through the shakedown lifecycle. Upstream refresh,
packaging, release, and integration remain outside this lane until qualification is active.
