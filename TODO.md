# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active: `CAOL-ZOMBIE-RIDER-0.3-v0`.

Monster/evolution footing checkpoint is green in `d50715f00e`; the large-body `SMALL_PASSAGE` / window terrain seam is green in `zombie_rider_large_body_small_passage_pathing`; local combat AI footing is green in `4343dbdad1`; overmap light-attraction footing is green in `d2ffbd54c3`. Do not reprove these by ritual unless changed.

Next narrow slice: rider convergence/band formation test footing only, not live funness yet.

1. Identify the smallest existing overmap/map-AI seam that can turn bounded rider light interest into rider convergence without private horde magic or infinite clown cavalry.
2. Add deterministic tests for rider convergence, band formation, accumulation caps across repeated light interest, and no-convergence when interest is inactive/decayed.
3. Wire the smallest behavior needed for those tests while preserving the mature spawn gate, max light draw cap, and local combat constraints.
4. Validate with focused `[zombie_rider]` or narrower map-AI tests, then decide whether the next boundary is live-shaped local funness rows.
