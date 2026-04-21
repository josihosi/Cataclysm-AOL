# Bandit scoring refinement seam v0 (2026-04-21)

## Status

This is **GREENLIT / QUEUED NEXT**.
It is approved as the next follow-up behind the active concealment lane.

This is **not** permission to preempt the active concealment seam.
It is the next bounded scoring packet once the active lane reaches its next real state.

---

## Summary

Land one bounded scoring-refinement packet on the current bandit dry-run/evaluator seam.
The goal is to make existing marks and camp state turn into sharper opportunistic job choice without inventing a brand new threat cosmology.

---

## Scope

This lane should:
- refine how existing camp ledger state plus existing marks become job choice on the current evaluator footing
- inspect existing threat/danger mechanics first and collapse whatever usable footing already exists into the bandit scoring layer
- preserve the product read that bandits avoid strong opponents but opportunistically pounce when zombie pressure or other distraction lowers effective target coherence
- keep the scoring logic deterministic and reviewer-readable
- stay on the current dry-run/evaluator seam rather than widening into a fresh architecture layer

---

## Explicit non-goals

This lane should **not** do any of the following:
- invent a fresh bespoke threat ontology when existing threat/danger footing can be collapsed instead
- add new visibility signals or new signal-ingestion machinery
- rewrite broad heatmap/memory damping policy
- simulate tactical bandit-versus-zombie combat
- add coalition strategy, diplomacy, or multi-camp command behavior
- widen into fresh world simulation just because the scoring question is interesting

---

## Success state

This lane is done for now when:
- one bounded scoring-refinement adapter exists on the current bandit dry-run/evaluator seam
- the threat side first inspects and collapses usable existing threat/danger footing instead of inventing a fresh bespoke threat ontology
- deterministic coverage proves the key opportunism split honestly: bandits avoid strong opponents, but pounce when zombie pressure or other distraction lowers effective target coherence
- reviewer-readable output shows why a target was avoided, deferred, or exploited
- the slice stays bounded and does not smuggle in new visibility, memory, coalition, or world-sim layers

---

## Testing shape

- docs-only canon patch -> no compile
- code change later -> narrow deterministic bandit coverage first on the current evaluator / playback seam
- broaden validation only if the implementation actually widens risk

Likely first proof shape:
- one clearly too-strong target gets rejected or deferred
- one zombie-pressured or otherwise distracted target becomes materially more attractive
- one neutral case shows the scorer did not become deranged or omniscient

---

## Product guardrail

This lane should make bandits feel opportunistic and worldly, not psychic.
The scoring refinement should mostly compress already-real danger footing into usable choice, not hallucinate a brand new danger model because that sounds elegant in a document.
