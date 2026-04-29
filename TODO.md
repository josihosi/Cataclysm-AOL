# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active target: `C-AOL actual playtest verification stack v0` / `C-AOL live AI performance audit packet v0`.

Current state: the live AI performance audit has a compact matrix shell, top-level live-path timing/counter instrumentation, and green one-site plus two-site performance rows. Dedicated run `.userdata/dev-harness/harness_runs/20260429_025639/` (`performance.bandit_one_site_remembered_lead_wait_30m`) reached feature-path classification with 4/4 green step ledger, green bounded-wait ledger, and compact `bandit_live_world perf:` counters for one remembered-lead bandit camp. Dedicated run `.userdata/dev-harness/harness_runs/20260429_032427/` (`performance.two_site_bandit_cannibal_wait_30m`) reached feature-path classification with 4/4 green step ledger, green bounded-wait ledger, and compact counters for `camp_style:stalk=1,cannibal_camp:stalk=1` with `signals=0`. Player-lit fire and roof-horde proof remain blocked behind the fuel/writeback gate.

Canonical anchors for the active target:

- Stack contract: `doc/c-aol-actual-playtest-verification-stack-v0-2026-04-27.md`.
- Performance audit contract: `doc/c-aol-live-ai-performance-audit-packet-v0-2026-04-28.md`.
- Performance imagination source: `doc/c-aol-live-ai-performance-imagination-source-of-truth-2026-04-28.md`.
- Performance matrix: `doc/c-aol-live-ai-performance-matrix-v0-2026-04-29.md`.
- Completed cannibal confidence matrix: `doc/cannibal-camp-confidence-push-live-playtest-matrix-v0-2026-04-29.md`.

Next narrow work queue:

1. Inspect/stage the smallest honest three-site hostile-overmap-AI fixture/scenario for the same 30m measured window, using the green two-site `signals=0` mixed-profile row as the comparable footing.
2. Run the three-site measured row and update the matrix from claim-scoped timing/counter evidence only.
3. Extend to a four-site row only after the three-site setup is honest and comparable, or name the staging blocker.
4. Keep code changes to instrumentation or measured hot-spot fixes only; no behavior redesign.

Proof discipline:

- This is performance audit and playtest measurement, not behavior redesign.
- Live path measurement, wall-clock timing, in-game elapsed time, and counters must stay separate from deterministic or startup/load support.
- Do not promote player-lit fire or roof-horde work while the fuel/writeback gate remains blocked.
