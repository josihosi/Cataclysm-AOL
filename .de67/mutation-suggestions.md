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

- A fresh coordinator could not open the required whole-item deadline after mutation review.
  Generation 21 acknowledged successfully, but `R-029-exploration-039` rejected the independently
  estimated 345,600-second window because the clock still enforced the retired 259,200-second
  generation. This matters because repository work remains authorized and recoverable, but no
  truthful worker task can start. Change the post-mutation clock route so the first new task arms a
  fresh claim generation from its evidence-based estimate without inheriting the retired duration.
  Preserve the rejected start as a nonterminal routing counterexample. A falsifying check is a fresh
  restart where a changed honest estimate opens one new generation and binds its first task without
  altering any retired deadline.
