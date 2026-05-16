# CAOL-WRITHING-STALKER-ZOMBIE-SHADOW-PREDATOR-v0

Status: CLOSED / CHECKPOINTED GREEN V0 / PREDATOR VARIETY PACKAGE

Imagination source: `doc/predator-behavior-variety-imagination-source-of-truth-2026-05-01.md`.

## Summary

Shift the writhing stalker away from hit-and-dash and toward a zombie-shadow attention predator. It should stalk from overmap light interest and reality-bubble evidence, grow more confident when ordinary zombies are pressuring the player, prefer the quiet side where the zombies are not, and strike when the player is distracted, running, boxed, reloading, bleeding, or fighting. It remains counterable by light, focus, open exposure, deliberate space-clearing, and broken evidence.

## Scope

- Rework stalker opportunity/confidence scoring so nearby ordinary zombie pressure raises confidence only when honest evidence/interest exists.
- Define the “quiet side” around the player from local enemy distribution: prefer arcs/candidate tiles opposite the dominant zombie pressure, not literal facing mechanics.
- Prefer shadow/cover/broken-LoS/cutoff tiles near likely retreat routes or under-occupied fight arcs.
- Keep no-omniscience, evidence gating, light/focus counterplay, cooldown, and injured retreat constraints.
- Distinguish stalker use of circling from raptor circling: stalker circles through shadow/cutoff logic, not visible open-space skirmishing.
- Add deterministic little-map tests and live/playtest rows that prove zombie-distraction behavior changes the encounter shape.

## Non-goals

- Do not make the stalker another flesh raptor with different text.
- Do not invent literal player-facing/backstab mechanics unless an existing honest engine seam exists.
- Do not make zombie proximity a magical buff; zombies create attention cover, not omniscience.
- Do not reopen old v0 proof claims except where this package explicitly changes the product behavior.
- Do not remove all strike/withdraw behavior; strikes can still happen, but they are punctuation after stalking/opportunity.

## Success state

- [x] The stalker confidence model explicitly distinguishes evidence/interest, zombie pressure, quiet-side/cutoff opportunity, and counterpressure from light/focus/open exposure.
- [x] Deterministic tests prove that with zombies on one side of the player, the stalker prefers the under-occupied/quiet side when shadow or cover permits.
- [x] Deterministic tests prove zombie pressure increases stalker confidence only when local evidence or overmap-interest footing exists; no-evidence/no-magic cases stay targetless.
- [x] Deterministic tests prove light/focus/open exposure suppresses quiet-side cutoff/strike behavior.
- [x] Live/playtest rows prove at least one “fighting zombies, stalker appears on quiet side/back route” scenario and one “running from zombies, stalker blocks/cuts off escape side” scenario, scoped to local-evidence-only live footing.
- [x] Metrics include zombie-pressure side, chosen quiet-side/cutoff tile, confidence reasons, strike timing, counterplay outcome, turn-time cost, warnings/errors, and player fun/fairness notes, with the live overmap-interest caveat preserved.

## Current proof checkpoint

- Deterministic footing: focused `[writhing_stalker]` tests are green for confidence components, quiet-side/cutoff choice, split-pressure ambiguity, pressure gating, overmap-interest helper admission, and light/focus/open-exposure suppression.
- First staged-but-live row: `writhing_stalker.live_quiet_side_zombie_pressure_mcw` -> `.userdata/dev-harness/harness_runs/20260501_071548/`; proof note `doc/writhing-stalker-zombie-shadow-live-quiet-side-proof-v0-2026-05-01.md`.
- Second staged-but-live row: `writhing_stalker.live_escape_side_zombie_retreat_mcw` -> `.userdata/dev-harness/harness_runs/20260501_071940/`; proof note `doc/writhing-stalker-zombie-shadow-live-escape-side-proof-v0-2026-05-01.md`.
- Scope caveat: both live rows are local-evidence-only and explicitly log `overmap_interest=no`; they do not claim live overmap-interest footing.
- Decisive bridges: live `zombie_pressure=3` / `pressure_x=3` maps to `quiet_x=-1` and first matched `chosen_rel_x=-1`, `chosen_rel_y=-4`; live `zombie_pressure=3` / `pressure_y=-3` maps to `quiet_y=1` and first matched `chosen_rel_y=4`. Both bridges come from the live shadow-destination path, not from confidence-total logs alone.
- Closure caveat: v0 proves local-evidence zombie-shadow behavior. Live overmap-interest wiring/logging remains unclaimed unless a later packet promotes it.

## Targeted tests

- Unit/helper tests for local pressure arcs: dominant zombie side, under-occupied side, candidate scoring, and cutoff preference.
- Deterministic small-map tests:
  - zombies east/front, stalker chooses west/quiet-side shadow tile;
  - two zombie clusters reduce quiet-side certainty and avoid fake precision;
  - no evidence plus nearby zombies still does not create target magic;
  - light/focus suppresses cutoff and strike;
  - recent player movement/retreat route biases cutoff when consistent with evidence.
- Live/playtest scenarios:
  - street fight with ordinary zombies in front and stalker arriving from quiet side;
  - retreat from zombie pressure where stalker occupies the escape-side/cutoff tile;
  - brightly lit/focused negative-control where the stalker fails or withdraws;
  - mixed hostile performance row with ordinary zombies plus one stalker.

## Fun metrics

- Confidence components per sampled decision: evidence, overmap/light interest, zombie pressure, quiet-side/cutoff opportunity, counterpressure.
- Time spent stalking/shadowing before strike.
- Quiet-side candidate chosen vs zombie-pressure side.
- Strike count, damage, cooldown rhythm, and repeated-strike spacing.
- Player counterplay: lighting, facing/focus proxy, clearing zombies, retreating into controlled space, closing doors/corners.
- Jitter count / target-tile churn.
- Turn-time cost and debug/log noise.

## Relationship to anti-redundancy package

This is a product behavior change. The separate `CAOL-WRITHING-STALKER-BEHAVIOR-SEAM-REDUCTION-v0` package should preserve whichever stalker behavior is current and approved when it executes. If this package runs first, the anti-redundancy refactor must preserve the zombie-shadow predator shape, not the older hit-and-dash-adjacent behavior.

## Cautions

The good fantasy is “zombies make noise in front; stalker becomes the silence behind.” If the implementation turns that into teleporting, omniscient backstabs, or generic flee-after-hit behavior, it missed the point.
