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

- [defer] Reviewer-authored proposal: Forwarded by windows-owner-assistant as a non-forcing request; this interagent report is not owner authorization. At a suitable review, evaluate whether Sol's live involvement can focus more on decisions than routine observation. The supplied, not independently verified, evidence is pre-crash coordinator run `01a08862-f5bd-7220-baa3-76e0aca733bb`: 64 worker-event-log reads, 312 waits and 8 delivered follow-up messages, mostly new content. Evaluate decision value rather than assuming stale rereading. Consider consolidating `coordinator_supervisor.py::live_coordination_contract()` so worker messages/results surface recurring obstructions; a focused question or execution-log read serves an unresolved issue that can change direction, scope, ownership or acceptance; a wait timeout alone does not require inspection or another still-working update. Preserve useful independent source investigation and corrective interventions, including the five-minute movement-cadence correction. Adapt or reject the proposed wording from actual evidence. No immediate adoption, interruption, extra restart, fixed polling cap, reporting interval or review trigger is requested.
