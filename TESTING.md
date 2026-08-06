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
```

Build the Mac non-tiles test target when core ownership code changes:

```sh
make -j8 SOUND=1 LOCALIZE=1 LANGUAGES=all LINTJSON=0 ASTYLE=0 TESTS=1 tests
```

Use `tools/openclaw_harness/startup_harness.py` for scenario list/probe/handoff operations. Preserve
the run directory, exact command, source/binary identity, observer snapshot, incident JSON, and
screenshot receipt. Cross-platform qualification follows only when the vertical behavior is green.
