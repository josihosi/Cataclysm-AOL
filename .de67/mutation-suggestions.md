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

- Owner-authorized [defer]: Generalize Sol's worker briefing to both coding and testing wherever it can reduce uncertainty, prevent rework or improve correctness. Consolidate the experiment-specific wording in `/Users/josefhorvath/.codex/skills/de67/de-67-3/scripts/coordinator_supervisor.py::worker_handoff_contract()` and keep `live_coordination_contract()` consistent. The idea is for Sol to turn the FS outcome and relevant source knowledge into useful working guidance, using bounded Luna searches when helpful and synthesizing their findings rather than forwarding search history.
  For coding, useful guidance may explain the existing design, proposed approach, interfaces and invariants worth preserving, unresolved choices, and how correctness could be established. For testing, it may explain the behavior under examination, material premises, distinguishing actions/observations, and what the results would establish. These are examples to select from, not required sections. Scale detail to the actual uncertainty and consequences; use the existing brief/handoff mechanism and current context. Skip additional briefing when it would add no value.
  The FS remains the outcome authority; the brief is revisable engineering guidance. Distinguish established facts from hypotheses, leave room for worker judgment and discovery, and use returned evidence to revise the approach and inform Sol's review. Improve the existing instruction rather than layering on ceremony: no mandatory template, document, checklist, helper chain, receipt, approval or extra review stage. This is deferred input for a normal mutation opportunity and does not request interruption of active work.

- Owner-authorized [trigger]: Josef requests complete removal of the unrouted legacy guidance identified in the local de67-lab audit because it can steer agents incorrectly. Remove `de-67-3/references/kernel.md` and `de-67-3/assets/environment/orchestrator-guidelines.md`, including the active workspace `.de67/orchestrator-guidelines.md` copy when its lack of agent routing is confirmed. Remove or update the dependencies that keep this prose mechanically alive: required/protected-file lists, candidate overlays, hashing/provenance, legacy comparisons, bootstrap copies and tests. Do not relocate the obsolete instructions into another active fixture or retain dummy files to satisfy old checks. Preserve the real invariants in live code, compiled policy, accepted proof, immutable historical receipts and current ownership; history remains evidence rather than active guidance. This removal authorizes the necessary dependency changes, including protected-file bookkeeping where it exists solely to require these obsolete documents; it does not authorize unrelated clock or gameplay changes. Keep `references/external-supervisor.md` because SKILL.md actively routes supervisor operations to it. Verify actual incoming readers before each deletion, update relevant supported tests and validate the remaining runtime/guard route. Use the existing exclusive mutation lifecycle and preserve concurrent unrelated configuration work. The prior separate deadline compatibility-repair approval request is not answered by this cleanup instruction.
