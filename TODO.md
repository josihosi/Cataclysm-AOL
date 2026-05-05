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
- remaining proof: staged/live currently-sighted bandit watcher row where the active scout breaks LoS, backs off, reroutes, escalates/reports, or logs a concrete blocker and does not keep hot-doorstep pickup behavior;
- after that row, align `Plan.md`, `TODO.md`, `TESTING.md`, `SUCCESS.md`, `andi.handoff.md`, and `doc/work-ledger.md` for packet closure or the next honest blocker;
- do not rerun the green 2026-05-05 smoke-out row unless the claim or code changes.

Current green checkpoint: deterministic/source-path smoke and cannibal split gates plus staged/live bandit smoke-out proof are green in `.userdata/dev-harness/harness_runs/20260505_101407/` (`feature-path`, `feature_proof=true`).

Do not rerun completed eleven-slice debug-stack proof as ritual.
