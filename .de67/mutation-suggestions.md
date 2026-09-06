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

- Owner-authorized [defer]: Keep harness workers oriented through concise test summaries and occasional coordinator guidance. Josef requested this independently of the sidecar lab trial. Refine the existing live-coordination paragraph in the generated coordinator prompt and its guideline source rather than adding a reporting procedure. Suggested text: "Stay engaged with tests and proof runs through concise summaries of their purpose, procedure, current state, and results. Give occasional guidance to keep the bound worker oriented toward the assigned outcome and next useful evidence, and request a clearer or more useful summary when needed." Preserve worker autonomy, meaningful proof, and the existing nonterminal communication lifecycle; this is not a fixed schedule, reporting quota, or instruction to stop tests early. A concrete candidate exists in /Volumes/CodexBulk/Schanigarten/workspaces/de67-lab-evidence-trial/de-67-3/scripts/coordinator_supervisor.py (live_coordination_contract) and de-67-3/assets/environment/orchestrator-guidelines.md. Carry only this targeted guidance change if useful; the separate experimental evidence sidecar is not thereby promoted. Do not trigger or interrupt a review merely because this deferred entry arrived.
