# Bandit live-world control + playtest restage packet v0 (2026-04-22)

Status: ACTIVE / GREENLIT.

## Greenlight verdict

Josef corrected the product bar plainly: this seam is not allowed to stop at one showcase anchor, one cute harness trick, or one disconnected proof packet.
If Andi's bandit work is meant to exist in the real game, the live bandit spawns themselves need to come under that system, and the resulting setup needs to be easy enough to restage nearby that Schani and Josef can actually playtest it and measure turn cost.

This packet therefore absorbs the remaining useful playtest-kit-v2 helper work instead of killing it, but makes that helper surface subordinate to the real product: live owned bandit spawns with controllable nearby restage.

## Why this packet exists

The current repo has real hostile bandit content in play, but it is still mostly static-content / mapgen footing.
The newer `src/bandit_*` stack has evaluator, mark-generation, and handoff substrate, but it does not yet honestly own the bandits that spawn into the world.

That gap is now the real product problem.
A broad playtest harness without live ownership would just be more elegant theater.
Likewise, a live owner without a restageable near-player setup would still leave playtesting and perf measurement too awkward to use.

So this packet binds the two needs together:
- bring the real current bandit spawn families tied to this lane under one live owner/control path
- make that live system deliberately restageable near the player/basecamp for repeatable testing and performance measurement

## Scope

1. Connect the relevant current bandit spawn families tied to this lane — including the real overmap-special and map-extra style bandit site/spawn footing — to one live saveable owner instead of leaving them as disconnected static spawns.
2. Track explicit per-site and per-spawn-tile bandit headcount / membership strongly enough that the system can reason about who exists there and what has changed.
3. Let the live system actually control, dispatch, reconcile, and cool those spawned bandits through the existing bandit evaluator / handoff substrate instead of only counting them after they already spawned.
4. Add one bounded cadence/tick and persistence path so the live owner survives save/load and updates on the real world side rather than only in playback/tests.
5. Add a bounded playtest restage helper that can intentionally seed a controlled bandit camp about `10 OMT` away from the player or current basecamp footing.
6. Split that restage helper into two honest modes:
   - **probe / capture mode** may collect artifacts and clean up afterwards
   - **playtest handoff mode** must leave the game/session running after setup instead of auto-terminating it
7. Produce a reviewer-clean performance readout on that real nearby setup, including baseline turn time, bandit-cadence turn time, spike ratio, and max turn cost.
8. Carry forward only the harness/helper work from `Bandit + Basecamp playtest kit packet v2` that directly serves this live-world-control packet.

## Non-goals

- every hostile human system in the whole game
- a giant generic map-authoring or scenario-authoring empire
- harness-only theater that still leaves live mapgen bandits unmanaged
- a full faction grand-strategy rewrite
- perfect declarative world authoring for every possible state on day one
- deleting useful playtest-kit helper work just because the headline lane changed

## Success shape

This packet is good enough when:
1. one live saveable owner exists for the relevant current bandit spawn families tied to this lane
2. the owner keeps explicit per-site / per-spawn-tile headcounts and membership rather than treating spawned bandits as anonymous folklore
3. the system can actually control or dispatch those spawned bandits through the live world path instead of leaving them as disconnected `place_npcs` islands
4. a controlled bandit camp can be restaged about `10 OMT` away on demand on current build
5. that restage can be used both for reviewer capture and for manual playtesting, with the manual handoff mode **not** auto-terminating the session
6. the setup exercises the real overmap/bubble handoff plus local writeback path rather than a fake isolated theater
7. local outcomes change later world behavior instead of leaving the live owner stale
8. a reviewer-clean perf report exists for that nearby live setup, with the concrete metrics above
9. the slice stays bounded instead of widening into a generic hostile-human or debug-platform empire

## Validation expectations

This packet touches real game-state ownership, persistence, live restage, and likely some supporting harness glue.
So validation should stay explicit and mixed:

- deterministic bridge coverage for the live owner / headcount / save-load path where practical
- narrow deterministic coverage for any helper logic that can be tested outside the full live run
- fresh current-build live proof that the nearby controlled camp can be restaged on demand
- fresh current-build proof that the manual handoff path leaves the playtest session running after setup
- reviewer-readable evidence that the live system, nearby restage, and later writeback all exercised the real path instead of a private fake path
- a concrete perf packet/report on the nearby setup using baseline turn time, cadence turn time, spike ratio, and max turn cost

## Review questions this packet should answer

- Do the real live bandit spawns now belong to Andi's system instead of merely coexisting beside it?
- Are bandit counts/headcounts now explicit enough that later control and writeback are honest rather than guessed?
- Can Schani and Josef deliberately restage a nearby bandit camp quickly enough to use this as an actual playtest surface?
- Does the handoff mode now leave the game alive for manual play rather than rudely cleaning up the moment it gets interesting?
- Did we get concrete performance numbers on the real nearby setup instead of hand-waved vibes?
- Did the helper/tooling follow-through stay in service of the product instead of taking over as the product?

## Position in the sequence

This is now the active lane.
The useful open helper work from `Bandit + Basecamp playtest kit packet v2` survives here as supporting scope, but it is no longer allowed to masquerade as the whole product by itself.
