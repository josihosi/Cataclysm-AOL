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

### T01-M16 — phase-specific receipt strings blocked authoritative return

Source: worker finding and coordinator source review

**Short verdict:** test overdefined / false-red physical-return guard

**Diagnosis:** Exact run `20260810_222228` naturally returned members 4/5 and matched the same-run
authoritative facts `structural outing returned home lead=frontier_probe:0` and
`members_returned=2`. The scenario's physical-return audit still aborted because its
`required_line_patterns` also demanded both a `returning_home` local handoff and dematerialization
string. Those internal receipts no longer determine identity or the physical-return verdict, while
their absence prevented final saved report/decision proof. Direct evidence:
`.userdata/dev-harness/harness_runs/20260810_222228/probe.report.json` and
`tools/openclaw_harness/scenarios/bandit.scout_to_decision_observer_live_mcw.json`.

**Suggested treatment:** Keep the scenario, geometry, timing, identities, authoritative returned-home
fact, and returned-member count unchanged. Remove only the two redundant phase-specific strings from
the abort guard and update its focused fixture-contract test. No guideline or DFS mutation is due.

Disposition: classified as a tooling/test correction; `R-001` remains red pending final report and
decision proof.
