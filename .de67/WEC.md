# WEC — Affordable playtests, reliable harness and safer subsystem changes

## Current owner reconciliation — 2026-09-20

Josef invoked “ok de67 2 please. merge with existing FS”. The contribution appended
below is additive. Prior gameplay intent, accepted evidence and unfinished regression
work remain binding at their original scope. This invocation authorizes specification
and necessary workspace/tooling preparation only; the stopped gameplay campaign and
Phase 3 stay stopped. Earlier launch authorization below is historical and superseded.

Josef further instructed “no do not preserve that arrangement but clean it up”, “its
FS not DFS”, and “yes get rid of DFS.md and replace with FS.md please in the de67
tooling too”. `.de67/FS.md` is the single functional specification. Remove the pointer
arrangement and reconcile current tooling and references; preserve historical receipt
identities and prior requirements without retaining an obsolete file wrapper.

Josef instructed “get rid of that dev worktree too. everything in the worktree should
be on dev please”. Final workspace is the main C-AOL checkout on `dev`; transfer this
phase's work and relevant local state there, then remove its temporary dev worktree
without losing unrelated files or changing the preserved old archive.

*User intent and language brief — Phase 1 draft, 2026-09-19*

## Owner authorization for Phase-2 import and reconciliation

Josef invoked `de67 2`, confirmed this exact Mac C-AOL dev worktree, and explicitly
approved preserving the previous input by archive or merge, whichever is practical.
Use the archive route: preserve the existing imported WEC and pre-refreeze
specification/pointer verbatim together in the existing history convention, verify
that preservation, then move this local WEC into `.de67/WEC.md`. This explicitly
resolves the differing-WEC import gate. Carry still-binding prior gameplay intent
and accepted evidence into the revised specification; do not discard acceptance
history or grant old proof credit for new obligations. Archive only the superseded
brief/specification material, preserving unrelated files, live state and evidence.
Complete Phase 2. Latest owner instruction: "when done start de67 3, so remove blockers for de67 2 yourself pls, and dont waste too many tokens on short waitcalls, its no hurry, i just wanna afk". Resolve routine reversible technical and setup blockers autonomously while preserving the product intent and existing evidence. After the frozen, prepared and checkpointed Phase-2 handoff, the invocation owner is authorized to start Phase 3. Earlier statements withholding Phase-3 launch below and in archived input are superseded by this instruction; optional paid experiments still require their explicit configuration and prerequisites.

## User outcome

Continue C-AOL zombies-and-light work while reducing the time and tokens spent on
playtests that are unnecessarily difficult to set up or depend on rare chance events.
Josef wants a short explanation of each remaining test: what we need to see and the
simplest credible way to see it. Discuss the setup before commissioning more play.

This is an addendum to the accepted zombies-and-light intent in `.de67/WEC.md`, not
a replacement for its gameplay goals or existing accepted evidence.

## Settled decisions — six efficiency improvements

Josef promotes all six items from the human maintenance queue into this WEC for
subsequent specification and Phase-3 delivery:

1. **Wait without repeated reasoning turns.** Keep pending-response waiting within
   one tool execution until useful progress, failure or the task deadline. Preserve
   responsiveness and never replay an already submitted action.
2. **Use the observation already returned.** Avoid a fresh look when the last action
   supplied the current valid frame and needed facts. Retain refresh/recovery when
   the frame is missing, stale or no longer belongs to the current input state.
3. **Read the relevant evidence fields first.** Filter by known run, actor, request
   or event and return only fields needed for the question. Preserve original
   evidence and recoverable handles; omitted fields are not absent evidence.
4. **Use Jev for semantic judgment where useful.** Exact IDs, failure flags and known
   fields use deterministic retrieval. Telescope should help distinguish competing
   explanations, not add a model call to routine moves or known-field lookups.
5. **Give Telescope a useful candidate pool.** Narrow and deduplicate equivalent
   evidence before selection, while retaining distinct observations, late relevant
   records and contradictions. Report truncation and missing coverage honestly.
6. **Resume without reconstructing everything.** Carry a small continuation note
   with session/binding, pending request, relevant evidence handles, unresolved
   question and next decision. Reuse static controls guidance while rechecking
   current action authority and freshness.

Success means lower total agent/provider effort for comparable completed outcomes
with equally trustworthy evidence. Count Jev usage and follow-up retrievals as well
as fresh agent tokens; smaller output or slower work alone does not prove savings.
The earlier live Jev trial consumed 16,317 provider tokens and did not establish a
net saving. Prefer existing working mechanisms over duplicate retrieval layers.

## Settled playtest scope — stalker follows into a city

Current ledger lookup: `R-ZL-PLAYTEST` is the only unchecked zombies-and-light item.
Its previously specified remaining gap is natural stalker opportunism: a naturally
present stalker chooses a visibly bad moment during ordinary play and acts on it. Focused
light, stalker, rider, lifecycle, evolution, band, encounter and sprite claims are
already accepted. The broader integrated requirement names four accounts, but this
ledger does not individually enumerate their acceptance; do not reopen all four
merely from that list. Latest recorded continuation also needs a valid ordinary
scenario/charter/profile route; the activity-resume repair is already accepted.

Josef's decision on 2026-09-19 replaces the requirement to find a naturally spawned
stalker for this remaining behavioral playtest. Use this sequence:

1. Spawn only the stalker through the debug menu.
2. Move the player around through ordinary gameplay and observe whether the stalker
   follows. Establish actual stalking movement, not merely successful spawning.
3. Walk into a city and encounter its naturally present zombies; do not debug-spawn
   an accompanying zombie group.
4. As those zombies pressure or distract the player, observe whether the same
   stalker exploits the opening and attacks. Let its AI choose the approach and
   attack; do not inject attention, contact or an attack outcome.

This preserves the connected stalking-to-opportunistic-attack experience while
removing the expensive search for a rare stalker. Record the debug spawn honestly:
the run proves behavior after setup, not natural stalker occurrence or prevalence.
Existing accepted encounter and focused-mechanism evidence remains accepted.

Preparing or repairing the harness route needed for this test is useful and in
scope for subsequent delivery. Carry this owner decision into the scenario and
acceptance wording so the old natural-stalker setup restriction does not block it.
No new run quota, zombie-count threshold or wider replay campaign is chosen here.

## Additional settled workstream — C-AOL regressions and cleanup

Josef's attached brief requests completed implementation slices on
`josihosi/Cataclysm-AOL`, branch `dev`: protect the boundaries between zombies,
predators, riders, bandits, physical light and reality-bubble simulation, then use
those tests to support small behavior-preserving refactors. Fix confirmed defects
with targeted regressions. This is future delivery work, not merely another review;
the current Phase-1 conversation records its intent without starting implementation.

The named symbols and suspected defects below are investigation leads from an older
static review, not verified present defects. Phase 2 must inspect current code,
existing equivalent tests, repository instructions and build/test routes. Record the
actual starting commit at implementation time. Preserve unrelated changes; do not
reset, force-checkout, destructively clean, recreate removed code or duplicate tests.

This C-AOL workstream must not modify de67 or implement Jev Telescope. The previously
promoted six efficiency improvements remain a separate workstream with their own
ownership; they are not incidental cleanup within these product changes.

### Delivery order and protected behavior

1. **Player-tile light regression and reliable fixture cleanup first.** Investigate
   `collect_stationary_emitters()` and `index_loaded_z_sources()`: ground-item
   deduplication may also skip stationary emitters beneath the player. Through the
   actual loaded-source indexing path, use a walkable emitter fixture and move the
   player beside it, onto it and beside it again. Check the same absolute emitter
   location remains indexed; add a ground lamp and verify exactly one occurrence.
   Confirm any failure before changing production behavior. Inspect
   `physical_light_stationary_records_are_source_bound` and other touched tests;
   use existing scope cleanup or a small guard for temporarily changed terrain,
   furniture and global definitions, including assertion-abort paths.
2. **Repeated real handoffs with save/load.** Reuse authoritative transition APIs
   and fixtures for ordinary zombies, durable predators/riders and bandit groups
   where ownership differs. Begin without combat or unrelated resource changes;
   cross the reality-bubble boundary both ways repeatedly and insert save/load.
   After each transition check one authoritative owner per surviving actor, no loss,
   duplicates or stale resurrection, durable identity, required state and correct
   inventory/resource accounting. Repeating a completed handoff must not transfer or
   charge twice. Cover health, ammunition, group membership and operation state
   where required, allowing legitimate time-driven abstract changes.
   Exercise a blocked member of a departing bandit pair, destination invalidation
   before commit, death during pending departure, failed-transfer retry, and
   overlapping light/sound/boundary events. Verify the existing partial-transfer or
   rollback policy; do not invent one. Event order may change tactics, but not
   ownership/accounting coherence. Constructing a desired final state is not proof
   that the real handoff works.
3. **Turn-driven light lifecycle.** Expose a real source, advance turns and observe
   eligible recipients receiving clues and reacting through existing rules. Hide or
   extinguish it and advance through retention/expiry. Protect sampling/delivery
   cadence, cessation of fresh observations, legitimate memory persistence and
   expiry, and occlusion/eligibility boundaries. Inspection/redraw without elapsed
   game time must not create exposure or refresh memory. Include a bubble or
   save/load variation where practical; distinguish history from fresh knowledge.
4. **History and coordinate lifetimes.** Investigate stalker pressure memory,
   `writhing_stalker_pressure_memories` samples and rider relationship maintenance.
   Replace actors at roughly constant population, shift the bubble origin while
   absolute positions stay fixed, save/reload, and start a separate world in one
   process if supported. Check identity, absolute position, observation age,
   ownership and relationships rather than identical AI decisions. Old identities
   must not affect replacements and transient state must not leak between worlds.
   Clarify memory ownership/expiry, eviction and historical coordinate semantics
   before changing retention policy; a policy change needs its own regression.

### Cleanup supported by those tests

- Share genuinely duplicated physical-light discovery rules while keeping ground
  item deduplication distinct from stationary emitters and preserving provenance.
- Move cohesive implementation out of `do_turn.cpp` while leaving turn ordering and
  cadence clear. Investigate rider reconciliation inside
  `sample_and_deliver_live_light_for_advancing_turn()` without changing its order or
  frequency accidentally.
- Investigate moving `openclaw_harness_r022_item_spawn_bridge()` setup out of ordinary
  turn code. Give one-shot state the proper scenario/run lifetime; preserve setup
  receipts and the distinction between interventions and gameplay evidence.
- Consolidate only identical bandit eligibility rules into narrow named queries.
  Preserve operation-specific checks and ownership, preflight, commit and rollback;
  avoid a generic helper controlled by many Boolean switches.
- Separate lengthy diagnostics from movement decisions where useful. Preserve
  evidence fields and disabled-logging behavior; label timing according to the work
  actually measured, not the entire planner when only destination selection is timed.
- Fix confirmed memory-lifetime issues with the smallest suitable change. No general
  memory framework or unrelated state-management layer. Keep mechanical extractions
  separate from intentional behavior changes so each is reviewable.

### Mandatory evidence for every cleanup

Josef explicitly requires all delivered cleanups to be tested, including mechanical
extraction and test-fixture cleanup. Each must map to named, executed tests of the
affected behavior. Compilation alone, unchanged signatures or a claim that code
was merely moved is insufficient. Reuse adequate existing tests; add or extend
coverage where a real gap remains, not one new test per helper by ritual.

| Cleanup | Required observable evidence |
|---|---|
| Fixture restoration | Temporary definition changes are restored on normal exit and assertion-abort/unwinding. Subsequent tests see the original definitions; repeat/shuffle the affected tests where supported. Exercise failure cleanup safely without leaving a deliberately failing test in the normal suite. |
| Shared light discovery | The real loaded index retains the stationary emitter as the player moves beside/onto/away from its tile, indexes the ground lamp exactly once and preserves source identity/location. Exercise the formerly separate discovery routes being consolidated with the same relevant fixtures. |
| Extraction from `do_turn.cpp`, including rider reconciliation | Through the actual turn route, the affected sampling, delivery and reconciliation retain their intended order and cadence. Advancing turns produce the expected observations/state transitions; non-time-advancing inspection/redraw does not produce extra sampling or refresh. Test an order-sensitive consequence wherever the extracted responsibilities depend on one another. |
| Harness setup relocation and run lifetime | Repeated calls within one scenario do not duplicate setup; a subsequent scenario/run in the same process gets its own correct setup lifecycle. Cover supported failure/retry/termination paths, preserve setup receipts and distinguish intervention from native gameplay evidence. Phase 2 must establish the actual lifecycle/retry contract before defining expected outcomes. |
| Shared bandit eligibility queries | Exercise each affected operation through its real caller with eligible and ineligible cases. Preserve operation-specific safety checks and cover blocked pair members, failed preflight, retry and the established partial-transfer/rollback outcome, with no duplicate ownership or resource charge. |
| Diagnostic formatting extraction and timing labels | With equivalent controlled inputs, logging enabled/disabled preserves gameplay decisions and state. Required evidence fields retain their meaning; disabled logging avoids diagnostic-only work/side effects. Timing labels identify the instrumented interval accurately; do not assert exact prose or machine-dependent duration. |
| Memory-lifetime cleanup | Actor replacement, absolute-position-preserving origin shifts, save/load and supported same-process world changes preserve required identity, position, age and relationships without stale-identity effects or cross-world leakage. Intentional retention/expiry changes have a separate behavioral regression. |

Mechanical refactors should have relevant behavior tests passing before and after
the change. A confirmed defect instead needs the targeted regression to fail against
the previous behavior and pass after its fix. Keep those evidence types distinct;
do not demand an artificial failing test for a behavior-preserving extraction.

## Deferred experiment — Pit Crew coordination notices

**Sequence chosen under Josef's delegated discretion:** finish the agreed production
development, efficiency, cleanup and stalker-playtest acceptance first; then evaluate
Pit Crew, followed by Reflex Pilot. Pit Crew can begin with recorded events and its
funding guard may be reusable by other Jev integrations. This is an experiment, not
a prerequisite for completing production work. A negative usefulness result does
not block Reflex Pilot or require an indefinite Pit Crew improvement campaign.

### Goal and packaging

Telescope answers “Where should I look?” Pit Crew answers “Has something happened
that should change what I am doing?” Reduce wasted work when new evidence,
overlapping investigations or changed assumptions matter to active work. Pit Crew
assists existing coordination; it is not a supervisor or autonomous project manager.

Inspect current de67 packaging, orchestration contracts and optional integrations
before choosing integration points. Deliver a working, tested slice inside the
normally installable/copied de67 skill, including implementation, configuration and
activation instructions. A standalone service merely linked from SKILL.md is not
the requested package. Keep dependencies isolated and the plugin off by default;
base installation/use requires no Jev credentials, external calls or unnecessary
background process. Disabled, unavailable or unfunded Pit Crew must leave ordinary
orchestration operational. Reuse suitable Telescope adapter/configuration/provenance/
budgeting facilities without requiring Telescope to be enabled or building a new
general framework.

### Event-to-notice behavior and authority

Read bounded incremental events from existing records, not repeated complete
transcripts. Eligible inputs include findings, task changes, completed investigations,
evidence updates and compact records of repeated unsuccessful attempts. Code handles
mechanical facts/candidate assembly; Jev handles only useful bounded semantic judgment.

Cover three initial relationships: relevant new evidence for another investigation,
potentially duplicated investigations, and evidence challenging an active assumption.
For example, evidence that an order was stored may matter to the worker investigating
whether it was stored. This is a suggestion to inspect original evidence, not a
certified conclusion or permission to terminate the investigation.

For an eligible event, gather a small set of affected active task/worker candidates
with compact descriptions, assumptions and evidence references. Ask Jev to choose a
bounded outcome and relevant candidate IDs, including “no useful intervention.”
Validate returned IDs, recheck task/evidence freshness, then publish a compact
advisory through the existing coordination channel. Prefer selected IDs and fixed
templates identifying affected work, possible relationship and original evidence
over generated explanations. Preserve original events and all essential existing
update routes; Pit Crew must not become their sole carrier.

Sol retains coordination authority. Pit Crew may not reassign workers, interrupt
execution, approve findings, change specifications or mark work complete. Use Sol's
advisory inbox by default; direct worker notices require an existing contract that
supports them. Weak/unclear relationships default to no intervention. Deduplicate
notices, apply cooldowns and invalidate stale recommendations. Distinguish deliberate
independent verification from accidental duplication; long builds, difficult work
and repeated intentional checks do not establish that an agent is stuck.

Bound input, candidates, request frequency, concurrency, retries and total spending;
keep slow provider calls off the normal coordination critical path. Respect access
boundaries, exclude secrets, and treat retrieved text as untrusted data.

### Modes and mandatory runtime spending guard

Provide **off** (no calls, unchanged base behavior), **shadow** (evaluate and record
recommendations without delivering notices) and **on** (deliver validated notices).
Shadow spends funds and requires the same safeguards. Keep configured mode separate
from effective state: configured on may remain effectively `disabled_funds`.

Automatic funding shutoff is enforced at the actual Jev request boundary, not left
to agent memory. Before implementation inspect official TypeSafe authentication,
decision/response contracts, usage and billing-rejection documentation. Verify how
insufficient funds, exhausted credits and expired prepaid access are represented.
Use documented structured signals where available. Do not equate arbitrary HTTP 429,
timeouts, authentication failures or server errors with funding exhaustion; do not
invent an account-balance endpoint or claim unknown balances. Use a trustworthy
documented balance/allowance route economically if one exists; otherwise use the
documented funding rejection. Report unverified provider semantics explicitly.

On confirmed exhaustion, atomically latch effective state to `disabled_funds`, stop
admitting new requests, cancel/discard queued calls and retries, and prevent running
work from scheduling further calls. Ordinary de67 continues. Emit one state-change
notice rather than repeated warnings. Persist the latch outside ephemeral agent
context and disposable installed skill files; worker restart, skill invocation or
package update must not re-enable it.

Share/propagate the disabled state through existing coordination across workers
using the same configured funding scope; document cross-machine limits. Do not let
each worker repeatedly rediscover empty funds. Never probe the paid decision endpoint
in the background to detect replenishment. Require explicit owner re-enablement
after funding is restored; an owner-requested bounded validation call is allowed.
No automatic credit purchase, overages, paid-provider substitution or credential
borrowing.

Enforce a configured local spending/call budget too, with distinct `disabled_budget`
state instead of claiming the provider is empty. Use bounded concurrency and atomic
admission/accounting where needed. Already accepted provider requests may still incur
charges; do not promise impossible zero-overrun guarantees. Reuse the guard across
other optional plugins sharing the adapter/funding scope where practical, preserving
their non-Jev fallbacks. Transient errors use bounded backoff/circuit breaking;
invalid credentials have a separate configuration/authentication state with no futile
request loop. Discard malformed outputs, unknown IDs and stale recommendations;
base orchestration is the fallback, never an invented finding or another paid model.

### Tests, evaluation and delivery

Ordinary tests use provider stubs with no credentials/network. Cover all three notice
cases and no-intervention; unknown IDs; stale tasks/changed evidence; deduplication
and cooldown; legitimate long work; off/shadow/on isolation; funding exhaustion;
persistence across restart/update; queued/concurrent callers and shared shutdown;
no automatic re-enable/background funding probes; explicit owner re-enable; distinct
rate-limit/authentication/timeout handling; and accurate local budget exhaustion.
Verify normal de67 operation after every plugin failure and installation through the
normal skill packaging path. At the actual API boundary, request counters must prove
that no new calls dispatch after the funding latch is observed; testing a label
helper alone is insufficient.

Start with shadow evaluation on recorded/controlled orchestration events. Measure
useful notices, irrelevant interruptions, missed relationships, request volume,
latency and total provider usage. Only with explicit configuration and funds run a
bounded enabled experiment. Compare overall work and outcome quality, including
extra agent effort induced by bad notices; fewer worker tokens alone is not a win.
Stub success does not prove live usefulness.

Deliver focused code/tests, packaged documentation, activation/deactivation and budget
instructions, and the exact recovery procedure after topping up Jev. Report changed
files, executed commands/results, provider uncertainties and experimental limits.
The first milestone is a normally packaged optional plugin producing a small number
of useful evidence notices, failing harmlessly, and reliably stopping requests when
provider funds or its local budget are exhausted.

## Deferred experiment — Jev Reflex Pilot

**Status: deferred follow-on; its prerequisites have not been verified complete.**
Before implementation, confirm acceptance of the currently agreed six efficiency
improvements, revised stalker playtest, and C-AOL development/regression/cleanup
slices. “Everything done” means that agreed scope, not every repository TODO.
Phase 2 must name those prerequisite slices and the acceptance evidence needed to
release this experiment. Keep it separate from unfinished production work; current
work does not depend on the experiment succeeding.

After that gate is met, implement the smallest useful optional integration on an
isolated branch/worktree following repository conventions and preserving unrelated
changes. Keep it disabled by default. Do not automatically merge it, replace the
default playtester or promote a successful demo into default behavior.

### Goal, roles and scope

Test whether Jev can perform routine native playtest decisions with less total
reasoning-agent work while preserving meaningful coverage and trustworthy findings.
The reasoning agent defines objectives, constraints and checkpoints. Jev selects
one valid next action or abstains. The existing harness executes and observes;
independent assertions or review decide whether the feature worked. Jev is the
**Reflex Pilot**, not the test designer, bug adjudicator or source of gameplay truth.

This experiment is confined to the C-AOL harness on `dev`. It is separate from the
Telescope evidence selector and must not implement NPC intelligence, the de67
Telescope or a general agent framework. Inspect the actual current harness/player
interfaces, pending requests, setup, evidence and checks before designing the slice.
Reuse those interfaces. Consult current official Jev/TypeSafe documentation for
authentication, supported decision formats, response semantics, limits and usage;
do not assume generic chat or unrestricted generated JSON.

### First slice and execution contract

Choose one already-qualified scenario with a clear objective, small native action
surface, repeatable setup and existing evidence/assertions for meaningful progress.
Do not add game features merely to demonstrate the integration.

The loop obtains the current player-visible observation/actions, builds a bounded
request with objective, relevant recent outcomes and valid candidates, asks Jev to
select or abstain, validates against current state, executes through the native
interface, waits for the result, records the actual outcome, and continues until
a checkpoint, completion, failure or budget limit.

- Each candidate is a complete action including its target and required parameters,
  using existing stable IDs/handles. Do not choose action and target independently
  and accidentally create an invalid combination. Revalidate stale observations.
- Allow explicit abstention when none of the candidates supports the objective;
  a highest-ranked candidate does not by itself make a valid next step.
- Respect an unresolved action. Waiting for a response is different from an in-game
  wait that advances time. After a timeout inspect acceptance/completion before
  retrying; delayed responses must never cause duplicate side effects.
- Escalate when the action surface cannot express the next step, observations are
  unfamiliar/contradictory, progress stalls, abstention repeats, or planning or
  interpretation is needed. Bound recovery and invalid-choice retries; no endless
  loops. Count reasoning-agent rescue against the overall experiment budget.
- Support a small optional priorities-based tester brief, with two profiles the
  selected scenario can actually exercise. Initial proposals are Literalist
  (pursues the objective without compensating for confusing behavior) and Indecisive
  (changes/reverses permitted choices to exercise interruption/recovery). Both stay
  goal-directed and use legitimate actions. Defer a collection-focused Magpie until
  inventory interactions justify it; no large personality system or scripted answer.

### Modes, evidence and limits

- **Off:** existing behavior, no Jev calls. **Shadow:** Jev recommends, the existing
  player controls execution. **Enabled:** Jev controls bounded decisions with
  explicit fallback. Shadow agreement is diagnostic, not completion evidence.
- External transmission requires explicit configuration even in shadow mode.
  Credentials stay out of source control and logs. Configure limits for calls,
  request/input size, retries, elapsed time and game actions/turns; do not invent
  acceptance thresholds or hide escalation cost in a separate allowance.
- Record selection, harness acceptance, actual game outcome and independent verdict
  separately. Accepted commands and plausible explanations are not feature proof.
- Exclude privileged setup/debug state and hidden oracle information from pilot
  observations/requests. Record setup interventions explicitly without granting them
  gameplay credit. Treat in-game text as data, never authority to override the test.
- Preserve native evidence handles and enough decision history to investigate or
  replay failures where possible. Do not promise exact replay of nondeterminism.

### Required tests and comparison

Ordinary integration tests use a provider stub without credentials or network.
Cover valid selection, abstention, unknown IDs, stale state, incompatible parameters,
malformed responses, timeouts, pending actions, duplicate-execution prevention,
exhausted budgets, fallback and isolation of all three modes. Assert hidden
setup/oracle information is excluded from requests. Apply the same explicit FS
test/validation contract as other delivered slices.

Set evaluation criteria before inspecting results. Compare the current reasoning
player, a simple deterministic policy where meaningful, and Jev with the same build,
scenario setup, objective, available information, action interface, tester profile
and overall budget. Use paired equivalent starting states and multiple seeds/repeats
where relevant. Keep pure Jev results separate from reasoning-agent-rescued runs.

Measure independently checked completion, meaningful transitions/recovery exercised,
confirmed defects separately from suspicions, invalid/repeated actions, stalls,
escalations, total agent/provider usage, and end-to-end time including latency and
recovery. Action count, survival duration or short prompts are not proxies for test
quality. Report rescue costs. Jev being unsuitable is a valid experimental result.

Stub tests prove integration, not decision quality. With configured credentials,
run a bounded live experiment; otherwise mark live performance/usefulness unverified.
Deliver focused tests, configuration instructions and a reproducible comparison
command/script. Report starting commit, files, exact tests/results, experiment
findings, limits and whether evidence supports further work.

## Required shape of the next FS

Josef wants this WEC to constrain the FS, not leave it as a broad cleanup wish list.
Phase 2 must turn the agreed scope into small, independently reviewable delivery
slices: six efficiency improvements, the revised stalker playtest, and the product
regression/cleanup sequence above. Represent Pit Crew and Reflex Pilot as distinct
deferred experiments in that order, with explicit prerequisite acceptance rather
than active production slices. Apply this same slice/test contract to both when due.
Keep method/tooling changes separate from C-AOL cleanup ownership. Use the existing
FS structure rather than create a parallel plan.

For every slice, specify:

1. **Outcome and current evidence:** the WEC requirement it serves, current-code
   findings, existing coverage, and whether the lead is confirmed, dismissed, already
   fixed or already covered. Do not turn an old suspicion into a mandatory code edit.
2. **Concrete change boundary:** verified files/symbols and responsible state owner;
   existing versus proposed functions; inputs/outputs, coordinate spaces/units,
   callers and real integration entrypoints. Resolve technical choices from the
   current code instead of handing the worker an unchosen list of designs.
3. **Mechanism and invariants:** before/after flow, turn ordering/cadence, ownership,
   persistence and lifetime, applicable failure/retry/rollback behavior, and what
   must remain unchanged. Explicitly label any intentional behavior change.
4. **Executable proof:** named existing or proposed tests, fixture/setup, action or
   transition sequence, independent observable assertions and failure cases. Map
   every delivered cleanup to its evidence row above. State which real production
   route the test exercises; do not substitute manually constructed final state.
5. **Validation and completion:** exact build/test commands and selectors verified
   against the repository, relevant platform coverage, how execution is confirmed,
   and the evidence required to close the slice. Separate narrow regression tests,
   integration proof, the agreed gameplay account and optional performance work.
6. **Order and exclusions:** prerequisite tests/fixtures, shared-file conflicts and
   the smallest complete delivery boundary. Preserve the requested priority order
   and accepted evidence. Name deferrals and unresolved owner choices explicitly.

A worker should not have to invent what “refactor safely,” “test the lifecycle” or
“preserve behavior” means. Mechanistic detail must make the real behavior and proof
clear without forcing tests to mirror private implementation. A cleanup with no
adequate executable coverage remains unverified, not complete. Do not expand this
into a full redesign or replay accepted playtests merely to fill specification rows.

### Performance, validation and delivery evidence

Where existing instrumentation makes this inexpensive, establish reproducible
baselines/work counters for blocked returning bandit pairs, rider reconciliation as
population grows, light discovery with much loot, predator ownership checks against
stored populations, and repeated handoffs/long-lived memory. Separate candidate
scans, pathfinding, discovery and delivery where possible. Prefer deterministic work
counts in ordinary tests and optimized timing in an explicit benchmark target; no
machine-specific timing assertions, invented limits or speculative algorithm rewrite.

Use existing conventions/dependencies, deterministic fixtures and bounded sequences.
Randomized tests must report seed and failing sequence; expensive soak tests remain
opt-in. Assert independent outcomes, not a copy of the algorithm, private helper
names or exact diagnostic strings. For confirmed bugs, safely demonstrate failure
against previous behavior and success after the fix without disturbing the tree.

Build affected targets; run focused and related suites plus an appropriate integration
subset. Verify that filters actually execute tests. Repeat affected tests and shuffle
order where supported to catch shared-state contamination. Preserve assertions and
expected behavior; distinguish pre-existing failures and exact environmental limits
from regressions, and never claim unexecuted tests passed.

Delivery reports must name starting commit/files, disposition of each investigated
lead (confirmed, dismissed, already fixed or covered), protected behaviors, mechanical
versus behavioral changes, exact commands/results and deferred or unverified gaps.
Prefer a finished tested slice over a broad unfinished redesign. The aim is fewer
duplicated rules, explicit ownership/lifetimes and reliable subsystem boundaries,
not a larger test count or smaller files for their own sake.

## Handoff to DE-67-2

- Product: `/Volumes/CodexBulk/Schanigarten/workspaces/Cataclysm-AOL-hostile-ecology-dev`
  on the Mac mini; branch `dev`, observed HEAD `c2ad7514a3`. Recheck before delivery.
- Method source: `/Volumes/CodexBulk/Schanigarten/workspaces/de67-lab`;
  installed skill: `/Users/josefhorvath/.codex/skills/de67`.
- Current acceptance: `.de67/work-ledger.md`; canonical specification `.de67/FS.md`;
  prior intent `.de67/WEC.md`. The current owner correction above replaces the old pointer route.
- Specific maintenance details and evidence: de67 source
  `de-67-3/agents_ignore_todo.md`, section “Playtest token consumption”; audit
  `docs/token-audit-2026-09-16.md`; adapter `integrations/jev_telescope/`.
  Josef explicitly requested carrying these six items forward; this does not import
  the unrelated human backlog into delivery.
- Phase 2 should reconcile this addendum with the existing FS and encode the six
  improvements, agreed stalker-only debug setup, and separate regression/cleanup
  workstream as actionable Phase-3 work. Preserve the requested regression order.
  Include Pit Crew and Reflex Pilot separately as gated follow-on experiments in
  that order, with the optionality and acceptance boundaries specified above.
  Sol/workers receive the resulting specification and relevant task context; do not
  restore routine full-WEC injection into Phase 3.
- Full owner regression/cleanup brief:
  `/Users/josefhorvath/.codex/attachments/84f1fab6-9abe-41d9-a696-5f96048efedf/Pasted text.txt`.
- Full owner Reflex Pilot brief:
  `/Users/josefhorvath/.codex/attachments/4a15031b-2618-4787-aab2-68d3eadb35b6/Pasted text.txt`.
- Full owner Pit Crew brief:
  `/Users/josefhorvath/.codex/attachments/ef1dff1d-1f60-46a6-9c2a-90b1e5e30d76/Pasted text.txt`.
- This discussion and saved draft do not launch Phase 2/3, rewrite current acceptance,
  restart workers, change live tests, or authorize unrelated gameplay work.

<!-- DE67:OWNER-CONTRACT:BEGIN -->
Josef invoked DE67 2 on 2026-09-20 to merge the lean/reliable harness contribution into the
existing FS. Preserve prior product intent, accepted evidence and unfinished regression work.
Remove obsolete harness machinery in later delivery as specified; Git is its archive. Use FS.md
as the single specification in project and tooling, with no DFS.md pointer. Finish this phase on
dev in the main C-AOL checkout, remove the temporary dev worktree after safe transfer, checkpoint
the authorized changes and push dev to origin/dev. The older preserved archive remains untouched.
DE67 3 and gameplay remain stopped. Do not resume autonomous implementation, dispatch, automatic
review/restart or the prior native campaign without a new explicit owner start.
<!-- DE67:OWNER-CONTRACT:END -->

# WEC

*User intent and language brief — lean and reliable C-AOL playtesting*

## User outcome

Lean, reliable C-AOL playtesting: the LLM decides what to test and interprets what happens; the harness handles execution, waiting, bookkeeping and recovery. Routine playtesting must not require reconstructing request machinery or writing temporary helper scripts. Josef describes the current harness as “like a car that keeps falling apart” and wants it in top shape, including removal of old machinery.

## Intended experience

1. One coherent playtesting experience. An agent can find or prepare a suitable scenario, start or resume it, inspect the game, act, wait, save/reload, and finish. Scripted scenarios and interactive play obey the same execution rules. Preparing a new test mostly means describing its setup and intended observations.
2. Actions report what actually happened. Distinguish pending, completed, interrupted and failed. Starting a wait, receiving an input acknowledgment, or exhausting a wall-clock timeout cannot establish completion. Report actual game-time progress separately. If the outcome is uncertain, retain the request and explain the uncertainty without automatically repeating the action.
3. The harness owns routine waiting. It continues polling through ordinary progress and returns control when the requested outcome occurs, a decision is needed, cancellation arrives, or a concrete failure occurs. Interruptions expose the current prompt and available responses. A dead process or stuck operation produces a useful explanation rather than endless polling.
4. Every response supports the next decision. Return a compact account of the action outcome, current input availability, relevant game state and valid next actions. Include save completion and reload readiness when relevant. Keep detailed evidence retrievable separately. After interruption or a fresh agent handoff, recover the current situation without reconstructing a conversation or scanning huge logs.
5. Retries and lifecycle become ordinary operations. Collecting a request retrieves that request. Resuming a session recovers that session. Starting another run is a distinct action with fresh launch authority. Save, quit and reload have observable completion conditions and preserve the intended saved world and run history.
6. Remove the accumulated scaffolding. Candidates include duplicate wait/poll/retry implementations, sleeps used as completion conditions, superseded launch wrappers, scenario-specific helpers that duplicate general operations, obsolete scenarios and fixtures, and stale instructions. Consolidate overlapping tests while preserving their distinct failure cases. Exact deletions require checking current consumers; age or filename alone is insufficient.
7. Prove the experience and the internals. Representative playtests work through the documented interface without inventing helper scripts, manually editing session files, or repeatedly asking whether a wait finished. Tests exercise actual polling and scenario execution paths, including delayed completion, interruptions, cancellation, process failure, duplicate collection and save/reload.

## Settled deletion policy

Breaking obsolete harness commands and scripts is acceptable. Migrate scenarios that still serve a useful playtesting need; delete superseded scenarios, helpers, fixtures, tests and documentation. Remove replaced execution paths as part of delivering their replacement. Do not retain compatibility wrappers, fallback chains, or legacy directories merely to keep old code alive. Git is the code archive. Preserve user saves and valuable runtime evidence; that does not require preserving the machinery that produced them. Retention requires a clear current purpose rather than hypothetical future usefulness. Tests protecting obsolete implementation details may be deleted; distinct behavioral protections follow the supported path. A prettier interface layered over the same accumulated machinery does not satisfy this brief.

## Proposed sequencing and validation intent

Establish reliable action completion through the existing failing route; migrate useful callers onto that behavior and delete their replaced machinery; complete the compact state/resume experience; validate the complete journey and remove stale guidance. Each slice leaves a working harness. This sequence is a hypothesis, not a constraint on a better code-grounded design.

Native acceptance examples include the original bandit long wait, an interrupted wait requiring judgment, and save → quit → reload → continue. Automated tests cover failure combinations; platform checks follow changed paths. A preceding read-only assessment ran cockpit_raw_wait_test, cockpit_keep_watch_test and r008_natural_wait_completion_test: 56 tests passed, but largely with simulated frames and a substitute completion callback. This is existing narrow evidence, not native end-to-end proof or a required fixed test count.

## Boundaries and handoff

Target the C-AOL dev harness. Merge this contribution with the existing FS/DFS and retain unrelated product requirements and their evidence ceilings. Produce a concrete keep / consolidate / migrate / delete inventory tied to current consumers and these outcomes, rather than an isolated patch list. This is specification authorization, not phase-3 implementation or automatic restart of the stopped gameplay campaign. Shared state ownership, existing registry/evidence obligations, and the supported platform routes must remain honest while obsolete implementations are removed. No arbitrary deletion quotas, scenario-count caps, timing promises, or ritual replay of the full historical gameplay campaign.

Relevant baseline: dev 4ad0fd67d7896bb07fcc782a704f0bbf6077a315 and its .de67/manual-handoff.md. Existing active .de67/FS.md, .de67/DFS.md, .de67/WEC.md and durable state must be reconciled with this additive user request. The earlier 1–3-day estimate concerned four narrow fixes; this broader consolidation/removal scope needs its own code-grounded assessment.
