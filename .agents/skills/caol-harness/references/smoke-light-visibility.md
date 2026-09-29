# Brazier smoke and light: strength, time and visibility

Use this when a fire is burning but a camp has not noticed it. Values below describe the current
production adapter inspected 2026-09-28; a source-bound run's measured packet is authoritative.
The standard indoor brazier is [3159,3449,0], roof brazier [3156,3447,1]. Verify the selected
save's actual furniture, fuel, fire, windows and source positions.

## Do not conclude from the first puff

Ignition, smoke fields, a viable signal, observer admission and dispatch are different events.
One weak sample establishes only that moment's range. Continue through relevant field changes
and the next observer scan using normal `safe` waits; don't repeatedly relight a burning brazier.
Compare samples at changed fire/smoke intensity, weather or time band. If the source is steady
and the rejection unchanged, diagnose that boundary instead of waiting indefinitely.

`field_processor_fd_fire` in `src/map_field.cpp` consumes fuel and probabilistically produces
smoke. It puts smoke on the tile above when that tile has no floor, otherwise on the fire tile.
Fire can strengthen or decay with fuel/field age; elapsed seconds alone do not guarantee growth.
A brazier contains fire spread; that does not mean its smoke/light is invisible.

The adapter's “persistence” is a current-intensity flag, **not time spent burning**. Signal range
does not accumulate every minute. Samples are per source map-square: smoke above the brazier
and fire below can have separate IDs and different z. Don't add their intensities together.

`live_light.cpp` samples once per advancing game turn. The loaded-source index rechecks active
sources each turn; discovery of a new non-player tile uses a 900-turn sweep (up to15 game minutes),
while the player tile is checked directly. Reading `look` does not advance this. Staffed camp
signal recording is on five-minute boundaries; dispatch cadence is30 minutes and structural
maintenance60 minutes (`do_turn.cpp::overmap_npc_move`). These are opportunities, not promises
of dispatch: observer eligibility, existing outings and retained knowledge also matter.

## Smoke: nominal range in OMT

From `bandit_mark_generation.cpp::adapt_local_field_signal_reading` and `adapt_smoke_packet`:
let F be sampled fire intensity, S smoke intensity on **that same tile**, A=clamp(F+S,1,3),
P=1 if S>0 else0, H=1 if F>=2 else0. With F>0 or S>0:

`cap = clamp(4 + 2*A + 2*P + H - weather_penalty, 1, 15)`

The packet is viable only if `A + P + H - weather_penalty > 0`. Therefore a positive
printed cap alone is insufficient. Clear-weather examples:

- F1/S0: cap6; F1/S1: cap10; F1/S2 or S3: cap12.
- F2/S0: cap9; F2 with smoke: cap13. F3/S0: cap11; F3 with smoke: cap13.
- A separate smoke-only tile F0/S1, S2, S3: caps8,10,12.

Subtract0 clear,1 windy,2 rain,3 fog,1 portal storm from the cap and visibility score.
The runtime classifies portal storm first, then precipitation, then sight_penalty>=2 as fog,
then windspeed>=20 as windy. Smoke has **no day/night penalty** in this adapter.
These are emitted range limits, not guaranteed detection radii through terrain.

## Ordinary brazier light: nominal range in OMT

For a brazier without another emitter on the tile, A=clamp(F,1,3), P=1 if F>=2 else0.
The formula is `cap = clamp(1 + 2*A + P + M, 0, 30)`, viable only if `A + P + M > 0`.
M combines exposure, aperture leakage, roof elevation, weather and time; this is ordinary fire,
not the searchlight bonus.

For an exposed flat-roof brazier, M=2 exposure + L leakage +2 roof bonus - T - W.
L is the measured1 or2, not assumed. T=0 night,2 dawn/dusk,6 daylight. W=0 clear,2 rain,3 fog.
Clear roof examples (L1–2), before terrain costs:

- F1: night8–9, twilight6–7, day2–3. Day/L1 is **nonviable** despite cap2.
- F2: night11–12, twilight9–10, day5–6.
- F3: night13–14, twilight11–12, day7–8.

An exposed ground-level fire lacks the roof +2. A direct indoor-to-exterior aperture uses
leakage2 and exposed classification but no roof bonus. `physical_light::evaluate_escape`
checks an actual transparent path to an outside tile within three local tiles, not a README's
“windows open” label. Fully contained light is rejected by `physical_light::detect`; it cannot
shine through a sealed room just because its packet has a positive cap.
Portal storms normally subtract3 for exposed light, reduced to1 for exposed F>=2 at night/twilight;
contained light has additional penalty. Inspect the actual packet rather than assuming clear weather.

## Terrain, height and actual reach

**Intended behavior:** exposed smoke and light can be seen across z-levels with a clear physical
path; a roof one floor above is not automatically hidden. Current light already has a cross-z
exposure path. The inspected smoke reader still rejects differing z in
`live_bandit_overmap_los_from`; the roof-smoke repair is pending. Do not describe that rejection
as intended gameplay or claim waiting fixes it. Recheck the repaired build before retaining this caveat.

The current coarse overmap sight model uses `line_to(observer, source)` and terrain see costs:
clear/none0, low1, medium2, spaced-high4, high5, full-high10, opaque999.
For distinct points, the ray **excludes its observer origin and includes the source endpoint**;
coincident points produce one origin tile. Earlier reports saying both endpoints always count,
and therefore the observer camp's cost5 always blocks light, were wrong.

Light needs `distance + sum(ray terrain costs) <= cap`, plus exposure/vertical sightline.
Current same-level smoke needs distance<=cap and independently sum(costs)<=cap; it does not
subtract distance from that second budget. Preserve that distinction when explaining results.
A high-cost source or intervening forest/building can reduce reach; don't charge the camp origin
when it is absent from the actual ray. Use the game's `rl_dist` and actual ray, not guessed geometry.

## Judge the experiment

At useful changes record source tile/ID, F/S, sample turn, weather/time band, exposure, cap,
observer position, measured distance, ray costs and admission/rejection. Use existing bound
channel/transition logs and compact native observations; no normal-game analytical spam.
A live_smoke packet can be generated from fire alone, so separately inspect fd_smoke when proving
visible smoke fields. Extinguished fire can leave smoke and already learned camp knowledge.

The earlier roof sample cap6 smoke/cap4 light versus distance7 was out of range **then**;
it does not establish the mature fire's maximum. A changed packet can change reach. A hard cross-z
rejection or an unchanged valid range/occlusion failure is a different diagnosis. If something
“worked five seconds ago,” compare exact source/sample IDs, intensities, height, weather, build and
observer—not just the fire's name. Keep progressing while the inputs are evolving; preserve the
first repeatable contradiction when they are not. Neither one early rejection nor endless waiting
is a complete playtest.
