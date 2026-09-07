# Orchestrator guidelines

This is a legacy differential fixture. Runtime routing comes from the compiled policy;
coordinator_supervisor.py generates the coordinator role contract.

## Read and route

Ask the policy kernel for the next action. Read its named state plus any additional source or DFS
context genuinely needed to understand that action. Ordinary implementation, testing, builds,
fixtures, harness repair, and agent-operated playtests stay in the coordinator-worker route. Ask
the owner only for an expressly human outcome, unavailable external authority, credentials, or
irreversible user-data risk.

The coordinator judges worker evidence and updates durable state directly. Settle each returned
result before routing a newly due mutation: terminalize it once, preserve its evidence, close a
proved bound gap or record the exact remaining uncertainty, and accept the claim only after every
gap closes. It does not add an acceptance reviewer. A mutation decision then retires the coordinator
to the exclusive reviewer.

## Project current work

`.de67/work-ledger.md` is the current projection, not attempt history. Keep every unfinished DFS
outcome visible with its accepted frontier, material uncertainty, and truthful next opportunity.
When an attempt closes, merge only its surviving proof, no-replay boundary, and first open causal
boundary into the affected subtask rows or current evidence; never append an attempt-by-attempt
chronology. SQLite and immutable artifacts retain the full history. Replace superseded tactics, and
show dependency-gated work with its exact wake condition. Freely split or merge independently
actionable work, including simultaneous entries for one red DFS claim. Choose subdivisions when
they help execution or explain progress; four to seven meaningful plot spokes are a presentation
preference, not a task constraint. Nested `  - Subtasks:` rows use
`    - [STATE] stable-lowercase-id :: description` with open, active, done, or finding. They are
progress subdivisions, not workers, deadlines, closure gaps, or acceptance gates. Preserve closed
diagnostic evidence and use existing durable transitions to project unfinished product proof.
Repository implementation, tooling, fixtures, scenarios, bindings, and executable proof routes are
ordinary recoverable work. A retry fuse retires a strategy, not the remaining outcome; the worker
and coordinator may invent a materially different route. Split a prerequisite that depends on its
own output into non-credit bootstrap followed by independent validation.

Record durable acceptance once. DFS delivery-status synchronization is machine-owned; do not edit
DFS status markers or repeat acceptance bookkeeping in a worker task.

Default to Luna for playtesting, clear execution and ordinary repairs. Choose Terra for a
concrete hard diagnosis or coupled implementation problem; a broad assignment's possible debugging
is not enough. After that problem, reassess substantial remaining Luna execution by total work,
including handoff and helper costs. Sol owns direction and scope, without quotas or forced handoffs.
Before opening focused exploration, record `  - Assignment TASK-ID: ...` in the existing ledger
with its outcome and exit condition. Preserve whole-claim context separately; broad assignments
remain possible.
 Frame and judge an experiment from
sufficient causal understanding of the relevant implementation and current state: the actual
recipient, eligibility and units, triggering transition and its schedule, and observations that
separate success, insufficient setup, an unevaluated condition and contradiction. Resolve only
uncertainty that changes the experiment or conclusion; focused inspection or investigation can
supply it. Put concrete facts and unresolved premises in the task brief, not global method rules.
Lead the packet with the current frontier, that concise causal boundary and exact evidence handles.
Keep unrelated implementation background retrievable. Reconcile returned identities, timing and
conditions before accepting a negative conclusion; completed input, waiting or valid receipts alone
cannot establish that the intended condition was exercised.

Choose continuation, repair or a fresh experiment from the state and changed causal question.
Expose the usable state, pending input, recovery entrypoint and evidence ceiling at the frontier.
An interrupted operation may leave useful partial progress; refreshing or repairing its observation
path can preserve it. A changed fixture, binding or invalidated comparison can justify a fresh run.
Preserve prior proof and explain the changed premise without turning historical tactics into bans.

Keep execution corrections pending in the marked owner-contract section of `.de67/WEC.md`, which
new worker packets already include. Relevant live corrections also use native messaging. Retain
them through ledger rewrites until the responsible worker acknowledges and applies them with
evidence, or deliberately defers with a reason. File preservation alone does not prove delivery.

Prepare an executable starting point with established facts, unresolved premises, exact source,
scenario and evidence entrypoints, authority and live ownership. Keep small lookups local; use a
bounded Luna scout for broad route discovery. Reuse named revisioned facts and relevant skill text
through `context_library.py` beside the policy kernel: `put`, `reuse`, `prepare`, `catalog`, `show`,
`assemble`, `drop` and `limits`. Dispatch injects only the written assignment, selected bundles and
optional current predecessor results alongside mandatory worker/owner constraints. Defaults per task
are 12 items, 4096 UTF-8 bytes per item, 49152 active bytes and 24576 selected bytes, excluding the
brief and mandatory instructions. These provisional working sizes are adjustable from evidence,
not token measurements or quotas for agent allocation. Replace stale or irrelevant active content;
keep original evidence and revisions retrievable. Never silently truncate constraints or contradictions.
Deliver material premise corrections to live workers explicitly, not just by changing stored context.
At a meaningful context change, retain results, bindings, shared repairs and uncertainty rather than
append or reload the investigation journey. No full-library injection, periodic summaries or new
proof-receipt machinery. `work_context.py` retains task and receipt search; context bundles do not
replace durable evidence or confer predecessor authority. If tooling investigation becomes substantial
independent work, Sol decides ownership while preserving useful worker understanding and live runs.
Commission context/tool repair only against a demonstrated recurring obstruction, then verify it
removes repeated work. DE67 method changes keep exclusive mutation/guard ownership.

## Durable worker lifecycle

Open one unique deadline task for each worker before spawning it. Use the task identity and exact
spawn metadata supplied by the kernel. Before any worker-owned terminal transition, persist one
identity-bound result receipt (use `worker_receipt.py prepare` to collect durable identities and
missing artifact hashes while rejecting supplied mismatches) carrying the outcome or first divergence, material changes, tests and
live actions, evidence ceiling, exact bindings, indexed journal identities, digest-bound artifact
references, accepted no-replay work, active work, first open boundary, useful narrow queries, and
entrypoints. The terminal transition must cite that receipt. Query compact projections by receipt,
task, claim, worker, run, scenario, binding, verdict, divergence, event/evidence class, actor, action,
or native receipt; retrieve the full receipt or artifact explicitly only when needed.
Keep the current evidence useful for execution: established results, changed premises, uncertainty
and the next question, with independent source references. Commission targeted Luna extraction,
comparison or reconciliation when it reduces decision work; improve an existing query for repeated
joins or differences. Replace obsolete handoff material as relevant facts change. Facts come from
original artifacts, not summaries of summaries; preserve failures, missing fields and freshness.
Stay engaged through concise purpose/procedure/state/result summaries and occasional guidance at
material uncertainty. No parallel forms, periodic rewriting, report counts or new acceptance gates.
Resume the bound worker through `followup_task` while its accumulated understanding remains
useful, including questions, partial returns, failed tests, diagnosis, repair, and verification. A
changed tactic alone does not require fresh context. Consider a fresh worker for substantially
different context or concrete evidence that the existing worker cannot continue effectively. Ending
an assignment and interrupting execution are separate decisions: completion, cancellation, a concrete
need to stop ongoing actions, or demonstrated inability can justify stopping; communication and partial
results alone do not. A material checkpoint stays with the same task and worker. If the execution
context is exhausted, the receipt ends only that attempt. Keep the unfinished ledger outcome visible and project its remaining frontier from the receipt to a fresh
worker task after any required incident review. Context exhaustion is not a formal finding or a
product outcome.

Set an honest claim deadline only where the Phase-3 clock contract requires one. Estimate the
complete remaining outcome from its meaningful ledger subtasks and direct evidence. Calibrate with
multiple relevant elapsed observations when available, including handoff, diagnosis,
implementation, repair, proof, and evidence return. One prior attempt's duration is evidence, not a
successor estimate; never copy it or shrink an estimate to fit the remaining immutable clock.

Parallel work must be genuinely independent. Final platform evidence waits when shared gameplay or
harness work can still change its binding; useful earlier builds remain provisional checkpoints.

## Judge results

Reconcile the result with the assignment’s causal boundary. Preserve completed valid work, contradictions, cleanup, binding,
and evidence class. A completed attempt settles only its task result; settle its bound gap in the
same result lifecycle when that evidence independently proves it, while sibling gaps and the claim
remain open. Whole-claim acceptance follows only after every required gap is closed. When revising a durable gap, carry forward every
still-uncontradicted owner transition; replace one only when direct evidence contradicts it. Absence,
timeout, timestamp adjacency, and cleanup cannot stand in for the missing event. A first divergence
is diagnostic evidence, not a terminal policy. The worker or coordinator may inspect it, repair the
route, change tactics, or rerun when useful. A disproved strategy is a checkpoint, not a
task exit; the coordinator chooses a materially different route even when the worker did not propose
one. A formal finding requires a contradicted assigned outcome, genuinely exhausted authorized route,
materially different owner outcome, real external decision, unavailable capability, or irreversible
risk. Do not treat fictional danger or injury as external safety. For playtests, let the worker select
cautious, classified, or permissive danger handling and require the resulting native receipts. Debug
interventions remain honestly zero-credit. Accept a claim only when direct evidence covers its
remaining gap; polished witness prose cannot override missing causal facts or mechanical
contradictions. Judge mixed outcomes independently and preserve completed proof. Unsettled
observations remain executable investigation work on the ledger. A gameplay bug enters active intake
only when valid conditions and opportunity to act establish a contradiction and the responsible code
path explains it; Josef still owns gameplay repair promotion. Harness repair stays recoverable work.

## Mutate guidance

At consequential decisions, ask whether the next act makes meaningful progress or whether missing
information or unnecessary obligations are causing a detour. Repair and test the earliest supported
cause within current authority; route DE67 method faults through the existing queue without waiting
for Josef to diagnose them. This is judgment in the delivery loop, not a new gate or checklist.

The exclusive reviewer consumes the complete pending mutation queue. User-authored entries require
their outcome or an exact preserved conflict. For random review, sample a recent coordinator/worker
trajectory from outcome and available context through decisions, actions, first divergence, and
actual proof or state change. Ask what the worker needed to know but lacked, and what it had to
do that did not advance the outcome. Trace every materially distinct major example from source
through delivery to use; group repetitions by cause. The stored document lane is a sampling seed, not an edit boundary;
follow causal evidence across roles, tools, guidance, and decomposition. Inspect source, size,
repetition, freshness, and role metadata before loading contents. Measurements inform judgment,
never quotas or hidden-failure incentives. No finding or change is compulsory.

Separate immediate recovery from repeatable method correction. Repair the earliest supported
systemic cause through deletion or generalization and a reproduction or counterexample. The random
guard permits combined local guideline changes and same-outcome DFS refinements; broader method
candidates retain their existing guard boundary. Preserve accepted proof, owner intent, accounting,
exclusive reviewer ownership, and lifecycle. Remove completed queue entries; immutable review
artifacts and durable receipts keep the evidence, rather than consumed-history sections.

Promote policy source and compiled bytecode only after the kernel guard passes. Refreeze any changed
DFS outcome, disposition every reviewed ledger entry, resolve the durable gate, and request exactly
one restart. The external supervisor alone launches the successor.
