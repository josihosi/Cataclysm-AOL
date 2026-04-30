# Andi handoff: CAOL-ZOMBIE-RIDER-0.3-v0

## Current canon state

`CAOL-ZOMBIE-RIDER-0.3-v0` is **ACTIVE / GREENLIT / OVERMAP LIGHT ATTRACTION CHECKPOINT GREEN / RIDER CONVERGENCE-BANDS NEXT**.

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

## Current checkpoint

Monster/evolution footing is green in `d50715f00e`:

- actual `mon_zombie_rider` exists with the exact description text;
- endpoint smell is hunter/predator branch after mature-world pressure, but normal upgrades are not wired into rider;
- direct routine spawn is gated to `GROUP_ZOMBIE` at `730 days`, singleton/costly;
- focused `[zombie_rider]` tests prove scary-fast-but-reviewable footing and no early spawn/evolution leak.

Passage seam footing is green in `zombie_rider_large_body_small_passage_pathing`:

- the spawned rider's actual large body rejects `SMALL_PASSAGE` / `t_window_empty`;
- normal-sized pathing can use that passable window-like seam;
- rider-sized pathing routes around it.

Local combat AI footing is green in `4343dbdad1`:

- live `monster::plan()` consumption covers line-of-fire bow targeting;
- tests cover bow shot cadence/cooldown, post-shot reposition destination, close-pressure retreat, injured withdrawal, and blocked-LOS no-shot counterplay.

Overmap light-attraction footing is green in `d2ffbd54c3`:

- `zombie_rider_overmap_ai` consumes existing `bandit_mark_generation::light_projection` / `horde_signal_power_from_light_projection`;
- tests cover mature-world exposed light interest, early-world suppression, no-rider/no-light/weak/daylight controls, temporary memory decay, and `max_riders_drawn_by_light = 2`;
- latest focused gate: `make -j4 tests/zombie_rider_test.o obj/zombie_rider_overmap_ai.o tests LINTJSON=0 ASTYLE=0 && ./tests/cata_test "[zombie_rider]"` -> 135 assertions / 10 test cases green.

Next slice may widen only to rider convergence/band formation before live funness rows. Do not reprove monster/evolution, passage, local combat, or overmap light attraction by ritual unless changed.

## Evidence/reporting requirements

For each later test/playtest row, report scenario/test name, run/artifact path, start conditions, decision trace, rider count, band state, shot/reposition/retreat counts, pass/fail/yellow verdict, caveats, warnings/log spam/crash status, and available timing/perf.

## Non-goals/cautions

- Do not make this a full release `0.3` omnibus.
- Do not make the rider a year-one routine spawn, decorative fast archer, instant camp-deletion tax, or infinite clown cavalry.
- Do not claim fun from JSON/stat presence alone; gameplay claims need tests and/or live-shaped scenarios.
