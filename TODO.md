# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active lane: **Bandit scenario fixture + playback suite v0**.

Current target:
1. build the first named deterministic bandit scenarios from the current playback contract
   - use `doc/bandit-scenario-fixture-playback-suite-v0-2026-04-20.md` as the active contract
2. add playback checkpoints that can inspect behavior over time
   - at least `tick 0`, `tick 5`, `tick 20`, and one honestly justified longer horizon
3. make the playback packet answer viciousness / passivity drift questions without broadening into full world simulation
   - stay on `hold / chill`
   - investigate smoke
   - stalk edges or moving carriers
   - peel off under growing pressure
4. keep any helper glue explicitly smaller than the later perf/save-budget lane

Out of scope right now:
- broad mutation of normal world generation
- live harness theater as the primary first answer
- broad perf/save-budget work from the queued later item
- full autonomous bandit world behavior
- extortion / diplomacy / coalition strategy
- full tactical bubble AI
- broad save-schema ambition
- reopening checkpointed unrelated lanes for entertainment
