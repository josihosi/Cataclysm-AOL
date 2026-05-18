# CAOL-ZOMBIE-RIDER-0.3-v0 — initial dev packet

## Classification

**ACTIVE / GREENLIT INITIAL DEV STARTED AFTER WRITHING-STALKER LIVE FUN CLOSURE**

Josef greenlit preparing the zombie rider for Andi, including playtests and a small map-AI funness benchmarking suite. The writhing-stalker live fun packet is now closed green, so this packet is the current active initial-dev target.

Imagination source: `doc/zombie-rider-0.3-imagination-source-of-truth-2026-04-30.md`.
Raw intake / exact flavor text: `doc/zombie-rider-raw-intake-2026-04-30.md`.
Map-AI/funness benchmark suite: `doc/zombie-rider-map-ai-funness-benchmark-suite-v0-2026-04-30.md`.

## Product target

Initial release 0.3 dev for the **zombie rider**: an endpoint late-game mounted ranged predator that is attracted by overmap light, can harass and flee, and can form rider bands when riders converge.

## Hard flavor-text rule

The monster description must preserve Josef’s exact flavor text and punctuation unless Josef explicitly rewrites it:

```text
Up on, what can only be described as, a six legged horse - or is it a spider? - a towering figure, with eyes the color of blood, holds a gory bow of wet bones and sinews.
It's movement ferocious, as the tumbling feet hasten across the terrain.
Running is out of the question.
```

Do not “fix” `It's`, commas, dashes, or rhythm. The weirdness is intentional.

## Implementation scope

### Monster footing

- Add a zombie rider monster entry with endpoint-tier placement.
- Preserve exact description text.
- Tune movement in the scary-fast range; Josef suggested `150-200`, but implementation should prove the final value does not erase counterplay.
- Make the rider physically large/huge enough to lean on existing monster-size passage rules before inventing any bespoke movement limiter.
- Add ranged shooting behavior using a gory bow / wet bone-and-sinew fiction.
- Ensure the rider is not simply a melee brute with inflated speed.

### Evolution / spawn gate

- Treat the rider as an endgame / end-of-line mutation.
- Prefer mature-world timing such as year two or equivalent evolution pressure over year-one routine spawn.
- Candidate lineage: zombie hunter or another advanced predator/hunter branch; choose the cleanest implementation seam and document it.
- Add deterministic tests that prove early-world/routine spawn gating does not leak riders too soon.

### Local combat AI

- First-line movement constraint should use existing large/huge monster size behavior: large/huge monsters cannot path/move through `SMALL_PASSAGE` terrain such as many windows/embrasures. Normal doors/corridors may still be passable, so prove the real blocked/passable set before adding any new mounted-movement system.
- Core loop: approach / shoot / reposition / flee from bad odds / return if advantage persists.
- Threat posture is higher than the writhing stalker: it may attack more readily and stalk less delicately, but it must not become omniscient or suicidal.
- Flee/shoot/flee must have readable break conditions: cover, indoors, line of sight break, bad terrain, injury, cooldown/ammo/range, or loss of advantage.

### Overmap / map AI

- Add overmap-side or map-AI footing for rider interest in late-game light.
- Bright camp light, elevated/roof light, vehicle/flood/base lighting, or significant fire/light should be able to attract rider interest at a bounded range.
- Light memory should decay after lights go out rather than creating permanent camp doom.
- Rider overmap behavior should support investigate/harass/circle/withdraw, not only direct charge.
- When two riders meet or converge, they should be able to form a rider band with caps/guardrails.

## Non-goals

- Do not implement a full release 0.3 omnibus. This packet is zombie rider initial dev only.
- Do not reopen writhing-stalker lanes, bandit/horde proof lanes, Smart Zone, or release packaging.
- Do not make camp light an instant unavoidable death sentence.
- Do not allow infinite rider accumulation or uncontrolled rider-band multiplication.
- Do not claim fun from JSON/stat presence alone.

## Success state

- Exact flavor text is preserved in raw intake, imagination source, and actual monster description.
- Monster JSON / definitions validate and focused tests cover endpoint spawn/evolution gating.
- Local combat tests cover speed/range, large/huge passage constraints, shoot/flee/reposition cadence, injury/pressure withdrawal, and counterplay through cover/line-of-sight/terrain.
- Overmap/map-AI tests cover light attraction, no-light negative control, light-memory decay, rider convergence, rider-band formation, and accumulation caps.
- Live or harness playtests cover open-field terror, cover/indoor escape, camp-light attraction, rider-band circling/harassment, and wounded-rider retreat or disengagement.
- Metrics include scenario/run ids, turn/time budgets, decision/reason traces, rider counts, band state, shot/reposition/retreat counts, warnings/log spam/crash state, and available per-turn/cadence costs.
- `Plan.md`, `SUCCESS.md`, `TESTING.md`, `doc/work-ledger.md`, and `andi.handoff.md` match final state when this packet becomes active or closes.
