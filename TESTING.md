# Testing

Current validation policy, latest relevant evidence, and pending proof for hostile-camp ecology.
Historical test receipts remain in Git history, `doc/work-ledger.md`, external manifests, and
feature-specific proof documents.

## Policy

- Test the smallest honest route a player or harness will use. A helper-only pass does not prove a
  live owner transition.
- Before a behavior run, record a proof contract: claim; preconditions/interventions; causal
  boundary; production code path; expected transition; negative/control; deadline; pass/fail rule;
  and commit/binary/scenario/fixture/tool identities.
- Fixture transforms may establish deterministic seed/time/weather, terrain, actors, a known lead,
  and observer mutation. They stop before the claimed transition and record every intervention.
- Production scheduler, perception, pathfinding, movement, handoff, casualty, report, decision,
  persistence, and outcome code must cause the claimed behavior.
- Prefer authoritative structured snapshots and transition deltas over OCR. A screenshot pairs
  human-visible state with the same incident, but does not replace machine-readable owner proof.
- A live-probe obstacle gets at most two meaningfully different attempts. Then isolate the actual
  gameplay defect or record the harness limitation; do not grow brittle OCR/menu heuristics.
- Redirect builds/tests/probes to named artifacts. Report exit code, first hard error, final test
  summary, verdict, and missing required artifacts rather than full logs.
- Reuse a green build only when source identity and affected objects match. Run narrow tests first;
  broaden only for integration risk. Claim only platforms actually built or run.
- `DEBUG_CLAIRVOYANCE` observer state is display/debug-only. Gate-off must do zero observer work;
  gate-on must not mutate knowledge, AI, reports, routes, or normalized authoritative save bytes.
- Debug edits require confirmation, authoritative mutation APIs, stale-token rejection, and explicit
  before/after/intervention provenance. Never credit an intervention as natural behavior.

## Latest relevant evidence

### Overdue total loss — green

Checkpoint `a46897c637` resolves an all-missing exact pair at the persisted deadline, emits one
bounded `missing-route:<camp OMT>-><target OMT>` mark only after durable missing/casualty ownership,
and imports no observation, dossier, cargo, or report. Exact-boundary, late-jump, pre-advanced
cursor, save/reload, replay, public-packet bypass, cap, and confirmed-dead controls are covered.

- Mac non-tiles test build/link: exit 0; existing deployment-target library warnings only.
- `[overdue_total_loss]`: 1 case / 73 assertions, seed `860806`.
- `[physical_report],[split_return],[local_handoff],[camp_map]`: 24 cases / 1,400 assertions.
- Final `autoreview --mode local`: clean after accepted deadline-anchor and packet-ownership fixes.
- `git diff --check`: clean before commit.
- Not claimed: astyle 3.1, GUI, Linux, or Windows runtime.

### Observer/editor v0 — usable

- Read-only camp/dispatch view, overmap UI, deterministic compact export, selected details,
  transition deltas, six watches, and incident capture are checkpointed.
- Field run `20260805_101713` selected local dispatch `BD-374153`, armed a watch, stepped one turn,
  killed NPC 4 through `npc::die`, retained NPC 5, and captured the same-turn overlay plus incident
  JSON/PNG with `debug_intervention` provenance. Receipt: `ecology_field_gate_receipt.json`.
- Performance/save neutrality at `117857f551`: disabled queries do zero measured work; enabled
  100-camp queries stayed bounded on this Mac; normalized authoritative ecology bytes remained
  stable through the existing save/menu-load harness.
- Both-faction casualty reconciliation at `1e6a0924e7` covers one-dead/one-survivor,
  both-confirmed-dead, and wounded-pair returns through save/reload.
- Target-relocation run `20260805_124207` keeps dispatch `BD-374153`, generation, pair, route, and
  camp target fixed while only the player moves; both incidents have an empty intervention ledger.
- Mobile horde/stalker production adapters remain disabled because stable movement/load-transfer
  identity is unavailable.

### Scout-to-decision vertical — tooling green, live proof pending

- The new fixture `bandit_scout_to_decision_observer_east_v0_2026-08-07` derives from the exhausted
  west fixture with exactly one declared precondition transform: player offset `[96,0,0]`, moving
  the observer from OMT `(162,35,0)` to `(166,35,0)`. Camp, route, clock, zero-lead/zero-outing
  boundary, and `DEBUG_CLAIRVOYANCE` are inherited; no lifecycle outcome is written. Both JSON
  payloads parse, all 149 harness contracts pass, and `git diff --check` is clean. No live credit.
- East runs `20260807_052130` on clean `a85691bd0c+SDL3` and `20260807_053354` on clean
  `949a6dbab1+SDL3` naturally reached terrain discovery, frontier dispatch, and
  `local_handoff committed ... members=2`. Both then repeated `assembled=no`,
  `failed_routes=0`; the first aborted at ten minutes and the second at twenty. Both verdicts were
  `blocked_scout_to_decision_pair_handoff_missing`. The second run disproves rendezvous duration as
  the cause, so the twenty-minute change is reverted. The owner read sees the NPCs through the
  overmap buffer, while the movement adapter can act only on members in the current map; no route
  was attempted before timeout. Both east attempts are exhausted. No live success credit and no
  permission/OCR blocker are assigned.
- The repair filters both entry and staging candidates through the exact Chebyshev submap radius
  used by `game::load_npcs` before a local transaction can be planned. The real-map/NPC regression
  covers the non-activating loaded-map fringe (no commit, both NPCs inactive, serialized abstract
  world byte-identical) and the adjacent geometry (complete epoch-1 commit and both NPCs active
  through the production loader). Canonical Mac SDL3/SOUND test build/link: exit 0; `[local_handoff]`:
  2 cases / 783 assertions, seed `860807`. Existing newer-deployment-target dylib warnings only.
- Run `20260807_043746` on clean `5010d98e3d+SDL3` naturally reached exact dispatch and
  `local_handoff committed ... members=2`. During the ordinary wait it opened the real bandit
  Pay/Fight dialogue for Giuseppe Bachman before physical return/report/decision; the paired
  screenshot is `wait_5_minutes_through_real_pair_handoff_cadence.before.png`. This is a gameplay
  ownership failure, not proof of the vertical or a permission/OCR failure. The footing is
  exhausted and must not be rerun.
- The repair requires explicit `toll` intent and, for the new owner, operation kind `shakedown`.
  A favorable two-person structural scout contact remains `probe`; production-shaped hostile
  operation tests prove the positive surface and aftermath paths through `active_external_outing()`.
  All live gate-input, trader/speaker/fight, parley, and defender-aftermath consumers now read the
  same authoritative external outing. Mac `bandit_live_world.o`/`do_turn.o` plus test rebuild:
  exit 0. `[approach_gate],[shakedown],[hostile_operation]`: 11 cases / 1,429 assertions, seed
  `860807`. The first medium review found the legacy-only live consumers; the required re-review
  found the remaining gate-input and defender-aftermath bypasses. Both findings were accepted and
  fixed; deterministic closeout replaced another reviewer chain.
  Live success credit remains pending on a materially different footing.
- Run `20260807_041806` on clean `38ab88e8cf+SDL3` naturally reached the exact pair's dispatch,
  handoff, repeated `assembled=yes` cohesion, observer selection, and the final six-hour watch. At
  scheduler hour 144 the outing was still active. A safe Shadow EOC popup was acknowledged and its
  structured open/return pair was preserved; production then opened the separate activity
  distraction query, whose partial OCR correctly failed closed. Verdict:
  `red_wait_mid_poll_interruption`; no return/report/decision credit and no debug intervention.
- The repair traces the authoritative `cancel_activity_or_ignore_query` open/return only under the
  harness gate, including distraction type and returned action. It reuses the existing safe
  `I`/`IGNORE` response only for a complete known-type query when OCR is compatible with that UI;
  unrelated unknown, malformed, truncated, and stale inputs produce no action. Mac evidence:
  `obj/tiles/game.o` compile exit 0; 149 harness contracts green; medium autoreview clean at
  `patch is correct (0.88)`; `git diff --check` clean. Runtime proof requires the next committed
  SDL3 build to record `action=IGNORE` on the real query.
- The replacement wait footing traces the real EOC popup open/return boundary only when the
  harness-set `OPENCLAW_HARNESS_UI_TRACE` gate is active. It reads at most 256 KiB of the existing
  debug log and reuses the existing interruption classifier: only an already-known safe prompt can
  be acknowledged; unknown, portal/contaminating, release-blocking, truncated, and repeated active
  events fail closed. Normal game starts emit no trace. Mac evidence for this checkpoint:
  `obj/tiles/npctalk.o` compile exit 0; 145 harness contracts green; medium autoreview clean at
  `patch is correct (0.91)`; `git diff --check` clean. Live behavior credit remains pending a clean
  committed SDL3 build and new scenario run.

- The canonical Mac SDL3 binary rebuilt successfully as clean `9b435e1ee3+SDL3`; existing
  deployment-target dylib warnings remain non-fatal.
- The saved-state audit now exposes the authoritative final scout report, camp decision, and their
  exact revision/generation/activity/application/target identity match. Harness contract suite:
  139 tests green.
- Checkpoint `9b435e1ee3` adds bounded route-rejection reasons without changing eligibility or
  state. Mac `[routed_dispatch]`: 5 cases / 157 assertions; `git diff --check` green. Medium
  autoreview found and the implementation corrected misleading summary reuse and rejection-order
  taxonomy; deterministic tests closed the localized final wording fix.
- Runtime identity remains strict: any dirty build title is untrusted because current worktree state
  cannot prove which paths were dirty at build time. A clean committed rebuild is required.
- Run `20260806_232503` naturally discovered the road at the original open camp but reached
  `scheduler_hour=137` with no routed score-eligible candidate after the new concealed-watch route
  contract. This supersedes the pre-watch historical dispatch expectation.
- Run `20260806_234324` used a declared forest-camp precondition, naturally accumulated 12 leads,
  and remained healthy but idle at drive `473` through `scheduler_hour=137`. The stable camp ID
  yields its first frontier deadline at hour `139`; the inherited 12-hour timeout was two hours
  short. Permissions, startup, wait elapsed time, and feature error guard were green.
- Runs `20260807_001847` and `20260807_004900` then reached hour `139` on the unchanged forest
  footing. The clean diagnostic run proved `frontier_probe:0` exceeded the complete-route cap and
  the remembered road lead's watch route was malformed. The old `(160,39,0)` footing has exhausted
  its attempts and must not be rerun.
- The replacement declared precondition moves only the camp/player footing onto the existing
  x=164 route corridor: camp `(164,39,0)`, sector-0 inner waypoint `(164,35,0)`, and outer target
  `(164,30,0)`. The player observer now stands two OMTs west at `(162,35,0)`. It still injects no
  lead, dispatch, observation, return, report, or decision.
- Run `20260807_005959` reached the replacement camp's code-derived deadline at hour `138` but
  rejected the exact road route because the NPC pathfinder's raw cost included half the source
  structure tile. The structural cap is defined from the camp boundary, so this slice subtracts
  only that pathfinder-defined departure fraction before normalizing both target and watch routes;
  the pathfinder's zero-cost impassable-source escape remains intact. `[routed_dispatch]` is green
  at 5 cases / 163 assertions and the harness contract remains green at 139 tests. Medium review
  found both adapter inconsistencies; the final localized fallback fix closed with deterministic
  tests. A clean committed SDL3 rebuild and live rerun are still required.
- Run `20260807_013140` on clean `1b7b40da3e+SDL3` naturally dispatched the exact pair for
  `frontier_probe:0`; the previous route rejection is resolved. It then timed out honestly because
  the scenario allowed only five minutes for a route whose production stalking delay is 135
  minutes and whose owner advances hourly. The repaired contract waits three ordinary game hours
  for the first route waypoint; it does not change or inject the outing.
- Run `20260807_014256` on clean `c53568ccaf+SDL3` naturally dispatched and committed both exact
  members into local ownership at route position `(164,35,0)`. The player occupied that same OMT,
  so ordinary hostile combat began before cohesion assembled; the run stopped red with only
  `assembled=yes` missing. That footing is exhausted. Moving the observer two OMTs west removes
  the causal collision without changing the camp, route, actors, clock, or production transition.
- Run `20260807_015545` on committed off-route footing dispatched, handed off, assembled, and then
  entered ordinary exposure-driven `returning_home`. The pair remained active for the six-hour
  window without the physical-return receipt. A global return artifact incorrectly aborted before
  incident/save audit; checkpoint `4520134379` removes only that wait-completion coupling while the
  same-run log and saved-owner audits still require return.
- Run `20260807_021525` proved the nominal three-hour wait can return after scheduler hours 139 and
  140, before the hour-141 materialization turn; `materialization_attempts=0`, so this run is a
  harness-duration limitation rather than a handoff rejection. The repaired contract follows the
  three-hour wait with a bounded five-minute overmap-NPC cadence wait, then audits the whole run for
  the one-shot handoff/cohesion receipt. The harness contract remains green at 139 tests; live proof
  on this repaired footing is pending.
- Run `20260807_023247` showed that a wait without step-local patterns falls back to one-shot global
  proof patterns and aborts before the cadence step. Checkpoint `6bf8b190e0` gives both boundary
  waits a repeatable post-choice performance heartbeat while the whole-run audit exclusively owns
  handoff/cohesion proof; the 139-test contract remains green.
- Run `20260807_024225` stopped during the first six-hour wait on upstream
  `overmap::place_highway_line` debug popup `highway slant pathing out of bounds; falling back to
  onramp`. The ecology bootstrap already calls `overmap_special_at_existing` and does not generate
  missing overmaps; no ecology transition was reached and no behavior credit or defect is assigned.
  The repaired live footing exhausted its attempts until a return-path code/test change existed.
- The loaded return repair now has a causal real-NPC regression: both actors begin with nominal camp
  goals but target-reentering OMT paths, and the authoritative aftermath tick repairs both paths
  before the local-gate skip. The focused Mac re-review run is green at 1 case / 28 assertions;
  `[npc][bandit]` is green at 11 cases / 1,000 assertions after one unrelated
  environmental-visibility test failed in the combined run and passed immediately in isolation.
  Mac tiles test build/link and `git diff --check` are green. Medium autoreview found one P1 in the
  initial broad guard removal; the narrowed fix preserves deferred-phase target skipping, and the
  clean re-review reports `patch is correct (0.94)`. A clean committed SDL3 rebuild and one live
  observer rerun are next; Linux/Windows runtime are not claimed.
- Run `20260807_030535` on clean `5090474d85+SDL3` naturally dispatched, handed off, assembled,
  exposed, and reached the repaired `returning_home` phase. It stopped at 79% of the three-hour
  wait on the non-contaminating base-game `Shadow_Warnings_Snippets_early` lifeless-grass story
  popup. The existing fail-closed classifier already accepts the full snippet, but the observed
  column-split OCR preserved its two distinctive phrases across newlines. Normalizing whitespace
  for only those existing lifeless-grass markers recovers this exact safe popup while partial text,
  safe-mode prompts, confirmations, and contaminating portal warnings remain fail-closed. Harness
  contract: 140 tests green.
- Final run `20260807_031409` naturally dispatched on the same gameplay binary, completed both
  frontier waits and the three-hour boundary wait, then stopped at 55% of the five-minute cadence
  wait on another non-contaminating Shadow story popup: `You have a vague feeling of being
  watched.` The visible popup was absent from the structured classifier input, so the bounded
  `bandit_live_world perf:` heartbeat timed out. No handoff/return credit is assigned. This footing
  has consumed its two attempts; do not add another OCR/screenshot heuristic or rerun it. The next
  proof is a bounded real-NPC in-process owner integration through travel, camp arrival,
  dematerialization, return/report, and decision; later Josef playtesting may reuse the retained
  artifacts without reopening the coordinator cap.
- The replacement in-process owner proof is green on Mac at 1 case / 50 assertions. Its fixture is
  created by the production structural outing planner, then declares the real inactive pair's
  local `returning_home` state and carried assessment immediately before the causal boundary.
  After that boundary only `overmap_npc_move()` advances time: both NPCs physically traverse the
  two-OMT field corridor, the complete pair dematerializes transactionally, the abstract scheduler
  applies the roster return, the carried assessment becomes the final report, and the camp accepts
  that report into `report_awaiting_assessment`. No position, return generation, report, or decision
  is written after the boundary. This is isolated owner-chain proof, not live-row credit. Final
  build/test logs: `physical_return-reref-build.log`,
  `physical_return-reref-focused.log`, and `physical_return-reref-adjacent.log`. The adjacent
  `[npc][bandit]` slice is green at 12
  cases / 1,046 assertions. Medium autoreview found one P2 test-lifetime defect: the scheduler may
  append camps and invalidate a retained vector-element reference. The fixed test reacquires its
  site by stable ID around every production tick; the one clean re-review reports `patch is correct
  (0.92)` with its focused test green.

### Capped non-credit probes

- Smoke/light/sound runs `20260805_121516` and `20260805_122335` stopped on overmap-key and
  interruption-handler defects before credited observer proof.
- Decoy runs `20260805_125925` and `20260805_130217` exposed legacy/current scheduler-fixture drift;
  the replacement fixture is contract-green but has no live credit.
- Visible-burn runs `20260806_100318` and `20260806_100839` stopped before gameplay on legacy
  transform fields; schema repairs are checkpointed.

These rows exhausted the two-attempt coordinator cap. Do not rerun them now; retain the corrected
fixtures for Josef's disposable playtest package.

## Pending proof

One cohesive observer-backed run must show:

1. a real natural camp and exact scout pair selected from authoritative state;
2. ordinary discovery/travel/watch behavior without pre-writing the result;
3. at least one real survivor physically returning through the normal movement/handoff owner;
4. eligible evidence carried by that survivor into the final report;
5. the report consumed into the camp decision; and
6. a compact before/after incident pair with natural/debug provenance and owner identity.

Primary negative controls: quiet/no-evidence target, private fact lost with its carrier, all-dead or
deadline-missing pair, stale selection, save/reload during ownership transfer, and no progress by
the declared deadline. Natural total loss is valid control evidence but cannot satisfy the primary
survivor-report artifact. The first observed blocker selects the next code slice.

## Current commands

Use the narrow test binary when affected objects match:

```sh
./tests/cata_test '[overdue_total_loss]' --rng-seed 860806
./tests/cata_test '[physical_report],[split_return],[local_handoff],[camp_map]' --rng-seed 860806
python3 -m unittest tools.openclaw_harness.test_fixture_contract
```

Build the Mac non-tiles test target when core ownership code changes:

```sh
make -j8 SOUND=1 LOCALIZE=1 LANGUAGES=all LINTJSON=0 ASTYLE=0 TESTS=1 tests
```

Use `tools/openclaw_harness/startup_harness.py` for scenario list/probe/handoff operations. Preserve
the run directory, exact command, source/binary identity, observer snapshot, incident JSON, and
screenshot receipt. Cross-platform qualification follows only when the vertical behavior is green.

## Active bandit scout-to-decision proof contract

- Claim: one real bandit camp naturally builds terrain memory, reaches its ID-derived sector-0
  frontier deadline, dispatches its exact pair,
  travels and observes, completes quietly, physically returns at least one survivor, creates a
  final non-provisional report, and enters the authoritative camp decision owner in one run.
- Preconditions/interventions: derive from the existing McWilliams save; set deterministic time,
  place one five-member camp at `(164,39,0)` on the existing x=164 route corridor, park the player
  two OMTs west of its sector-0 inner waypoint at `(162,35,0)`, retain outer target `(164,30,0)`,
  retire the empty original roster, clear inherited evidence/outings, and add
  `DEBUG_CLAIRVOYANCE`. Each transform records a receipt. Do not inject a lead, dispatch,
  observation, casualty, return, report, or decision.
- Causal boundary: the saved idle five-member camp with zero leads and no active outing before the
  first wait. Production owns every later transition.
- Real path: hourly structural scheduler -> terrain discovery -> exact-pair route/handoff/cohesion
  -> ordinary observation and return movement -> canonical return packet/report -> decision
  acceptance.
- Expected transition: the same stable pair reaches observing, at least one real survivor returns
  home, the dispatch completes without a debug ecology intervention, its eligible evidence enters
  one final report, and that exact report enters assessment/decision.
- Negative/control: preflight proves zero leads/zero outing; an empty or all-loss return cannot pass;
  private evidence without a carrier cannot appear; stale identity, duplicate owner, or mismatched
  report generation is red.
- Timeout: 13 game hours to the code-derived frontier deadline, three hourly structural advances
  for real local handoff, then an initial bounded six-hour post-observation window. The road-connected correction
  is one new causal footing; stop and isolate the first concrete blocker if it fails.
- Pass/fail identities: record the clean committed binary identity plus the committed source/fixture
  manifest/hash, scenario name, run ID, saved-owner audit, same-run transition patterns, and compact
  incident JSON/PNG. Passing requires a surviving physical return, final report, and decision;
  startup, handoff, or schema validity alone is non-credit.
