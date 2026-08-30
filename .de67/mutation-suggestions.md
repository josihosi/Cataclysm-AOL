# DE-67 mutation suggestion ledger

This is a consumable mutation queue, not history. User-authored entries carry explicit
mutation-scoped owner authority; reviewer candidates remain evidence-backed proposals. A successful
review removes only completed entries; a failed or unproved review removes nothing. Preserve a
blocked entry with its exact conflict, impossibility, missing authority, risk, or uncertainty.

The pending section is machine-owned protocol as well as owner input. Encode each owner batch as
one top-level `- Owner-authorized [trigger]: ...` or `- Owner-authorized [defer]: ...` entry, and
encode a lesser-authority reviewer proposal as `- [defer] Reviewer-authored proposal: ...`. Indent
every continuation. A trigger requests review at the next durable quiet junction; a defer does not.
Accumulate a batch under one deferred entry and promote that entry once when ready. Any other
top-level syntax retains legacy trigger behavior.

For a miss, keep immediate recovery distinct from the smallest repeatable method correction and
state the counterexample that could falsify it. For random or universal review, preserve the
applicable policy's stored target, scope, authority, and evidence limits; a guard result never proves
more than its inputs.

## Pending suggestions

None.

## Consumed suggestions

- Owner-authorized [trigger]: Loosen the active playtesting and worker instruction system so it
  supplies goals, truthful context, capable tools, and a few real integrity boundaries without
  prescribing the agent's tactics. Perform a structural alignment pass across the installed de67 3
  coordinator prompts and policy obligations, `.de67/orchestrator-guidelines.md`,
  `.de67/test-and-task-guidelines.md`, `.de67/playtest-witness-contract.md`,
  `.agents/skills/caol-harness/SKILL.md`, the LLM-facing cockpit/TUI contracts and tool output, active
  DFS/ledger/charters, and tests that enforce those routes. Delete, merge, soften, or move procedural
  instructions instead of appending a generic “be creative” paragraph beneath them.

  Treat the live repository and installed de67 route as the research corpus. Trace each instruction
  from its authored source through the generated coordinator/worker payload and into any machine
  contract or test before changing it. Distinguish active routed policy from historical DFS evidence;
  delete or move inactive remnants instead of making workers obey them. Do not use online guidance as
  a substitute for this repository-grounded alignment pass.

  Local evidence anchors for the current overconstraint include
  `.de67/test-and-task-guidelines.md:13-17` (continue only after a broad positive materiality proof and
  otherwise fail closed), `:25-34` (one narrow fact and named causal question after every result),
  `:42-60` (charter-fixed shortcuts/stops, unsafe-divergence stop, and a derived maximum before every
  gameplay action), and `:68-79` (sentinel-before-authority and evidence-class procedure). Inspect
  `.agents/skills/caol-harness/SKILL.md:12-38` for the mandatory exact mutation workflow and `:70-86`
  for the single-`next_action`, repair-one-declaration, repeat-query loop. Inspect
  `.de67/playtest-witness-contract.md:6-35` for fields or rejection categories that have become
  paperwork rather than intellectual scrutiny. Inspect `.de67/orchestrator-guidelines.md:118-123`,
  where first-divergence continuation is still gated by positive proof and uncertainty fails closed.
  Follow generated runtime policy through `.de67/phase3-policy.json` and
  `.de67/phase3-contracts.json`, then through `coordinator_prompt`, `coordinator_ledger_contract`, and
  the worker-brief construction in
  `/Users/josefhorvath/.codex/skills/de67/de-67-3/scripts/coordinator_supervisor.py` and
  `/Users/josefhorvath/.codex/skills/de67/de-67-3/scripts/policy_kernel.py`; verify the installed de67
  prompt/contract tests do not silently restore removed tactics. Inspect the actual tool boundary in
  `tools/openclaw_harness/cockpit.py` (`keep_watch`, `keep_watch_unsafe_condition`, raw wait, and
  `guarded_move_relative`) and its LLM-facing recipes in `cockpit_tui.py`. Review enforcing tests,
  especially `cockpit_keep_watch_test.py` exact recipe/action assertions,
  `cockpit_raw_wait_test.py`, `r023_relative_movement_test.py`,
  `proof_classification_unit_test.py` repeat-query assertions, `r005_corridor_observation_test.py`,
  fixture mutation-contract tests, and any test that freezes one action sequence rather than an
  integrity boundary. Preserve binding-, contradiction-, receipt-, and evidence-credit tests such as
  `playtest_witness_test.py`; they protect truth rather than dictate exploration.

  Preserve the small hard kernel: user data and real external safety; source/executable/run/identity
  binding where a claim depends on it; honest observations and tool receipts; visible contradictions;
  evidence ceilings and no false promotion; cleanup ownership; and truthful terminal state. Treat
  everything else as strategy owned by the coordinator or worker. A playtest worker may explore
  messily, inspect broadly when useful, improvise native action sequences, change tactics, use debug
  interventions with honest zero-credit labeling, accept fictional injury or death, repair tools and
  scenarios, rerun, or abandon an unproductive strategy without earning administrative punishment.
  A coordinator brief should state the desired outcome, known evidence, real constraints, and useful
  tools—not a guessed implementation plan. Intermediate exploration need not look like final proof;
  only the claim presented for acceptance must satisfy the relevant truth and evidence boundaries.

  Make persistence creative rather than repetitive. The worker should stick with the playtest outcome
  and actively use the available game, debug, cockpit, TUI, map, witness, and inspection tools, but it
  does not owe loyalty to the first setup or action sequence. When a run becomes uncertain, it may
  rethink the causal model, inspect source or game state, restart with a better setup, or improve the
  instrument itself. If the missing fact is not observable, the preferred repair may be clearer game
  logging, richer structured observations or deltas, better minimap/overmap/status information, or a
  new composable TUI action—not another paragraph of procedure. Trace those opportunities through
  `tools/openclaw_harness/cockpit.py` observation/journal surfaces and
  `tools/openclaw_harness/cockpit_tui.py` (`_local_map`, `_overmap`, `_commands`,
  `_receipt_drilldown`, and `render_state`). Give the agent enough legible state and capable controls
  to invent a route the coordinator did not predict. Require honesty about what a restarted or
  instrumented run proves, but do not make uncertainty itself terminal or require advance approval
  to repair the harness. Favor inspired problem-solving and useful new observability over repeating
  an uninformative run or polishing its paperwork.

  Do not equate one playthrough with one proof attempt. The same claim may be demonstrated through
  different causal routes and may be observed more than once during a single live session. Let the
  worker retain useful world state, recover from an unhelpful event, reposition actors, wait again,
  try another native stimulus, or collect a second independent observation without automatically
  finishing or restarting the run. A divergence normally becomes context for the next action in that
  playthrough, not a reason to abandon it. Restart only when the current world state can no longer
  answer the claim honestly or when the worker judges a clean setup to be the better experiment.
  Receipts and witness output should be able to represent multiple attempts and observations while
  keeping their causal routes and evidence ceilings distinguishable.

  Explicitly remove `stop at the first divergence` as a worker or playtest policy. Preserve the first
  divergence as diagnostic evidence, but require a real effort to make the assigned proof work:
  inspect it, act, repair, change tactics, use available TUI/debug capabilities, rerun when useful,
  and continue inside the same outcome-sized task. A divergence is neither success nor an automatic
  terminal finding. Stop only when the outcome is proved, the agent concludes from evidence that the
  strategy or claim is genuinely contradicted, or continuation needs a real external/human decision,
  irreversible user-data risk, unavailable system capability, or materially different owner outcome.
  Delete or rewrite charter stop conditions, ledger phrasing, coordinator briefs, harness defaults,
  and tests that terminalize a run or worker merely because it encountered the first unexpected game
  event. Do not replace this with a fixed retry count or another administrative continuation test;
  trust the agent to judge whether continued effort advances the proof.

  Replace process-shaped tests with behavioral tests that admit multiple competent strategies and
  witness wording. Keep exact tests only for an actual machine protocol, irreversible side effect,
  identity/binding invariant, contradiction, or credit boundary. Simulate messy playtests where the
  agent continues after fictional danger, changes approach, uses an optional tool creatively, skips
  irrelevant clerical work, and still returns an honest supported or inconclusive result. Include
  inverse cases where a false binding, hidden contradiction, fabricated observation, or promoted
  zero-credit intervention is rejected. Compare prompt/tool payloads before and after: the revised
  route should provide less tactical control while retaining or improving outcome truth. Consume this
  trigger only after DFS, ledger, skills, prompts, TUI/tool surfaces, and enforcing tests agree on the
  same “structure, not prison” model.

  Disposition — resolved by exclusive review `b95c62281c33`. The incident was produced by tactical
  duplication: compiled obligations, the generated coordinator prompt, workspace guidance, witness
  fields, the harness skill, cockpit recipes, active ledger prose, and exact-wording tests each
  repeated a locally plausible process until the combined route rewarded packet compliance and
  terminalized first divergence. Immediate recovery compressed those active layers around the
  outcome, known evidence, real constraints, useful tools, and the hard binding/receipt/
  contradiction/credit/cleanup/terminal-state kernel; it preserved prior evidence and left the
  coordinator and worker responsible for causal strategy. The repeatable correction is to protect
  machine-owned truth mechanically and test observable behavior, while treating diagnosis, repair,
  action order, instrumentation, and reruns as agent judgment. Installed policy/prompt tests now
  admit different strategies while still rejecting invalid durable transitions. Messy-playtest
  counterexamples prove that classified and permissive choices can continue without hiding their
  receipts, while false binding, contradiction, and evidence promotion remain rejected. The
  adaptive-autodrive proposal was not implemented: the current worker-owned cockpit route already
  permits source/tool repair and the proposal supplied no failure that survived the corrected
  instruction path.

- Owner-authorized [trigger]: Stop routing R-005 around every monster the player happens to notice.
  Reassess the active R-005 causal model before continuing: distinguish the player/observer's
  auto-walk and wait path from the bandits' production-owned path calculation, and inspect current
  monster positions only to determine which actor and route they actually influence. The current
  qualification and waypoint scenarios have clairvoyance/night vision but no `DEBUG_CLOAK` or
  `observer_safety_mode: invisible`; repeated coordinate avoidance is therefore measuring an
  uncloaked player's hostile-sighting interruptions and may not test the intended bandit ecology.
  Preserve authentic findings, but retire or rewrite route-planner work that does not contribute to
  the intended natural bandit outcome.

  Add explicit, receipt-bearing danger-handling modes for both bounded wait and auto-walk in the
  cockpit/TUI. Preserve clear choices equivalent to: stop on every interruption; handle only
  classified non-dangerous interruptions; or ignore danger and interruption popups while waiting or
  auto-walking. The third choice is an ordinary optional per-operation LLM-facing control, not an
  approval gate: expose it, execute the agent's choice, and receipt what was skipped. Do not require
  cloak proof, scenario permission, a safety classification, or supervisor approval. A concise
  tooltip may say that cloaking is often useful with this mode. Reuse the existing `raw_wait`,
  guarded `keep_watch`, guarded/raw movement, native interruption handling, run binding, and receipts
  rather than building competing wait or travel engines. Trust the agent to select the appropriate
  mode from the current situation. Keep the default cautious, but do not make the permissive option
  fail closed merely because the harness would have chosen differently. Make all three choices
  obvious and independently available for waiting and auto-walking.

  Calibrate safety to playtesting rather than real-world harm. A disposable fictional character
  being spotted, bitten, injured, or killed is an observable game result, not an external safety
  incident. Do not teach workers or encode tests that stop merely because a generic `danger` or
  `damage` flag became true. The LLM may continue, ignore, retreat, heal, zap, restart, or inspect as
  appropriate to the playtest. Stop or invalidate only when the event contradicts the selected
  test's outcome, evidence boundary, identity/binding, or the agent's chosen mode—not because the
  harness applies blanket caution to fictional stakes.

  Allow narrowly scoped creature zapping when it is the simplest diagnostic or setup intervention.
  Bind it to an exact creature identity and position, record before/after state and cause, preserve
  cleanup, and grant zero natural-ecology, route, combat, lifecycle, qualification, or certification
  credit to the intervention. Use reassessed monster positions and zero-credit zap controls to learn
  whether bandit path selection changes, whether only the player's observer UI was interrupted, or
  whether the creature genuinely blocks production bandit behavior. A final natural claim must
  still come from an independent run without ecology-changing intervention; ignoring observer/player
  interruption UI may remain valid only when it does not alter bandit or monster state or decisions.

  Rewrite the DFS, active ledger, charters/scenarios, harness guidance, TUI contracts, and behavioral
  tests together around that corrected distinction. Prefer deletion of coordinate-churn machinery
  over another exception layer. Behavioral tests should show that each LLM-selected wait and
  auto-walk mode does what its label says, the default remains cautious, skipped interruptions are
  visible in receipts, a zero-credit zap reveals whose path changed, and a separate intervention-free
  natural run that alone can receive R-005 credit. Consume this trigger only when the current R-005
  trajectory no longer treats incidental player sightings as unexplained bandit-route blockers.

  Disposition — resolved by exclusive review `b95c62281c33`. The earliest preventable cause was an
  ownership error in the native-travel boundary: every event after a player-visible hostile boundary
  was rejected as route contamination, while the active R-005 scenario disabled continuation and the
  ledger responded by accumulating alternate coordinates. Those receipts honestly prove uncloaked
  player auto-walk interruptions, not production bandit path causality, so all completed runs were
  preserved as zero-credit evidence and the coordinate planner was retired from the active route.
  Immediate recovery added explicit cautious, classified-non-dangerous, and permissive modes to wait
  and movement contracts, kept the cautious default, and made handled prompts, fictional-danger
  decisions, and native hostile boundaries durable in results. Classified movement can consume only
  a bound safe recovery; permissive travel can answer only the exact native auto-move cancellation
  and records `N`. An R-021 exact-identity creature zap remains optional, receipted, and zero-credit.
  The repeatable method is now a causal comparison of observer control flow with production ecology
  state, followed by a separate intervention-free natural run as the sole qualification route. The
  counterexample uses one trace containing active, hostile-boundary, progress, and completion events:
  default handling rejects post-boundary travel, while explicit permissive handling reaches green
  and retains the handled boundary; inverse cockpit tests show stop mode takes no recovery action,
  classified mode handles a safe prompt, and permissive mode continues visible fictional danger.

## Earlier consumed suggestions

- Owner-authorized [defer]: Put cheap source/executable readiness before costly playtest authority
  or repair runs, without turning build order into a rigid ritual. In `R-009-closure-014`, the
  Terra/high worker correctly received “revalidate the current binding” and the macOS-first route,
  but `.agents/skills/caol-harness/SKILL.md` routes query contradictions directly into
  `registry-repair-bootstrap`, while `.de67/test-and-task-guidelines.md` says only that binding drift
  invalidates a receipt. The worker therefore consumed a repair run, diagnosed and repaired the
  unconditional-Escape startup defect, and only at the later launch boundary learned that executable
  source `7f740f8fc7` did not match repository source `adb27ff46d`. Evidence anchors are worker
  `01a04e24-1693-7b63-93f2-e7f0daf74ff0`, task `R-009-closure-014`, its final finding, and
  `build_logs/r009_closure014/`.

  Review the worker brief, harness skill, registry query/repair-bootstrap output, and owning tests as
  one information path. Make the earliest cheap preflight expose whether the selected executable is
  current enough for the intended run. When product-binary staleness would invalidate the next
  gameplay or harness conclusion, build or select a source-matching executable before consuming
  single-use authority or launching a repair/bootstrap run. Preserve agent discretion to use a stale
  executable for a deliberately isolated diagnosis whose conclusion does not depend on current
  product code; label that evidence provisional and still revalidate on a current build before
  claiming the playtest outcome. Prefer one actionable status/next action over duplicated prose or a
  new approval gate. Add a counterexample that starts with a stale executable and proves the route
  requests the build before launch, plus a control showing that an explicitly isolated harness-only
  diagnosis is not unnecessarily blocked.

  Disposition: consumed by exclusive incident review `c94a5e1b3eb0`. Immediate recovery preserves
  the unconditional-Escape repair, its focused tests, accepted cleanup, revised gap 6, and zero wait
  or feature credit; the stale binary now receives `build_required` before repair authority. The
  incident came from an information-path defect, not worker capability: query-time binding paired
  current source with an arbitrary executable, the harness skill routed the contradiction directly
  to repair bootstrap, and the authoritative compiled-head comparison appeared only after startup.
  The repeatable correction reads the existing executable `--version` surface before query-bound
  repair, reports one actionable source/executable status, and refuses repair-token issuance until
  product-binary inputs match. Explicit harness-only diagnosis remains available with a provisional
  ceiling and required current-build revalidation. The original `7f740f8fc7` versus `adb27ff46d`
  counterexample names changed `src/bandit_live_world.h` and `src/game.h` and requests a build before
  launch; the isolated control remains unblocked without gaining playtest credit. Sixty focused
  readiness, registry, charter, and capture-owner tests passed.

- Owner-authorized [defer]: Give mutation review an honest `no change justified` outcome and apply
  the alignment manifest to the active de67 3 instruction system. A scheduled, random, universal,
  reviewer-authored, or general guideline review currently pressures the reviewer to edit something
  merely because a review was opened. That manufactures method churn and lets tests accumulate as
  constitutional rules. The reviewer must be able to inspect the active route, conclude that the
  present method is already aligned or that the proposed change lacks evidence, record a concise
  no-change disposition, resolve the review normally, and return control without altering DFS,
  ledger, policy, guidance, scripts, or tests.

  Preserve the authority distinction. `No change justified` is valid for automatic reviews and
  lesser-authority reviewer proposals. It is not permission to silently ignore an explicit
  owner-authorized outcome. For an owner-authorized suggestion, either achieve the requested
  outcome or preserve the entry with the exact conflict, impossibility, missing authority, material
  risk, or unresolved uncertainty. A no-op review may still complete the exclusive reviewer
  lifecycle and hand control to the required fresh coordinator; it must not invent a code diff as
  proof of diligence.

  Use the optional alignment-audit method as a review lens, not production prompt cargo or a fourth
  phase. Inspect only active de67 3 and repository instruction routes, the machine output presented
  to agents, and tests enforcing those routes. Classify active authority, useful reference, and
  discoverable legacy residue. Trace every suspected problem through its exact runtime branch before
  changing anything. The earlier claim that `adaptive_semantic_autodrive` stole control from the
  worker is a mandatory rejected counterexample: tracing showed it drives declared semantic-window
  setup, while `cockpit_live_session` blocks in `serve_cockpit_live` and hands actions to the LLM.
  Do not revive or encode that false finding.

  Recheck the alignment findings that did survive the evidence pass and make only changes whose
  deletion would leave the live workflow materially misaligned:

  Evidence receipts below are investigation anchors, not automatic verdicts. Resolve paths against
  the installed de67 3 skill and this CAOL workspace, recheck current symbols when line numbers
  drift, and trace callers before editing:

  - Corrective-recovery mismatch: installed
    `de-67-3/scripts/coordinator_supervisor.py:804-833` describes later corrective opportunities,
    while `:1451-1461` immediately returns failure for an unchanged fingerprint. The active tests
    `de-67-3/tests/test_coordinator_supervisor.py:755-803`
    (`test_unchanged_success_with_executable_work_is_not_resumed` and
    `test_persistent_crash_without_durable_progress_is_not_retried`) encode the immediate-stop path.
  - Text-shaped progress and routing facts: installed
    `de-67-3/scripts/coordinator_supervisor.py:372-425` hashes raw DFS, ledger, mutation-ledger, and
    compiled-policy bytes into `supervision_fingerprint`; installed
    `de-67-3/scripts/policy_kernel.py:719-809` derives work from any ledger heading, ordinary prose
    phrases, and any DFS red glyph. These are the anchors for quoted-history, whitespace,
    waiting-section, and clerical-edit counterexamples.
  - Hardcoded worker strategy: installed `de-67-3/scripts/policy_kernel.py:299-379` emits one exact
    `gpt-5.6-terra`/`medium` spawn example although
    `de-67-3/scripts/coordinator_supervisor.py:845-857` delegates Luna/Terra and effort choice to the
    coordinator. `de-67-3/tests/test_policy_kernel.py:737-810` freezes the exact spawn shape and
    asserts Terra at line 778.
  - Test-as-policy candidates: installed `de-67-3/tests/test_policy_kernel.py:140-146` asserts that
    compiled policy is smaller than Markdown; `:869-879` preserves historical-baseline behavior;
    `de-67-3/tests/test_policy_compare.py:31-44` carries the legacy divergence list; and
    `de-67-3/tests/test_coordinator_supervisor.py:1057-1097`, `1119-1213` assert substantial prompt
    content. Determine which assertions protect observable behavior and which merely fossilize one
    implementation.
  - False service health: installed `de-67-3/scripts/supervisor_service.py:247-255` asks tmux for
    pane fields while targeting the session. The observed live response was
    `pid= created= windows=` even though direct process inspection showed the supervisor and Codex
    child alive. `de-67-3/tests/test_supervisor_service.py:323-370` accepts any output containing
    `pid=` at line 359 rather than requiring numeric/process truth.
  - False cockpit TUI health: workspace `tools/openclaw_harness/cockpit_tui.py:265-295` defaults
    missing status to `active` and a clear error; `:316-325`, `349-353`, and `369-384` discard or
    fail to surface unsuccessful `run.status` responses after refresh, dispatch, and finish.
    `tools/openclaw_harness/cockpit_tui_test.py:155-165` covers failed observation, but not a
    successful observation followed by failed status.
  - Rejected autodrive finding: workspace
    `tools/openclaw_harness/startup_harness.py:23311-23443` constructs and blocks in the
    worker-controlled `cockpit_live_session`, whereas `:24186-24227` applies semantic autodrive only
    inside `adaptive_semantic_window`; `tools/openclaw_harness/scenario_registry_cli.py:1955-1974`
    sets the flag for the canonical probe. Preserve this mode distinction.

  - Treat tests as executable explanations, not constitutional authority. Preserve mechanical
    invariants such as identity, ownership, leases, single-use receipts, immutable evidence,
    cleanup, process reaping, restart acknowledgement, truthful lifecycle, and fail-closed parsing.
    Rewrite, demote, or delete active tests that freeze exact prompt prose, one worker model or
    effort, one strategy or task order, proof-shaped ceremony, a historical baseline, obsolete
    compatibility, arbitrary projection caps, or token-size aesthetics without an authoritative
    contract. Test behavior and concrete false outcomes rather than wording.
  - Reconcile documented corrective recovery with runtime behavior and tests. A recovery contract
    that promises corrective opportunities must not immediately hard-fail the first unchanged
    frontier; conversely, any retry or fuse must come from existing authoritative policy and must
    stop repeated state rather than prescribe coordinator strategy.
  - Derive executable ledger work and durable progress from structured active state and meaningful
    transitions, not arbitrary headings, quoted history, stray red glyphs, whitespace, or raw
    Markdown byte hashes. Add negative controls for historical examples, waiting sections,
    clerical-only edits, and unchanged executable frontiers without turning the parser into another
    prose bureaucracy.
  - Make dashboard and TUI health truthful when process or status metadata is absent. The service
    currently permits empty tmux fields to satisfy a weak `pid=` assertion, and the cockpit TUI can
    replace a failed status read with an apparently active state. Require real numeric/process
    evidence or render state as unavailable/stale; keep durable coordinator, worker, and mutation
    roles distinct.
  - Let the coordinator choose among allowed worker profiles. Machine-check allowed model,
    ownership, and binding rather than hardcoding Terra/medium or one exact spawn sentence when
    coordinator judgment has selected another supported profile.

  Before promoting a candidate, run behavioral messy-state controls that admit multiple correct
  agent strategies and include a genuine no-change review. Report which suspected findings were
  rejected and why. The best valid outcome of this review may be a smaller change set than this
  list, including no method change for any item not supported by the active runtime path.

  Disposition: consumed by mutation review `b58bd8d728b7`. Immediate recovery made service and TUI
  unknowns unavailable instead of active, removed hardcoded worker strategy, and let no-change
  reviews close honestly. The repeatable correction now derives routing and progress from structured
  active state, preserves workspace-local policy at restart, and uses the existing three decision
  opportunities against semantic state. Behavioral counterexamples passed. The unavailable legacy
  comparison became optional, token-size and arbitrary projection-cap assertions were removed, and
  prompt tests now retain only operational invariants. The autodrive allegation was rejected after
  caller tracing confirmed that live cockpit play remains worker-owned.

- Owner-authorized [trigger]: Reorder the remaining CAOL proof trajectory around its actual
  dependency chain and restore every unfinished playtest claim to a visible lawful route. The
  current active ledger bundles R-009 macOS gameplay evidence with Linux/WSL and Windows compiler
  work, so the coordinator selected provisional cross-platform repairs while shared harness and
  gameplay work can still change the code they validate. Preserve the current in-flight compiler
  result as a checkpoint; do not kill it, discard it, or claim final platform proof from it.

  Structurally rewrite the DFS projection and active work ledger so the ordinary trajectory is:
  finish or repair the shared playtest harness and agent-facing playtest route; complete the
  remaining macOS and ecology/gameplay witnesses; settle the resulting gameplay and harness code;
  establish a reviewable source/binding freeze for those outcomes; then run final Linux/WSL and
  Windows compilation, preflight, and platform witnesses against that settled boundary. Split
  R-009's platform-independent/macOS playtest outcome from its final cross-platform validation so a
  compiler lane cannot outrank unfinished gameplay merely because both currently share one red
  ledger item. Preserve a narrow exception only when a platform compile is proved necessary to
  repair the shared harness or determine the gameplay implementation itself; state that causal
  dependency explicitly instead of treating general platform parity as parallel filler.

  Audit every red DFS claim against the ledger. Do not remove, accept, or hide an unfinished claim
  merely to simplify the projection. R-005, R-008, R-009, R-012, and R-023 must each retain a
  visible next action, accepted subproofs, and honest wake condition until their playtest outcome is
  settled. In particular, investigate whether R-008's manifest review and R-012's draft review are
  genuinely human or third-party authority. If they are repository-owned exclusive reviewer roles,
  route them to the correct agent-owned review mechanism rather than `Waiting on external
  authority`; if a real external owner exists, name that owner and the exact unavailable decision.
  Current evidence already shows R-008 is orphaned: `_exclusive_source_review_state` can require a
  decision and `record_source_bound_review_decision` can store one, but the latter is called only by
  a unit test, the public scenario-registry CLI exposes no source-review command, and the supervisor
  dispatches no source reviewer. Treat `external review` in that store function's docstring as a
  historical design claim, not proof that a real external owner or wake event exists. Trace R-012's
  draft-review path with the same caller-and-wake test. A waiting state with no named owner and no
  executable wake mechanism is a DFS/workflow gap and must be repaired or removed.
  Josef then explicitly authorized the current Codex session to perform both reviews. R-008 exact
  source revision 3 was accepted after its focused source/review and registry/observation controls
  passed; the registry now reports it executable and token-eligible. R-012's draft was reviewed as
  already pointing to an existing valid, no-review-required, zero-credit bootstrap scenario with no
  missing requirements, so its fictional review blocker was removed and its bootstrap plus later
  live validation were restored as ordinary active work. Preserve these decisions while removing
  the orphaned review mechanism from the future workflow.
  Preserve R-006 as the later certification-plus-Josef Windows-feel gate without letting it pull
  final platform validation ahead of the playtest freeze.

  Keep the coordinator free to change tactics inside this dependency order. Encode causal
  prerequisites and visible outcomes, not a brittle task script or exact claim-selection sequence.
  Preserve all authentic runs, findings, accepted evidence, dirty-worktree ownership, technical
  authority boundaries, and current source changes. Prove the rewritten projection with messy
  cases: an unfinished harness repair keeps final platform validation waiting; a completed
  playtest that changes shared code invalidates earlier provisional compiler proof; an in-flight
  platform build returns without being discarded; an agent-owned exclusive scenario review does
  not become a human blocker; a genuinely human Windows-feel judgment remains external; and every
  red DFS claim is either executable or has a truthful named wake condition. Promote the DFS,
  ledger, policy, and tests together only after the guarded route selects playtest/harness work
  before final cross-platform certification under the current frontier.

  Disposition: consumed by mutation review `b58bd8d728b7`. R-009 now has an active upstream macOS and
  shared-behavior route plus a separately dependency-gated final Linux/WSL and Windows route.
  Provisional compiler evidence is preserved without final credit and must be revalidated after the
  reviewable input freeze. R-005, R-008, R-009, R-012, and R-023 remain visible; R-008 revision 3 was
  durably accepted for execution, R-012's no-review bootstrap is executable, and append-only gap
  revisions remove both orphaned reviewer dependencies. R-006 remains the named external Josef gate.

### Earlier consumed suggestions

  - Owner-authorized [trigger]: Eradicate the false human-permission gate for ordinary agent-owned
  playtests and align the DFS, active work ledger, CAOL harness guidance, charters, tests, and runtime
  authority language with the trust-the-agent model. This is a structural authority-model repair,
  not permission for a search-and-replace or another sentence layered over contradictory rules.
  Research the complete live route before editing: coordinator outcome selection and witness-charter
  authorship; typed scenario query and selection; selection token, descriptor, cockpit/run receipt,
  canonical launch and cleanup; worker observation/action/repair/rerun; immutable journal and cited
  witness; evidence ceiling and registry ingestion; coordinator causal judgment; certification; and
  the separately owner-reserved Windows-feel judgment. Identify which component owns each decision
  and distinguish technical single-use run authority, evidence/proof authority, and genuinely human-
  reserved authority.

  Use the observed failure chain as a mandatory counterexample. The frozen DFS section
  `Competing systems and override direction`, row `Scenario selection and run opening`, says the
  worker owns fit judgment and may open one tracked run from a valid selection. The live-playtest
  section says the coordinator supplies a charter and the worker controls native play and acts as
  witness. In `.agents/skills/caol-harness/SKILL.md`, `Explicit selected launch and report` says the
  descriptor supplies authority, but the older sentence `Only after an explicit request to run the
  selected scenario` was read as a fresh human-permission requirement. R-009 workers
  `R-009-exploration-004` and `R-009-exploration-005` received no runtime permission rejection and
  never attempted a launch; the first inferred external authority from that ambiguous guidance, the
  coordinator projected the inference into `.de67/work-ledger.md`, and the second merely repeated
  the ledger. Commits `16bfadc64f` and `7f740f8fc7` then embedded `explicit selected-run authority`
  into R-009/R-005 charters and tests, converting the mistaken inference into repository law. Treat
  those references as evidence to investigate, not as an exhaustive edit list.

  Restore one coherent rule: when the coordinator selects an executable playtest outcome and
  supplies or authorizes the matching validated witness charter, that brief is the explicit request.
  The trusted worker may select the fitting scenario, claim and consume the mechanically issued
  single-use binding-specific run authority, launch, observe, act, repair, rerun when the binding and
  outcome remain valid, finish, and submit an objective evidence-cited witness without asking Josef
  again. The coordinator may create or repair the charter and judge the witness. Missing charter or
  invalid selection is repository/coordinator work or an honest technical finding, not automatically
  an external-owner blocker. Reserve human approval only where the DFS expressly assigns the outcome
  to a human, such as Josef's distinct Windows-feel judgment, or where system/platform safety truly
  requires it.

  Rewrite the active work ledger to remove the false R-005/R-009 external-authority blockers and
  remove the temporary standing-permission workaround once the native model makes it unnecessary.
  Reconcile and refreeze the DFS so its authority tables, playtest/witness contract, red claims, and
  executable routes agree. Rewrite the CAOL harness skill and any coordinator/worker routed guidance
  so query-only inspection cannot accidentally launch, while an explicit coordinator playtest brief
  plus validated charter is sufficient. Audit all current charters, honest-stop conditions, tests,
  token/descriptor names, CLI messages, dashboard text, and ledger projections for the same
  conflation. Delete, rewrite, or demote tests that assert human permission; do not preserve a faulty
  workflow merely because a regression test currently encodes it. Promote further research and
  context alignment wherever the surrounding mechanism reveals a more general mismatch.

  Preserve the useful hard boundaries: single-use source/executable/scenario/world/owner binding;
  no launch from an inert query or draft; exclusive process/world ownership; immutable native
  observations, actions, receipts, cleanup, citations, and contradictions; honest stop on binding
  drift or unsafe/unknown state; evidence ceilings; no setup, diagnostic, focused, or witness prose
  self-promotion; and separate coordinator causal judgment. Trust the agent's actions because the
  instructions and information are correct; mechanically police only machine truth and the few
  boundaries whose failure could fabricate evidence, corrupt shared state, or consume the wrong run.

  Prove the repaired model with behavioral scenarios, not exact prose matching: query without a
  playtest brief stays non-launching; coordinator brief plus validated charter lets a worker obtain
  and consume one bound run receipt without human intervention; a missing charter routes to charter
  creation/repair rather than Josef; a changed binding invalidates the receipt and permits a fresh
  agent-selected retry after revalidation; the worker can honestly conclude proved, contradicted, or
  inconclusive from cited evidence; supplied contradictions and cleanup failures remain visible;
  clerical defects are repaired without replay; certification and Windows-feel ownership cannot be
  stolen; and the historical R-009 chain cannot recreate an external-authority ledger blocker when
  no harness rejection occurred. Guard and promote the aligned DFS/ledger/guidance/harness candidate
  together, consume this trigger only after stale authority language and tests are removed, and
  request one fresh coordinator at the safe mutation boundary.

  - Disposition: consumed by exclusive owner-suggestion review `9f2e30b20829`. The incident began
    when ambiguous harness guidance split an already selected coordinator outcome from its run
    request; two workers made no launch attempt and received no runtime rejection, but the inferred
    human gate was projected into the ledger and then frozen into charters and exact-prose tests.
    Immediate recovery removes those false external blockers from R-005, R-009, and R-023. The
    repeatable correction gives the coordinator brief plus matching validated charter ownership of
    the execution request; keeps query inspection inert; gives binding-specific, single-use
    technical run authority to the registry; and reserves human authority for outcomes the DFS
    expressly assigns to a human. The aligned DFS, ledger, routed guidance, harness skill, charters,
    runtime language, tests, and compiled policy were compressed around that ownership split.
    Counterexamples prove that query selection alone does not launch, a brief-requested charter can
    consume a bound token without a human-permission parameter, binding drift still fails closed,
    and contradictions, evidence ceilings, immutable witness history, and cleanup remain enforced.
    The guarded 15-rule policy compiled reproducibly with guard digest
    `76f338c9b05ba3595d81c3a66e842b5eeac7590395f84f99fccd28efa4b1f68c`; 79 focused authority,
    charter, witness, ingestion, and cockpit tests passed.

  - [defer] Reviewer-authored proposal: Define the authority model for R-006 Windows feel evidence.
  The current route stores an immutable pass or fail and binds it to the certified executable, but
  any local caller can submit the literal label `Josef`. This matters because a forged row can make
  the combined acceptance green. The DFS must choose one of two coherent contracts. It can name a
  trusted identity authority and verifier, including enrollment, revocation, replay resistance, and
  handoff and build binding. Or it can declare Windows feel an external non-machine-verifiable
  judgment and prohibit that row from mechanically making `overall_acceptance` green. Preserve the
  ordinary pending handoff, executable binding repair `483c772211`, immutable storage, and separate
  automated gate. Do not treat an author string, mutable Git identity, or unavailable local signing
  configuration as authentication.

  - Disposition: consumed by exclusive owner-suggestion review `9f2e30b20829`. Repository search
    found no authority capable of authenticating Josef, so the smaller supported contract is the
    external, non-machine-verifiable judgment model. The registry still binds and immutably records
    an external pass or fail against the certified Windows handoff, while machine eligibility never
    turns `windows_feel` or `overall_acceptance` green from that row. The counterexample supplies a
    registry-shaped fact carrying both `authority=windows-josef` and `owner=Josef`; eligibility
    remains false with reason `external_windows_feel_not_machine_verifiable`. A separate ingestion
    test proves a labelled pass remains visible as an external attestation without satisfying the
    automated gate. R-006 remains honestly red until the real external product judgment occurs.

  - Owner-authorized [defer]: At the next regular mutation review, inspect and reconcile the three
    cautions from the latest live trajectory audit, then make the smallest evidence-backed changes
    needed to both the frozen DFS and active work ledger. First, examine R-005's seven consecutive
    findings without closure and distinguish cumulative discovery of real route/ecology constraints
    from repeated strategy or administrative churn. Preserve useful evidence, but rewrite or
    consolidate its DFS gap and ledger spokes if their current shape encourages replay instead of a
    materially different next move. Second, reconcile R-022's accepted ledger state with the DFS
    projection that still presents it as red. Preserve its zero-credit ceiling and do not broaden
    its accepted item-spawn result into gameplay or economy proof. Third, inspect the very large
    dirty worktree and accumulated run artifacts as an auditability and trajectory-management
    problem: reflect any genuinely unfinished integration, cleanup, or release-boundary work in
    outcome-sized DFS/ledger entries, while removing stale, duplicate, superseded, or purely
    clerical projections that no longer represent required work. Do not turn repository tidiness
    into a new proof bureaucracy, discard authentic evidence, replay settled gameplay, or expand
    scope merely because files are dirty. The review is complete only when DFS and ledger agree on
    what is accepted, what remains red, why it remains red, and the next materially useful route.
  - Disposition: consumed by exclusive review `063affb459d9`. R-005's seven latest findings were
    cumulative causal discovery, not seven unchanged retries, but each unsafe run stop was
    terminalized and the latest counterexample left SQLite revision 28 asserting a final-handoff
    defect that it never reached. Immediate recovery appended revision 29, rewrote the DFS and
    canonical ledger around zero-credit qualification of a materially different natural route, and
    forbids repairing the handoff until that boundary is reproduced. The selected task guideline now
    distinguishes ending unsafe gameplay input from terminalizing an authorized diagnosis/repair
    task. R-022 now reflects durable acceptance `R-022-closure-004` in the DFS while preserving its
    setup-only ceiling. The trajectory audit found a real checkpoint boundary—56 tracked status
    entries, 906 untracked entries, 1,684 build-log files, and 688 runtime run directories—so R-025
    owns reviewable checkpointing and evidence retention without requiring a clean tree, artifact
    deletion, or settled-gameplay replay. Duplicate R-019 projection prose was consolidated, and the
    previously noncanonical active ledger changed from zero machine-visible items to five guarded
    slice briefs. Counterexamples prove revision 29 preserves the closure-029 interruption and
    rejects replay repair; the R-022 completion guard, cycle-15 lane guard, live ledger validator,
    five slice extractions, and `git diff --check` pass. No authentic evidence or product code was
    removed or changed by the review.

  - Reviewer-authored proposal [defer]: Add a named machine slice for R-015 to the frozen DFS. The
  compiled policy rejected `R-015-exploration-001` because R-015 has no `DFS-SLICE` binding. This
  matters because a deadline task can be created but cannot be bound to an ordinary worker, so the
  lifecycle-boundary ordering defect cannot be investigated or repaired. Preserve the existing R-015
  outcome and proof text. Add the smallest stable slice identifier and claim binding that let the
  policy pass only the R-015 contract to an exploration or closure worker. Prove the kernel emits a
  valid deterministic spawn brief for R-015 before marking this suggestion resolved.
  - Disposition: consumed by exclusive review `0f4add0c7800`. Immediate recovery added marker-only
    slice `R-015-S001`, reprojected R-015 from mutation wait to executable exploration, and preserved
    its outcome, proof, and all prior evidence. The task failed because R-015 was refrozen while not
    active, but mutation closeout required slice proof only for active claims. The repeatable
    correction requires every added or changed red claim—projected, waiting, or unselected—to yield
    a lawful slice brief before restart. The reviewer entry also used a human-readable defer label
    outside the parser grammar and was treated as a legacy trigger; the compressed ledger protocol
    now places `[defer]` first for lesser-authority proposals. Copied-state counterexamples prove the
    original R-015 task fails before the marker, the corrected state emits an isolated deterministic
    R-015 spawn brief, a correctly encoded reviewer defer does not retire the coordinator, and an
    owner trigger still does.

  - Owner-authorized [trigger]: Repair the R-018 closure contract after the witness replacement.
  The current gap still requires a green R-018 matrix, but the accepted replacement deliberately
  removed that matrix and forbids keeping both proof systems. No R-018 charter now defines which
  preserved raw, primitive, master-off, and gadget-off facts form a mechanically valid immutable
  witness. This matters because authentic native receipts cannot become admissible proof, and new
  gameplay runs would reproduce the same contradiction. Rewrite the R-018 gap and its executable
  decomposition around one coordinator-authored charter, cited immutable journals, contradiction
  disclosure, evidence ceilings, and coordinator judgment. Preserve the existing raw and primitive
  receipts. Do not restore the retired matrix. Require the repaired route to state exactly which
  off-role facts remain necessary before another ordinary worker is dispatched.
  - Disposition: consumed by exclusive review `c6eefc49a809`. Immediate recovery rebased open
    gap `R-018/G-001` from the retired matrix to one coordinator-chartered composite witness and
    projected the executable subtasks without replaying accepted raw/primitive reports. The
    remaining off controls are exactly master-off and raw-wait-gadget-off; each must prove no gadget
    native dispatch, preserved primitive acceptance and target progress, no interpreted
    interruption, successful cleanup, and a focused ceiling. The incident occurred because the
    witness replacement changed DFS, harness, and ledger method owners but left SQLite gap revision
    9 authoritative. The repeatable correction makes proof-owner replacement set a conditional
    projection-rebase obligation that blocks mutation resolution until affected durable gaps and
    active briefs no longer name the retired owner. Policy contracts accept the rebased trace and
    reject the original suggestion-disposition-plus-stale-gap transition.

  - Owner-authorized [trigger]: Replace playtest proof bureaucracy with an LLM witness channel and
  route its instructions to the agents that own the judgment. This is a structural reduction, not
  another report layered over existing matrices. Research the complete live path first: coordinator
  playtest recognition and dispatch brief, worker skill routing, cockpit/TUI descriptor and event
  journal, immutable run artifacts, registry ingestion and evidence ceilings, coordinator result
  judgment, DFS acceptance, and every scenario-specific proof matrix or step-ledger owner that the
  replacement can delete.

  This mutation explicitly authorizes and requires a **DFS rewrite and active work-ledger rewrite**,
  not merely a policy, prompt, harness, or schema patch. Recast playtest work as outcome-sized DFS
  tasks with visible internal spokes. When an outcome genuinely decomposes, one task should contain
  **4-7 coherent, independently visible subtasks** covering the useful arc (for example preparation,
  authentic execution, observation, causal judgment, repair when needed, and acceptance), rather
  than opening a succession of single-subtask clerical tasks. Preserve fewer subtasks for work that
  is honestly smaller; never invent filler to reach the range, merge unrelated outcomes, or turn a
  task into an unbounded bucket. A one-subtask task is appropriate only when the outcome is genuinely
  atomic. Rewrite the current ledger into this shape so the sidecar plot visibly renders meaningful
  spokes and the coordinator can continue toward the outcome without detaching and reopening the
  task after every small step. Make task closure mean that the outcome is settled, while subtask
  state communicates real progress and remaining uncertainty.

  Put the coordinator-facing policy in the routed coordinator playtest contract, not in a large
  supervisor prompt. When the selected ledger work requires a live playtest, the supervisor may
  inject only a compact deterministic pointer that tells the coordinator to use that contract and
  name the relevant harness skill; it must not prescribe the test trajectory or police the model.
  The coordinator drafts a short witness charter containing the claim in plain English, what would
  materially prove or contradict it, already-accepted evidence, the current uncertainty, forbidden
  shortcuts or credit, and honest stop conditions. It describes the proof question rather than one
  exact interaction transcript. The coordinator gives that charter to the worker and later judges
  the returned witness against authentic evidence.

  Put worker-facing operating and witness guidance in the CAOL harness skill and expose the same
  compact contract through the live cockpit/TUI only when a playtest is active. The worker owns
  observe, choose, act, repair, rerun when necessary, finish, and witness. It receives a compact
  immutable evidence journal with scenario/source/executable/run identity, native observations,
  dispatched actions and receipts, state deltas, interruptions, cleanup, evidence ceiling, and
  visible contradictions. It writes one evidence-cited witness statement with: the smallest
  supported claim; verdict `proved`, `contradicted`, or `inconclusive`; causal account; cited
  observations and their meaning; material deviations; contradictions; remaining unknowns; and
  recommended accept, continue, repair, or change-strategy disposition. Instruct it explicitly:
  report the smallest conclusion supported; distinguish observation, inference, contradiction, and
  unknown; preserve inconvenient evidence; do not request more proof after the claim is honestly
  settled. The TUI may expose a generic `WITNESS / FINISH` action, but must not force a fixed prose
  template before ordinary gameplay actions can proceed.

  Retain only mechanical truth owners: source, executable, scenario, selection, run and ownership
  binding; immutable native event/action/cleanup receipts; requested-to-executed action correlation;
  append-only final artifacts; citation existence and value checks; contradiction visibility; and a
  ceiling preventing setup, intervention, diagnostic, or focused evidence from earning broader
  credit. The LLM interprets causal sufficiency, semantic equivalence across independent runs,
  materiality of deviations, whether another run is necessary, and what remains unproved. A witness
  cannot generate facts, cite absent receipts, conceal a supplied contradiction, or promote its own
  evidence class.

  Give the coordinator this governing review rule: **intellectual scrutiny, administrative
  leniency; challenge the conclusion, repair the paperwork.** Judge proof by causal evidence, not
  conformity to an expected packet. Before rejecting a witness conclusion, name the concrete false
  conclusion, unresolved causal ambiguity, unsafe action, identity failure, contradiction, or false
  promotion that the missing or malformed information could permit. If none exists, preserve the
  proved result and derive, normalize, annotate, or repair the clerical record without replaying
  gameplay. Never rerun gameplay merely to repair documentation when the immutable evidence still
  contains the required truth. Conversely, polished prose never overrides a contradictory receipt,
  missing authentic identity, or absent causal observation.

  Prove the replacement first through R-018 as the smallest live vertical slice: coordinator charter
  -> worker-controlled raw/primitive/off playtest -> immutable event journal -> witness statement ->
  citation/integrity validation -> coordinator acceptance. The witness may explain expected run-local
  identity differences and semantically equivalent advertised native actions without teaching a
  bespoke matrix every field in advance. Preserve any genuinely necessary inverse control and source
  binding. Once this route proves the same R-018 outcome, delete or demote to optional analytics the
  superseded R-018-specific matrix, duplicated red/yellow/green ledgers, exact run-local cross-run
  equality, forced menu sequence, report-only rerun triggers, and special ingestion policy. Do not
  retain both proof systems. Then adapt R-019 using separate precondition-compatible safe and unsafe
  starts; do not revive the contradictory timed-entry dog route.

  Behavioral tests must allow multiple correct action sequences and witness wording while proving:
  correct causal proof with an optional or derivable field missing is accepted and normalized; a
  semantically equivalent native sequence is accepted; a nonexistent or mismatched citation is
  rejected; an omitted supplied contradiction is rejected; a source/executable/run or ownership
  mismatch is rejected; missing clerical summaries are derived without replay; a genuinely absent
  causal fact remains inconclusive; setup/intervention evidence cannot promote itself; and the
  coordinator receives the routed charter/judgment guidance while non-playtest dispatches do not pay
  for or receive the full playtest manual. Guard and promote the complete same-outcome policy/DFS/
  guidance/harness candidate together, consume this trigger only after the replacement route and
  deletion are proved, request one fresh coordinator, and preserve current authentic run evidence.
  - Disposition: consumed by exclusive review `892e4aaae887`. Immediate recovery replaced the
    active R-018/R-019 matrix gate with chartered immutable witnesses while preserving prior reports.
    Repeatable correction added outcome-sized durable subtasks, nonterminal checkpoints, sidecar
    spokes, playtest-only coordinator routing, and worker/coordinator witness contracts. Focused
    counterexamples prove independent-run semantic comparison, optional-field normalization,
    citation/value and contradiction rejection, identity/digest and ceiling failure, missing-fact
    inconclusion, generic registry persistence, coordinator review separation, and absence of the
    playtest manual from unmarked dispatches. Policy guard digest:
    `b3f1d2e28fb4409173c5f24717f12ccdaa245e14eb8fb9addd50aba1753a6af5`.
