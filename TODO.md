# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active lane: **Locker clutter / perf guardrail probe v0**.

Current target:
1. identify and instrument the real `CAMP_LOCKER` service path in `basecamp.cpp`
   - use `doc/locker-clutter-perf-guardrail-probe-v0-2026-04-20.md` as the active contract
   - prefer deterministic or directly instrumented service evidence before any harness theater
2. sweep a first bounded clutter matrix that matches likely play
   - top-level item counts around `50 / 100 / 200 / 500 / 1000`
   - worker counts around `1 / 5 / 10`
   - at least junk-heavy, locker-candidate-heavy, and ammo/magazine/container-heavy stock shapes
3. answer the nested-content question explicitly
   - do loaded magazines and ordinary containers mostly behave like one top-level locker item
   - or do their contents create meaningful extra cost on the service path
4. if the curve looks bad, finish with the cheapest guardrail order first
   - early junk-ignore
   - bounded candidate consideration
   - simple curated-stock warning/cap

Out of scope right now:
- redesigning the whole locker system
- broad caching or persistence architecture unless measurement clearly forces it
- reopening checkpointed locker claims for entertainment
- pretending giant NPC populations are the primary risk when real games hoard items much harder than they grow camp staff
- smuggling this locker perf lane back into bandit work just because both slices say `perf`
