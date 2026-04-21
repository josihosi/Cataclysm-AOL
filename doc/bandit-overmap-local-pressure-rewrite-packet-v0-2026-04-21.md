# Bandit overmap/local pressure rewrite packet v0 (2026-04-21)

## Status

This is **GREENLIT / QUEUED NEXT**.
It is approved as the next bounded follow-up behind the active directional-light proof packet.

---

## Summary

Land one bounded pressure-rewrite proof packet on the current bandit overmap/local seam.
The goal is to prove that when a stalking or interception posture meets a much hotter local reality, later overmap updates can rewrite that posture toward retreat, withdrawal, or re-evaluation instead of homing forever on stale intent.

---

## Scope

This lane should:
- add one small named multi-turn scenario packet where a previously plausible stalking or intercept approach becomes locally too dangerous
- prove that later overmap-side updates can rewrite the same outing away from stale pursuit into retreat, withdrawal, or another bounded safer posture
- keep the proof reviewer-readable with explicit before/after posture output and clear benchmark checks
- carry explicit per-scenario goals and tuning metrics instead of vague "seems smarter" prose
- stay on the current abstract playback / handoff footing without reopening broader world-sim architecture

---

## Explicit non-goals

This lane should **not** do any of the following:
- become a broad handoff-architecture redesign
- widen into tactical local combat AI
- reopen the already-closed pursuit handoff seam just because it is nearby
- solve directional light or z-level visibility in the same packet
- replace multi-turn benchmark proof with single-turn unit math and declare victory

---

## Success state

This lane is done for now when:
- one bounded overmap/local pressure-rewrite proof packet exists on the current bandit scenario / playback footing
- deterministic multi-turn proof shows a stalking or intercept approach can honestly cool, retreat, or reroute after local reality makes the original posture too dangerous
- reviewer-readable output shows the pressure rewrite clearly enough to explain why the stale pursuit no longer holds
- each scenario carries explicit goals and tuning metrics, with benchmark outcomes visible in the report
- the slice stays bounded: no broad handoff redesign, no tactical bandit-vs-player combat AI expansion, and no fresh world-sim jump

---

## Testing shape

- docs-only canon patch -> no compile
- code change later -> rebuild the relevant bandit targets and run the current bandit suite
- this queued packet, like the active one, should ultimately land as real multi-turn overmap-side proof with explicit benchmarks, not single-turn correctness theater

Expected proof shape later:
- `make -j4 tests`
- `./tests/cata_test "[bandit][playback]"`
- `./tests/cata_test "[bandit][handoff]"`
- `./tests/cata_test "[bandit]"`

---

## Product guardrail

Bandits should be stubborn when it is useful, not braindead.
If the local tile turns obviously hotter than the old overmap read, the system should be able to admit that and peel away instead of pretending stale intent is destiny.
