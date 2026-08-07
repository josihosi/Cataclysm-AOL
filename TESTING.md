# Testing

Current proof policy and evidence for the active hostile-camp ecology claim. Historical commands,
failed geometries, review transcripts, and detailed receipts remain in Git and named harness run
artifacts; they are not active instructions.

## Evidence policy

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

## Current scout-to-decision proof contract

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
persisted deadline. The handoff waits follow the scheduler's hourly cadence. The post-observation
window is six game hours because that is the existing production observation/return interval under
test; the controller measures `now_minutes` immediately before the wait and requires a relative
advance of 360 minutes, accepting scheduler overshoot. These values are code/fixture-derived, not
agent-created retry budgets.

Pass: a surviving physical return, final non-provisional report, and matching authoritative camp
decision appear in the same structured incident. Startup, schema validity, dispatch, handoff,
assembly, or visible wait completion alone is non-credit.

Failure: preserve the run and name the first reproducible production or visibility seam that blocks
the pass condition. The next work claim must be necessary to close that exact gap.

Identities: record source commit, executable identity, fixture manifest/hash, scenario name, run
ID, authoritative owner audit, compact incident JSON, and paired screenshot where UI state matters.

## Latest relevant evidence

- Observer performance/save neutrality: `117857f551`.
- Authoritative casualty intervention and both-faction reconciliation: `1e6a0924e7`; field run
  `20260805_101713` preserves the screenshot/incident pair and debug provenance.
- Deterministic owner tests cover roster identity, route/handoff, cohesion, unload/writeback,
  physical return/report, no-progress loss, and report/decision matching. They support diagnosis but
  do not close the live lifecycle row.
- Prior live geometries exposed concrete owner, intent, motor, structured-popup, and relative-wait
  defects that are checkpointed in Git. None produced the required survivor-return/report/decision
  incident, so none receives end-to-end credit and none is an active rerun target.
- Run `20260807_083248` on clean `2b263a6354+SDL3` naturally dispatched, handed off, and assembled
  the exact pair, then stalled at ingress through the six-hour window. This proves the southwest
  geometry and identifies the missing forward motor; it earns no lifecycle credit.
- Checkpoint `6c1a574ba6` adds exact-pair ingress destination ownership and a physical arrival
  transaction. The Mac tests target built cleanly. The scheduler tags passed 27,912 assertions in
  12 cases, and
  `bandit_live_world_production_watch_geography_adapter_is_bounded_and_owner_committed` passed 126
  assertions including split arrival, completion, replay, and save round-trip. Clean SDL3 build
  identity: `6c1a574ba6+SDL3`.
- Run `20260807_090025` exposed an initial staging path failure outside cohesion's route accounting;
  committed checkpoint `81c8a9f46c` makes first-pass staging orders transactional. Run
  `20260807_091941` on clean `81c8a9f46c+SDL3` then passed startup, preflight, frontier discovery,
  exact-pair handoff, and immediate `assembled=yes, failed_routes=0, abort=no`. It failed the
  survivor-return audit because forward steps repeatedly changed cohesion back to `assembled=no`,
  reasserted staging ownership, and reached `abort=yes` at the rendezvous deadline. Saved state
  preserved the pair alive under the local owner in `returning_home`; no lifecycle credit. The
  current fix preserves the completed assembly gate during its exact forward-ingress route. Mac
  `[local_handoff]` passed 824 assertions in 2 cases, including the new released-ingress regression.
  The unchanged live scenario on a clean committed SDL3 binary remains the pending proof.

## Focused commands

Harness contract and scenario probe:

```sh
python3 -m unittest tools.openclaw_harness.test_fixture_contract
python3 tools/openclaw_harness/startup_harness.py probe --compact-stdout \
  bandit.scout_to_decision_observer_live_mcw
```

When core ownership code changes, build the Mac tests target and run only the tags covering the
changed owner seam. Do not rebuild for this documentation cleanup or repeat a broad test suite
without a contract-derived reason.

Cross-platform performance/save/runtime qualification begins after the natural vertical incident
is green; until then only the Mac route exercised here is claimed.
