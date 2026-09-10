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

- Owner-authorized [defer]: Remaining deployment portion of Josef's returned-worker/checkpoint relaxation: installed source and focused regressions are complete in `.de67/task-logs/review-returned-workers-020/report.md`, but supervisor36914 loaded its loop before the repair. Its prompt refresh does not replace imported checkpoint/mutation-gate functions. At the next owner-authorized external service restart, verify that the new supervisor loads the installed optional-checkpoint and returned-worker review behavior. Do not replay accepted product proof, investigate why workers returned, or introduce another review/restart solely for this deferred entry. The current review requests only its ordinary one fresh coordinator; the reviewer does not launch it. Exact original authority and evidence remain in `.de67/state/review-returned-workers-020/baseline/mutation-suggestions.md`.
