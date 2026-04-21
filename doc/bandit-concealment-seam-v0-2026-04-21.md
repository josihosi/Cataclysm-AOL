# Bandit concealment seam v0 (2026-04-21)

## Status

This is **ACTIVE / GREENLIT**.
It is the next narrow bandit promotion after the first honest 500-turn playback proof.

This is **not** permission to implement the whole visibility/concealment system.
For v0, the lane is explicitly **light concealment first**.
Weather belongs only as a reducer/modifier, not as the main feature.

---

## Summary

Land one bounded concealment packet that weakens already-landed visibility signals when outward exposure is poor.
The v0 packet should operate on the current light signal seam, not on every signal family at once.

---

## Scope

This lane should:
- reuse existing local light truth instead of inventing a second fake light simulator
- collapse outward visibility into coarse directional exposure buckets or an equivalent bounded side-leakage summary
- apply suppression from daylight, weather/visibility penalties, and containment/terrain
- allow one-sided leakage, meaning a site can be visible from one approach and concealed from another
- emit a bounded reduction on the current light-born mark seam rather than a general-purpose stealth doctrine

---

## Explicit non-goals

This lane should **not** do any of the following:
- full weather modeling
- a new fog-based sound law
- a broad all-signals concealment rewrite
- global smoke/world simulation
- bandit tactical stealth behavior
- pursuit/handoff expansion
- architecture/perf theater disguised as a tiny slice

---

## Success state

This lane is done for now when:
- one bounded concealment adapter exists on the current light signal seam
- deterministic coverage proves daylight suppression, weather penalty, containment, and side-dependent leakage/suppression honestly
- reviewer-readable output shows why a light-born mark was reduced, blocked, or allowed
- the slice stays cheap and inspectable, and if cost starts looking suspicious the packet carries one small readable cost/probe angle

---

## Testing shape

- docs-only canon patch -> no compile
- code change -> narrow deterministic bandit coverage first on the current mark-generation / playback seam
- broaden validation only if the implementation actually widens risk

Likely first proof shape:
- contained versus exposed night light
- daylight suppression
- one weather penalty case
- one side-dependent leakage/suppression case

---

## Product guardrail

This lane should make bandit visibility feel more honest, not more magical.
The point is to let exposure and concealment alter the existing light signal story without turning the overmap bandit code into a second tile-perfect light engine.
