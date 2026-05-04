# TESTING

Current validation policy and current evidence only.

This file is not a trophy wall.
Remove stale or completed fluff instead of stacking crossed-off test history forever.

## Validation policy

Use the **smallest evidence that honestly matches the change**.

- **Docs-only change** -> no compile
- **Small local code change** -> narrow build and the narrowest relevant test target
- **Broad or risky code change** -> broader rebuild and relevant filtered tests
- **Before a Josef handoff** or after suspicious stale-binary behavior -> rebuild the real targets and rerun the real smoke path

### Agent vs Josef

Schani should do agent-side playtesting first whenever the harness or direct in-game probing can answer the question.
Josef should be asked only for:
- product judgment
- tone/feel
- human-priority choices
- genuinely human-only interaction

Josef being unavailable is **not** a blocker by itself.
Josef self-testing is **not** a plan blocker and does not belong in the active success state as a gate.
If Josef-specific checks are useful, write them down as non-blocking notes so he can run them later from his own list.
If another good agent-side lane exists, keep moving there.
If several human-only judgments are likely soon, batch them instead of sending tiny separate asks.

### Anti-churn rule

Do not keep rerunning the same startup or test packet when it is no longer the missing evidence class.
If startup/load is already green, and the missing proof is live behavior, then the next probe must target live behavior.
If a target is merely waiting on Josef, do not keep revalidating it unless the code changed.

Current debug-stack attempt rule for the same item/blocker:
- attempts 1-2 may be Andi solo retries, including multiple focused harness attempts in one cron run when each attempt changes setup, instrumentation, or evidence class
- before attempt 3, consult Frau Knackal
- attempts 3-4 are the final changed retries after consultation
- after attempt 4, if code is implemented but proof still fails, add a concise implemented-but-unproven packet to Josef's playtest package and move to the next greenlit debug note; do not close it and do not park it as dead

### Test-to-game wiring rule

A test is not allowed to impersonate implementation. Before claiming gameplay behavior, identify the live code path that consumes the tested seam and name the evidence class that proves it: unit/evaluator, playback/proof packet, live source hook, harness/startup, screen, save inspection, or artifact/log. Deterministic-only packets may close only as deterministic-only packets; if the contract says the game does something, the proof must reach the game path or the claim stays open.

### Promotion / closure hygiene

Before promoting, closing, or handing off a lane, confirm that `TESTING.md` pending probes still match the active `Plan.md` lane. If the pending-probe text points at an older slice, fix it before Andi uses it as execution truth.

### Bandit overmap-proof rule

For the remaining bandit AI proof packets, single-turn deterministic checks are **not** enough by themselves.
The honest bar now includes real overmap-side multi-turn scenario proof, up to `500` turns where needed, with explicit per-scenario goals and tuning metrics.

## Current validation targets

### Active validation target - CAOL-JOSEF-LIVE-DEBUG-BATCH-v0

Repo canon now has active lane `CAOL-JOSEF-LIVE-DEBUG-BATCH-v0`. Contract: `doc/josef-live-debug-batch-packet-v0-2026-05-03.md`; imagination source: `doc/josef-live-debug-batch-imagination-source-2026-05-03.md`; scenario matrix: `doc/josef-live-debug-batch-test-matrix-v0-2026-05-03.md`; handoff: `doc/josef-live-debug-batch-handoff-v0-2026-05-03.md`.

Use slice-local evidence. Do not close a live UI/behavior claim from deterministic proof alone. Scenario matrix: `doc/josef-live-debug-batch-test-matrix-v0-2026-05-03.md`.

Required evidence families:
- **Shakedown UI/payment:** green checkpoint for the visible fork, real Pay surface, whole basecamp-side payment pool, pool-derived debt/toll basis, cancel/refusal mapping, item-offer/autobalance, and successful payment writeback: proof `doc/shakedown-pay-fight-npc-trade-ui-proof-v0-2026-05-03.md`. Josef's red live-options report is preserved as `.userdata/dev-harness/harness_runs/20260503_173812/`; Josef's later no-credit / Pay-does-not-open hard stop is treated as the reason the latest UI-first closure required rebuilt-current proof rather than trade-window-open folklore. Latest three-source row `.userdata/dev-harness/harness_runs/20260503_233823/` reports `evidence_class=feature-path`, `feature_proof=true`, `verdict=artifacts_matched`, `step_ledger_status=green_step_local_proof`, window `Cataclysm: Dark Days Ahead - eddfca4f10-dirty` with no runtime-relevant mismatch, immediate `shakedown_trade_ui opened demanded=33542 reachable=95834 player_pool=3711 nearby_npc_pool=2268 basecamp_npc_pool=3154 scene_pool=86701 basecamp_storage_pool=50000 trader=4 trade_api=npc_trading::trade title=Pay:`, same-window UI-trace markers for player `binoculars`, basecamp-assigned NPC `saxophone`, and basecamp storage-zone `gold_watch`, acceptance prompt `Accept this trade? (Case Sensitive)`, and paid artifacts `shakedown_trade_ui result=paid demanded=33542 reachable=95834` / `shakedown_surface paid toll=33542 demanded=33542 reachable=95834`. The rebuilt `7e5a506c76` master-profile rows remain the clean cancel/success receipts: cancel/refusal `.userdata/master/harness_runs/20260503_213831/` and successful Pay/save/writeback `.userdata/master/harness_runs/20260503_214015/`; earlier `d1a4f076c8` rows `20260503_202326`/`20260503_203141` and `04de6e0f94` Pay/cancel rows remain supporting history. Source/test checkpoint now logs/asserts `responses=pay/fight payment_surface=npc_trade_ui`, opens `npc_trading::trade` / `trade_ui` with `Pay:` debt, logs player/NPC/scene/basecamp-storage/basecamp-NPC pool components and derived demand values, maps cancel/shortfall to `fight`, and persists paid writeback. Current code extends the real trade pane, not a fake selector: basecamp scenes add basecamp storage via `inventory_selector::add_basecamp_items` and basecamp-assigned allied NPC carried items, while skipping duplicates already covered by nearby panes. Deterministic shakedown gate `/tmp/caol_shakedown_contract_tests_20260503.log` passed 5 cases / 162 assertions; full focused `[bandit][live_world]` gate `/tmp/caol_bandit_live_world_tests_20260503.log` passed 66 cases / 1365 assertions. The old fake selector / silent-confiscation path, stale `20260503_171632`/`171825`/`172007` rows, and loose no-credit playtest rows are no longer the credited product path.
- **Scout/standoff/hot-loot:** current-pass checkpoint `doc/defended-camp-scout-standoff-hot-loot-proof-v0-2026-05-03.md` covers deterministic/local-gate `5` OMT defended watch plus hot-doorstep pickup blocking and current-build live artifact `.userdata/dev-harness/harness_runs/20260503_181426/` for `posture=hold_off` / `live_dispatch_goal=140,46,0` (`standoff_distance=5`). Josef reported his own tests looked good enough that bandits did their thing, so Andi may continue to the next debug note instead of blocking on deeper sight/smoke playtest now. Deferred validation: live defended-base proof that sighted or smoke-revealed bandit/cannibal/compatible scouts/stalkers visibly flee, break LoS, reroute, wait, or escalate; current standoff scenario remains yellow overall because two startup/safe-mode screen checkpoints are OCR-caveated despite the green wait-action/artifact row.
- **Multi-z/roof-z dispatch:** green checkpoint at `doc/multi-z-roof-dispatch-fallback-proof-v0-2026-05-03.md`: deterministic `[multi_z]` site-identity/routing fallback tests plus live `bandit.roof_z_dispatch_fallback_mcw` run `.userdata/dev-harness/harness_runs/20260503_201355/` prove the multi-z camp stays one site, the elevated target id (`player@140,39,5`) is preserved, the live dispatch goal routes through reachable ground (`140,39,0`), and the old `route_missing` / empty-site `30_minute_throttle` silence is absent. Broader local/reality-bubble roof/stair entry remains outside this checkpoint.
- **Hostile-camp escalation:** green checkpoint `doc/hostile-camp-toll-escalation-proof-v0-2026-05-03.md` covers deterministic dispatch-size/escalation/risk gates plus live/staged bandit toll cadence `.userdata/dev-harness/harness_runs/20260503_214648/` with three-member `toll` party and `shakedown_capable=yes`; full live cannibal raid/contact remains outside this checkpoint unless later promoted.
- **All-light-source adapter:** green at `doc/all-light-source-live-adapter-proof-v0-2026-05-04.md`. Local live source scanning feeds fire/smoke plus terrain/furniture/item/vehicle light into the existing packet adapter; deterministic adapter coverage proves non-fire light packets, screened leakage, searchlight semantics, and horde-power gating. Gate run: `git diff --check`; `make -j4 tests LINTJSON=0 ASTYLE=0`; `./tests/cata_test "[bandit][marks]" --reporter compact` -> 19 cases / 250 assertions. Rebuilt live run `.userdata/dev-harness/harness_runs/20260504_092840/` via `bandit.all_light_source_live_adapter_mcw` logged `packets=5`, `smoke_packets=1`, `light_packets=4`, `raw_weather=clear`, `light_weather=clear`, four horde light-signal rows with `destination=`, `current_signal=yes`, `horde_signal_power=22`, guarded save/writeback, saved turn advance `5266800 -> 5267100`, and saved ground-z horde retarget-persistence to `destination_ms=[3360,984,0]`. Boundary: local active-zombie devirtualization and positive `tracking_intensity` are not claimed.
- **Patrol aggression/alarm/report hygiene:** green at `doc/camp-patrol-aggression-alarm-report-hygiene-proof-v0-2026-05-04.md`. Focused source/test coverage proves hostile-vs-neutral targeting, active shakedown/toll/parley contact non-escalation until `fight_unresolved`, zombie targeting, alarm-roster expansion, visible routine-report suppression, and harness parser compatibility for `alarm=` cache lines.
- **Writhing stalker distance/sight/threat-drop:** green at `doc/writhing-stalker-distance-sight-threat-drop-live-proof-v0-2026-05-04.md`. Deterministic/source coverage plus live `writhing_stalker.live_hit_fade_retreat_mcw` run `.userdata/dev-harness/harness_runs/20260504_113828/` prove larger `5` OMT far-watch writeback, sighted LoS-break/withdraw handling, pounce/short-strike timing, and post-burst withdraw/cooling-off. Boundary: natural discovery, broad house navigation, smoke-out playfeel, player-hunts-it feel, and high-z variants are not claimed.
- **NPC sorting debounce:** green at `doc/npc-sorting-failure-debounce-proof-v0-2026-05-04.md`. Source-path tests prove no-progress NPC sorting records an `ACT_MOVE_LOOT` cooldown, blocked jobs are not recreated before expiry, cooldowns serialize/expire, and ordinary sorting coverage remains green.
- **Debug spawn options:** green at `doc/debug-spawn-overmap-threat-options-proof-v0-2026-05-04.md`. Deterministic option/menu/destination/group coverage plus live/save inspection prove medium horde size and requested `5`/`10` OMT horde/stalker/rider placement/state.
- **Locker/basecamp equipment consistency:** green at `doc/locker-basecamp-equipment-consistency-proof-v0-2026-05-04.md`; basecamp fixture coverage proves orphan loose magazine/ammo cleanup after ranged-weapon replacement, compatible supply retention for retained firearms, carried-junk cleanup expectations, and damaged bag replacement with contents accounted for.
- **Monsterbone spear:** next active target; JSON validation, item stat sanity, rare camp item-group distribution check, and selected important/elite cannibal NPC weapon/loadout proof.

Do not rerun closed rows as ritual. Use old proof only as footing when the slice explicitly preserves it; if code changed under a live claim, rebuild/probe the relevant current path.

### Closed validation receipt - CAOL-WRITHING-STALKER-THREAT-DISTRACTION-HANDOFF-v0

`CAOL-WRITHING-STALKER-THREAT-DISTRACTION-HANDOFF-v0` is closed/checkpointed green v0 after Frau review. Contract: `doc/writhing-stalker-threat-distraction-handoff-packet-v0-2026-05-02.md`; imagination source: `doc/writhing-stalker-threat-distraction-handoff-imagination-source-2026-05-02.md`; handoff: `doc/writhing-stalker-threat-distraction-handoff-handoff-v0-2026-05-02.md`; proof: `doc/writhing-stalker-threat-distraction-live-staged-proof-v0-2026-05-03.md`.

Credited current-build staged/live rows: high-threat/allies retreat/stalk `.userdata/dev-harness/harness_runs/20260503_021310/`; zombie/distraction clean shadow-then-strike `.userdata/dev-harness/harness_runs/20260503_031247/`; night/outside anti-gnome strike `.userdata/dev-harness/harness_runs/20260503_025712/`. Gate at closure: `git diff --check`; `python3 -m py_compile tools/openclaw_harness/startup_harness.py`; scenario/fixture JSON validation; `make -j4 tests LINTJSON=0 ASTYLE=0 && ./tests/cata_test "[writhing_stalker]" --reporter compact` -> 23 cases / 264 assertions; spillover guard `./tests/cata_test "[zombie_rider],[flesh_raptor]" --reporter compact` -> 24 cases / 268 assertions.

Claim boundary: staged/live feature-path proof plus deterministic seam coverage only. No natural random discovery, full natural retreat pathing, broad house navigation, door opening, burglar/locked-door solving, or general ecosystem claim.

### Handoff-boundary validation receipt - CAOL-JOSEF-PLAYTEST-SAVE-PACK-v0

`CAOL-JOSEF-PLAYTEST-SAVE-PACK-v0` is at a handoff boundary, not an active Andi lane. Contract: `doc/caol-josef-playtest-save-pack-packet-v0-2026-05-02.md`; imagination source: `doc/caol-josef-playtest-save-pack-imagination-source-2026-05-02.md`; handoff: `doc/caol-josef-playtest-save-pack-handoff-v0-2026-05-02.md`; working card: `doc/caol-josef-playtest-save-pack-card-v0-2026-05-02.md`.

Card checkpoint: six ready staged rows are listed for Josef (camp locker weather/service, bandit first-demand contact, cannibal night pressure, flesh raptor crowded-arc skirmisher, zombie rider cover/wounded contrast, writhing stalker hit-fade/light/zombie-side pressure). Optional staged bandit contrast footing is ready and caveated in the card/doc ledger. All ready rows record current-build load/start-state footing and portal-storm status; no current validation target calls for rerunning them.

### Archived closed receipts

Detailed closed validation history has been trimmed out of this active testing file. For older closed lanes and non-credit proof, use `SUCCESS.md`, `Plan.md`, `doc/work-ledger.md`, linked `doc/*proof*` / packet files, and git history. This includes closed bandit signal adapter, portal-storm warning-light, zombie rider close-pressure, camp locker zone playtests, flesh raptor, stalker pattern/live/zombie-shadow/hit-fade, multi-camp, roof-horde, Smart Zone, fire, bandit, cannibal, and older basecamp/locker packets.

## Pending probes

Active pending probes are the slice-local proof families for `CAOL-JOSEF-LIVE-DEBUG-BATCH-v0`, in the order listed in `TODO.md` and the contract doc. Slice 1 payment, Slice 2 current-pass standoff, Slice 3 roof/tower-z dispatch fallback, Slice 4 bandit toll escalation, Slice 5 all-light-source adapter, Slice 6 patrol aggression/alarm/report hygiene, Slice 7 writhing-stalker timing, Slice 8 NPC sorting debounce, Slice 9 debug spawn options, and Slice 10 locker/basecamp equipment consistency now have credited checkpoints. Next active probe is Slice 11 Monsterbone spear: JSON validation, item stat sanity, rare camp item-group distribution check, and selected important/elite cannibal NPC weapon/loadout proof.

`CAOL-WRITHING-STALKER-THREAT-DISTRACTION-HANDOFF-v0` is closed/checkpointed green v0 after Frau review. The previously pending high-threat/allies, night/window/outside anti-gnome, and zombie/distraction staged/live rows are credited in the closed validation receipt above. Do not rerun them by ritual unless the new threat-drop slice touches the relevant behavior and names the changed evidence class.

Closed zombie-rider, flesh-raptor, writhing-stalker, roof-horde, Smart Zone, fire, bandit, and multi-camp proof trains are represented by `SUCCESS.md`, `Plan.md`, `doc/work-ledger.md`, linked aux proof docs, and git history. Do not rerun solved rows as ritual.

Future-only watchlist unless Schani/Josef explicitly promotes it:
- Natural three/four-site player-pressure behavior and true zero-site idle baseline remain decision/watchlist items, not current requirements.
- A stricter roof-horde positive `tracking_intensity` proof remains optional/future-only; current green roof proof is retarget/movement-budget metadata after live roof-fire horde signaling.
- Full Smart Zone process-reload disk persistence can be promoted later if Josef wants that stricter audit; the current green proof covers live create/inspect/close-save/reopen coordinate labels.

If a later live probe is promoted:
- build the current runtime first when binary freshness matters;
- use one named scenario/command path;
- extract only decisive report/log fields into context;
- after two same-blocker attempts, consult Frau Knackal before attempt 3; after four unresolved attempts, package implemented-but-unproven state for Josef and move to the next greenlit unblocked target.

---

## Reusable commands

Use these when they are actually the missing evidence, not as ritual.

### Narrow camp deterministic pass after a code slice lands
- `git diff --check`
- `make -j4 obj/basecamp.o tests/faction_camp_test.o tests LINTJSON=0 ASTYLE=0`
- focused `./tests/cata_test` filters for the touched camp/locker/patrol reporting path

### Plan status summary command
- `python3 tools/plan_status_summary.py --self-test`
- `python3 tools/plan_status_summary.py /plan`
- `python3 tools/plan_status_summary.py /plan active`
- `python3 tools/plan_status_summary.py /plan greenlit`
- `python3 tools/plan_status_summary.py /plan parked`

### Fresh tiles rebuild only if a later handoff really needs live proof
- `make -j4 TILES=1 cataclysm-tiles`

## Local build caveat

On this Mac, treat top-level `make -j4 tests` as the reliable path for a fresh `cata_test`.
Avoid treating `make -C tests cata_test` as authoritative here; it has been a repeated source of toolchain/stale-build nonsense.
Also: if you actually need a fresh tiles binary, use `make -j4 TILES=1 cataclysm-tiles`; plain `make cataclysm-tiles` is not an honest rebuild path here.
