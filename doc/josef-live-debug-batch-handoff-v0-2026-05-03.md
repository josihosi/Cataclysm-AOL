# Andi handoff — CAOL-JOSEF-LIVE-DEBUG-BATCH-v0

Classification: ACTIVE / GREENLIT DEBUG-PACKET STACK.

Contract: `doc/josef-live-debug-batch-packet-v0-2026-05-03.md`.

Imagination source: `doc/josef-live-debug-batch-imagination-source-2026-05-03.md`.

Test matrix: `doc/josef-live-debug-batch-test-matrix-v0-2026-05-03.md`.

Raw intake:
- `/Users/josefhorvath/.openclaw/workspace/runtime/caol-bandit-playtest-intake-2026-05-02.md`
- `runtime/josef-bandit-debug-intake-2026-05-03.md`
- `runtime/josef-locker-zone-debug-intake-2026-05-03.md`

## Execute in this order

1. Shakedown UI/payment correction: Pay + Fight only; Pay opens trade/debt-style selector over the honest basecamp/faction pool; remove misleading `pay/fight/refuse` success expectations.
2. Defended-camp scout/standoff/sight-avoid/hot-loot: 5 OMT watch distance, sighted scouts plus stalking-mode bandits/cannibals/compatible hostiles actually break LoS/back off/reroute/escalate, no casual pickup on hot defended doorstep.
3. Multi-z camp identity + roof-z routing + throttle: one site across z-levels, reachable ground approach target, no `route_missing` -> 30-minute silence.
4. Hostile-camp escalation: confirmed scout + ample roster can promote next response; bandits to toll/shakedown party, cannibals to attack dispatch up to large/whole-camp commitment when odds say they can overpower the defenders.
5. All-light-source adapter: lamps/household/searchlight/fire all count when exposed/bright; zombies treat light broadly, bandits distinguish source semantics; audit fog/weather mismatch and stale `tracking=0` horde destinations.
6. Patrol aggression/alarm/log hygiene: hostile bandits/zombies attacked on sight, alarm roster can pull patrol-capable NPCs, routine patrol route messages leave visible log.
7. Writhing stalker distance/sight/threat-drop rhythm: bigger stalking distance, real sight avoidance, dropped threat quickly becomes approach/pounce/short strike, then it boots back out/recovers instead of idling exposed.
8. NPC sorting debounce: failed sorting attempts record cooldown/blocked state instead of per-turn retry/log spam.
9. Debug spawn options: `spawn horde` creates a medium horde; add horde `5`/`10` OMT options plus writhing stalker and zombie rider spawns at `5` and `10` OMT.
10. Locker/basecamp equipment consistency: no orphan ammo/mag carry after firearm replacement; broken/`XX` backpack replacement should transfer contents or log the concrete blocker.
11. Cannibal Monsterbone spear: add a rare ritual/status elite spear, 1-2 in camps and only a couple important cannibal wielders, carrying the huge-monster-bone / monster-meat madness lore.

## Non-goals

- No full diplomacy tree, hostage/reputation/economy system, or remote magical inventory.
- No full vertical assault/house-clearing/pathfinder rewrite.
- No every-camp dogpile or scout suicide rush.
- No overmap tile-perfect second light engine.
- No full military command simulation.
- No sorting redesign.

## Evidence bar

For each slice, use deterministic coverage for the contract and live/path proof for the player-facing/live claim. If proof fails after the normal attempt budget, consult Frau Knackal and then package implemented-but-unproven state for Josef rather than closing it.

Minimum expected proof families:
- shakedown: first-demand and reopened-demand dialogue + payment proof;
- hostile camps: defended camp, stalking-mode LoS avoidance, cannibal 5 OMT stalk-to-attack escalation, roof-z, multi-turn dispatch/retry evidence;
- light: fire/lamp/household/searchlight controls, weather-source sanity, and horde destination/tracking save inspection;
- patrol: hostile/neutral/alarm/log wait evidence;
- stalker: larger stalking distance, LoS avoidance, threat-drop pounce cadence, and post-burst boot-out;
- sorting: blocked retry debounce + recovery;
- debug spawns: option/menu coverage plus live/save inspection for medium horde size and requested `5`/`10` OMT horde/stalker/rider placement;
- locker equipment: deterministic compatibility/transfer tests plus narrow locker/basecamp fixture proof if required;
- Monsterbone spear: JSON validation, stat sanity, rare camp distribution, and selected important/elite cannibal wielder proof.
