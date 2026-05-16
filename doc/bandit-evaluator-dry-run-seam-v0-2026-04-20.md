# Bandit evaluator dry-run seam v0 (2026-04-20)

Status: active auxiliary for the current `Plan.md` bandit lane.

## Why this exists

The parked bandit concept chain is finally coherent enough to promote one narrow implementation slice.
Do **not** greenlight the whole bandit system from that fact.
Greenlight only the first inspectable evaluator seam.

The point of this slice is to build the bandit camp decision engine as a **dry-run, explainable evaluator** before full autonomous behavior, broad save architecture, or live scenario theater starts sprawling.

## Current top goal

Land one deterministic evaluator seam that can answer, on controlled inputs:
- what leads existed
- what candidates were generated
- how they scored
- why veto or soft-veto happened
- why the winning job beat `hold / chill`

## Inputs and authority

Product truth still comes from the current bandit concept docs, especially:
- `doc/bandit-concept-formalization-followthrough-2026-04-19.md`
- `doc/bandit-overmap-ai-concept-2026-04-19.md`
- `doc/bandit-bounty-threat-scoring-guidance-2026-04-19.md`
- `doc/bandit-mark-generation-and-heatmap-model-2026-04-19.md`
- `doc/bandit-overmap-to-bubble-handoff-seam-2026-04-19.md`
- `doc/bandit-player-basecamp-visibility-and-concealment-2026-04-19.md`

This active lane is the first promoted implementation slice from that parked concept substrate.
The broader bandit concept chain still stays parked outside the explicitly greenlit slices.

## Scope

1. Build a dry-run bandit evaluator that consumes controlled abstract state and produces a candidate board.
2. Keep `hold / chill` as the always-available baseline candidate.
3. Generate outward candidates only from real compatible leads or current hard state.
4. Expose an explanation surface for:
   - leads considered
   - generated candidates
   - per-candidate score inputs and final score
   - veto / soft-veto reasons
   - winner versus `hold / chill`
5. Keep any return-state or persistence seam **bounded and skeletal** in v0, only as much as needed to keep the evaluator contract honest.
6. Add narrow deterministic tests using pure reasoning fixtures for the first reference cases.

## Non-goals

- full autonomous bandit world behavior
- full scenario fixture/playback suite
- broad harness or live-probe theater
- final persistence architecture or large save-schema commitment
- extortion / negotiation / diplomacy layers
- coalition strategy or multi-camp coordination logic
- full tactical bubble AI

## Minimum debug contract

The first evaluator seam should expose enough information that review does not turn into ghost hunting.

Minimum expected output shape:
- current leads and why they are still valid
- full candidate board including `hold / chill`
- score terms for each candidate
- veto / soft-veto outcome and reason
- final winner and why it beat the alternatives
- any bounded return-packet fields touched in this seam

The exact logging/surface can stay implementation-friendly, but those facts must be observable.

## First reference cases

The first pure reasoning fixtures should cover at least these cases:
- no real lead -> `hold / chill`
- unreachable attractive lead -> fail closed instead of fake wander
- weak but real need-aligned lead can beat `hold / chill` only when the documented need-pressure rule allows it
- serious threat can soft-veto opportunistic extraction while still allowing bounded pressure/recon behavior
- strong honest local lead can beat `hold / chill` without inventing extra behavior outside the winning job

## Success shape

This slice is good enough when:
1. a deterministic dry-run evaluator exists on controlled inputs
2. the evaluator exposes the minimum debug contract above
3. narrow deterministic tests exist for the first reference cases
4. the slice stays visibly bounded and does not smuggle in the next two greenlit items

## Validation expectations

Use the smallest honest evidence for the touched code:
- docs-only canon updates -> no compile
- first code slice -> narrow compile/test only
- pure reasoning fixtures first
- do **not** jump to harness/live play unless the first seam cannot be tested honestly without it

## Queued follow-ons, not part of this active slice

After this seam lands cleanly, the next intended order is:
1. `doc/bandit-scenario-fixture-playback-suite-v0-2026-04-20.md`
2. `doc/bandit-perf-persistence-budget-probe-v0-2026-04-20.md`
