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

2026-05-05 defended-camp sight/smoke packet is agent-side green pending Frau/Augerl review. Code feeds live `fd_smoke` into local-gate/sight-avoid decisions and keeps smoke-obscured sight-avoid reasons explicit; deterministic/source-path `bandit_live_world*` is green at 43 cases / 1002 assertions; tiles build and `git diff --check` are green. Staged/live current-sight row `bandit.scout_stalker_sight_avoid_live` run `.userdata/dev-harness/harness_runs/20260505_102525/` is `feature-path` / `feature_proof=true` with 10/10 green step-local rows and bounded adjacent exposed-scout reposition. Staged/live smoke row `bandit.scout_stalker_smoked_watcher_live` run `.userdata/dev-harness/harness_runs/20260505_102629/` is `feature-path` / `feature_proof=true` with 11/11 green step-local rows and smoke-obscured hold-off plus bounded adjacent sight-avoid reposition.

Remaining action: Frau/Augerl review; if accepted, Schani should close/retitle this packet and promote the next greenlit lane. No routine Andi reprobe is pending.

## Next review/action

Frau/Augerl should review the checkpointed proof and claim boundary. If accepted, Schani should close/retitle this packet and promote the next specific greenlit lane; no routine Andi reprobe is pending.
