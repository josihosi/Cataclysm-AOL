# Andi handoff: CAOL-FLESH-RAPTOR-CIRCLING-SKIRMISHER-v0

## Current canon state

`CAOL-FLESH-RAPTOR-CIRCLING-SKIRMISHER-v0` is **ACTIVE / GREENLIT / PREDATOR VARIETY PACKAGE / DETERMINISTIC CIRCLING FOOTING NEXT**.

Authoritative canon is `Plan.md`, with `TODO.md`, `SUCCESS.md`, `TESTING.md`, and `doc/work-ledger.md` aligned downstream. This handoff is only a terse executor packet; if it disagrees with canon, repair it from canon before continuing.

`CAOL-ZOMBIE-RIDER-0.3-v0` is closed/checkpointed green v0 initial dev. Do not rerun its endpoint, local-combat, overmap-light, convergence/band, or staged live funness rows by ritual unless Schani/Josef explicitly promote a stricter follow-up.

Prior closed lanes that must not be reopened by drift: zombie rider, writhing-stalker lanes, roof-horde, Smart Zone, old fire proof lanes, `CAOL-MULTI-CAMP-SIGNAL-GAUNTLET-v0`, bandit/horde proof lanes, and release packaging.

## Product target

Retrofit flesh raptors from plain `HIT_AND_RUN` annoyance into visible circling skirmishers. In open terrain they should prefer readable 4–6 tile orbit/flank pressure, under-occupied arcs, bounded swoop cadence, and graceful fallback in corridors/blocked terrain, without increasing equipment-destruction tedium or globally rewriting all attack-and-retreat enemies.

## Canonical packet

- Imagination source: `doc/predator-behavior-variety-imagination-source-of-truth-2026-05-01.md`
- Contract: `doc/flesh-raptor-circling-skirmisher-packet-v0-2026-05-01.md`

## Current checkpoint

No flesh raptor behavior code has changed yet. The lane was promoted after zombie-rider closure.

Next narrow slice: deterministic circling footing.

1. Audit current flesh raptor IDs/variants and their existing `HIT_AND_RUN` / special-attack behavior.
2. Identify the smallest live-consumed seam for raptor-only orbit/flank behavior.
3. Add or adapt minimal scoring for 4–6 tile spacing, lateral/orbit movement, passable open tiles, under-occupied arcs, and crowding penalty.
4. Add deterministic little-map tests for open-space orbit preference, under-occupied arc preference, corridor/blocked fallback, and cadence/hysteresis jitter guard.
5. Only after deterministic footing is green, widen to live/playtest rows with orbit turns, swoop cadence, hit/equipment events, counterplay outcome, warnings/errors, and turn-time cost.

## Evidence/reporting requirements

For deterministic tests, name the live seam under test and the behavior each row would catch if broken. For live rows, report scenario/test name, run/artifact path, start conditions, orbit/swoop/retreat counts, target-tile churn, hit/equipment-damage events, pass/fail/yellow verdict, caveats, warnings/log spam/crash status, and available timing/perf.

## Non-goals/cautions

- Do not remove every attack-and-retreat enemy from the game.
- Do not redesign eigenspectres or other portal-storm enemies in this package.
- Do not create a global pack-AI rewrite unless the narrow raptor seam proves impossible.
- Do not make raptors harder by raising damage, armor piercing, or equipment destruction.
- Do not start the writhing-stalker zombie-shadow package or anti-redundancy refactors by drift.
