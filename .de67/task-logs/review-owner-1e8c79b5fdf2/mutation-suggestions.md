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

- Owner-authorized [trigger]: GPT6-MODEL-GUIDANCE-20260924 — Complete the GPT-6-only migration and preserve both model-selection and reasoning-effort guidelines. Josef explicitly requests a forcing mutation: Terra is gone and forbidden; only GPT-6 Luna, GPT-6 Sol and GPT-6 Astra may be spawned. GPT-5.6 must not remain an available dispatch choice or fallback.
  Preserve the separate guidelines identified in `coordinator_supervisor.py::worker_selection_contract()`: Luna for routine work/playtesting; replace Terra's coupled implementation/difficult diagnosis role with GPT-6 Sol; Astra when stronger judgment reduces uncertainty or rework. For coding, generally max reasoning for Luna/Sol and low for Astra; other work uses judgment appropriate to its reasoning needs. Preserve role-specific coordinator/reviewer bindings and Luna-only live harness operation. Remove the stale claim that Sol is not an ordinary worker, reconcile the older conflicting lab `policy_kernel.py` version, and restore the available GPT-6 model/effort choices without turning preferences into quotas.
  Search relevant installed method, lab source, project guidance, policy, rosters, defaults, overrides, dispatch/resume paths and tests for Terra and GPT-5.6 references. Replace Terra with Sol wherever it directs current or future work; remove retired model choices and enforce GPT-6-only execution across supported spawning and resumed-worker routes. Keep historical transcripts, model identities, receipts and evidence truthful; historical compatibility readers do not authorize retired-model execution. Verify delivered context as well as source text, supported GPT-6 model/effort combinations and rejection of retired or otherwise non-GPT-6 execution, including overrides and resumed threads. Use the existing exclusive mutation/candidate validation route, preserve accepted work and lifecycle ownership, and consume this entry only after disposition with durable evidence.
