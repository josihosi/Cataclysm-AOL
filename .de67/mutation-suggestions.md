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


- Owner-authorized [defer]: Consolidate random mutation around open-ended review of a recent coordinator/worker trajectory: outcome, available context, decisions, actions, first divergence, and actual proof or state change. Treat the stored document lane as a sampling seed, not an edit boundary. Follow evidence across roles, tools, guidance, and decomposition; no finding quota or compulsory change. The installed random-review guard now permits combined local guideline changes and guarded same-outcome DFS refinements; broader method candidates retain their existing guard boundary. Preserve accepted proof, owner intent, accounting, exclusive reviewer ownership, and restart lifecycle. Reconcile local mutation guidance with this approach and consume this entry at the next regularly due review; do not interrupt active work merely to adopt it.

- Owner-authorized [trigger]: Eliminate oversized playtest observation and diagnostic output at its source. This requires an exclusive mutation review at the next durable quiet junction; a reminder to read less or a lower output truncation setting does not complete this request. Improve the actual harness interfaces and their agent-facing discovery so workers can obtain the evidence needed for a decision without dumping nested observations, repeated game messages, or whole JSON log records.
  - Evidence: Terra medium worker 01a06eab-5f5e-79c3-9543-28a23285af96, task R-029-exploration-038, rollout /Users/josefhorvath/.codex/sessions/2026/09/05/rollout-2026-09-05T01-05-20-01a06eab-5f5e-79c3-9543-28a23285af96.jsonl. On 2026-09-04 UTC, the 23:06:30 response-artifact output reported about 20,336 original tokens; the 23:11:47 session-artifact search reported about 67,342; the 23:14:46 debug-log search reported about 247,995; even the 23:18:01 tail of 16 debug-log lines reported about 40,921. These are tool estimates before truncation, not measured tokens fully ingested by the worker. Inspect only the relevant call/output pairs first.
  - Required outcome: compact, task-relevant observation and diagnostic queries are the discoverable normal route. Retain exact run/request/frame identities, action availability, acceptance or failure, and the next useful evidence lookup. Let workers selectively retrieve omitted fields or full digest-bound artifacts when needed. Narrow log queries by the relevant run, request, event, or semantic fields before rendering output; line counts alone do not bound giant JSON records. Preserve raw evidence on disk, report omissions honestly, and do not hide failures or weaken gameplay proof.
  - Follow the demonstrated causal path across harness commands, response formatting, debug-log access, role-local guidance, and worker handoff. Choose the smallest coherent correction; do not impose arbitrary token quotas, mandatory reading rituals, or a new generic logging framework. Preserve the worker's current save/quit repair and its accepted evidence. Existing scenario-search pagination remains useful but does not satisfy these observation and log cases.
  - Prove the correction against the recorded oversized examples or equivalent faithful fixtures: a worker can identify the save/quit first divergence through compact output, retrieve the exact underlying evidence on demand, and still see error and mismatch cases. Measure returned output sizes as evidence, exercise the real CLI path, and verify that compact and full retrieval agree on identities and outcomes. Resolve and consume this entry only after the repair and behavioral verification; preserve any genuinely blocked remainder explicitly.
