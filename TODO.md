# TODO

Short execution queue only.

Remove finished items when they are done.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active lane: `CAOL-JOSEF-LIVE-DEBUG-BATCH-v0`.

Contract: `doc/josef-live-debug-batch-packet-v0-2026-05-03.md`.

Imagination source: `doc/josef-live-debug-batch-imagination-source-2026-05-03.md`.

Test matrix: `doc/josef-live-debug-batch-test-matrix-v0-2026-05-03.md`.

Handoff packet: `doc/josef-live-debug-batch-handoff-v0-2026-05-03.md`.

Raw intake:
- `/Users/josefhorvath/.openclaw/workspace/runtime/caol-bandit-playtest-intake-2026-05-02.md`
- `runtime/josef-bandit-debug-intake-2026-05-03.md`
- `runtime/josef-locker-zone-debug-intake-2026-05-03.md`

Current execution item: Slice 3, **Multi-z camp identity + roof/tower-z routing/throttle fallback**. Slice 1 visible fork + real NPC trade-window opening is green at `doc/shakedown-pay-fight-npc-trade-ui-proof-v0-2026-05-03.md`. Slice 2 has a current-pass checkpoint at `doc/defended-camp-scout-standoff-hot-loot-proof-v0-2026-05-03.md`: deterministic/local-gate + live artifact evidence covers the `5` OMT hold-off goal and hot-doorstep pickup guard, and Josef reported his own tests looked good enough that bandits did their thing. Do not spend the next Andi cycle chasing the deeper sight/smoke playtest; keep that as deferred validation/future hardening and continue down the debug stack.

Execution order:
1. Shakedown visible Pay/Fight only; Pay opens the actual NPC trade UI / trade window (`npc_trading::trade` / `trade_ui` shape), autostarts with a demanded debt/toll, lets the player dump items into the offer, uses the whole honest basecamp/faction-side inventory pool, and derives debt/toll from what that camp side has rather than a fixed/stub amount or player-carried-only value; correct misleading `pay/fight/refuse` docs/tests/log expectations.
2. Defended-camp scout/standoff/sight-avoid/hot-loot: about `5` OMT watch distance, sighted or player-smoked-out scouts plus stalking-mode bandits/cannibals/compatible hostiles actually break LoS/back off/reroute/wait/escalate, no casual looting on hot defended doorstep.
3. Multi-z camp identity + roof-z routing/throttle: one site across z-levels, reachable ground approach target, no `route_missing` -> 30-minute silence.
4. Hostile-camp escalation after scout confirmation: ample roster can promote next pressure; bandits into toll/shakedown party, cannibals into attack dispatch up to large/whole-camp commitment when they can overpower defenders.
5. All-light-source adapter: lamps/household/searchlights/fire can feed bounded live light signals; zombies treat light broadly, bandits distinguish light semantics; audit fog/weather mismatch and stale `tracking=0` horde destinations.
6. Patrol aggression/alarm/report hygiene: attack hostile bandits/zombies on sight, avoid neutral false positives, alarm patrol-capable camp NPCs, suppress routine visible patrol spam.
7. Writhing stalker distance/sight/threat-drop rhythm: bigger stalking distance, real sight avoidance, dropped threat quickly becomes approach/pounce/short strike, then it boots back out/recovers instead of idling exposed.
8. NPC sorting debounce: failed sorting attempts record cooldown/blocked state instead of per-turn retry/log spam.
9. Debug spawn options: `spawn horde` creates a medium horde; add horde `5`/`10` OMT options plus writhing stalker and zombie rider spawns at `5` and `10` OMT.
10. Locker/basecamp equipment consistency: no orphan ammo/mag carry after firearm replacement; broken/`XX` backpack replacement should transfer contents or log the concrete blocker.
11. Cannibal Monsterbone spear: add a rare ritual/status elite spear, 1-2 in camps and only a couple important cannibal wielders, carrying the huge-monster-bone / monster-meat madness lore.

Do not reopen the save-pack handoff, closed predator/rider/locker lanes, release packaging, full diplomacy, full vertical assault AI, broad sorting redesign, or a tile-perfect overmap light engine by drift.
