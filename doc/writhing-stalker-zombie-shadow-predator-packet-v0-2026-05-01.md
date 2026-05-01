# CAOL-WRITHING-STALKER-ZOMBIE-SHADOW-PREDATOR-v0

Status: GREENLIT / BACKLOG / PREDATOR VARIETY PACKAGE

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

- [ ] The stalker confidence model explicitly distinguishes evidence/interest, zombie pressure, quiet-side/cutoff opportunity, and counterpressure from light/focus/open exposure.
- [ ] Deterministic tests prove that with zombies on one side of the player, the stalker prefers the under-occupied/quiet side when shadow or cover permits.
- [ ] Deterministic tests prove zombie pressure increases stalker confidence only when local evidence or overmap-interest footing exists; no-evidence/no-magic cases stay targetless.
- [ ] Deterministic tests prove light/focus/open exposure suppresses quiet-side cutoff/strike behavior.
- [ ] Live/playtest rows prove at least one “fighting zombies, stalker appears on quiet side/back route” scenario and one “running from zombies, stalker blocks/cuts off escape side” scenario.
- [ ] Metrics include zombie-pressure side, chosen quiet-side/cutoff tile, confidence reasons, strike timing, counterplay outcome, turn-time cost, warnings/errors, and player fun/fairness notes.

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
