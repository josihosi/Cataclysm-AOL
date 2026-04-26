# Bandit local sight-avoid + scout return cadence packet v0 (2026-04-26)

Status: greenlit / queued follow-up.

## Premise

Josef's 2026-04-26 live playtest surfaced two related problems in the current nearby-camp bandit behavior.

First, the previously discussed creepy local sight-avoid behavior is not implemented yet.  A stalking or holding-off bandit can be seen in the reality bubble and may simply stand there instead of slipping back behind cover.

Second, the current scout outing can linger too long.  The current live path can dispatch a single scout from a nearby owned bandit camp, but there is no honest finite sortie law that says `watch for a while, return home, report, then maybe come back with stronger pressure if the camp still cares`.

This packet promotes those two observations into one greenlit queued local-stalking credibility slice. It should wait behind the active audit and higher-priority live-wiring packages unless Schani/Josef explicitly reorders the stack, but it is concrete enough that later work should not have to rediscover the problem from chat audio.

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
- no interruption of the current active lane until this packet is explicitly greenlit

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

## Notes for later implementation

The current harness scenario `bandit.live_world_nearby_camp_mcw` is not hollow: it seeds a real nearby `bandit_camp` and then exercises the current build's live owned-site dispatch/control path.  The caveat is frequency and setup: the harness guarantees a nearby site on useful Basecamp footing, while a normal world must naturally place/encounter a qualifying hostile site before the same behavior matters.
