# C-AOL Harness Cockpit and Proof-Preserving Playtesting DE-67 Functional Specification

Status: Refrozen
WEC: `.de67/WEC.md`
Source baseline: `Cataclysm-AOL-hostile-ecology-dev | dev tracking origin/dev | 54a60872e2b8dcabb00288e8fcfe6b976b82fd99 | intentionally broad dirty product and harness frontier | inspected 2026-08-25`
Inspected-source fingerprint: `98e30ed1400ad496ce21a4b140fea69b8cbe76a08977176ff9eb641846043402` (SHA-256 over the ordered per-file SHA-256 records named in the current code map)

## Document authority

This document is the mechanistic product contract derived from the user-owned WEC and inspected
production code. It is not a task-dispatch plan. If this document conflicts with current code about
what code does, re-inspect the code; if it conflicts with the WEC about what the product should do,
the WEC and the user win.

The source baseline includes the modified and untracked product, harness, registry, semantic-frame,
proof, fixture, and hostile-ecology files named below. The dirty frontier is evidence under
inspection, not Phase-2 implementation credit, and was preserved.

Status markers:

- `[x]` — present in the production path with proportionate evidence.
- `[ ] 🔴 R-...` — missing, wrong, or unproved; the stable red item is implementation work.

## Functional contract

C-AOL exposes one compact harness-only cockpit. A worker discovers the current proof frontier,
searches or creates a compatible scenario, prepares deterministic setup, observes only the avatar's
perceivable world, performs bounded player-intent transactions, and receives honest evidence or a
durable reusable capability gap. The cockpit does not expose registry tokens, frame offsets,
keystrokes, OCR guesses, or raw logs as gameplay state.

C-AOL development runs preserve compatible focused proof and recommend a compatible known state for
diagnosis without awarding either one final-certification credit. A final automated pass starts a
new, completely bound round and proves every required lifecycle gate in that one round. Josef then
performs a separate ordinary Windows play pass.

```text
frontier -> scenario -> capability -> game -> run -> gap
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

- **Cockpit** — the compact worker-facing interface for scenario discovery, deterministic setup,
  live observation, action transactions, evidence, and reusable gaps.
- **Intervention** — a recorded setup or recovery mutation that earns no proof for the state or
  behavior it manufactured.
- **Capability gap** — a durable reusable record of a missing observation, action, setup, recovery,
  or structured failure surface.
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
language and is not a diagnostic capsule or certification boundary. Do not call setup validation
gameplay proof. Do not call registry tokens, frame offsets, key bindings, raw logs, or OCR a
worker-facing gameplay observation.

The cockpit is a harness-only product surface. `src/llm_intent.cpp` and its declarations, prompts,
NPC-centred snapshot text, request-scoped target mapping, action parser, runner timing, and NPC
behavior are a protected regression boundary. Cockpit code must not intercept or replace that path.

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
| Existing LLM perception pattern | `src/llm_intent.cpp` :: `filter_visible`, `build_ascii_map_snapshot`, `build_snapshot_json`; `npc::set_llm_intent_legend_map` | The NPC LLM path reads `npc::get_visible_creatures`, `Creature::sees`, `map`, `creature_tracker`, avatar state, items, time, and weather to build an NPC-centred prompt snapshot. It stores request-scoped letter targets in NPC LLM state. This path does not expose an avatar cockpit frame and is protected from harness changes. | `tests/llm_intent_test.cpp` checks snapshot text, visibility/attitude, letter targets, stale request rejection, move parsing, and NPC behavior. |
| Native semantic frames | `src/handle_action.cpp` :: `openclaw_harness_semantic_step_frame`, `openclaw_harness_semantic_step_receipt`, `game::get_player_input`, `game::handle_action`; `tools/openclaw_harness/semantic_state.py`; `semantic_broker.py` :: `SemanticStepChannel` | The game emits run-bound frames and receipts only for world wait, wait menus, and wait activities. Python copies and parses selected `debug.log` lines, keeps key bindings private from `observe()`, and requires a matching receipt plus a fresh frame. It is a useful transaction prototype, not a general observation or action surface. | `semantic_step_test.py`, `semantic_state_test.py`, and `semantic_broker_test.py`. |
| Modal and interruption readers | `src/popup.cpp`, `src/game.cpp`, `src/npctalk.cpp` semantic/debug trace writers; `startup_harness.py` :: `read_active_semantic_ui_trace`, `read_active_activity_query_trace`, `acknowledge_blocking_interruptions` | Quit, activity-distraction, and EOC popup owners emit different debug-log grammars. The harness combines native traces with extensive OCR and screen classifiers. Some safe paths use advertised semantic actions; many legacy paths still infer current UI or recovery from logs, OCR, fixed keys, and offsets. | Modal, semantic-broker, quit-confirmation, proof-classification, and scenario tests. |
| Scenario discovery and selection | `scenario_registry.py` :: `validate_manifest`; `scenario_registry_store.py` :: `manifest_current`, `manifest_capability_current`, `evaluate_registry_query_from_store`, `execute_registry_query`, `reload_selection_token_for_launch`; `scenario_registry_cli.py` | SQLite projects typed manifest capabilities, lifecycle, binding, proof, report, and selection history. `registry-query` evaluates hard requirements and preferences without launching. An eligible result mints an internal token that the canonical launcher revalidates. Purpose, starting state, actors, setup limits, and capability contracts are not available as one progressive worker-facing scenario view. | Scenario registry CLI, ingestion, store, binding, and production-binding tests. |
| Scenario creation and setup | `startup_harness.py` :: fixture capture/install and save transforms, `debug_spawn_monster`, `debug_spawn_follower_npc`, `debug_map_editor_place_furniture`, item/zone/fire helpers; `scenario_registry_cli.py` certification fixture install | Existing setup owners can install or transform saves and drive debug UI helpers. The helpers use raw menu paths and timing and do not share one structured intervention receipt, free-tile resolver, stable target owner, validation result, or zero-proof firewall. The registry has no worker-facing scenario create/validate/prepare transaction. | Fixture-contract, registry production-binding, startup, saved-state audit, and scenario tests. |
| Proof classification | `startup_harness.py` :: `startup_proof_classification`, `probe_proof_classification`, `evaluate_structured_proof_gates`, report finalization; `scenario_registry_store.py` :: WEC authority and final-gate eligibility | Current proof owners preserve startup/load, focused, diagnostic, certification, and Windows authority boundaries. Large legacy classifiers still accept OCR and raw screen/log artifacts for limited evidence and diagnostics. No cockpit call reports its evidence effect through one structured result. | `proof_classification_unit_test.py`, registry ingestion tests, certification route tests. |
| Planned competing perception/controller | `doc/OPENCLAW_HARNESS.md` proposed `src/openclaw_harness.*`, `src/openclaw_ui_adapter.*`, frame schema, classifier, and translator; `TechnicalTome.md` adaptive-playtesting note | The plan describes a parallel C++ harness perception/manager layer and optional debug spawning. Only the wait-specific semantic channel is shipped. The planned second perception owner would duplicate game-state interpretation if implemented as written. | Static design documents; no production-path implementation credit. |
| Debug ecology projection | `src/ecology_debug_view.*`, `src/overmap_ui.cpp`, `TechnicalTome.md` debug observer sections | The debug view intentionally exposes authoritative offscreen ecology state under debug authority. It is useful for diagnosis and incident artifacts but is not avatar perception and cannot populate `game.observe` or `game.look`. | Ecology debug-view tests and durable design note. |

The inspected-source fingerprint covers, in order: `src/llm_intent.h`,
`src/llm_intent.cpp`, `src/handle_action.cpp`, `src/input_context.h`,
`src/input_context.cpp`, `src/popup.cpp`, `src/game.cpp`, `src/do_turn.cpp`,
`src/npctalk.cpp`, `src/ecology_debug_view.h`, `src/ecology_debug_view.cpp`,
`src/bandit_live_world.h`, `src/bandit_live_world.cpp`,
`src/bandit_live_world_probe.h`, `src/bandit_live_world_probe.cpp`,
`tests/llm_intent_test.cpp`, `tests/bandit_live_world_test.cpp`,
`tests/bandit_live_world_natural_test.cpp`, `tools/openclaw_harness/startup_harness.py`,
`scenario_registry.py`, `scenario_registry_store.py`, `scenario_registry_cli.py`,
`semantic_state.py`, `semantic_broker.py`, `production_capture.py`,
`scenario_registry_cli_test.py`, `scenario_registry_ingestion_test.py`,
`semantic_state_test.py`, `semantic_broker_test.py`, `semantic_step_test.py`,
`proof_classification_unit_test.py`, `doc/OPENCLAW_HARNESS.md`, and
`TechnicalTome.md`.

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
- [x] R-004 — Each actor keeps one identity and one owner across crossings and relaunch.
  - What changed: load repairs a uniquely matching stale pending crossing after a failed save and rejects partial, malformed, conflicting, or ambiguous ownership claims. `crossing_receipt::run_id` now connects each saved receipt to the harness run or durable world binding that produced it. Acknowledgement and rollback tokens validate the same identity.
  - Why it matters: a failed second save can no longer make relaunch reject an actor that has one safe source owner. A stale or conflicting receipt cannot silently let both simulation layers advance the same actor.
  - Proof: a fresh supported test build passes the projection lease control with 11 assertions, the crossing receipt and transition event controls with 1,065 assertions, and the transactional handoff control with 2,045 assertions. These production-path tests cover success, repeat, stale, partial, callback failure, persistence failure, duplicate, crash-window repair, both crossing directions, and save or relaunch boundaries.

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
  actor receipts, relaunch the configured world under the same run ID, seal certification
  authority, reject segment assembly, and finalize one atomic report.
<!-- DE67:DFS-SLICE:BEGIN id=R-005-S001 claim=R-005 -->
- [ ] 🔴 R-005 — No natural run has yet proved the entire required lifecycle through the
  fail-closed certification route in one bound uninterrupted round.
  - Current gap: zero-credit runs proved route construction and native travel progress, but did not
    establish a repeatable precondition-compatible path to the certification destination. The best
    run reached the penultimate overmap tile; the next fresh counterexample stopped earlier on an
    undeclared zombie attack. The proposed final-handoff defect was therefore not reproduced and
    remains unproved.
  - Required mechanism: first qualify a materially different natural route or geography with zero
    credit while preserving the same world, player, ecology actors, scheduler inputs, destination,
    lifecycle gates, and fail-closed threat handling. Only after that route reaches the destination
    may a worker diagnose and repair a reproduced final-handoff divergence. Then start one independent
    certification round and let its first causal divergence, rather than a prior replay hypothesis,
    determine any further repair.
  - Proof: one natural production execution covers the complete hostile-ecology vertical slice—including departure, overmap advance, both bubble crossings, actor-level outcomes, save/quit/relaunch, return/report, and downstream camp decision—while rollback, segment splicing, diagnostic replay, fixture/scenario mutation, and replacement identities each produce a failed or invalidated certification result.
<!-- DE67:DFS-SLICE:END id=R-005-S001 claim=R-005 -->

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

### 7. Semantic harness observation and intent-aware recovery

This mechanism is setup support for focused qualification and diagnosis. It does not change hostile
ecology, movement, visibility, or any other player-visible rule, and it earns zero continuous-round
certification credit.

Mechanism:

- Files and symbols: the existing harness environment gate and bound transition-event writer in
  `bandit_live_world_probe.*`; the gameplay, travel, activity, destination, and modal owners that
  possess each fact; `startup_harness.py`; versioned scenario grammar; and a reviewed semantic
  handler registry plus replay corpus under `tools/openclaw_harness/`.
- Entry point: an explicitly authorized harness run path. Ordinary play without that path performs
  no extra event emission, file creation, polling serialization, or background work.
- Product observation: emit bounded, deduplicated semantic transitions beside the authoritative
  production state change. Each event carries run identity, monotone sequence, transition kind,
  relevant stable identity, before/after state, and outcome, and writes only within the exact bound
  run directory. Required facts include gameplay/input context, native-travel progress and
  completion, activity state, destination arrival, semantic prompt class, and blocking-modal
  lifecycle.
- Evidence boundary: structured product events and bound saved artifacts alone may affect machine
  verdicts. Moon phase, clock or weather text, map glyphs, creature names, transient messages,
  fixture-specific HUD fragments, localization, UI wording, screenshots, and OCR are diagnostic
  context unless this DFS explicitly requires that exact fact. OCR cannot make a gate green or red,
  authorize input, establish or contradict state, recover or reject a run, or compensate for absent
  structured evidence. Unrelated incidental facts cannot be combined into synthetic semantic proof.
- Agent step channel: the game emits a fresh frame identity, current semantic state, and valid action
  IDs from the authoritative input or modal owner. The harness keeps physical bindings private,
  accepts only a current-frame choice from the same run and worker session, and requires both the
  native accepted/rejected receipt and a fresh next frame. Unknown, invalid, stale, contaminating,
  wrong-destination, escaped-authority, and release-invalidating choices receive no input and remain
  recorded. An advertised recovery action is conditional unless the scenario explicitly requires
  its occurrence. Proven non-occurrence may continue only with a zero-credit materiality record;
  an observed interruption, missing required receipt or postcondition, owner mismatch, persistence
  gap, or causal harness defect remains fail-closed. Fixed input may reset or stage a deterministic
  footing; live play proceeds as `observe -> bound -> choose -> act -> receipt + next state`.
- Experience: the scenario fixes authority, invariants, proof targets, and native capabilities; the
  worker owns continuation and explicit finish. A progressing observation remains live. Every
  continuation names its expected causal signal and a maximum derived from the mechanic, scheduler,
  path, or measured rate. Proof, no progress, unsafe divergence, authority or binding drift, missing
  receipts, and bound exhaustion terminate; an arbitrary observation or transport timeout does not.
  The final immutable report preserves the action/observation sequence, bound, stop reason, cleanup,
  unused authority, first divergence, and contradictory evidence. A reviewed handler still requires
  deterministic recovery and inverse stale/invalid replays, replacing rather than adding key guesses.

Implementation status:

- [x] ✅ R-007 — The harness now decides the required travel, arrival, activity, interruption,
  destination, progress, and popup-recovery routes from bounded run-owned semantic facts.
  - Code gap: version-2 `checkpoint_safe_ui` requires `screen_text_contains`; the active R-005
    scenario declares OCR and screen phrases; native travel and wait recovery classify OCR text and
    may send prompt keys without a bound semantic UI identity or structured postcondition.
  - Required mechanism: implement the semantic event and broker contract above through the existing
    environment gate and transition stream; migrate scenario grammar and active routes; delete the
    superseded incidental proof and key-guess paths.
  - Proof: tests cover zero-artifact ordinary play; missing or escaped paths; bounded emission;
    completion, interruption, recovery with and without progress, wrong destination, unknown modal,
    and contamination; expected-versus-accidental UI; stale identity; and invariant verdicts while
    irrelevant moon, weather, clock, message, map, fixture, localization, wording, screenshot, and
    OCR inputs vary, disappear, succeed, fail, or contradict the structured facts.
  - Accepted evidence: `R-007-closure-006` closed `R-007-live-broker-path~B1`. Focused tests passed
    across the semantic parser, production caller, scenario, and popup broker routes. Relevant native
    objects built successfully. The registry returned no executable token, so this acceptance claims
    focused implementation proof only and claims no live feature or certification credit.
  - Owner-authorized mutation proof: canonical run `37c1510a…f082` kept session
    `c6559f71375f-m095` through two native
    `world.wait -> wait.duration_menu -> wait.6h` cycles. Authoritative time advanced
    `7560 -> 7920 -> 8280` minutes with a receipt and fresh frame after every choice. The run remained
    zero-credit and honestly red when its saved-state audit found no structural-scout consumer.

### 8. Improved-harness focused qualification tail

These qualification runs use the semantic substrate after R-007. They preserve focused development
proof but cannot satisfy R-005, cannot be spliced into a continuous round, and cannot satisfy R-006.
Orthogonal stabilizers or observer instrumentation are setup support only and may not change ecology,
movement, or visibility behavior.

A focused lifecycle row declares the exact transition predicate and owner state it intends to
persist. The harness evaluates that predicate after every native transaction and before another
progression action. On the first match, unused progression actions are ineligible and persistence is
the next proof-bearing transaction. A row targeting a later owner state declares that boundary
directly. A fixed action order that can cross the target before capture fails closed; an absent or
unevaluable boundary stays red and later state cannot be reinterpreted as proof of the missed state.

Implementation status:

<!-- DE67:DFS-SLICE:BEGIN id=R-008-S001 claim=R-008 -->
- [ ] 🔴 R-008 — Natural bandit and cannibal lifecycles and their persistence boundaries lack the
  restored focused qualification matrix.
  - Required mechanism: exercise natural production bandit and cannibal lifecycles with stable actor
    identity and single-owner receipts, then save, quit, and relaunch at each materially distinct
    abstract/local ownership and crossing boundary. Preserve every run as independently classed
    focused evidence; never resume, roll back, or join segments for certification credit.
  - Proof: both ecology families reach their named lifecycle outcomes through production behavior;
    each persistence matrix row proves the expected actor identity, generation, owner, crossing
    receipt, and saved normalization before and after relaunch; malformed, stale, duplicate, partial,
    and replacement-identity controls fail closed.
<!-- DE67:DFS-SLICE:END id=R-008-S001 claim=R-008 -->
<!-- DE67:DFS-SLICE:BEGIN id=R-009-S001 claim=R-009 -->
- [ ] 🔴 R-009 — Integrated waits, performance and memory observation, and the supported-platform
  technical witness set remain unqualified on the improved harness.
  - Required mechanism: observe complete integrated waits at existing semantic boundaries, including
    product game-time progress, latest transition, child-process CPU and resident memory when the host
    exposes them, and explicit unavailable fields otherwise. Run proportionate technical witnesses
    on macOS, Linux/WSL, and Windows against source-bound executables and the same semantic contracts.
  - Proof: advancing and stalled waits are distinguishable without incidental UI text; repeated
    events remain bounded and causally readable; missing resource fields are `unavailable`, never
    invented zero; and each platform witness records the build/runtime binding, exercised route,
    direct result, and platform-specific limitation. Every artifact remains setup or focused
    qualification evidence with zero continuous-final-certification credit.
<!-- DE67:DFS-SLICE:END id=R-009-S001 claim=R-009 -->
<!-- DE67:DFS-SLICE:BEGIN id=R-015-S001 claim=R-015 -->
- [ ] 🔴 R-015 — Focused lifecycle scenarios can advance past the owner boundary they intend to
  persist before evaluating it.
  - Code gap: scenario progression and proof-gate evaluation are independently ordered, so a valid
    transition can occur while later time-driving actions remain queued and invalidate the owner
    state expected by the eventual persistence audit.
  - Required mechanism: make the declared causal boundary an incremental execution precondition and
    stop the row at its first matching transition as specified above.
  - Proof: a counterexample row whose target matches before a remaining wait is rejected or stops
    before that wait; the corresponding positive row persists immediately. Wrong-run, wrong-actor,
    and wrong-owner events do not match, while a row explicitly targeting the later owner state may
    continue to that separately declared boundary.
<!-- DE67:DFS-SLICE:END id=R-015-S001 claim=R-015 -->

### 9. One progressive cockpit interface

Mechanism:

- Files and symbols: add a thin harness façade at
  `tools/openclaw_harness/cockpit.py` with `CockpitService.call(request) -> dict`. The façade
  imports existing registry, startup, semantic, report, and binding owners. It must not duplicate
  their durable state or parse their command-line text.
- Entry point: one worker call names a cockpit operation and structured arguments. Approximate WEC
  operation names may be mapped to stable implementation names without exposing transport details.
  After scenario selection, `run.open` starts a tracked playtest without requiring prior proof
  eligibility.
- Parameters: operation, cockpit schema version, run/session identity when a game is active, and
  operation-specific arguments.
- Inputs: the compact frontier supplied by the caller; current SQLite scenario, capability,
  binding, lifecycle, proof, and report facts; the selected run; and native game responses.
- Preconditions: the caller sees only the behavior under test, required evidence class, unresolved
  proof gap, relevant binding, forbidden shortcuts, and available capability summaries. Full
  manuals, raw history, logs, tokens, offsets, and bindings are not preloaded.
- Transition: validate one request, call one authoritative owner, and return one result envelope.
  Summary calls reveal identifiers and fit-relevant facts. Describe calls reveal the selected
  object's contract, examples, recovery, proof effect, and limitations. One live service instance
  retains run-scoped handles and receipts across `game.observe`, `run.continue`, `game.act`,
  `run.status`, and explicit `run.finish`; scenario execution does not pre-frame its last observation.
- Postconditions: every call returns one object containing operation status, structured result or
  structured failure, evidence effect, and valid next calls. Subprocess stdout, registry selection
  tokens, frame offsets, physical inputs, OCR, and raw logs remain internal.
- Failure behavior: malformed, stale, unsupported, or owner-conflicting requests fail without game
  input or durable promotion. The response names the first rejected precondition and useful next
  calls.
- Persistence/compatibility: the façade owns no product or proof truth. It supports Windows,
  Linux/WSL, and macOS through the existing Python and game platform seams.
- Protected boundary: the façade must not import, invoke, intercept, or replace the
  `llm_intent` request/response path. It must not alter NPC prompt timing or behavior.

Implementation status:

- [x] The scenario registry, startup harness, semantic-step prototype, and report owners already
  return structured Python values internally.
- [x] R-010 — Workers do not have one progressive cockpit whose calls return one structured
  result without exposing internal transport.
  - Code gap: `scenario_registry_cli.py`, `startup_harness.py`, and semantic helpers expose
    separate CLIs and artifacts. Workers must currently coordinate tokens, offsets, PIDs, raw
    output, and detailed commands.
  - Required mechanism: add the stateless façade above and compact frontier/capability discovery.
    Existing registry and run owners remain authoritative.
  - Proof: a worker receives only the compact frontier, searches one relevant capability and
    scenario summary, asks for one detail, and continues through the same façade. Token, offset,
    key, OCR, log, and subprocess-output fields are absent from every public result and cannot be
    supplied to bypass owner validation.

### 10. SQLite scenario lifecycle and deterministic setup

Mechanism:

- Files and symbols: `scenario_registry.py::validate_manifest`;
  `scenario_registry_store.py` schema migrations, `evaluate_registry_query_from_store`,
  `execute_registry_query`, and `reload_selection_token_for_launch`;
  `scenario_registry_cli.py` canonical launch owner; `startup_harness.py` fixture and debug
  setup helpers; cockpit `scenario.search/describe/create/validate/select/prepare`.
- Entry point: scenario discovery starts from typed proof and capability requirements. Preparation
  starts only after one current selection.
- Inputs: scenario purpose, starting state, actors, fixture/save identity, compatible bindings,
  capabilities, evidence authority, prior scoped results, setup limitations, requested deterministic
  entities, and selected setup capability.
- Preconditions: search facts come from the current SQLite projection. Create accepts a complete
  deterministic declaration under the canonical scenario root. Validate reads the exact declaration,
  fixture, save, and capability owners but does not launch or award gameplay credit.
- Transition: search returns summaries; describe returns details on demand; select evaluates hard
  requirements and preferences and records the worker's fit reason beside the internal selection
  history; create writes and validates one source-bound declaration before projection; prepare
  installs the selected fixture and performs requested setup through existing native debug or saved
  fixture owners.
- Deterministic entities: a required creature, NPC, item, terrain condition, or interruption must
  exist in the selected loaded save or be created through an explicit setup intervention. Debug
  placement resolves a currently free tile before dispatch and confirms the exact spawned or placed
  target. Missing expected entities never cause incidental-world waiting.
- Intervention receipt: every fixture install, save transform, spawn, targeted removal, item,
  furniture, zone, shelter, basecamp, fire, brazier, `roof_on_fire`, or other setup/recovery action
  records operation, arguments, selected target/tile, native receipt, before/after setup facts,
  run/binding, and `evidence_effect: none_for_manufactured_state`.
- Macros: a setup or recovery macro stores ordered constituent intervention/action receipts. A
  macro has no independent proof effect.
- Postconditions: one selected scenario brief names why it fits, what setup occurred, the current
  binding, limitations, and the proof firewall. Scenario validation proves setup only.
- Failure behavior: absent capability, occupied or stale target, conflicting binding, incomplete
  fixture, failed native setup, or unconfirmed postcondition leaves the run unprepared and returns a
  reusable capability gap candidate. The harness never substitutes an incidental entity.
- Persistence/compatibility: extend the existing SQLite schema with append-only scenario-validation,
  selection-reason, and intervention history. Existing source manifests and binding histories remain
  authoritative; create is idempotent for identical bytes and rejects identity collisions.

Implementation status:

- [x] SQLite already projects typed scenario capabilities and immutable lifecycle, binding, report,
  query, and token history. Existing fixture/debug helpers cover several requested setup actions.
- [x] R-011 — Scenario creation, setup validation, selection rationale, and deterministic
  preparation are not one SQLite-backed, proof-firewalled lifecycle.
  - Code gap: registry search lacks progressive purpose/starting-state/actor/setup-limit summaries;
    no worker-facing create/validate/prepare transaction exists; debug and fixture helpers use
    separate key/timing and transform paths without one intervention receipt or deterministic-target
    contract.
  - Required mechanism: extend the existing manifest projection and schema histories, then wrap
    current setup owners with the lifecycle and receipt rules above.
  - Proof: the worker selects a compatible existing scenario and records why; a no-fit query creates
    and validates a deterministic scenario; a required zombie dog is present or explicitly spawned
    on a confirmed free tile; an absent incidental dog is reported only as absent; every manufactured
    fact stays setup-only under report ingestion and final-gate queries.

### 11. Avatar-centred observation and stable handles

Mechanism:

- Files and symbols: add a narrow, harness-gated adapter in
  `src/openclaw_harness_observation.h/.cpp`; read `game`, `avatar`, `map`,
  `creature_tracker`, `Creature::sees`, item/location, field, light, weather, time, activity,
  input-context, and modal owners. Expose the adapter only through the run-bound cockpit channel.
- Existing pattern: `src/llm_intent.cpp::filter_visible`, `build_ascii_map_snapshot`, and
  `build_snapshot_json` demonstrate game-native visibility, spatial summary, and target concepts.
  They are inspection evidence, not cockpit entry points.
- Protected boundary: the adapter is harness-only and reads the same lower-level authoritative game
  state independently. It must not call or alter `llm_intent` request creation, prompts, NPC
  snapshots, request target maps, action parsing, runner timing, or NPC execution. If later work
  extracts a shared primitive, neutral code must own it and direct regression proof must show
  unchanged LLM-intent observable output and behavior.
- Entry point: `game.observe` after run preparation and after every meaningful action or
  interruption; `game.look(target_or_area?)` on demand.
- Inputs: the current avatar, loaded map, avatar-perceivable entities and tiles, current activity,
  native input/modal context, last accepted observation, and run handle table.
- Transition: refresh native visibility; build a compact local map centred on the avatar; emit
  avatar position/z, activity, visible creatures/NPCs/items/interactables/terrain/furniture/fields/
  hazards, relevant light/weather/time, active modal/interruption, available semantic actions, and
  material changes. `look` returns focused detail only for a currently perceivable target or area.
- Perception boundary: offscreen world, debug-clairvoyance/ecology projections, logs, OCR, and
  registry declarations cannot populate visible fields. A missing or non-visible entity has no
  inferred location, cause, or movement story.
- Handles: one run-owned table assigns opaque handles to observed creatures, NPCs, items, and
  interactable locations. An NPC or ecology actor may rebind only through its exact durable identity
  and binding. A process-local creature/item/location handle that cannot prove the same identity
  after disappearance or relaunch becomes stale. Map markers may change without changing a proved
  handle.
- Postconditions: unchanged observations may return a compact unchanged result; meaningful actions
  and interruptions always return a fresh observation identity. Long activities emit boundary
  changes and interruptions, not every turn.
- Failure behavior: a stale handle, non-visible target, unknown modal, unavailable observation
  owner, or binding mismatch rejects the request and returns a fresh safe observation when the game
  remains usable. A stale handle is never retargeted.
- Persistence/compatibility: handles are run-scoped evidence metadata, not save state or gameplay
  identity. The adapter performs no work when the harness gate is absent.

Implementation status:

- [x] The game already owns every required state fact, and the LLM-intent snapshot proves that
  visibility and a compact local map can be derived from game-native reads. The wait prototype has
  fresh frame identities.
- [ ] 🔴 R-012 — The cockpit has no avatar-centred, game-native observation or stable handle owner.
  - Code gap: `llm_intent.cpp` is NPC-centred and protected; current semantic frames contain only
    state/action IDs; screen/OCR, saved audits, and debug ecology views are competing partial readers
    and cannot supply live avatar perception.
  - Required mechanism: implement the harness-only adapter and handle rules above while leaving
    LLM-intent observable behavior unchanged.
  - Proof: in one live run, observation reports visible local facts and omits an offscreen zombie
    dog; look succeeds for a visible handle; movement preserves the same proved entity handle while
    its marker changes; disappearance makes an unprovable handle stale; a stale action is rejected
    with a fresh observation. OCR/log/debug/global contradictions cannot change the frame.
    `tests/llm_intent_test.cpp` and direct NPC request/action regression checks produce unchanged
    observable output and behavior.

### 12. Player-intent transactions and native recovery

Mechanism:

- Files and symbols: `game::get_player_input`, `game::handle_action`,
  `game::do_regular_action`, `input_context`, native popup/menu/activity owners, and the
  harness-gated observation adapter; merge the useful identity/receipt rules from
  `SemanticStepChannel` into cockpit `game.act`.
- Entry point: `game.act(intent,target?,parameters?,recovery_policy?)` from a fresh observation.
- Inputs: current observation identity, advertised semantic action, optional current stable handle,
  parameters, recovery policy, current input/modal identity, and expected native postcondition.
- Preconditions: the observation belongs to the current run/session; the action is advertised; any
  handle is current and visible when the action requires visibility; the recovery policy authorizes
  only named safe responses.
- Transition: recheck the precondition; translate semantic intent privately; dispatch through the
  existing native input and `game::handle_action` route; consume the exact native accepted/rejected
  receipt; handle only expected safe interruptions; confirm the postcondition; return a fresh
  observation.
- Strategy boundary: the worker chooses look, move, wait, interact, talk, target, attack, use-item,
  and activity strategy. The harness owns native UI mechanics, physical input, receipts, and safe
  recovery. The adapter does not call gameplay mutators directly to manufacture a proof-bearing
  outcome.
- Success result: state changes, interruption/responses, proof-bearing events and their evidence
  effect, constituent recovery receipts, and the fresh observation.
- Failure result: first unrecovered divergence, expected and observed state, recovery attempted,
  whether the game remains usable, and valid next calls. Feature failure remains visible.
- Recovery: expected harmless interruptions may be resolved only from the current native modal
  identity and advertised actions. Material gameplay choice, unauthorized response, unconfirmed
  recovery, unknown modal, or feature failure returns control without additional input.
- Persistence/compatibility: one append-only transaction receipt binds run, session, issuing
  observation, action, target identity, native receipt, recovery, postcondition, evidence effect,
  and next observation. Duplicate submission is rejected or returns the existing receipt without
  redispatch.

Implementation status:

- [x] The wait-specific semantic channel rejects stale actions and requires a native receipt plus
  fresh next frame. Several modal owners expose native identities and advertised actions.
- [x] R-013 — The proof-bearing player surface is now proved through one native transaction route.
  depends on special-case logs, OCR, fixed keys, and polling for most actions and recovery.
  - Code gap: `handle_action.cpp` emits semantic frames only for wait; popup, activity, and EOC
    traces use separate log grammars; `startup_harness.py` contains extensive screen/log
    classifiers and scenario-specific key paths.
  - Required mechanism: generalize the run-bound native action/modal contract behind `game.act`,
    keep UI mechanics private, and make every accepted or failed transaction return the receipt and
    fresh observation defined above.
  - Proof: one live route performs a player action, handles an ordinary authorized interruption,
    and returns a fresh changed observation. Stale observation, stale handle, unadvertised action,
    unknown modal, unauthorized recovery, missing receipt, missing postcondition, and duplicate
    submission each fail without silent input or retargeting. Raw log, OCR, offset, and key changes
    cannot alter the result.
  - Accepted evidence: task `R-013-closure-046` closed the live-validation gap. Independent run
    `20260826_135902` accepted `world.wait`, `wait.duration_menu`, and `wait.1m`, advanced game time
    from minute `8904` to `8905`, and returned fresh world frame
    `52d5d554c2464c3c9908266abc0c103f:5286318:5`. The report recorded
    `structured_gates_matched`, `feature_proof=true`, and evidence class `feature-path`. Registry
    report `aa8dd106e1d65b1d628334b6688d8ead272626efc38123166c7168716ea80c2c` ingested hard proof
    `6df31ffe649b97c4650fe17e99b53484ae1e45a86340a8efbb6738c7b8cdc31c`. Canonical cleanup
    terminated the owned game process. Focused negative controls keep their focused evidence class.

### 13. Run truth, evidence effects, capability catalog, and reusable gaps

Mechanism:

- Files and symbols: existing report finalization and structured gate evaluation in
  `startup_harness.py`; WEC authority, report ingestion, evidence class, binding, diagnostic
  capsule, certification, and final-gate owners in `scenario_registry_store.py`; cockpit
  `run.status`, `run.finish`, `capability.search/describe`, and `gap.report`.
- Catalog: extend the existing SQLite schema with current capability contract records and
  append-only revisions. Each capability names structured inputs/results, preconditions,
  postconditions, recovery, examples, proof effects, supported scenarios/platforms, and current
  validation evidence. Manifest capability values remain scenario compatibility facts and do not
  become the catalog owner.
- Run state: status returns selected scenario/binding, latest observation/transaction, material
  changes, proof frontier, first divergence, game usability, active causal bound, and valid next
  calls. Continue binds a fresh observation to an evidence-derived causal signal and maximum.
  Finish explicitly finalizes one immutable action/observation report and lets registry ingestion
  derive evidence eligibility; setup, continuation, and recovery never promote earlier evidence.
- Run authority: `run.open` consumes one current valid selection and mints one opaque receipt binding
  its exact scenario revision and bytes, executable bytes, run identity, ownership scope, and derived
  evidence ceiling. It requires no prior proof token. Missing prior verification yields diagnostic or
  zero-credit authority; concrete source/executable drift, conflicting ownership, corrupt identity,
  or unsafe debug setup fails closed. The receipt grants observation authority only. Registry
  ingestion remains the sole proof-promotion owner.
- Evidence firewall: each cockpit result reports `none`, setup-only, diagnostic, focused, or the
  existing bound certification effect. A caller cannot promote the effect. Interventions and
  scenario validation always earn zero gameplay behavior credit.
- Gap record: append one durable record containing blocked intent, missing observation/action/setup/
  recovery/structured failure, direct evidence, reusable outcome, affected scenarios, observed
  cost, run/binding, and disposition. Repeated equivalent reports link to the existing gap instead
  of creating scenario-specific warning text.
- Efficiency: status and observation return deltas after the first full result; unchanged polling
  is suppressed; long actions report event boundaries; focused describe/look calls reveal detail on
  demand. Record token cost, repeated calls, waiting, polling, and manual recovery only when
  observable; otherwise record `unavailable`. Efficiency cannot remove required truth.
- Persistence/compatibility: use append-only SQLite history for catalog revisions, cockpit run
  receipts, interventions, and gaps. Final report artifacts remain immutable source evidence.
  Capability gaps are ordinary product backlog facts and carry no mutation trigger or dispatch
  policy.

Implementation status:

- [x] Existing proof and registry owners already keep evidence authority separate from mutable
  reports and preserve binding/history. Current scenario capabilities are queryable compatibility
  facts.
- [x] R-014 — Cockpit run results, reusable capability knowledge, and gap evidence have no
  unified durable owner or end-to-end prototype proof.
  - Code gap: no capability contract catalog or reusable gap store exists; report/status paths expose
    artifact-heavy detail; setup/action calls cannot state a registry-derived evidence effect; and
    repeated recovery reasoning remains in scripts and scenario-specific instructions.
  - Required mechanism: extend the existing SQLite owner and cockpit façade with the catalog,
    status/finish, gap, evidence-effect, delta, and observed-cost rules above.
  - Proof: the prototype searches SQLite, describes only relevant capabilities, selects and prepares
    a scenario, observes the avatar, performs one action, recovers one ordinary interruption, and
    returns a fresh observation. An absent incidental dog stays simply not visible; a required dog
    uses deterministic setup; one intentionally unsupported action creates one reusable gap that a
    later query retrieves for another affected scenario; a long activity emits boundaries rather
    than per-turn frames; final registry queries preserve setup, focused, certification, and Windows
    evidence separation.
  - Owner-authorized run-authority correction: a current valid selection can open a tracked run
    without proof-token eligibility. The single-use receipt binds scenario revision/source,
    executable, run, and ownership while a derived zero-credit/diagnostic/focused ceiling remains
    unable to promote evidence. Focused controls cover selection replay, conflicting ownership,
    exact finish/reopen, byte drift, and unchanged final-gate eligibility.

### 14. Toggleable cockpit gadgets and seven independent spokes

Research and design basis:

- `tools/openclaw_harness/cockpit.py`, `scenario_registry_store.py`, and the current scenario
  contracts already provide run binding, one-use fresh frames, semantic action advertisement,
  evidence-derived continuation bounds, native receipts, setup intervention history, and explicit
  finish. `src/handle_action.cpp`, popup/EOC/wait input owners, and the focused wait trace confirm
  that native actions and active modal owners—not macro claims—must decide transitions.
- The public observation does not yet prove avatar position, scenario-defined safety-region
  evaluation, danger/damage state, or the absence of an unresolved prompt/activity in one
  authoritative preflight. Existing debug/setup helpers can manufacture fixtures but do not share
  one exact mutation-receipt contract. Every gadget therefore remains blocked on those observable
  facts or receipts rather than assuming a clean start.
- Playwright's actionability and locator contracts re-resolve current state for each action and fail
  ambiguous targets; BehaviorTree.CPP treats asynchronous actions, reactive conditions, and
  transition logs as first-class. See <https://playwright.dev/docs/actionability>,
  <https://playwright.dev/docs/locators>, and
  <https://github.com/BehaviorTree/BehaviorTree.CPP>. The applicable design consequence is a bounded
  local state machine with fresh checks, exact identities, and transaction receipts—not an opaque
  click batch. External patterns are specification evidence only.

Shared mechanism and dependencies:

- Add one run-bound gadget controller behind `CockpitService`, not a second game, scenario, input,
  perception, or proof owner. It exposes master and per-gadget enablement, visible
  disabled/ready/running/stopped/failed state, and immediate semantic stop. With the master or a
  gadget off, the primitive cockpit schema, advertised actions, transitions, and evidence effects
  remain the reference path unchanged.
- Every start consumes a fresh authoritative observation proving scenario/run binding, avatar and
  position, the scenario-defined safety region and absence of visible/detected monsters there,
  absence of danger/damage and unresolved prompt/activity, and the gadget's declared invariants.
  These facts depend on R-010 through R-014's single façade, deterministic setup, avatar perception,
  native transaction, and durable-result owners; missing or stale facts reject execution.
- After every native transaction, re-observe the binding, safety facts, target predicate, and
  progress. Stop on a monster, unknown prompt, operation-owned actor-state change, damage, movement
  outside the declared operation, binding drift, stale frame, missing receipt, no progress,
  evidence-derived bound exhaustion, or any unclassified interruption. Raw mode never interprets or
  handles an interruption; guarded mode handles only explicitly declared-safe cases. Neither falls
  back to the other.
- Return one compact ordered receipt containing gadget/mode and toggle state, preflight frame,
  declared target and bound basis, each native action/receipt/observation boundary, handled flavour
  or prompt classification, game and available wall-time deltas, safety decisions, progress, exact
  terminal reason, and cleanup owner. The receipt inherits only native evidence effects. Setup/debug
  mutations remain zero-credit and cannot prove acquisition, combat, survival, ecology, or natural
  gameplay.
- Composition retains every constituent receipt and stops before the next constituent after the
  first failed precondition or unsafe result. No fixed retry, step, time, output, or performance
  threshold is admissible without a scenario contract, native mechanic, platform contract, or
  measured primitive reference that derives it.
- Each spoke is one outcome-sized task with coherent visible subtasks when its work genuinely
  decomposes. Subtasks expose progress and uncertainty without becoming separate acceptance claims
  or terminal worker windows. One spoke cannot color another.
- A live playtest uses a coordinator charter, worker-controlled native play, compact immutable
  evidence journal, cited witness, mechanical validation, and separate coordinator judgment.
  Mechanical owners retain identity, action correlation, receipts, cleanup, append-only artifacts,
  citations, contradictions, and evidence ceilings. The witness judges causal sufficiency and
  semantic equivalence; it cannot invent facts, hide contradictions, change bindings, or promote
  evidence. Scenario-specific proof matrices, prescribed menu sequences, duplicate colour ledgers,
  exact run-local equality, and paperwork-only reruns receive no acceptance credit.

Implementation status — exactly seven gadget spokes:

<!-- DE67:DFS-SLICE:BEGIN id=R-018-S001 claim=R-018 -->
- [ ] 🔴 R-018 — Raw bounded time passage without interruption handling has no toggleable receipt-bearing route.
  - Proof: from compatible clean starts, a cited witness shows raw time passage reaches its declared
    target through semantically equivalent advertised native waits and the same terminal world state
    as primitive calls while preserving receipts, identity, cleanup, contradictions, and evidence
    ceiling. A prompt, activity requiring interpretation, monster, stale frame, no progress, or bound
    exhaustion stops immediately with partial progress and no guarded handling; off is primitive-only.
<!-- DE67:DFS-SLICE:END id=R-018-S001 claim=R-018 -->
<!-- DE67:DFS-SLICE:BEGIN id=R-019-S001 claim=R-019 -->
- [ ] 🔴 R-019 — Guarded time passage and the first `Keep watch` slice have no classified-safe local loop.
  - Proof: cited witnesses from separately bound safe and unsafe starts show a toggleable `Keep
    watch` recipe handles only declared-safe flavour/prompt interruptions and stops at its target or
    first meaningful event, with semantically equivalent native transitions and terminal state to
    the primitive reference and fewer measured model/tool round trips. Monsters, danger, damage,
    unknown prompts, target crossing, stale/binding drift, no progress, exhausted derived bounds,
    and either off switch fail closed. This slice does not wait for or prove the other spokes.
<!-- DE67:DFS-SLICE:END id=R-019-S001 claim=R-019 -->
<!-- DE67:DFS-SLICE:BEGIN id=R-020-S001 claim=R-020 -->
- [ ] 🔴 R-020 — Controlled camp setup lacks exact mutation, invariant, provenance, and cleanup receipts.
  - Proof: a disposable scenario creates the declared camp composition at exact coordinates through
    the existing setup owner, records every before/after identity and mutation, verifies resulting
    invariants, and cleans up under the named owner. Occupied/unsafe placement, identity drift,
    partial setup, undeclared mutation, or attempted gameplay/economy proof fails without promotion.
<!-- DE67:DFS-SLICE:END id=R-020-S001 claim=R-020 -->
<!-- DE67:DFS-SLICE:BEGIN id=R-021-S001 claim=R-021 -->
- [ ] 🔴 R-021 — Exact-identity debug creature HP-to-zero lacks a non-ambiguous zero-credit transaction.
  - Proof: a disposable scenario selects one exact creature handle/identity and coordinate, records
    before/after HP and cause, sets that identity to zero, and records caller authority and cleanup.
    Nearby/name-only ambiguity, stale identity, avatar or operation-owned ecology target, incidental
    death, partial mutation, and any combat/survival/ecology/natural credit fail.
<!-- DE67:DFS-SLICE:END id=R-021-S001 claim=R-021 -->
<!-- DE67:DFS-SLICE:BEGIN id=R-022-S001 claim=R-022 -->
- [x] R-022 — Exact item spawning has deterministic identity, destination, provenance, and cleanup receipts.
  - Accepted evidence: closure task `R-022-closure-004`, bound zero-credit report
    `58a34dcb…14c5f6`, and verification `b338f94c…487d60` created exactly three separately tagged
    apples with ordinals 0–2 at offset `[4,0,0]` for owner `your_followers`, preserved declared item
    state and provenance, removed exactly those three items, retained none, and completed canonical
    ingestion with green structured gates.
  - Evidence ceiling: the accepted transaction is setup support only. It grants no acquisition,
    economy, survival, ecology, natural-gameplay, continuous-certification, or Windows-feel credit.
<!-- DE67:DFS-SLICE:END id=R-022-S001 claim=R-022 -->
<!-- DE67:DFS-SLICE:BEGIN id=R-023-S001 claim=R-023 -->
- [ ] 🔴 R-023 — Relative and destination movement lack distinct raw and guarded bounded routes.
  - Proof: signed `+x`/`-x`/`+y`/`-y` offsets and a declared destination produce exact step,
    position, terrain, and terminal receipts from fresh frames. Raw stops at the first unhandled
    interruption; guarded rechecks creatures, terrain, prompts, damage, displacement, blockage, and
    target progress. Wrong/stale binding, unexpected movement, blocked step, unknown event, no
    progress, derived-bound exhaustion, and off switches return exact partial progress.
<!-- DE67:DFS-SLICE:END id=R-023-S001 claim=R-023 -->
<!-- DE67:DFS-SLICE:BEGIN id=R-024-S001 claim=R-024 -->
- [ ] 🔴 R-024 — A compact LLM-first cockpit TUI with structured local/overmap awareness has no real route.
  - Proof: deterministic state renders stable command/field IDs, current binding/frame, toggles,
    legal raw/guarded operations, safety gates, mission/target/progress, exact stop reason, receipt
    drill-down, a bounded avatar-centred semantic local map, and a bounded provenance/recency-labelled
    overmap. `KEEP WATCH`, `MAKE CAMP`, `STOCK UP`, `ZAP`, `MOVE OUT`, `EYES UP`, and `BIG MAP` are
    aliases only; every action has an exact non-interactive equivalent and contract view. Tests cover
    keyboard/command parity, clipping/coordinates/unknown cells, stale/error state, large-observation
    bounds derived from measured mission needs, and structured/visual parity. A controlled recipe
    matches direct cockpit receipts and terminal state; real roster-worker comparisons preserve
    model/effort and derive any efficiency gate from the primitive reference. Screenshots, ANSI,
    glyphs, prose, hidden coaching, or knowledge outside authoritative observation cannot decide.
<!-- DE67:DFS-SLICE:END id=R-024-S001 claim=R-024 -->

### 15. Reviewable integration and checkpoint boundary

Mechanism:

- Preserve the current dirty frontier as user-owned work. Classify tracked source, tests, fixtures,
  scenarios, guidance, and proof-owner changes by the accepted or active outcome they implement,
  including shared files that support several claims.
- Create reviewable behavior, probe, and ledger/document checkpoints at real state boundaries under
  `COMMIT_POLICY.md`. A checkpoint must name its changed file class, narrow gate already run, retained
  evidence references, and any remaining active or red work. It does not rerun settled gameplay or
  broaden an evidence class merely to make a commit.
- Classify generated logs, screenshots, run directories, registry databases, and saved fixtures by
  durable evidence value and owner. Preserve authentic immutable evidence and the references needed
  to retrieve it. Retire only copies proved redundant, derived, superseded, or safely reproducible;
  repository tidiness is not an acceptance gate.
- Remaining dirt is permitted when it belongs to named active work and is bounded enough that its
  owner and proof status are legible. A globally clean tree, an arbitrary artifact count, or deleting
  large files is neither required nor sufficient.

Implementation status:

<!-- DE67:DFS-SLICE:BEGIN id=R-025-S001 claim=R-025 -->
- [ ] 🔴 R-025 — The preserved Phase-3 frontier lacks a reviewable source-control checkpoint and
  artifact-retention boundary.
  - Current evidence: the dev worktree contains 56 tracked status entries spanning source, tests,
    harness, scenarios, guidance, and ledgers, with approximately 17,984 added and 878 removed lines;
    it also exposes 906 untracked status entries, 1,684 build-log files, and 688 runtime run
    directories. Several durable claim acceptances exist only inside this combined frontier.
  - Required mechanism: partition completed valid work into the smallest coherent checkpoints the
    existing diff supports, retain active work without rewriting bystanders, and record where
    authentic run evidence remains. Do not delete evidence, manufacture a clean tree, or replay a
    settled claim solely to improve packaging.
  - Proof: each checkpoint maps its files and existing validation to accepted or active claims;
    accepted source/tests and immutable evidence references remain intact; generated artifacts have
    an explicit retain, superseded, reproducible, or active-run disposition; and every remaining
    dirty entry is attributable to named unfinished work rather than an unbounded mixed frontier.
<!-- DE67:DFS-SLICE:END id=R-025-S001 claim=R-025 -->

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
| Cockpit call/result | Worker and cockpit clients | Separate registry CLI, startup CLI, semantic helpers, subprocess output | `CockpitService` is the single public façade and owns only request validation/result composition. Existing component functions retain state authority. Internal tokens, offsets, keys, OCR, logs, and subprocess output never become public success facts. |
| Scenario source and searchable facts | Cockpit scenario calls, canonical launcher, reports | Scenario JSON files, registry rebuild/reconcile, caller prose | Canonical validated scenario bytes remain source declarations. SQLite is the searchable current projection and history owner. Selection revalidates both. Caller prose cannot add facts. |
| Scenario selection and run opening | Worker strategy and selected brief | Pure query evaluator, proof-token issuer, cockpit run authority, canonical launcher | The worker owns fit judgment and may open a tracked run from one current valid selection. The run receipt is single-use and source/executable/ownership-bound; its derived evidence ceiling is separate from proof-token eligibility and registry promotion. |
| Setup state and interventions | Scenario brief, observation adapter, report classifier | Fixture install/transforms, native debug setup, incidental world state, scenario macros | Loaded-save state or a confirmed explicit intervention wins. Required preconditions never come from incidental entities. Every intervention and macro constituent is recorded and receives no manufactured-state or behavior credit. |
| Avatar-visible world | Cockpit observation/look and player UI | `map`, avatar/Creature visibility, creature tracker, fields/items/light/weather/time; LLM NPC snapshot; debug ecology view; OCR/screens/logs | Native game state and avatar perception alone supply cockpit visible facts. The cockpit adapter is a read-only projection. LLM-intent remains a separate protected consumer. Debug/offscreen projections and visual/log reconstruction cannot write cockpit perception. |
| LLM-intent request and NPC behavior | NPC LLM manager, NPC state, tests | Cockpit adapter or shared-helper refactor | Keep `llm_intent` as sole owner. Cockpit code must yield completely. A neutral shared primitive is admissible only after direct unchanged-output and unchanged-behavior proof; otherwise use the harness-only adapter. |
| Run-scoped entity/location handles | Cockpit observation and action validation | Request letter maps, screen markers, coordinates, names, object pointers after reload | The cockpit handle table alone owns opaque worker handles. Durable identity may rebind under the exact binding; unproved process-local identity becomes stale. Markers/names/coordinates never retarget a stale handle. |
| Player action dispatch | Cockpit transaction and game result | Worker strategy, scenario key scripts, direct state transforms, `input_context`, `game::handle_action` | The worker selects intent. Existing native input/action code alone performs proof-bearing gameplay. Cockpit translation is private and must yield to current modal/input authority. Direct state transforms are setup interventions only. |
| Modal and recovery state | Cockpit observation/action and player UI | Popup, uilist, activity-distraction, EOC, OCR/log classifiers, fixed-key handlers | The currently active native modal/input owner advertises identity/actions and decides acceptance. Recovery may use only the selected safe policy. Existing separate trace readers merge into this contract, then retire as decision owners. |
| Capability contract | Capability search/describe, scenario compatibility, gap triage | Manifest capability values, manuals, scenario prose, repeated worker reasoning | SQLite catalog revisions own reusable contracts/examples/recovery/proof effects. Manifest values remain per-scenario compatibility facts. Manuals and prose cannot silently create capability truth. |
| Capability gap | Gap queries and later improvement work | Ad hoc warnings, mutation suggestions, repeated scenario instructions | Append-only SQLite gap history owns reusable missing-interface evidence. Equivalent gaps link. Gap creation carries no code mutation, scheduling, or proof authority. |
| Cockpit evidence effect | Worker, run status/finish, final-gate query | Setup helpers, screenshot/OCR classifiers, report labels, caller assertions | Existing report/registry evidence authority wins. Cockpit calls report the derived effect but cannot promote it. Setup and validation stay zero-credit; certification and Windows gates keep their existing owners. |

The ownership transfer is a compare-and-swap on activity ID, generation, owner, handoff epoch, and
last-advance cursor plus a complete actor set. The destination cannot advance before acknowledgement
and durable persistence. The losing owner yields without editing the winner. Duplicate identical
receipts are no-ops; stale or conflicting receipts are explicit failures. Aggregate population or
resource state never substitutes for a named actor receipt.

### Keep, merge, and retire decisions

| Existing mechanism | Decision | Code-grounded reason and boundary |
|---|---|---|
| `scenario_registry.py`, `scenario_registry_store.py`, and canonical registry launch | **Keep** | They already own typed declarations, SQLite projection, binding/evidence/lifecycle history, fail-closed queries, single-use launch authority, and final-gate eligibility. Extend their schema and expose summaries through the cockpit; do not create another scenario database. |
| Fixture capture/install, validated save binding, and native debug setup helpers | **Keep and wrap** | They are the smallest existing setup mechanisms. Wrap them in deterministic target resolution and intervention receipts. They remain setup-only and may not prove manufactured behavior. |
| `SemanticStepChannel` frame/session/stale-action/native-receipt/fresh-frame rules | **Keep and merge** | The prototype has the correct transaction invariants. Move those invariants behind `game.act`; do not preserve debug-log parsing, PIDs, offsets, or physical bindings as worker concerns. |
| Native `input_context`, `game::handle_action`, popup/menu/activity owners | **Keep** | They remain the only proof-bearing player dispatch and modal acceptance owners. The cockpit must dispatch through them and observe their receipts, not call gameplay mutators directly. |
| `src/llm_intent.cpp` NPC snapshot, target map, request/response, parser, timing, and behavior | **Keep unchanged** | It is a protected regression boundary and a perception pattern only. The cockpit reads neutral lower-level game state through a harness-only adapter. It does not call, intercept, or replace LLM-intent. |
| A neutral lower-level observation primitive extracted from existing game-state reads | **Conditional merge only** | Extraction is allowed only when neutral code owns the primitive and direct regression proof shows unchanged LLM-intent snapshot output, target behavior, action behavior, and timing. The smallest current design does not require extraction. |
| `startup_harness.py` structured gate evaluation, report finalization, and registry WEC authority | **Keep** | These are the existing proof firewall. Cockpit results expose their derived evidence effect and cannot introduce a competing classifier. |
| OCR, screen text, raw debug logs, registry tokens, frame offsets, PIDs, and key bindings as public gameplay state or success inputs | **Retire from the worker interface** | Current code uses them for legacy mechanics and diagnostics. They may remain internal diagnostics until replaced, but they cannot decide cockpit observation/action success or be parsed by the worker. |
| Separate debug-log grammars and special-case recovery classifiers as modal decision owners | **Merge, then retire** | Popup, EOC, activity, and wait paths currently duplicate current-state inference. The native active modal/input owner must publish one identity/action/postcondition contract. Legacy classifiers may remain diagnostic-only. |
| Planned `src/openclaw_harness.*` perception/manager and `src/openclaw_ui_adapter.*` universal controller in `doc/OPENCLAW_HARNESS.md` | **Retire as an ownership plan** | A second perception/scenario/action owner would compete with the registry, native game state, input owners, and proof classifier. Implement only the narrow harness-gated observation adapter and stateless Python façade named in this DFS. |
| `ecology_debug_view` and observer/incident surfaces | **Keep separate** | They expose offscreen debug truth for diagnosis. They must never populate avatar perception or silently satisfy a cockpit gameplay claim. |
| Scenario-specific macros and repeated manual instructions | **Retire as gameplay controllers** | Macros remain allowed only for setup/recovery with constituent receipts. Repeated missing capability becomes a durable gap and reusable catalog work. |

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
| `R-007` | Vary or remove every incidental visual/OCR input while replaying semantic completion and interruption traces. | Bound semantic events, broker decisions, postconditions, handler-registry entries, inverse replays, and zero-artifact ordinary-play evidence. | OCR, screen phrases, guessed keys, stale UI identity, unrelated transient facts, escaped paths, unbounded output, or progress-free recovery cannot affect a machine verdict. |
| `R-008` | Qualify natural bandit and cannibal lifecycles and save/relaunch at each authoritative-owner boundary. | Independently classed focused reports with actor/owner continuity, crossing receipts, and saved-state normalization. | Synthetic ecology edits, behavior-changing stabilizers, aggregate populations, replacement identities, rollback, resume, or segment union receive no lifecycle or certification credit. |
| `R-009` | Observe integrated waits and run source-bound semantic-harness witnesses on macOS, Linux/WSL, and Windows. | Game-time/transition/resource samples plus per-platform build/runtime bindings and direct route results. | Wall time alone, invented zero metrics, repeated-event floods, incidental HUD/OCR text, or one platform standing in for another cannot pass. |
| `R-010` | Use one compact frontier to search and describe only the needed capability/scenario through one cockpit. | Public call/result transcripts and schema tests. | Public token, PID, offset, key, OCR, log, subprocess text, full manual, raw history, or unsupported caller field fails or is absent. |
| `R-011` | Search/select an existing scenario, create/validate a no-fit scenario, and deterministically prepare its required dog. | SQLite scenario/selection/validation history, exact source/binding, and intervention receipts marked zero-credit. | Incidental dog waiting, occupied/stale placement, unrecorded macro, scenario validation as gameplay proof, or manufactured behavior credit fails. |
| `R-012` | Observe and look from the avatar while moving one visible entity, hiding another, invalidating a stale handle, and relaunching. | Native observation frames, visibility facts, handle lifecycle, deltas, and LLM-intent regression artifacts. | Offscreen/debug truth, OCR/log/registry facts, name/marker/coordinate retargeting, or changed LLM prompt/snapshot/target/action/timing behavior fails. |
| `R-013` | Execute one player intent through native dispatch, handle one authorized interruption, and receive a confirmed postcondition plus fresh observation. | Issuing observation, action/target, native receipt, recovery receipts, postcondition, evidence effect, and next observation. | Direct state mutation, guessed key, stale frame/handle, unknown modal, unauthorized recovery, missing receipt/postcondition, replay, or silent feature failure fails. |
| `R-014` | Select a valid scenario, open a tracked run regardless of prior-proof eligibility, complete the prototype cockpit route, report and retrieve one reusable gap, and finish. | Source/executable/ownership-bound open receipt, derived evidence ceiling, SQLite catalog/gap/run history, compact deltas, final immutable report, and registry eligibility result. | Selection replay, conflicting ownership, source/executable drift, false proof promotion, duplicated warning, invented cost, per-turn stream, or lost hostile-ecology proof fails. |
| `R-015` | Persist a focused lifecycle row at its first declared causal boundary, then exercise a separately declared later-boundary row. | Per-transaction boundary decisions, the first matching event/state, immediate persistence receipt, and independently classed later-boundary evidence. | A queued progression action after the matched boundary, a mismatched event, or post-hoc reinterpretation of later state cannot satisfy the earlier row. |
| `R-018` | Run raw bounded time passage from a proved clean start and compare it with primitive waits. | Cited immutable journal with ordered native wait/frame receipts, derived bound, partial progress, exact stop reason, cleanup, and primitive equivalence judgment. | Interruption interpretation, hidden batching, stale/missing receipt, unsafe continuation, invented bound, concealed contradiction, false promotion, or off-state mutation fails. |
| `R-019` | Run toggleable guarded `Keep watch` through safe flavour interruptions to its target or first meaningful event. | Separately bound safe/unsafe journals with fresh safety frames, classifications, native receipts, terminal-state and measured-round-trip comparison, cited witnesses, and coordinator judgments. | Mixed start authority, unknown/unsafe handling, monster/danger/damage continuation, target crossing, binding drift, no progress, exhausted bound, concealed contradiction, or off-state mutation fails. |
| `R-020` | Create and clean one exact controlled camp fixture. | Before/after identities, coordinates, mutation/invariant receipts, setup-only evidence effect, and cleanup. | Unsafe/occupied target, partial/undeclared setup, identity drift, lost cleanup, or gameplay/economy promotion fails. |
| `R-021` | Set one exact disposable-scenario creature identity's HP to zero. | Exact identity/position, before/after HP, cause, authority, zero-credit effect, and cleanup. | Proximity/name ambiguity, stale identity, protected target, incidental death, or gameplay/ecology credit fails. |
| `R-022` | Spawn and clean one exact declared item fixture. | Type/quantity/charges/condition/ownership/destination identities and mutation/cleanup receipts. | Substitution, duplicate/partial spawn, destination or ownership drift, unreceipted mutation, or acquisition/economy credit fails. |
| `R-023` | Exercise raw and guarded signed-offset and destination movement from identical starts. | Per-step frames/receipts, safety and progress decisions, exact terminal position/state, partial-progress results, and primitive comparison. | Silent mode fallback, ambiguous target, unsafe/unclassified continuation, blocked/no-progress loop, invented bound, or off-state mutation fails. |
| `R-024` | Operate one controlled recipe through the LLM-first TUI and direct semantic cockpit. | Deterministic structured render, command parity, local/overmap provenance, native receipt equivalence, terminal-state match, and blinded worker comparisons. | ANSI/screenshot/glyph/prose authority, stale/unknown knowledge, hidden coaching, fake controls, unproved threshold, or evidence promotion fails. |
| `R-025` | Partition the preserved Phase-3 frontier at real implementation and evidence boundaries. | Reviewable checkpoint mapping, existing narrow validation references, retained immutable evidence pointers, and explicit ownership for remaining active dirt. | A clean-tree assertion, artifact-count target, deletion without retention proof, replay of settled gameplay, mixed unrelated commit, or evidence promotion fails. |

## Freeze record

- Status: `Refrozen`
- Frozen source baseline: remote `dev` at
  `54a60872e2b8dcabb00288e8fcfe6b976b82fd99`, tracking `origin/dev`, with the
  intentionally broad dirty frontier and inspected-source fingerprint
  `98e30ed1400ad496ce21a4b140fea69b8cbe76a08977176ff9eb641846043402`.
- User-owned choices: preserve useful focused proof; diagnostic capsules are recommendations and
  replays earn zero certification credit; final automation is one continuous bound round; normal
  save/quit/relaunch is permitted within that round; binding changes, rollback, splicing, and
  replacement identities invalidate final credit; durable identity and exactly one owner cross
  overmap/local/persistence boundaries; automated certification and Windows feel are independent;
  one compact cockpit owns the worker interface; required entities use deterministic recorded setup;
  avatar observation exposes only perceivable state; worker strategy remains separate from harness
  UI/input/recovery; reusable capability gaps are ordinary improvement facts; and existing
  LLM-intent observable behavior is a protected regression boundary.
- Evidence-implied refinements: the inspected code supports durable outing state as authority and
  local NPCs as temporary cursor/epoch-bound projections; retain its candidate/rollback transaction,
  require persistence-confirmed symmetric receipts, and reconcile or reject projection conflicts at
  load. Aggregate population evidence remains ineligible for named actor claims.
- Owner-authorized same-outcome expansion: R-007 through R-009 restore only the unfinished
  improved-harness qualification tail. They replace incidental/OCR proof with semantic observation,
  qualify natural bandit and cannibal lifecycles plus persistence boundaries, add integrated
  wait/resource observation and macOS/Linux-or-WSL/Windows technical witnesses, and award no
  continuous-certification or Windows-feel credit. R-005 and R-006 remain the two final gates.
- Owner-authorized cockpit reconciliation: R-010 through R-014 add the progressive cockpit,
  SQLite-backed scenario lifecycle, deterministic intervention receipts, avatar-centred native
  observation and stable handles, proof-bearing player transactions, capability catalog, compact
  run results, worker-owned live continuation and finish, and durable reusable gaps. Scenarios set
  authority and proof boundaries but fixed observation windows do not terminate progressing play.
  A current valid selection may open a tracked source/executable/ownership-bound run without prior
  proof eligibility; the derived evidence ceiling cannot promote its result.
  These requirements reuse existing registry, native game, input/modal, and proof owners. They retire
  competing public log/OCR/offset/key reconstruction and the planned universal parallel harness
  perception/controller without coupling to the protected LLM-intent path. Existing hostile-ecology
  R-005, R-006, R-008, and R-009 remain binding.
- Owner-authorized additive expansion: R-018 through R-024 are exactly seven independently accepted gadget spokes over one
  proof-preserving controller foundation. The first product slice is guarded `Keep watch`. Debug and
  setup spokes remain zero-credit, convenience cannot satisfy hostile-ecology proof, and all prior
  red claims, accepted work, final automation, and Windows-feel gates remain unchanged.
- Owner-authorized trajectory reconciliation: R-005 now names the first still-unproved natural-route
  precondition instead of a disproved final-handoff repair, R-022 reflects its durable zero-credit
  acceptance, and R-025 makes the already-required source-control and artifact-retention boundary an
  explicit delivery outcome without treating cleanliness, deletion, or replay as proof.
- Remaining design uncertainty: the current cockpit observation/setup surfaces do not yet expose
  every safe-start fact or exact setup receipt. Each affected gadget spoke stays red until those
  authoritative surfaces exist and the named counterexamples pass.

After this refreeze, automation may only close existing red items after named proof and remove their red
markers; make an evidence-implied nonmaterial clarification; or append a uniquely implied
same-contract mechanism, ownership/proof detail, and necessary new stable red claim after a verified
phase-3 worker finding. Existing claim identities, text, status, accepted work, and acceptance
strength remain fixed. Refreeze immediately. Product intent, project language, permissions,
user-visible behavior, balance, and materially different design choices remain user-owned.
