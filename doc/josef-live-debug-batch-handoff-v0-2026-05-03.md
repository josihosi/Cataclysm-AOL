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

Current checkpoint: Slice 1 shakedown Pay/Fight + real NPC trade-window/payment path is green at `doc/shakedown-pay-fight-npc-trade-ui-proof-v0-2026-05-03.md` with latest rebuilt `7e5a506c76` master-profile rows `.userdata/master/harness_runs/20260503_213831/` and `.userdata/master/harness_runs/20260503_214015/`; older Pay/cancel rows are supporting branch evidence. Slice 2 defended-camp standoff/hot-doorstep guard has a current-pass partial checkpoint at `doc/defended-camp-scout-standoff-hot-loot-proof-v0-2026-05-03.md` with deeper sight/smoke behavior deferred; Slice 3 multi-z roof/tower dispatch fallback is green at `doc/multi-z-roof-dispatch-fallback-proof-v0-2026-05-03.md`; Slice 4 bandit toll escalation is green at `doc/hostile-camp-toll-escalation-proof-v0-2026-05-03.md` via live/staged run `.userdata/dev-harness/harness_runs/20260503_214648/`; Slice 5 all-light-source adapter is green at `doc/all-light-source-live-adapter-proof-v0-2026-05-04.md`; Slice 6 camp patrol aggression/alarm/report hygiene is green at `doc/camp-patrol-aggression-alarm-report-hygiene-proof-v0-2026-05-04.md`. Next execution work should start at Slice 7 writhing stalker distance/sight/threat-drop rhythm unless a higher-priority debug note appears.

1. Shakedown UI/payment correction: Pay + Fight only; Pay opens the actual NPC trade UI / trade window (`npc_trading::trade` / `trade_ui` shape) over the whole honest basecamp/faction inventory pool; it autostarts with demanded debt/toll derived from what that camp side has / can plausibly pay, and the player dumps items into the offer to satisfy it; remove misleading `pay/fight/refuse` success expectations.
2. Defended-camp scout/standoff/sight-avoid/hot-loot: 5 OMT watch distance, sighted or player-smoked-out scouts plus stalking-mode bandits/cannibals/compatible hostiles actually break LoS/back off/reroute/wait/escalate, no casual pickup on hot defended doorstep and no smoke-camping the same tile.
3. Multi-z camp identity + roof/tower-z routing + throttle: one site across z-levels, reachable ground approach target for first-floor/roof/`z=5` tower cases, no `route_missing` -> 30-minute silence.
4. Hostile-camp escalation: checkpoint green for confirmed-scout bandit toll dispatch plus deterministic cannibal/risk sizing; broader live cannibal raid/contact remains unclaimed unless promoted.
5. All-light-source adapter: lamps/household/searchlight/fire/smoke all count when exposed/bright/visible; zombies treat light/smoke broadly, bandits/cannibals distinguish source semantics; audit fog/weather mismatch and stale `tracking=0` horde destinations.
6. Patrol aggression/alarm/log hygiene: green at `doc/camp-patrol-aggression-alarm-report-hygiene-proof-v0-2026-05-04.md`; hostile bandits/zombies attacked on sight, alarm roster can pull patrol-capable NPCs, routine patrol route messages stay out of the visible log.
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
- shakedown: first-demand and reopened-demand dialogue + payment proof showing the actual NPC trade window, initial debt/toll, item-dump offer flow, whole basecamp-side inventory pool, and debt/toll derived from camp-side value;
- hostile camps: defended camp, stalking-mode LoS avoidance, player smoke-out of bandit/cannibal watcher locations, cannibal 5 OMT stalk-to-attack escalation, roof/first-floor/`z=5` tower fallback, multi-turn dispatch/retry evidence;
- light/smoke: fire/lamp/household/searchlight/smoke controls, weather-source sanity, and horde destination/tracking save inspection;
- patrol: hostile/neutral/alarm/log wait evidence;
- stalker: larger stalking distance, LoS avoidance, threat-drop pounce cadence, and post-burst boot-out;
- sorting: blocked retry debounce + recovery;
- debug spawns: option/menu coverage plus live/save inspection for medium horde size and requested `5`/`10` OMT horde/stalker/rider placement;
- locker equipment: deterministic compatibility/transfer tests plus narrow locker/basecamp fixture proof if required;
- Monsterbone spear: JSON validation, stat sanity, rare camp distribution, and selected important/elite cannibal wielder proof.
