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
- Checkpoint `b998327e91` lets an already-assembled homeward resume reuse its complete adjacent
  entry pair as staging only at the first later non-camp route waypoint. Focused route/motor/camp
  dematerialization proof passes 94 assertions and the owning case passes 1,054 assertions.
- Unchanged run `20260809_042151` on `b998327e91+SDL3` (SHA-256 `d96514f3ef2a20e22fd75faa082e481a40f43baa357b2367a8141403508c1ea7`)
  kept the fresh identity-bound incident gate green but still found no complete later-waypoint
  entry allocation, then incorrectly granted abstract return. The current rejection aggregates
  entry and staging failure, so it does not yet identify the remaining physical geometry seam.
- Checkpoint `3f781eecf1` adds bounded read-only later-candidate counts. Unchanged run
  `20260809_043442` on its exact SDL3 binary (SHA-256
  `c99a87ed828cd90ad99dde5127bc68738fdfceab4d69f2e06cb1e064f207524c`) advanced further: the exact
  generation-1 pair committed local `returning_home` ownership at `(164,34,0)`, then remained
  loaded/local through four observed scheduler hours without a homeward boundary or camp
  dematerialization.
- Checkpoint `8ab8fcb84b` adds a cadence-bounded read-only homeward motor receipt and repairs two
  test-only dangling references exposed by its layout; `[local_handoff]` passes 94 assertions and
  the owning case passes 1,054 assertions. Unchanged run `20260809_045732` on its exact SDL3 binary
  (SHA-256 `b96ef49272fef891c4770da18ed50674af67f1d98fc131057f44058107999577`) did not reach local
  homeward ownership. It instead proved that an outing whose earlier forward resume had been
  consumed could receive abstract return credit with no physical camp dematerialization.
- Checkpoint `588cf29c69` requires a schema-10 scout return to retain exact physical ownership until
  its same-operation resume is physically at the owning camp, and preserves the strategic return
  clock across local ownership. The full owning case passes 1,069 assertions and the adjacent
  assessment/report case passes 152 assertions at seed `123456`.
- Unchanged run `20260809_053039` on the exact `588cf29c69+SDL3` binary (SHA-256
  `4ceec8f8003cb35c12fc9bceedd5d11181022e204a99091ba3cbb82aee05e0c2`) kept the fresh
  identity-bound incident gate green, withheld all abstract return/report/decision credit, and
  committed the exact generation-1 pair to local `returning_home` ownership at `(164,34,0)`. Both
  members then remained at `(3936,828,0)` and `(3936,829,0)` with travelling goals, six-OMT routes,
  and no local path through four scheduler hours; no homeward boundary or camp dematerialization
  occurred.
- Checkpoint `501b66c61f` exposes only the selected homeward boundary departure, exit, and exact
  departure equality. Unchanged run `20260809_055430` on its exact SDL3 binary (SHA-256
  `56914f6692fab6bf40cc4e3f86d044d29854ddb063d186a4e9349799e5614cb9`) proves both members
  remain 118--119 map squares short of distinct adjacent departures with `local_path=0` through
  five scheduler receipts. Boundary completion is never attempted; the red seam is route
  acquisition or movement to the selected departure.
- Checkpoints `d1dca64971` and `959f3c0e96` add the bounded route-result receipt and correct its
  live gate. Unchanged run `20260809_063218` on the exact `959f3c0e96+SDL3` binary proves member 4's
  actual combined solve repeatedly returns no route and an empty path, consumes its move budget by
  pausing, and never calls movement. The pair therefore cannot reach the boundary transaction.
- Checkpoint `2f916249a0` compares bounded diagnostic routes to the same departure without mutating
  the actor path. Unchanged run `20260809_065048` on its exact SDL3 binary proves baseline,
  ordinary-NPC-only, covert-only, and actual combined point routes are all empty. The far selected
  bubble edge is unreachable under ordinary local pathfinding; neither avoidance layer causes the
  rejection.
- Checkpoint `6287923514` falls back from an unreachable selected boundary point to the existing
  nonreentry-safe one-OMT motor. Its accepted red control held both members still (132/136
  assertions), while the repaired focused section moves both toward the next persisted waypoint
  and later dematerializes at camp (266/266); the full owning case passes 1,241 assertions.
- Checkpoints `66e137b56a` and `0a124a769b` keep ordinary split wait progress passive and match exact
  OCR phrases only from left-to-right observations on one overlapping visual row. The owning
  interruption/wait scope passes 62 tests; wrong identity, separate rows, and reversed order remain
  red. These are observer proofs, not gameplay credit.
- Final fuse run `20260809_074804` on exact `6287923514+SDL3` (SHA-256
  `953947702c6787bdc4a386f51de13999eebff08b1c7162e6b42cb80819378169`) published a fresh natural,
  intervention-free identity-bound incident pair and committed generation 1 to local
  `returning_home` epoch 3 at `(164,34,0)`. The fallback physically advanced the scouts from
  `(3936,828/829,0)` to `(3941/3940,912,0)`, then the selected camp-adjacent departures
  `(3935/3936,947,0)` remained unreachable and neither scout moved in four hourly receipts. No
  camp dematerialization, return, report, or decision credit is accepted.

## Active claim

The bounded complete-pair discriminator is focused-green at `be77732d45`, but the exact
`20260809_074804` H1/H0 result is unresolved because its preserved save predates the loaded pair
and its artifacts contain no complete-candidate receipt. Under the current no-live and no-fixture-
construction authority, T01 is blocked. Resume only from an existing exact-state artifact or new
authority to execute the read-only discriminator. H1 admits only route-reachable pair selection;
H0 returns the material geometry/product choice to Josef.

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
