# Locker clutter / perf guardrail probe v0 (2026-04-20)

Status: active lane contract.

## Why this exists

This is not imaginary fussing. In normal Cataclysm play, items get hoarded, and the first thing many players will do with a `CAMP_LOCKER` zone is spread it under a mountain of crap and assume the NPCs can sort it out.

If that behavior makes locker service expensive, the likely fixes are small and blunt. We should measure it honestly before the zone teaches bad habits and then punishes them invisibly.

## Scope

1. Measure the real `CAMP_LOCKER` service path against clutter-heavy locker stock, not a fake isolated microbench.
2. Sweep bounded item counts that are plausible for player hoarding, starting with `50 / 100 / 200 / 500 / 1000` top-level items.
3. Sweep bounded worker counts that match likely play more than fantasy camps, roughly `1 / 5 / 10` assigned NPCs.
4. Compare at least three item-mix shapes:
   - junk-heavy clutter
   - locker-candidate-heavy gear
   - ammo / magazine / container-heavy clutter
5. Answer the nested-content question explicitly: do loaded magazines and common containers mostly behave like one top-level scan unit, or do their contents create meaningful extra cost on the locker path?
6. If the cost is real, recommend the cheapest mitigation tier first instead of jumping straight to architecture opera.

## Non-goals

- broadening this into adjacent locker work just because it is the active lane now
- redesigning the whole locker system
- broad caching or persistence architecture unless measurement clearly forces it
- reopening already-closed locker claims for sport
- pretending giant NPC populations are the primary risk when real games usually hoard items far harder than they grow camp staff

## Success shape

This item is good enough when:
1. one honest measurement packet exists for the real locker service path across the first bounded clutter matrix
2. the packet can say where the cost curve starts looking fine, suspicious, or stupid
3. the loaded-magazine / container question has a concrete answer instead of vibes
4. if the curve looks bad, the packet ends with a small recommended guardrail order, such as early junk-ignore, bounded candidate consideration, or a simple curated-stock warning/cap

## Validation expectations

- prefer deterministic or directly instrumented locker service evidence first
- keep the packet reviewer-readable and tied to the real code path in `basecamp.cpp`
- bias the stress shape toward heavy item hoards, because that is normal player behavior here, while NPC counts above about ten are much less common
- escalate to broader live/harness play only if the measurement seam honestly cannot answer the question without it

## Dependency note

This item is active because the bandit perf lane is now honestly checkpointed.
Do not smuggle old bandit perf questions back into this locker lane just because both slices contain the word `perf`.
