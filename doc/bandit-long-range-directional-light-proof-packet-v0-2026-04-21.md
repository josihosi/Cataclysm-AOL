# Bandit long-range directional light proof packet v0 (2026-04-21)

## Status

This is **CHECKPOINTED / DONE FOR NOW**.
It was the active narrow bandit lane and is now honestly landed on the current tree.

This packet is not just another single-turn unit nibble.
Josef explicitly wants real overmap-side multi-turn proof with benchmarks, not only one-tick correctness theater.

---

## Summary

Land one bounded long-range directional-light proof packet on the current bandit playback / generated-mark footing.
The goal is to prove that directional night-light leakage stays actionable only from the honest side, stays inert from the hidden side, and still shares corridor consequences with zombie pressure instead of turning into isolated bandit-only theater.

---

## Scope

This lane should:
- add one small named multi-turn scenario packet for a north-of-bandit camp whose light leaks only east, then a matching south-visible variant
- prove that east-only leakage does **not** justify north-side staking-out or pursuit from the bandit side
- prove that south-visible leakage **does** become actionable from the bandit side under the same broader footing
- include the matching deterministic zombie-horde corridor variant so the same light does not become private bandit information on the proof seam; do not claim live horde attraction without a live bridge
- carry explicit per-scenario goals and tuning metrics instead of leaving the answer as vague vibes
- run on the current overmap-side playback / proof seam for up to **500 turns**, with reviewer-readable checkpoints and benchmark outcomes

---

## Explicit non-goals

This lane should **not** do any of the following:
- widen into a broad light-system rewrite
- reopen the already-closed light visibility mark seam v0
- solve z-level visibility edge cases in the same packet
- turn into a full handoff rewrite or local combat AI pass
- replace benchmarked multi-turn proof with single-turn unit tests and call that good enough

---

## Success state

This lane is done for now when:
- one bounded long-range directional-light proof packet exists on the current bandit scenario / playback seam
- deterministic multi-turn proof up to **500 turns** shows the hidden-side leakage case stays non-actionable while the visible-side case becomes actionable under the same surrounding conditions
- the matching deterministic zombie-horde corridor variant proves the same light can carry abstract horde pressure too instead of existing in bandit-only theater; live horde attraction remains a separate bridge
- each scenario carries explicit goals and tuning metrics, and the reviewer-readable output shows whether those benchmarks were met
- the slice stays bounded: no z-level expansion, no fresh concealment architecture, no handoff rewrite, and no broad world-sim jump

---

## Testing shape

- docs-only canon patch -> no compile
- code change later -> rebuild the relevant bandit targets and run the current bandit suite
- this packet specifically requires at least one real **multi-turn** scenario family on the overmap-side proof seam, not only single-turn unit checks

Expected first evidence:
- `make -j4 tests`
- `./tests/cata_test "[bandit][playback]"`
- `./tests/cata_test "[bandit]"`

Required proof style:
- scenarios should run long enough to show approach drift honestly, up to **500 turns** where needed
- each scenario should name benchmark expectations plainly, for example no north-side stalking on hidden leakage, visible-side interest on south leakage, and shared horde attraction in the corridor variant

---

## Product guardrail

The point is to prove the bandits are not psychic and not blind.
They should only react to the side that honestly leaks.
And when the same light would also matter to zombies, the world should admit that instead of staging a private little bandit opera.
