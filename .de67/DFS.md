# C-AOL Proof-Preserving Playtesting DE-67 Functional Specification

Status: Frozen
WEC: `.de67/WEC.md`
Source baseline: `Cataclysm-AOL-hostile-ecology-dev | dev tracking origin/dev | 3f8d5e0a1b6b21ed7e23f43d27a9e40aafa1fe88 | intentionally broad dirty product and harness frontier | inspected 2026-08-22`
Inspected-source fingerprint: `e26e145206657d5cb43a3482617be8a09400d37a8bb062f96b74b5cc59016217` (SHA-256 over the ordered per-file SHA-256 records named in the current code map)

## Document authority

This document is the mechanistic product contract derived from the user-owned WEC and inspected
production code. It is not a task-dispatch plan. If this document conflicts with current code about
what code does, re-inspect the code; if it conflicts with the WEC about what the product should do,
the WEC and the user win.

The source baseline includes modified `src/bandit_live_world.*`,
`src/bandit_live_world_probe.*`, `src/do_turn.*`, `src/overmap_ui.cpp`,
`src/player_activity.cpp`, the named tests, and the named harness and registry files. It also
includes the untracked `tools/openclaw_harness/transition_event_reader_unit_test.py`. The dirty
frontier is evidence under inspection, not Phase-2 implementation credit, and was preserved.

Status markers:

- `[x]` — present in the production path with proportionate evidence.
- `[ ] 🔴 R-...` — missing, wrong, or unproved; the stable red item is implementation work.

## Functional contract

C-AOL development runs preserve compatible focused proof and recommend a compatible known state for
diagnosis without awarding either one final-certification credit. A final automated pass starts a
new, completely bound round and proves every required lifecycle gate in that one round. Josef then
performs a separate ordinary Windows play pass.

```text
focused run -> immutable classed evidence -> compatible proof remains visible
failed run -> first divergence -> compatible diagnostic capsule recommendation -> smallest next probe
certification start -> complete binding sealed -> one uninterrupted lifecycle -> automated gate result
automated pass -> ordinary Windows handoff -> Josef's separate Windows feel gate
```

A permitted save, quit, and relaunch is an event inside the same continuous certification round. It
does not create a replacement round. A binding change, replacement world or identity, diagnostic
replay, checkpoint rollback, or segment splice cannot contribute final-certification credit.

## Project language and terminology

Use the WEC terms exactly:

- **Diagnostic capsule** — a bound preserved state used to investigate from a known point.
- **Diagnostic replay** — a run from a diagnostic capsule that earns no final-certification credit.
- **Continuous certification round** — the single bound execution used by the automated
  certification gate.
- **Automated certification gate** — the machine-verifiable final integrated gate.
- **Windows feel gate** — Josef's separate ordinary-play judgment gate.
- **First divergence** — the earliest failed causal expectation.
- **Binding** — the complete identity of the code, runtime, data, harness, scenario, world, and
  actors relevant to evidence compatibility.
- **Authoritative simulation owner** — the one layer currently allowed to advance an
  identity-bearing actor.

Do not call a diagnostic replay a resumed certification. Do not call assembled segments a
continuous round. A build, startup image, helper result, synthetic proof, or focused test keeps only
its own evidence class. “Checkpoint contract” in current scenario manifests is setup preflight
language and is not a diagnostic capsule or certification boundary.

## Current code map

| Concern | Files and symbols | Current production behavior | Evidence |
|---|---|---|---|
| Durable ecology truth | `src/bandit_live_world.h` :: `world_state`, `site_record`, `active_outing_state`, `simulation_advance_cursor`, `local_handoff_snapshot` | The global overmap state serializes stable actor IDs, activity ID, generation, owner, handoff epoch, progress cursor, and local snapshots. | `src/bandit_live_world.cpp` :: serialization/deserialization; `tests/bandit_live_world_test.cpp` save/load and malformed-state sections. |
| Single-owner transfer | `src/bandit_live_world.cpp` :: `preflight_local_pair_handoff`, `plan_local_pair_handoff`, `commit_local_pair_handoff`, `plan_local_pair_dematerialization`, `commit_local_pair_dematerialization`, `advance_external_simulation` | A cursor-bound candidate is assembled before the complete pair is bound or quiesced. Callback failure rolls the pair back; the durable site is replaced only after all callbacks succeed. | `tests/bandit_live_world_test.cpp`; `tests/bandit_live_world_natural_test.cpp`; production calls from `src/do_turn.cpp`. |
| Load validation | `src/bandit_live_world.cpp` :: `active_outing_state::deserialize`, `site_record::deserialize`, `world_state::deserialize` | Malformed owner/snapshot state is rejected and a stable member ID claimed by two sites is rejected. Loaded local NPC projections are not reconciled against the durable owner record here. | Deserializer guards and focused round-trip/rejection tests. |
| Transition evidence | `src/bandit_live_world_probe.cpp` :: `record_live_transition_event`, `append_live_transition_event`; `src/bandit_live_world.cpp` :: `record_live_transition` | A run-ID-bound append-only JSONL stream validates schema and sequence. Events carry activity/generation/epoch/actor fields, but the JSONL omits the declared `simulation_owner` field and dematerialization has no matching transition event. | `tools/openclaw_harness/transition_event_reader_unit_test.py`; event writer and handoff call sites. |
| Harness binding | `tools/openclaw_harness/startup_harness.py` :: `runtime_source_binding`, `build_runtime_binding`, `build_plan` | Executable and selected runtime-source bytes are hashed; scenario declarations and fixture/profile owners have separate binding adapters. Harness bytes, complete data/config, world/save, player, and ecology actor identities are not sealed as one certification binding. | `tools/openclaw_harness/scenario_registry_cli.py` :: `production_binding_adapters`; registry unit tests. |
| Structured lifecycle proof | `tools/openclaw_harness/startup_harness.py` :: `StructuredTransitionEventReader`, `evaluate_structured_proof_gates`, `normalize_saved_artifact_receipt`, `run_probe_post_relaunch` | Gates can correlate event identity with a saved artifact and relaunch the same configured world under one run ID. The public route is still probe/handoff and can classify only startup/load or feature-path evidence. | `tools/openclaw_harness/test_probe_relaunch.py`, proof classification and transition reader tests. |
| Evidence registry | `tools/openclaw_harness/scenario_registry.py`, `scenario_registry_store.py`, `scenario_registry_cli.py` | Scenario declarations are explicitly not evidence. Immutable report references, manifest history, binding history, lifecycle, compatibility/reconciliation, quarantine, and capability evidence are stored. | Module contract, SQLite schema, ingestion and unit tests. |
| Process ownership | `tools/openclaw_harness/startup_harness.py` :: `kill_existing_game_processes`, `run_startup`, `process.json` | Startup kills all matching game processes before launch; later cleanup is PID/command checked. There is no exclusive certification-round lease, so concurrent unrelated runs can be destroyed or confused with the bound round. | Production startup path and process receipt. |
| Diagnostic reporting | Probe report construction in `tools/openclaw_harness/startup_harness.py` | Reports preserve step ledgers, gate evidence, raw structured events, and summaries of repeated non-committed events. There is no first-divergence record or selected diagnostic capsule. | Final report assembly around `structured_transition_events` and `proof.gates.json`. |

The inspected-source fingerprint covers, in order: `src/bandit_live_world.h`,
`src/bandit_live_world.cpp`, `src/bandit_live_world_probe.h`,
`src/bandit_live_world_probe.cpp`, `src/do_turn.h`, `src/do_turn.cpp`,
`src/overmap_ui.cpp`, `src/player_activity.cpp`, `tests/bandit_live_world_test.cpp`,
`tests/bandit_live_world_natural_test.cpp`, `tools/openclaw_harness/startup_harness.py`,
`scenario_registry.py`, `scenario_registry_store.py`, `scenario_registry_cli.py`,
`scenario_registry_unit_test.py`, `transition_event_reader_unit_test.py`,
`test_probe_relaunch.py`, and `proof_classification_unit_test.py` under
`tools/openclaw_harness/`.

## External research sweep

These primary-source comparisons clarify implementation patterns; they do not override C-AOL.

| Source | What was searched | Claim to validate | Effect on DFS |
|---|---|---|---|
| [Veloren `server/src/cmd.rs` at `7d859ff4`](https://gitlab.com/veloren/veloren/-/blob/7d859ff41190f3c9b4e9ed9e9d11c9d82b1faffd/server/src/cmd.rs#L4918) | `EntityTarget::RtsimNpc`, durable RT-sim actor IDs, loaded ECS lookup | A durable simulation identity can exist independently of its currently loaded entity. | Supports durable C-AOL actor truth plus a temporary loaded projection; it does not prove C-AOL transfer correctness. |
| [Luanti `serverenvironment.cpp` at `ecda1027`](https://github.com/luanti-org/luanti/blob/ecda1027f3c501eb9c2274a1815e1997831300af/src/serverenvironment.cpp#L1566-L1763) and [world format](https://github.com/luanti-org/luanti/blob/ecda1027f3c501eb9c2274a1815e1997831300af/doc/world_format.md#L499-L515) | Activation of stored objects, deactivation of active objects, persistent static object representation | Loaded objects can be projected from stored objects and written back when leaving the active region. | Supports explicit stored/active conversion. Luanti's delete-old-before-save path is not adopted; C-AOL's candidate-then-commit invariant remains stronger and required. |
| [OpenMW `worldimp.cpp` at `8a5315f`](https://github.com/OpenMW/openmw/blob/8a5315f46dddaa807d340febd2d09cef97da32d2/apps/openmw/mwworld/worldimp.cpp#L600-L624) | `World::searchPtr`, active cells, `WorldModel::getPtrByRefId` | Scene membership and durable world lookup are separate concerns. | Supports treating local presence as projection/lookup state, not a second authority. |
| [Cataclysm-DDA `mongroup.h` at `45216d8`](https://github.com/CleverRaven/Cataclysm-DDA/blob/45216d868c808e3fb4cca0903dd18a9e49bab225/src/mongroup.h#L116-L139) | Concrete `monsters` versus aggregate `population` in an overmap group | An aggregate population is not equivalent to evidence about named actors. | Negative comparison: aggregate-only simulation may prove population/resource claims but receives no credit for identity-bearing lifecycle gates. |

The comparison refines the WEC's likely invariant as follows: C-AOL's durable outing and member
records are authoritative; local NPCs are temporary projections under a cursor- and epoch-bound
lease. Existing transactional handoff logic is retained. Every transfer must additionally be
observable, persistence-confirmed, idempotent, and reconciled or rejected at load.

## Mechanistic requirements

### 1. Evidence classes and gate authority

Mechanism:

- Files and symbols: `tools/openclaw_harness/startup_harness.py` report assembly and proof
  classification; `scenario_registry.py` and `scenario_registry_store.py` report records.
- Entry point: every build helper, startup, probe, diagnostic replay, certification run, and
  Windows handoff that writes or ingests evidence.
- Inputs: route invoked, immutable run/report ID, complete binding ID, scenario ID, and whether the
  run was created under diagnostic or certification authority.
- Preconditions: evidence authority and class are fixed before the run starts and cannot be raised
  by a later report edit.
- Transition: record exactly one applicable class among setup support, build proof, synthetic proof,
  focused feature proof, diagnostic replay, automated continuous-round certification, and Windows
  feel evidence. A report may reference lower classes without converting them.
- Postconditions: already-proven compatible focused gates remain visible, while final gates accept
  only their own evidence.
- Failure behavior: missing, unknown, contradictory, or post-hoc-promoted class is ineligible for
  both final gates.
- Persistence/compatibility: class, authority, run ID, binding ID, source artifact hashes, and
  supersession reason are immutable registry facts.

Implementation status:

- [x] Scenario registry declarations state that they are not evidence, and the current report route
  distinguishes startup/load from feature-path proof.
- [x] R-001 — Evidence is not classified and authorized strongly enough to preserve focused proof without allowing it to masquerade as either final gate.
  - Code gap: `startup_harness.py` proof classification and the scenario-registry report schema expose only coarse startup/load versus feature-path classes and no immutable diagnostic/certification/Windows authority.
  - Required mechanism: extend the existing report and registry owners with the WEC evidence classes and start-time authority; make final-gate eligibility a derived, fail-closed decision rather than a caller-supplied label.
  - Proof: ingest one artifact from every class plus attempted post-hoc promotions; compatible focused proof remains queryable, while only one genuine certification artifact and one Josef-authored Windows feel result are eligible for their respective gates.

### 2. Complete binding and round isolation

Mechanism:

- Files and symbols: `startup_harness.py` :: `runtime_source_binding`, `build_runtime_binding`,
  `build_plan`, `run_startup`, `process.json`; scenario-registry binding tables and adapters.
- Entry point: certification-round creation before any setup action that can affect evidence.
- Parameters: immutable round ID, scenario lineage ID, binding ID, authority, and process lease.
- Inputs: exact code and dirty-worktree bytes, data/config bytes, executable, harness, fixture,
  scenario declaration, world/save lineage, player identity, and identity-bearing ecology actors.
- Preconditions: all components are resolvable and hashable; the world, player, actors, and process
  lease are not already owned by another live round.
- Transition: seal one binding manifest, acquire an exclusive round/process lease, and append every
  allowed lifecycle event to that round. Recompute the binding before each evidence-bearing segment
  and after relaunch.
- Postconditions: every event and artifact names the same round, lineage, and binding. Normal
  save/quit/relaunch preserves those identities.
- Failure behavior: any binding change, replacement identity/world, concurrent owner, missing hash,
  checkpoint rollback, or sequence discontinuity invalidates certification credit and records the
  first incompatible component. Focused history remains stored under its own binding.
- Persistence/compatibility: the sealed manifest and lease history are immutable registry records;
  stale processes are released only after PID, executable, round, and lease identity agree.

Implementation status:

- [x] Current code hashes an executable and selected runtime-source bytes and separately binds
  scenario/fixture/profile declarations.
- [x] R-002 — A certification round has no complete immutable binding or exclusive process/world lease.
  - Code gap: `runtime_source_binding` omits harness, full data/config, world/save, player, and actor identities; `kill_existing_game_processes` kills matching processes globally rather than enforcing a bound exclusive lease.
  - Required mechanism: extend the existing binding and registry owners to seal the complete WEC binding and acquire/release a round-specific process/world lease; fail closed on component drift or competing ownership.
  - Proof: a round survives an ordinary save/relaunch with the same binding and identities, while independent mutations of every binding component, a replacement world/player/actor, a sequence rollback, and a competing process each invalidate only certification credit and name the first mismatch.

### 3. Diagnostic capsule selection and first divergence

Mechanism:

- Files and symbols: scenario registry report/binding history; `startup_harness.py` step ledger,
  structured gate evaluation, saved artifact receipt normalization, and final report assembly.
- Entry point: completion of any failed focused, diagnostic, or certification run.
- Inputs: ordered causal gates, expected state, observed structured events and saved state, compatible
  capsule candidates, binding relationships, actor IDs, generation, owner, and timestamps.
- Preconditions: a capsule candidate is an immutable preserved state with a complete known binding;
  the report can establish an ordered last-green/first-red boundary.
- Transition: identify the earliest unmet causal expectation, suppress repeated transport/no-op
  noise, rank only compatible capsules by latest proven gate then latest durable time, and record the
  selected capsule plus the deterministic selection reason. Agent selection is a recommendation,
  not authority.
- Postconditions: the report contains first divergence, last proven gate, expected and observed
  states, actor identities and authoritative owner, selected compatible diagnostic capsule, and the
  smallest next probe.
- Failure behavior: if no compatible capsule exists, say so and recommend a fresh setup probe.
  Diagnostic replay authority always yields zero certification credit.
- Persistence/compatibility: capsules and replays remain immutable history under their original
  binding; incompatibility never erases them.

Implementation status:

- [x] The current report preserves ordered gate evidence, immutable report references, saved actor
  receipts, and a compact summary of repeated non-committed events.
- [x] R-003 — Failed runs do not report the first causal divergence or deterministically recommend a compatible diagnostic capsule.
  - Code gap: report assembly has raw/aggregated events and a step ledger but no capsule record, compatibility-ranked selector, last-green/first-red calculation, or smallest-next-probe field.
  - Required mechanism: use the existing ordered gates, saved receipts, binding history, and registry queries to persist capsule candidates and derive the required failure summary and deterministic recommendation.
  - Proof: seeded failures at successive lifecycle gates select the latest compatible capsule and report the exact earliest mismatch; an incompatible newer capsule is rejected, repeated identical events are summarized, and every resulting replay remains certification-ineligible.

### 4. Durable actor identity, ownership transfer, and crossing receipts

Mechanism:

- Files and symbols: `bandit_live_world.h` durable state and cursors;
  `bandit_live_world.cpp` handoff/dematerialization/advance/deserialization;
  `bandit_live_world_probe.*` structured transition stream; production callers in `do_turn.cpp`.
- Entry point: abstract-to-local materialization, local-to-abstract dematerialization, abstract
  advance, save, load, and post-relaunch reconciliation.
- Inputs: stable actor IDs, activity ID, generation, expected owner, handoff epoch, last-advance
  cursor, complete member snapshots, persistence result, and local NPC identity map.
- Preconditions: the durable outing is structurally valid, each live actor occurs exactly once,
  the expected cursor matches, and only the current owner may advance.
- Transition: build the destination candidate without mutating durable truth; bind or quiesce every
  member; persist the candidate; then acknowledge the next owner. Repeated identical receipts are
  no-ops; stale generations/epochs and partial pairs fail. Source truth remains authoritative until
  destination acknowledgement and persistence are established.
- Postconditions: each crossing has one compact receipt containing actor IDs, activity, generation,
  prior owner, next owner, epoch, outcome, persistence acknowledgement, and run/binding identity.
  Exactly one owner can advance each actor after the transition.
- Failure behavior: callback or persistence failure rolls back the complete pair. Load compares
  durable and local claims, repairs a uniquely determined stale projection or rejects ambiguity;
  it never silently advances both.
- Persistence/compatibility: the durable site/outing record remains authoritative across saves;
  receipts survive relaunch and are correlated to saved actor receipts by the harness.

Implementation status:

- [x] Materialization and dematerialization already use cursor/epoch checks, complete-pair candidate
  state, callback rollback, and commit-after-callback. Durable actor IDs and owner state serialize,
  and duplicate durable site claims are rejected on load.
- [ ] 🔴 R-004 — Actor-level ownership continuity is not completely receipted or reconciled across both crossing directions and load.
  - Code gap: `commit_local_pair_dematerialization` emits no symmetric transition result; `record_live_transition` does not populate `simulation_owner`; `append_live_transition_event` does not serialize it; load rejects duplicate durable site claims but does not reconcile durable owner state with loaded local NPC projections or prove persistence acknowledgement.
  - Required mechanism: retain the existing candidate/rollback transfers, add symmetric persistence-confirmed crossing receipts through the existing event stream, and reconcile or reject local projection claims during load before either simulation layer advances.
  - Proof: production-path tests cover successful, repeated, stale, partial, callback-failed, persistence-failed, duplicate, and crash-window crossings in both directions and across save/relaunch; each named actor retains one identity, one owner, a monotone generation/epoch/cursor, and one compact correlated receipt.

### 5. One continuous automated certification round

Mechanism:

- Files and symbols: `startup_harness.py` production probe/relaunch/gate routes, structured event
  reader, saved receipts, report assembly; scenario declarations and registry ingestion.
- Entry point: a distinct automated-certification command operating under certification authority.
- Inputs: a certification scenario that names the complete required C-AOL lifecycle and proof gates,
  plus the sealed binding and round lease from requirement 2.
- Preconditions: no existing segment or diagnostic artifact has certification authority; the round
  starts from its declared initial world and actor state.
- Transition: execute every named setup-independent lifecycle gate in causal order within one run
  lineage. A normal save, quit, and new process may occur, but the same round ID, binding, world,
  player, actor IDs, transition sequence, and gate ledger continue.
- Postconditions: one immutable report proves all named gates and both sides of every persistence and
  bubble crossing. The report passes only if every gate is green and identity/owner continuity holds.
- Failure behavior: stop certification credit at first divergence. No retry from a capsule,
  checkpoint rollback, replacement world, or assembled report may fill the missing suffix.
- Persistence/compatibility: final status and sealed evidence are ingested atomically; lower-class
  artifacts can be linked for diagnosis but not counted.

Implementation status:

- [x] The harness can run production gameplay steps, consume run-scoped events, correlate saved
  actor receipts, and relaunch the configured world under the same run ID.
- [ ] 🔴 R-005 — No distinct fail-closed automated certification route proves the entire required lifecycle in one bound uninterrupted round.
  - Code gap: the public routes are probe/handoff/repeatability; their focused gates and post-relaunch support do not seal complete certification authority, prevent segment assembly, or require the whole C-AOL lifecycle in one report.
  - Required mechanism: compose the existing production step, structured-event, save-receipt, and relaunch owners behind a distinct certification entry point that requires R-001, R-002, and R-004 and emits one atomic round result.
  - Proof: one natural production execution covers the complete hostile-ecology vertical slice—including departure, overmap advance, both bubble crossings, actor-level outcomes, save/quit/relaunch, return/report, and downstream camp decision—while rollback, segment splicing, diagnostic replay, fixture/scenario mutation, and replacement identities each produce a failed or invalidated certification result.

### 6. Separate ordinary Windows feel gate

Mechanism:

- Files and symbols: harness handoff/report generation and scenario-registry evidence records; no
  product automation owns Josef's judgment.
- Entry point: after an automated certification pass, create a Windows ordinary-play handoff.
- Inputs: certified build/binding reference, concise ordinary-start instructions, expected world,
  and Josef's eventual pass/fail judgment with optional notes.
- Preconditions: the automated result is visible but cannot pre-answer the feel gate. Exploratory
  Windows play may exist separately and is labeled non-final.
- Transition: launch or package an ordinary play state without scripted proof instructions or debug
  overlays; record Josef's explicit judgment as Windows feel evidence.
- Postconditions: the two final gates are displayed independently; overall acceptance requires both
  and preserves who decided each.
- Failure behavior: absent judgment remains pending; automated green cannot turn it green, and feel
  approval cannot repair automated failure.
- Persistence/compatibility: record build/binding and platform with the judgment, while treating it
  as human evidence rather than machine lifecycle proof.

Implementation status:

- [ ] 🔴 R-006 — The Windows feel gate has no separate ordinary-play handoff and owner-authored evidence state.
  - Code gap: current handoff mode is a scenario/probe continuation and the registry has no dedicated Josef-owned Windows feel result linked independently to certification.
  - Required mechanism: adapt the existing handoff/report and registry surfaces to prepare ordinary Windows play and record Josef's explicit outcome without turning it into another scripted automated checklist.
  - Proof: a certified build produces an ordinary Windows handoff; pending, pass, and fail judgments are recorded only by Josef, remain separate from automation, and overall acceptance is green only when both independent gates pass.

## Competing systems and override direction

| State or action | Readers | Writers / competing owners | Authoritative decision |
|---|---|---|---|
| Durable ecology actor state | Overmap scheduler, local projection calls, save/load, harness receipt reader | Abstract overmap simulation; local NPC simulation | `world_state`/`active_outing_state` is durable truth. Only `simulation_owner` may advance; local NPCs hold a cursor/epoch-bound temporary lease. |
| Owner transfer | Abstract scheduler and local game loop | Handoff and dematerialization transaction callbacks | The current owner retains authority until the complete destination candidate is acknowledged and persisted. Stale/replayed cursor or epoch is unchanged or rejected. |
| Evidence class/authority | Reports, registry queries, final-gate evaluator | Command route, report ingestion, manual Windows result | Start-time route authority is immutable. Ingestion derives eligibility and cannot promote evidence. Josef alone writes the Windows feel judgment. |
| Binding | Runner, registry, diagnostic selector | Runtime hashers, fixture/scenario resolvers, world/player/actor receipts | The sealed certification manifest is authoritative for that round. Any mismatch invalidates final credit but not historical focused proof. |
| Certification sequence | Structured reader, gate evaluator, final report | Gameplay process, relaunch process, checkpoint/diagnostic tooling | One round lease and append-only sequence win. Relaunch may continue the round; rollback, replacement, replay, and splicing cannot write certification gates. |
| Diagnostic capsule choice | Failure report and agent | Registry candidates, user or agent starting probes | Compatibility and deterministic rank constrain recommendations; selection does not grant authority and replay credit is always zero. |
| Process/world ownership | Startup/cleanup and game process | Other probes, manual play, certification | An exclusive round lease wins for its exact PID/executable/world; unrelated processes are not killed. Conflict fails closed. |

The ownership transfer is a compare-and-swap on activity ID, generation, owner, handoff epoch, and
last-advance cursor plus a complete actor set. The destination cannot advance before acknowledgement
and durable persistence. The losing owner yields without editing the winner. Duplicate identical
receipts are no-ops; stale or conflicting receipts are explicit failures. Aggregate population or
resource state never substitutes for a named actor receipt.

## Acceptance and proof

For each red ID, proof follows:

```text
preconditions -> authoritative owner -> transition -> observable outcome -> artifact -> pass/fail
```

| Red ID | Outcome test | Required evidence | False-green controls |
|---|---|---|---|
| `R-001` | Run and ingest every evidence route, then query both final gates. | Immutable class/authority records and gate-eligibility query. | Caller label changes, report edits, focused greens, diagnostic replay, startup image, helper result, and build success cannot promote credit. |
| `R-002` | Seal a round, relaunch normally, then independently mutate or replace each binding member and introduce a competing process. | Binding manifest, component hashes/identities, lease history, first mismatch. | Partial source hash, same scenario name with changed bytes, replacement save/player/actor, PID reuse, and sequence rollback must fail. |
| `R-003` | Fail successive causal gates with compatible and incompatible capsule candidates. | Failure reports with first divergence, last gate, expected/observed, actors/owner, chosen capsule/reason, next probe. | Newest-but-incompatible capsule, repeated event flood, and replay certification credit must fail. |
| `R-004` | Cross abstract/local both ways, save/relaunch at each owner boundary, and exercise retry/crash/duplicate paths. | Persisted actor/owner state and correlated compact crossing receipts. | Aggregate populations, helper-only events, partial pairs, duplicate owners, stale epochs, callback-only success, and unpersisted acknowledgement must fail. |
| `R-005` | Execute the complete hostile-ecology lifecycle once through the natural production route. | One sealed certification report with uninterrupted round/gate/event sequence and saved identity receipts. | Segment union, capsule replay, rollback, fixture/scenario edit, replacement world/player/actor, focused tests, or synthetic state setting must fail. |
| `R-006` | Hand a certified Windows build to ordinary play and record Josef's judgment. | Separate certified reference and Josef-owned Windows feel record. | Automated pass cannot supply feel; exploratory play cannot supply final feel; feel pass cannot repair automation. |

## Freeze record

- Status: `Frozen`
- Frozen source baseline: remote `dev` at
  `3f8d5e0a1b6b21ed7e23f43d27a9e40aafa1fe88`, tracking `origin/dev`, with the
  intentionally broad dirty frontier and inspected-source fingerprint
  `e26e145206657d5cb43a3482617be8a09400d37a8bb062f96b74b5cc59016217`.
- User-owned choices: preserve useful focused proof; diagnostic capsules are recommendations and
  replays earn zero certification credit; final automation is one continuous bound round; normal
  save/quit/relaunch is permitted within that round; binding changes, rollback, splicing, and
  replacement identities invalidate final credit; durable identity and exactly one owner cross
  overmap/local/persistence boundaries; automated certification and Windows feel are independent.
- Evidence-implied refinements: the inspected code supports durable outing state as authority and
  local NPCs as temporary cursor/epoch-bound projections; retain its candidate/rollback transaction,
  require persistence-confirmed symmetric receipts, and reconcile or reject projection conflicts at
  load. Aggregate population evidence remains ineligible for named actor claims.

After freeze, automation may only close existing red items after named proof and remove their red
markers; make an evidence-implied nonmaterial clarification; or append a uniquely implied
same-contract mechanism, ownership/proof detail, and necessary new stable red claim after a verified
phase-3 worker finding. Existing claim identities, text, status, accepted work, and acceptance
strength remain fixed. Refreeze immediately. Product intent, project language, permissions,
user-visible behavior, balance, and materially different design choices remain user-owned.
