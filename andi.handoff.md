# Andi handoff

Active lane: `CAOL-JOSEF-LIVE-DEBUG-BATCH-v0`.

Contract: `doc/josef-live-debug-batch-packet-v0-2026-05-03.md`.

Imagination source: `doc/josef-live-debug-batch-imagination-source-2026-05-03.md`.

Test matrix: `doc/josef-live-debug-batch-test-matrix-v0-2026-05-03.md`.

Standing build cadence: `doc/andi-build-cadence-note.md`.

Handoff packet: `doc/josef-live-debug-batch-handoff-v0-2026-05-03.md`.

Raw intake:
- `/Users/josefhorvath/.openclaw/workspace/runtime/caol-bandit-playtest-intake-2026-05-02.md`
- `runtime/josef-bandit-debug-intake-2026-05-03.md`
- `runtime/josef-locker-zone-debug-intake-2026-05-03.md`

## Current ask

Execute the packaged 2026-05-02 + 2026-05-03 live debug correction stack in order. Continue at Slice 9 debug spawn options: medium horde default, horde `5`/`10` OMT options, writhing stalker `5`/`10` OMT, and zombie rider `5`/`10` OMT with clear labels and honest spawn/location proof. Slices 1-8 have checkpoint proof.

## Execution order

Current checkpoints: Slice 1 visible Pay/Fight + Pay opening the real NPC trade window is green at `doc/shakedown-pay-fight-npc-trade-ui-proof-v0-2026-05-03.md`. Latest three-source row `.userdata/dev-harness/harness_runs/20260503_233823/` proves the same open Pay trade window contains player `binoculars`, basecamp-assigned NPC `saxophone`, and basecamp storage-zone `gold_watch`, then accepts a balanced offer and records paid writeback. The rebuilt `7e5a506c76` master rows remain the clean cancel/success receipts. Slice 2 has enough current-pass evidence to continue: `doc/defended-camp-scout-standoff-hot-loot-proof-v0-2026-05-03.md` plus Josef's own test read that bandits did their thing. Slice 3 multi-z / roof-tower routing is green at `doc/multi-z-roof-dispatch-fallback-proof-v0-2026-05-03.md`. Slice 4 bandit toll escalation is green at `doc/hostile-camp-toll-escalation-proof-v0-2026-05-03.md`. Slice 5 all-light-source adapter is green at `doc/all-light-source-live-adapter-proof-v0-2026-05-04.md` via `.userdata/dev-harness/harness_runs/20260504_092840/`: source/weather/log adapter proof, guarded save/writeback, saved turn advance, and saved ground-z horde retarget-persistence are green; local active-zombie devirtualization and positive `tracking_intensity` are not claimed. Slice 6 patrol aggression/alarm/report hygiene is green at `doc/camp-patrol-aggression-alarm-report-hygiene-proof-v0-2026-05-04.md`, including hostile/neutral targeting, shakedown/toll/parley non-escalation until `fight_unresolved`, alarm roster expansion, and routine visible-report suppression. Slice 7 writhing stalker distance/sight/threat-drop is green at `doc/writhing-stalker-distance-sight-threat-drop-live-proof-v0-2026-05-04.md` via live run `.userdata/dev-harness/harness_runs/20260504_113828/`: `5` OMT far-watch writeback, sighted LoS-break/withdraw handling, pounce/short-strike timing, and post-burst withdraw/cooling-off are credited; natural discovery, broad house navigation, smoke-out playfeel, player-hunts-it feel, and high-z variants are not claimed. Slice 8 NPC sorting debounce is green at `doc/npc-sorting-failure-debounce-proof-v0-2026-05-04.md`: no-progress NPC sorting blocks `ACT_MOVE_LOOT`, job scans skip it until cooldown expiry, cooldowns serialize/expire, and ordinary sorting remains covered. Do not repeat closed proof rows now; continue with Slice 9 debug spawn options unless a fresh higher-priority debug note appears.

1. Shakedown Pay/Fight only + the actual NPC trade UI / trade window (`npc_trading::trade` / `trade_ui` shape) over the whole honest basecamp/faction-side inventory pool; it autostarts with demanded debt/toll derived from what that camp side can plausibly pay, and the player dumps items into the offer to satisfy it; not a fixed/stub amount, not player-carried-only inventory, and not a fake selector; remove/refute misleading `pay/fight/refuse` success expectations.
2. Defended-camp scout/standoff/sight-avoid/hot-loot behavior, including bandits/cannibals/compatible hostiles in stalking mode actually breaking LoS/backing off/rerouting/waiting/escalating when seen or when the player smokes out the suspected stalking tile/approach line.
3. Multi-z hostile camp identity + roof/tower-z routing/throttle fallback, including first-floor/roof and `z=5` tower-style player camps.
4. Hostile-camp post-scout escalation into roster-scaled pressure: bandit toll/shakedown, cannibal attack dispatch up to large/whole-camp commitment when odds permit.
5. All-light-source adapter for lamps/household/searchlight/fire/smoke with bandit/cannibal vs zombie interpretation split, plus fog/weather mismatch and stale `tracking=0` horde destination audit.
6. Camp patrol aggression/alarm plus patrol-report hygiene.
7. Writhing stalker bigger stalking distance, real sight avoidance, threat-drop fast swoop timing, and post-burst boot-out/recovery — green at `doc/writhing-stalker-distance-sight-threat-drop-live-proof-v0-2026-05-04.md`.
8. NPC sorting failure debounce — green at `doc/npc-sorting-failure-debounce-proof-v0-2026-05-04.md`.
9. Debug spawn horde/stalker/rider options: medium horde default, horde `5`/`10` OMT options, writhing stalker `5`/`10` OMT, zombie rider `5`/`10` OMT.
10. Locker/basecamp equipment consistency: no orphan ammo/mag carry after firearm replacement; broken/`XX` backpack replacement should transfer contents or log the concrete blocker.
11. Cannibal Monsterbone spear: add a rare ritual/status elite spear, 1-2 in camps and only a couple important cannibal wielders, carrying the huge-monster-bone / monster-meat madness lore.

## Evidence bar

Each slice needs deterministic coverage for the contract and live/path proof for player-facing or live-world claims. Do not close a live UI/behavior claim from a test seam alone.

Use `doc/andi-build-cadence-note.md` when choosing build scope. Build when the next claim depends on current compiled code; avoid ritual clean rebuilds unless the note's clean-rebuild triggers are actually met.

After two same-blocker attempts, consult Frau Knackal before attempt 3. After four unresolved attempts, package implemented-but-unproven state for Josef and move to the next greenlit debug note.
