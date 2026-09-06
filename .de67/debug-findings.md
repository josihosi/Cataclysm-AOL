# Debug observations and suspected gameplay bugs

Current intake follows the 2026-09-06 WEC. DE67 loop faults go to method mutation.
Harness faults may be diagnosed, repaired and verified by the coordinator, mutator or repair worker.
CAOL gameplay observations are suspected bugs, not authorized findings or repair tasks. Only Josef's
explicit promotion authorizes a finding, followed by a mutator DFS update and fix/retest plan.
Classify the failed responsibility rather than the source filename; uncertainty remains explicit.

Record expected versus observed behavior, exact run/artifact/source evidence, affected and unaffected
tests, evidence ceiling, blocking consequence and current disposition. Keep the original observation
and any later owner decision, correction and verification distinct. Continue independent tests while
waiting; preserve blocked tests and report all-blocked work as awaiting the owner, never complete.

## Current suspected gameplay bugs

No new gameplay observation was produced by the stopped R-026-exploration-002 launch. Its semantic
receipt implementation is preserved at checkpoint `38ff17ef99` pending verification.

## Historical evidence boundary

The dated entries below retain their original observations and dispositions. Their old finding labels
and proposed fixes do not establish Josef's promotion under the current contract. Consult current
source before assuming a listed repair remains missing; retain prior valid work and its evidence.

## R026-F001 — prepared-base audit declaration was not registry-selectable

- Date: 2026-09-03
- Initial diagnostic artifact: `build_logs/r026_prepared_base_audit_20260903.json`, run
  `a15f0de6763f4b329aadd03b71aced02`; repaired-binding validation:
  `build_logs/r026_prepared_base_audit_bound_20260903.json`, run
  `99528d25e8da4263a1e64337df14e770`.
- Observed defect: `bandit.basecamp_prepared_base_audit_mcw` loaded the named fixture and reached a
  clean gameplay HUD, but its declaration had no manifest, runtime contract, capability facts, or
  source binding.  The harness therefore classified the run as `startup/load-or-inconclusive` and
  the typed registry query had only `unknown_fact` results for the exact fixture/world request.
- Affected claims: `R-026` package-footing selection and audit reuse.
- Explicitly unaffected: `R-027` accepted signal observation/response, and every preserved
  standalone report at its recorded ceiling.  No gameplay observation is discarded by this finding.
- Evidence ceiling: startup/load only.  Clean launch, screenshots, fixture installation, and this
  declaration repair do not prove living-base, camp-operation, hostile-lifecycle, or flesh-raptor
  behavior.
- Disposition: repaired declaration binding in
  `tools/openclaw_harness/scenarios/bandit.basecamp_prepared_base_audit_mcw.json`; independently
  validated its manifest and clean startup/load through the same disposable route.  Registry
  projection now returns it as an active exact-footing candidate, but with no selection token or
  package authority.  Do not rerun the unchanged setup as feature proof.

## R026-F002 — first live-package declaration initially had no usable registry path

- Date: 2026-09-03
- Bound artifacts: `build_logs/r026_living_npc_registry_query_20260903.json` and
  `build_logs/r026_living_npc_registry_query_authorized_20260903.json`.
- Observed defect: the registry ranked the new exact-footing living-NPC scenario, but its
  first-run route allowlist issued no route authority.  The first repair then exposed a second
  validator path that still rejected `grants_gameplay_proof: true`; the originally emitted token is
  stale and must not be launched.
- Affected claims: `R-026` living-NPC package route only.
- Explicitly unaffected: prepared-base load audit, R-027, and all preserved standalone evidence.
- Evidence ceiling: route/registry diagnosis only; no NPC behavior was observed.
- Disposition: add the same narrowly named R-026 allowance to both versioned-manifest validators
  and the first-run route resolver, rebuild the registry, and obtain a fresh post-repair token
  before any launch.

## R026-F003 — live-package executable drifted from its recorded build receipt

- Date: 2026-09-03
- Bound run and artifacts: live run
  `ff75149ca0e0145899e5ab59c4f8bf9f2963648573f9998d43a01b64dd84bccb`, its sealed terminal
  response `r026-finish-binding-drift-001.json`, runtime binding
  `.userdata/dev-harness/harness_runs/20260903_051950_ccd678a1d17045fd9a492b72e95d0fc6/runtime.binding.json`,
  and authorization-side build receipt
  `.userdata/openclaw_harness/source_bindings/cataclysm-tiles-9c12cd9305daf8f1.json`.
- Observed defect: the active session identified `cataclysm-tiles` as
  `1162d79dcb73421f288941ce668ffa6f6b59e60ac4aadb99aadeb5d466f2dc16`, while the recorded
  build receipt for the selected executable records
  `9429879929dcb615cb0eafe0d818c8b6475f68b106cd8e824798d20ca9d4d4a5`.  Native receipts did
  reach World chat, the real chat menu, Text input, and an accepted prompt submission, but that
  interaction is diagnostic only because the executable identity cannot be qualified against its
  launch receipt.
- Affected claims: R-026 follower free text, ambient LLM behavior, request snapshots, basecamp
  request context, persistence, and gameplay feel from this run.
- Explicitly unaffected: the canonical prepared-base startup/load audit; this run's native-owner
  diagnostic receipts; R-027; and all standalone evidence at its recorded ceiling.
- Evidence ceiling: diagnostic.  The sealed run stopped as
  `source_executable_binding_mismatch`, completed cleanup, and grants no gameplay credit.
- Disposition: re-establish one exact current source-to-executable receipt, re-query/relaunch the
  living-NPC scenario after that changed binding is independently validated, and retain this
  terminal run as the first binding-divergence anchor rather than treating its accepted prompt as
  behavior proof.

## R026-F004 — current living-NPC route has camp and ambient-identity divergence

- Date: 2026-09-03
- Bound run: source-bound registry run
  `5899e6957e17aef803a3848660c8b2b9ecfe6ff0b7110c7e0ea0dc26bf1c735b`, executable
  `cataclysm-tiles` SHA-256 `1162d79dcb73421f288941ce668ffa6f6b59e60ac4aadb99aadeb5d466f2dc16`,
  and source receipt
  `.userdata/openclaw_harness/source_bindings/cataclysm-tiles-9c12cd9305daf8f1.json`.
  The direct/ambient native action receipts are in
  `.userdata/openclaw_harness/r026_living_npc_session_bound_20260903/responses/`; current LLM
  prompt, snapshot, response, and routing records are in `config/llm_intent.log` and
  `config/llm_intent_events.log`.
- Observed defect: the direct Katharina request records `uses_basecamp=no`, `camp_found=no`, and
  `assigned_camp=none`, even though the same current native world frame exposes Basecamp Food,
  Locker, and Storage zones.  Separately, the unaddressed utterance is routed to Giuseppe Bachman
  (actor id 4), who is not in that frame's visible entities, while the resulting Giuseppe snapshot
  calls visible friendly Katharina and Robbie hostile.
- Affected claims: basecamp request context; ambient recipient identity; snapshot relationship
  fidelity; gameplay feel for ambient speech; and persistence of any camp operation that would
  depend on that request route.
- Explicitly unaffected: the accepted current string-prompt owner; the direct follower free-text
  utterance/response; the accepted current ambient request/response; the current executable
  binding receipt; prior prepared-base startup footing; R-027; and all standalone claims at their
  own ceilings.
- Evidence ceiling: current mechanical and product-observation evidence only.  The reply is real
  LLM-run output (`use_api: true`), but an offscreen speaker and contradictory relationship
  snapshot cannot establish a coherent ambient/basecamp gameplay result.
- Disposition: inspect fixture-to-basecamp membership and ambient-target eligibility before
  attempting save/reload or camp-operation credit.  Keep the logged direct and ambient response
  receipts as independent positive observations; do not substitute zone presence for camp routing.

## R026-F005 — repaired fixture launch emitted a native descriptor that the bridge did not ingest

- Date: 2026-09-03
- Bound diagnostic run: `b638d28e34ac03a5fe42ab273661934b92116ca0a33a7c56c4b7bed6b76db15e`, bridge binding
  `f9bbe62462900b0df97c7daf2ad90951d63b23e7bcdf907a9a38eead404596c6`, and run directory
  `.userdata/dev-harness/harness_runs/20260903_054944_4ca6b0d2229b4d4c9f9fa87092cd684e`.
- Observed defect: fixture installation applied the Katharina/Robbie camp-resident repair and the
  game reached a clean HUD.  `debug.final.log` contains its current `surface_descriptor` frame 1,
  but `semantic.wake.observation.jsonl` has only `writer_bound`; the launcher timed out waiting
  for a same-run semantic frame.  No feature input was sent and bridge cleanup was accepted.
- Affected claims: the repaired camp-routing rerun, ambient-control rerun, and persistence check.
- Explicitly unaffected: the fixture transform receipt itself, source-bound compile/readiness,
  R026-F004's mixed witness, and all previously sealed independent living-NPC observations.
- Evidence ceiling: startup/load diagnostic only.  A descriptor in the debug stream does not grant
  semantic operation authority when the bridge lacks its corresponding ingested frame.
- Disposition: repair the semantic wake/descriptor ingestion handoff, then obtain a new source-bound
  token and repeat the camp-routed request with a save/reload witness.

## R026-F006 — addressed camp craft utterance reaches camp routing but is not parsed as a craft order

- Date: 2026-09-03
- Bound run: `f74aa9800e2c9a09d4aa96251702506a9d3fd0d9f26150084039d8a8394e1087`, bridge binding
  `70540eca55418d7cded979e82ee607871af86fd6b370ad8a3aba89594049b314`, report
  `.userdata/dev-harness/harness_runs/20260903_055636_928be2ec3ff04c2888f0f0cbb9c374a4/probe.report.json`,
  and current source receipt `.userdata/openclaw_harness/source_bindings/cataclysm-tiles-9c12cd9305daf8f1.json`.
- Observed defect: the descriptor-native World/chat/menu/string-prompt sequence accepted every
  current request, and the event log records Katharina as `uses_basecamp=yes`, `camp_found=yes`,
  `assigned_camp=140,41,0`, `reason=camp_grouped`, then `camp heard` for `Katharina, craft a
  bandage for the camp.`  The leading direct address remains in the utterance presented to camp
  craft parsing, so no durable craft request is queued and ordinary LLM handling follows.
- Affected claims: camp craft state change and save/reload continuity only.
- Explicitly unaffected: source/executable binding, native descriptor authority, fixture camp
  membership, visible-friendly actor fidelity, the native request receipts, and accepted cleanup.
- Evidence ceiling: current mechanical routing evidence; not a camp-operation or persistence proof.
- Disposition: normalize the selected listener's direct-address prefix before camp request parsing,
  rebuild/rebind, then independently submit the same route, quicksave, and relaunch the saved world
  to verify the new request record.
