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

- The `DEBUG_CLAIRVOYANCE` ecology observer reads the authoritative camp/dispatch projection for
  UI, compact JSON, selection/follow, deltas, watches, incident capture, and labelled intervention.
  Its disabled-cost and save-neutrality proof is checkpointed at `117857f551`; authoritative
  casualty intervention and reconciliation are checkpointed at `1e6a0924e7`.
- Focused tests cover authority, persistence, exact-pair roster ownership, perception,
  route/handoff, cohesion, physical return/report/decision, resource, fairness, and loss controls.
  They do not substitute for a live incident.
- Gameplay checkpoints through `f46f8f45ca` carry the exact pair through natural discovery,
  transactional forward ingress, paired reality-bubble exit, watch completion, and a retained
  physical homeward resume. Focused tests prevent failed materialization from granting abstract
  return credit.
- Checkpoint `8f642ddd7a` makes in-bounds homeward A* apply the nonreentry rule during selection and
  permits a complete active pair physically inside camp to dematerialize transactionally. Its
  focused `[local_handoff]` proof passes 1,102 assertions; it is not live lifecycle credit.
- Checkpoints `30b27b9d5f` and `a629eb804d` preserve the original terminal observer identity across
  console close/reopen and require one fresh same-run incident JSON/screenshot pair before later
  return, report, or decision checks. Focused harness and `[debug_console]` tests are green.
- Checkpoint `9c6b73adff` advances an abstract-resume homeward pair to the first later safe loaded
  route waypoint, excluding the final camp, then uses the existing physical motor and camp
  dematerialization route. Its exact SDL3 focused proof is green; it is not live lifecycle credit.
- Live run `20260809_033014` on `9c6b73adff+SDL3` proves the exact natural generation-1
  `BD-DF9E73` pair entered local `returning_home` ownership at `(164,34,0)`. The retained observer
  then failed closed with `entity_token_mismatch` because the expected abstract-to-local owner
  transfer was treated as replacement identity, so no fresh incident pair was published and no
  later return/report/decision credit is accepted.
- Checkpoint `51c6810706` keeps world, canonical ID, generation, and authority as stable observer
  identity while treating abstract/local owner as visible mutable state. Watch, delta, and
  owner-strict intervention tests pass 506 assertions together.
- Unchanged run `20260809_035926` on `51c6810706+SDL3` published the fresh natural generation-1
  `BD-DF9E73` incident pair with no interventions. It then repeatedly rejected homeward
  materialization because the loaded return waypoint lacked a complete entry-plus-staging
  allocation and incorrectly reached abstract `members_returned=2`/`returned home`; no physical
  return credit is accepted.

## Active claim

Close T01's homeward placement seam. An already-assembled abstract-resume pair must materialize at
the first later safe non-camp route waypoint when the loaded map offers one complete adjacent pair
but no second disjoint assembly-staging pair. Forward assembly, complete-pair adjacency,
transactional rollback, and the final-camp exclusion remain strict. Then the unchanged incident
must show the `returning_home` local handoff and camp dematerialization before canonical home
reconciliation, final report, and authoritative camp decision.

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
