# Bandit overmap/local pressure rewrite packet v0 (2026-04-21)

## Status

This is **CHECKPOINTED / DONE FOR NOW**.
It is the bounded pressure-rewrite packet that landed after the directional-light proof packet.

---

## Summary

One bounded pressure-rewrite proof packet now exists on the current bandit overmap/local seam.
It proves that when a stalking or interception posture meets a much hotter local reality, later overmap updates can rewrite that posture toward retreat, withdrawal, or re-evaluation instead of homing forever on stale intent.

---

## Current honest state

This lane now provides:
- the named `generated_local_loss_rewrites_corridor_to_withdrawal` and `generated_local_loss_reroutes_to_safer_detour` multi-turn scenarios on the current playback seam
- deterministic proof that later overmap-side updates can rewrite the same outing away from stale pursuit into retreat, withdrawal, or another bounded safer posture after local loss makes the old read too dangerous
- reviewer-readable packet output through `run_overmap_local_pressure_rewrite_proof_packet()` plus `render_overmap_local_pressure_rewrite_proof_packet( const proof_packet_result &result )`
- explicit scenario-specific benchmark hooks that stay visible on the same report path without pretending the final tuned benchmark table was already frozen
- bounded scope on the current abstract playback footing, with no broad handoff redesign, no tactical local combat AI pass, and no fresh world-sim jump

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
- each scenario carries explicit goals plus scenario-specific benchmark hooks, and the later locked benchmark outcomes can be read from the same report path without rewriting the packet from scratch
- the slice stays bounded: no broad handoff redesign, no tactical bandit-vs-player combat AI expansion, and no fresh world-sim jump

That success state is now met on the current tree.

---

## Testing shape

- docs-only canon patch -> no compile
- code change later -> rebuild the relevant bandit targets and run the current bandit suite
- the immediate step is the real multi-turn scenario family plus reviewer-readable packet output; the exact scenario-specific benchmark table gets locked afterward, not faked up front
- the packet should still ultimately land as real multi-turn overmap-side proof, not single-turn correctness theater

Expected proof shape later:
- `make -j4 tests`
- `./tests/cata_test "[bandit][playback]"`
- `./tests/cata_test "[bandit]"`

---

## Product guardrail

Bandits should be stubborn when it is useful, not braindead.
If the local tile turns obviously hotter than the old overmap read, the system should be able to admit that and peel away instead of pretending stale intent is destiny.
