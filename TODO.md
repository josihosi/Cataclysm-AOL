# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active target: `Bandit structural bounty overmap completion packet v0`.

Canonical contract: `doc/bandit-structural-bounty-overmap-completion-packet-v0-2026-04-30.md`.
Imagination source: `doc/bandit-structural-bounty-overmap-completion-imagination-source-of-truth-2026-04-30.md`.

Current state boundary: **Phase 5 live wiring**. Phase 4 save/load and anti-loop is locally green; live/harness proof waits until the real `do_turn` maintenance path is wired and built.

1. Inspect the current `do_turn` / live-world maintenance cadence and identify the cheapest overmap terrain lookup seam for structural bounty scans.
2. Wire `advance_structural_bounty_scan`, `plan_structural_bounty_outing`, `apply_structural_bounty_outing_plan`, and `advance_structural_bounty_outings` into live maintenance with strict per-turn cadence/budget and no player bait requirement.
3. Add concise debug/report artifacts for scan/outing work: sampled/seeded/suppressed leads, dispatches planned, stalking checks, turnbacks, arrivals/harvests, and active/returned member state.
4. Add/extend source-hook or deterministic tests proving the live maintenance path invokes structural scan/outing work, respects active pressure/debounce, and remains bounded.
5. Build the current runtime after wiring, then promote the Phase 6 harness scenario only when this substrate is honestly green.

Do **not** start live/harness proof yet. Do **not** implement writhing stalker, local bubble materialization, exact loot harvest, random casualty economy beyond deterministic counters, release packaging, or player/Josef-facing tuning in this phase.
