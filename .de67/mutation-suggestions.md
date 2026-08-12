# DE-67 mutation suggestion ledger

This is consumable scratch, not history. Independent reviews and manual suggestions use ordinary
Markdown. The current mutation transaction reads every pending entry. After a real guarded mutation
is applied, replace the whole ledger with this empty template; if guard or application fails, clear
nothing.

A random improvement review records one to three concrete inefficiencies ranked by causal
importance, direct evidence, the candidate target/section change, and proposed treatment of pending
suggestions. The accepted subset must correspond to the actual guarded file change. A guarded DFS
no-op leaves this scratch ledger intact.

## Pending suggestions

### Manual proposal — explicitly search for overengineering

Every incident and random-improvement mutation review should explicitly look for
**overengineering**, not only generic inefficiency. Check whether implementation layers, special
cases, proof matrices, fixtures, diagnostics, coordination, retries, identity bookkeeping, or
handoff artifacts exceed what the DFS contract and honest proof require. When deleting or
simplifying machinery preserves the requested behavior, authoritative ownership, correctness, and
the smallest reliable proof, prefer that deletion or simplification and name the avoided
overengineering in the verdict.

Evidence: R-002 expanded into a bespoke natural-world certification campaign with seed searches,
per-row live identity continuity, analyzers, and exact-clock harness work even though focused owner
tests plus one compact live negative/positive proof were sufficient after the user-approved
rescope. Candidate treatment: add an explicit overengineering check to the applicable efficiency,
task/test design, and orchestration review sections; keep correctness and proof above efficiency as
required by the kernel fitness order.
