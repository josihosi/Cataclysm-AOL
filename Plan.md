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
- Clean checkpoints `2606701d4e`, `2b263a6354`, and `6c1a574ba6` route frontier scouts through the
  watch owner, allow canonical frontier watch outings to advance, and release an assembled local
  pair from staging into a transactionally completed physical watch arrival. Focused Mac tests
  cover split-arrival rejection, exact-pair destination ownership, replay, and save round-trip.
- Run `20260807_083248` on `2b263a6354+SDL3` naturally dispatched, handed off, and assembled the
  exact pair, proving the southwest geometry. It then remained at the ingress waypoint through the
  six-hour window with zero observations or return; that stalled motor is fixed at `6c1a574ba6`.
- Run `20260807_090025` on clean `6c1a574ba6+SDL3` naturally dispatched and committed the exact-pair
  handoff at `(164,34,0)`, but cohesion changed from `assembled=no, abort=no` to
  `assembled=no, abort=yes` before watch ingress. Verdict:
  `blocked_scout_to_decision_pair_handoff_missing`; no lifecycle credit. No run yet proves the
  complete natural survivor return, final report, and camp decision chain.
- Checkpoint `81c8a9f46c` puts the first same-minute staging orders through cohesion's transactional
  route accounting. Run `20260807_091941` on its clean SDL3 binary naturally reached the exact-pair
  handoff and `assembled=yes, failed_routes=0, abort=no`, then exposed the next owner defect: forward
  travel revoked the completed staging gate on every step, oscillated the pair back to staging, and
  eventually forced `returning_home` by the rendezvous deadline. The current fix latches completed
  assembly while the forward ingress motor owns that route. Focused Mac local-handoff tests pass
  824 assertions.

## Active claim

Checkpoint the forward-ingress assembly release, build a clean SDL3 binary from that commit, and rerun
`bandit.scout_to_decision_observer_live_mcw` without changing its causal contract. The run must
either reach the natural survivor-return/report/decision pass or preserve the first new
authoritative blocker.

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
