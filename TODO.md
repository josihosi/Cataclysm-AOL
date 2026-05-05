# TODO

Short execution queue only.

Remove finished items when they are done.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active lane: `CAOL-DEFENDED-CAMP-SIGHT-SMOKE-HARDENING-v0`.

Contract: `doc/defended-camp-sight-smoke-hardening-packet-v0-2026-05-05.md`.

Parent checkpoint: `CAOL-JOSEF-LIVE-DEBUG-BATCH-v0`.

Current task: execute one bounded hardening packet for defended-camp sight/smoke behavior. Prove that bandit watchers and compatible cannibal stalk/attack-pressure profiles react honestly to current LoS and player smoke instead of visible loitering, hot-doorstep pickup, or same-tile smoke camping.

Next concrete steps:
- inspect the existing Slice 2 proof and the shared sight/smoke rows in `doc/josef-live-debug-batch-test-matrix-v0-2026-05-03.md`;
- add/repair deterministic or source-path gates for current-LoS, recent-LoS, cover/no-cover fallback, and smoke-obscured lead handling;
- run staged/live proof rows for bandit sighted watcher, bandit smoke-out, and cannibal outcome-split sight/smoke behavior;
- update the packet/SUCCESS/TESTING with run ids, evidence class, and claim boundary.

Do not rerun completed eleven-slice debug-stack proof as ritual.
