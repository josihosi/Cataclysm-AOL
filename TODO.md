# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active lane: **Bandit perf + persistence budget probe v0**.

Current target:
1. measure evaluator-loop cost on the named deterministic playback scenarios
   - use `doc/bandit-perf-persistence-budget-probe-v0-2026-04-20.md` as the active contract
   - reuse `src/bandit_playback.{h,cpp}` instead of inventing fresh one-off probes
2. expose obvious churn signals instead of hand-waving them away
   - candidate counts
   - repeated evaluation / path-check style work
   - any other cheap visible waste in the current bounded seam
3. estimate save-size pressure from the bounded bandit state shape the v0 docs actually need
   - marks
   - pressure / threat memory
   - any explicitly persisted return-state payload or map-knowledge slice we now claim
4. keep the measurement glue smaller than a full optimization or persistence-architecture lane

Out of scope right now:
- broad optimization theater
- final persistence schema freeze
- live harness theater as the primary first answer
- broad mutation of normal world generation
- full autonomous bandit world behavior
- extortion / diplomacy / coalition strategy
- full tactical bubble AI
- broad save-schema ambition beyond the first honest estimate
- reopening checkpointed unrelated lanes for entertainment
