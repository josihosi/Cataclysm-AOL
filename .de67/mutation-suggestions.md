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
top-level syntax retains legacy trigger behavior. Delete completed entries; do not keep consumed-history sections in this queue. Durable receipts
and review artifacts retain the evidence.

For a miss, keep immediate recovery distinct from the smallest repeatable method correction and
state the counterexample that could falsify it. For random or universal review, preserve the
applicable policy's stored target, scope, authority, and evidence limits; a guard result never proves
more than its inputs.

## Pending suggestions

- Owner-authorized [trigger]: RELIABLE-CONTEXT-20260906 — Reliable context across workers. Josef approves all three directions: carry relevant context forward in briefs, organize access to current and historical artifacts, and use Luna for targeted discovery.
  - Workers are spending too much of their context rediscovering sessions, scenarios and evidence scattered through `.userdata` and `.de67`. Make successors able to continue from the relevant prior work without making workers maintain another layer of paperwork.
  - Put routine recording and indexing in the supervisor, runner and harness infrastructure. Use information those tools already know. Keep the coordinator responsible for choosing work and the worker responsible for explaining findings and uncertainty.
  - Provide a clear view of current work, related results and searchable history. Preserve existing artifact references. Briefs should carry the relevant evidence and operational entrypoints directly; full records remain available when needed.
  - Make this reliable with multiple workers and across restarts. Different contributions to the same claim must remain distinct and available; the newest receipt must not automatically replace the context of every other branch. Preserve task relationships, evidence limits and current session status.
  - Use Luna helpers for questions that require searching and interpreting scattered evidence. Do not make Luna repeatedly catalogue the archive or perform bookkeeping that code can handle.
  - Judge the change by whether successors receive the right context and reach useful work with less unnecessary reading, including after concurrent work and a coordinator restart. Preserve completeness where it matters; do not impose arbitrary context limits or hide missing information.
  - Build on existing mechanisms and simplify where possible. Keep the current self-correction guidance unless concrete evidence from this work shows it needs changing.
