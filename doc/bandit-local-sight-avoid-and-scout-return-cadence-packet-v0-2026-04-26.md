# Bandit local sight-avoid + scout return cadence packet v0 (2026-04-26)

Status: reopened / active follow-up.

2026-04-27 reopening note: Josef's fresh live playtest grants that smoke attraction is now working, but says the local scout/hold-off behavior still fails product expectations. The bandit gets far too close, sight-avoid is not credible enough on-screen, and the scout is not visibly timing out/returning home in the current save. The proof checkpoint below remains useful seam evidence, but it no longer closes the product packet by itself.

## Premise

Josef's 2026-04-26 live playtest surfaced two related problems in the current nearby-camp bandit behavior.

First, the previously discussed creepy local sight-avoid behavior is not implemented yet.  A stalking or holding-off bandit can be seen in the reality bubble and may simply stand there instead of slipping back behind cover.

Second, the current scout outing can linger too long.  The current live path can dispatch a single scout from a nearby owned bandit camp, but there is no honest finite sortie law that says `watch for a while, return home, report, then maybe come back with stronger pressure if the camp still cares`.

This packet promotes those two observations into the active local-stalking credibility slice now that the higher-priority live-wiring package has landed its bounded proof. It is concrete enough that implementation should not have to rediscover the problem from chat audio.

## Current code read

Current live footing is useful but incomplete:

- `steer_live_bandit_dispatch_toward_player()` checks nearby owned hostile sites on the shared 30-minute overmap cadence and can dispatch real owned NPCs toward the player.
- The nearby-camp player-pressure path usually wins a `scout` job, and `required_dispatch_members( scout ) == 1`, so one dispatched bandit is normal for the current v0 scenario.
- Camp-style sites keep a home reserve and block a second dispatch from the same site while that site already has an `outbound` or `local_contact` active group.
- `return_clock` is carried through the handoff/writeback memory, but it is not currently a live timeout that forces an in-bubble scout to stop watching after N hours or days.
- `resolve_active_group_aftermath()` resolves cleanly only when the active member is observed home, dead, or missing; `local_contact`, `returning_home`, and unresolved outbound states stay open.

So if a scout reaches local contact and remains active near the player/Basecamp for a long time, current behavior can feel stuck rather than like a finite scouting mission.

## Scope

Build one bounded local-stalking correction on top of the existing live-world/control substrate.

The slice should:

1. Add local sight-avoid behavior for stalking / hold-off bandits in the reality bubble.
2. Treat visible exposure as a local heuristic, not magic future knowledge:
   - if currently visible to the player or nearby camp NPCs, or recently exposed, prefer moving to a nearby tile that breaks line of sight or improves cover
   - do not teleport
   - do not predict future sight cones perfectly
   - do not make every bandit a master stealth tactician
3. Add a finite scout sortie law:
   - a scout can watch for a bounded time window
   - after that window, the scout should return to its owned site if still alive and not committed to a shakedown/fight
   - the camp can then re-evaluate what it learned rather than leaving the scout parked forever
4. Let follow-up pressure become stronger only through the existing owned-site/camp logic:
   - a returned scout may refresh marks, threat/bounty, anger/caution, or remembered pressure
   - any larger dispatch must be an explicit later decision, not an automatic spawn cheat
5. Keep the behavior reviewer-readable with logs/reports such as:
   - `sight_avoid: exposed -> repositioned`
   - `scout_sortie: linger limit reached -> return_home`
   - `scout_report: returned -> pressure refreshed`

## Non-goals

- no full stealth doctrine
- no magical perfect sight-cone omniscience
- no broad local combat AI rewrite
- no multi-camp strategy layer
- no guaranteed bigger attack after every scout
- no fake proof that a harness-only setup means natural world frequency is tuned
- no interruption of this active lane for adjacent bandit/local-AI rewrites

## Success state

This packet is good enough when:

1. Stalking / hold-off bandits in the reality bubble can detect current or recent exposure to the player or nearby camp NPCs and attempt a bounded reposition toward cover or broken line of sight.
2. The sight-avoid behavior is local and heuristic: deterministic tests prove it does not use magical future-cone omniscience and does not teleport.
3. A scout outing has a finite live sortie window and can return home after watching long enough, instead of lingering indefinitely in local contact.
4. Returned scout state writes back through the owned-site memory path and can drive later re-evaluation without automatically conjuring a larger group.
5. The single-scout current behavior remains explainable: `scout` is still one member unless a later job or escalated decision explicitly requires more.
6. Reviewer-readable output distinguishes `still stalking`, `repositioning because exposed`, `returning home`, and `re-dispatch/escalation decision`.
7. At least one live/harness proof uses `bandit.live_world_nearby_camp_mcw` or an equivalent real owned-site scenario and confirms the same code path would apply to a normal discovered bandit camp, while separately naming the harness bias that places the camp nearby on purpose.

## Validation expectations

Prefer a narrow deterministic layer first, then one live/handoff proof:

- deterministic tests for sight exposure classification and no-teleport/no-perfect-omniscience reposition choice
- deterministic tests for finite scout sortie expiry and return-home state transition
- deterministic or integration proof that return writeback refreshes pressure without auto-spawning a larger dispatch
- live handoff/probe proof on the current nearby-camp scenario showing a scout either scooches out of sight when exposed or returns after the sortie window
- a no-smoke control run remains useful when judging Josef's current smoke-attraction observation; do not claim smoke itself is the live cause without that control

## Current proof checkpoint - 2026-04-27

The first live/harness proof for this packet is a return-home decision proof. Scenario `bandit.local_scout_return_preaged_mcw` starts from real nearby-owned-site local-contact footing and uses only the narrow `bandit_active_sortie_clock` fixture transform to pre-age the scout sortie. Run `.userdata/dev-harness/harness_runs/20260427_051117/` records `scout_sortie: linger limit reached -> return_home` and `returning_home -> local_gate skipped` on the current runtime, with no matched shakedown/local-gate artifact for that returning scout.

The walked-home/writeback follow-through rung is now also live-proved. Run `.userdata/dev-harness/harness_runs/20260427_054353/` used the same copied nearby-owned-site footing on clean runtime `489e629073`, advanced a bounded 2600-turn travel window, and matched `scout_report: returned -> pressure refreshed ... remaining_pressure=ample`. Copied-save inspection shows the site active group/target/member ids and sortie clocks cleared, `remembered_pressure=ample`, and member `4` saved back on the home footprint with no remaining `omt_path`. Stable rerun scenario: `bandit.local_scout_return_followthrough_mcw`.

Final live proof rung: run `.userdata/dev-harness/harness_runs/20260427_061344/` via stable scenario `bandit.local_sight_avoid_exposed_mcw` starts from equivalent nearby-owned-site local-contact footing, moves only the player to an exposed south-of-shelter sightline, disables safe mode, advances 20 turns, and records `bandit_live_world sight_avoid: exposed -> repositioned npc=4 from=(60,23,0) to=(59,22,0) reason=repositioning because exposed`. This is now treated as a useful seam proof, not final product closure, because Josef's later live playtest contradicted the visible behavior.

## Reopened live-product correction - 2026-04-27

Current 2026-04-27 correction pass:

- Hold-off dispatch goal now uses `bandit_live_world::choose_hold_off_standoff_goal()` with a minimum five-OMT visible standoff instead of the previous two-OMT crowding goal. Deterministic coverage asserts straight and diagonal goals stay at/above `minimum_hold_off_standoff_omt()` and keep the no-movement case stable.
- The standoff scenario's expected artifact moved from `live_dispatch_goal=140,43,0` to `live_dispatch_goal=140,46,0`, matching the intended visible hold-off distance for the McWilliams/Basecamp footing.
- Recent copied-save inspection of `.userdata/dev-harness/save/McWilliams/dimension_data.gsav` after inflating the current player save shows the inspected site with `active_sortie_started_minutes=-1`, `active_sortie_local_contact_minutes=-1`, and no active group/target/job ids. That answers only the inspected fixture/save state: the scout timer was not running there.
- A fresh live probe attempt at `.userdata/dev-harness/harness_runs/20260427_145148/` loaded the game and captured only startup/normal-gameplay screens (`post_load_settle`, `disable_safe_mode`). It then hung in the long advancement path and was killed. Classify that run as **load proof only / inconclusive for standoff and scout-return behavior**, not as feature proof.

The next pass must answer the actual live playtest failure before making another closure claim:

1. Inspect Josef's recent live logs and save fields for `active_sortie_started_minutes`, `active_sortie_local_contact_minutes`, `return_home`, `local_contact`, and `sight_avoid` so the report says whether the scout timer is actually running.
2. Fix hold-off/stalking distance if confirmed: the final local/overmap goal must not routinely crowd the immediately adjacent / neighboring-OMT-scale area unless the behavior has escalated into an explicit shakedown or fight.
3. Prove the return-home path on the current product path, not only by pre-aging a harness clock. If a pre-aged fixture is used, label it as seam proof and keep the live-product condition open.
4. Any harness proof must include named screenshots/checkpoints for the intended action/result. Opening the game and closing it proves startup only.

## Notes for later implementation

The current harness scenario `bandit.live_world_nearby_camp_mcw` is not hollow: it seeds a real nearby `bandit_camp` and then exercises the current build's live owned-site dispatch/control path.  The caveat is frequency and setup: the harness guarantees a nearby site on useful Basecamp footing, while a normal world must naturally place/encounter a qualifying hostile site before the same behavior matters.
