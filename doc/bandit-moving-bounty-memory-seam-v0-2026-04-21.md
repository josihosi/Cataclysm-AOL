# Bandit moving-bounty memory seam v0 (2026-04-21)

## Status

This is **CHECKPOINTED / DONE FOR NOW**.
The bounded moving-memory packet now exists on the current bandit seam and is honest enough to stay closed for now.

This is still **not** permission to widen into a broad memory architecture pass.
It stays the narrow moving-memory packet until later canon says otherwise.

---

## Summary

One tiny moving-bounty memory packet is now landed.
Structural bounty stays on site state.
Only moving/NPC bounty gets a small source-shaped pursuit memory, just enough for bounded stalk / shadow / intercept behavior.

Current honest state:
- `src/bandit_mark_generation.{h,cpp}` now keeps a bounded `moving_bounty_memory` packet on moving-carrier and corridor marks only, with leash, opportunity/threat bands, and reviewer-readable transition state
- structural/site marks stay on cheap site footing and do not gain chase memory
- deterministic coverage in `tests/bandit_mark_generation_test.cpp` now proves three key bounded distinctions honestly: moving prey can be stalked briefly after the raw mark cools, structural sites do not gain chase memory, and stale moving contact drops reviewer-cleanly instead of retrying forever
- `tests/bandit_playback_test.cpp` now keeps the corridor playback expectation aligned with the new bounded memory window instead of pretending the mark vanishes immediately
- narrow deterministic validation passed via `make -j4 tests` and `./tests/cata_test "[bandit]"`

---

## Scope

This lane should:
- keep structural bounty on existing site state such as harvested / recently-checked / false-lead / sticky threat instead of adding chase memory there
- add one small moving-bounty memory object for live `actor` or `corridor` style bounty
- let camps briefly stalk, shadow, or intercept live prey while the lead still looks honest
- bound the moving memory with confidence plus leash so it collapses cleanly when the prey is lost, splits, disproves itself, becomes too dangerous, or the outing expires
- stay reviewer-readable and easy to inspect on the current bandit seam

---

## Explicit non-goals

This lane should **not** do any of the following:
- build a general memory architecture for everything on the map
- add per-turn tracking
- store path-history scrapbooks
- create per-NPC biography graphs or personal vendetta state
- let bandits retry forever until prey eventually splits off
- widen into fresh world simulation or route-planning theater

---

## Success state

This lane is done for now when:
- one bounded moving-bounty memory object exists for live `actor` or `corridor` style bounty while structural bounty stays on site state
- structural sites keep only cheap state such as harvested / recently-checked / false-lead / sticky-threat footing and do not gain stalking logic
- deterministic coverage proves the key bounded pursuit split honestly: live moving prey can be stalked briefly, but the memory collapses cleanly on lost contact, split, bad recheck, rising threat, or leash expiry
- reviewer-readable output shows whether a moving lead was refreshed, narrowed, or dropped
- the slice stays computationally cheap and does not smuggle in a bigger memory system

---

## Testing shape

- docs-only canon patch -> no compile
- code change later -> narrow deterministic bandit coverage first on the current mark/evaluator/playback seam
- broaden validation only if the implementation actually widens risk

Likely first proof shape:
- one harvested site does not re-grow chase behavior just because structural bounty still exists in the world
- one moving prey lead supports bounded stalking or shadowing for a short window
- one lost/split/too-dangerous lead gets dropped or downgraded instead of chased forever

---

## Cost guardrail

This lane should stay tiny.
Good v0 shape is a few very small moving-lead fields such as lead shape, last known location, confidence band, age steps, leash, opportunity band, and threat band.

If the implementation starts asking for path histories, many concurrent pursuit branches, per-NPC biographies, or continuous tracking updates, that is the moment to say no.
