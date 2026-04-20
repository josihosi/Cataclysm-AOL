# Locker lag-threshold probe v0 (2026-04-20)

Status: greenlit backlog contract.

## Why this exists

The first locker clutter / perf packet answered the shape question reasonably well:
service cost grew with top-level locker items and worker passes, and ordinary filled bags or loaded magazines did not reveal a hidden nested-content cliff.

But that first packet did **not** yet answer the sharper player-facing question:

> around what amount of locker clutter does the real `CAMP_LOCKER` service path start looking suspicious or actually laggy?

This follow-up exists to hunt for that rough threshold honestly instead of pretending the first bounded shape probe already answered it.

## Scope

1. Stay on the real `CAMP_LOCKER` service path and direct probe seam rather than switching to fake isolated microbench theater.
2. Extend the top-level clutter sweep beyond the first `50 / 100 / 200 / 500 / 1000` packet until a rough knee / suspicious zone / bad zone appears, or until the tested bound still looks fine.
3. Keep worker-count sweeps bounded around realistic camp sizes such as `1 / 5 / 10`, using item-hoard pressure as the main stress axis.
4. Produce a reviewer-readable threshold answer such as `fine through X`, `suspicious around Y`, `bad by Z`, or honestly `no clear threshold found within the tested bound`.
5. If the curve starts looking bad, end with the cheapest guardrail order first instead of jumping straight to architecture opera.

## Non-goals

- implementing the mitigation in this same packet
- redesigning the locker system
- treating fantasy-huge NPC camps as the main stress shape
- ceremonial reruns of the old packet without a sharper timing question
- claiming felt player lag from vibes alone if the packet never actually establishes a threshold

## Success shape

This item is good enough when:
1. one honest threshold packet exists for the real locker service path
2. the packet distinguishes top-level item pressure from worker-count pressure
3. the result can name an approximate fine / suspicious / bad range, or honestly report that the threshold was not found within the tested bound
4. if the threshold looks bad, the packet ends with a small cheap-first guardrail recommendation order

## Validation expectations

- prefer directly instrumented or timing-based locker service evidence first
- keep the packet reviewer-readable and tied to the real `basecamp.cpp` service path
- bias the stress search toward heavy item hoards, because that is the believable player behavior here
- escalate to broader live or harness proof only if the direct seam cannot honestly answer the threshold question

## Dependency note

This queued follow-up exists because `doc/locker-clutter-perf-guardrail-probe-v0-2026-04-20.md` answered shape, not the rough lag threshold.
Do not reopen the old packet just to restate the same conclusion more loudly.
