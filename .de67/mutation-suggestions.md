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

- Owner-authorized [trigger]: Repair owner-wait closure routing so an explicit decision boundary is not treated as executable work merely because its required proof text is nonempty. R-031 gap `R031-package-evidence-integrity` revision 3 is the current counterexample. It says Josef must choose between bounded acceptance and fresh evidence, but `policy_kernel.py` marks the gap executable from its nonempty `proof_route`, the ledger fallback repeats that inference, and global keyword matching can add the same fact. This repeatedly requests another closure worker and prevents independent delivery after the evidence route is exhausted. Preserve receipt `33656681a25465607762bc4efcf348ecd0dea6ff183a2a85888b673604254944`, the one-time terminal transition for task `R-031-closure-provenance-disposition-005`, and the exact owner-only A/B boundary. Do not infer owner authority from prose, blank required proof text, replay gameplay, duplicate-terminalize the task, or let an owner wait block independently actionable claims. Add guarded policy and lifecycle tests that distinguish executable repair or observation routes from explicit owner-only waits and that consume terminal results only through the correct successor revision. Mutator advisory message `25e1632c03b14b328d4eb6fd715367ec` records the reproduced parser paths and grants no A/B disposition.
