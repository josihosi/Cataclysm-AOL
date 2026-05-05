# Andi handoff

Active lane: `CAOL-DEFENDED-CAMP-SIGHT-SMOKE-HARDENING-v0`.

Contract: `doc/defended-camp-sight-smoke-hardening-packet-v0-2026-05-05.md`.

Parent checkpoint: `CAOL-JOSEF-LIVE-DEBUG-BATCH-v0`.

Parent imagination source: `doc/josef-live-debug-batch-imagination-source-2026-05-03.md`.

Parent test matrix: `doc/josef-live-debug-batch-test-matrix-v0-2026-05-03.md`.

Standing build cadence: `doc/andi-build-cadence-note.md`.

## Current ask

Execute the bounded defended-camp sight/smoke hardening packet. The old eleven-slice debug stack is checkpointed green; do not rerun those rows as comfort work.

The promoted target is the deferred Slice 2 hardening row: bandit watchers and compatible cannibal stalking/attack-pressure profiles should react honestly to current LoS and player smoke instead of standing visible, casing hot defended doorsteps, or camping inside smoke.

## Evidence boundary

Minimum proof:
- deterministic/source-path gates for current LoS, recent LoS, cover/no-cover fallback, and smoke-obscured lead handling;
- staged/live bandit sighted-watcher row;
- staged/live bandit smoke-out row;
- cannibal sight/smoke outcome-split proof with no shakedown leakage;
- no stale-binary/runtime-version mismatch for feature-path claims.

Claim only what the evidence reaches: staged/live feature-path proof is enough for this packet, but not natural random discovery, full vertical assault, full cannibal raid/contact, or tile-perfect smoke physics.

## Current checkpoint

2026-05-05 smoke-out checkpoint is green. Code now feeds live `fd_smoke` into local-gate/sight-avoid decisions; deterministic/source-path `bandit_live_world*` is green at 43 cases / 1002 assertions; tiles build is green; staged/live `bandit.scout_stalker_smoked_watcher_live` run `.userdata/dev-harness/harness_runs/20260505_101407/` is `feature-path` / `feature_proof=true` with 11/11 green step-local rows and same-run smoke-obscured `local_gate` hold-off plus bounded adjacent sight-avoid reposition.

Remaining active proof before closure: staged/live currently-sighted bandit watcher row.

## Next review/action

Move this lane to its next honest state boundary, update canon/evidence docs, commit/push if a clean state boundary is reached, then leave a concise `agent-visible only` report for Augerl/Frau review.
