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
- Committed gameplay checkpoints through `307e43efda` carry the exact pair through natural
  discovery, transactional forward ingress, a paired reality-bubble exit, watch completion, and a
  distance-derived home leg. The focused `[local_handoff]` proof now covers an already-populated
  local path, a camp beyond the loaded bubble, a paired exit onto the persisted overmap route,
  continued physical travel, and two-member camp dematerialization.
- Live run `20260807_110822` proved the final non-provisional report and matching
  `report_awaiting_assessment` decision, but only after an abstract jump home; it therefore earns no
  physical-return credit.
- Run `20260807_135056` on `307e43efda+SDL3` reached the exact `returning_home` handoff at
  `(164,34,0)` and completed the ordinary six-hour game window, but emitted no paired bubble-exit,
  camp dematerialization, or returned-member event. Its final harness abort was separately proved
  to be a post-wait flavor-text input error and fixed at `c74427ca37`; it did not erase the full
  gameplay window. Run `20260807_140347` did not reach this seam because the pair missed the
  existing assembly guard under accelerated time, so it is inconclusive.

## Active claim

Identify why the exact live `returning_home` pair does not select or complete its persisted
reality-bubble route edge after handoff. Prefer a focused reproduction using the preserved live map
geometry and actual `omt_path` ordering; if that cannot distinguish selector absence from motor
failure, add only the homeward boundary state needed to do so. Repair the proved seam, then rerun
`bandit.scout_to_decision_observer_live_mcw` unchanged. The incident must show the paired physical
bubble exit and camp dematerialization before canonical home reconciliation, final report, and the
authoritative camp decision.

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
