# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active target: `Bandit structural bounty overmap completion packet v0`.

Canonical contract: `doc/bandit-structural-bounty-overmap-completion-packet-v0-2026-04-30.md`.
Imagination source: `doc/bandit-structural-bounty-overmap-completion-imagination-source-of-truth-2026-04-30.md`.

Current state boundary: **Phase 4 save/load and anti-loop**. Phase 3 deterministic structural outing planner/resolver is locally green; do not start live harness work yet.

1. Add/extend serialization coverage for structural bounty leads, active structural outings, harvested/dangerous outcomes, and member active/returned state.
2. Prove arrival after reload consumes structural bounty once, not twice, and preserves the stalking-before-arrival distinction.
3. Add deterministic 500-turn playback for forest/town progression: no repeated harvested tile, no ping-pong against dangerous/low-interest tiles, and no stuck active outing.
4. Add/verify counters for dispatches planned, stalking checks, lost-interest returns, arrivals/harvests, skipped harvested/dangerous/recently-checked leads, and bounded multi-camp work.
5. Run a short Ralph Wiggum pass: save/load must not erase risk memory, harvested/dangerous debounce must survive long enough to stop repeat loops, and counters must prove bounded work rather than only final status.

Do **not** start live/harness proof yet. Do **not** implement writhing stalker, local bubble materialization, exact loot harvest, random casualty economy beyond deterministic counters, release packaging, or player/Josef-facing tuning in this phase.
