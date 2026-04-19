# Bandit visibility physical-systems recon (2026-04-19)

## Status

This is a **supporting recon note** for the parked bandit concept chain.
It is not the player/basecamp visibility concept item itself, and it is not an implementation greenlight.

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
- `src/weather.cpp`
- `src/character.cpp`
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
- overmap sight range depends on light level
- overmap LOS then burns sight points through terrain `get_see_cost()`

In `src/lightmap.cpp`:
- weather `sight_penalty` participates in visibility behavior
- smoke/haze cache also affects whether detail is visible locally

Meaning:
- daylight, darkness, and weather already materially alter what can be seen
- the future visibility model should reuse this truth rather than pretending smoke/light visibility is totally separate from existing sight logic

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
- visible light source -> possible activity mark
- loud human activity -> possible activity mark

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

---

## Design constraints implied by this recon

### Constraint 1: do not invent a second weather system
The code already has real weather modifiers.
Bandit visibility should consume those rather than mirror them in a disconnected ruleset.

### Constraint 2: do not pretend smoke/light are already overmap beacons
Smoke, fire, and light exist physically, but the repo does not obviously already turn them into overmap bounty/threat signals.
That translation still needs explicit design.

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
