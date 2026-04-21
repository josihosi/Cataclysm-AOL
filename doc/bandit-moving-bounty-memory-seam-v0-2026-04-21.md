# Bandit moving-bounty memory seam v0 (2026-04-21)

## Status

This is **GREENLIT / QUEUED NEXT**.
It is approved as the next follow-up behind the active scoring lane.

This is **not** permission to preempt the active scoring seam.
It is the next bounded memory packet once the active lane reaches its next real state.

---

## Summary

Land one tiny moving-bounty memory packet.
Structural bounty stays on site state.
Only moving/NPC bounty gets a small source-shaped pursuit memory, just enough for bounded stalk / shadow / intercept behavior.

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
- reviewer-readable output shows whether a moving lead was refreshed, narrowed, converted, or dropped
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
