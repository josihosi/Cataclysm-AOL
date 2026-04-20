# Bandit scenario fixture + playback suite v0 (2026-04-20)

Status: checkpointed contract for the first landed playback slice.

## Why this exists

One plausible evaluator tick is not enough.
After the dry-run evaluator seam landed, the next job was to make bandit behavior replayable across multiple ticks so viciousness, passivity, and drift could be inspected instead of guessed.

## Scope

1. Build named deterministic scenario fixtures for the first bandit reference cases.
2. Support playback checkpoints such as `tick 0`, `tick 5`, `tick 20`, and `tick 100` or another honestly justified long horizon.
3. Make the playback answer scenario questions like:
   - do they stay on `hold / chill`
   - do they investigate distant smoke
   - do they stalk city edges instead of charging deep in
   - do they peel off under growing zombie pressure
   - does interest stay attached to a moving carrier instead of upgrading the whole region magically
4. Prefer pure reasoning fixtures first, then add synthetic overmap scenarios only where the question really needs them.
5. Keep the scenario names stable so the same case can be rerun after each change.

## Non-goals

- broad mutation of normal world generation just because it is convenient
- live harness theater as the primary first answer
- pretending the whole bandit AI is done because playback exists
- folding perf/save-budget work into this item except for tiny support hooks that are unavoidable

## First scenario families

The initial suite should cover at least:
- empty region / nothing around
- smoke-only distant clue
- smoke plus searchlight corridor
- forest plus town
- single traveler in forest
- strong camp near weaker split-off route
- city edge with moving hordes

## Success shape

This item is good enough when:
1. the named scenarios rerun deterministically
2. playback checkpoints can show how behavior evolves over time instead of only one snapshot
3. the resulting packet makes viciousness/passivity drift inspectable on the chosen scenarios
4. the suite stays bounded enough that new scenarios are added for product questions, not for collection-hobby reasons

## Validation expectations

- deterministic fixture coverage first
- narrow compile/test for the touched seam
- use live harness/playtest only when the deterministic layers stop answering the real question honestly

## Dependency note

This is queued behind `doc/bandit-evaluator-dry-run-seam-v0-2026-04-20.md`.
Do not broaden the active evaluator slice into this backlog item just because the fixtures sound adjacent.
