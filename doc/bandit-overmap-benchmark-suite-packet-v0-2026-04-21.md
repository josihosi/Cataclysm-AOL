# Bandit overmap benchmark suite packet v0 (2026-04-21)

## Status

This is **CHECKPOINTED / DONE FOR NOW**.

Josef explicitly held the z-level visibility packet back again.
That part stayed true.
What changed is simpler: the benchmark suite now exists and closes honestly on the current tree.

---

## Summary

One bounded overmap benchmark-suite packet now exists on the current bandit playback / proof seam.
The current tree turns the now-landed bandit slices into one coherent scenario family with explicit `100`-turn benchmarks for every required scenario, plus `500`-turn carry-through checks where the longer horizon is what proves the behavior honestly.

The suite does not stop at binary pass/fail theater.
It now exposes timing, cadence, revisit, backtrack, and cooldown metrics so Josef can see how long a camp takes to react, how often it revisits, whether it bounces stupidly, and whether the long horizon stays bounded.

The review burden is also human-facing.
The output is now plain enough that Schani or Josef can look at it and answer, without hand-wavy guesswork:
- is it leiwand or not leiwand?
- is it actually fun?
- do things happen on the map, or does it still feel dead?
- do the results show real emergent activity, pressure, scouting, revisits, and movement, or mostly inert legal-but-boring behavior?

Current honest closure points:
- `src/bandit_playback.{h,cpp}` now carries the named packet via `run_overmap_benchmark_suite_packet()` plus `render_overmap_benchmark_suite_packet( const benchmark_suite_result &result )`
- the suite covers fourteen named scenarios, including the explicit empty-frontier, blocked-route, weather, and inter-camp independence packets
- `tests/bandit_playback_test.cpp` now proves the contract, metric surface, and rendered packet shape honestly
- narrow deterministic validation passed on the current tree via `make -j4 tests`, `./tests/cata_test "[bandit][playback]"`, and `./tests/cata_test "[bandit]"`

If later evidence shows a scenario benchmark lying or drifting, treat that as a routing-logic or proof-surface problem to fix, not as a reason to blur the benchmark until the packet can fake a pass.

---

## Scope

This lane should:
- build one complete named overmap scenario family on the current playback/proof seam instead of leaving the proof picture spread across isolated narrow packets
- give every scenario a clear **`100`-turn** benchmark packet that is easy to read and easy to fail
- keep or add **`500`-turn** checks where the long horizon is the honest proof burden rather than optional decoration
- expose reviewer-readable timing and cadence metrics, for example first non-idle turn, first actionable turn, first scout departure, first target arrival, revisit count, route-flip count, and cooldown / drop turn where those are meaningful
- make the benchmark output reviewer-readable so Josef can inspect why a scenario passed or failed without debugger soup
- keep the packet reviewer-readable enough that Schani or Josef can judge plainly whether the result is leiwand, fun, alive on the map, and genuinely emergent rather than merely legal-but-boring
- use benchmark failures to drive routing-logic fixes instead of watering the expectations down
- keep `Bandit z-level visibility proof packet v0` **not greenlit** until this suite exists and has already started surfacing the next real problems

---

## Explicit non-goals

This lane should **not** do any of the following:
- greenlight or implement the z-level packet in the same slice
- widen into a broad overmap architecture rewrite
- replace scenario benchmarks with vague prose like "looks smarter now"
- quietly drop the `500`-turn burden just because the `100`-turn checkpoints are easier to wire
- turn into player-present combat/extortion implementation work
- pretend benchmark failures are acceptable if the code "basically seems fine"

---

## Reviewer-readable benchmark packet shape

Every scenario packet should emit a reviewer-readable mini-table, not just a winner line.
Use `none` or `n/a` explicitly where a metric does not apply.

Preferred metric families:
- **turn metrics**: `first_non_idle_turn`, `first_actionable_turn`, `first_departure_turn`, `first_arrival_turn`, `first_withdrawal_turn`, `cooldown_turn`, `drop_turn`
- **count metrics**: `visit_count_100`, `visit_count_500`, `revisit_count_500`, `route_flip_count_500`, `burned_route_retry_count_500`, `shared_pressure_event_count`, `other_camp_overlap_count_500`
- **delta metrics**: `frontier_visibility_delta_100`, `frontier_visibility_delta_500`, confidence/precision deltas, threat deltas, bounty deltas where meaningful
- **state metrics**: winner/action at turn `100`, winner/action at turn `500`, last meaningful action, final state, whether the long horizon stayed bounded

If the scenario is about scouting or camp visits, the packet should say plainly:
- how many turns it took before the camp first decided to scout
- how many turns it took before the camp first arrived at the relevant frontier / camp / target area
- how many total visits and revisits happened by turn `500`
- how many obvious back-and-forth reversals happened by turn `500`

---

## Required scenario set

1. **empty_frontier_expands_visibility**
   - Core ask: when there is nothing useful on the current map, the camp should not sit forever. It should venture out through bounded scout/explore behavior and increase frontier visibility.
   - Required metrics:
     - `first_non_idle_turn`
     - `first_departure_turn`
     - `first_frontier_gain_turn`
     - `frontier_visibility_delta_100`
     - `frontier_trip_count_500`
     - `frontier_revisit_count_500`
     - `route_flip_count_500`
   - `100`-turn benchmark:
     - `first_non_idle_turn <= 20`
     - `first_departure_turn <= 40`
     - `first_frontier_gain_turn <= 80`
     - winner at turn `100` is not indefinite `hold / chill`
     - `frontier_visibility_delta_100 > 0`
   - `500`-turn benchmark:
     - `frontier_trip_count_500` stays bounded, target band `1..4`
     - `frontier_revisit_count_500 <= 2`
     - `route_flip_count_500 <= 2`
     - the outing stays bounded and reviewer-readable instead of degrading into immortal random wandering

2. **blocked_route_stays_fail_closed_until_explicit_explore**
   - Core ask: unreachable work must still fail closed instead of inheriting engine random-goal nonsense.
   - Required metrics:
     - `blocked_goal_attempt_count_100`
     - `random_wander_start_count_100`
     - `explicit_explore_departure_count_500`
   - `100`-turn benchmark:
     - blocked routes do not silently mutate into random wandering
     - `random_wander_start_count_100 = 0`
   - `500`-turn benchmark:
     - `explicit_explore_departure_count_500 = 0` unless the scenario explicitly grants explore footing

3. **hidden_side_light_stays_inert**
   - Core ask: directional light that does not honestly leak to the bandit side stays non-actionable.
   - Required metrics:
     - `first_actionable_turn`
     - `intercept_commit_count_100`
     - `target_visit_count_500`
   - `100`-turn benchmark:
     - `first_actionable_turn = none`
     - `intercept_commit_count_100 = 0`
   - `500`-turn benchmark:
     - `target_visit_count_500 = 0`

4. **visible_side_light_becomes_actionable**
   - Core ask: the matching visible-side light case should become actionable under the same broader footing.
   - Required metrics:
     - `first_actionable_turn`
     - `first_departure_turn`
     - `first_arrival_turn`
     - `target_visit_count_100`
   - `100`-turn benchmark:
     - `first_actionable_turn <= 40`
     - `first_departure_turn <= 60`
     - reviewer-readable output shows the light-born clue becoming a real actionable posture rather than staying inert
   - `500`-turn benchmark:
     - `target_visit_count_100 >= 1` or the equivalent explicit intercept posture lands honestly before turn `100`

5. **corridor_light_shares_horde_pressure**
   - Core ask: the same corridor light should not become private bandit-only truth.
   - Required metrics:
     - `first_actionable_turn`
     - `shared_pressure_event_count`
     - `private_omniscience_flag`
   - `100`-turn benchmark:
     - the report shows shared corridor consequence with zombie pressure instead of isolated bandit omniscience
     - `shared_pressure_event_count >= 1`
     - `private_omniscience_flag = false`

6. **local_loss_rewrites_to_withdrawal**
   - Core ask: when local reality turns the tile too hot, stale pursuit should collapse.
   - Required metrics:
     - `first_withdrawal_turn`
     - winner at turn `100`
     - `burned_route_retry_count_500`
   - `100`-turn benchmark:
     - `first_withdrawal_turn <= 100`
     - by tick `100`, the stale corridor posture has rewritten to withdrawal or `hold / chill`
   - `500`-turn benchmark:
     - `burned_route_retry_count_500 = 0`
     - the long horizon stays off the burned route instead of regrowing stale pursuit by habit

7. **local_loss_reroutes_to_safer_detour**
   - Core ask: some cases should reroute instead of merely giving up.
   - Required metrics:
     - `first_reroute_turn`
     - `safer_detour_visit_count_500`
     - `burned_route_retry_count_500`
   - `100`-turn benchmark:
     - `first_reroute_turn <= 100`
     - the report shows a safer detour or equivalent reroute rather than direct recommitment to the burned path
   - `500`-turn benchmark:
     - `safer_detour_visit_count_500 >= 1`
     - `burned_route_retry_count_500 = 0`

8. **moving_prey_memory_stays_bounded**
   - Core ask: moving bounty can be stalked briefly, but not forever.
   - Required metrics:
     - `first_actionable_turn`
     - `memory_refresh_count_100`
     - `drop_turn`
     - `retry_after_drop_count_500`
   - `100`-turn benchmark:
     - active moving memory still exists when the clue honestly remains warm enough
     - `first_actionable_turn <= 60`
   - `500`-turn benchmark:
     - `drop_turn` becomes explicit once the contact has honestly gone stale
     - `retry_after_drop_count_500 = 0`
     - stale moving contact drops cleanly on leash expiry and does not retry forever

9. **repeated_site_interest_stays_bounded**
   - Core ask: repeated site signals can strengthen revisit interest, but not unlock immortal settlement truth.
   - Required metrics:
     - `site_visit_count_500`
     - `site_revisit_count_500`
     - `cooldown_turn`
     - `endless_pressure_flag`
   - `100`-turn benchmark:
     - repeated mixed signals can strengthen one site mark modestly and reviewer-readably
   - `500`-turn benchmark:
     - `site_visit_count_500` stays bounded, target band `1..3`
     - `site_revisit_count_500 <= 2`
     - `endless_pressure_flag = false`
     - the strengthened site still cools back out instead of becoming endless pressure

10. **windy_smoke_stays_scoutable_but_fuzzy**
   - Core ask: wind should not just subtract one token number from smoke.
   - Required metrics:
     - `first_actionable_turn`
     - `source_precision_grade`
     - `confidence_grade`
     - `displacement_flag`
   - `100`-turn benchmark:
     - the smoke clue remains potentially scoutable when strong enough
     - the report shows worse source precision / confidence or a fuzzier displaced read than the same clear-weather case

11. **portal_storm_smoke_is_harder_to_localize**
   - Core ask: portal-storm smoke should be explicit, dark, weird, and less trustworthy.
   - Required metrics:
     - `source_precision_grade`
     - `confidence_grade`
     - `corridorish_displacement_flag`
   - `100`-turn benchmark:
     - portal-storm smoke shows worse source certainty or more displaced/corridor-ish interpretation than ordinary night smoke

12. **portal_storm_exposed_light_stays_legible_but_sheltered_light_stays_bounded**
   - Core ask: dark storm conditions can still let exposed bright light read, but sheltered ordinary light should not turn into a magical beacon.

13. **independent_camps_do_not_dogpile_by_default**
   - Core ask: one camp reacting does not mean every other camp becomes a free allied chorus line.

14. **other_camps_read_as_threat_bearing_not_default_allies**
   - Core ask: another camp should read as a possible threat-bearing factional presence, not as automatic reinforcement.

Keep the naming exact unless Josef explicitly renames the packet.

---

## Success state

This lane is done for now when:
- one complete named overmap benchmark-suite packet exists on the current bandit playback/proof seam
- every required scenario above has a clear `100`-turn benchmark packet that is easy to inspect and easy to fail
- the scenarios that genuinely need the longer horizon also carry `500`-turn carry-through checks instead of pretending `100` turns proves everything
- reviewer-readable output explains why each scenario passed or failed
- the packet is reviewer-readable enough that Schani or Josef can answer plainly whether it is leiwand, actually fun, alive on the map, and showing real emergent activity rather than inert legal-but-boring behavior
- the packet stays bounded: no z-level implementation, no broad architecture rewrite, no vague benchmark theater, and no hand-waved passes when routing logic is still wrong

---

## Testing shape

- docs-only canon patch -> no compile
- code change later -> rebuild the relevant bandit targets and rerun the bandit packet coverage
- this packet exists to tighten the proof burden, so the evidence should stay explicit and scenario-shaped rather than buried in one generic `[bandit]` line

Expected proof shape later:
- `make -j4 tests`
- `./tests/cata_test "[bandit][playback]"`
- `./tests/cata_test "[bandit][marks]"`
- `./tests/cata_test "[bandit]"`

---

## Product guardrail

Bandits should become easier to trust and easier to criticize.
That is the whole point of this packet.
If the camp behaves stupidly, the suite should say so plainly.
If the camp behaves well, the suite should earn that too.

The reviewer should be able to read the packet and answer, plainly and without mystical interpolation:
- is it leiwand or not leiwand?
- is it actually fun?
- do things happen on the map, or does it still feel dead?
- do the results show real emergent activity, pressure, scouting, revisits, and movement, or mostly inert legal-but-boring behavior?
