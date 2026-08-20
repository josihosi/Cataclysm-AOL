# Trustworthy Hostile-Ecology Harness Checkpoint Chains DE-67 Functional Specification

Status: Frozen
WEC: `.de67/WEC.md`
Source baseline: `dev@7f4697ee6b17fb897461e3ceb290342b83787a30`; preserved dirty
hostile-ecology and harness work listed in the freeze record; inspected 2026-08-20.

## Document authority

This document is the mechanistic product contract derived from the user-owned WEC and the inspected
production code. It is not a task-dispatch plan. If this document conflicts with current code about
what the code does, re-inspect the code. If it conflicts with the WEC about what the product should
do, the WEC and the user win.

Status markers:

- `[x]` — present in the production path with proportionate evidence.
- `[ ] 🔴 R-...` — missing, wrong, or unproved; the stable red item is implementation work.

The phase-3 work ledger may project the red items in this document. It may not rename, split,
weaken, reorder, or replace them except under the freeze rule at the end of this document.

## Functional contract

For current bandit and cannibal scenarios, an agent can ask the registry for a scenario, inspect the
selection, and launch it through the canonical harness entry point. Before the game process starts,
the harness validates that the scenario has a coherent, observable proof route and that the
installed save has the required stabilizer state. During execution, the harness evaluates named
causal proof gates, records structured transition evidence, and captures one durable checkpoint
after every successful gate. Supporting input remains operational evidence and never becomes a
proof obligation merely because it lacks an immediate artifact.

After an interruption or failed expectation, the report identifies the earliest causal divergence,
the gates already completed, and the latest valid checkpoint. The harness recommends that
checkpoint but does not choose for the agent. An explicit resume restores the selected checkpoint
into a disposable run profile and continues the same bound chain. A contiguous, binding-valid chain
whose ordered gates satisfy the scenario's final proof route is a certification chain. A relevant
code, data, harness, executable, fixture, profile, or scenario change invalidates the entire chain.
A changed binding never prevents a new trusted rerun.

```text
registry query
  -> explicit selection token
  -> contract preflight and installed-save preflight
  -> diagnostic or certification segment
  -> successful proof gate
  -> atomic checkpoint and registry receipt
  -> explicit resume or continued segment
  -> contiguous chain verification
  -> authoritative diagnostic report or certification-chain verdict
```

The smallest useful vertical slice is the existing
`bandit.scout_to_decision_observer_live_mcw` route: preflight it before launch, execute it to the
first named causal gate, save and capture a bound checkpoint, fail or interrupt later without losing
that checkpoint, resume it explicitly, and report the same chain's first divergence or final
certification. The same mechanisms must then prove the existing
`cannibal.live_world_night_local_contact_pack_mcw` route so the implementation is not accidentally
bandit-specific.

## Project language and terminology

The implementation, CLI, report, documentation, and tests use these terms exactly:

- **Contract preflight** — validation completed before the game process is launched.
- **Proof gate** — a named, causally meaningful contract boundary.
- **Checkpoint** — captured save state and evidence after a successful proof gate.
- **Checkpoint chain** — ordered segments joined by verified checkpoint lineage.
- **Diagnostic run** — execution intended to locate and explain a divergence.
- **Certification chain** — verified segments that collectively satisfy final proof.
- **First divergence** — the earliest failed causal expectation in proof-route order.
- **Structured transition event** — machine-readable evidence emitted when product or harness state
  changes.
- **Incidental-hostile suppression** — the non-combat harness facility; do not call it autokill in
  user-facing output.
- **Actor receipt** — evidence identifying every entity considered and every entity affected by
  suppression.

`press`, `type`, `wait`, and save/quit/relaunch are supporting actions. They may fail operationally
and thereby block the next proof gate, but successful delivery without a screen or metadata change
is not yellow proof. A log substring is diagnostic context, not a product transition fact.

Writhing-stalker behavior, zombie-rider behavior, and production perception hardening are outside
this specification. No exception to the standard observer traits is permitted for writhing
stalkers in this round.

## Current code map

| Concern | Files and symbols | Current production behavior | Evidence |
|---|---|---|---|
| Canonical agent entry point | `.agents/skills/caol-harness/SKILL.md` | Query, explicit registry launch, and report inspection use the registry CLI, but checkpoint selection and resume do not exist. | Inspected skill contract. |
| Scenario contract | `tools/openclaw_harness/scenario_registry.py :: MANIFEST_VERSION`, `_validate_proof_route`, `normalize_relation_contract` | Manifest version 1 relates capability gates to primitive step labels. It checks references but not causal gate ordering, checkpoint safety, or final observability. | `MANIFEST_VERSION = 1`; current validator. |
| Scenario under motivating failure | `tools/openclaw_harness/scenarios/bandit.scout_to_decision_observer_live_mcw.json` | A long list of input, wait, audit, and save steps is treated as one run. The fixture has inherited `DEBUG_CLAIRVOYANCE` and added `DEBUG_NOTEMP`, not the full WEC stabilizer policy. | Manifest and fixture manifest inspection. |
| Primitive runner | `tools/openclaw_harness/startup_harness.py :: execute_probe_steps`, `build_probe_step_ledger`, `probe_proof_classification` | One monolithic loop assigns proof colors to primitive steps. Successful transport with no immediate artifact becomes yellow. Only one declared post-relaunch boundary is supported. | Current functions at lines 15664, 10058, 18426, and 15576. |
| Runtime binding | `startup_harness.py :: RUNTIME_RELEVANT_PATHS`, `runtime_source_binding`, `build_runtime_binding`, `compare_runtime_binding` | Executable and selected source/data paths are bound, but `tools/openclaw_harness` is absent from the relevant source set. | Current tuple and binding functions. |
| Save capture | `startup_harness.py :: snapshot_world_state`, `finalize_probe_report` | A world copy can be captured at finalization. There is no gate checkpoint manifest, parent lineage, atomic publication, or general restore path. | Current functions at lines 19238 and 19262. |
| Report | `startup_harness.py :: finalize_scenario_report`, `compact_probe_report_for_stdout` | The full report embeds extensive step evidence; compact output truncates step arrays. It does not lead with a structured first divergence or resume recommendation. | Current finalizer and compact formatter. |
| Registry authority | `tools/openclaw_harness/scenario_registry_store.py :: reload_selection_token_for_launch`, `ingest_report_reference`, `ingest_token_linked_report_reference` | Token launch revalidates current manifest/runtime evidence. A second different report for one token is invalidated as `multiple_report_runs`; there is no checkpoint-chain history. | Current ingestion path, including `multiple_report_runs`. |
| Structured product transition seam | `src/bandit_live_world_probe.h/.cpp :: collection_mode::transition_events`, `transition_event`, `record_transition_event` | Focused in-process tests can collect committed transition events. The collection is session-local and bounded; live harness runs do not receive a durable structured stream. | Existing unit probe and calls in `bandit_live_world.cpp`. |
| Central hostile-ecology transitions | `src/bandit_live_world.cpp :: commit_local_pair_handoff`, `commit_local_pair_dematerialization`, `transition_active_scout_phase_impl`, `transition_hostile_operation_phase`, `accept_current_scout_report_for_assessment`, `transition_camp_decision_state`, `release_structural_outing_reservation`, `advance_structural_bounty_outings`, `apply_dispatch_plan` | These functions own durable state changes and already call or can call the structured transition seam. | Inspected call paths at lines 3988, 4228, 5684, 5779, 5944, 6008, 12360, 14422, and 16730. |
| Installed-save audit | `startup_harness.py :: apply_player_mutations_transform`, `audit_saved_player_condition` | Fixture transforms can add traits and the audit reads the actual compressed save, but this audit is an ordinary scenario step rather than mandatory pre-launch policy. | Current transform/audit and fixture tests. |
| Non-death removal primitive | `src/game.cpp :: game::remove_zombie`, `src/creature_tracker.cpp :: creature_tracker::remove` | A loaded monster can be removed from the tracker without invoking `monster::die` and its ordinary death effects. No scenario-scoped eligibility, ecology exclusion, or receipt facility exists. | Current removal path. |
| Motivating observed run | `.userdata/dev-harness/harness_runs/20260818_160832/probe.report.json` and its referenced artifact log | The run lasted about 18 minutes, ended at homeward materialization because the loaded bubble lacked paired entry or staging positions, colored 18 primitive steps yellow, and repeated identical diagnostics thousands of times. | Direct report and artifact counts from the preserved run. |

## Mechanistic requirements

### 1. Contract preflight and proof-gate declarations

Mechanism:

- Files and symbols: `scenario_registry.py`, `startup_harness.py`, scenario manifests, fixture
  manifests, and their existing unit/corpus tests.
- Entry point: every registry-backed diagnostic or certification launch, after fixture installation
  and before `launch_game`.
- Manifest compatibility: retain version-1 loading for unaffected scenarios. A scenario that opts
  into checkpoint-chain certification uses a versioned contract that declares `run_class`,
  `observer_character`, ordered `proof_gates`, and a final `proof_route` over gate IDs. It does not
  silently reinterpret version-1 primitive labels as checkpoint gates.
- Proof-gate declaration: each gate has a stable ID, a human label, one ordered boundary after a
  known scenario step, causal expectations backed by structured events or saved-state artifacts,
  predecessor requirements, and an observable checkpoint-safe UI state. Capability gates refer to
  proof-gate IDs, not to transport-step labels.
- Static preflight rejects duplicate or unknown IDs, missing or out-of-order predecessors, route
  gaps, cycles, a gate whose only evidence is input/wait delivery, a checkpoint boundary without an
  observable safe UI state, and final proof without terminal observability.
- Installed-save preflight reads the transformed player save through the existing save audit. Every
  current scenario requires `DEBUG_LS` and `DEBUG_NOTEMP`. A non-combat scenario additionally
  requires `DEBUG_STAMINA` and `DEBUG_CARDIO`. An observer character additionally requires standard
  `DEBUG_CLAIRVOYANCE` and `DEBUG_NIGHTVISION`. The preflight reports requested, observed, missing,
  and forbidden traits and launches no process when the policy is false.
- A manifest declares `run_class` as `combat` or `non_combat`; it does not infer the class from the
  filename. `observer_character` is an explicit boolean. These fields are authoritative for trait
  policy and suppression eligibility.
- The two named vertical-slice scenarios declare causally meaningful gates and receive the exact
  required fixture traits. Fixture work may establish only stabilizer and observer footing; it may
  not inject a product transition for proof credit.
- Contract-preflight failure produces a compact, machine-readable report and no run directory that
  claims execution.

Implementation status:

- [x] Version-1 schema validation, fixture transforms, and read-only saved-player trait audit exist.
- [ ] 🔴 R-001 — Versioned proof-gate contracts and mandatory pre-launch validation are absent.
  - Code gap: version 1 binds proof to primitive labels; trait audits run only when a scenario step
    asks for them; the current bandit fixture lacks required stabilizers.
  - Required mechanism: add the compatible versioned declarations and validator above, run static
    and installed-save preflight before `launch_game`, and migrate the named bandit and cannibal
    vertical slices without granting fixture-produced product credit.
  - Proof: invalid contracts and missing traits fail before child-process launch; valid installed
    saves report the exact policy; both named manifests pass corpus validation.

### 2. Structured transition evidence and causal gate evaluation

Mechanism:

- Files and symbols: `bandit_live_world_probe.h/.cpp`, the central transition owners in
  `bandit_live_world.cpp`, `do_turn.cpp` only where it owns a handoff, and the harness event reader.
- Entry point: a harness child process receives a run-owned structured-event path in its existing
  child environment. The product opens that path only when the harness explicitly enables it.
- The existing transition-event seam becomes a live append-only JSON Lines stream. The live stream
  is not subject to the in-memory test session's bounded collection. Each committed event includes
  a schema version, monotonic sequence, product game time, transition owner/domain, faction/site,
  operation ID, generation/epoch when applicable, previous state, new state, reason, stable actor
  identities when applicable, and a run correlation ID. Fields not applicable to a transition are
  explicitly absent; they are not guessed from log text.
- Every central state writer needed by a declared proof gate emits after the durable transition has
  committed. Rejected or no-op transitions emit a typed rejection/diagnostic event and cannot
  satisfy a committed-transition expectation.
- The harness incrementally validates sequence and run correlation and records the byte/event
  watermark used by each gate. A malformed or truncated record is a causal diagnostic and cannot
  be proof.
- A proof gate evaluates declared predicates over events and referenced saved-state artifacts since
  its predecessor watermark. It reports expected facts, observed facts, and exact event/artifact
  references. Log substrings may be attached as context only.
- Raw structured evidence is stored once. Reports reference event ranges and summarize repeated
  semantic diagnostics by identity, state/reason, count, first occurrence, and last occurrence.

Implementation status:

- [x] A focused, committed-only in-process transition-event recorder exists and has unit coverage.
- [ ] 🔴 R-002 — Live runs do not expose a durable structured transition stream, and current proof
  still depends on log-substring archaeology.
  - Code gap: the existing recorder is session-local and the central transition coverage is not
    complete for the named proof routes.
  - Required mechanism: extend the existing seam to the explicit run-owned stream, instrument the
    central writers needed by both named scenarios, and evaluate gates from typed events and saved
    artifacts.
  - Proof: committed transitions satisfy matching gates; rejection/no-op events and identical text
    logs do not; bandit handoff/return/decision and cannibal dispatch/contact retain stable actor and
    operation identity across their event ranges.

### 3. Gate execution, checkpoint capture, and first divergence

Mechanism:

- Files and symbols: `startup_harness.py :: execute_probe_steps`, save/writeback audits,
  `snapshot_world_state`, report finalizers, and new gate/segment/checkpoint helpers in the same
  canonical harness.
- Entry point: after contract preflight, the chain executor partitions the existing ordered scenario
  steps at declared proof-gate boundaries. `execute_probe_steps` remains the primitive segment
  executor; it is not a second proof owner.
- Press/type/wait/relaunch details are recorded as supporting action diagnostics. Only named gate
  predicates appear in the proof ledger. A supporting action failure becomes the observed cause of
  the next unmet gate; a supporting action success without immediate artifact is not yellow.
- When a gate predicate passes and its declared safe UI state is observed, the harness invokes the
  existing guarded save-and-quit input path, waits for normal child exit, and proves save writeback.
  Those internal inputs remain supporting actions.
- The harness copies the complete disposable world and gate evidence into a temporary directory in
  the current run, writes a checkpoint manifest, validates every referenced hash, and atomically
  publishes the checkpoint directory. A partial temporary directory is never a checkpoint.
- The checkpoint manifest binds chain ID, checkpoint ID, predecessor checkpoint hash, gate ID and
  order, selection token, scenario and manifest hash, fixture and installed-world hash, profile
  contract, executable/runtime/source/data/harness binding, run options, world snapshot hash,
  evidence/event range hashes, product game time, and creation outcome.
- After publication the executor may relaunch that just-saved disposable world for the next segment.
  A final gate publishes its checkpoint before certification finalization.
- On interruption, timeout imposed by an explicit scenario/platform contract, child failure, or
  unmet gate, no checkpoint is created for the incomplete gate. Prior immutable checkpoints remain.
- The report computes first divergence by proof-route order, not report arrival order. It leads with
  gate ID and label, expected state, observed state, causal event/artifact references, completed
  gates, and latest valid checkpoint. If no gate started, it names the contract-preflight failure.

Implementation status:

- [x] The harness can deliver the game's save/quit sequence, audit writeback, relaunch once, and copy
  a final world snapshot.
- [ ] 🔴 R-003 — The runner has no general gate-segment executor, atomic gate checkpoint, or causal
  first-divergence report, and it incorrectly colors transport actions as proof.
  - Code gap: one monolithic primitive loop and one post-relaunch contract own current execution.
  - Required mechanism: add the partitioned executor, gate-only proof ledger, safe save/relaunch
    boundary, atomic checkpoint manifest, and report fields above.
  - Proof: an interrupted bandit run retains the checkpoint after its last successful gate; a later
    transport failure identifies the next gate as first divergence without yellowing earlier input;
    a deliberately interrupted checkpoint publication leaves no valid partial checkpoint.

### 4. Checkpoint-chain authority, resume, certification, and reruns

Mechanism:

- Files and symbols: `scenario_registry_store.py`, a new append-only registry migration,
  `scenario_registry_cli.py :: registry-launch` and status/report commands, startup harness restore,
  and `.agents/skills/caol-harness/SKILL.md`.
- The registry is the authoritative index for chains, segments, checkpoints, binding-validation
  results, and terminal certification/diagnostic outcomes. Checkpoint files remain immutable,
  hash-referenced artifacts. Registry rows are append-only history; a later validation event changes
  effective status without rewriting the earlier receipt.
- Runtime relevance includes `tools/openclaw_harness` in addition to the current executable,
  product source, data, and build inputs. The chain binding also includes the exact scenario,
  fixture, installed profile contract, run options, and selection declaration.
- Before every new segment or resume, the registry recomputes the complete chain binding. Any
  relevant difference marks the entire existing chain invalid for certification and reports the
  first differing component. No descendant of an invalid checkpoint may certify.
- Resume is explicit: the agent passes a checkpoint ID to the registry-backed launch command. The
  registry validates token authority, chain binding, checkpoint hash, predecessor lineage, gate
  order, and artifact availability, then restores a copy into a new disposable run profile. It
  never mutates the source fixture or the checkpoint snapshot.
- The report recommends the latest binding-valid checkpoint on the selected chain and explains the
  recommendation. The agent may resume that checkpoint, select another valid checkpoint, or start
  a new run.
- A certification chain is green only when checkpoint lineage is contiguous from the declared
  initial state through every gate in the final proof route, each gate is green from its own bound
  evidence range, and the final saved-state artifact is green. Diagnostic segments never receive a
  certification verdict merely because they found the cause.
- `registry-launch` distinguishes an intended diagnostic run from a certification attempt in its
  arguments and report. The default documented path remains the certification path.
- Replace the current one-token/one-report invalidation behavior with multiple immutable attempt
  records under the same causally unchanged token/binding. A different report does not invalidate a
  token merely because it is a rerun. Changed bindings invalidate prior certification authority but
  never forbid querying or launching a fresh trusted attempt.
- The harness skill documents query, inspection, diagnostic launch, certification launch, explicit
  resume, report interpretation, invalidation, and the fact that recommendations do not make the
  agent's choice.

Implementation status:

- [x] Registry tokens and reports are durably bound and revalidated before launch.
- [ ] 🔴 R-004 — Durable chain lineage, explicit resume, whole-chain invalidation, collective
  certification, and causally unchanged reruns are absent.
  - Code gap: there are no chain/checkpoint tables or restore command, harness code is not in the
    runtime relevance set, and `multiple_report_runs` rejects a second report for one token.
  - Required mechanism: add append-only chain persistence and validation, explicit restore through
    the canonical registry launch path, chain certification, repeated-attempt history, and the
    authoritative skill/CLI/report surfaces above.
  - Proof: two resumed segments certify only with matching contiguous lineage; changing product
    source, harness source, data, scenario, fixture, executable, profile, or options invalidates the
    entire old chain; the same token can record causally unchanged diagnostic/certification reruns;
    invalidation still permits a fresh query and launch.

### 5. Slow-run progress and bounded diagnostic presentation

Mechanism:

- Files and symbols: `execute_probe_steps`, the existing wait loops, child-process monitoring,
  structured event reader, report finalizers, and compact stdout formatter.
- At existing step boundaries and existing wait-poll observations, the harness records a progress
  sample containing wall-clock elapsed time, product game turn/time when observable, child process
  CPU and resident-memory observations available on the host, current segment/gate, latest
  structured transition sequence, and artifact byte growth. This contract creates no new sampling
  cadence or intuitive threshold.
- Long waits publish progress through the existing harness output/report path while they run. A
  platform that cannot provide one resource field records it as unavailable and retains the other
  fields; it does not fabricate zero.
- Repeated diagnostics are keyed by typed semantic identity. The report stores count, first/last
  occurrence, and representative event references, while the raw artifact is stored once and
  referenced by path/hash. Report JSON does not duplicate thousands of identical log lines.
- Compact output leads with run intent, chain/binding status, current or first-divergent gate,
  completed gates, latest valid/recommended checkpoint, last product progress, and decisive artifact
  references. It does not hide causal evidence behind a fixed truncation of primitive-step rows.

Implementation status:

- [x] Existing steps and waits already supply observation boundaries, and the child process and
  artifact paths are known to the harness.
- [ ] 🔴 R-005 — Slow runs lack useful live progress/resource evidence, and reports duplicate or
  foreground repeated diagnostics and primitive-step colors.
  - Code gap: the current compact formatter truncates arrays while the full report embeds extensive
    repeated evidence; no progress record joins wall time, game time, resources, and latest event.
  - Required mechanism: collect at existing boundaries, aggregate typed repetition, store raw
    evidence once, and make the causal chain summary the report's primary surface.
  - Proof: a controlled wait exposes advancing or stalled product time plus resource observations;
    unavailable metrics are explicit; the motivating repeated diagnostic is represented by one
    aggregate and artifact reference; first divergence remains visible in compact output.

### 6. Non-combat incidental-hostile suppression

Mechanism:

- Files and symbols: versioned scenario contract and preflight, a narrow harness-only live-game
  command near the existing harness UI trace integration, `game::remove_zombie`, hostile-ecology
  actor identity lookup, structured event/receipt writer, and focused game/harness tests.
- A non-combat scenario may explicitly declare incidental-hostile suppression. A combat scenario,
  an undeclared scenario, or a version-1 scenario cannot invoke it.
- The declaration supplies the exact spatial eligibility region and eligible loaded monster
  selectors. There is no implicit radius or filename-based policy. Current scope does not remove
  NPCs; a hostile NPC is ineligible and remains ordinary product state.
- Before mutation, the command takes one loaded-world snapshot of all candidates and verifies every
  candidate's monster type, absolute position, tracker identity, hostile attitude, selector match,
  and non-membership in the bandit/cannibal operation under test. Duplicate identity, missing
  identity, changed position/state, unknown ecology ownership, or any mixed eligible/ambiguous batch
  fails closed before removing any actor.
- After full-batch validation, the command removes only the selected monsters through
  `game::remove_zombie`/creature-tracker removal, never `die`, damage, debug kill-area, or an ecology
  casualty intervention. It then verifies absence. Ordinary drops, kill events, morale, anger,
  death callbacks, bounty, camp evidence, and operation casualties do not occur.
- One actor receipt per considered entity records run/chain, command ID, type, stable observed
  identity, absolute position, attitude, eligibility decision/reason, ecology-exclusion result, and
  mutation/verification outcome. A batch receipt records atomic success or fail-closed rejection.
  Receipts are bound into the next checkpoint evidence but never satisfy a product transition gate.

Implementation status:

- [x] A direct no-death-effect loaded-monster removal primitive exists.
- [ ] 🔴 R-006 — Scenario-scoped eligibility, fail-closed ecology exclusion, atomic suppression, and
  actor receipts do not exist.
  - Code gap: existing debug kill paths use normal death semantics or ecology intervention semantics
    and cannot meet the non-combat boundary.
  - Required mechanism: add the explicit versioned declaration and the narrow validated removal
    command above; do not reuse ordinary combat/death or claim proof from suppression.
  - Proof: an eligible incidental hostile is removed with complete receipts and no death side
    effects; an ecology actor, hostile NPC, ambiguous identity, combat scenario, or changed candidate
    causes atomic rejection and leaves all actors untouched.

## Competing systems and override direction

| State or action | Readers | Writers / competing owners | Authoritative decision |
|---|---|---|---|
| Scenario intent and proof route | Registry query, preflight, chain executor, report | Scenario manifest versus primitive step list | The versioned manifest owns intent and ordered proof-gate IDs. Primitive steps support the route and cannot add proof requirements. |
| Product transition truth | Gate evaluator, report, registry ingestion | Central hostile-ecology writers versus text logs/screens | A committed structured transition or bound saved-state artifact owns truth. Logs and screens are supporting context unless a gate explicitly requires a saved visual artifact. |
| Step execution | Segment executor | Existing `execute_probe_steps` versus new chain executor | Primitive executor owns delivery within one segment; chain executor alone owns gate evaluation, save boundaries, and segment order. |
| Save state at a gate | Checkpoint publisher and later restore | Live disposable world, source fixture, checkpoint snapshot | Normal game save/writeback produces state; atomic publisher captures it. Source fixtures and prior checkpoints are immutable. |
| Chain history and status | CLI, report, selection verification | Filesystem artifacts versus SQLite registry | Registry append-only receipts own lineage/status; artifact hashes prove referenced bytes. Neither may silently repair the other. |
| Resume selection | Agent and registry launch | Harness recommendation versus automatic recovery | Harness recommends and explains; agent explicitly chooses; registry only validates. |
| Certification | Registry verification | Individual green segments, diagnostic reports, sibling runs | Only one contiguous binding-valid checkpoint chain over the declared route certifies. Sibling or diagnostic success cannot fill a gap. |
| Rerun authority | Agent | Existing `multiple_report_runs` policy versus history retention | Causally unchanged reruns are permitted and recorded as attempts. Binding change invalidates old certification but does not prohibit new execution. |
| Observer/stabilizer state | Installed-save preflight | Fixture declarations versus actual compressed save | Actual installed save is authoritative; declarations specify expected policy but cannot prove it. |
| Incidental-hostile removal | Suppression command | Ordinary combat/death, ecology casualty intervention, generic debug kill | Only the validated non-combat suppression batch owns this removal. It yields entirely on ambiguity and cannot mutate ecology actors. |

Ownership transfer is atomic at two boundaries. A proof gate transfers from live execution to a
checkpoint only after product evidence, normal save writeback, complete snapshot hashing, and atomic
publication. A resume transfers from an immutable checkpoint to a new disposable profile only after
registry binding and parent-lineage validation. Failure before either commit leaves the previous
owner and checkpoint status unchanged.

## Acceptance and proof

For every red ID, the proof route is:

```text
declared preconditions
  -> canonical registry/harness owner
  -> real product or harness transition
  -> named observable outcome
  -> immutable artifact and registry receipt
  -> explicit pass/fail classification
```

| Red ID | Outcome test | Required evidence | False-green controls |
|---|---|---|---|
| `R-001` | Preflight valid and invalid versioned bandit/cannibal contracts before child launch. Install fixtures and audit the actual saves. | Versioned normalized contract, preflight report, fixture transform report, saved-player audit, proof that no child PID/run execution exists on rejection. | Filename inference, manifest prose, requested-but-not-present trait, v1 primitive label, transport-only gate, writhing exception, or fixture-injected transition cannot pass. |
| `R-002` | Drive committed and rejected bandit/cannibal transitions through central production writers. | Ordered JSONL events with run correlation, identities, generation/epoch, previous/new state, reason, watermarks, saved-artifact references, and gate predicate results. | Matching log text, rejected/no-op event, wrong operation/generation/actor, malformed tail, or event outside the gate range cannot pass. |
| `R-003` | Run the bandit route through one successful gate, interrupt/fail later, and inspect then resume the published checkpoint. Inject a checkpoint-publication failure. | Gate-only ledger, normal save/writeback evidence, atomic checkpoint manifest/hashes, retained previous checkpoint, first-divergence block, no published partial checkpoint. | Successful keypress without artifact is not proof or yellow; copied live memory without save is not a checkpoint; a failed/incomplete gate creates none. |
| `R-004` | Complete the named bandit route using resumed segments and complete the named cannibal route through the same chain machinery. Mutate each binding class in isolated tests and perform unchanged reruns. | Append-only chain/segment/checkpoint rows, parent hashes, restore-copy evidence, complete binding comparisons, certification verdicts, repeated attempt history, CLI and harness-skill output. | Noncontiguous segments, sibling reports, missing checkpoint, stale/changed binding, diagnostic-only success, automatic resume, source-fixture mutation, or `multiple_report_runs` rejection cannot certify. |
| `R-005` | Observe a controlled long wait and the motivating repeated-diagnostic shape. | Progress samples from existing boundaries, explicit unavailable fields, semantic aggregates, one raw artifact path/hash, compact first-divergence and checkpoint summary. | Invented zero resource value, fixed-array truncation that hides cause, duplicated raw lines, or activity in wall time without product-turn progress cannot claim product progress. |
| `R-006` | Suppress a declared eligible incidental loaded monster in a non-combat route, then exercise each rejected class. | Candidate snapshot, per-actor and batch receipts, tracker removal verification, event/death-effect negative evidence, next-checkpoint binding. | Ecology actor, NPC, ambiguous/changed identity, out-of-region monster, combat/undeclared scenario, partial batch, `die`, damage, kill-area, or casualty intervention must fail or remain untouched. |

Integrated acceptance uses the registry-backed commands documented by the harness skill and the
current executable. It must preserve the motivating physical-return failure as an honest first
divergence until product behavior actually crosses that gate. The chain machinery does not convert
an incomplete hostile-ecology path into green product evidence.

## Failure cases

- The game launches before static and installed-save preflight complete.
- A gate is satisfied by key delivery, elapsed wait, filename, prose, or an unbound log substring.
- A copied world lacks proven normal save writeback or is published before all hashes validate.
- Resume mutates a fixture/checkpoint, skips a gate, changes actor/operation identity, or joins a
  segment from another binding.
- A relevant change invalidates only a suffix instead of the entire certification chain.
- An unchanged trusted rerun is refused because a previous report exists.
- A diagnostic run, sibling run, or old green report silently supplies final certification.
- Report ordering hides the earliest causal divergence behind later noisy errors.
- Slow-run reporting shows wall activity as gameplay progress when product game time is stalled.
- Incidental-hostile suppression runs in combat, guesses a radius, kills through ordinary death,
  touches an ecology actor, partially mutates an ambiguous batch, or omits an actor receipt.
- Writhing-stalker or zombie-rider work enters this round, or observer perception traits are removed
  as a workaround.

## Freeze record

- Status: Frozen.
- Frozen source baseline: `dev@7f4697ee6b17fb897461e3ceb290342b83787a30`, with the pre-existing
  dirty hostile-ecology source/tests, harness/registry/scenario/fixture work, phase-3 runtime files,
  and unrelated artifacts preserved. This phase changes specification/setup artifacts only.
- Relevant inspected dirty frontier: `.agents/skills/caol-harness/SKILL.md`,
  `src/bandit_live_world.cpp`, `src/bandit_live_world.h`, `src/do_turn.cpp`, focused hostile-ecology
  tests, `tools/openclaw_harness/startup_harness.py`, registry schema/store/CLI/tests, the named
  bandit scenario and fixture, and related existing scenarios/fixtures.
- User-owned choices: all clauses in `.de67/WEC.md`, including full-chain invalidation, agent-owned
  resume choice, causally unchanged rerun authority, standard observer traits, non-combat-only
  suppression, and explicit round boundaries.
- Evidence-implied refinements: none. The mechanisms above are the smallest code-grounded route
  through existing registry, harness, save, transition-event, and creature-removal seams.
- Worker capability probes completed before setup: default model (intended `gpt-5.6-luna`) at
  medium and high effort, and explicit `gpt-5.6-terra` at medium and high effort, each returned its
  exact nonce and performed no task work.

After freeze, automation may only close an existing red item after its named proof and remove that
red marker; make an evidence-implied nonmaterial clarification; or append a uniquely implied
same-contract mechanism, ownership/proof detail, and necessary stable red claim after a verified
phase-3 worker finding. Existing claim identities, text, status, accepted work, and acceptance
strength remain fixed. Refreeze immediately. Product intent, project language, permissions,
user-visible behavior, balance, and materially different design choices remain user-owned.
