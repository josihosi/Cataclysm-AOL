# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active: `CAOL-ZOMBIE-RIDER-0.3-v0`.

Monster/evolution footing checkpoint is green in `d50715f00e`; the large-body `SMALL_PASSAGE` / window terrain seam is green in `zombie_rider_large_body_small_passage_pathing`; local combat AI footing is green in `4343dbdad1`. Do not reprove these by ritual unless changed.

Next narrow slice: overmap light attraction/test footing only, not rider bands or live funness yet.

1. Identify the smallest existing overmap/light-attraction seam that can make late-game zombie riders notice meaningful light without becoming a camp-deletion tax.
2. Add deterministic tests for light attraction, no-light negative control, memory/decay or bounded interest, and accumulation caps.
3. Wire the smallest behavior needed for those tests while preserving the mature spawn gate and local combat constraints.
4. Validate with focused `[zombie_rider]` or narrower overmap/light tests, then decide whether the next boundary is rider convergence/bands or live-shaped local funness rows.
