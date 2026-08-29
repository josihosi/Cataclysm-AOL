# Phase-3 frontier checkpoints and evidence retention

This index records the source-control boundary created from the preserved Phase-3 frontier.  It is
an ownership and retrieval map, not a proof-classification change.  A retained artifact keeps the
evidence class it already had; committing implementation never promotes a focused, setup, or
diagnostic result.

## Checkpoints

| Checkpoint | Claims and shared owners | Files | Existing narrow gate / retained receipt |
|---|---|---|---|
| `3ebec231ff` `Checkpoint ecology ownership and return behavior` | R-005 and R-008 active lifecycle work; R-015's owner-boundary support | `src/bandit_live_world*`, `tests/bandit_live_world_test.cpp`, and the local-scout/cannibal scenario declarations | The changed C++ test remains paired with the behavior.  Source-bound R-005/R-008 scenario and fixture records remain under `tools/openclaw_harness/{scenarios,fixtures}/`; run receipts remain under `build_logs/r005_*`. |
| `6da6fa2612` `Checkpoint native cockpit actions and controlled setup` | Shared semantic transaction owner for R-009, R-018, R-019, R-020, R-021, R-022, and R-023 | Native input/action, wait, debug mutation, and event source; item-spawn C++ test; R-018/R-020–R-023 scenario declarations | The source test and claim-specific Python adapters remain with their callable routes.  Retained report roots are `build_logs/r018_*`, `build_logs/r019_*`, `build_logs/r020_*`, `build_logs/r021_*`, `build_logs/r022_*`, and `build_logs/r023_*`. |
| `355d7689af` `Checkpoint semantic cockpit and probe substrate` | Shared harness owner for the accepted cockpit claims R-010–R-014 and active/accepted proof routes R-005, R-008, R-009, R-018–R-023 | `tools/openclaw_harness/`: public cockpit, registry, adapters, tests, profiles, scenarios, and source-bound saved fixtures | The harness test modules and fixture manifests are versioned with their routes.  At partition time, `git diff --check` and `python3 -m compileall -q tools/openclaw_harness` passed.  No settled gameplay route was replayed merely to package these files. |

The remaining documentation and policy changes form the accompanying ledger/document checkpoint.
They preserve the accepted frontier, active projection, task guidance, and the playtest witness
contract; they do not change an R-025 deadline or promote any evidence.

## Artifact dispositions

| Artifact family | Claim owner | Disposition | Retrieval / reason |
|---|---|---|---|
| `build_logs/r005_*` | R-005 active natural-route work | retain | Natural-route and certification diagnostics, including source/fixture audit and registry receipts. |
| `build_logs/r018_*` and `build_logs/r019_*` | R-018 and R-019 accepted focused witnesses | retain | Immutable focused-run, inverse-control, and cleanup evidence; not certification credit. |
| `build_logs/r020_*`, `build_logs/r021_*`, and `build_logs/r022_*` | R-020–R-022 accepted zero-credit setup work | retain | Exact setup, mutation, provenance, and cleanup receipts; not gameplay credit. |
| `build_logs/r023_*` | R-023 active movement work | retain | Raw/guarded route diagnostics retained while the exact charter authority remains unavailable. |
| `tools/openclaw_harness/fixtures/saves/**` | Claim-specific scenario owners | versioned durable fixture | Reproducible source-bound setup, including the R-008 save capsules; these are not generated run output. |
| `tools/openclaw_harness/scenario_registry.sqlite3` | Shared harness | reproducible active state | Leave untracked.  Rebuild/reconcile derives its catalog from the versioned declarations and fixtures; it is retained locally for current active work. |
| Other untracked runtime sessions below `build_logs/**` | Their matching `rNNN` claim prefix | active-run or superseded diagnostic evidence | Preserve locally unless its claim owner proves a more specific supersession.  Do not delete for cleanliness. |

No artifact-count target or clean-tree assertion is part of this boundary.  New dirty entries must
be attributed to a named active claim before they join this frontier.
