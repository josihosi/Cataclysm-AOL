# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active target: `C-AOL actual playtest verification stack v0` / `C-AOL live AI performance audit packet v0`.

Current state: the live AI performance audit has a compact matrix shell, top-level live-path timing/counter instrumentation, and green one-site, two-site, and pre-staged three-site performance rows. Dedicated run `.userdata/dev-harness/harness_runs/20260429_025639/` (`performance.bandit_one_site_remembered_lead_wait_30m`) reached feature-path classification with 4/4 green step ledger, green bounded-wait ledger, and compact `bandit_live_world perf:` counters for one remembered-lead bandit camp. Dedicated run `.userdata/dev-harness/harness_runs/20260429_032427/` (`performance.two_site_bandit_cannibal_wait_30m`) reached feature-path classification with 4/4 green step ledger, green bounded-wait ledger, and compact counters for `camp_style:stalk=1,cannibal_camp:stalk=1` with `signals=0`; note that its saved-state preflight proved only the remembered-lead bandit site, while the cannibal site/profile was proven later by same-run `abstract_bootstrap`/dispatch/perf lines. Direct-range three-site attempt `.userdata/dev-harness/harness_runs/20260429_035001/` is blocked/non-credit: it proves both added cannibal sites bootstrapped and dispatched, but every perf row is `sites=3 active_sites=2 active_job_mix=cannibal_camp:stalk=2`, the remembered-lead bandit dispatch line is absent, and the wait ledger is yellow. Schani/Josef greenlit attempt 3 as a pre-staged performance-load row rather than natural dispatch proof; run `.userdata/dev-harness/harness_runs/20260429_040926/` (`performance.three_site_prestaged_active_wait_30m`) is green feature-path with 6/6 green step ledger, green 30m wait ledger, preflight-proven distinct active non-player-target jobs, and same-run `sites=3 active_sites=3 active_job_mix=camp_style:stalk=1,cannibal_camp:stalk=2 signals=0` perf counters. Player-lit fire and roof-horde proof remain blocked behind the fuel/writeback gate.

Canonical anchors for the active target:

- Stack contract: `doc/c-aol-actual-playtest-verification-stack-v0-2026-04-27.md`.
- Performance audit contract: `doc/c-aol-live-ai-performance-audit-packet-v0-2026-04-28.md`.
- Performance imagination source: `doc/c-aol-live-ai-performance-imagination-source-of-truth-2026-04-28.md`.
- Performance matrix: `doc/c-aol-live-ai-performance-matrix-v0-2026-04-29.md`.
- Completed cannibal confidence matrix: `doc/cannibal-camp-confidence-push-live-playtest-matrix-v0-2026-04-29.md`.

Next narrow work queue:

1. Preserve the direct-range three-site attempt as blocked/cap evidence, not row credit; do not rerun the two-direct-cannibal recipe blindly.
2. Preserve the pre-staged three-site row as performance-load evidence only, not natural dispatch behavior proof.
3. Stage the four-site row only under an explicit contract. Recommended comparable footing: pre-staged active performance-load jobs with separated site ids/anchors/target ids, no smoke/fire/map-field signals, preflight proving all active jobs and live/findable members, green 30m wait ledger, and same-run `sites=4 active_sites=4` perf/job-mix lines.
4. If natural four-site player-pressure behavior is desired instead, stop and ask Schani/Josef; the current cap makes that a behavior/contract question.
5. Keep code changes to instrumentation or measured hot-spot fixes only; no behavior redesign.

Proof discipline:

- This is performance audit and playtest measurement, not behavior redesign.
- Live path measurement, wall-clock timing, in-game elapsed time, and counters must stay separate from deterministic or startup/load support.
- Do not promote player-lit fire or roof-horde work while the fuel/writeback gate remains blocked.
