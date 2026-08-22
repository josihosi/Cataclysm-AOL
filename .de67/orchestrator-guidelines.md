# Orchestrator guidelines

This workspace-local file is active mutable policy. Read the sections needed for the current route.
Do not replace it with the packaged template after bootstrap.

## Read and route state

Read the compact clock status, active and blocked ledger entries, pending mutation suggestions,
repository state, and the DFS context needed for the next decision. DFS slices are a token-saving
index, not an access prohibition. Read more of the DFS when the decision genuinely needs it. Do not
read predecessor transcripts or packaged DE67 prose as a startup ritual.

Before projecting or dispatching a registry-backed run, compare the ledger token with authoritative
registry lifecycle and token eligibility, manifest identity, and the current executable and source
binding. Any mismatch invalidates the projected launch authority: re-query the current declarations,
issue a fresh selection token, and align the ledger before dispatch.

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
Use the packaged command help when a durable transition needs exact arguments. Execute scripts as
tools; do not read packaged prose or script source as policy.

## Plan and dispatch

Put the currently actionable red claims in `.de67/work-ledger.md`. Keep each entry short and point it
to the DFS context needed for the work. Do not impose a batch-size limit. A missing or imperfect
slice can be repaired, but it must not prevent necessary read-only context gathering.

Use exploration when ownership, mechanism, strategy, or proof is unknown. Use closure immediately
when the strategy, finite gaps, and proof route are already known. A task may cover more than one
gap when the work and proof form one inseparable authority boundary.

Choose Luna by default for ordinary implementation, builds, harness execution, focused tests, and
bounded debugging. Use Terra for ambiguous ownership, difficult diagnosis, risky cross-cutting
work, or demanding exploration. Choose effort from uncertainty: low for a known mechanical route,
medium for ordinary work, high for substantial research, and max for genuinely open-ended research.
Do not use Terra at max or Sol for ordinary work. These are recommendations; a mismatch does not
stop delivery.

When the selected worker model differs from the coordinator model, never use a full-history fork:
`fork_turns: "all"` inherits the coordinator model and defeats the worker selection. Use
`fork_turns: "none"` or a bounded positive turn count with the explicit model and reasoning effort.
Before assigning ordinary work, verify the spawned session metadata matches the selected model;
retire a mismatched child without giving it the task and respawn it with a non-inheriting fork.

Assign ordinary harness runs and their monitoring to Luna workers. While a run is active, the
coordinator waits on completion or failure events for approximately one fifth of its expected
runtime (for example, five to six minutes for a 30-minute run); terminal events wake it early, and
mere confirmation that the run remains active does not justify shorter polling.

Set one generous deadline for the whole ledger item and carry that clock across attempts. For
variable playtesting, use a planning floor of approximately five times the longest relevant measured
run, then add known setup, code changes, builds, focused reruns, evidence review, disposition,
mandatory downstream work, and route uncertainty. Unknown mandatory work does not take zero time.
Before arming a fresh deadline generation for an active closure claim, require its estimate to cover
all measured preparation, revalidation, and build work still required plus the established integrated
run and mandatory finalization and evidence review. If that measured sum does not fit, retain the
current frontier and report the route as clock-inadmissible until an authorized generation covers it;
do not spend the generation on preparation or add an invented buffer.
Before dispatching a repair or probe whose success still requires a measured build, integrated run,
finalization, or review, require its estimate plus those downstream durations to fit the remaining
item clock. Otherwise mark it as preparation rather than a closure-capable attempt and keep the
downstream route explicitly unproved.
For a repeated review lane, the next estimate must not be shorter than the longest materially
comparable completed review in the current item unless a concrete route change explains the
reduction. If that evidence-based estimate does not fit the remaining item clock, do not dispatch it
as closure-capable.
A finding ends its attempt; it does not rebase the item clock or manufacture a miss by requiring the
original estimate to fit again after time was consumed. Finish early when possible. A deadline miss
occurs only when the item clock expires.

Give each worker a self-contained brief. Require the worker to read the relevant sections of
`.de67/test-and-task-guidelines.md`. Use parallel workers only when their work is genuinely disjoint.

Before spawning a worker, start one unique deadline-harness task for that worker. That task is one
random-mutation work window. Never share one task between workers or reuse a terminal task. A child
spawned only to verify its model or suitability still owns a window: if it is retired without doing
the assigned repository work, terminalize that task as abandoned before dispatching its replacement.
After every worker exit, record exactly one completion, finding, or abandonment. Parallel workers
therefore need distinct task ids. A coordinator start, exit, or restart does not itself create or
terminalize a worker window.

## Receive results

Judge the actual diff and direct evidence. A focused test proves only the route it exercised. An
ordinary failed test stays inside the worker route: inspect, repair, and rerun. Record a terminal
finding only when the assigned strategy is disproved, a materially different route is required, an
external blocker exists, or the bounded route is exhausted. A finding does not cause mutation or a
coordinator restart. Revise the DFS only when accepted evidence changes the requested outcome or its
authoritative decomposition.

When a callback-local postcondition passes but a later enclosing assertion fails, inspect every
subsequent owner or mutation between that postcondition and the assertion before naming the first
contradicted premise. Do not infer callback failure from end-of-call state.

Do not repeat an unchanged failing route. Change the implementation, setup, observation, tooling,
or causal hypothesis. After the MSW three-round fuse, replace narrower probes with the smallest
implementation or observation that crosses the first still-unproved authoritative transition. The
same attempt may continue through its downstream consumer when that integrated proof is sensible.

Accept a claim only from direct evidence that covers its remaining gaps. Preserve prior attempts,
misses, findings, and accepted evidence. New contradictory evidence may reopen accepted work.

After a reproduced integrity false green, do not re-accept the contradicted claim until a fresh
reviewer reruns the original reproducer against the integrated state and confirms its paired
preserved outcome.

## Mutate guidance

The active mutation targets are the workspace-local files:

- `.de67/orchestrator-guidelines.md`;
- `.de67/test-and-task-guidelines.md`;
- `.de67/DFS.md` when the selected route authorizes a same-outcome DFS change.

Before briefing any mutation reviewer, deterministically probe the `## Pending suggestions`
section of `.de67/mutation-suggestions.md`.  When an entry-shaped line is present, brief only the
relevant entries and require the reviewer to apply, reject with evidence, preserve, or reroute each
one explicitly.  After a successful guarded treatment, consume only the entries it resolved.

Start from the exact live file. Prefer deleting or generalizing redundant situational prose. Use the
trajectory sidecar when pending owner guidance requires it or when repeated direction makes it
useful. The sidecar advises; it does not decide acceptance.

Every deadline or integrity incident gets a practical recovery. Add a repeatable method change only
when the evidence supports one; otherwise record `no change required`. A random review examines its
stored lane. If part of a suggestion is valid, apply that part. Move unapplied owner-visible ideas
to `.de67/human-todo.md`, clear the consumed scratch, resolve the review honestly, and continue.
Neither an unapplied suggestion nor a failed candidate may freeze ordinary delivery indefinitely.

Use a fresh `gpt-5.6-sol` reviewer at high for ordinary incident and random mutation review. The
rare stored `30 + DFS` route may use Sol at ultra when the due-time capability snapshot proves it.
That rare review returns an isolated candidate for owner-authorized promotion; it does not edit live
state or promote itself. Use the mutation guard for the selected local guideline or DFS candidate.

Use guarded DFS transitions for acceptance, reopen, or same-outcome expansion. A guard protects
existing accepted work and evidence; it does not require a packaged role document.

A successful local mutation requests one fresh coordinator. The external supervisor owns that
restart. Publishing a generalized rule to `de67-lab` is a separate owner-authorized maintenance
action and is not required for local delivery.

## Stop or block

Stop when the requested outcome is honestly proved and no product gap or required state transition
remains. Block only when no executable route exists without a material owner choice, unavailable
external authority, or irreversible user-data risk. An optional dashboard, sidecar, or blocker
adapter never blocks ordinary DE67 work.
