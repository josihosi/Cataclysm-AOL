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

### Manual proposal — design first, test only uncertainty

Make each work-ledger outcome begin with a read-only design pass before implementation or proof
execution. Trace the complete production owner, state transitions, precedence, readers, writers,
early exits, and competing owners; state the intended invariants and cohesive correction before
editing. Implement that correction as one design-owned change instead of using successive tests to
discover and patch one branch at a time.

Use focused tests only at genuinely blurry boundaries such as persistence, replay/idempotency,
competing ownership, timing, or complex geometry, followed by the smallest integrated production
proof required by the DFS. A failure must first be classified as an implementation deviation, a
contradicted design premise, a false test premise, or irrelevant harness behavior. It does not
automatically authorize another product patch, dispatch, or ledger-history entry. Keep the ledger
item open across necessary replanning and close it with one terminal outcome; tests verify the
design rather than generate an unbounded implementation plan.

Evidence: R-002's returned-signal route fixed one watch-arrival interception in `R002-M104`, then
`R002-M105` found an earlier watch-assessment interception on the same production route. A complete
owner/precedence pass before editing would have exposed both branches together. Candidate treatment:
add this design-first gate and failure-classification rule to `test-and-task-guidelines.md`, while
preserving the DFS outcome and the kernel's requirement for honest focused and integrated proof.

### Manual proposal — macroscopic convergence review

Make mutation review evaluate the trajectory of the whole active ledger item, not only the last
task, test, deadline, or finding. Reconstruct the terminal user outcome, the current production
design, the accepted starting frontier, and what uncertainty or unmet behavior remains now. Treat a
new test, fixture, diagnostic, dispatch, commit, or newly classified failure as activity rather than
progress unless it removes a necessary contract gap or durably narrows the causal uncertainty that
blocks closure.

Before any follow-up dispatch after a finding, require the coordinator to decide whether the current
decomposition is converging. Look for repeated local patches, serial discovery of adjacent branches,
growing proof or coordination surfaces, new machinery used only to validate earlier machinery,
reopened premises, and movement of the test frontier without movement of the product frontier. If
the next task follows only from the latest failure rather than a complete causal model, stop the
dispatch chain and replace it with a whole-owner design review, simplify or delete the unnecessary
surface, reframe the ledger item, or return for a material user decision as appropriate.

Require mutations themselves to be subtractive when possible. They may replace guidance that causes
churn rather than accumulating another local rule, and their verdict should state whether the
workflow has a credible beginning, terminal condition, and causal path between them. Candidate
treatment: add the convergence gate to `orchestrator-guidelines.md` and align
`test-and-task-guidelines.md` so task briefs measure movement toward the ledger outcome rather than
completion of intermediate artifacts. Preserve correctness and honest proof; the purpose is to make
the system choose a coherent route to them.

Evidence: after the R-002 proof rescope, tasks `R002-M102` through `R002-M107` continued to advance
the same decoy-signal subcontrast through successive fixture, owner, synthetic-test, and live-run
frontiers without closing it. Each finding truthfully improved local knowledge, but the repeated
dispatch chain shows that local correctness review alone does not detect non-convergent method.
