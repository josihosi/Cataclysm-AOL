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

- Owner-authorized [defer]: Restore Josef's intended FS (Functional Specification) terminology and purpose instead of the model-introduced DFS. This is a non-forcing suggestion for the next review; it must not trigger or interrupt delivery. The FS should specifically describe the core functionality of the code, almost 1:1 in language: data and identities, component/function responsibilities and interactions, state changes, inputs/outputs, error and boundary behavior, with relevant source references, examples and discriminating acceptance tests. Rewrite vague requirements into explicit functional behavior; renaming alone does not satisfy the outcome.
  Refocus the specification by moving delivery tracking/assignments to the existing ledger and historical proof to retrievable existing evidence, preserving all functional requirements, accepted proof and durable claim/slice identities. Reduce obscuring or duplicated workflow prose rather than adding a parallel specification or more ceremony. Coordinate with the pending code-quality/coordinator-review suggestion so terminology and effective role context agree.
  Inspect actual readers, writers, guards, policy/tool references and dispatched context before changing names or document structure. Use the exclusive mutation lifecycle for active changes; own authorized specification/context edits and commission ordinary migration tooling through Sol as needed. Preserve live bindings, issued packets, historical references, freeze/acceptance integrity and restart ownership. Validate that the intended FS content reaches its callers and existing lifecycle operations still work; do not blindly rename DFS.md while consumers depend on it or claim completion from a cosmetic label change. Disposition the supported migration/refinement, retaining an exact gap if necessary, and delete this entry once completed with durable evidence.

  Review disposition 2026-09-08: Restored FS terminology and rewrote prompt/retrieval/routing behavior; consolidated obsolete commissioning/freeze prose into retrievable baseline evidence. Preserved all 30 existing slice identities, 57 assignments and 19 delivery-status blocks. Exact remaining gap: active filename and tracking/proof migration requires coordinated resolver, dispatch, status/acceptance, provenance and guard changes; moving the blocks now breaks existing consumers. `R-MAINT-FS-MIGRATION-001` commissions the concrete owner-authorized migration through Sol. Keep this deferred entry until the functional-only FS reaches callers and acceptance/reopen compatibility is proved.
