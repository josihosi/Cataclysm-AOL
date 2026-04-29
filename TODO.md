# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active target: `C-AOL actual playtest verification stack v0` / `C-AOL live AI performance audit packet v0`.

Current state: the live AI performance audit has a compact matrix shell, top-level live-path timing/counter instrumentation, and green one-site plus two-site performance rows. Dedicated run `.userdata/dev-harness/harness_runs/20260429_025639/` (`performance.bandit_one_site_remembered_lead_wait_30m`) reached feature-path classification with 4/4 green step ledger, green bounded-wait ledger, and compact `bandit_live_world perf:` counters for one remembered-lead bandit camp. Dedicated run `.userdata/dev-harness/harness_runs/20260429_032427/` (`performance.two_site_bandit_cannibal_wait_30m`) reached feature-path classification with 4/4 green step ledger, green bounded-wait ledger, and compact counters for `camp_style:stalk=1,cannibal_camp:stalk=1` with `signals=0`; note that its saved-state preflight proved only the remembered-lead bandit site, while the cannibal site/profile was proven later by same-run `abstract_bootstrap`/dispatch/perf lines. Direct-range three-site attempt `.userdata/dev-harness/harness_runs/20260429_035001/` is blocked/non-credit: it proves both added cannibal sites bootstrapped and dispatched, but every perf row is `sites=3 active_sites=2 active_job_mix=cannibal_camp:stalk=2`, the remembered-lead bandit dispatch line is absent, and the wait ledger is yellow. Player-lit fire and roof-horde proof remain blocked behind the fuel/writeback gate.

Canonical anchors for the active target:

- Stack contract: `doc/c-aol-actual-playtest-verification-stack-v0-2026-04-27.md`.
- Performance audit contract: `doc/c-aol-live-ai-performance-audit-packet-v0-2026-04-28.md`.
- Performance imagination source: `doc/c-aol-live-ai-performance-imagination-source-of-truth-2026-04-28.md`.
- Performance matrix: `doc/c-aol-live-ai-performance-matrix-v0-2026-04-29.md`.
- Completed cannibal confidence matrix: `doc/cannibal-camp-confidence-push-live-playtest-matrix-v0-2026-04-29.md`.

Next narrow work queue:

1. Preserve the direct-range three-site attempt as blocked/cap evidence, not row credit; do not rerun the two-direct-cannibal recipe blindly.
2. Decide the next three-site footing: either stage a materially different remembered-lead / persisted-active hostile job with explicit setup proof, or ask Schani/Josef to revise the row contract to a cap-aware `sites=3 active_sites=2` measurement.
3. Add one setup audit that explicitly proves all intended sites/profiles before measurement, or label any abstract-bootstrap-only site proof separately.
4. Run the three-site measured row and update the matrix from claim-scoped timing/counter evidence only, requiring same-run `sites=3 active_sites=3` perf/job-mix lines unless the row contract is explicitly revised.
5. Extend to a four-site row only after the three-site setup is honest and comparable, or name the staging blocker.
6. Keep code changes to instrumentation or measured hot-spot fixes only; no behavior redesign.

Proof discipline:

- This is performance audit and playtest measurement, not behavior redesign.
- Live path measurement, wall-clock timing, in-game elapsed time, and counters must stay separate from deterministic or startup/load support.
- Do not promote player-lit fire or roof-horde work while the fuel/writeback gate remains blocked.
