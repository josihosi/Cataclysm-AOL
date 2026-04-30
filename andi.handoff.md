# Andi handoff: CAOL-ZOMBIE-RIDER-0.3-v0

## Current canon state

`CAOL-ZOMBIE-RIDER-0.3-v0` is **ACTIVE / GREENLIT INITIAL DEV STARTED AFTER WRITHING-STALKER LIVE FUN CLOSURE**.

Authoritative canon is `Plan.md`, with `TODO.md`, `SUCCESS.md`, `TESTING.md`, and `doc/work-ledger.md` aligned downstream. This handoff is only a terse executor packet; if it disagrees with canon, repair it from canon.

Prior closed lanes that must not be reopened by drift: writhing-stalker lanes, roof-horde, Smart Zone, old fire proof lanes, `CAOL-MULTI-CAMP-SIGNAL-GAUNTLET-v0`, and release packaging.

## Product target

Release `0.3` zombie rider initial dev: endpoint late-game mounted ranged predator, exact flavor preservation, scary-fast movement, shoot/flee/reposition local combat, overmap light attraction, rider convergence/band formation, and deterministic/live funness proof that open-ground terror remains fair through counterplay.

## Canonical packet

- Raw intake / exact flavor text: `doc/zombie-rider-raw-intake-2026-04-30.md`
- Imagination source: `doc/zombie-rider-0.3-imagination-source-of-truth-2026-04-30.md`
- Contract: `doc/zombie-rider-0.3-initial-dev-packet-v0-2026-04-30.md`
- Map-AI / funness benchmark suite: `doc/zombie-rider-map-ai-funness-benchmark-suite-v0-2026-04-30.md`

## Exact flavor text — preserve punctuation

```text
Up on, what can only be described as, a six legged horse - or is it a spider? - a towering figure, with eyes the color of blood, holds a gory bow of wet bones and sinews.
It's movement ferocious, as the tumbling feet hasten across the terrain.
Running is out of the question.
```

Do not “fix” `It's`, commas, dashes, or rhythm. Josef explicitly said the weird punctuation is intentional.

## Immediate slice

Start with monster/evolution footing only:

1. Add/preserve actual `mon_zombie_rider` definition with the exact description text.
2. Choose and document the endpoint lineage (default smell: hunter/predator branch into rider after mature-world pressure, not year-one routine spawn).
3. Add the smallest deterministic/source validation proving the rider definition exists, movement footing is scary-fast-but-reviewable, and early/routine spawn/evolution gates do not leak riders.

Only after this checkpoint should the lane widen to local shoot/flee/reposition AI, overmap light attraction, rider bands, and live funness rows.

## Evidence/reporting requirements

For each later test/playtest row, report scenario/test name, run/artifact path, start conditions, decision trace, rider count, band state, shot/reposition/retreat counts, pass/fail/yellow verdict, caveats, warnings/log spam/crash status, and available timing/perf.

## Non-goals/cautions

- Do not make this a full release `0.3` omnibus.
- Do not make the rider a year-one routine spawn, decorative fast archer, instant camp-deletion tax, or infinite clown cavalry.
- Do not claim fun from JSON/stat presence alone; gameplay claims need tests and/or live-shaped scenarios.
