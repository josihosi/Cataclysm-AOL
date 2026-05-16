# C-AOL live AI performance audit packet v0 - 2026-04-28

Status: GREENLIT / BOTTOM-OF-STACK PERFORMANCE AUDIT

Imagination source: `doc/c-aol-live-ai-performance-imagination-source-of-truth-2026-04-28.md`

## Intent

Josef requested one final Andi-side test family for game performance: not just “does the logic work?”, but “does the game still play when multiple overmap hostile AIs are doing their thing?”

This packet should measure and audit the live bandit/cannibal overmap AI path with several active hostile sites, then produce either a green performance note or a concrete list of hot spots/fixes.

## Scope

1. Build a measured live/harness scenario family with one, two, three, and four active hostile overmap AI sites.
2. Include at least two profile/job shapes when feasible:
   - cannibal camp scout/stalk/night-attack-window pressure;
   - bandit camp scout/watch/return-home or remembered-target risk/reward pressure.
3. Measure a bounded live time window through the real game path. Prefer a real `wait_action`/bounded wait primitive over dot-spam where possible.
4. Capture enough counters to explain cost:
   - active hostile site count;
   - active groups/jobs by profile;
   - signal/mark counts considered;
   - dispatch/local-gate/sight-avoid evaluations;
   - wall-clock duration for the measured window and any per-cadence slices available.
5. Run a code-level performance audit of the current live AI path, especially `src/do_turn.cpp`, `src/bandit_live_world.cpp`, serialization/save state, signal matching, local-gate evaluation, sight/exposure checks, and report/log emission.
6. Add lightweight instrumentation or a deterministic benchmark helper only if existing evidence cannot separate game cost from harness overhead.
7. Apply small low-risk optimizations only when a measured bottleneck is clear. Otherwise report the performance envelope and leave behavior untouched.

## Non-goals

- No behavior redesign for bandits or cannibals.
- No release publishing.
- No broad refactor without measured cause.
- No claim that deterministic evaluator speed proves live game performance.
- No load-and-close closure.
- No giant log dump as a substitute for timing/counter evidence.

## Suggested scenario shape

The scenario family should compare:

- baseline / zero or one active hostile AI;
- two active hostile AIs;
- three active hostile AIs;
- four active hostile AIs.

Each measured run should record the same kind of live time window so the result is comparable. If exact 2/3/4-site live fixtures are too expensive to construct immediately, Andi should first land the narrowest fixture/instrumentation needed to create them honestly, then run the smallest measured slice.

A useful first measured window is a bounded overmap-AI cadence window such as 30 in-game minutes to several hours, depending on which jobs need to fire. The packet should name why the chosen window is sufficient.

## Success state

- A live/harness performance matrix exists with one, two, three, and four active hostile overmap AI site cases, or an explicit blocker explains which cases could not be staged honestly.
- Each measured case names profile/job mix, active site count, in-game elapsed time, wall-clock measurement, and relevant counters/log fields.
- The measurement reaches the real live game path for dispatch/local-gate/sight-avoid/signal work; evaluator-only or startup/load evidence is classified as support only.
- The code audit identifies the likely hot loops and data-scaling risks, even if no optimization is needed.
- Any optimization is tied to a measured bottleneck and has before/after evidence.
- Reports/logs stay compact and reviewer-readable.
- The final verdict says one of: green enough for current playtest scale, green with watchlist, blocked by missing fixture/instrumentation, or red with concrete hot spots.

## Validation burden

Minimum gates:

- `git diff --check`
- narrow compile for touched runtime/test files
- focused deterministic test or benchmark if one is added
- live/harness measured runs with artifact paths and small extracted timing/counter summaries

The final report must separate:

- **game path:** what actually ran in the live turn/wait path;
- **timing:** wall-clock and in-game elapsed measurement;
- **counters:** active sites/jobs/signals/evaluations;
- **code audit:** hot-path risks and any fixes;
- **verdict:** green / watchlist / blocked / red.
