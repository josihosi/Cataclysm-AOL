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

- Owner-authorized [trigger]: Josef explicitly requested “Ok fix and restart” after the tested migration repair and protected-surface blocker were explained. Apply and validate only the necessary deadline_harness.py legacy-component migration repair, including the protected surface as required; finish the remaining ambush incident closeout and restart DE67 3 through the existing supervisor. The stopped-service mutator installed the previously tested narrow repair under the service control lock after verifying zero worker claims/nonterminal tasks. Current repair evidence: .de67/state/restart-migration-repair/. Prior incident diagnosis, accepted proof, context correction and unresolved macro receipt are in .de67/task-logs/review-ambush-066/report.md and closeout.json. The old macro receipt predates this protected repair; refresh it against the current protected baseline as necessary. Preserve completed proof, deadlines, incident history and existing pending restart generation 50; do not replay gameplay or create a second coordinator. This entry authorizes the necessary repair and closeout, not unrelated method or product changes.
