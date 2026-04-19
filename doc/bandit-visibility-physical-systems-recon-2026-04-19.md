# Bandit visibility physical-systems recon (2026-04-19)

## Status

This is a **supporting recon note** for the parked bandit concept chain.
It supports the paired visibility item at:
- `doc/bandit-player-basecamp-visibility-and-concealment-2026-04-19.md`

It is not itself an implementation greenlight.

Its job is to answer a narrower question first:

> what existing physical systems already exist in the repo for smoke, light, sound, weather, and visibility, and what glue would bandit overmap logic still need?

---

## Why this note exists

Before freezing the player/basecamp visibility and concealment concept, it is worth checking what the code already does so the design stays grounded in real systems rather than decorative fiction.

The recon result is encouraging:
- the repo already has real weather-modified sound behavior
- the repo already has real weather-modified light and sight behavior
- the repo already has real wind-aware smoke behavior
- the repo already has per-overmap-point weather lookup

But it does **not** yet appear to have a ready-made bridge from those physical systems into bandit bounty/threat marks.

That adapter seam is the real missing layer.

---

## Files inspected

Primary repo footing looked at during this recon:
- `src/sounds.cpp`
- `src/map_field.cpp`
- `src/lightmap.cpp`
- `src/lightmap.h`
- `src/weather.cpp`
- `src/character.cpp`
- `src/item.cpp`
- `src/overmap_ui.cpp`
- `data/json/weather_type.json`
- `data/json/field_type.json`

---

## Recon summary

### 1. Sound already has a real overmap signal path

In `src/sounds.cpp`, `sounds::process_sounds()` clusters local sounds, applies weather-based attenuation, and then feeds a signal into the overmap horde system.

Important details:
- `get_signal_for_hordes()` subtracts `get_weather().weather_id->sound_attn`
- sufficiently large sounds call `overmap_buffer.signal_hordes( target, sig_power )`

Meaning:
- there is already a proven engine pattern for local physical events becoming overmap-readable signals
- weather attenuation is already part of that pipeline

This is the cleanest existing precedent for future bandit investigation/pursuit or sound-driven interest logic.

---

### 2. Weather already exposes the right visibility knobs

`data/json/weather_type.json` already defines per-weather parameters such as:
- `sight_penalty`
- `light_modifier`
- `light_multiplier`
- `sound_attn`

And `src/weather.cpp` already uses these to alter incident sunlight, moonlight, and wind/sound consequences.

Meaning:
- weather state is not a decorative label
- the engine already treats weather as a first-class visibility/hearing modifier

This is good news because the future bandit visibility model can lean on real weather categories instead of inventing a parallel weather law.

---

### 3. Per-overmap-point weather lookup already exists

In `src/overmap_ui.cpp`, `get_weather_at_point( const tripoint_abs_omt &pos )` caches and returns weather for a specific overmap location.

Meaning:
- future cadence-based bandit logic can plausibly ask what weather is happening at a target overmap tile or region
- visibility scoring does not have to assume one global flat weather truth

That matters for any later model where fog, mist, rain, or storms change how legible signals are.

---

### 4. Smoke/fire behavior is already physical and wind-aware

In `src/map_field.cpp`:
- fires can create `fd_smoke`
- smoke creation depends partly on `windpower`
- gas/smoke spread uses `get_local_windpower(...)`
- local wind is modified by shelter, forest overmap terrain, z-level, and nearby wind blockers

`data/json/field_type.json` also confirms that `fd_smoke` is a real field type with intensity levels and gameplay effects.

Meaning:
- smoke already exists as a physical system with wind-aware behavior
- fires already produce smoke without inventing a new bandit-only mechanic

What is missing is not smoke itself.
What is missing is the overmap-facing interpretation layer.

---

### 5. Light and sight already respond to weather and conditions

In `src/weather.cpp`:
- weather modifies incident sun/moon light through `light_multiplier` and `light_modifier`

In `src/character.cpp`:
- `Character::sight_range()` derives visible range from light level using Beer-Lambert-style falloff against `LIGHT_TRANSPARENCY_OPEN_AIR`
- `Character::overmap_sight_range()` and `Character::overmap_los()` then convert that into coarse overmap sight with terrain `get_see_cost()`

In `src/lightmap.h`:
- open-air transparency is explicitly tuned to run out near `MAX_VIEW_DISTANCE`
- `LIGHT_RANGE()` already converts luminance into an approximate visible-distance result

In `src/lightmap.cpp`:
- weather `sight_penalty` participates in visibility behavior
- smoke/haze cache also affects whether detail is visible locally
- `apparent_light_at()` distinguishes between ordinary visibility and `BRIGHT_ONLY`, meaning strong light sources can still read as visible even when detail is not

Meaning:
- daylight, darkness, and weather already materially alter what can be seen
- the future visibility model should reuse this truth rather than pretending smoke/light visibility is totally separate from existing sight logic
- there is already real code footing for the intuition that a bright night light can be noticed at long range without granting clean tactical knowledge

### 6. Directional light is already a real local concept

This turned out better than expected.
The local lighting system is not just a scalar brightness soup.
It already carries directional structure that matters for outward leakage.

In `src/item.cpp`:
- `item::getlight()` can expose `luminance`, `width`, and `direction`
- width `> 0` means the source is an arc light, not just an omnidirectional glow

In `src/lightmap.cpp`:
- `apply_light_arc()` casts only the octants covered by the source arc
- `apply_directional_light()` already projects light from one cardinal approach into a tile
- sunlight leakage into interior openings is handled directionally during `generate_lightmap()`
- the lightmap stores `four_quadrants` per tile, not just one flat brightness value
- `apparent_light_helper()` can evaluate which quadrants of an opaque tile are visible from the observer side before deciding how much light is actually apparent

Meaning:
- the repo already knows the difference between light existing and light leaking from a particular side
- there is likely no need to invent a brand-new vector-visibility law from scratch just to get directional exposure semantics
- if bandit visibility later wants a cheap overmap adapter, a coarse side-bucket or quadrant-derived exposure summary would be much more grounded than a fresh fictional system

---

## Weather cases that matter immediately

The current weather data already supports several useful design truths.

### Fog / mist
`data/json/weather_type.json` includes both `mist` and `fog` with meaningful sight penalties and negative light modifiers.

Meaning:
- foggy or misty conditions already have engine footing for “harder to see things”
- a future bandit visibility model can honestly reduce smoke/light legibility under those conditions

### Rain / storms
Rain, thunderstorms, and rainstorms already carry stronger `sound_attn` and visibility penalties.

Meaning:
- weather already provides a clean reason for distant sound to become less legible
- storms and heavy rain can also legitimately lower visual confidence

### Wind
`get_local_windpower(...)` already depends on shelter, terrain, z-level, and blockers.

Meaning:
- wind already has enough reality to influence smoke spread/persistence
- the future model should likely use wind to modify smoke shape or confidence, not invent a separate fake wind system

---

## What seems directly reusable for bandit design

### Reusable now
- sound -> overmap signal precedent
- weather attenuation of sound
- weather-based light and sight penalties
- per-overmap-point weather queries
- real smoke creation from fires
- wind-aware smoke/gas spread
- terrain and shelter affecting wind strength

These are not speculative. They already exist.

---

## What still needs new glue

The missing pieces appear to be an adapter layer that does the following:

### 1. Convert physical traces into overmap-readable marks
For example:
- fire/smoke activity -> possible activity mark
- externally visible light source -> possible activity mark
- loud human activity -> possible activity mark

For light specifically, the missing glue is not just source detection.
It is an **external leakage / obstruction** judgment:
- is the light actually visible outside the structure or terrain pocket
- how suppressed is it by walls, curtains, indoor containment, or forest cover
- is the site more exposed from one approach direction than another

### 2. Interpret those marks as bandit interest, not omniscience
The bridge should output things like:
- location confidence
- bounty hint
- threat hint

Not:
- exact actor identity
- exact inventory truth
- perfect tactical knowledge

### 3. Decide cadence and aggregation rules
Likely questions:
- how often does cadence sample active fires/light/sound into overmap marks?
- how much does repetition reinforce confidence?
- when do weather conditions suppress or blur the result?
- when does a persistent signal become a stronger camp/basecamp suspicion rather than a one-off curiosity?

### 4. Keep local physics and overmap abstraction separate
The future system should not require simulating every smoke puff on the overmap.
It should read coarse consequences from real local systems and convert them into abstract marks.

For light, this likely means a cheap coarse exposure model instead of tile-perfect omnidirectional raycasting across the whole map.
The good news is that the local engine already has directional light arcs and quadrant-aware apparent-light handling, so the missing piece looks more like a summarizing adapter than a blank-sheet invention.
A directional bucket or side-exposure abstraction still looks much more plausible than full decorative realism.

A strong v1 candidate now looks like:
- sample only relevant active sites / signal regions
- skip or heavily suppress daylight cases
- reuse post-lightmap local light truth
- collapse that truth into 4 side buckets or quadrants plus a peak/contained summary
- then let weather and cadence rules decide whether a light-family overmap mark should exist at all

---

## Design constraints implied by this recon

### Constraint 1: do not invent a second weather system
The code already has real weather modifiers.
Bandit visibility should consume those rather than mirror them in a disconnected ruleset.

### Constraint 2: do not pretend smoke/light are already overmap beacons
Smoke, fire, and light exist physically, but the repo does not obviously already turn them into overmap bounty/threat signals.
That translation still needs explicit design.

For light, daylight should remain a hard suppressor, and closed/contained indoor light should not magically become a world-map beacon just because a lamp exists somewhere inside.
The code already supports directional leakage thinking, but it still does not hand bandit AI a ready-made overmap exposure score.

### Constraint 3: sound has the cleanest first precedent
Because the code already uses sound -> weather attenuation -> overmap signal, sound-driven investigation is the safest first bridge for later implementation thinking.

### Constraint 4: visibility should stay about confidence, not certainty
Existing systems support degraded perception under weather and terrain.
The future bandit visibility model should therefore produce uncertain marks and hints, not magical truth.

---

## Good implication for the next parked concept item

The next parked concept item should still be:
- player/basecamp visibility and concealment

But it should now be written on recon-backed footing.

A good shape for that later item is:
- signal sources
  - smoke
  - light
  - human activity
  - repeated settlement activity
- environmental filters
  - fog/mist
  - rain/storms
  - daylight/darkness
  - wind
  - shelter / terrain masking where relevant
- interpretation outputs
  - location confidence
  - bounty hint
  - threat hint
- concealment levers
  - fire discipline
  - light discipline
  - route discipline
  - weather exploitation

That would keep the design honest and tied to what the repo can actually support.

---

## What this note is not claiming

This recon does **not** prove that implementation is easy.
It only shows that the engine already contains several real hooks worth reusing.

It also does **not** yet prove:
- the exact best sampling cadence for smoke/light visibility
- the exact cost/performance shape of turning local activity into overmap marks
- the exact API surface where bandit overmap logic should read these signals

Those remain later design questions.

---

## Bottom line

The repo already provides a credible physical substrate for a later bandit visibility model.
The missing work is mostly not “invent smoke, weather, and light.”
The missing work is:

> build a disciplined adapter from existing physical systems into coarse overmap marks that bandits interpret as possible bounty and possible threat.
