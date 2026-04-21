# Bandit weather concealment refinement packet v0 (2026-04-21)

## Status

This is **GREENLIT / QUEUED NEXT**.
It is approved behind the current overmap/local pressure rewrite packet.

This is a bounded refinement follow-up on the already-landed smoke, light, and concealment seams.
It is not permission to reopen the whole visibility stack just because weather is nearby and dramatic.

---

## Summary

Land one bounded weather-refinement packet on the current smoke/light mark-generation footing.
The goal is to make weather concealment feel less fake by strengthening wind-driven smoke fuzziness and adding explicit portal-storm handling, while keeping rain and fog as honest visual reducers instead of vague vibes.

---

## Scope

This lane should:
- refine the current smoke adapter so **wind** does more than a tiny range penalty, pushing smoke toward lower confidence, fuzzier source precision, and more displaced or corridor-shaped clue output where that reads more honestly
- add explicit **portal-storm** weather footing, or one equivalent bounded weather band, instead of silently pretending portal storms are just ordinary clear/rain/fog cases
- treat portal-storm smoke as harder to localize and less trustworthy, rather than merely shrinking range a bit
- let darkness during portal-storm conditions still support exposed bright light or searchlight-style light staying legible when that is the honest read, without turning every ordinary lantern into a magical lighthouse
- keep **rain** explicit for both smoke and light instead of burying it as hand-wavy cloudiness
- expose the weather/fuzziness interpretation reviewer-readably so later proofs can explain whether a clue was narrowed, displaced, fuzzed, reduced, or blocked

---

## Explicit non-goals

This lane should **not** do any of the following:
- reopen the already-closed broad concealment seam as a second giant visibility rewrite
- invent a full wind-vector plume simulator or global offscreen smoke-field physics model
- invent a new fog-based sound law
- solve z-level visibility in the same packet
- widen into a broader weather-system redesign or portal-storm architecture opera
- preempt the currently active pressure-rewrite packet

---

## Success state

This lane is done for now when:
- one bounded weather-refinement packet exists on the current smoke/light mark-generation footing
- deterministic coverage proves wind meaningfully fuzzes or de-precises smoke output instead of acting only as a token penalty
- deterministic coverage proves portal-storm weather is handled explicitly for smoke and light instead of falling through as an unmodeled special case
- reviewer-readable output explains how weather changed clue quality, for example reduced range, fuzzier origin, displaced/corridor-ish smoke read, or preserved bright-light legibility under dark storm conditions
- the slice stays bounded: no full plume physics, no global smoke sim, no sound-law rewrite, no z-level packet, and no broad visibility architecture rework

---

## Testing shape

- docs-only canon patch -> no compile
- code change later -> rebuild the relevant bandit targets and run the narrowest mark-generation plus playback coverage that honestly matches the change
- prefer deterministic packet tests over live weather theater unless deterministic proof stops being enough

Expected proof shape later:
- `make -j4 tests`
- `./tests/cata_test "[bandit][marks]"`
- `./tests/cata_test "[bandit][playback]"`
- `./tests/cata_test "[bandit]"`

Likely named proof cases:
- weak versus strong smoke under wind, showing fuzzier or less source-precise projection instead of only a tiny range haircut
- portal-storm smoke showing worse source certainty than ordinary night conditions
- portal-storm exposed bright light staying legible when darkness honestly supports it, while ordinary sheltered light still stays bounded
- rain remaining an explicit reducer for both smoke and light

---

## Product guardrail

Weather should make the bandits feel less psychic, not less coherent.
Wind should not just politely subtract one number from smoke and go home.
Portal storms should look weird and hard to read, but not so weird that the AI becomes a superstitious idiot or a clairvoyant one.
