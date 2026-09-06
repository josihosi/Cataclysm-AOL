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

- Owner-authorized [trigger]: Enforce Josef's current fresh-CAOL playtest contract through the Phase-3 projection and every worker handoff. This implements the settled owner instructions in the 2026-09-06 WEC and refrozen DFS, not a new product outcome. Fresh proof covers all in-scope CAOL families even when older runs were green. DE67 loop faults belong in method mutation; harness faults may be repaired by the coordinator, mutator, or repair worker; CAOL gameplay observations go on the suspected-bug list and only Josef's explicit promotion authorizes a finding followed by a mutator DFS update and fix/retest plan. Continue independent tests while waiting, preserve blocked tests and evidence, and report an all-blocked campaign as awaiting the owner, never complete. Do not impose automatic game termination by runtime or memory; preserve the native quit/finish/cleanup boundary.
  The first launch after the refreeze exposed a real handoff failure. Coordinator run initial-e82993b30cae4535b0665e03308c92e8 dispatched R-026-exploration-002 from the old ledger and predecessor receipt without reading the current DFS/WEC. Its sealed brief at .de67/state/worker-dispatch/task_522d3032362d6578706c6f726174696f6e2d303032-dd982c35588885d8a0460b37d1f0230ea75472e366fe578a215e0165e44aa343.md grants generic implementation repair, carries historical no-replay instructions, omits the CAOL promotion rule, and clips the current multiline R-026 acceptance boundary to its first line. The launch was stopped before any game was running; its semantic receipt edits remain in the working tree and must be preserved and assessed, not discarded or credited as proof.
  Correct the stale projection and the responsible handoff mechanism so current owner constraints and complete outcome-sized acceptance reach workers and outrank historical strategy. Preserve prior evidence and avoid reimplementing already-existing repairs, while giving historical greens no exemption from the explicitly requested fresh testing. Validate a regenerated real R-026 brief against this counterexample and verify that the next coordinator uses the refrozen contract. Do not expand gameplay-fix authority, erase durable history, or turn this correction into a new product requirement. Resolve through the normal exclusive mutation review and fresh-coordinator lifecycle.
