# Andi handoff

Active lane: `CAOL-JOSEF-LIVE-DEBUG-BATCH-v0`.

Contract: `doc/josef-live-debug-batch-packet-v0-2026-05-03.md`.

Imagination source: `doc/josef-live-debug-batch-imagination-source-2026-05-03.md`.

Test matrix: `doc/josef-live-debug-batch-test-matrix-v0-2026-05-03.md`.

Handoff packet: `doc/josef-live-debug-batch-handoff-v0-2026-05-03.md`.

Raw intake:
- `/Users/josefhorvath/.openclaw/workspace/runtime/caol-bandit-playtest-intake-2026-05-02.md`
- `runtime/josef-bandit-debug-intake-2026-05-03.md`
- `runtime/josef-locker-zone-debug-intake-2026-05-03.md`

## Current ask

Execute the packaged 2026-05-02 + 2026-05-03 live debug correction stack in order. Start with shakedown UI/payment contract correction.

## Execution order

Current checkpoints: Slice 1 visible Pay/Fight + Pay opening the real NPC trade window is green at `doc/shakedown-pay-fight-npc-trade-ui-proof-v0-2026-05-03.md`. Slice 2 has enough current-pass evidence to continue: `doc/defended-camp-scout-standoff-hot-loot-proof-v0-2026-05-03.md` plus Josef's own test read that bandits did their thing. Do not block on deeper sight/smoke playtest now; continue at Slice 3 multi-z / roof-tower routing unless a fresh higher-priority debug note appears.

1. Shakedown Pay/Fight only + the actual NPC trade UI / trade window (`npc_trading::trade` / `trade_ui` shape) over the whole honest basecamp/faction-side inventory pool; it autostarts with demanded debt/toll derived from what that camp side can plausibly pay, and the player dumps items into the offer to satisfy it; not a fixed/stub amount, not player-carried-only inventory, and not a fake selector; remove/refute misleading `pay/fight/refuse` success expectations.
2. Defended-camp scout/standoff/sight-avoid/hot-loot behavior, including bandits/cannibals/compatible hostiles in stalking mode actually breaking LoS/backing off/rerouting/waiting/escalating when seen or when the player smokes out the suspected stalking tile/approach line.
3. Multi-z hostile camp identity + roof/tower-z routing/throttle fallback, including first-floor/roof and `z=5` tower-style player camps.
4. Hostile-camp post-scout escalation into roster-scaled pressure: bandit toll/shakedown, cannibal attack dispatch up to large/whole-camp commitment when odds permit.
5. All-light-source adapter for lamps/household/searchlight/fire/smoke with bandit/cannibal vs zombie interpretation split, plus fog/weather mismatch and stale `tracking=0` horde destination audit.
6. Camp patrol aggression/alarm plus patrol-report hygiene.
7. Writhing stalker bigger stalking distance, real sight avoidance, threat-drop fast swoop timing, and post-burst boot-out/recovery.
8. NPC sorting failure debounce.
9. Debug spawn horde/stalker/rider options: medium horde default, horde `5`/`10` OMT options, writhing stalker `5`/`10` OMT, zombie rider `5`/`10` OMT.
10. Locker/basecamp equipment consistency: no orphan ammo/mag carry after firearm replacement; broken/`XX` backpack replacement should transfer contents or log the concrete blocker.
11. Cannibal Monsterbone spear: add a rare ritual/status elite spear, 1-2 in camps and only a couple important cannibal wielders, carrying the huge-monster-bone / monster-meat madness lore.

## Evidence bar

Each slice needs deterministic coverage for the contract and live/path proof for player-facing or live-world claims. Do not close a live UI/behavior claim from a test seam alone.

After two same-blocker attempts, consult Frau Knackal before attempt 3. After four unresolved attempts, package implemented-but-unproven state for Josef and move to the next greenlit debug note.
