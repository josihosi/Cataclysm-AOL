# Live playtest witness contract

This contract applies only to an active ledger item marked `Playtest witness: required`. The
coordinator routes it by pointer; the harness skill supplies the worker-facing operations.

Before launch, the coordinator writes a compact charter naming the smallest claim, material proof
and contradiction, already accepted evidence, current uncertainty, forbidden shortcuts or credit,
honest stop conditions, and requested evidence ceiling. It describes the outcome, not a menu
sequence or prose template.

The worker owns `observe -> choose -> act -> repair -> rerun -> witness`. The live cockpit exposes
only current authority and `WITNESS / FINISH`. Its immutable journal binds scenario, source,
executable, run, and authority identities and records native observations, actions, receipts,
deltas, interruptions, cleanup, evidence ceiling, and contradictions. Semantically equivalent
native action sequences and run-local identifiers are allowed when the causal facts and required
binding are preserved.

The witness states the smallest supported claim; `proved`, `contradicted`, or `inconclusive`;
causal account; cited observations and meanings; material deviations; contradictions; remaining
unknowns; and `accept`, `continue`, `repair`, or `change-strategy`. It distinguishes observation,
inference, contradiction, and unknown, preserves inconvenient evidence, and stops requesting proof
after the charter is settled. It cannot invent a fact, cite an absent value, conceal a supplied
contradiction, change identity, or promote the evidence ceiling.

Mechanical validation checks journal integrity, bindings, citations and cited values,
contradictions, append-only report references, and evidence ceiling. The coordinator then judges
causal sufficiency and semantic equivalence. Apply intellectual scrutiny and administrative
leniency: reject or continue only for a concrete false conclusion, causal ambiguity, unsafe action,
identity failure, material contradiction, or false promotion. Derive optional clerical summaries
without replay when the underlying facts are present. Polished prose never overrides a mechanical
or causal contradiction, and gameplay is never replayed only to improve paperwork.
