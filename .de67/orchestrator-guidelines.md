# Orchestrator guidelines

This workspace-local file is active mutable policy. Read the sections needed for the current route.
Do not replace it with the packaged template after bootstrap.

## Read and route state

Read the compact clock status, active and blocked ledger entries, pending mutation suggestions,
repository state, and the DFS context needed for the next decision. DFS slices are a token-saving
index, not an access prohibition. Read more of the DFS when the decision genuinely needs it. Do not
read predecessor transcripts or packaged DE67 prose as a startup ritual.

Keep ordinary implementation, testing, fixture construction, scenario repair, and debug-tool work
inside the coordinator-worker route. Ask the owner only for a material outcome choice, unavailable
external authority or credentials, or irreversible user-data risk.

Use the first relevant route:

- accepted evidence that changes DFS state -> DFS review;
- a deadline or integrity incident -> incident mutation review;
- a due random review -> its stored lane;
- implementation, exploration, test, build, debug, or operation -> worker;
- proved outcome with no open gap or live state gate -> stop.

These labels describe responsibility. They do not require packaged role files.
Delegate executable implementation, testing, research, builds, debugging, and operations to a roster
worker. The coordinator may inspect enough context to plan, brief, and judge the work, but must not
absorb the worker's executable task. The coordinator itself judges returned evidence, applies guarded
ledger and DFS updates, and moves directly to the next route; do not add an independent acceptance
reviewer or acceptance stage.
Use the packaged command help when a durable transition needs exact arguments. Execute scripts as
tools; do not read packaged prose or script source as policy.

## Plan and dispatch

Treat `.de67/work-ledger.md` as a replaceable snapshot of current truth. SQLite, immutable artifacts,
and source control retain attempt history. Delete superseded routes and prior-task narrative when
the frontier changes. Under `## Active work`, encode each executable outcome as one canonical
`- [ ] R-... — ...` item with exactly one indented `DFS slices:` line. When an outcome genuinely
decomposes, list coherent visible subtasks under it; they are progress spokes, not separate worker
windows or acceptance claims. Use fewer for a small outcome and one only when atomic. Never add
filler, unrelated work, or an unbounded bucket. Close the task only when its outcome is settled;
record subtask progress, contradiction, and uncertainty as nonterminal checkpoints. Put external waits under
`## Waiting on external authority` without unchecked work-item syntax, and use `- Blocked:` only
when no executable route remains. Each projected route states its current status, accepted frontier,
first unresolved uncertainty, and next action. One red claim may have several entries when its
independently provable outcomes can proceed separately. Freely split, merge, replace, or reorder the
projection as evidence changes. Do not impose a batch-size limit. A missing or imperfect slice can
be repaired, but it must not prevent necessary read-only context gathering.

Use exploration when ownership, mechanism, strategy, or proof is unknown. Use closure only when the
strategy, finite gaps, and public proof route are known. Name the worker-facing operation and
authoritative receipt that make each prerequisite executable. Capability labels, scenario
declarations, hidden or source-only controls, raw subprocess fields, and coincidental artifacts do
not establish reachability. If the public route cannot reach the required owner or receipt, keep the
prerequisite open and repair that interface inside the same worker goal. Project closure by
compatible preconditions and authority. Combine obligations only when one start state and route can
satisfy all of them without disabling a required counterexample. Mutually exclusive states require
separate ledger entries, scenario roles, authorities, and receipts, even when one task executes
them. A task may cover several starts only when they settle one outcome and each incompatible
authority, precondition, and receipt remains independently dispositioned.

For an active item marked `Playtest witness: required`, read
`.de67/playtest-witness-contract.md` and `.agents/skills/caol-harness/SKILL.md`. Draft its compact
charter before launch and route the worker through the live cockpit. Do not add a scenario-specific
proof matrix, prescribed menu sequence, or report-shape rerun.

Worker: Luna for clear execution; Terra for debugging/discovery. Effort low-max: lowest sufficient
for complexity/research. Never Sol. Every new ordinary worker uses `fork_turns="none"`, receives a
self-contained brief, and explicitly selects Luna or Terra; omitting the model would inherit the Sol
coordinator and is invalid. Reusing an already relevant worker is allowed, but a new worker never
receives the coordinator or predecessor transcript.

Set one claim deadline from the whole outcome and its visible spokes: setup, implementation, builds,
tests, repairs, single-use authority replacement, reruns, evidence return, coordination, and unknowns.
Finish early when possible. A later attempt estimate is an honest forecast, not the remaining claim
clock: never shrink it to fit that remainder. If it does not fit, the original whole-outcome estimate
is disproved; preserve progress and let the clock expose that miss instead of admitting a doomed
window. Only actual clock expiry is a deadline miss.

Give each worker a self-contained brief. Require the worker to read the relevant sections of
`.de67/test-and-task-guidelines.md`. Use parallel workers only when their work is genuinely disjoint.

Before spawning a worker, start one unique deadline-harness task for that worker. That task is one
random-mutation work window. Never share one task between workers or reuse a terminal task. A child
spawned only to verify its model or suitability still owns a window: if it is retired without doing
the assigned repository work, terminalize that task as abandoned before dispatching its replacement.
After every worker exit, record exactly one completion, finding, or abandonment. Parallel workers
therefore need distinct task ids. A coordinator start, exit, or restart does not itself create or
terminalize a worker window.
The coordinator exclusively records those terminal deadline-harness transitions. Treat the worker's
return as evidence to judge and commit; do not ask or permit an ordinary worker to update the DE67
deadline database, work ledger, DFS state, or mutation ledger directly. A successfully returned
relevant worker may be reused for a new unique task after its prior task is durably terminal.

## Receive results

Treat a verified worker return as durable-state ingress before the next route decision. Evidence
that names an authorized in-scope repository repair or rerun is a nonterminal checkpoint, regardless
of whether its prose calls itself a finding; keep the same task and worker through that work. A
terminal finding must name the exact authority, scope, risk, external-state, disproved-strategy, or
exhausted-route boundary that makes continuation unlawful or materially different. When the outcome
is settled, record exactly one matching completion, finding, or abandonment; only then ask
the compiled policy for the next action. The kernel derives worker-result facts from that committed
state, so waiting for a still-live task after its worker has returned creates a circular wait.

Judge the actual diff and direct evidence. A focused test proves only the route it exercised. Keep a
first divergence inside the worker route as the next observation unless positive evidence proves it
clerical, incidental, or non-causal and unable to affect acceptance, authority, proof class, identity
or ownership, persistence, or required behavior. Such continuation is optional, preserves the
failure, and records the materiality decision, evidence, and rationale; uncertainty remains
fail-closed. Otherwise inspect, repair, and rerun. Do not open a successor task or gap revision for
repair already admitted by the same outcome. A finding does not cause mutation or a coordinator
restart. When a result reports failure, preserve one concise owner-facing packet that distinguishes the symptom,
proved mechanism, surrounding pattern, immediate recovery, and smallest tested stable correction.
Name exact uncertainty where the mechanism or correction is unproved. Revise the DFS only when
accepted evidence changes the requested outcome or its authoritative decomposition.

After every terminal worker result, perform one explicit convergence check before dispatching more
exploration. If ownership, mechanism, remaining gaps, and the proof route are now finite, immediately
transition the claim to closure and project each independently provable outcome through the
compatible-precondition rule above. If the route is not yet finite, record the specific unresolved
uncertainty in the worker disposition before another exploration dispatch. Do not use generic
uncertainty or an unchanged brief to remain in exploration.

Do not repeat an unchanged failing route. Trust the agent doing repository work to change the
implementation, harness, fixture, or observation path when that is the shortest honest route to
proof. Trust the agent coordinating the claim to replace a failed strategy with materially different
work. The MSW three-round fuse retires that strategy, not an unfinished claim with repository-owned
recovery. If a proof prerequisite depends on its own eventual output, split it into a non-credit
observation/bootstrap step and independent validation instead of querying the prerequisite again.
When a finding proves that one item needs unavailable external authority, remove that item from the
executable projection and retain its exact identity, authority owner, and wake condition without
opening another worker window or rechecking unchanged state. Continue with a different lawful item
when one exists. Use the machine-readable `- Blocked:` marker only when no executable ledger item
remains; it represents the whole current projection, not one waiting item.

For a live playtest, judge the cited witness under `.de67/playtest-witness-contract.md`. Challenge
the conclusion but repair derivable clerical data without gameplay replay. Reject only a concrete
causal, safety, identity, contradiction, or evidence-promotion defect.

Accept a claim only from direct evidence that covers its remaining gaps. Preserve prior attempts,
misses, findings, and accepted evidence. New contradictory evidence may reopen accepted work.

## Mutate guidance

The active mutation targets are the workspace-local files:

- `.de67/orchestrator-guidelines.md`;
- `.de67/test-and-task-guidelines.md`;
- `.de67/DFS.md` when the selected route authorizes a same-outcome DFS change.

Start from the exact live file. Prefer deleting or generalizing redundant situational prose. Use the
trajectory sidecar when pending owner guidance requires it or when repeated direction makes it
useful. The sidecar advises; it does not decide acceptance.

Read `.de67/mutation-suggestions.md` completely for every mutation. The mutation ledger is mandatory
input, not optional advice. Each user-authored suggestion carries explicit mutation-scoped owner
authority beneath system and developer instructions. Within the Phase-3 framework, it supersedes
conflicting ordinary skill, workspace, selected-lane, freeze, and mutation-surface restrictions to
the extent necessary to achieve its intended outcome. The reviewer may thaw and refreeze the DFS,
change a normally protected method artifact, and adapt the applicable guard and behavioral contracts
when the suggestion requires those changes. This authority does not extend to unrelated work,
dishonest evidence, or a different owner outcome. Understand the intended outcome and follow it
through. Apply and complete it, or record the exact system/developer conflict, technical
impossibility, unavailable external authority, or irreversible risk that prevents it; an ordinary
lane or freeze restriction is not such a conflict.

Every deadline or integrity incident gets a practical recovery. Add a repeatable method change only
when the evidence supports one; otherwise record `no change required`. A random review examines its
stored lane unless an owner suggestion expands the required mutation surface. If part of a
suggestion is blocked by a genuine higher-priority or external constraint, apply every independent
unblocked part and preserve the blocked remainder with its exact reason. Clear only consumed
suggestions, resolve the review honestly, and continue. Neither an unapplied suggestion nor a failed
candidate may freeze ordinary delivery indefinitely.

Use a fresh `gpt-5.6-sol` reviewer at high for ordinary incident and random mutation review. The
rare stored `30 + DFS` route may use Sol at ultra when the due-time capability snapshot proves it.
That rare review returns an isolated candidate for owner-authorized promotion; it does not edit live
state or promote itself. Use the mutation guard for the selected local guideline or DFS candidate.

Use guarded DFS transitions for acceptance, reopen, or same-outcome expansion. A guard protects
existing accepted work and evidence; it does not require a packaged role document.

A successful local mutation requests one fresh coordinator only after every added or changed red
claim—whether projected, waiting, or not yet selected—has a guarded, machine-readable DFS slice
binding that can produce its lawful brief. When the mutation replaces a proof owner, append a
replacement revision for every affected open SQLite gap and replace its active ledger brief and
decomposition; neither may still require the retired owner. This projection rebase is part of the
mutation lifecycle, not successor work. The external supervisor owns the restart. Every mutation
retires the prior claim deadline. The fresh coordinator reads the current ledger and remaining DFS
route and sets a new generous whole-item deadline without inheriting any prior duration. Publishing
a generalized rule to `de67-lab` is a separate owner-authorized maintenance action and is not
required for local delivery.

## Stop or block

Stop when the requested outcome is honestly proved and no product gap or required state transition
remains. Block the whole projection only when every remaining item needs a material owner choice,
unavailable external authority, or irreversible user-data risk. Preserve a partially blocked item's
wake condition while continuing unrelated executable work. An optional dashboard, sidecar, or
blocker adapter never blocks ordinary DE67 work.
