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

- Owner-authorized [trigger]: Restore Josef's intended FS (Functional Specification) terminology and purpose instead of the model-introduced DFS. Josef's current request delegates approval of dashboard blockers and authorizes the existing exclusive handoff for this prepared migration. The FS should specifically describe the core functionality of the code, almost 1:1 in language: data and identities, component/function responsibilities and interactions, state changes, inputs/outputs, error and boundary behavior, with relevant source references, examples and discriminating acceptance tests. Rewrite vague requirements into explicit functional behavior; renaming alone does not satisfy the outcome.
  Refocus the specification by moving delivery tracking/assignments to the existing ledger and historical proof to retrievable existing evidence, preserving all functional requirements, accepted proof and durable claim/slice identities. Reduce obscuring or duplicated workflow prose rather than adding a parallel specification or more ceremony. Coordinate with the pending code-quality/coordinator-review suggestion so terminology and effective role context agree.
  Inspect actual readers, writers, guards, policy/tool references and dispatched context before changing names or document structure. Use the exclusive mutation lifecycle for active changes; own authorized specification/context edits and commission ordinary migration tooling through Sol as needed. Preserve live bindings, issued packets, historical references, freeze/acceptance integrity and restart ownership. Validate that the intended FS content reaches its callers and existing lifecycle operations still work; do not blindly rename DFS.md while consumers depend on it or claim completion from a cosmetic label change. Disposition the supported migration/refinement, retaining an exact gap if necessary, and delete this entry once completed with durable evidence.

  Owner relaxation 2026-09-08: Josef asks to remove the unnecessary restriction behind the current activation blocker. The migration does not have to preserve the old supervisor PID, journal-owner identity or runtime epoch. A controlled external stop/start through the existing service is authorized for this upgrade once current workers have returned and the exclusive activation boundary is established. Normal restart normalization may retire old runtime ownership; preserve completed valid work, durable evidence, claim/slice identities, pending decisions and the semantic restart request. Verify old runtime exit before one new supervisor launches its sole coordinator. The temporary maintenance stop does not revoke this explicit restart authorization.
  This supersedes owner-preserving in-place reload/exec/replacement requirements in prior reviewer guidance, FS/ledger text and candidate handoff assignments. Do not add or install a custom parent-adoption control plane solely to retain an obsolete runtime owner. Reconcile those active instructions during the exclusive review, retaining completed candidate work as evidence rather than mandatory deployment scope. Existing normal restart behavior and a quiet-state check supply the required ownership boundary; no new owner approval or arbitrary universal cadence is required.
  Activation authority remains granted for the necessary FS, resolver/guard/projection and dashboard changes. Integration receipt `2c5490b929d62f7d5e8a1a5ca28dcca1685276bcd5f900e63ed0d4b1451cf359` reports the fixed guard/functional-content cases; verify its current candidate at activation. Receipt `5a5639ee45e72d03e5bb7c492ce09fdaac18fc3b45df9590b79490da0003ec03` and parent-handoff-005 staging remain evidence of the old restriction, not a requirement to preserve that solution. Resolve this entry after the validated migration and authorized restart route are completed; retain an exact remaining technical gap if one is found. No ordinary worker may install shared method changes while coordination is live.
