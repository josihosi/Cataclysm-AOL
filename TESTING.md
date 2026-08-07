# Testing

## Current validation policy

- Prove the route Josef will play: authoritative game owner -> changed executable -> loaded game ->
  structured observer/artifact. Isolated helpers prove only the helper.
- Harness setup may establish and receipt deterministic preconditions. It must stop before the
  asserted lead, dispatch, observation, return, report, decision, or outcome.
- Prefer structured authoritative state over OCR. Add or change debug visibility only when the
  current behavior cannot otherwise be observed, and only enough to expose that subject.
- A failed run earns no lifecycle credit. Preserve its first concrete blocker; do not change
  unrelated geometry, deadlines, tooling, or mechanics to manufacture success.
- Reuse a green build/test result when its source and affected objects still match. Run only the
  smallest additional compile or test that settles the active claim.
- Performance evidence measures production behavior and save effects. Debug observer cost is
  measured separately with the gate closed and open.
- Apply the global MSW deletion rule to every proposed test and remediation. Once evidence closes a
  claim, repeating it requires a new contract-breaking reason.

## Latest relevant evidence

- Observer performance/save neutrality is checkpointed at `117857f551`; authoritative casualty
  intervention and both-faction reconciliation are checkpointed at `1e6a0924e7`.
- Focused owner tests cover roster identity, transactional route/handoff, cohesion,
  unload/writeback, physical return/report, no-progress loss, and report/decision matching. They do
  not close the live lifecycle outcome.
- Run `20260807_094606` on clean `25be8f51c8+SDL3` passed startup, zero-lead/outgoing preflight,
  natural discovery, dispatch, exact-pair handoff, two successful staging routes, 21 path steps,
  and immediate assembly. The assembly latch stayed green, but the local ingress owner split the
  pair: saved NPC `4` remained at staging `(3936,828)` while NPC `5` was at camp `(3948,936)`.
  Both retained goal `(163,33)` and the outing remained local/observing. No dematerialization,
  observation, survivor return, report, or decision occurred, so the run earns no lifecycle credit.

## Pending proof contract

Claim: one real bandit camp naturally creates terrain knowledge, dispatches its exact scout pair,
travels and watches, completes or burns coherently, physically returns at least one survivor,
creates one eligible final report, and enters the authoritative camp decision owner in one run.

Preconditions and interventions: derive the southwest fixture from the original idle McWilliams
source; retain the five-member camp at `(164,39,0)`, road-connected target `(164,30,0)`, zero leads,
no active outing, deterministic clock, and `DEBUG_CLAIRVOYANCE`. The sole player transform is
`[0,24,0]`, moving the observer to OMT `(162,36,0)`, two west and one south of the handoff waypoint
`(164,35,0)`. Record transform receipts. Do not inject any claimed ecology transition.

Causal boundary: the loaded idle camp with zero leads and no active outing immediately before the
first ordinary wait.

Real path: hourly structural scheduler -> bounded terrain discovery -> exact-pair dispatch ->
ordinary overmap route/handoff/cohesion -> watch and egress -> physical return owner -> canonical
return packet and report -> camp assessment/decision.

Expected transition: the observer follows the same stable camp, outing generation, pair, route,
survivor, report, and decision identities through one compact incident without a debug ecology
intervention.

Negative controls required by the claim: preflight zero lead/outing; no report knowledge from an
absent carrier; an empty/all-dead return cannot satisfy the survivor artifact; stale selection,
duplicate owner, or mismatched generation/report identity is red.

Time boundaries: the fixture's frontier timing derives from the production scheduler and its
persisted deadline. Handoff waits follow the scheduler's hourly cadence. The post-observation
window is the existing production observation/return interval under test: six game hours. The
controller measures `now_minutes` immediately before the wait and requires a relative advance of
360 minutes, accepting scheduler overshoot.

Pass: a surviving physical return, final non-provisional report, and matching authoritative camp
decision appear in the same structured incident. Startup, schema validity, dispatch, handoff,
assembly, or visible wait completion alone is non-credit.

Failure: preserve the run and name the first reproducible production or visibility seam that blocks
the pass condition. The next work claim must be necessary to close that exact gap.

Identities: record source commit, executable identity, fixture manifest/hash, scenario name, run
ID, authoritative owner audit, compact incident JSON, and paired screenshot where UI state matters.

Necessary validation after the owner fix:

```sh
python3 -m unittest tools.openclaw_harness.test_fixture_contract
python3 tools/openclaw_harness/startup_harness.py probe --compact-stdout \
  bandit.scout_to_decision_observer_live_mcw
```

Build the Mac tests target and run the focused local-handoff/ingress-owner test that demonstrates
both reserved members complete one transactional watch arrival. Then run the unchanged live
scenario above. The live pass still requires a surviving physical return, final non-provisional
report, and matching authoritative camp decision in the same incident.

Cross-platform performance/save/runtime qualification begins after the natural vertical incident
is green; until then only the Mac route exercised here is claimed.
