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
- Committed gameplay checkpoints through `ff981207da` now carry the exact pair through natural
  discovery, transactional forward ingress, a paired reality-bubble exit, watch completion, and a
  distance-derived future home leg. Focused `[local_handoff]` and `[scout_assessment]` tests prove
  the corresponding owner transitions, including coherent homeward route binding.
- Live run `20260807_110822` proved the final non-provisional report and matching
  `report_awaiting_assessment` decision, but only after an abstract jump home; it therefore earns no
  physical-return credit.
- Latest unchanged run `20260807_121217` on `ff981207da+SDL3` proved the future home schedule, then
  remained abstract until canonical reconciliation. It emitted neither the required
  `returning_home` handoff nor camp-boundary dematerialization. Existing diagnostics do not expose
  which live materialization preflight rejected the pair, so that rejection is the first unproven
  seam.

## Active claim

Identify the first authoritative live materialization preflight that rejects the exact homeward
pair in the preserved fixture, using the smallest route-level isolation or homeward-only diagnostic
that distinguishes the existing rejection branches. Repair only that proven seam, then rerun
`bandit.scout_to_decision_observer_live_mcw` unchanged. The incident must show a `returning_home`
local handoff and physical boundary exit for the exact pair before the already proved canonical
home reconciliation, final report, and authoritative camp decision.

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
