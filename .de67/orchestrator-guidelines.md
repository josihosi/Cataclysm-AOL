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

Set one generous deadline for the whole ledger item. Include setup, implementation, builds, repeated
test runs, repairs between runs, review, disposition, and the uncertainty of the route. Unknown work
does not take zero time. Variable playtesting needs room for several relevant runs and material code
changes between them. Finish early when possible. Do not turn an attempt estimate, test finding, or
revised plan into a deadline miss. A deadline miss occurs only when the item clock actually expires.
The coordinator must set a deadline it can honestly deliver, including room for foreseeable problems,
known unknowns, and an uncertainty margin for problems it has not predicted.
Never copy one worker attempt's runtime into the next whole-item deadline or omit worker startup,
evidence return, diagnosis, repair, rebuild, rerun, and coordination time. Give a deadline reviewer
the repeated estimates and outcomes so it can judge whether the estimation method needs to change.

Give each worker a self-contained brief. Require the worker to read the relevant sections of
`.de67/test-and-task-guidelines.md`. Use parallel workers only when their work is genuinely disjoint.

Before spawning a worker, start one unique deadline-harness task for that worker. That task is one
random-mutation work window. Never share one task between workers or reuse a terminal task. A child
spawned only to verify its model or suitability still owns a window: if it is retired without doing
the assigned repository work, terminalize that task as abandoned before dispatching its replacement.
After projecting a terminal worker result into the ledger and any authorized DFS transition, record
its durable result receipt with `deadline_harness.py receive-result`. An unreceived terminal result
is an event to process once, not a permanent description of the workspace.
After every worker exit, record exactly one completion, finding, or abandonment. Parallel workers
therefore need distinct task ids. A coordinator start, exit, or restart does not itself create or
terminalize a worker window.

## Receive results

Judge the actual diff and direct evidence. A focused test proves only the route it exercised. An
ordinary failed test is not a finding. Keep it inside the worker route: inspect, repair, and rerun.
Record a terminal finding only when the assigned strategy is disproved, a materially different route
is required, an external blocker exists, or the bounded route is exhausted. A finding does not cause
mutation or a coordinator restart. Revise the DFS only when accepted evidence changes the requested
outcome or its authoritative decomposition.

Do not repeat an unchanged failing route. Change the implementation, setup, observation, tooling,
or causal hypothesis. After the MSW three-round fuse, replace narrower probes with the smallest
implementation or observation that crosses the first still-unproved authoritative transition. The
same attempt may continue through its downstream consumer when that integrated proof is sensible.

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

Every deadline or integrity incident gets a practical recovery. Add a repeatable method change only
when the evidence supports one; otherwise record `no change required`. A random review examines its
stored lane. If part of a suggestion is valid, apply that part. Move unapplied owner-visible ideas
to `.de67/human-todo.md`, clear the consumed scratch, resolve the review honestly, and continue.
Neither an unapplied suggestion nor a failed candidate may freeze ordinary delivery indefinitely.

Give the mutation reviewer the evidence needed to reason rather than a requested verdict. For a
deadline review, include the full available deadline history for the claim: every planned window,
actual attempt duration and outcome, overrun, and recorded diagnosis, together with the relevant
ledger item, DFS gap, active guideline or policy surface, and pending owner suggestions. For other
reviews, provide the equivalent incident history and product context. The reviewer may inspect
local implementation and history, and may research analogous systems in primary sources when the
causal pattern is unfamiliar or external comparison would materially reduce guesswork. Research is
evidence, not authority. The reviewer remains free to conclude that no change is required.

When the claim clock is already expired, record the claim deadline miss before receiving another
result or dispatching more work. Preserve any worker evidence that returned before or after expiry;
expiry changes the method-review route, not the truth of the result.
Every resolved deadline miss retires the current coordinator even when review concludes that no
guideline change is required. The fresh coordinator must set the successor generation's whole-item
deadline from the remaining route and current evidence. Never inherit the expired duration.

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
