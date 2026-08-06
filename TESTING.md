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
  x=164 route corridor: camp `(164,39,0)`, sector-0 inner waypoint/player `(164,35,0)`, and outer
  target `(164,30,0)`. It still injects no lead, dispatch, observation, return, report, or decision.

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
  at its sector-0 inner waypoint `(164,35,0)`, retain outer target `(164,30,0)`, retire the empty
  original roster, clear inherited evidence/outings, and add
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
- Timeout: 14 game hours to the code-derived frontier deadline, five minutes for real local
  handoff, then an initial bounded six-hour post-observation window. The road-connected correction
  is one new causal footing; stop and isolate the first concrete blocker if it fails.
- Pass/fail identities: record `9b435e1ee3` binary identity plus the committed source/fixture
  manifest/hash, scenario name, run ID, saved-owner audit, same-run transition patterns, and compact
  incident JSON/PNG. Passing requires a surviving physical return, final report, and decision;
  startup, handoff, or schema validity alone is non-credit.
