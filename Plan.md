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
- Unchanged run `20260807_102520` on `59879bc2bf+SDL3` kept the exact pair coherent and advanced
  both from staging `(164,34)` to adjacent tiles `(3925,818)` and `(3926,818)` with the correct
  watch goal `(163,33)`. The watch center was outside the loaded map, so generic local routing could
  not cross the last two squares and active NPCs never became eligible for abstract travel.
- The forward owner now handles that exact reality-bubble seam as a paired adjacent boundary exit.
  The focused `[local_handoff]` gate passes 878 assertions in two cases, including transactional
  watch arrival, unload, and abstract resume when the destination center is off-map.
- Unchanged run `20260807_104736` on `1f99097e22+SDL3` live-proved that seam: the pair committed
  watch arrival at `(163,33)`, then committed local dematerialization as the same exact two members.
  Night visibility yielded no observation, and the selected route had no alternate watch. The
  assessment had an existing two-hour no-progress boundary but no transition for that state, so it
  fell through to the eight-hour maximum and missed the scenario's last scheduler tick by seven
  minutes. No survivor return, report, or decision was proved.
- A selected watch with no assessment progress and no qualified alternate now returns
  inconclusive at that existing two-hour boundary. The focused `[local_handoff]` test carries the
  off-bubble pair through abstract resume into `returning_report`; `[scout_assessment]` preserves
  the separate eight-hour maximum for incomplete assessments that continue making progress.
- Unchanged run `20260807_110822` on `2a08d892b5+SDL3` completed that return and wrote a final
  non-provisional report plus the matching `report_awaiting_assessment` camp decision. All twelve
  reconciled camp members were home and no outing remained active. The saved-state audit had
  incorrectly required the five-member fixture roster and a nonempty quiet-watch report; removing
  those non-contract constraints makes the preserved authoritative state pass. The run still does
  not prove the required physical return: production advanced the abstract pair directly from the
  watch to home without a homeward local handoff.
- Run `20260807_112737` on `638808e67d+SDL3` cleanly repeated physical ingress and watch arrival,
  then stopped during the three-hour watch wait when OCR rendered the ordinary wait banner's
  percentage as `71-`. The harness acknowledged the structured wilderness-flavor popup and its
  activity query, but misclassified the remaining `Waiting ... Press ... to interrupt` banner as a
  partial safe-mode prompt. This is a harness visibility blocker, not lifecycle evidence.
- Run `20260807_113559` on `a4a8c2efdb+SDL3` passed that wait guard and proved the exact pair's
  `returning_home` local handoff at `(164,34)`. The bind cleared their abstract goals and the
  rendezvous gate treated their adjacent transactional entry as unassembled; one scout reached a
  staging slot, the other missed the existing rendezvous deadline, and the abort path cleared both
  routes. No camp-boundary dematerialization or canonical return occurred.

## Active claim

Hand the abstract returning pair to local reality at the existing homeward approach, then rerun
`bandit.scout_to_decision_observer_live_mcw` with the same causal setup. The incident must show a
`returning_home` local handoff and physical boundary exit for the exact pair before the already
proved canonical home reconciliation, final report, and authoritative camp decision.

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
