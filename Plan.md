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
- Committed gameplay checkpoints through `c7be851d23` carry the exact pair through natural
  discovery, transactional forward ingress, a paired reality-bubble exit, watch completion, and a
  distance-derived home leg. The focused `[local_handoff]` proof covers an already-populated
  local path, a camp beyond the loaded bubble, a paired exit onto the persisted overmap route,
  continued physical travel, and two-member camp dematerialization. The return repair keeps a pair
  unloaded while its persisted home route is incomplete and gives camp dematerialization a final
  pre-reload opportunity.
- Live run `20260807_110822` proved the final non-provisional report and matching
  `report_awaiting_assessment` decision, but only after an abstract jump home; it therefore earns no
  physical-return credit.
- Run `20260807_135056` on `307e43efda+SDL3` reached the exact `returning_home` handoff at
  `(164,34,0)` and completed the ordinary six-hour game window, but emitted no paired bubble-exit,
  camp dematerialization, or returned-member event. Its final harness abort was separately proved
  to be a post-wait flavor-text input error and fixed at `c74427ca37`; it did not erase the full
  gameplay window. Run `20260807_140347` did not reach this seam because the pair missed the
  existing assembly guard under accelerated time, so it is inconclusive.
- Run `20260807_142412` on `c7be851d23+SDL3` proved a valid two-member forward assembly route with
  twenty total local path steps and no route failure, then forced `returning_home` before either
  scout assembled. The focused owner test proves that an ordinary production NPC turn consumes a
  real staging order; exact live positions were still needed to distinguish route geometry from
  elapsed-time ownership at that handoff.
- Checkpoint `c846f9d929` added exact cohesion positions. Run `20260807_144329` proved that both
  scouts physically reached their staging tiles, crossed the forward boundary, completed the
  watch, and rematerialized together for the home leg. They then stopped before their first
  homeward boundary. The focused schema-10 reproduction showed why: default local A* selected a
  diagonal shortcut through the forbidden watch OMT, the post-route safety check rejected it, and
  every later turn selected the same shortcut again.

## Active claim

Checkpoint the focused homeward-motor repair, then rerun
`bandit.scout_to_decision_observer_live_mcw` unchanged. The repair supplies the existing
non-reentry contract to local A* as an avoid predicate, so the pair searches around forbidden OMTs
instead of repeatedly choosing and rejecting the same shortcut. The incident must show the paired
physical homeward boundary and camp dematerialization before canonical home reconciliation, final
report, and the authoritative camp decision.

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
