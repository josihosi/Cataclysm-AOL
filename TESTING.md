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
- Every red item in `doc/bandit-cannibal-hostile-camp-ai-spec.md` maps to an unchecked outcome in
  `SUCCESS.md`. Cross it off only with named changed-source, executable, fixture, run, and artifact
  evidence at the scope claimed; old packet documents and helper-only tests cannot close it.

## Latest relevant evidence

- Observer performance/save neutrality is checkpointed at `117857f551`; authoritative casualty
  intervention and both-faction reconciliation are checkpointed at `1e6a0924e7`.
- Focused owner tests cover roster identity, transactional route/handoff, cohesion,
  unload/writeback, physical return/report, no-progress loss, and report/decision matching. They do
  not close the live lifecycle outcome.
- `[local_handoff]` passes 950 assertions in two cases with fixed seed `123456` on the current
  homeward-path repair. It
  proves a pair with
  existing local paths can leave a bubble several overmap tiles from camp, retain/rebuild physical
  routes, traverse intermediate loaded/unloaded ownership, and dematerialize together at camp. It
  now also proves that the first ordinary NPC turn after real materialization reduces every
  nontrivial member-to-stage distance, and that a schema-10 homeward pair routes around its
  forbidden watch OMT on its first physical motor turn.
  `WaitStepLedgerContractTest` passes all 38 tests on `c74427ca37`; a completed wait with retained
  wilderness flavor now sends no spurious input, while unknown confirmations remain blocking. The
  full harness fixture contract passes 155 tests.
- Run `20260807_135056` used `307e43efda+SDL3`, executable SHA-256
  `fd7f9d5ee742957f66e63173dee8125b5fc99038f744d310d9525531a2182fae`. It proved the forward
  handoff/ingress/dematerialization, normal watch report, and exact `returning_home` handoff at
  `(164,34,0)`. The full six-hour post-observation window then produced no homeward boundary,
  dematerialization, or returned-member event. The later `SPACE`/unknown-command abort was
  post-window harness behavior, not a gameplay interruption.
- Run `20260807_140347` on `c74427ca37+SDL3` was inconclusive: the observing handoff committed but
  the exact pair did not assemble within its existing five-minute production guard. It did not
  exercise the active return seam and earned no return credit.
- Run `20260807_142412` on clean `c7be851d23+SDL3`, executable SHA-256
  `47eacd1f94eb939c573d7852f3bb10faabf1a3219cfd75ed9ae820dd0028bcbd`, committed the observing
  handoff and found two valid staging paths totaling twenty steps with zero failed routes. The next
  cohesion write aborted with no movement order. This is the first live assembly-state blocker.
- Attempt `20260807_144214` performed no feature steps: the harness correctly rejected the dirty
  runtime source at startup. It earns no gameplay evidence.
- Run `20260807_144329` on clean `c846f9d929+SDL3`, executable SHA-256
  `05c198627c625c9a366f173d34bf015bc6a039ad86c939afc91026a312e3456e`, proved exact staging
  assembly, forward boundary/dematerialization, watch completion, and the paired `returning_home`
  handoff. It then produced no homeward boundary or camp dematerialization. The focused schema-10
  case reproduced the stall: both default local paths crossed the forbidden watch OMT and were
  rejected after selection. Supplying that safety rule to A* as an avoid predicate closes the
  focused motor claim; the unchanged live incident remains pending.
- Run `20260807_151252` used clean `caf1844007+SDL3`, executable SHA-256
  `d1c9242a00d0b531f94ce2f3ce0b3ad753cc47a69996ad2ff9ed41935ca1fb2e`. Startup and the feature
  debug guard were green. At minute 8400 the exact pair received 21 valid staging path steps. At
  minute 8410 member 4 was `(3936,829,0)->(3936,828,0)` and member 5 was
  `(3935,828,0)->(3936,829,0)`: both were adjacent, `movement_orders=0`, `assembled=no`, and
  `abort=yes`. The terminal verdict was `blocked_scout_to_decision_pair_handoff_missing`; no live
  credit reaches the repaired homeward seam.
- Checkpoint `1844bc8324a3` closes that exact staging mismatch. Run `20260807_152913` proves exact
  two-member assembly, forward travel, watch completion, and a later valid `returning_home`
  handoff at route position `(164,34,0)` with owner epoch 3. Earlier materialization attempts were
  correctly rejected because the loaded bubble lacked paired entry/staging positions. The valid
  handoff still produced no later homeward boundary or camp dematerialization through the observed
  window, so return/report/decision credit remains blocked at the next physical-owner seam.
- Checkpoint `8f642ddd7a` passes 1,102 `[local_handoff]` assertions with seed `123456`. It proves
  safe A* selection, byte-inert partial camp arrival, complete loaded pair arrival, and exact camp
  dematerialization. It does not prove that the live abstract pair can enter local ownership.
- Checkpoint `30b27b9d5f` removes the destructive post-window rearm. Checkpoint `a629eb804d` shares
  the ecology watch session across console reopen; `[debug_console]` passes 1,122 assertions in 11
  cases. The original terminal identity can now publish after its active row disappears.
- Run `20260809_030013` used clean `a629eb804d+SDL3`, executable SHA-256
  `31f170ce99d99489ce947652c533c7048c607dbd1be2fecb35b6bdbce90bac4f`. Fresh artifacts are
  `ecology_incident_5285093.json` (SHA-256 `238382595f0526283653ac1ef90b97b974797780051c5fc326dc5f5ba5df9d28`)
  and `.png` (SHA-256 `388498156e3652ffeecbd19dec54c1dcc373310a7c2c3d219694ea842f5b91f6`).
  They bind scenario `bandit.scout_to_decision_observer_live_mcw`, run `20260809_030013`, canonical
  generation-1 dispatch `BD-DF9E73`, natural provenance, and an empty intervention ledger.
- That run passed publication and then correctly aborted at
  `blocked_scout_to_decision_physical_return_not_reached_in_initial_window`. It observed abstract
  `members_returned=2` and `returned home`, but lacked both required physical facts:
  `phase=returning_home` local handoff and `(164,39,0)` local dematerialization. The first production
  blocker is repeated in-bubble rejection at route `(163,33,0)` because paired entry/staging tiles
  were unavailable.
- Checkpoint `9c6b73adff` passes the focused advanced-resume physical-return/camp-dematerialization
  section with 88 assertions, the owning case with 1,044 assertions, and the motor-boundary
  negative with 159 assertions at seed `123456`. Its exact relinked macOS SDL3 executable SHA-256 is
  `5b9a9886687926778ae38a989c86dd55d6d7069d95f916150301b4726e5e28a9`.
- Unchanged run `20260809_033014` on that executable proves the exact generation-1 pair's
  `phase=returning_home` local handoff at `(164,34,0)`. It published no incident because the retained
  watch failed closed with `entity_token_mismatch` when owner changed from abstract to local. The
  run therefore earns no camp-dematerialization, return, report, or decision credit.

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
persisted deadline. Handoff waits follow the scheduler's hourly cadence. The unchanged scenario's
post-observation wait spans the production assessment's existing two-hour no-progress boundary and
later return cadences; its duration is probe execution space, not a gameplay deadline or acceptance
substitute. The controller proves the actual relative clock advance and the pass still requires the
authoritative outcome below.

Pass: a surviving physical return, final non-provisional report, and matching authoritative camp
decision appear in the same structured incident. Startup, schema validity, dispatch, handoff,
assembly, or visible wait completion alone is non-credit.

Failure: preserve the run and name the first reproducible production or visibility seam that blocks
the pass condition. The next work claim must be necessary to close that exact gap.

Identities: record source commit, executable identity, fixture manifest/hash, scenario name, run
ID, authoritative owner audit, compact incident JSON, and paired screenshot where UI state matters.

Necessary validation before and through the next live probe:

```sh
python3 -m unittest tools.openclaw_harness.test_fixture_contract
python3 tools/openclaw_harness/startup_harness.py probe --compact-stdout \
  bandit.scout_to_decision_observer_live_mcw
```

First preserve the retained operation identity across expected abstract/local ownership transfers
without weakening world, canonical-ID, generation, authority, or intervention guards. Then relink
the Mac executable from exact committed source and run the same causal scenario above. The live
pass requires the exact pair's `returning_home` local handoff and camp dematerialization before the
surviving return, final non-provisional report, and matching authoritative camp decision.

Cross-platform performance/save/runtime qualification begins after the natural vertical incident
is green; until then only the Mac route exercised here is claimed.
