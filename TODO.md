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

Current execution item: Slice 5, **All-light-source live adapter and horde/bandit interpretation**. Slice 1 visible fork + real NPC trade-window opening + whole basecamp-side payment pool + pool-derived debt/toll basis is green at `doc/shakedown-pay-fight-npc-trade-ui-proof-v0-2026-05-03.md`; the latest three-source row `.userdata/dev-harness/harness_runs/20260503_233823/` proves the same open Pay trade window contains player `binoculars`, basecamp-assigned NPC `saxophone`, and basecamp storage-zone `gold_watch`, then accepts a balanced offer and records `shakedown_trade_ui result=paid demanded=33542 reachable=95834`. The rebuilt `7e5a506c76` master-profile pair remains the clean cancel/success receipt: `.userdata/master/harness_runs/20260503_213831/` and `.userdata/master/harness_runs/20260503_214015/`; earlier `d1a4f076c8` rows are supporting history. Slice 2 has a current-pass checkpoint at `doc/defended-camp-scout-standoff-hot-loot-proof-v0-2026-05-03.md`: deterministic/local-gate + live artifact evidence covers the `5` OMT hold-off goal and hot-doorstep pickup guard, and Josef reported his own tests looked good enough that bandits did their thing. Slice 3 roof/tower-z fallback is green at `doc/multi-z-roof-dispatch-fallback-proof-v0-2026-05-03.md`: deterministic multi-z identity/routing plus live `bandit.roof_z_dispatch_fallback_mcw` proof show the elevated target id is preserved while the live dispatch goal routes to reachable ground and avoids the old `route_missing` / empty-throttle silence. Slice 4 bandit toll escalation is green at `doc/hostile-camp-toll-escalation-proof-v0-2026-05-03.md`: deterministic roster/risk/cannibal gates plus live `bandit.hostile_camp_toll_escalation_live` run `.userdata/dev-harness/harness_runs/20260503_214648/` prove a scout-confirmed seven-member bandit camp dispatches a three-member `toll` party, preserves reserve, and records `shakedown_capable=yes` while pre-contact `shakedown=no`. Continue down the debug stack; do not spend the next Andi cycle chasing deferred Slice 2 sight/smoke playtest, broader vertical local-pathfinding, or full cannibal live raid/contact unless canon is updated.

Execution order:
1. Shakedown visible Pay/Fight only; Pay opens the actual NPC trade UI / trade window (`npc_trading::trade` / `trade_ui` shape), autostarts with a demanded debt/toll, lets the player dump items into the offer, uses the whole honest basecamp/faction-side inventory pool, and derives debt/toll from what that camp side has rather than a fixed/stub amount or player-carried-only value; correct misleading `pay/fight/refuse` docs/tests/log expectations.
2. Defended-camp scout/standoff/sight-avoid/hot-loot: about `5` OMT watch distance, sighted or player-smoked-out scouts plus stalking-mode bandits/cannibals/compatible hostiles actually break LoS/back off/reroute/wait/escalate, no casual looting on hot defended doorstep.
3. Multi-z camp identity + roof-z routing/throttle: one site across z-levels, reachable ground approach target, no `route_missing` -> 30-minute silence.
4. Hostile-camp escalation after scout confirmation: checkpoint green for scout-confirmed bandit toll party + deterministic cannibal/risk sizing; broader live cannibal raid/contact remains outside the credited checkpoint unless promoted.
5. All-light-source adapter: initial code/test checkpoint landed for fire/smoke plus terrain/furniture/item/vehicle light observations, screened side leakage, searchlight semantics, exposed-light horde pressure, raw weather logging, and current horde signal metadata. Next: prove the live path with lamp/household/fire/smoke controls and inspect horde destination/tracking state before closing.
6. Patrol aggression/alarm/report hygiene: attack hostile bandits/zombies on sight, avoid neutral false positives, alarm patrol-capable camp NPCs, suppress routine visible patrol spam.
7. Writhing stalker distance/sight/threat-drop rhythm: bigger stalking distance, real sight avoidance, dropped threat quickly becomes approach/pounce/short strike, then it boots back out/recovers instead of idling exposed.
8. NPC sorting debounce: failed sorting attempts record cooldown/blocked state instead of per-turn retry/log spam.
9. Debug spawn options: `spawn horde` creates a medium horde; add horde `5`/`10` OMT options plus writhing stalker and zombie rider spawns at `5` and `10` OMT.
10. Locker/basecamp equipment consistency: no orphan ammo/mag carry after firearm replacement; broken/`XX` backpack replacement should transfer contents or log the concrete blocker.
11. Cannibal Monsterbone spear: add a rare ritual/status elite spear, 1-2 in camps and only a couple important cannibal wielders, carrying the huge-monster-bone / monster-meat madness lore.

Do not reopen the save-pack handoff, closed predator/rider/locker lanes, release packaging, full diplomacy, full vertical assault AI, broad sorting redesign, or a tile-perfect overmap light engine by drift.
