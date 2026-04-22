# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active lane: `Bandit live-world control + playtest restage packet v0`.

- Freeze the live ownership map for the relevant current bandit spawn families tied to this lane, including how overmap-special and map-extra style bandit spawns roll into one saveable owner instead of staying disconnected mapgen islands.
- Land the smallest honest world-side owner/cadence/persistence seam that can keep explicit per-site and per-spawn-tile bandit headcounts plus membership strongly enough to support real control/writeback later.
- Add the bounded near-player restage path: intentionally seed a controlled bandit camp about `10 OMT` away, with one reviewer probe/capture mode and one manual playtest handoff mode that does **not** auto-terminate the session after setup.
- Prove the live path on current build with real overmap/bubble interaction, local writeback, and a reviewer-clean perf readout covering baseline turn time, bandit-cadence turn time, spike ratio, and max turn cost.
- Keep supporting harness work bounded and subordinate to the product: no giant generic map-authoring empire, no disconnected debug theater, and no fake integration that still leaves live bandits unmanaged.
