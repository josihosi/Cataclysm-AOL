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

- Owner-authorized [trigger]: Finish the already-authorized Phase-3 launch by repairing the newly exposed compatibility failure between the refrozen DFS and durable historical acceptance projection. The completed handoff correction in .de67/state/review-owner-dced6f928643/review.md remains valid and must be preserved; do not redo it or widen gameplay-fix authority.
  After mutation-eed7313e30c847de982d03777ed0c543 completed successfully and checkpoint 13cde52c201a92e88ca13354164b69462a10144e was pushed and verified, the supervisor stopped in its actual post-review transition: deadline_harness.py synchronize_dfs_statuses raised `DFS has no implementation status block for R-027`. No fresh coordinator launched. Historical accepted claims and baselines survived, but the phase-2 refreeze removed the structural implementation-status blocks expected by this projection. Inspect all affected accepted claims so fixing the first parser error does not conceal the next one. Preserve pending semantic restart generation 23 and durable history; do not manufacture new acceptance or let historical green status discharge the fresh campaign.
  Make the smallest compatible correction to the frozen-document/projection boundary and its responsible authoring or runtime mechanism. Preserve the current owner contract, full new acceptance criteria, the tested worker-handoff correction, valid historical evidence and the native game lifecycle. Prove the actual failed status-projection operation against a disposable copy of current state, then exercise the supervisor's post-review handoff boundary, not only prompt construction. Resolve this exclusive gate and allow the external supervisor to launch the intended fresh coordinator from the corrected current contract. No gameplay proof or automatic gameplay fix is authorized by this startup repair.
