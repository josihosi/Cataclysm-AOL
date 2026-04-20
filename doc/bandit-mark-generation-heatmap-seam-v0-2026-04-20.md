# Bandit mark-generation + heatmap seam v0 (2026-04-20)

Status: active greenlit contract.

## Why this exists

The bounded bandit evaluator, deterministic playback suite, and first budget packet now exist and look cheap enough to continue.
What is still fake is the writer side: the current reference cases can consume leads, but they do not yet create or refresh those leads from a real bounded overmap-side mark model.

This slice exists to turn the parked mark/heatmap guidance into one small implementation seam instead of leaving the bandit lane dependent on hand-authored leads forever.

## Scope

1. Build a bounded overmap-side mark ledger and broad bounty/threat heat-pressure seam.
2. Accept explicit deterministic input packets or scenario-side events that create, refresh, merge, cool, or freeze marks without pretending full world simulation already exists.
3. Model the first selective decay / sticky-threat / confidence-refresh rules on that seam.
4. Bridge the generated mark output cleanly into the existing bandit evaluator / playback footing.
5. Produce a reviewer-readable summary surface for the generated mark/heat state instead of making this a debugger-only secret.

## Non-goals

- full visibility / concealment implementation
- full overmap scheduler or actor ecosystem
- overmap-to-bubble handoff logic
- broad multi-camp coordination behavior
- live-harness-first theater before deterministic footing is exhausted

## Success shape

This item is good enough when:
1. a bounded mark/heat seam exists for the first deterministic bandit inputs instead of only hand-authored leads
2. the packet can prove mark creation, refresh, selective cooling, and sticky confirmed threat on named deterministic cases
3. the current evaluator/playback seam can consume generated mark output reviewer-cleanly
4. the slice stays bounded and does not smuggle in handoff, visibility, or full hostile-world simulation work

## Validation expectations

- prefer deterministic bandit coverage first
- keep evidence reviewer-readable and inspectable
- reuse the current bandit evaluator / playback suite where possible instead of building a second disconnected test scaffold
- escalate to broader rebuild or live proof only if the touched seam honestly stops being answerable through deterministic packets

## Dependency note

This item is the first promoted implementation slice out of the parked mark/heatmap concept packet at `doc/bandit-mark-generation-and-heatmap-model-2026-04-19.md`.
It should build on the already-checkpointed evaluator, playback, and budget seams instead of reopening them.
