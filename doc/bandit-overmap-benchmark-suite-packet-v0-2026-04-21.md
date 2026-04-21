# Bandit overmap benchmark suite packet v0 (2026-04-21)

## Status

This is **ACTIVE / GREENLIT**.
It is the current bounded bandit lane.

Josef explicitly held the z-level visibility packet back again.
The next job is not another edge-case promotion.
The next job is to build the real overmap proof suite and freeze a reviewer-readable benchmark table that can actually fail.

---

## Summary

Land one bounded overmap benchmark-suite packet on the current bandit playback / proof seam.
The goal is to turn the now-landed bandit slices into one coherent scenario family with explicit 100-turn benchmarks for every scenario, plus 500-turn carry-through checks where the longer horizon is what proves the behavior honestly.

The suite should not stop at binary pass/fail theater.
It should expose timing, cadence, revisit, backtrack, and cooldown metrics so Josef can see how long a camp takes to react, how often it revisits, whether it bounces stupidly, and whether the long horizon stays bounded.

If a scenario misses its benchmark, that should be treated as a routing-logic problem to fix, not as a reason to blur the benchmark until the packet can fake a pass.

---

## Scope

This lane should:
- build a complete named bandit overmap scenario family on the current playback/proof seam instead of leaving the proof picture spread across isolated narrow packets
- give every scenario a clear **100-turn** benchmark packet that is easy to read and easy to fail
- keep or add **500-turn** checks where the long horizon is the honest proof burden rather than optional decoration
- expose reviewer-readable timing and cadence metrics, for example first non-idle turn, first actionable turn, first scout departure, first target arrival, revisit count, route-flip count, and cooldown / drop turn where those are meaningful
- make the benchmark output reviewer-readable so Josef can inspect why a scenario passed or failed without debugger soup
- use benchmark failures to drive routing-logic fixes instead of watering the expectations down
- keep `Bandit z-level visibility proof packet v0` **not greenlit** until this suite exists and has already started surfacing the next real problems

---

## Explicit non-goals

This lane should **not** do any of the following:
- greenlight or implement the z-level packet in the same slice
- widen into a broad overmap architecture rewrite
- replace scenario benchmarks with vague prose like "looks smarter now"
- quietly drop the 500-turn burden just because the 100-turn checkpoints are easier to wire
- turn into player-present combat/extortion implementation work
- pretend benchmark failures are acceptable if the code "basically seems fine"

---

## Scenario family and benchmark table

### Reviewer-readable benchmark packet shape

Every scenario packet should emit a reviewer-readable mini-table, not just a winner line.
Use `none` / `n/a` explicitly where a metric does not apply.

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

### Required scenario set

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
   - 100-turn benchmark:
     - `first_non_idle_turn <= 20`
     - `first_departure_turn <= 40`
     - `first_frontier_gain_turn <= 80`
     - winner at turn `100` is not indefinite `hold / chill`
     - `frontier_visibility_delta_100 > 0`
   - 500-turn benchmark:
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
   - 100-turn benchmark:
     - blocked routes do not silently mutate into random wandering
     - `random_wander_start_count_100 = 0`
   - 500-turn benchmark:
     - `explicit_explore_departure_count_500 = 0` unless the scenario explicitly grants explore footing

3. **hidden_side_light_stays_inert**
   - Core ask: directional light that does not honestly leak to the bandit side stays non-actionable.
   - Required metrics:
     - `first_actionable_turn`
     - `intercept_commit_count_100`
     - `target_visit_count_500`
   - 100-turn benchmark:
     - `first_actionable_turn = none`
     - `intercept_commit_count_100 = 0`
   - 500-turn benchmark:
     - `target_visit_count_500 = 0`

4. **visible_side_light_becomes_actionable**
   - Core ask: the matching visible-side light case should become actionable under the same broader footing.
   - Required metrics:
     - `first_actionable_turn`
     - `first_departure_turn`
     - `first_arrival_turn`
     - `target_visit_count_100`
   - 100-turn benchmark:
     - `first_actionable_turn <= 40`
     - `first_departure_turn <= 60`
     - reviewer-readable output shows the light-born clue becoming a real actionable posture rather than staying inert
   - 500-turn benchmark:
     - `target_visit_count_100 >= 1` or the equivalent explicit intercept posture lands honestly before turn `100`

5. **corridor_light_shares_horde_pressure**
   - Core ask: the same corridor light should not become private bandit-only truth.
   - Required metrics:
     - `first_actionable_turn`
     - `shared_pressure_event_count`
     - `private_omniscience_flag`
   - 100-turn benchmark:
     - the report shows shared corridor consequence with zombie pressure instead of isolated bandit omniscience
     - `shared_pressure_event_count >= 1`
     - `private_omniscience_flag = false`

6. **local_loss_rewrites_to_withdrawal**
   - Core ask: when local reality turns the tile too hot, stale pursuit should collapse.
   - Required metrics:
     - `first_withdrawal_turn`
     - winner at turn `100`
     - `burned_route_retry_count_500`
   - 100-turn benchmark:
     - `first_withdrawal_turn <= 100`
     - by tick `100`, the stale corridor posture has rewritten to withdrawal or `hold / chill`
   - 500-turn benchmark:
     - `burned_route_retry_count_500 = 0`
     - the long horizon stays off the burned route instead of regrowing stale pursuit by habit

7. **local_loss_reroutes_to_safer_detour**
   - Core ask: some cases should reroute instead of merely giving up.
   - Required metrics:
     - `first_reroute_turn`
     - `safer_detour_visit_count_500`
     - `burned_route_retry_count_500`
   - 100-turn benchmark:
     - `first_reroute_turn <= 100`
     - the report shows a safer detour or equivalent reroute rather than direct recommitment to the burned path
   - 500-turn benchmark:
     - `safer_detour_visit_count_500 >= 1`
     - `burned_route_retry_count_500 = 0`

8. **moving_prey_memory_stays_bounded**
   - Core ask: moving bounty can be stalked briefly, but not forever.
   - Required metrics:
     - `first_actionable_turn`
     - `memory_refresh_count_100`
     - `drop_turn`
     - `retry_after_drop_count_500`
   - 100-turn benchmark:
     - active moving memory still exists when the clue honestly remains warm enough
     - `first_actionable_turn <= 60`
   - 500-turn benchmark:
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
   - 100-turn benchmark:
     - repeated mixed signals can strengthen one site mark modestly and reviewer-readably
   - 500-turn benchmark:
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
   - 100-turn benchmark:
     - the smoke clue remains potentially scoutable when strong enough
     - the report shows worse source precision / confidence or a fuzzier displaced read than the same clear-weather case

11. **portal_storm_smoke_is_harder_to_localize**
   - Core ask: portal-storm smoke should be explicit, dark, weird, and less trustworthy.
   - Required metrics:
     - `source_precision_grade`
     - `confidence_grade`
     - `corridorish_displacement_flag`
   - 100-turn benchmark:
     - portal-storm smoke shows worse source certainty or more displaced/corridor-ish interpretation than ordinary night smoke

12. **portal_storm_exposed_light_stays_legible_but_sheltered_light_stays_bounded**
   - Core ask: dark storm conditions can still let exposed bright light read, but sheltered ordinary light should not turn into a magical beacon.
   - Required metrics:
     - `exposed_light_actionable_turn`
     - `sheltered_light_actionable_turn`
     - `exposed_vs_sheltered_confidence_delta`
   - 100-turn benchmark:
     - exposed bright light remains legible when that is the honest read
     - sheltered ordinary light stays materially more bounded under the same storm family
     - the packet shows an explicit exposed-versus-sheltered difference instead of flattening both cases together

13. **independent_camps_do_not_dogpile_by_default**
   - Core ask: camps should behave as mostly independent actors, not automatic coalition spam.
   - Required metrics:
     - `camp_a_first_commit_turn`
     - `camp_b_first_commit_turn`
     - `same_target_overlap_turns_500`
     - `other_camp_overlap_count_500`
   - 100-turn benchmark:
     - two camps do not converge on the same target by default without their own honest local reasons
   - 500-turn benchmark:
     - `other_camp_overlap_count_500 <= 1`
     - `same_target_overlap_turns_500` stays low and reviewer-readable instead of looking like routine coalition behavior
     - repeated same-region pile-on pressure stays suppressed unless a later scenario gives both camps genuine separate reasons

14. **other_camps_read_as_threat_bearing_not_default_allies**
   - Core ask: another camp usually reads as threat-bearing pressure, not free coalition support.
   - Required metrics:
     - `first_other_camp_read_turn`
     - `ally_assumption_flag`
     - `threat_bearing_read_flag`
   - 100-turn benchmark:
     - the report does not treat the other camp as automatic ally truth on first read
     - `ally_assumption_flag = false`
     - `threat_bearing_read_flag = true`

---

## Success state

This lane is done for now when:
- one complete named overmap benchmark-suite packet exists on the current bandit playback / proof seam
- every required scenario above has a clear 100-turn benchmark packet that is easy to inspect and easy to fail
- the scenarios that genuinely need the longer horizon also carry 500-turn carry-through checks instead of pretending 100 turns proves everything
- reviewer-readable output explains why each scenario passed or failed, including timing / cadence / revisit counters where they matter
- the packet stays bounded: no z-level implementation, no broad architecture rewrite, no vague benchmark theater, and no hand-waved passes when routing logic is still wrong

---

## Testing shape

- docs-only canon patch -> no compile
- code change later -> rebuild the relevant bandit targets and run the current bandit suite plus the new scenario packet coverage
- this packet specifically exists to tighten the proof burden, so the evidence should be explicit and scenario-shaped, not hidden in one generic `[bandit]` pass summary
- the evidence should show timeline and counter data where the scenario is about motion or persistence, for example first scout turn, first arrival turn, visit counts, revisit counts, and back-and-forth counts by turn `500`

Expected proof shape later:
- `make -j4 tests`
- `./tests/cata_test "[bandit][playback]"`
- `./tests/cata_test "[bandit][marks]"`
- `./tests/cata_test "[bandit]"`

---

## Product guardrail

This packet should make the bandits easier to trust and easier to criticize.
That is the point.
If the camp behaves stupidly, the suite should say so plainly.
If the camp behaves well, the suite should earn that too.
