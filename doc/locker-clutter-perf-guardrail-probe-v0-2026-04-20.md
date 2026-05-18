# Locker clutter / perf guardrail probe v0 (2026-04-20)

Status: checkpointed / done for now.

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

## Current evidence packet (2026-04-20)

The current tree now has a direct service-path probe instead of hand-wavy proxy counting.

- `basecamp::service_camp_locker_impl( npc &, camp_locker_service_probe * )` now drives the real locker-service path while optionally collecting probe metrics.
- `basecamp::measure_camp_locker_service( npc & )` exposes that seam without inventing a fake microbench path.
- `render_camp_locker_service_probe()` provides a reviewer-readable summary string for the captured counts.
- `tests/faction_camp_test.cpp` now covers:
  - junk-heavy clutter sweeps at `50 / 100 / 200 / 500 / 1000` top-level items
  - worker-count sweeps at `1 / 5 / 10`
  - junk-heavy, locker-candidate-heavy, and ammo/magazine/container-heavy stock shapes
  - loaded-vs-empty magazines and filled-vs-empty bags on the same top-level item counts

Current measured verdict:
- current service cost is dominated by top-level locker items and worker passes, with straight-line scan growth across the bounded matrix above
- loaded magazines and ordinary filled bags currently behave like one top-level locker item for this service-path probe, not like a hidden nested-cost cliff
- no immediate guardrail is forced by this first packet, so the honest verdict is `fine for now`, with future mitigation only warranted if later timing evidence says the linear top-level scan is becoming too expensive in practice

## Validation expectations

- prefer deterministic or directly instrumented locker service evidence first
- keep the packet reviewer-readable and tied to the real code path in `basecamp.cpp`
- bias the stress shape toward heavy item hoards, because that is normal player behavior here, while NPC counts above about ten are much less common
- escalate to broader live/harness play only if the measurement seam honestly cannot answer the question without it

## Validation snapshot

- `make -j4 tests`
- `./tests/cata_test "[camp][locker]"`

## Closure note

This item is checkpointed because the first bounded direct packet now exists and already answers the intended question honestly enough.
Reopen it only if a concrete new timing-specific locker perf question appears.

## Dependency note

This item originally became active because the bandit perf lane was honestly checkpointed.
Do not smuggle old bandit perf questions back into this locker lane just because both slices contain the word `perf`.
