# Brazier smoke and light: strength, time and visibility

Use this when preparing a sustained fire or when a camp has not noticed it. Use the bound
product's actual source and receiver observations; historical calibration is not current reach.
The standard indoor brazier is [3159,3449,0], roof brazier [3156,3447,1]. Verify the selected
save's actual furniture, fuel, fire, windows and source positions.

## Prepared-save day/night comparison

For a focused fire-visibility comparison, the owner open/closed-window
[standard saves](../GAME-MANUAL.md#verified-save-templates) provide ready ground-floor fire setups.
Use them when they fit the question; a different save should offer a concrete testing advantage.
Adapt surrounding camps/routes as needed while retaining the intended window/fire geometry.
An R050 remote-field derivative is a different starting scene, even if copied into an entry
named day or night. The remote journey distances and scout/report sequence below belong to
end-to-end journey tests, not this focused comparison.

The endpoint is actual daytime/nighttime smoke and firelight visibility under the chosen
windows, weather and observer geometry. Ignition and tending are setup. For Josef's selected
natural-night route, keep the fire fueled, let ordinary game time reach night, quicksave and
inspect fresh smoke and light evidence from that state. Missing time-edit controls do not
prevent ordinary waiting. Closed windows should be tested as physical concealment; positive
night light needs an exposed path and eligible observer. Separate emitted samples, physical
escape and actual reception; never substitute one for another or infer suppression from an
absent receiver. Use existing saved site/roster facts to establish the candidate observer,
then check its actual opportunity in the running scene. Carry established facts forward in
the current brief with their save/run identity instead of repeatedly rediscovering them.

## Representative source placement

Owner clarification, 2026-10-26: representative end-to-end camp/tower stories require at least
10 OMTs between the player activity/source area and the camp or tower. Keep the player at least
10 OMTs from the participating home footprints during the representative remote leg; measure from
the nearest footprint edge, not just its anchor. Record actual home, player, source and observer
coordinates/z and OMT distances. Label the footprint-edge layout metric separately from the
native observer-to-source distance and measured reception cap; diagonal layout separation does
not establish that a source is in range. For 30–40 OMT reach scenarios, use substantially greater
separation that exercises the claimed reach, with actual signal strength, terrain and weather
supporting the selected case. Ten OMTs is a floor, not sufficient coverage of long-range reach.
For ordinary full stories, choose a believable separated setup; about15 OMT is a useful starting
point, not a precision target. Josef explicitly accepts31 OMT for a roughly30-OMT journey. Do not
reposition or restart solely to match a nominal number. Exact distances matter when interpreting
a specific range/threshold result, and must be distinguished from the intended setup. Keep the emitter actually loaded/active
when its sampling requires that; the remote party is the camp, not necessarily the fire.

Closer setups remain explicitly labelled component, near-contact, combat or diagnostic controls;
they do not count as representative distant sensing, scouting or return proof. A necessary close-home
setup visit earns no remote proof; return to the separated player/source area before the remote leg.
Stay near the emitter when its native sampling requires a loaded source. If existing source reach
cannot support the minimum, report that coverage
limit and use a suitable scenario rather than silently moving the camp/player closer or relaxing
observation. Preserve current saves, actors and input ownership when adapting a live continuation.
Do not treat historical example coordinates/formulas below as current calibration.

Across the selected signal/journey recipes, include useful separated positive cases and meaningful
near-reach or beyond-reach/occluded negatives. Reuse existing variants; this does not order a full
suite replay or a new run matrix. Keep near-contact, combat, hazard and other deliberately short
controls when their purpose calls for them. Source receipt, scene recognition, selection of a
watching position and physical travel are separate outcomes; a distant signal need not reveal
nearby actors or permit a remote observer to inspect interior details.

Missing remote lighting is a separate diagnosis, not a reason to keep moving the brazier next to
the camp. AI observation must not require the player to see or explore the target. Distinguish
player visibility/history from loaded-map lighting, existing saved geometry and actually missing
physical inputs. At a demonstrated discrepancy, compare matched observer/target/time/terrain with
player near versus far and isolate which geometry the lighting calculation requires. Preserve real
darkness, roofs, occlusion and observer capability; unavailable data is neither proof of darkness
nor permission to declare visibility. Moving the player nearby may be a diagnostic comparison,
not evidence that the original remote behavior is correct. Fix a demonstrated dependency at its
owner rather than bypass it with easier placement. Coordinate any live setup change with the
existing input owner and preserve the first divergence and its reproduction context.

## Keep the source alive through the useful scan

Prefer the compatible [prepared two-brazier template](../GAME-MANUAL.md#verified-save-templates)
and its actual stock/zone geometry. Keep the source loaded and choose a practical tending position.
The [fire and zone recipe](../GAME-MANUAL.md#normalized-firewood-zones-and-stock) owns source selection,
reach/access and the activity condition: use a real refuelling-enabled activity such as `ACT_WAIT`
when automatic tending is intended. A cancelled wait, nearby avatar or enabled zone alone does not
prove replenishment.

Distinguish fuel already on the burning tile from reachable burnable stock inside the enabled
`SOURCE_FIREWOOD` zone. Expand the zone over usable wood or add ordinary stock where needed;
keep it separate enough to observe transfer when testing automatic tending. Inspect the actual
activity and fire behavior after an interruption or setup correction. Fuel movement/messages and
before/after stock support replenishment; an unchanged stock can be normal while existing fuel is
sufficient. Do not infer a universal duration from a wood count or the number of serialized entries.

The minute8355 Mixed checkpoint sustained a fire for300 turns with eight added firewood pieces,
an enabled source zone and an adjacent player. It established persistence and exterior samples,
not automatic refuelling. Later fuel failures still require their actual cause to be resolved;
stock, distance/loading and tending activity are competing premises, not preset diagnoses.

In the prepared R050 geometry, a player adjacent to the ground brazier waited with the enabled
kindling zone reachable. The fire stayed lit for 60 minutes; during the second 30 minutes, five
native fuel-drop messages matched the splinter stock falling from 999 to 994. After walking the
actual ladder to the roof, the player lit the roof brazier and waited nearby: it stayed lit for
30 minutes, with five splinters transferred from that same ground-level stock. This is a verified
recipe for that saved geometry, not a universal cross-level reach or fuel-duration guarantee. The
roof fire later went out during longer waits; check it and its supply again before relying on it.

Keep the useful sampling window proportional to the question. Source persistence, signal sampling,
reception and dispatch are separate observations. Once a stable source is sampled and the receiver
repeatedly refuses the same range or sightline, investigate that reason instead of repeating fuel
waits or declaring that no source exists.

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

Read the actual smoke packet and its receiver projection for the bound executable.
[`adapt_smoke_packet`](../../../../src/bandit_mark_generation.cpp) derives viability and projected
range from source strength, persistence, height/spread and weather. The current source supports a
30-OMT ceiling; this is not a promise that an ordinary brazier produces a maximum-strength packet.
A cap alone does not establish reception: actual distance, terrain and physical visibility still apply.
Smoke does not gain the night-time light advantage. A visible puff, an emitted packet and a camp's
received observation are different facts.

## Ordinary brazier light: nominal range in OMT

Qualify the prepared roof brazier at **night in clear weather**, with actual supported roof footing,
fuel and exposure. Read the LIGHT channel independently from smoke; weak or absent visible smoke
does not settle whether the fire emits useful light.

[`adapt_light_packet`](../../../../src/bandit_mark_generation.cpp) combines source strength,
persistence, exposure/aperture, elevation, weather and daylight. The current source allows an
extended ceiling of40 OMT for qualifying elevated exposed light; ground/other light has a30-OMT
ceiling. These are limits, not expected brazier reach. The actual packet and receiver determine
whether this fire under these conditions is sufficient. Use current projections rather than old
intensity-to-range tables from another build.

`physical_light::evaluate_escape` checks an actual transparent path to an outside tile, not a
README's “windows open” label. Fully contained light cannot shine through a sealed room merely
because its packet has a positive cap. Keep window, roof and weather distinctions meaningful.

## Terrain, height and actual reach

Exposed smoke and light can be seen across z-levels when their physical visibility conditions
hold. Current `do_turn.cpp::live_bandit_overmap_smoke_los_from` uses the observable plume position
or exposed-to-sky source for cross-level smoke and pays intervening terrain costs. A current loaded
source with neither observable plume nor sky exposure is rejected. The generic
`live_bandit_overmap_los_from` still rejects differing z, but it is not the complete smoke path.
Use the bound run's actual smoke support and admission result; neither roof height alone nor
extra waiting establishes visibility. Light has its own exposure/vertical path.

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

An early weak sample does not establish the mature fire's maximum. A changed packet can change reach. A hard cross-z
rejection or an unchanged valid range/occlusion failure is a different diagnosis. If something
“worked five seconds ago,” compare exact source/sample IDs, intensities, height, weather, build and
observer—not just the fire's name. Keep progressing while the inputs are evolving; preserve the
first repeatable contradiction when they are not. Neither one early rejection nor endless waiting
is a complete playtest.
