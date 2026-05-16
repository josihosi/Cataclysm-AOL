# Bandit bounded scout/explore seam v0 (2026-04-21)

## Status

This is **CHECKPOINTED / DONE FOR NOW**.
It was the bounded bandit slice for this pass.

This is **not** permission to reopen random wandering, broad pathfinding AI, coalition behavior, or handoff theater.
It is one explicit map-uncovering packet on the current evaluator/playback footing.

---

## Summary

Land one bounded scout/explore packet for map uncovering.

The product rule is narrow:
- unreachable jobs still fail closed for that dispatch pass
- bandits do **not** get accidental random wandering as a consolation prize
- if a camp is explicitly allowed to uncover more map, it may dispatch one short bounded scout/explore outing that competes honestly against `hold / chill` and any real reachable lead

---

## Scope

This lane should:
- add one explicit bounded scout/explore option on the current dry-run evaluator seam
- keep that option separate from ordinary lead generation and separate from `no_path` fallback
- let the explicit scout/explore outing beat `hold / chill` when no real outward job wins honestly
- keep strong real reachable leads ahead of exploratory wandering when they score better
- carry reviewer-readable wording that this is explicit map uncovering, not fake random behavior
- add one small playback/reference proof packet later with explicit scenario goals and tuning metrics

---

## Explicit non-goals

This lane should **not**:
- turn `no_path` into random wandering
- add a new visibility signal family
- add coalition behavior or camp-to-camp alliance logic
- reinterpret other camps as allies by default
- widen into handoff, local tactical AI, or broad world-sim work
- solve general exploration strategy beyond one bounded scout/explore packet

---

## Current honest state

So far:
- `src/bandit_dry_run.{h,cpp}` now carries one explicit camp-side bounded scout/explore option on the current evaluator seam via `camp_input`, instead of minting fake wandering from failed routes
- unreachable leads still fail closed as `no_path` and do not auto-create explore output
- deterministic coverage in `tests/bandit_dry_run_test.cpp` now proves three key bounded distinctions honestly: explicit explore can beat `hold / chill`, blocked routes do not mint explore without the explicit greenlight, and strong real reachable leads still outrank exploratory wandering
- reviewer-readable dry-run output now states that the scout/explore packet is explicit map uncovering and not accidental random wandering

Closure evidence that landed on this pass:
- `src/bandit_playback.cpp` now carries the named `bounded_explore_frontier_tripwire` scenario packet with explicit goals and tuning metrics covering the deliberate-explore, blocked-route, and strong-real-lead splits on the current scenario seam
- `tests/bandit_playback_test.cpp` now proves that playback packet stays explicit, stays secondary to stronger reachable work, and does not mint explore from a blocked route without greenlight

---

## Success state

This lane is done for now when:
- one explicit bounded scout/explore option exists on the current dry-run evaluator seam
- unreachable jobs still fail closed and do not auto-mint explore
- explicit explore can beat `hold / chill` when no real outward job wins, but stays secondary to stronger real reachable leads
- reviewer-readable output says plainly that the outing is explicit map uncovering, not accidental random wandering
- [landed] one small playback/reference scenario packet proves the behavior with explicit goals and tuning metrics
- the slice stays bounded: no coalition logic, no fresh visibility family, no broad pathfinding rewrite, and no handoff expansion

---

## Testing shape

- code change now -> narrow deterministic dry-run coverage first
- once the evaluator packet is stable -> add one small playback/reference scenario proof with explicit goals and tuning metrics
- broaden validation only if the implementation widens beyond the current evaluator/playback seam

---

## Product guardrails

Keep the intent plain:
- exploration is a deliberate bounded outing, not an engine accident
- camps stay independent with their own maps and bounty reads
- other camps still read more like threat-bearing spots than default allies
- scenario packets should be reviewer-readable enough to tune for behavior that feels sharp and grounded instead of psychic or deranged
