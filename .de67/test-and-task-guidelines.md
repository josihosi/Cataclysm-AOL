# Test and task guidelines

The headings in this document are frozen. Text beneath them is mutable after evidence-backed failure
review.

## Task preparation

Read the exact working tree, affected DFS claim, current owner path, relevant tests, and current tool
state. Choose the smallest task whose deletion would leave that claim unmet or unproven. Name real
dependencies and likely collision surfaces before parallel dispatch.

## Tooling check

Confirm the required tool, environment, command, and smallest runnable route before estimating or
dispatching. Treat setup uncertainty as work to resolve, not as invisible worker time.
Validate brittle flags, dates, fixtures, and output paths before they consume an implementation
deadline. When repeated runs cannot expose the needed production fact, stop rerunning the product
task and add the smallest useful logging or probe capability.

## Task definition

Describe the desired outcome in natural language, bind it to one red DFS claim and the relevant code
surface, state material boundaries, and name the evidence that will decide acceptance. Do not demand
a fixed receipt shape. Ask the worker to stop and report expected versus observed behavior with
direct evidence when a blocker or unexpected production result invalidates a task premise.

## Worker and model selection

Use the weakest sufficient available worker for understood implementation. Use a stronger
implementation-capable model for ambiguous ownership, causal diagnosis, or risky cross-cutting work.
Change the route or worker only when evidence supports the change.

## Test definition

Define the smallest honest test that proves the DFS outcome through its authoritative owner. Include
negative controls only when they distinguish a real competing explanation. Do not substitute test
volume for coverage of the requested behavior. Separate the mechanism verdict from incidental
metadata or receipt formatting; an incidental mismatch is fatal only when it can change identity,
the verdict, or a false-green control.

## Test checking

The coordinator checks the actual diff, command result, and relevant artifact or observation. A test
passes only the claim it exercises. Preserve concise evidence paths; do not encode the same result in
nested receipts. A blocker or unexpected result is evidence for causal review, never proof that the
claim passed or automatic authority to weaken its test. Do not rerun an unchanged failing route
without changed source, tooling, or causal evidence.

## Deadline estimation

Estimate from inspected scope, measured tooling/setup time, dependencies, and relevant prior tasks.
State the reason in the work ledger. The immutable timer begins at actual worker dispatch, and honest
late work still counts as a deadline miss.
