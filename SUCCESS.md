# SUCCESS

Exit criteria ledger for roadmap items.

Use this file so completion is explicit instead of vibes-based.

## Rules

- Every real roadmap item in `Plan.md` should have a matching success state here (or an equally explicit inline auxiliary).
- When a success-state condition is reached, **cross it off here instead of deleting it immediately**.
- When all conditions for a roadmap item are crossed off, that roadmap item is **DONE** and can be removed from `Plan.md`.
- Josef self-testing is **never** a blocker in this file.
- If Josef-specific tests or checks are useful, write them down as non-blocking notes so Josef can do them later from his own testing list.
- This file is an exit-criteria ledger, not a state-transition log or changelog.
- `doc/work-ledger.md` carries compact receipts for raw asks, state changes, supersessions, evidence links, and next owner.
- Older closed sections may be compacted: retain success-state bullets, final status, and canonical aux/proof references, but move transition/closure narrative to `doc/work-ledger.md`, `doc/*.md`, and git history.

---

## CAOL-JOSEF-LIVE-DEBUG-BATCH-v0 — Josef live debug batch

Status: ACTIVE / GREENLIT DEBUG-PACKET STACK

Success state:
- [x] Shakedown visible UI is exactly Pay/Fight; backout/refuse maps to the fight/refusal branch without a third visible `Refuse` response. _Latest credited UI-first rows after Josef's hard stop: rebuilt `7e5a506c76` master-profile runs `.userdata/master/harness_runs/20260503_213831/` and `.userdata/master/harness_runs/20260503_214015/`, both with `visible_responses=pay/fight`, green step-local proof, clean current window title, and no version mismatch; stale `20260503_171632`/`171825`/`172007` and loose no-credit rows are not closure proof._
- [x] Shakedown Pay opens the actual NPC trade UI / trade window, autostarted with demanded debt/toll, where the player can dump items into the offer from the whole honest basecamp/faction-side inventory pool instead of silent auto-confiscation, fake selector, fixed/stub debt, or player-carried-only payment. _Implemented via `npc_trading::trade` / `trade_ui`; proof `doc/shakedown-pay-fight-npc-trade-ui-proof-v0-2026-05-03.md`; latest three-source row `.userdata/dev-harness/harness_runs/20260503_233823/` shows immediate post-Pay `trade_api=npc_trading::trade title=Pay:`, pool `player_pool=3711 nearby_npc_pool=2268 basecamp_npc_pool=3154 scene_pool=86701 basecamp_storage_pool=50000 reachable=95834`, player/basecamp-NPC/storage marker traces, F1/autobalance offer, accept prompt, and successful paid writeback; `7e5a506c76` rows remain the clean master cancel/success receipts._
- [x] Basecamp-side shakedown payment can use the honest faction/basecamp pool, and the debt/toll basis depends on that pool rather than a fixed/stub or player-carried-only value. _Current source extends the normal trade pane with basecamp storage goods and basecamp-assigned allied NPC carried goods in addition to carried/nearby/ally pools; current three-source row logs `player_pool=3711 nearby_npc_pool=2268 basecamp_npc_pool=3154 scene_pool=86701 basecamp_storage_pool=50000 reachable=95834`, with demanded toll `33542` derived from that reachable pool. The older clean master rows still cover first/reopened values (`15797` / `22116`) and cancel/save writeback. Deterministic gate `/tmp/caol_shakedown_contract_tests_20260503.log` passed 5 shakedown cases / 162 assertions; full focused gate `/tmp/caol_bandit_live_world_tests_20260503.log` passed 66 cases / 1365 assertions._
- [x] Misleading scenic-shakedown `pay/fight/refuse` proof/docs/log expectations are corrected so future work cannot cite them as final payment-contract proof: source log now emits `responses=pay/fight payment_surface=npc_trade_ui`, the report/test assert `visible_responses=pay/fight` and no `pay/fight/refuse`, and historical proof docs carry superseded caveats.
- [x] Defended-camp bandit current-pass behavior is accepted enough to continue the debug stack: deterministic/local-gate proof covers `5` OMT hold-off and hot-doorstep pickup blocking, live artifact `20260503_181426` shows `posture=hold_off` / `standoff_distance=5`, and Josef reported his own test bandits did their thing. Deep sight/smoke playtest remains deferred validation, not the next blocker.
- [ ] Defended-camp scout/hold-off behavior uses about `5` OMT watch distance; sighted or player-smoked-out scouts and bandits/cannibals/compatible hostiles in stalking mode actually break LoS/back off/reroute/wait/escalate instead of staying visible or smoke-camping the same tile; hot doorstep actors do not loiter or loot casually.
- [x] Multi-z hostile camps collapse into one site/owner with z-level footprint metadata, and first-floor/roof/`z=5` tower player targets no longer cause duplicate sites, impossible-z beelines, or route-missing/throttle silence. _Proof: `doc/multi-z-roof-dispatch-fallback-proof-v0-2026-05-03.md`._
- [x] A confirmed big camp with ample roster can escalate from lone scout into roster-scaled pressure while preserving reserve and high-threat caution: bandits to toll/shakedown, cannibals to attack dispatch up to large/whole-camp commitment when odds permit. _Green checkpoint: `doc/hostile-camp-toll-escalation-proof-v0-2026-05-03.md` proves live/staged scout-confirmed bandit toll dispatch (`.userdata/dev-harness/harness_runs/20260503_214648/`) plus deterministic cannibal attack-pack/risk/reserve gates. Full live cannibal raid/contact remains outside this checkpoint._
- [ ] Meaningful light sources beyond `fd_fire`/`fd_smoke` — lamps, household/window light, searchlights, bright appliances, fires — can feed bounded live light signals when brightness/weather/time/exposure justify it, with fog/weather mismatch and stale `tracking=0` horde destinations handled or explained.
- [ ] Zombies/hordes receive broad exposed-light/smoke attraction; bandits/cannibals distinguish searchlight/threat light and smoke/concealment from household/fire occupancy evidence.
- [ ] Camp patrols attack hostile bandits/zombies on sight, avoid neutral false positives, and hostile sighting can alarm patrol-capable camp NPCs independent of shift.
- [ ] Routine patrol assignment/route reports no longer spill into visible in-game messages during waits.
- [ ] Writhing stalker high-threat caution is preserved as larger-distance stalking plus real sight avoidance; threat-drop windows quickly produce approach/pounce/short strike pressure, then the stalker boots back out/recovers when threat rises or the burst is spent.
- [ ] NPC sorting failures are debounced so impossible/failing sort jobs do not immediately retry/log every turn, while successful sorting and recovery after state change still work.
- [ ] Debug spawn options can create a medium horde, horde-at-`5`/`10` OMT setups, writhing stalker at `5`/`10` OMT, and zombie rider at `5`/`10` OMT, with clear labels and honest spawn/location proof.
- [ ] Locker/basecamp equipment cleanup prevents orphan ammo/magazine carry after firearm replacement and can replace broken/`XX` backpacks by transferring stored contents or logging a concrete transfer blocker.
- [ ] Cannibal camps and selected important cannibal NPC loadouts can surface a rare ritual/status `Monsterbone spear` that reads as monster-meat/huge-bone lore, with only about `1` or `2` camp copies and bounded elite wielder frequency.
- [ ] Each live UI/behavior claim has proof that reaches the actual player-facing surface, live dispatch/local-gate path, live signal adapter, monster plan path, patrol path, or NPC activity/job loop it names.

Canonical docs:
- Imagination source: `doc/josef-live-debug-batch-imagination-source-2026-05-03.md`.
- Contract: `doc/josef-live-debug-batch-packet-v0-2026-05-03.md`.
- Test matrix: `doc/josef-live-debug-batch-test-matrix-v0-2026-05-03.md`.
- Handoff: `doc/josef-live-debug-batch-handoff-v0-2026-05-03.md`.
- Raw 2026-05-02 intake: `/Users/josefhorvath/.openclaw/workspace/runtime/caol-bandit-playtest-intake-2026-05-02.md`.
- Raw 2026-05-03 bandit intake: `runtime/josef-bandit-debug-intake-2026-05-03.md`.
- Raw 2026-05-03 locker/basecamp intake: `runtime/josef-locker-zone-debug-intake-2026-05-03.md`.

---

## CAOL-WRITHING-STALKER-THREAT-DISTRACTION-HANDOFF-v0 — Writhing stalker threat/distraction handoff

Status: CLOSED / CHECKPOINTED GREEN V0 / FRAU-ACCEPTED

Success state:
- [x] Day/bright + same OMT or close overmap pressure + about three friendly NPCs produces retreat/stalking behavior instead of close loiter/pressure: `.userdata/dev-harness/harness_runs/20260503_021310/`.
- [x] Stalking/retreat behavior records or proves about `3` OMT distance where overmap pathing permits, and avoids direct sight tiles rather than hovering at the defended house: `.userdata/dev-harness/harness_runs/20260503_021310/` reports `stalk_omt=3`.
- [x] Night/outside/reachable player no longer produces indefinite non-action: `.userdata/dev-harness/harness_runs/20260503_025712/` reports `decision=strike`, `reason=live_anti_gnome_night_reachable_probe_strike`, `anti_gnome=yes`, `bad_loiter=2`.
- [x] Zombie/distraction entering the player/NPC tile raises opportunity and produces dark-square approach/strike behavior without omniscience: `.userdata/dev-harness/harness_runs/20260503_031247/` reports cover-shadow then strike with `reason=live_vulnerability_window_strike` and `zombie_pressure=2`.
- [x] Overmap-to-bubble handoff carries intent/reason/strike-budget state, and bubble-to-overmap writeback preserves spent/retreat/cooldown/threat memory: deterministic checkpoint `doc/writhing-stalker-threat-distraction-deterministic-checkpoint-v0-2026-05-03.md`.
- [x] Existing stalker guarantees still pass: no omniscience, light/focus counterplay, injured retreat, cooldown anti-spam, bounded burst/fade, and zombie-shadow/quiet-side behavior; `[writhing_stalker]` remains green at 23 cases / 264 assertions.
- [x] Door opening was not implemented for this packet; the optional narrow door-opening line remains out of scope unless later promoted.
- [x] `Plan.md`, `TODO.md`, `SUCCESS.md`, `TESTING.md`, `doc/work-ledger.md`, and `andi.handoff.md` agree on closed / Frau-accepted state before closure.

Checkpoint note: deterministic evaluator/live-plan seam coverage is green and current-build staged/live high-threat, night/outside anti-gnome, and zombie/distraction rows are green. Frau accepted the packet as closure-ready agent-side staged/live feature-path evidence; keep claims bounded away from natural random discovery, full natural retreat pathing, broad house navigation, and door/burglar behavior.

Canonical docs:
- Imagination source: `doc/writhing-stalker-threat-distraction-handoff-imagination-source-2026-05-02.md`.
- Contract: `doc/writhing-stalker-threat-distraction-handoff-packet-v0-2026-05-02.md`.
- Handoff: `doc/writhing-stalker-threat-distraction-handoff-handoff-v0-2026-05-02.md`.
- Deterministic checkpoint: `doc/writhing-stalker-threat-distraction-deterministic-checkpoint-v0-2026-05-03.md`.
- Live/staged proof: `doc/writhing-stalker-threat-distraction-live-staged-proof-v0-2026-05-03.md`.

---

## CAOL-CAMP-LOCKER-ZONE-PLAYTESTS-v0 — Camp locker zone playtests

Status: CLOSED / CHECKPOINTED GREEN V0

Success state:
- [x] The playtest pass chooses a bounded v0 set instead of reopening all locker-zone history.
- [x] At least one current-build harness/live row proves real `CAMP_LOCKER` zone footing with a scenario/run id and artifact path, or records the exact harness/UI blocker.
- [x] At least one row proves camp locker service behavior from zone stock on the real service path, not only helper classification.
- [x] At least one row checks boundary/exclusion behavior such as `NO_NPC_PICKUP`, non-locker stock, or saved zone persistence.
- [x] Weather/wait or practical gear behavior is either playtested on a live/harness row or explicitly scoped out with a reason.
- [x] Screenshots/OCR/save audits name the visible or persisted fact they prove; raw JSON and startup/load are not credited as gameplay proof.
- [x] If the harness cannot honestly prove a required row, `runtime/josef-playtest-package.md` or a repo doc gets a concise Josef manual playtest recipe with expected outcomes and closure criteria.
- [x] `Plan.md`, `TODO.md`, `SUCCESS.md`, `TESTING.md`, `doc/work-ledger.md`, and `andi.handoff.md` agree on the final state before closure.

Canonical docs:
- Proof/readout: `doc/camp-locker-zone-playtest-proof-v0-2026-05-02.md`.
- Imagination source: `doc/camp-locker-zone-playtests-imagination-source-2026-05-02.md`.
- Contract: `doc/camp-locker-zone-playtest-packet-v0-2026-05-02.md`.
- Handoff packet: `doc/camp-locker-zone-playtest-handoff-v0-2026-05-02.md`.

---

## CAOL-WRITHING-STALKER-LIVE-FUN-SCENARIOS-v0 — Writhing stalker live fun-scenario benchmarks

Status: CLOSED / CHECKPOINTED GREEN V0 LIVE FUN-SCENARIO PACKET

Success state:
- [x] Scenario A campfire/light counterplay is green: `writhing_stalker.live_campfire_counterplay_mcw` -> `.userdata/dev-harness/harness_runs/20260430_233129/`.
- [x] Scenario B alley predator/shadow route is green: `writhing_stalker.live_alley_predator_mcw` -> `.userdata/dev-harness/harness_runs/20260430_233156/`.
- [x] Scenario C zombie distraction without magic is green: `writhing_stalker.live_zombie_distraction_mcw` -> `.userdata/dev-harness/harness_runs/20260430_233521/`; no-magic guard `writhing_stalker.live_zombie_distraction_no_magic_guard_mcw` -> `.userdata/dev-harness/harness_runs/20260430_233335/`.
- [x] Scenario D door/light escape is green: `writhing_stalker.live_door_light_escape_mcw` -> `.userdata/dev-harness/harness_runs/20260430_233405/`.
- [x] Scenario E wounded predator retreat is green: `writhing_stalker.live_wounded_predator_mcw` -> `.userdata/dev-harness/harness_runs/20260430_233434/`.
- [x] At least one repeated-turn trace proves the fun rhythm: shadow/pressure -> strike -> cooldown/reposition -> second strike, plus wounded retreat and escape break.
- [x] All credited rows include scenario/run ids, decision/reason traces, pass/fail verdicts, and stability/perf notes in `doc/writhing-stalker-live-fun-scenario-proof-v0-2026-04-30.md`.
- [x] Smallest behavior/tuning change made: live cooldown no longer refreshes `effect_run` every cooling-off turn, preserving no-omniscience, counterplay, cooldown anti-spam, and injured retreat.
- [x] `Plan.md`, `TODO.md`, `SUCCESS.md`, `TESTING.md`, `doc/work-ledger.md`, and `andi.handoff.md` match final state.

Canonical docs:
- Imagination source: `doc/writhing-stalker-live-fun-scenarios-imagination-source-of-truth-2026-04-30.md`.
- Contract: `doc/writhing-stalker-live-fun-scenarios-packet-v0-2026-04-30.md`.
- Prior deterministic proof: `doc/writhing-stalker-behavior-pattern-proof-v0-2026-04-30.md`.
- Closure proof: `doc/writhing-stalker-live-fun-scenario-proof-v0-2026-04-30.md`.

---

## CAOL-ZOMBIE-RIDER-0.3-v0 — Zombie rider initial dev

Status: CLOSED / CHECKPOINTED GREEN V0 INITIAL DEV

Success state:
- [x] Exact flavor text is preserved in raw intake, imagination source, and actual monster description.
- [x] Monster JSON / definitions validate and focused tests cover endpoint spawn/evolution gating.
- [x] Pre-local-combat counterplay footing proves the actual large rider body rejects `SMALL_PASSAGE` / window-like terrain while normal-sized pathing can use the same passable seam.
- [x] Local combat tests cover scary-fast movement, ranged shooting, shoot/flee/reposition cadence, injury/pressure withdrawal, and counterplay through cover/line-of-sight/terrain.
- [x] Overmap light-attraction tests cover mature-world attraction, early-world suppression, no-light/weak/daylight negative controls, temporary light-memory decay, and a max two-rider draw cap.
- [x] Map-AI tests cover rider convergence, rider-band formation, repeated-interest accumulation caps, and no-convergence after decayed interest.
- [x] Live or harness playtests cover open-field terror, cover/indoor escape, camp-light attraction, rider-band circling/harassment, and wounded-rider retreat or disengagement. Rows green so far: open-field local-combat pressure via `zombie_rider.live_open_field_pressure_mcw` -> `.userdata/dev-harness/harness_runs/20260501_013055/`; wounded disengagement via `zombie_rider.live_wounded_disengagement_mcw` -> `.userdata/dev-harness/harness_runs/20260501_014613/`; cover/LOS escape via `zombie_rider.live_cover_escape_mcw` -> `.userdata/dev-harness/harness_runs/20260501_021656/`; camp-light convergence/band circle-harass via `zombie_rider.live_camp_light_band_mcw` -> `.userdata/dev-harness/harness_runs/20260501_030432/`; matched no-camp-light negative control via `zombie_rider.live_no_camp_light_control_mcw` -> `.userdata/dev-harness/harness_runs/20260501_032016/`.
- [x] Metrics include scenario/run ids, turn/time budgets, decision/reason traces, rider counts, band state, shot/reposition/retreat counts, warnings/log spam/crash state, and available per-turn/cadence costs. Current row metrics are recorded in `doc/zombie-rider-live-funness-open-field-proof-v0-2026-05-01.md`, `doc/zombie-rider-live-funness-wounded-disengagement-proof-v0-2026-05-01.md`, `doc/zombie-rider-live-funness-cover-escape-proof-v0-2026-05-01.md`, `doc/zombie-rider-live-funness-camp-light-proof-v0-2026-05-01.md`, and `doc/zombie-rider-live-funness-no-camp-light-control-proof-v0-2026-05-01.md`.
- [x] No unresolved tuning problem surfaced during v0 proof; the smallest landed changes preserve endpoint danger, readable counterplay, no early routine spawn, no omniscient light doom, and no infinite banding.
- [x] `Plan.md`, `SUCCESS.md`, `TESTING.md`, `TODO.md`, `doc/work-ledger.md`, and `andi.handoff.md` match final closed state; closure readout: `doc/zombie-rider-0.3-closure-readout-v0-2026-05-01.md`.

Evidence: commit `d50715f00e` adds `mon_zombie_rider`, pseudo bow footing, mature `GROUP_ZOMBIE` direct gate at `730 days`, and `tests/zombie_rider_test.cpp`. Gates: `make -j4 tests/zombie_rider_test.o tests LINTJSON=0 ASTYLE=0`; `./tests/cata_test "[zombie_rider]"` -> `All tests passed (34 assertions in 2 test cases)` with clean JSON load/no debug errors. Follow-up deterministic passage-seam proof adds `zombie_rider_large_body_small_passage_pathing`: spawned rider actual size rejects `TFLAG_SMALL_PASSAGE` / `t_window_empty`, normal floor remains move-valid, normal-sized pathfinding can use the passable window seam, and rider-sized pathfinding routes around it. Local combat checkpoint `4343dbdad1` proves live `monster::plan()` consumption for bow shot cadence/cooldown, post-shot reposition destination, close-pressure retreat, injured withdrawal, and blocked-LOS no-shot counterplay; gate `./tests/cata_test "[zombie_rider]"` -> `All tests passed (100 assertions in 7 test cases)`. Overmap light-attraction checkpoint `d2ffbd54c3` adds `zombie_rider_overmap_ai` over existing light projection/horde signal footing with mature-world gate, negative controls, memory decay, and `max_riders_drawn_by_light = 2`; gate `make -j4 tests/zombie_rider_test.o obj/zombie_rider_overmap_ai.o tests LINTJSON=0 ASTYLE=0 && ./tests/cata_test "[zombie_rider]"` -> `All tests passed (135 assertions in 10 test cases)`. Rider convergence/band checkpoint `ce05ef44ec` adds capped deterministic convergence by distance then `rider_id`, decayed-memory/no-eligible/lone-rider controls, and camp pressure posture selection for `none`, `investigate`, `circle_harass`, `direct_attack`, and `withdraw`; gate `git diff --check && make -j4 tests/zombie_rider_test.o obj/zombie_rider_overmap_ai.o tests LINTJSON=0 ASTYLE=0 && ./tests/cata_test "[zombie_rider]"` -> `All tests passed (170 assertions in 14 test cases)`. First staged-but-live open-field row `zombie_rider.live_open_field_pressure_mcw` is green at `.userdata/dev-harness/harness_runs/20260501_013055/`: one hostile rider at `[0,-5,0]`, noon start, safe mode off, `6/6` step rows green, no abort/runtime warnings, `bow_pressure` then `withdraw` live-plan artifact rows, max observed `eval_us=2`; proof note `doc/zombie-rider-live-funness-open-field-proof-v0-2026-05-01.md`. Wounded disengagement row `zombie_rider.live_wounded_disengagement_mcw` is green at `.userdata/dev-harness/harness_runs/20260501_014613/` after forced fresh executable relink: one wounded hostile rider at `[0,-8,0]`, `hp=36`, `6/6` step rows green, no abort/runtime warnings, `withdraw=14`, `bow_pressure=0`, distance widens `7` -> `24`, max observed `eval_us=3`; proof note `doc/zombie-rider-live-funness-wounded-disengagement-proof-v0-2026-05-01.md`. Cover/LOS escape row `zombie_rider.live_cover_escape_mcw` is green at `.userdata/dev-harness/harness_runs/20260501_021656/`: one hostile rider at `[10,0,0]` behind a thick `f_locker` wall, noon start, zero overmap NPC targets, `9/9` step rows green, no abort/runtime warnings, feature-path proof, `target_probe=104`, `decision=bow_pressure=0`, `line_of_fire=yes=0`; proof note `doc/zombie-rider-live-funness-cover-escape-proof-v0-2026-05-01.md`. Camp-light convergence/band row `zombie_rider.live_camp_light_band_mcw` is green at `.userdata/dev-harness/harness_runs/20260501_030432/` after the fresh `cataclysm-tiles` relink: mature-world midnight (`world_age_days=735`), zero overmap NPC targets, two hostile riders, four bright `fd_fire` camp lights, `8/8` step rows green, feature-path proof, and same-run artifact row `aggregate_sources=3`, `selected_riders=2`, `band_formed=yes`, `band_size=2`, `posture=circle_harass`; proof note `doc/zombie-rider-live-funness-camp-light-proof-v0-2026-05-01.md`. Matched no-camp-light control row `zombie_rider.live_no_camp_light_control_mcw` is green at `.userdata/dev-harness/harness_runs/20260501_032016/` after refreshing `obj/tiles/version.o`: same mature-world midnight/two-rider footing, zero overmap NPC targets, no `fd_fire` at the four positive-row source offsets, `8/8` step rows green, feature-path proof, and negative artifact guard absence for `zombie_rider camp_light:`, `bandit_live_world horde light signal:`, `selected_riders= && band_formed=yes`, and `posture=circle_harass`; proof note `doc/zombie-rider-live-funness-no-camp-light-control-proof-v0-2026-05-01.md`. Scope caveat: these are staged-but-live rows, not natural discovery or full siege/navigation proof.

Canonical docs:
- Raw intake / exact flavor text: `doc/zombie-rider-raw-intake-2026-04-30.md`.
- Imagination source: `doc/zombie-rider-0.3-imagination-source-of-truth-2026-04-30.md`.
- Contract: `doc/zombie-rider-0.3-initial-dev-packet-v0-2026-04-30.md`.
- Map-AI / funness benchmark suite: `doc/zombie-rider-map-ai-funness-benchmark-suite-v0-2026-04-30.md`.

---

## CAOL-ZOMBIE-RIDER-CLOSE-PRESSURE-NO-ATTACK-v0 — Zombie rider close-pressure no-attack fix

Status: CLOSED / CHECKPOINTED GREEN V0 / BUGFIX + PRODUCT-FEEL FOLLOW-UP

Success state:
- [x] A focused repro or minimized test/harness scenario demonstrates the original no-attack smell or explains why the yellow seed run cannot be trusted.
- [x] The root cause of `bow_pressure + line_of_fire=yes` producing no visible attack is named with source evidence.
- [x] A focused fix makes close/indoor rider pressure visibly hostile: shoot when line of fire and action resources allow; otherwise reposition/bunny-hop/circle irregularly instead of idling.
- [x] Deterministic tests cover the decision-to-action bridge and at least one close/indoor reposition/pressure case.
- [x] Fresh live/handoff validation shows a close rider attacking or aggressively repositioning within a few turns, with screenshots/artifacts and no runtime-version mismatch if claiming feature proof.
- [x] Existing rider guarantees still pass: wounded disengagement, blocked LOS/cover counterplay, overmap light attraction/band cap, no-light negative control, and no wall-suicide/perfect-orbit behavior.
- [x] Zombie rider description text is updated/preserved according to Josef's corrected product image.

Canonical docs:
- Imagination source: `doc/zombie-rider-close-pressure-no-attack-imagination-source-2026-05-02.md`.
- Contract: `doc/zombie-rider-close-pressure-no-attack-packet-v0-2026-05-02.md`.
- Handoff packet: `doc/zombie-rider-close-pressure-no-attack-handoff-v0-2026-05-02.md`.
- Closure proof: `doc/zombie-rider-close-pressure-no-attack-proof-v0-2026-05-02.md`.

Evidence: root cause named as the missing `aggro_character` bridge between rider `bow_pressure` planning and the monster gun actor's avatar-target gate. Gates: `git diff --check && make -j4 tests/zombie_rider_test.o tests src/monmove.o LINTJSON=0 ASTYLE=0`; `./tests/cata_test "[zombie_rider]"` -> `All tests passed (199 assertions in 16 test cases)`; `./just_build_macos.sh > /tmp/caol-zombie-rider-tiles-build3.log 2>&1` -> exit `0`. Fresh staged-but-live row `zombie_rider.live_open_field_pressure_mcw` -> `.userdata/dev-harness/harness_runs/20260502_050055/` is green feature-path proof: `feature_proof=true`, `verdict=artifacts_matched`, `green_step_local_proof`, no abort/runtime warnings, saved rider ammo audited as `arrow_wood=18`, live log shows `aggro_before=no aggro_after=yes`, bow ammo decrement, and close `decision=reposition reason=too_close_bunny_hop`. Caveat: staged-but-live McWilliams proof, not natural random discovery/full siege proof.

---

## CAOL-WRITHING-STALKER-HIT-FADE-RETREAT-DISTANCE-v0 — Writhing stalker hit-fade retreat distance

Status: CLOSED / CHECKPOINTED GREEN v0 / PRODUCT-TUNING + BEHAVIOR FIX FOLLOW-UP

Success state:
- [x] Original flesh-raptor hit-and-run behavior is checked and summarized as the reference baseline, including likely retreat distance/rhythm where evidence allows.
- [x] Current stalker burst/retreat behavior is reproduced or minimized, with retreat distance and decision reasons visible.
- [x] Stalker post-burst retreat aims for about 8+ tiles when pathing allows, with tested fallback when blocked.
- [x] Stress/counterpressure affects burst length and retreat: light plus multiple allies causes earlier/farther caution; high-stress dark/distraction cases may allow 2-4 attacks before disengage.
- [x] Deterministic tests cover retreat distance, stress-modulated burst count/caution, and no A/B near-player oscillation after the burst.
- [x] Fresh live/handoff validation shows readable attack burst then real disengage, with screenshots/artifacts and no runtime-version mismatch if claiming feature proof.
- [x] Existing stalker guarantees still pass: no omniscience, no constant strike spam, injured retreat, light/focus counterplay, and zombie-shadow/quiet-side behavior.

Evidence: proof/readout `doc/writhing-stalker-hit-fade-retreat-distance-proof-v0-2026-05-02.md`; deterministic `[writhing_stalker]` gate green (`206 assertions in 17 test cases`); current-build `cataclysm-tiles` build green; staged-live feature proof `.userdata/dev-harness/harness_runs/20260502_113738/` reports `feature_proof=true`, `verdict=artifacts_matched`, `step_ledger_status=green_step_local_proof`, with strike/withdraw/cooling-off, `burst=0/2`, `burst=1/2`, `burst=2/2`, `retreat_distance=8`, and `cooldown=yes`.

Caveats: staged-but-live McWilliams proof, not natural random discovery; prior watched seed `.userdata/dev-harness/harness_runs/20260502_015032/` remains yellow/debug footing only due to runtime-version mismatch; final human taste is optional/future-only.

Canonical docs:
- Imagination source: `doc/writhing-stalker-hit-fade-retreat-distance-imagination-source-2026-05-02.md`.
- Contract: `doc/writhing-stalker-hit-fade-retreat-distance-packet-v0-2026-05-02.md`.
- Handoff packet: `doc/writhing-stalker-hit-fade-retreat-distance-handoff-v0-2026-05-02.md`.
- Proof/readout: `doc/writhing-stalker-hit-fade-retreat-distance-proof-v0-2026-05-02.md`.

---

## CAOL-FLESH-RAPTOR-CIRCLING-SKIRMISHER-v0 — Flesh raptor circling skirmisher retrofit

Status: CLOSED / CHECKPOINTED GREEN V0 / PREDATOR VARIETY PACKAGE

Success state:
- [x] Flesh raptor behavior no longer depends solely on generic `HIT_AND_RUN` for its main readable pattern when open circling terrain exists.
- [x] Deterministic map tests prove a raptor prefers a valid 4–6 tile orbit/flank position over straight retreat when open lateral space exists.
- [x] Deterministic crowding tests prove a raptor prefers the under-occupied arc rather than stacking into the same zombie/enemy side.
- [x] Corridor/blocked-terrain tests prove graceful fallback without jitter loops or stuck non-actions.
- [x] Live/playtest rows compare old-feeling stab/flee frustration against new circling pressure with metrics for orbit turns, swoop cadence, hit count, equipment-damage/frustration events, player counterplay, warnings/errors, and perceived fun/annoyance caveats.
- [x] Existing JSON/load and focused monster tests remain green for raptor variants touched by the package.
- [x] Frau closure review accepted v0 for agent-side close with staged-but-live caveats; Josef taste/playtest is optional/future, not a blocker.

Evidence: focused `[flesh_raptor]` tests are green; full tiles build is green; `git diff --check` is clean; `astyle-diff` is unavailable on this host because `astyle` is not installed. Credited staged-but-live rows: open field `flesh_raptor.live_open_field_skirmisher_mcw` -> `.userdata/dev-harness/harness_runs/20260501_052709/`; crowded arc `flesh_raptor.live_crowded_arc_skirmisher_mcw` -> `.userdata/dev-harness/harness_runs/20260501_053414/`; blocked corridor `flesh_raptor.live_blocked_corridor_skirmisher_mcw` -> `.userdata/dev-harness/harness_runs/20260501_054807/`; old-feeling/equipment-frustration comparison `flesh_raptor.live_equipment_frustration_comparison_mcw` -> `.userdata/dev-harness/harness_runs/20260501_062300/`.

Caveats: staged-but-live McWilliams rows, not natural random discovery; no equipment-damage tuning changed; later Josef taste/tuning feedback would be a follow-up, not a v0 proof failure.

Canonical docs:
- Imagination source: `doc/predator-behavior-variety-imagination-source-of-truth-2026-05-01.md`.
- Contract / closure evidence: `doc/flesh-raptor-circling-skirmisher-packet-v0-2026-05-01.md`.

---

## CAOL-WRITHING-STALKER-ZOMBIE-SHADOW-PREDATOR-v0 — Writhing stalker zombie-shadow predator shift

Status: CLOSED / CHECKPOINTED GREEN V0 / PREDATOR VARIETY PACKAGE

Success state:
- [x] The stalker confidence model explicitly distinguishes evidence/interest, zombie pressure, quiet-side/cutoff opportunity, and counterpressure from light/focus/open exposure.
- [x] Deterministic tests prove that with zombies on one side of the player, the stalker prefers the under-occupied/quiet side when shadow or cover permits.
- [x] Deterministic tests prove zombie pressure increases stalker confidence only when local evidence or overmap-interest footing exists; no-evidence/no-magic cases stay targetless.
- [x] Deterministic tests prove light/focus/open exposure suppresses quiet-side cutoff/strike behavior.
- [x] Live/playtest rows prove at least one “fighting zombies, stalker appears on quiet side/back route” scenario and one “running from zombies, stalker blocks/cuts off escape side” scenario, scoped to local-evidence-only live footing.
- [x] Metrics include zombie-pressure side, chosen quiet-side/cutoff tile, confidence reasons, strike timing, counterplay outcome, turn-time cost, warnings/errors, and player fun/fairness notes, with the live overmap-interest caveat preserved.

Evidence: `src/writhing_stalker_ai.*` adds confidence and quiet-side/cutoff evaluators; `src/monmove.cpp` consumes the quiet-side scorer for live shadow destinations and logs confidence plus same-run quiet-cutoff pressure/chosen-tile metrics; `tests/writhing_stalker_test.cpp` covers one-sided zombie pressure, split-pressure ambiguity, retreat-route cutoff bias, pressure gating, and light/focus/open-exposure suppression. Gates: `make -j4 tests/writhing_stalker_test.o tests src/writhing_stalker_ai.o LINTJSON=0 ASTYLE=0 && ./tests/cata_test "[writhing_stalker][ai]"` -> `All tests passed (135 assertions in 12 test cases)`; `./tests/cata_test "[writhing_stalker]"` -> `All tests passed (167 assertions in 14 test cases)`. First scoped live row: `writhing_stalker.live_quiet_side_zombie_pressure_mcw` -> `.userdata/dev-harness/harness_runs/20260501_071548/`; proof note `doc/writhing-stalker-zombie-shadow-live-quiet-side-proof-v0-2026-05-01.md`; decisive lines include `overmap_interest=no`, `zombie_pressure=3`, `pressure_x=3`, `quiet_x=-1`, `chosen_rel_x=-1`, `chosen_rel_y=-4`, `reason=quiet_side_cutoff_preferred`, max matched `eval_us=38`. Later sampler audit found an `ERROR GAME` wall-location backtrace in this row's `probe.artifacts.log`, so it is dirty/caveated for Josef-facing optical footing unless rerun clean. Second scoped live row: `writhing_stalker.live_escape_side_zombie_retreat_mcw` -> `.userdata/dev-harness/harness_runs/20260501_071940/`; proof note `doc/writhing-stalker-zombie-shadow-live-escape-side-proof-v0-2026-05-01.md`; decisive lines include `overmap_interest=no`, `zombie_pressure=3`, `pressure_y=-3`, `quiet_y=1`, `chosen_rel_x=-2`, `chosen_rel_y=4`, `reason=quiet_side_cutoff_preferred`, max matched shadow `eval_us=16`, no runtime warnings, no abort.

Canonical docs:
- Imagination source: `doc/predator-behavior-variety-imagination-source-of-truth-2026-05-01.md`.
- Contract: `doc/writhing-stalker-zombie-shadow-predator-packet-v0-2026-05-01.md`.

---

## CAOL-WRITHING-STALKER-BEHAVIOR-SEAM-REDUCTION-v0 — Writhing stalker behavior seam reduction

Status: CLOSED / CHECKPOINTED GREEN V0 / ANTI-REDUNDANCY PACKAGE

Success state:
- [x] Existing monster behavior/strategy/special-attack seams that can replace bespoke writhing-stalker planning glue are identified in the implementation note or commit message: the refactor reuses `monster::plan()` target acquisition plus generic hostile/flee fallback through a named `targeted_live_plan_adapter`; no behavior-tree or special-attack seam honestly owns this destination-planning response today.
- [x] The live-facing `monster::plan()` path still consumes the approved stalker behavior current at execution time through a named, test-covered seam.
- [x] Focused `[writhing_stalker]` tests remain green for the approved zombie-shadow behavior, including no-evidence/no-beeline, quiet-side/cutoff scoring, cover preference, bright/focused counterplay, vulnerability strike, cooldown anti-spam, repeated strike rhythm, and injured retreat.
- [x] At least one new or updated seam-consumption test fails if the behavior is only exercised by detached helper code and not by the live planner path.
- [x] No product tuning or new gameplay claim is mixed into the refactor unless separately promoted and proven.

Evidence: `src/monmove.cpp` replaces three inline live-plan exceptions with `targeted_live_plan_adapter` dispatch for writhing stalker, zombie rider, and flesh raptor before generic destination fallback, while leaving product-specific stalker evaluator and quiet-side scorer custom. Gates: `git diff --check`; `make -j4 tests/writhing_stalker_test.o tests src/writhing_stalker_ai.o LINTJSON=0 ASTYLE=0 && ./tests/cata_test "[writhing_stalker]"` -> `All tests passed (192 assertions in 15 test cases)`; `./tests/cata_test "[zombie_rider],[flesh_raptor]"` -> `All tests passed (231 assertions in 21 test cases)`.

Canonical docs:
- Imagination source: `doc/anti-redundancy-packaging-imagination-source-of-truth-2026-05-01.md`.
- Contract: `doc/writhing-stalker-behavior-seam-reduction-packet-v0-2026-05-01.md`.

---

## CAOL-JOSEF-PLAYTEST-SAVE-PACK-v0 — Josef playable save/handoff pack

Status: WAITING FOR NEXT GREENLIGHT / JOSEF HANDOFF CARD READY / OPTIONAL BANDIT CONTRAST READY / NOT CURRENT ANDI LANE

Success state:
- [x] Six labelled playtest entries exist: Basecamp AI/camp locker, bandit pressure, cannibal camp, flesh raptor, zombie rider, writhing stalker.
- [x] Stalker contrast rows cover no-fire/low-threat close-strike-fade, fire/light stalking/hesitation, and high-threat stalking/pressure or are explicitly caveated/blocked.
- [x] Bandit contrast rows cover no-signal, fire/smoke/basecamp signal, and high-threat/resistant setup or are explicitly caveated/blocked.
- [x] Camp/NPC preflight audits assignment: unassigned harness-save NPCs are removed/neutralized for low-threat rows or replaced/repaired with properly camp-assigned members for high-threat rows.
- [x] Each entry has a loadable save/handoff/setup path or is explicitly marked blocked with the missing primitive.
- [x] Each ready entry has current-build load/start-state evidence and no silent portal-storm contamination.
- [x] Each entry has short Josef-facing instructions and focused taste questions.
- [x] Existing proof rows are cited only as footing and staged-vs-natural caveats stay visible.
- [x] The final Josef-facing card is short enough to use without log archaeology.
- [x] Any blockers become concrete follow-up packets or caveated omissions, not hidden failures.

Canonical docs:
- Imagination source: `doc/caol-josef-playtest-save-pack-imagination-source-2026-05-02.md`.
- Contract: `doc/caol-josef-playtest-save-pack-packet-v0-2026-05-02.md`.
- Handoff: `doc/caol-josef-playtest-save-pack-handoff-v0-2026-05-02.md`.
- Josef-facing card: `doc/caol-josef-playtest-save-pack-card-v0-2026-05-02.md`.

Checkpoint: v0 card is usable for six staged rows (locker, bandit first-demand contact, cannibal, flesh raptor crowded-arc skirmisher, zombie rider cover/wounded, writhing stalker hit-fade/light/zombie-side) and explicitly withholds broader bandit camp-threat credit from the first Josef card. The post-card repair boundary is now reviewed: low-threat/no-signal has cleaned no-loose-NPC proof (`20260502_134959/`), smoke/fire signal has guarded mixed-signal proof (`20260502_155058/`, live-signal scout from camp `@151,39,0`, separate structural scavenge from camp `@160,39,0`, active members `4` and `9` cross-referenced in saved overmap records), active-outside camp-pressure assignment has member cross-reference proof (`20260502_144842/`, `active_member_ids=[4,5]`, `active_members_all_found_in_saved_overmap=true`), and high-threat/low-reward hold has green risk/reward artifact proof (`20260502_145429/`). These are staged auxiliary contrast rows for an optional second bandit card; natural-discovery/full-raid bandit/Basecamp proof remains future-only.

---

## CAOL-VISIONS-PLAYTEST-SAMPLER-v0 — C-AOL visions product-feel sampler

Status: SUPERSEDED / FOLDED INTO `CAOL-JOSEF-PLAYTEST-SAVE-PACK-v0`

Success state:
- [x] The sampler chooses a bounded v0 set of 3-5 postcards instead of becoming open-ended “play the whole mod”.
- [x] Each postcard has a named vision, a prepared scenario/handoff or exact setup recipe, and a short Josef-facing play instruction.
- [x] Each visual postcard has at least one screenshot checkpoint with a named expected optical/visible fact.
- [x] Each postcard has focused taste questions that distinguish fun/readable/fair/alive/optically-legible/gnostic from annoying/fake/unfair/invisible/visually-confusing/hollow.
- [x] Existing closed proof rows are cited as footing where used, without laundering staged rows into natural discovery claims.
- [x] If fresh handoff runs are created, each run records artifact dir, cleanup/handoff status, and one visible fact or explicit reason visual proof is not the evidence class. (No fresh handoff runs created for this draft checkpoint; exact handoff commands are listed per postcard.)
- [x] The final Josef handoff is short enough to actually use.

Canonical docs:
- Imagination source: `doc/caol-visions-playtest-imagination-source-2026-05-01.md`.
- Contract: `doc/caol-visions-playtest-sampler-packet-v0-2026-05-01.md`.
- Josef card / short relay packet: `doc/caol-visions-josef-playtest-card-v0-2026-05-01.md`.

Checkpoint: v0 card selects four postcards (writhing stalker, zombie rider, flesh raptor, camp locker), includes handoff commands, screenshot checkpoints, existing artifact footing, taste/gnostic questions, staged-footing caveats, and a compressed Schani/Josef relay packet. Post-crunch correction applied: the stalker postcard uses the cleaner escape-side row `.userdata/dev-harness/harness_runs/20260501_071940/` as primary footing; the older quiet-side row `.userdata/dev-harness/harness_runs/20260501_071548/` is explicitly dirty/caveated because targeted grep found `ERROR GAME ... writhing stalker can't move to its location! ... reinforced white concrete wall`. Agent-side sampler prep is complete and now feeds `CAOL-JOSEF-PLAYTEST-SAVE-PACK-v0`; the active deliverable is a playable save/handoff pack rather than a relay-only card.

---

## CAOL-BANDIT-SCENIC-SHAKEDOWN-CHAT-OPENINGS-v0 — Bandit scenic shakedown chat openings

Status: CLOSED AS SCENIC-UI PROOF / SUPERSEDED BY `CAOL-JOSEF-LIVE-DEBUG-BATCH-v0` FOR FINAL RESPONSE + PAYMENT CONTRACT

Success state:
- [x] Shakedown opening reaches a normal chat/dialogue window path where feasible, or an explicit product-path reason says why not. _Implemented via `dialogue_window` in `src/do_turn.cpp`; narrow compile/test green._
- [x] At least three distinct opening beats exist and are selected from scenario context, not pure random decoration. _Current deterministic set: basecamp pressure, warning from cover, weakness read, roadblock toll, reopened demand._
- [x] Scenic proof showed the historical pay/fight/refuse fork legibly, but this response/payment contract is superseded. _Active correction: final visible responses must be Pay/Fight only; fight/refuse/backout enter the hostile branch; Pay must open the actual NPC trade UI / trade window over the whole honest basecamp/faction-side inventory pool, autostarted with debt/toll derived from what that camp side has so the player dumps items into the offer, instead of silently surrendering demanded reachable goods._
- [x] First-demand and reopened/higher-demand shakedown paths both retain deterministic/harness proof. _Green rows: `bandit.extortion_first_demand_pay_mcw` -> `.userdata/dev-harness/harness_runs/20260502_065253/`; `bandit.extortion_reopened_demand_mcw` -> `.userdata/dev-harness/harness_runs/20260502_065445/`._
- [x] Snapshot proof includes at least one normal-chat shakedown opening and one scenic variant, with named optical facts. _Screenshots: `advance_final_turn_to_first_shakedown.after.png` and `advance_final_turn_to_reopened_shakedown.after.png`, review copies in `/Users/josefhorvath/.openclaw/workspace/runtime/caol-bandit-scenic-review-20260502/`._
- [x] No cannibal/no-shakedown profile or unrelated bandit attack posture regresses into polite toll UI. _Green row: `cannibal.live_world_exposed_sight_avoid_mcw` -> `.userdata/dev-harness/harness_runs/20260502_065927/`, with `profile=cannibal_camp`, `shakedown=no`, `combat_forward=no`._

Canonical docs:
- Superseding correction: `doc/josef-live-debug-batch-packet-v0-2026-05-03.md`.
- Scenic contract: `doc/bandit-scenic-shakedown-chat-window-openings-packet-v0-2026-05-01.md`.
- Proof: `doc/bandit-scenic-shakedown-chat-window-openings-proof-v0-2026-05-02.md`.
- Gate: `git diff --check`; `make -j4 tests src/do_turn.o src/bandit_live_world.o tests/bandit_live_world_test.o LINTJSON=0 ASTYLE=0`; `./tests/cata_test "[bandit][live_world][shakedown]"` -> `All tests passed (136 assertions in 4 test cases)`.

---

## CAOL-HARNESS-PORTAL-STORM-WARNING-LIGHT-v0 — Harness portal-storm warning light

Status: CHECKPOINTED GREEN V0 / HARNESS-HARDENING FOLLOW-UP

Success state:
- [x] Harness reads the relevant saved/current weather state and recognizes portal storm weather explicitly.
- [x] `probe`, `handoff`, and `repeatability` report surfaces include a visible portal-storm warning/light when observed.
- [x] Step ledger or report status makes unallowed portal storm contamination yellow/red instead of silent green.
- [x] Scenarios can explicitly allow or require portal storms without making every intentional portal-storm test fail.
- [x] Negative-control proof shows normal weather does not trigger the warning.
- [x] Documentation tells Andi/Schani how to interpret the warning: rerun under controlled weather unless the scenario intentionally tests portal storms.

Canonical docs:
- Contract: `doc/harness-portal-storm-warning-light-packet-v0-2026-05-02.md`.
- Proof: `doc/harness-portal-storm-warning-light-proof-v0-2026-05-02.md`.
- Gate: `python3 tools/openclaw_harness/proof_classification_unit_test.py`; `python3 -m py_compile tools/openclaw_harness/startup_harness.py tools/openclaw_harness/proof_classification_unit_test.py`; `git diff --check`.

---

## CAOL-CAMP-LOCKER-EQUIPMENT-API-REDUCTION-v0 — Camp locker equipment API reduction

Status: CLOSED / CHECKPOINTED GREEN V0 / ANTI-REDUNDANCY PACKAGE

Success state:
- [x] The implementation note or commit message names which camp locker checks now defer to existing item, wearability, body coverage, reload, or zone APIs.
- [x] Camp locker candidate classification and upgrade selection remain green for clothing, armor, bags, melee/ranged weapons, ammo, magazines, and kept readiness items.
- [x] Carried cleanup still dumps only safe non-kept baggage and preserves worn/wielded items plus kept ammo/magazine/medical/insert supplies.
- [x] Ranged readiness still selects compatible magazines/ammo, uses existing reload behavior, and returns leftovers safely.
- [x] Live clothing/armor scoring uses the worker fit context through `item::get_avg_encumber()` for encumbrance penalties, while no-context helper calls keep the prior fallback.
- [x] Live worn-slot candidate collection uses `Character::can_wear(..., true)` to filter worker-specific unwearable armor/clothing before camp scoring/planning, while no-worker helper classification stays stable.
- [x] Live service pre-pass and post-service summary use `collect_camp_locker_live_state()` so worker items, locker stock, candidates, planning, cleanup, ranged readiness, and medical readiness share one aggregation path.
- [x] Live weapon-slot candidate collection uses `Character::can_wield()` to reject worker-specific unwieldable melee/ranged candidates while no-worker helper classification stays stable.
- [x] Camp locker ranged-weapon classification defers to `item::is_gun()` instead of a firearm-only predicate, preserving primitive ranged weapons as item-owned ranged candidates.
- [x] Camp locker armor/clothing classification defers its armor boundary to `item::is_armor()` instead of raw armor-slot lookup, preserving camp slot policy while respecting engine armor ontology.
- [x] Camp locker outerwear/body-armor classification uses existing armor layer APIs, including the body-part-specific `item::has_layer(..., bodypart_id)` overload for torso/arm/leg outer-layer checks, instead of raw `OUTER` flags or global layer checks plus duplicate coverage gates.
- [x] Camp locker leg-accessory classification uses existing armor layer data (`item::has_layer({ layer_level::BELTED })`) instead of raw `BELTED` flag reads for strapped armor, while `BELT_CLIP` remains explicit item clip policy.
- [x] Camp locker protection and ballistic-resistance scoring now consume the shared `resistances` aggregate for bash/cut/bullet armor resistance instead of separate camp-local item-resistance lookups; camp weighting policy remains explicit.
- [x] Camp locker body-part coverage/layer helper callers pass existing `body_part_*` ids and subpart coverage helper callers pass `sub_bodypart_str_id` constants instead of rebuilding body/subpart ids from repeated string literals; camp slot/body/sub-region policy remains explicit.
- [x] Live melee/ranged weapon scoring defers to `Character::evaluate_weapon(..., true)` when a worker fit context is available, while no-context helper calls retain the prior fallback scoring.
- [x] Worker-context ammo readiness defers reload viability to `Character::can_reload()`, so engine-owned reload constraints such as ammo-belt linkages gate locker ammo readiness.
- [x] Locker reload supply selection uses `item::ammo_remaining()` instead of ammo-only `charges`, allowing reload-compatible loaded speedloaders/supplies to ready weapons through existing reload APIs.
- [x] Compatible magazine total-capacity preference composes existing item ammo-state APIs (`item::ammo_remaining()` plus `item::remaining_ammo_capacity()`) instead of camp-local `ammo_data()` / default-ammo type lookup plumbing.
- [x] Managed ranged-readiness recognition asks existing `item::is_gun()` directly instead of re-entering camp locker classification, while enabled-slot policy stays explicit.
- [x] Ranged readiness ready/loaded checks use `item::has_ammo()` instead of local `ammo_remaining() > 0` / `<= 0` predicates, while camp policy still decides which magazines and reload supplies to move.
- [x] Direct medical supply recognition uses `item::get_usable_item()` / `item::get_use()` instead of raw type use lookup, while camp policy still limits readiness stock to direct bandage/bleed supplies.
- [x] Carried cleanup armor-insert preservation now asks existing ablative carrier pockets whether they can contain an item instead of using raw `CANT_WEAR` as insert ontology; ordinary carried armor still dumps through camp-storage cleanup.
- [x] Focused faction/basecamp tests pass for the closed camp-locker API-reduction package without widening the lane.

Canonical docs:
- Imagination source: `doc/anti-redundancy-packaging-imagination-source-of-truth-2026-05-01.md`.
- Contract: `doc/camp-locker-equipment-api-reduction-packet-v0-2026-05-01.md`.

---

## CAOL-BANDIT-SIGNAL-ADAPTER-REDUCTION-v0 — Bandit signal adapter reduction

Status: CLOSED / CHECKPOINTED GREEN V0 / ANTI-REDUNDANCY PACKAGE

Success state:
- [x] The source path from local fire/smoke/light observation to bandit mark input is named and tested as an adapter over local fields/time/weather.
- [x] Bandit mark-generation tests remain green for smoke/weather, light/time/weather, human route, repeated-site, and moving-memory cases touched by the package.
- [x] A focused test/source seam guard proves horde reactions still go through `overmap_buffer.signal_hordes` / `overmap::signal_hordes` rather than a private horde path.
- [x] Existing roof-fire/live-signal expectations remain true, or any changed expectation is explicitly classified as tuning instead of cleanup.
- [x] No bandit dispatch, roster, structural-bounty, or camp-map behavior claim is made from adapter refactoring alone.

Canonical docs:
- Imagination source: `doc/anti-redundancy-packaging-imagination-source-of-truth-2026-05-01.md`.
- Contract: `doc/bandit-signal-adapter-reduction-packet-v0-2026-05-01.md`.
- Closure proof: `doc/bandit-signal-adapter-reduction-proof-v0-2026-05-02.md`.

Evidence: `git diff --check`; `make -j4 tests/bandit_mark_generation_test.o src/bandit_mark_generation.o obj/do_turn.o LINTJSON=0 ASTYLE=0`; standalone adapter probe; `make -j1 tests LINTJSON=0 ASTYLE=0 && ./tests/cata_test "[bandit][marks]"` -> `All tests passed (236 assertions in 18 test cases)`.

---

## CAOL-MULTI-CAMP-SIGNAL-GAUNTLET-v0 — Multi-camp signal gauntlet

Status: CLOSED / CHECKPOINTED GREEN V0

Success state:
- [x] A named Challenge A scenario/fixture exists and runs: `bandit.multi_camp_structural_stress_mcw`, green run `.userdata/dev-harness/harness_runs/20260430_204416/`.
- [x] Challenge A proves at least two camp/site states before/after bounded time and reports active outings/target choices/no-repeat state.
- [x] Challenge A reports dogpile/spread/hold behavior and timing/log stability.
- [x] A named Challenge B scenario/fixture exists and runs: `bandit.mixed_signal_coexistence_mcw`, green run `.userdata/dev-harness/harness_runs/20260430_203757/`.
- [x] Challenge B combines structural bounty with live smoke/fire signal footing and reports candidate priority/reasons.
- [x] Challenge B proves neither signal class silently wipes the other’s state.
- [x] Reload/resume continuity is green for meaningful active live-signal scout + structural scavenge outings: `bandit.mixed_signal_reload_resume_mcw`, green run `.userdata/dev-harness/harness_runs/20260430_203944/`.
- [x] Metrics include waited time/sampled turns, per-cadence perf counters, active group/site counts, warnings/errors/log spam, and crash status.
- [x] `Plan.md`, `TODO.md`, `SUCCESS.md`, `TESTING.md`, `doc/work-ledger.md`, and `andi.handoff.md` match final state.

Canonical docs:
- Imagination source: `doc/multi-camp-signal-gauntlet-imagination-source-of-truth-2026-04-30.md`.
- Contract: `doc/multi-camp-signal-gauntlet-playtest-packet-v0-2026-04-30.md`.
- Proof: `doc/multi-camp-signal-gauntlet-proof-v0-2026-04-30.md`.
- Prior structural-bounty closure footing: `doc/bandit-structural-bounty-phase-7-closure-readout-2026-04-30.md` and `.userdata/dev-harness/harness_runs/20260430_115157/`.

Caveats preserved: two camps rather than four; staged-but-live smoke/fire signal footing; Challenge A proves harvested fixture leads plus east-camp followthrough/no-repeat, not all-camps-idle. No dogpile, stale-state, reload-loss, CPU churn, log spam, or crash failure was observed in the final green runs.

---


## CAOL-WRITHING-STALKER-PATTERN-TESTS-v0 — Writhing stalker primitive behavior-pattern tests

Status: CLOSED / CHECKPOINTED GREEN V0

Success state:
- [x] A named primitive minimap/ASCII/equivalent repeated-turn behavior-pattern helper exists in `tests/writhing_stalker_test.cpp` (`stalker_pattern_row`, `trace_rows`, `run_vulnerable_stalker_pattern`).
- [x] Tests cover no-evidence/no-beeline, weak evidence decay, cover/edge route preference, exposure hold/withdraw, vulnerability strike window, cooldown anti-spam, repeated attack cadence, badly-injured retreat, and jitter/stuckness smells.
- [x] Repeated-attack evidence shows a healthy stalker can strike, cool down/reposition, and strike again if the player remains vulnerable and the opportunity remains plausible.
- [x] Injured-retreat evidence shows badly-injured stalker self-preservation overrides greed even when the player is vulnerable (`hp=50`, `live_stalker_hurt_withdraw`).
- [x] Current code passes without behavior/tuning change; no-omniscience, cover preference, exposed-withdraw, and cooldown anti-spam constraints remain guarded.
- [x] Focused writhing-stalker tests were run and recorded; no fresh live-seam probe was needed because this slice changed tests only and the unchanged `monster::plan()` seam already consumes `writhing_stalker::evaluate_live_response`.
- [x] `Plan.md`, `SUCCESS.md`, `TESTING.md`, `doc/work-ledger.md`, and `andi.handoff.md` match final closed state.

Canonical docs:
- Imagination source: `doc/writhing-stalker-behavior-pattern-imagination-source-of-truth-2026-04-30.md`.
- Contract: `doc/writhing-stalker-behavior-pattern-minimap-packet-v0-2026-04-30.md`.
- Closure proof: `doc/writhing-stalker-behavior-pattern-proof-v0-2026-04-30.md`.
- Prior v0 closure footing: `doc/writhing-stalker-behavior-packet-v0-2026-04-30.md` and `doc/writhing-stalker-playtest-ladder-v0-2026-04-30.md`.

Evidence: `make -j4 tests/writhing_stalker_test.o tests LINTJSON=0 ASTYLE=0 && ./tests/cata_test "[writhing_stalker][ai]"` passed (`97 assertions in 8 test cases`); `./tests/cata_test "[writhing_stalker]"` passed (`129 assertions in 10 test cases`). Trace proof shows `t0 shadow`, `t1 strike`, `t2/t3 cooling_off`, `t4 shadow`, `t5 strike`, `t6/t7 withdraw` with strike count `2`, cooldown anti-spam, retreat at `hp=50`, and jitter count `0`.

---


## CAOL-ROOF-HORDE-NICE-FIRE-v0 — Roof-fire horde nice-fire playtest

Status: CLOSED / CHECKPOINTED GREEN V0

Success state:
- [x] A named scenario/fixture exists for this packet: `bandit.roof_fire_horde_nice_roof_fire_mcw` reuses the honest split fixture `roof_fire_horde_split_wait_from_player_fire_v0_2026-04-29`.
- [x] The credited fire is roof/elevated, inspectable before the wait, and tied to the previous player-created roof-fire chain: source run `.userdata/dev-harness/harness_runs/20260429_172847/`, audited again in green run `.userdata/dev-harness/harness_runs/20260430_191556/`.
- [x] A horde is present before the wait at a plausible distance with saved/metadata footing: `mon_zombie` at offset `[0,-120,0]`, destination self, `tracking_intensity=0`, `last_processed=0`, `moves=0`.
- [x] Bounded in-game time passes through the wait path: `5m`, observed turn delta `300` (`5266942` -> `5267242`).
- [x] Same-run artifacts show the roof-fire signal path firing for the elevated fire: `bandit_live_world horde light signal ... source_omt=(140,41,1) horde_signal_power=20 ... elevated_exposure_extended=yes`.
- [x] Saved/log artifacts show horde response after the wait: destination retargeted to `[3360,984,1]`, `last_processed=5267242`, `moves=8400`.
- [x] Metrics report cost/stability and labels unavailable horde-specific timing as `not instrumented`: end-to-end harness wall-clock `2:34.72`, `14/14` step rows green, `1/1` wait rows green, no runtime warnings/abort, horde-specific timing `not instrumented`.
- [x] `Plan.md`, `TODO.md`, `SUCCESS.md`, `TESTING.md`, `doc/work-ledger.md`, and `andi.handoff.md` match the final state.

Canonical docs:
- Imagination source: `doc/roof-fire-horde-nice-roof-fire-imagination-source-of-truth-2026-04-30.md`.
- Contract: `doc/roof-fire-horde-nice-roof-fire-playtest-packet-v0-2026-04-30.md`.
- Closure proof: `doc/roof-fire-horde-nice-roof-fire-proof-v0-2026-04-30.md`.
- Green run: `.userdata/dev-harness/harness_runs/20260430_191556/`.
- Prior footing: `doc/roof-fire-horde-detection-proof-v0-2026-04-29.md`, source run `.userdata/dev-harness/harness_runs/20260429_172847/`, split proof `.userdata/dev-harness/harness_runs/20260429_180239/`.

---

## CAOL-WRITHING-STALKER-v0 — Writhing stalker behavior packet v0

Status: CLOSED / CHECKPOINTED GREEN V0

Success state:
- [x] Monster/stat/spawn footing exists and validates.
- [x] Spawn rarity/singleton rules prevent ordinary stalker clutter spam.
- [x] Stalker interest/latch/opportunity/withdraw decisions have deterministic tests.
- [x] Direct player/human evidence can create a bounded latch without permanent omniscience.
- [x] Weak/no/stale evidence decays or fails to latch.
- [x] Exposed night light, cover/edge terrain, and zombie distraction affect interest/opportunity with named reasons.
- [x] Approach behavior avoids direct open beelines where cover/darkness/clutter alternatives exist.
- [x] Strike behavior creates short cut/bleed pressure rather than a tank duel.
- [x] Withdrawal/cooldown prevents immediate repeat spam after hurt/exposed/focused states.
- [x] Save/load preserves any new latch/cooldown state, or the packet explicitly avoids new persisted state.
- [x] Live/harness proof `writhing_stalker.live_shadow_strike_mcw` shows a real stalk/hold/strike/cooldown scene from the game path.
- [x] Live/harness proof `writhing_stalker.live_no_omniscient_beeline_mcw` shows no instant beeline/attack without valid evidence.
- [x] Live/harness proof `writhing_stalker.live_exposed_retreat_mcw` shows exposure/focus/hurt causes hold or withdrawal, or is explicitly classified future-only.
- [x] Mixed hostile performance playtest `performance.mixed_hostile_stalker_horde_mcw` reports metrics with bandit camp, cannibal camp, one writhing stalker, and horde present, or is explicitly classified follow-up/future-only.
- [x] Tuning readout records whether the stalker is too common, too fast, too tanky, too invisible, too honest, too stupid, or too expensive under mixed hostile load.
- [x] `Plan.md`, `TODO.md`, `SUCCESS.md`, `TESTING.md`, and `doc/work-ledger.md` match the final active/closed state.

Notes:
- Support live footing is green, but not final behavior closure by itself: `.userdata/dev-harness/harness_runs/20260430_161342/` proves debug-spawn/save `active_monsters` footing; `.userdata/dev-harness/harness_runs/20260430_161535/` proves target acquisition plus the live `live_plan` seam.
- Exposed/focused withdrawal behavior proof is green at `.userdata/dev-harness/harness_runs/20260430_163626/`: harness-only noon fixture `mcwilliams_live_debug_noon_2026-04-30` applies `game_turn=5227200`; saved pre-spawn audit shows `time_of_day_text=12:00:00` and zero noon delta; live-plan artifact proves `decision=withdraw route=hold_exposed reason=live_exposed_and_focused_withdraw ... stalker_bright=yes target_focus=yes cooldown=no`; save/writeback and saved `active_monsters` audits are green.
- Shadow/strike behavior proof is green at `.userdata/dev-harness/harness_runs/20260430_170528/`: harness-only vulnerable-midnight fixture `mcwilliams_live_debug_vulnerable_2026-04-30` applies `game_turn=5270400`, 60% HP, low stamina, and torso `bleed`; saved pre-spawn audit shows `time_of_day_text=00:00:00`; live-plan artifact proves `decision=shadow route=cover_shadow reason=live_shadowing_before_strike_window ... target_focus=no cooldown=no`, then `decision=strike route=cover_shadow reason=live_vulnerability_window_strike ... distance=3`, then `decision=cooling_off ... cooldown=yes`; save/writeback and saved `active_monsters` audits are green.
- No-omniscience negative-control proof is green at `.userdata/dev-harness/harness_runs/20260430_173555/`: harness-only clean fixture `mcwilliams_live_debug_no_evidence_clean_2026-04-30` removes overmap NPC/human targets, applies local noon, places one saved active `mon_writhing_stalker` behind an audited thick `f_locker` wall, and requires same-run `target_probe ... target=no ... sees_player=no` while forbidding `target=yes`, `sees_player=yes`, `writhing_stalker live_plan:`, strike, shadow, or cooldown lines; save/writeback and saved `active_monsters` distance audits are green.
- Strike pressure is green for v0 from combined footing/behavior evidence: monster stats/tests require cut damage plus `scratch`/`bite`, the live shadow/strike proof reaches `decision=strike`, and cooldown/withdraw evidence prevents tank-duel relatch spam.
- Mixed hostile performance proof is green/yellow at `.userdata/dev-harness/harness_runs/20260430_181748/`: `performance.mixed_hostile_stalker_horde_mcw` proves active bandit and cannibal stalk jobs, one `mon_writhing_stalker`, and one nearby `mon_zombie` horde before measurement; completes `500` sampled turns plus a bounded `30m` wait; records turn cost avg `236.239ms/turn`, hostile cadence `total_us` max `3777`, stalker `eval_us` max `54`, no crash/stderr/debug-error flood, and a tuning readout. Frau accepted the yellow horde-attribution caveat for v0 closure: horde cost is `not instrumented` separately; horde presence is setup proof, not direct horde timing, and stricter horde attribution is future-only unless explicitly promoted.
- Imagination source lives at `doc/writhing-stalker-imagination-source-of-truth-2026-04-30.md`.
- Contract lives at `doc/writhing-stalker-behavior-packet-v0-2026-04-30.md`.
- Playtest ladder lives at `doc/writhing-stalker-playtest-ladder-v0-2026-04-30.md`.
- Mixed hostile performance packet lives at `doc/mixed-hostile-stalker-horde-performance-playtest-v0-2026-04-30.md`.
- Raw intake remains preserved at `doc/writhing-stalker-raw-intake-2026-04-30.md`.


---

## CAOL-BANDIT-STRUCT-BOUNTY-v0 — Bandit structural bounty overmap completion packet v0

Status: CLOSED / CHECKPOINTED GREEN V0

Success state:
- [x] Structural OMT classifier exists and deterministic tests cover forest/town/open classes.
- [x] Per-camp bounded structural scan seeds sparse camp-map leads without global scanning.
- [x] Harvested/dangerous/recently-checked debounce prevents immediate repeat interest.
- [x] Non-player structural outing planner can send a small bandit dispatch to forest/town structural bounty.
- [x] Abstract outing resolver reveals threat at stalking distance, subtracts it from effective bounty/interest, and only consumes structural bounty on arrival if interest survives.
- [x] Player/NPC mobile bounty remains attached to actors/routes and does not permanently upgrade terrain.
- [x] Save/load preserves structural leads, active outings, harvested/dangerous outcomes, and member state.
- [x] Deterministic 500-turn tests prove bandits do not get stuck repeating the same harvested/dangerous tile.
- [x] Performance tests/counters prove scan/outing work is bounded for multi-camp scenarios.
- [x] Real `do_turn` maintenance invokes bounded structural scan/outing maintenance with concise report/debug output and a current-runtime tiles build.
- [x] Live/harness feature-path proof shows an idle camp dispatching to structural forest/town bounty without player smoke/light/direct-range bait.
- [x] Live/harness proof shows stalking-distance threat/interest writeback, later arrival harvest, and no immediate repeat of the consumed/harvested target; dangerous/lost-interest no-repeat remains covered by deterministic Phase 4/5 evidence.
- [x] Existing player smoke/light signal receipts remain classified separately and are not overwritten by structural-bounty closure.
- [x] Phase 7 tuning/readout records current numbers as good enough for v0 closure, with future tuning/playtests explicitly separated.
- [x] `Plan.md`, `TODO.md`, `SUCCESS.md`, and `TESTING.md` match the final closed/next-active state.

Compact reference:
- Closure readout lives at `doc/bandit-structural-bounty-phase-7-closure-readout-2026-04-30.md`.
- Imagination source lives at `doc/bandit-structural-bounty-overmap-completion-imagination-source-of-truth-2026-04-30.md`.
- Canonical contract lives at `doc/bandit-structural-bounty-overmap-completion-packet-v0-2026-04-30.md`.
- Testing ladder lives at `doc/bandit-structural-bounty-overmap-testing-ladder-v0-2026-04-30.md`.
- Green run: `.userdata/dev-harness/harness_runs/20260430_115157/`.
- Non-credit run: `.userdata/dev-harness/harness_runs/20260430_114106/`.


---

## GitHub Actions CI recovery + checkpoint packet v0

Status: CLOSED / CHECKPOINTED

Success state:
- [x] Current `dev` GitHub Actions are no longer red for code-caused C-AOL failures, or every remaining red check is explicitly classified with a bounded non-code cause and next owner.
- [x] The C++17/warnings-as-errors failures are fixed: no designated-initializer/missing-field failures in `tests/faction_camp_test.cpp`, no missing-declaration family in `src/basecamp.cpp` / `src/bandit_playback.cpp`.
- [x] Windows build failure is either green or reduced to a named, evidence-backed workflow/runner/package blocker.
- [x] CodeQL is green or its remaining failure is classified as upload/config/external rather than silently sharing the same source compile failure.
- [x] A lightweight CI checkpoint/linking rule exists so future reviewable Andi commits name changed file class, relevant local gate, Actions link when available, and remaining red-check classification.
- [x] `Plan.md`, `TODO.md`, `SUCCESS.md`, and `TESTING.md` match the final state.


Compact reference:
- Canonical contract lives at `doc/github-actions-ci-recovery-checkpoint-packet-v0-2026-04-25.md`.


---

## GitHub normal-download release packet v0

Status: HELD / NOT CURRENTLY PROMOTED

Success state:
- [ ] A new public GitHub release exists on `josihosi/Cataclysm-AOL` with a deliberate tag/version and clear release notes.
- [ ] The release assets match the stated platform support instead of implying broken platforms work.
- [ ] The release source commit and relevant Actions state are linked from canon/testing notes.
- [ ] Josef has a normal GitHub Releases URL he can download from.
- [ ] Any withheld/broken platform is plainly marked with the evidence-backed blocker.

Notes:
- Canonical contract lives at `doc/github-normal-download-release-packet-v0-2026-04-25.md`.
- Current latest stable release observed before packaging: `v0.2.0` / `Cataclysm - Arsenic and Old Lace v0.2.0`.
- This packet was once queued after CI recovery went green, but current `Plan.md` / `TODO.md` promote no active or greenlit execution target. Release/download work needs an explicit Schani/Josef promotion before Andi treats it as active.

---

## C-AOL debug-proof finish stack v0

Status: CLOSED / CHECKPOINTED

Success state:
- [x] The active Basecamp job-spam debounce lane is closed/checkpointed honestly before this stack became active.
- [x] The reopened bandit live signal/site-bootstrap stack has live/source-hook proof for remaining smoke/fire/light product claims, or a precise Josef playtest package for exact implemented-but-unproven behavior.
- [x] Signal range, candidate/scoring split, decay/maintenance, and hold/chill instrumentation are either completed with tests/evidence or explicitly listed as implemented-but-unproven playtest items.
- [x] Smart Zone clean-save live proof is converted into a precise Josef playtest package without reopening the contaminated McWilliams macro.
- [x] The reopened bandit local standoff / scout return live correction is inspected and fixed/proven on the current product path: current-runtime run `.userdata/dev-harness/harness_runs/20260427_154309/` logs five-OMT hold-off (`live_dispatch_goal=140,46,0`), return-home timeout, home-footprint observation, and returned pressure refresh.
- [x] Canon files stop calling current debug notes parked/review-only when Josef has explicitly greenlit another finish pass.

Notes:
- Canonical contract lives at `doc/caol-debug-proof-finish-stack-v0-2026-04-27.md`.
- Attempt rule for this stack was observed: the bandit local standoff / scout-return attempt 3 happened after Frau Knackal consultation and a material code-path/instrumentation change.

---

## Cannibal camp night-raid behavior packet v0

Status: CLOSED / CHECKPOINTED

Success state:
- [x] Deterministic coverage proves cannibal planned attacks/raids require a meaningful pack size under ordinary conditions, while scout/probe behavior may remain smaller only when explicitly classified that way.
- [x] Deterministic coverage proves a one-dispatchable-member cannibal camp does not perform an ordinary full attack/raid against a defended target.
- [x] Deterministic/code coverage proves smoke/light/human-routine nearby leads are interpreted as scout/probe or pack-dispatch pressure rather than instant combat where that substrate is available.
- [x] Deterministic coverage proves darkness/concealment can change a cannibal local-gate outcome from stalk/probe/hold to attack when the rest of the scene is favorable.
- [x] Deterministic coverage proves daylight/no-cover and high-threat cases still delay, hold, probe, stalk, or abort instead of making cannibals suicidal.
- [x] Deterministic coverage proves cannibal stalk/approach sight-avoidance reacts to exposure with bounded adjacent reposition or hold-off/abort rather than visible beeline pressure.
- [x] Bandit shakedown/pay/fight behavior still works unchanged and cannibal shakedown remains blocked.
- [x] Reports name lead source/notes where available, `profile=cannibal_camp`, pack size/strength, darkness/concealment input, current/recent sight-exposure state, posture, and no-shakedown/attack reason.
- [x] Save/load proof preserves cannibal roster, active member IDs, target state, and profile after a multi-member dispatch.
- [x] Live matrix scenario proves day smoke/light or repeated routine creates scout/stalk/dispatch pressure, not instant combat (`20260428_124902`, feature-path, `posture=hold_off`, `active_job=stalk`, smoke lead, no shakedown/combat-forward).
- [x] Live matrix scenario proves night/darkness/concealment can turn confirmed multi-member pack contact into attack through the real dispatch/local-contact path (`20260428_124947`, feature-path, `local_contact=yes`, `darkness_or_concealment=yes`, `posture=attack_now`, `pack_size=2`).
- [x] Live matrix scenario proves exposed stalking/scouting cannibals hold, reposition normally, or abort within a bounded window instead of visibly beelining or teleporting (`20260428_125138`, feature-path, bounded 20-turn window, cannibal `posture=hold_off`, `sight_exposure=recent`, no shakedown/combat-forward).
- [x] Live matrix scenario proves daylight/no-cover and high-threat negatives still hold/probe/abort (`20260428_135323`, feature-path, current runtime `48abd82de9`, `threat=3`, `opportunity=2`, `darkness_or_concealment=no`, `posture=hold_off`, no shakedown/combat-forward).
- [x] Live matrix scenario proves cannibal contact never opens bandit shakedown/pay/fight surfaces, with any bandit control run labeled separately (`20260428_124947` reaches local contact/attack report path with `shakedown=no`; no bandit control run credited).
- [x] Live matrix scenario proves saved-state persistence for active cannibal group/profile/target/live-signal state when a real active group is created (`20260428_130948`, feature-path, guarded save/writeback mtime, active members `[4,5]`, player target, `known_recent_marks` with `live_smoke@...`; no-fixture reload support `20260428_131031`; `intelligence_map.leads=[]` is out of scope, not credited).
- [x] Repeatability or fixture-bias check is run after the first passing product scenario (`repeatability --count 2 cannibal.live_world_day_smoke_pressure_mcw`, stable pass, runs `20260428_125319` and `20260428_125342`).
- [x] Any later harness/product proof reaches the real dispatch/local-contact path and does not close from evaluator-only tests for the scenarios marked green above; remaining open scenario(s) keep the same burden.

Notes:
- Deterministic/code substrate landed in `src/bandit_live_world.cpp`, `src/bandit_live_world.h`, `src/do_turn.cpp`, and `tests/bandit_live_world_test.cpp`: pack-size dispatch, smoke/light nearby-lead classification, live current-exposure feeding into the local gate, darkness/concealment local-gate input, current/recent exposure hold-off, bounded sight-avoid reposition helper proof, no-extort/no-shakedown reporting, high-threat abort, and multi-member save/load proof.
- Evidence: `git diff --check`; `make -j4 obj/do_turn.o obj/bandit_live_world.o tests/bandit_live_world_test.o LINTJSON=0 ASTYLE=0`; `make -j4 tests LINTJSON=0 ASTYLE=0`; `./tests/cata_test "[bandit][live_world][cannibal]"`; `./tests/cata_test "[bandit][live_world][sight_avoid]"`; `./tests/cata_test "[bandit][live_world][approach_gate]"`; `./tests/cata_test "[bandit][live_world]"`.
- Imagination source lives at `doc/cannibal-camp-night-raid-imagination-source-of-truth-2026-04-28.md`.
- Code audit lives at `doc/cannibal-camp-night-raid-code-audit-2026-04-28.md`.
- Canonical contract lives at `doc/cannibal-camp-night-raid-behavior-packet-v0-2026-04-28.md`.
- Live playtest matrix lives at `doc/cannibal-camp-night-raid-live-playtest-matrix-v0-2026-04-28.md`.
- Live feature-path evidence: day smoke pressure `.userdata/dev-harness/harness_runs/20260428_124902/`; night pack local-contact attack `.userdata/dev-harness/harness_runs/20260428_124947/`; exposed/recent-sight hold-off `.userdata/dev-harness/harness_runs/20260428_125138/`; daylight/high-threat negative `.userdata/dev-harness/harness_runs/20260428_135323/`; repeatability day smoke `.userdata/dev-harness/harness_runs/20260428_125319/` and `.userdata/dev-harness/harness_runs/20260428_125342/`.

---

## C-AOL harness trust audit + proof-freeze packet v0

Status: CLOSED / CHECKPOINTED PROCESS FREEZE

Success state:
- [x] A full harness-surface inventory exists and names each current primitive/scenario, its proof artifact, and its known blind spots: `doc/c-aol-harness-trust-audit-inventory-v0-2026-04-27.md`.
- [x] One canonical disposable audit save/profile is selected for the majority of checks, with explicit provenance and justified exceptions for any other fixture: provisional anchor `dev-harness` / `McWilliams` using `mcwilliams_live_debug_2026-04-07`, with transformed bandit and legacy dev fixtures called out as exceptions in the inventory doc.
- [x] Each audited primitive has a step ledger with precondition, action/keystroke, expected state, screenshot or metadata proof, failure rule, artifact path, and pass/yellow/red/blocked verdict. The proof-freeze matrix closes this for audited primitives; unaudited legacy scenarios remain explicitly untrusted rather than silently green.
- [x] The blocked inventory/deploy UI seam has better information extraction before retry: selector/menu trace and checked GUI/OCR guards now prove the selected `brazier`, `Deploy where?`, right/east key consumption, and guarded save prompt; saved-item metadata backs `deploy_furn -> f_brazier`; no terminal ASCII scrape was used as SDL GUI closure proof.
- [x] After that observation primitive landed, the current harness audit was rerun key by key; normal player-action brazier deployment only went green at `.userdata/dev-harness/harness_runs/20260427_222635/` after menu/selector state, uppercase-`Y` save/writeback, and saved east-tile `f_brazier` were all proven.
- [x] The audit covers launch/load readiness, focus/keystrokes, screenshot capture, debug spawn paths, target-tile metadata checks, save/zzip transforms, Smart Zone UI/layout inspection, fire/smoke setup, wait/time passage, NPC/bandit positioning, and report writing. Coverage means each surface has an explicit green/yellow/red/blocked verdict and claim boundary, not that every product feature is green.
- [x] False-pass behavior is guarded against: wrong screen, failed spawn, missing target field, missing save metadata, stale binary/profile, and load-only runs all produce explicit non-green verdicts. Report-classifier selftest covers load-only, stale-startup artifact-match, non-green step-ledger, yellow-wait, and blocked-wait false-pass cases; scenario-specific primitives without their own guards are explicitly non-green/untrusted, not latent closure evidence.
- [x] The frozen workflow states that load-and-close is startup proof only, and no feature package may close from it: see `doc/c-aol-harness-proof-freeze-matrix-v0-2026-04-28.md` and the C-AOL harness skill proof-freeze rules.
- [x] The relevant harness skill/docs are updated so another agent can run and audit the workflow without chat archaeology: the proof-freeze matrix is the repo reference and the C-AOL harness skill now names the step-ledger, claim-scoped artifact, metadata-only, save/writeback, debug target-state, map-editor, Smart Zone, and current boundary rules.
- [x] Frau Knackal or an equivalent review pass checks the proof matrix for contradictions, hidden fixture bias, and claims that outrun their evidence: Frau's process-altitude review found no major false-pass route after claim-scoped artifact, metadata-only, deferred-proof, and anti-fixture-bias tightenings were folded in.

Compact reference:
- Canonical contract lives at `doc/c-aol-harness-trust-audit-and-proof-freeze-packet-v0-2026-04-27.md`.
- Inventory checkpoint lives at `doc/c-aol-harness-trust-audit-inventory-v0-2026-04-27.md`.
- Frozen proof matrix lives at `doc/c-aol-harness-proof-freeze-matrix-v0-2026-04-28.md`.
- Closure review on 2026-04-28: no remaining process-audit seam blocks checkpoint closure. At that time, concrete product seams remained outside this lane: fuel Multidrop exact-`2x4` visibility was red/blocked, Smart Zone live layout was implemented-but-unproven/Josef-package, and real fire/bandit signal proof was blocked behind fuel. Later proof packets supersede those product seams where explicitly recorded below.
- Validation for closure review: `python3 tools/openclaw_harness/proof_classification_unit_test.py` (6 tests, OK).

---

## C-AOL actual playtest verification stack v0

Status: CLOSED / CHECKPOINTED READY FOR SCHANI REVIEW

Success state:
- [x] Fuel continuation behind the green brazier deploy gate has an honest old-path outcome: instrumented run `.userdata/dev-harness/harness_runs/20260427_231210/` keeps the chain non-green after the Multidrop `plank` filter redraw shows zero visible rows and `CONFIRM` blocks with `selected_stacks=0 total_selected_qty=0`; follow-up row-specific diagnostic `.userdata/dev-harness/harness_runs/20260427_232220/` names the current blocker as `blocked_untrusted_drop_filter_or_inventory_visibility` because filtered Multidrop has no selectable `typeid="2x4"` fuel row. No count selection, confirm-return, save request, post-fuel mtime/current-tile `2x4`/lighter/`fd_fire` proof is credited from that path.
- [x] Fuel writeback repair via wood source zone preserves the old-path non-proof history without reopening it: `.userdata/dev-harness/harness_runs/20260429_090634/`, `_093118/`, `_093509/`, `_095021/`, `_122807/`, `_122955/`, and final diagnostic `.userdata/dev-harness/harness_runs/20260429_142257/` remain blocker/postmortem evidence only; `fuel_writeback_source_zone_v0_2026-04-29` remains retired and must not be used for fire/lighter proof.
- [x] Fuel normal-map entry primitive is green at `.userdata/dev-harness/harness_runs/20260429_140645/`: setup footing and normal gameplay map UI are proven, with raw JSON/item-info/stale/debug screens red-blocked before action keys. Canonical contract: `doc/fuel-normal-map-entry-primitive-packet-v0-2026-04-29.md`.
- [x] Fuel writeback repair via wood source zone proves the visible/operational deployed-brazier gate at `.userdata/dev-harness/harness_runs/20260429_144805/`: normal Apply inventory -> filtered `brazier` -> `Deploy where?` -> east/right placement, look/OCR `hrazıer` + `burn things`, saved `f_brazier` + `log`, saved `SOURCE_FIREWOOD`, no lighter/fire keys.
- [x] Real player source-zone fire is green at `.userdata/dev-harness/harness_runs/20260429_153253/`: normal Apply inventory deployed `brazier`, normal Apply inventory selected charged `lighter`, exact `Light where?` UI trace opened before targeting, source-firewood prompt and recognizable ignition OCR were captured, save mtime advanced, and saved target tile contains `f_brazier` + `fd_fire`. Canonical proof: `doc/fuel-visible-brazier-source-zone-firestarter-action-v0-2026-04-29.md`.
- [x] Smart Zone Manager live layout verification first reached an honest non-green boundary: deterministic geometry/separation remained support only, the precise manual recipe was packaged in `/Users/josefhorvath/.openclaw/workspace/runtime/josef-playtest-package.md`, and current-runtime rerun `.userdata/smart-zone-safe-clean-20260427/harness_runs/20260428_001347/` was startup/load red only (`Dunn has no characters to load`) with no feature/layout proof credited. This historical boundary is superseded by the later green live coordinate-label proof recorded below.
- [x] Player-lit fire/bandit signal verification is green at `.userdata/dev-harness/harness_runs/20260429_162100/`: starting from the real player source-zone fire evidence at `.userdata/dev-harness/harness_runs/20260429_153253/`, fixture `player_lit_fire_bandit_signal_wait_v0_2026-04-29` adds mineral water plus an `AUTO_DRINK` zone, scenario `bandit.player_lit_fire_signal_wait_mcw` proves 30-minute bounded wait/time passage (`observed_delta_turns=1800`), same-run live smoke/fire signal scan/maintenance, `bandit_camp` dispatch with `candidate_reason=live_signal`, and saved active scout response metadata with `known_recent_marks` including `live_smoke@140,41,0`. Canonical proof: `doc/player-lit-fire-bandit-signal-verification-v0-2026-04-29.md`.
- [x] Roof-fire horde detection proof is green at `.userdata/dev-harness/harness_runs/20260429_180239/`: split-run fixture `roof_fire_horde_split_wait_from_player_fire_v0_2026-04-29` and scenario `bandit.roof_fire_horde_split_wait_from_player_fire_mcw` load the saved player-created roof-fire world from `.userdata/dev-harness/harness_runs/20260429_172847/`, re-audit roof/elevated `t_tile_flat_roof` + `f_brazier` + `fd_fire`, prove bounded time passage (`observed_delta_turns=300`), capture live roof-fire horde light-signal artifacts (`horde_signal_power=20`, `elevated_roof_exposed=yes`, `vertical_sightline=yes`), and save staged-horde response metadata (`destination_ms` retargeted to `[3360,984,1]`, `moves=8400`, `last_processed=5267242`). Product seam: `live_bandit_fire_source_is_elevated_roof_exposed` treats elevated `t_flat_roof` / `t_tile_flat_roof` fire sources as exposed roof signals. Boundary: `tracking_intensity` remained `0`, so the credited response is retarget/movement-budget metadata, not positive tracking-intensity proof. Canonical proof: `doc/roof-fire-horde-detection-proof-v0-2026-04-29.md`.
- [x] Bandit empty-camp retirement proof implements the conjunctive predicate: a camp/site retires from active AI calculations only when no home/inside live members or spawn-tile home headcount remain **and** no active dispatch/outbound/local-contact pressure remains. Deterministic coverage proves home-only, active-dispatch-only, unresolved returning-home/contact negative cases, plus the fully empty positive case and `retired_empty_site` report/dispatch/signal guards. Canonical contract: `doc/bandit-empty-camp-retirement-audit-mode-packet-v0-2026-04-28.md`.
- [x] Andi reports evidence class boundaries intact: the old Multidrop fuel path remains red/non-green at the visibility/writeback gate; the new fuel repair proves normal source-zone ignition + saved `fd_fire`; player-lit source-zone fire -> bandit signal response is green; roof-fire horde detection is green only through the split-run roof/elevated proof above; Smart Zone live layout is green only through the later reopened coordinate-label proof, not the old deterministic/startup attempts; performance rows are live-path performance-load evidence; the pre-staged three/four-site rows are not natural multi-site player-pressure proof; startup/load, deterministic support, live product proof, and unproven seams stay separately labeled.

Compact reference:
- Canonical contract lives at `doc/c-aol-actual-playtest-verification-stack-v0-2026-04-27.md`.

---

## Cannibal camp confidence-push live playtest packet v0

Status: COMPLETE / CONFIDENCE UPLIFT GREEN

Success state:
- [x] A compact live/harness matrix records the five proposed shapes and their current status: `doc/cannibal-camp-confidence-push-live-playtest-matrix-v0-2026-04-29.md`.
- [x] At least one broader wandering/day pressure run reaches the real live dispatch/signal path and supports the closed cannibal behavior claim without instant daytime suicide: `.userdata/dev-harness/harness_runs/20260429_013310/`, scenario `cannibal.live_world_day_smoke_pressure_mcw`, current window/runtime `782d8edabd`, 2/2 green step ledger, `feature_proof=true`, matched live smoke/signal path, `profile=cannibal_camp`, `darkness_or_concealment=no`, `shakedown=no`, `combat_forward=no`, and `signal_packet=live_smoke@`.
- [x] At least one night/contact run reaches real local-contact behavior and supports pack-forward danger under darkness/concealment: `.userdata/dev-harness/harness_runs/20260429_014900/`, scenario `cannibal.live_world_night_local_contact_pack_mcw`, current window/runtime `acfe6fd0ce`, 2/2 green step ledger, `feature_proof=true`, matched real local-contact pack-forward path, `profile=cannibal_camp`, `posture=attack_now`, `pack_size=2`, `darkness_or_concealment=yes`, `local_contact=yes`, `shakedown=no`, and `combat_forward=yes`.
- [x] Save/reload continuation proves active cannibal group/profile/target/job state stays coherent after reload: create/save feature proof `.userdata/dev-harness/harness_runs/20260429_021849/`, scenario `cannibal.live_world_day_smoke_persistence_mcw`, current runtime `e778902cac`, 7/7 green step ledger, guarded save mtime writeback, saved active `cannibal_camp` stalk group with active members `[4,5]`, target `player@140,41,0`, and `known_recent_marks` including `live_smoke@140,41,0`; paired no-fixture reload support `.userdata/dev-harness/harness_runs/20260429_021929/`, scenario `cannibal.live_world_day_smoke_persistence_reload_audit_mcw`, 2/2 green step ledger, saved-state metadata still present after fresh startup.
- [x] A different-seed/different-footing run reduces fixture-bias risk: `.userdata/dev-harness/harness_runs/20260429_022021/`, scenario `cannibal.live_world_exposed_sight_avoid_mcw`, current runtime `e778902cac`, 5/5 green step ledger, feature proof on fixture `cannibal_live_world_exposed_sight_avoid_v0_2026-04-28` from source footing `bandit_local_sight_avoid_exposed_v0_2026-04-27`, showing `profile=cannibal_camp`, `active_job=stalk`, `posture=hold_off`, `pack_size=2`, `recent_exposure=yes`, `sight_exposure=recent`, `shakedown=no`, `combat_forward=no`.
- [x] A bandit contrast control preserves the shakedown/pay/fight distinction while the credited cannibal day-pressure row remains no-shakedown/no-combat-forward: bandit run `.userdata/dev-harness/harness_runs/20260429_012915/` proves `pay_option=yes fight_option=yes` plus `shakedown_surface fight demanded=15797 reachable=45134`; cannibal day-pressure run `.userdata/dev-harness/harness_runs/20260429_013310/` proves `profile=cannibal_camp`, `darkness_or_concealment=no`, `shakedown=no`, and `combat_forward=no`. Night/contact no-shakedown is also covered by the checked night/contact row above.
- [x] The final verdict updates confidence honestly: confidence uplift green, with retained caveats that reload support is saved-file/startup support paired with create/save feature proof, and different-footing repeat reduces but does not eliminate fixture-bias risk.

Compact reference:
- Imagination source lives at `doc/cannibal-camp-confidence-push-live-playtest-imagination-source-of-truth-2026-04-28.md`.
- Canonical contract lives at `doc/cannibal-camp-confidence-push-live-playtest-packet-v0-2026-04-28.md`.
- This is confidence uplift for the closed cannibal behavior, not a behavior redesign or proof that the old closure failed.

---

## Smart Zone Manager harness-audit retry packet v0

Status: CLOSED / GREEN LIVE COORDINATE-LABEL PROOF AFTER REOPEN

Success state:
- [x] The current-runtime retry used clean startup/load hygiene before UI-entry checks.
- [x] Guarded screen-text rows prevented add-zone/filter/generation credit when `Zones manager` was not visible in the older blocked route.
- [x] Action-dispatch instrumentation identified the old agent-side primitive gap: delivered default `Y` reached `action_menu`, not `zones`, and no Zone Manager invocation trace fired.
- [x] The prior non-green result was classified as implemented-but-unproven / Josef playtest package instead of being laundered into green proof.
- [x] The reopened live UI path proves Zone Manager opened and Smart Zone generation was invoked: `.userdata/smart-zone-audit-live-20260429e/harness_runs/20260429_225644/` traces `invoke_zone_manager`, prompt acceptance, and `[harness][smart_zone] result success=1 placed_zones=23`.
- [x] The generated Zone Manager list is inspected after live generation and after close/save-changes/reopen; visible relative coordinate labels are captured in live `zone_manager_row` redraw traces with `visible_label` / `compact_label` fields.
- [x] Generated/reopened row metadata proves zone names/types plus absolute coordinates where available via `start_abs`, `end_abs`, and `center_abs`, showing a multi-position layout rather than one-tile collapse.

Compact reference:
- Imagination source lives at `doc/smart-zone-manager-harness-audit-retry-imagination-source-of-truth-2026-04-28.md`.
- Canonical contract lives at `doc/smart-zone-manager-harness-audit-retry-packet-v0-2026-04-28.md`.
- Final green run: `.userdata/smart-zone-audit-live-20260429e/harness_runs/20260429_225644/`, scenario `smart_zone.live_coordinate_label_proof_v0_tmp9` (stabilized as `smart_zone.live_coordinate_label_proof_v0`), `feature_proof=true`, `evidence_class=feature-path`, `verdict=artifacts_matched`, 18/18 green step-local rows.
- The proof verdict is green for the live coordinate-label/lumping claim. Screenshot/OCR artifacts support the UI checkpoints but OCR is fallback-quality; row trace metadata is the decisive coordinate-label evidence. Full process-reload disk persistence is not separately claimed.

---

## Generic clean-code boundary review packet v0

Status: CLOSED / CHECKPOINTED REPORT-ONLY BOUNDARY REVIEW

Success state:
- [x] One compact boundary-review report exists and names the repo state it inspected (`doc/generic-clean-code-boundary-review-report-v0-2026-04-29.md`).
- [x] The report checks canon consistency across `Plan.md`, `TODO.md`, `SUCCESS.md`, `TESTING.md`, and `andi.handoff.md`.
- [x] The report checks build/test/lint risk at the appropriate level for the current boundary, or explains why a gate was not run.
- [x] Findings are concrete, anchored, and classified as `fix now`, `queue`, or `ignore/watch`.
- [x] No feature lane is reopened, promoted, or deprioritized without explicit Schani/Josef review.
- [x] Any proposed edits are bounded follow-up items, not silent cleanup drift.

Compact reference:
- Imagination source lives at `doc/generic-clean-code-boundary-review-imagination-source-of-truth-2026-04-28.md`.
- Canonical contract lives at `doc/generic-clean-code-boundary-review-packet-v0-2026-04-28.md`.
- Report-only boundary is complete. The only unpromoted queue/watch item is whether to later remove the gated `src/handle_action.cpp` dispatch trace hook; do not reopen Smart Zone live proof without a repaired UI-entry/key-delivery primitive or Josef manual evidence.

---

## C-AOL live AI performance audit packet v0

Status: COMPLETE / GREEN ENOUGH FOR CURRENT PLAYTEST SCALE

Success state:
- [x] A live/harness performance matrix exists for one, two, pre-staged three, and pre-staged four active hostile overmap AI cases in `doc/c-aol-live-ai-performance-matrix-v0-2026-04-29.md`; a true zero-site idle baseline is explicitly omitted by current scope, with the one-site row serving as the low-end live baseline unless Schani/Josef requests a separate idle comparison.
- [x] Each measured case names the profile/job mix, active site count, in-game elapsed window, wall-clock timing, and relevant counters/log fields. Credited rows: `.userdata/dev-harness/harness_runs/20260429_025639/`, `.userdata/dev-harness/harness_runs/20260429_032427/`, `.userdata/dev-harness/harness_runs/20260429_040926/`, and `.userdata/dev-harness/harness_runs/20260429_041936/`.
- [x] The measured path reaches the real live-game bounded-wait/`overmap_npc_move()` cadence and emits compact `bandit_live_world perf:` counters; evaluator-only, benchmark-only, fixture/setup, and startup/load evidence are classified as support only.
- [x] The code performance audit names hot-loop/scaling risks in `src/do_turn.cpp`, `src/bandit_live_world.cpp`, save/serialization, signal matching, sight/exposure checks, and report/log emission in the matrix code-audit notes.
- [x] No optimization is needed for the measured current envelope: the four-site row stayed compact (`total_us` min/median/max `540/560.0/5572`, sum `8360`, one dispatch-due row `dispatch_us=4654`, harness wall-clock `real 39.27s`), and natural three/four-site player-pressure remains a cap/watchlist behavior question rather than a measured performance bottleneck.
- [x] The final verdict is **green enough for current playtest scale**, with the boundary that pre-staged three/four-site rows are performance-load evidence, not natural multi-site player-pressure dispatch proof.

Compact reference:
- Imagination source lives at `doc/c-aol-live-ai-performance-imagination-source-of-truth-2026-04-28.md`.
- Canonical contract lives at `doc/c-aol-live-ai-performance-audit-packet-v0-2026-04-28.md`.
- Current matrix progress: one-site and two-site rows are green as natural/live dispatch footing; the three-site row is green only as a labeled pre-staged performance-load row (`.userdata/dev-harness/harness_runs/20260429_040926/`). The natural three-player-pressure recipe remains cap/watchlist evidence at `sites=3 active_sites=2`, not row closure.

---

## Bandit camp-map risk/reward dispatch planning packet v0

Status: CLOSED / SCOPED LIVE PRODUCT CHECKPOINT GREEN

Success state:
- [x] Camp-map implementation substrate landed: camp-owned `camp_intelligence_map`, save/load serialization, active target OMT persistence, scout-return writeback into the source camp map, live signal marks writing camp-map leads, and remembered camp-map lead selection through the live dispatch cadence/report path.
- [x] Deterministic/code support landed for two-OMT ordinary scout stand-off, 720-minute ordinary scout return clock, 2/4/5/7/10 roster/reserve sizing, active-outside dogpile blocking, wounded/unready and killed-member shrinkage, bounded stockpile pressure, high-threat non-escalation, prior defender-loss pressure, prior bandit-loss cooling, larger-than-scout stalk sizing, and no-opening hold/return.
- [x] Feature-path evidence covers vanished-signal remembered-lead redispatch: `bandit.camp_map_vanished_signal_redispatch` in `.userdata/dev-harness/harness_runs/20260428_185947/`.
- [x] Feature-path evidence covers 2/5/10 variable-roster sizing: `bandit.variable_roster_tiny_dispatch_sizing_live` in `.userdata/dev-harness/harness_runs/20260428_202044/`, `bandit.variable_roster_dispatch_sizing_live` in `.userdata/dev-harness/harness_runs/20260428_192059/`, and `bandit.variable_roster_large_cooled_dispatch_sizing_live` in `.userdata/dev-harness/harness_runs/20260428_193621/`.
- [x] Feature-path evidence covers no-opening stalk-pressure decay: `bandit.stalk_pressure_waits_for_opening` in `.userdata/dev-harness/harness_runs/20260428_195617/`. This is only active remembered pressure -> no-opening rejection -> stale/decayed lead, not opening-present escalation.
- [x] Feature-path evidence covers bounded sight avoidance: `bandit.scout_stalker_sight_avoid_live` in `.userdata/dev-harness/harness_runs/20260428_173626/`.
- [x] Feature-path evidence covers high-threat/low-reward hold and same-fixture repeatability: `bandit.high_threat_low_reward_holds` in `.userdata/dev-harness/harness_runs/20260428_200600/`, repeated in `.userdata/dev-harness/harness_runs/20260428_211105/` and `.userdata/dev-harness/harness_runs/20260428_211153/` with `overall_verdict=stable_repeatability_pass`.
- [x] Feature-path evidence covers active-outside dogpile blocking: `bandit.active_outside_dogpile_block_live` in `.userdata/dev-harness/harness_runs/20260428_200434/`.
- [x] Feature-path evidence preserves shakedown/toll-control guardrails: `.userdata/dev-harness/harness_runs/20260428_204454/`, `.userdata/dev-harness/harness_runs/20260428_204630/`, and `.userdata/dev-harness/harness_runs/20260428_204813/`.
- [x] Optional empty-camp live sanity is green: `bandit.empty_camp_retirement_live` in `.userdata/dev-harness/harness_runs/20260428_214542/` proves the fully empty positive retires and home-side/active-outside negatives do not.
- [x] Product review accepted the scoped live/product matrix as enough for this checkpoint; no second-fixture bias variant is required before closure.

Compact reference:
- Canonical contract lives at `doc/bandit-camp-map-risk-reward-dispatch-planning-packet-v0-2026-04-28.md`.
- Andi lane draft lives at `doc/bandit-camp-map-risk-reward-dispatch-andi-lane-v0-2026-04-28.md`.
- Bandit live product matrix lives at `doc/bandit-live-product-playtest-matrix-v0-2026-04-28.md`.

---

## Test-vs-game implementation audit report packet v0

Status: CLOSED / CHECKPOINTED

Success state:
- [x] A concrete report exists at `doc/test-vs-game-implementation-audit-report-2026-04-26.md` or an explicitly equivalent path.
- [x] The report covers the highest-risk bandit AI / camp lanes, including `tests/bandit_mark_generation_test.cpp`, `tests/bandit_playback_test.cpp`, `tests/bandit_live_world_test.cpp`, and the live dispatch path through `src/do_turn.cpp` / `src/bandit_live_world.cpp`.
- [x] The report separately classifies smoke, light, weather, horde attraction, site bootstrap, dispatch, local handoff, and scout behavior by evidence class.
- [x] Each audited pass condition says whether the tested logic is produced/consumed by live gameplay, deterministic playback only, harness setup only, or currently hollow/missing.
- [x] Every hollow/missing bridge is assigned to one of the greenlit packages or marked as a new follow-up if it does not fit.
- [x] `TESTING.md` gets a compact update summarizing the audit result and preserving the rule that tests cannot impersonate live implementation.
- [x] The report names the first implementation package Andi should execute next after the audit.

Compact reference:
- Canonical contract lives at `doc/test-vs-game-implementation-audit-report-packet-v0-2026-04-26.md`.
- Closed report lives at `doc/test-vs-game-implementation-audit-report-2026-04-26.md`.

---

## CAOL-BANDIT-LIVE-SIGNAL-SITE-BOOTSTRAP-v0 — Bandit live signal + site bootstrap correction v0

Status: SUPERSEDED / FUTURE-ONLY REMAINDERS

Success state:
- [x] Existing hostile overmap special families that should participate in live hostile-site logic can register abstract `bandit_live_world` site records without requiring the player to enter spawn/load range first.
- [x] Materialized NPCs reconcile back to the same owned-site ledger, preserving exact-member writeback behavior when concrete members exist.
- [x] The hard `distance <= 10` live-dispatch gate is removed or demoted so `10 OMT` ordinary visibility no longer impersonates the whole system range.
- [x] Real player source-zone fire -> bandit signal response is superseded/closed by `CAOL-REAL-FIRE-SIGNAL-v0` (`doc/player-lit-fire-bandit-signal-verification-v0-2026-04-29.md`).
- [ ] Remaining range-matrix/scoring/decay/hold-chill refinements are future-only unless explicitly promoted as a new bounded item.

Compact reference:
- Receipt lives in `doc/work-ledger.md` as `CAOL-BANDIT-LIVE-SIGNAL-SITE-BOOTSTRAP-v0`.
- Canonical contract lives at `doc/bandit-live-signal-site-bootstrap-correction-v0-2026-04-26.md`.
- This section used to look like an active reopened partial lane. It is not active now. Raw-field/synthetic source-hook proof remains historical support/non-credit for player-fire claims; real player-fire signal response is closed by the later actual-playtest proof bundle.

---

## Bandit live-wiring audit + visible-light horde bridge correction v0

Status: CLOSED / MOVED DOWNSTREAM

Success state:
- [x] Docs/canon clearly distinguish deterministic proof/playback behavior from live game behavior for bandit light, smoke, horde-pressure, and handoff claims.
- [x] The live visible-light-to-horde bridge is implemented in source and deterministically tested.
- [x] If implemented, the bridge calls the real horde signal path through bounded thresholds and reviewer-readable reports.
- [x] At least one deterministic test proves bridge thresholds and one live/harness proof shows a real light/fire source can affect a real horde signal path.
- [x] Existing bandit test claims are audited enough that no closed packet says “game does X” when only an authored proof packet does X.

Compact reference:
- Canonical contract lives at `doc/bandit-live-wiring-audit-and-light-horde-bridge-correction-v0-2026-04-26.md`.

---

## Bandit local sight-avoid + scout return cadence packet v0

Status: RECLOSED / CURRENT-RUNTIME PRODUCT PROOF

Success state:
- [x] Stalking / hold-off bandits in the reality bubble can detect current or recent exposure to the player or nearby camp NPCs and attempt a bounded reposition toward cover or broken line of sight. _(Deterministic coverage plus live/harness proof `.userdata/dev-harness/harness_runs/20260427_061344/`: `bandit_live_world sight_avoid: exposed -> repositioned npc=4 ... reason=repositioning because exposed`.)
- [x] The sight-avoid behavior is local and heuristic: deterministic tests prove it does not use magical future-cone omniscience and does not teleport.
- [x] A scout outing has a finite live sortie window and can return home after watching long enough, instead of lingering indefinitely in local contact. _(Deterministic/live-wired source path plus live/harness return-home decision proof `.userdata/dev-harness/harness_runs/20260427_051117/`.)_
- [x] Returned scout state writes back through the owned-site pressure/outing cleanup path without automatically conjuring a larger group. _(Deterministic writeback coverage plus live/harness follow-through `.userdata/dev-harness/harness_runs/20260427_054353/`: returned scout pressure refresh logged, active group/target/member ids and sortie clocks cleared, member `4` saved back on home footprint.)_ **Boundary:** this is not proof of the newer persistent camp-map lead contract; it does not by itself prove bounty/threat/confidence/age/source/outcome scout memory, vanished-signal redispatch, or risk/reward sizing from remembered scout knowledge.
- [x] The single-scout current behavior remains explainable: `scout` is still one member unless a later job or escalated decision explicitly requires more.
- [x] Reviewer-readable output distinguishes `still stalking`, `repositioning because exposed`, `returning home`, and `re-dispatch/escalation decision`. _(Source/report strings cover the branches; live proof now covers returning-home/writeback and `sight_avoid: exposed -> repositioned`. Later redispatch tuning is not claimed beyond explicit owned-site re-evaluation.)_
- [x] At least one live/harness proof uses `bandit.live_world_nearby_camp_mcw` or an equivalent real owned-site scenario and confirms the same code path would apply to a normal discovered bandit camp, while separately naming the harness bias that places the camp nearby on purpose.
- [x] Recent live logs/save fields from the 2026-04-27 McWilliams/Basecamp fixture are inspected and answer whether the scout timer was actually running before proof: copied-save inspection found `active_sortie_started_minutes=-1`, `active_sortie_local_contact_minutes=-1`, and no active group/target/job ids for the stale inspected site, so pre-aged closure was rejected.
- [x] Bandit hold-off/stalking placement keeps a meaningful stand-off instead of crowding the immediately adjacent/neighboring OMT-scale area unless an explicit shakedown/fight state has started. _(Deterministic five-OMT correction plus live harness proof `.userdata/dev-harness/harness_runs/20260427_152117/`: real `wait_action` 30m path completed, `local_gate ... posture=hold_off ... standoff_distance=5`, `live_dispatch_goal=140,46,0`.)_
- [x] The scout timeout/return path is proven on the current live-product path. _(Current-runtime live run `.userdata/dev-harness/harness_runs/20260427_154309/`: real `wait_action` path logged `scout_sortie: linger limit reached -> return_home`, `scout_sortie: home footprint observed ... pos=(140,51,0)`, and `scout_report: returned -> pressure refreshed`; copied save later showed a fresh redispatch, not the stale returning-home loop.)_

Compact reference:
- Canonical contract lives at `doc/bandit-local-sight-avoid-and-scout-return-cadence-packet-v0-2026-04-26.md`.

---

## Smart Zone Manager v1 Josef playtest corrections

Status: CLOSED / LIVE COORDINATE-LABEL PROOF GREEN AFTER REOPEN

Success state:
- [x] Smart Zone Manager adds `LOOT_MANUALS` coverage on/near the Basecamp books cluster without removing ordinary `LOOT_BOOKS` coverage.
- [x] Gun-magazine coverage remains `LOOT_MAGAZINES`, preferably with an unambiguous user-facing label such as "Basecamp weapon magazines" if label text is touched.
- [x] Auto-eat and auto-drink coverage spans the full Basecamp storage zone, with `ignore_contents` explicitly false so Basecamp sorting still sees the covered items.
- [x] Deterministic tests assert the actual zone ids/types and the `ignore_contents == false` option, not just label text.
- [x] Save inspection confirms generated zones remain saved/reopenable and do not crash or corrupt the camp layout through zone-manager serialize/deserialize proof.
- [x] Live/player-visible layout no longer lumps intended-separate generated zones onto a single tile. The reopened coordinate-label proof `.userdata/smart-zone-audit-live-20260429e/harness_runs/20260429_225644/` opens the real Zone Manager, accepts Smart Zone generation, reopens the generated list, and captures row labels/absolute coordinates across the layout.
- [x] Deterministic tests assert zone geometry/separation and the intended-overlap allowlist, not only zone ids/options.
- [x] The implementation respects the smart-zone aux-plan separation rules that are deterministic-checkable in the fixture: fire-source / `splintered` / wood placement, readable crafting/food/equipment support separation, clothing/dirty support, outside rotten placement, and a larger unsorted intake area.
- [x] Clean live/UI proof or a precise Josef playtest package demonstrates the corrected layout without rerunning the contaminated old McWilliams/bandit macro. The old playtest package is superseded for the lumping/layout claim by the green coordinate-label proof; full process-reload disk persistence remains a separate optional future audit if promoted.

Compact reference:
- Canonical contract lives at `doc/smart-zone-manager-v1-josef-playtest-followup-2026-04-26.md`.

---

## Basecamp medical consumable readiness v0

Status: CLOSED / CHECKPOINTED

Success state:
- [x] Camp locker/service logic recognizes the practical bandage family, including at least `bandages`, `adhesive_bandages`, and medical gauze / obvious bandage-like wound-care stock when item definitions expose it cleanly, as bounded medical readiness supplies when stocking NPCs.
- [x] NPCs can pick up roughly 10 pieces/charges of bandage-family stock from the relevant Basecamp/locker storage path without hoarding all medical items.
- [x] Existing carried bandage-family stock is preserved through locker refresh and counts toward the reserve cap instead of being discarded as clutter.
- [x] Non-medical loadout behavior, ammo/magazine readiness, and clothing/armor selection remain unchanged.
- [x] Deterministic tests cover fresh pickup, carried-item preservation, cap/anti-hoarding behavior, and a negative case for unrelated drugs/items.
- [x] Live/harness proof is not required for this first slice; deterministic camp/locker tests exercise the actual service path and the focused `[camp][locker]` regression pass covers readiness behavior.

Compact reference:
- Canonical contract lives at `doc/basecamp-medical-consumable-readiness-v0-2026-04-26.md`.

---

## Basecamp locker armor ranking + blocker removal packet v0

Status: CLOSED / CHECKPOINTED

Success state:
- [x] A generic helper scores candidate protective/full-body gear against currently worn blockers using body-part priority, protection/coverage, encumbrance, condition, and active locker policy.
- [x] The helper is not item-id-specific and does not special-case `RM13 combat armor`.
- [x] When a candidate is clearly superior, the locker path removes/drops the blocking worn items needed to equip it.
- [x] When a candidate is not clearly superior or cannot be equipped, the locker path stops retrying the same failed swap and does not produce visible repeated wear spam.
- [x] Existing superior-full-body and ballistic-maintenance tests are reused/extended, including positive and negative cases.
- [x] Tests prove a clearly superior full-body/protective suit can displace worse blockers, while stronger current ballistic armor is preserved against worse candidates.
- [x] At least one targeted regression covers the original symptom shape without depending on the exact RM13 item ID as the only proof.

Compact reference:
- Canonical contract lives at `doc/basecamp-locker-armor-ranking-blocker-removal-packet-v0-2026-04-26.md`.

---

## Basecamp job spam debounce + locker/patrol exceptions packet v0

Status: CLOSED / CHECKPOINTED

Success state:
- [x] Repeated Basecamp completion/missing-tool/no-progress messages are debounced by stable cause so they do not flood the visible log.
- [x] First occurrence and changed state still produce a visible/reportable message.
- [x] Locker-zone work has a typed exception path: real locker gear/readiness failures remain visible once with reason, while repeats are compressed.
- [x] Patrol-zone work has a typed exception path: real assignment/interruption/reserve/backfill changes remain visible once with reason, while repeats are compressed.
- [x] The debounce state does not survive in a way that hides messages forever after save/load or unrelated job changes.
- [x] Deterministic tests cover ordinary repeated spam, changed-state reset, locker exception, patrol exception, and a negative case showing unrelated important messages are not swallowed.
- [x] If practical, a harness/log proof shows the old spam shape is reduced without losing one meaningful locker/patrol message. Local deterministic/log-message gates were sufficient for this narrow message-debounce slice.

Compact reference:
- Canonical contract lives at `doc/basecamp-job-spam-debounce-exceptions-packet-v0-2026-04-26.md`.

---

## Bandit overmap/local handoff interaction packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded overmap/local handoff interaction packet exists on the current bandit playback / proof seam.
- [x] Deterministic proof shows overmap intent entering local play as posture rather than exact-square puppetry or stale route puppetry.
- [x] Deterministic proof shows local engagement, obvious danger, or contact loss can honestly rewrite, cool, or drop the prior posture instead of preserving immortal certainty.
- [x] At least one player-present context beyond a static toy camp is covered, for example basecamp pressure, follower travel, or vehicle / convoy interception.
- [x] Reviewer-readable output exposes the entry posture, any local rewrite, and the returned abstract-state change clearly enough to debug the seam without guessing from side effects.
- [x] The slice stays bounded: no full local combat AI rewrite, no coalition strategy layer, no broad visibility rewrite, and no magical omniscience by the back door.

Compact reference:
- Canonical contract lives at `doc/bandit-overmap-local-handoff-interaction-packet-v0-2026-04-21.md`.

---

## Bandit elevated-light and z-level visibility packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded elevated-light and z-level visibility packet exists on the current bandit playback / proof seam.
- [x] Deterministic proof shows nearby cross-z visibility behaves sensibly instead of collapsing into floor-bound amnesia.
- [x] Deterministic proof shows elevated exposed light can stay legible or actionable farther than ordinary hidden light under the right conditions without turning into magical global sight.
- [x] A flagship exposed-high-fire scenario, for example a radio-tower fire in a dead dark world, proves genuinely long-range visibility instead of timid toy-local range.
- [x] Matching deterministic scenarios prove the same meaningful elevated light can carry abstract zombie-horde pressure too instead of living in private bandit-only theater; this is **not** live horde attraction until the live light-to-horde bridge exists.
- [x] Deterministic proof shows smoke does **not** gain magical extra general reach merely from floor changes.
- [x] Reviewer-readable output exposes the visibility read and benchmark outcomes clearly enough that later playtesting can argue about tuning instead of first principles.
- [x] The slice stays bounded: no broad world visibility rewrite, no handoff redesign smuggled into the same packet, and no full zombie tactical sim.

Compact reference:
- Canonical contract lives at `doc/bandit-elevated-light-and-z-level-visibility-packet-v0-2026-04-21.md`.

---

## Bandit repeated-site revisit behavior packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded repeated-site follow-through exists on the current playback / evaluator footing, producing one more deliberate revisit / cautious-watch posture than plain early scout bookkeeping.
- [x] Deterministic coverage proves repeated same-site corroboration still does **not** unlock free `scavenge`, `steal`, or `raid` truth, and does not become immortal pressure.
- [x] Scenario `repeated_site_interest_stays_bounded` exposes the benchmark-facing long-horizon metrics reviewer-cleanly: `site_visit_count_500`, `site_revisit_count_500`, `cooldown_turn`, and `endless_pressure_flag`.
- [x] The honest `500`-turn proof shows the strengthened site cooling back out instead of regrowing forever.
- [x] The slice stays narrow: no site-type-sensitive branching, no settlement taxonomy pass, no broad visibility rewrite, and no z-level smuggling.

Compact reference:
- Canonical contract lives at `doc/bandit-repeated-site-revisit-behavior-packet-v0-2026-04-21.md`.

---

## Bandit overmap benchmark suite packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One complete named overmap benchmark-suite packet exists on the current bandit playback / proof seam.
- [x] Every required scenario carries a clear `100`-turn benchmark packet that is easy to read and easy to fail.
- [x] The scenarios that honestly need the longer horizon also carry `500`-turn carry-through checks instead of pretending `100` turns proves everything.
- [x] Reviewer-readable output explains why each scenario passed or failed, including timing / cadence / revisit counters where they matter, such as first non-idle turn, first actionable turn, first scout departure, first arrival, revisit count, and route-flip / back-and-forth count by turn `500`.
- [x] The explicit empty-frontier scenario proves that a camp with nothing useful nearby ventures out and increases frontier visibility through bounded scout/explore behavior instead of sitting forever, with the packet plainly reporting first scout, first arrival, visit/revisit totals, and obvious reversals.
- [x] The packet is reviewer-readable enough that Schani or Josef can answer plainly whether it is leiwand, actually fun, alive on the map, and showing real emergent activity rather than inert legal-but-boring behavior.
- [x] The slice stays bounded: no z-level implementation, no broad architecture rewrite, no vague benchmark theater, and no hand-waved passes when routing logic is still wrong.

Compact reference:
- Canonical contract lives at `doc/bandit-overmap-benchmark-suite-packet-v0-2026-04-21.md`.

---

## Bandit long-range directional light proof packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded long-range directional-light proof packet exists on the current bandit scenario / playback seam.
- [x] Deterministic multi-turn proof up to `500` turns shows the hidden-side leakage case stays non-actionable while the visible-side leakage case becomes actionable under the same broader footing.
- [x] The matching deterministic zombie-horde corridor variant proves the same light can carry abstract horde pressure too instead of existing in isolated bandit-only theater; this is **not** live horde attraction until the live light-to-horde bridge exists.
- [x] Each scenario carries explicit goals and tuning metrics, and reviewer-readable output shows whether those benchmarks were met.
- [x] The slice stays bounded: no z-level expansion, no broad light-system rewrite, no handoff redesign, and no fresh world-sim jump.

Compact reference:
- Canonical contract lives at `doc/bandit-long-range-directional-light-proof-packet-v0-2026-04-21.md`.

---

## Bandit overmap/local pressure rewrite packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded overmap/local pressure-rewrite proof packet exists on the current bandit scenario / playback footing.
- [x] Deterministic multi-turn proof shows a stalking or intercept posture can honestly cool, retreat, or reroute after local reality makes the original posture too dangerous.
- [x] Reviewer-readable output shows the pressure rewrite clearly enough to explain why the stale pursuit no longer holds.
- [x] Each scenario carries explicit goals plus scenario-specific benchmark hooks, and the later locked benchmark outcomes stay visible on the same report path.
- [x] The slice stays bounded: no broad handoff redesign, no tactical local combat AI expansion, and no fresh world-sim jump.

Compact reference:
- Canonical contract lives at `doc/bandit-overmap-local-pressure-rewrite-packet-v0-2026-04-21.md`.

---

## Bandit weather concealment refinement packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded weather-refinement packet exists on the current smoke/light mark-generation footing.
- [x] Deterministic coverage proves wind meaningfully fuzzes or de-precises smoke output instead of acting only as a token penalty.
- [x] Deterministic coverage proves portal-storm weather is handled explicitly for smoke and light instead of falling through as an unmodeled special case.
- [x] Reviewer-readable output explains how weather changed clue quality, for example reduced range, fuzzier origin, displaced/corridor-ish smoke read, or preserved bright-light legibility under dark storm conditions.
- [x] The slice stays bounded: no full plume physics, no global smoke sim, no sound-law rewrite, no z-level packet, and no broad visibility architecture rework.

Compact reference:
- Canonical contract lives at `doc/bandit-weather-concealment-refinement-packet-v0-2026-04-21.md`.

---

## Bandit bounded scout/explore seam v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One explicit bounded scout/explore option exists on the current dry-run evaluator seam, for map uncovering only.
- [x] Unreachable jobs still fail closed and do not auto-mint explore as a consolation prize.
- [x] Deterministic coverage proves the key bounded distinctions honestly: explicit explore can beat `hold / chill`, blocked routes do not create explore without explicit greenlight, and strong real reachable leads still outrank exploratory wandering.
- [x] Reviewer-readable dry-run output says plainly that the outing is explicit map uncovering and not accidental random wandering.
- [x] One small playback/reference scenario packet proves the same rule on the current scenario seam, with explicit goals and tuning metrics.
- [x] The slice stays bounded: no coalition logic, no fresh visibility family, no broad pathfinding rewrite, and no handoff expansion.

Compact reference:
- Canonical contract lives at `doc/bandit-bounded-scout-explore-seam-v0-2026-04-21.md`.

---

## Bandit concealment seam v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded concealment adapter exists on the current light signal seam, weakening outward light legibility when exposure is poor instead of inventing a second fake visibility machine.
- [x] Deterministic coverage proves the key bounded distinctions honestly: daylight suppression, weather penalty, containment, and side-dependent leakage/suppression.
- [x] Reviewer-readable output shows why a light-born mark was reduced, blocked, or allowed instead of hiding the answer in debugger soup.
- [x] The slice stays bounded: no broad all-signals concealment rewrite, no new fog-sound law, no global smoke/world simulation, no tactical stealth doctrine, and no pursuit/handoff expansion.
- [x] If the concealment adapter starts looking computationally suspicious, the packet carries one small readable cost/probe angle instead of deferring perf truth to later folklore.

Compact reference:
- Canonical contract lives at `doc/bandit-concealment-seam-v0-2026-04-21.md`.

---

## Bandit scoring refinement seam v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded scoring-refinement adapter exists on the current bandit dry-run/evaluator seam, refining how existing camp ledger state plus existing marks become job choice.
- [x] The threat side first inspects and collapses usable existing threat/danger footing instead of inventing a fresh bespoke threat ontology.
- [x] Deterministic coverage proves the key opportunism split honestly: bandits avoid strong opponents, but pounce when zombie pressure or other distraction lowers effective target coherence.
- [x] Reviewer-readable output shows the refined choice breakdown clearly enough to explain why a target was avoided, deferred, or exploited.
- [x] The slice stays bounded: no new visibility signal family, no broad heatmap/memory rewrite, no tactical zombie simulation, no coalition strategy layer, and no fresh world-sim expansion.

Compact reference:
- Canonical contract lives at `doc/bandit-scoring-refinement-seam-v0-2026-04-21.md`.

---

## Bandit moving-bounty memory seam v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded moving-bounty memory object exists for live `actor` or `corridor` style bounty, while structural bounty stays on site state instead of gaining chase memory.
- [x] Structural sites keep only cheap state such as harvested / recently-checked / false-lead / sticky-threat footing, with no stalking logic glued onto them.
- [x] Deterministic coverage proves the key bounded pursuit split honestly: live moving prey can be stalked briefly, but the memory collapses cleanly on lost contact, split, bad recheck, rising threat, or leash expiry instead of retrying forever.
- [x] Reviewer-readable output shows whether a moving lead was refreshed, narrowed, or dropped instead of hiding the memory state in debugger soup.
- [x] The slice stays computationally cheap: no per-turn tracking, no path-history scrapbook, no per-NPC biography graph, no endless retry loop, and no broad memory-palace world model.

Compact reference:
- Canonical contract lives at `doc/bandit-moving-bounty-memory-seam-v0-2026-04-21.md`.

---

## Bandit first 500-turn playback proof v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] At least one honest deterministic 500-turn bandit playback packet exists on the current abstract seams.
- [x] Reviewer-readable report output exposes long-horizon checkpoints and winner drift cleanly enough to inspect the 500-turn answers without debugger soup.
- [x] Deterministic coverage proves the chosen long-horizon scenarios stay bounded, including cooldown, peel-off, and repeated-site reinforcement behavior that does not magically harden into free raid truth.
- [x] The proof reuses the current mark-generation / playback / evaluator seams instead of smuggling in a broader overmap simulator or persistence rewrite.
- [x] The slice stays bounded: no new visibility adapter family, no live-harness-first theater, and no broad AI architecture jump.

Compact reference:
- Canonical contract lives at `doc/bandit-first-500-turn-playback-proof-v0-2026-04-20.md`.

---

## Bandit repeated site activity reinforcement seam v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] A bounded repeated-site reinforcement adapter exists from mixed repeated smoke/light/traffic footing into modest site-mark confidence and bounty amplification.
- [x] Reinforced site marks feed the current bandit mark-generation / playback / evaluator seams reviewer-cleanly instead of staying hand-authored lore.
- [x] Deterministic coverage proves the key bounded distinctions: mixed repeated signals reinforce one site mark cleanly, weak repetition does not fake durable settlement truth, self-corroboration stays bounded, and strengthened site interest still does not unlock free extraction jobs.
- [x] Reviewer-readable report output exposes the reinforcement packet and resulting mark/lead path instead of hiding the bridge in debugger soup.
- [x] The slice stays bounded: no smoke/light/human-route rewrite, no broad concealment implementation, no settlement-signature mythology, and no first 500-turn proof smuggled in.

Compact reference:
- Canonical contract lives at `doc/bandit-repeated-site-activity-reinforcement-seam-v0-2026-04-20.md`.

---

## Bandit human / route visibility mark seam v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] A bounded human / route adapter exists from direct sightings and repeated route-shaped activity, or equivalent deterministic route-visibility packets, into coarse overmap-readable moving-carrier, corridor, or bounded site signal state.
- [x] Generated human / route marks or leads feed the current bandit mark-generation / playback / evaluator seams reviewer-cleanly instead of staying hand-authored lore.
- [x] Deterministic coverage proves the key bounded distinctions: direct sighting versus repeated corridor activity, moving-carrier attachment versus site inflation, self-camp routine suppression, and at least one corroborated shared-route refresh case.
- [x] Reviewer-readable report output exposes the route packet and resulting mark/lead path instead of hiding the bridge in debugger soup.
- [x] The slice stays bounded: no light/smoke rewrite, no broad concealment implementation, no settlement-signature mythology, and no first 500-turn proof smuggled in.

Compact reference:
- Canonical contract lives at `doc/bandit-human-route-visibility-mark-seam-v0-2026-04-20.md`.

---

## Bandit light visibility mark seam v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] A bounded light-specific adapter exists from current local light and directional-exposure footing, or equivalent deterministic light packets, into coarse overmap-readable light signal state.
- [x] Generated light marks or light-born leads feed the current bandit mark-generation / playback / evaluator seams reviewer-cleanly instead of staying hand-authored lore.
- [x] Deterministic coverage proves the key bounded distinctions: daylight suppression, contained versus exposed night light, ordinary occupancy light versus searchlight-like threat light, and at least one side-dependent leakage case.
- [x] Reviewer-readable report output exposes the light packet and resulting mark/lead path instead of hiding the bridge in debugger soup.
- [x] The slice stays bounded: no smoke rewrite, no broad concealment implementation, no sound/horde expansion, and no first 500-turn proof smuggled in.

Compact reference:
- Canonical contract lives at `doc/bandit-light-visibility-mark-seam-v0-2026-04-20.md`.

---

## Bandit mark-generation + heatmap seam v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] A bounded overmap-side mark ledger and broad bounty/threat heat-pressure seam exist for deterministic bandit inputs.
- [x] Deterministic coverage proves mark creation, refresh, selective cooling, and sticky confirmed threat on named reference cases.
- [x] The existing evaluator / playback footing can consume generated mark output reviewer-cleanly instead of relying only on hand-authored leads.
- [x] The slice stays bounded: no bubble handoff, no broad visibility adapter, and no full hostile-world simulation are smuggled in.

Compact reference:
- Canonical contract lives at `doc/bandit-mark-generation-heatmap-seam-v0-2026-04-20.md`.

---

## Bandit overmap-to-bubble pursuit handoff seam v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded pursuit / investigation handoff exists from abstract overmap group state into local play.
- [x] The return path preserves meaningful abstract consequences such as updated mark/threat knowledge, losses, panic, cargo, or retreat state instead of dropping them on the floor.
- [x] Entry payload and return packet stay explicit, small, and reviewer-readable.
- [x] The slice stays bounded: no full raid / ambush suite, no broad tactical AI rewrite, and no full per-bandit biography persistence are smuggled in.

Compact reference:
- Canonical contract lives at `doc/bandit-overmap-to-bubble-pursuit-handoff-seam-v0-2026-04-20.md`.

---

## Locker lag-threshold probe v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One honest threshold packet exists for the real `CAMP_LOCKER` service path.
- [x] The packet distinguishes top-level item pressure from worker-count pressure instead of flattening them together.
- [x] The result can name an approximate fine / suspicious / bad range, or honestly report that no clear threshold was found within the tested bound.
- [x] If the threshold looks bad, the packet ends with a small cheap-first guardrail recommendation order instead of architecture opera, and if it does not, the packet says so plainly.

Compact reference:
- Canonical contract lives at `doc/locker-lag-threshold-probe-v0-2026-04-20.md`.

---

## Bandit evaluator dry-run seam v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] A deterministic dry-run evaluator exists for controlled bandit camp inputs.
- [x] The evaluator always includes `hold / chill` and only emits outward candidates from real compatible leads or current hard state.
- [x] The first explanation surface shows leads considered, the full candidate board, per-candidate score inputs/final score, veto/soft-veto reasons, and winner versus `hold / chill`.
- [x] Narrow deterministic coverage exists for the first pure reasoning reference cases.
- [x] The slice stays bounded: no full autonomous bandit world behavior, no broad scenario playback suite, and no broad persistence architecture are smuggled in.

Compact reference:
- Canonical contract lives at `doc/bandit-evaluator-dry-run-seam-v0-2026-04-20.md`.

---

## Bandit scenario fixture + playback suite v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] Named deterministic bandit scenarios exist for the first reference cases.
- [x] The playback contract can inspect behavior at multiple checkpoints such as `tick 0`, `tick 5`, `tick 20`, and one longer horizon.
- [x] The scenario packet can answer whether camps stay idle, investigate smoke, stalk edges, peel off under pressure, or mis-upgrade whole regions from moving clues.
- [x] The suite stays bounded and does not turn into broad worldgen mutation or live-harness-first theater.

Compact reference:
- Canonical contract lives at `doc/bandit-scenario-fixture-playback-suite-v0-2026-04-20.md`.

---

## Bandit perf + persistence budget probe v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] Repeatable cost measurements exist for the named bandit scenarios.
- [x] Obvious evaluator churn signals such as candidate-count growth, repeated scoring/path checks, or similar waste are visible instead of hidden.
- [x] Save-size growth has an honest first estimate tied to the actually persisted bandit state shape.
- [x] The packet can say whether the current design looks cheap enough, suspicious, or clearly too bloated before broader rollout.

Compact reference:
- Canonical contract lives at `doc/bandit-perf-persistence-budget-probe-v0-2026-04-20.md`.

---

## Locker clutter / perf guardrail probe v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One honest measurement packet exists for the real `CAMP_LOCKER` service path across a first bounded clutter sweep such as `50 / 100 / 200 / 500 / 1000` top-level items.
- [x] The packet distinguishes realistic worker-count pressure from item-hoard pressure instead of pretending twenty-to-fifty camp workers are the main threat.
- [x] The packet answers whether loaded magazines and ordinary container shapes mostly behave like one top-level locker item or create meaningful nested-content pain.
- [x] The packet can end with a usable verdict: fine for now, watch this, or land a guardrail now, plus the first cheap mitigation order if needed.

Compact reference:
- Canonical contract lives at `doc/locker-clutter-perf-guardrail-probe-v0-2026-04-20.md`.

---

## Controlled locker / basecamp follow-through packet

Status: CHECKPOINTED / PACKAGE 5 DONE FOR NOW

Success state:
- [x] **Package 1, harness zone-manager save-path polish** is landed with screenshots/artifacts from the current McWilliams harness path.
- [x] **Package 2, basecamp toolcall routing fix** is landed or honestly blocked, and the right discriminator is separated from the bad location-only heuristic.
- [x] **Package 3, locker outfit engine hardening** is now landed for the current required closeout slice: the weird board/log leak stays re-proved live on the rebuilt current tiles binary, the in-game-log plus `llm_intent.log` observability helper is exercised on that same live probe path, and the required deterministic/service-parity canon is closed through the outer one-piece civilian-garment seam (`abaya`) without reopening open-ended clothing-case collection.
- [x] **Package 5, basecamp carried-item dump lane** is landed: ordinary carried junk gets dumped during locker dressing, the kept carried lane is intentionally limited to `bandages`, `ammo`, and `magazines`, and curated locker stock is not polluted by the dump behavior.
- [x] The queue stayed controlled while Package 5 ran, and the next slice can now move forward cleanly as Package 4 instead of broadening into unrelated lanes.

Compact reference:
- Detailed aux/proof references are in git history before `CAOL-DOC-HYGIENE-POST-STALKER-v0`; no current lane depends on this closed receipt.

---

## Patrol Zone v1

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] A Zone Manager patrol zone exists and patrol squares are grouped by 4-way connected clusters.
- [x] Patrol exists as a real camp job with its own priority surface.
- [x] Deterministic planner coverage exists for the reference staffing/topology cases.
- [x] The active patrol roster is chosen at shift boundaries from NPCs with patrol priority > 0.
- [x] On-shift patrol is sticky against routine chores.
- [x] Urgent disruption can break patrol, and reserve backfill works without full-roster reshuffle.
- [x] On-map behavior distinguishes hold-positions vs fixed-loop patrol in the intended simple v1 way.
- [x] Proportional live proof is recorded with separate screen/tests/artifacts reporting.
- [x] The player-legibility bar is met: guard behavior, uncovered posts, connected-vs-disconnected behavior, and reserve/off-shift state are understandable enough to read in play.
- [x] The result stays explainable as simple v1 patrol rather than quietly turning into smart-zone-manager soup.

Compact reference:
- Detailed aux/proof references are in git history before `CAOL-DOC-HYGIENE-POST-STALKER-v0`; no current lane depends on this closed receipt.

---

## Smart Zone Manager v1

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One explicit one-off smart-zoning action exists for Basecamp.
- [x] The v1 creates exactly one crafting niche, one food/drink niche, and one equipment niche.
- [x] Support placement exists for clothing, dirty, rotten, unsorted, and blanket/quilt-on-beds.
- [x] The corrected fire layout is implemented: fire tile = `SOURCE_FIREWOOD`, adjacent `splintered`, nearby wood.
- [x] Anchor selection is honest about the current shape: flag-first where the map exposes real signals, small explicit id allowlists where those signals are thin, and floor fallback instead of clever failure.
- [x] Existing sorting/subcategory machinery is reused by default, with only the deliberate v1 custom filters (`splintered`, `dirty`, `rotten`, `blanket`, `quilt`) kept as explicit exceptions.
- [x] Placement is deterministic and non-destructive by default.
- [x] Deterministic tests exist for anchor choice / zone choice / no-destructive-overwrite behavior.
- [x] Proportional live proof is recorded on the rebuilt current tiles binary.

Compact reference:
- Canonical contract lives at `doc/smart-zone-manager-v1-aux-plan-2026-04-06.md`.

---

## Locker-capable harness restaging

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] A fixture or restaging path exists that contains a real `CAMP_LOCKER` zone and suitable locker-state/NPC-state for `locker.weather_wait`.
- [x] `locker.weather_wait` is no longer blocked on missing fixture shape.
- [x] A fresh packaged `locker.weather_wait` run reports **screen** / **tests** / **artifacts** separately on the repaired fixture path.
- [x] The result is described reviewer-cleanly as harness/fixture work on existing locker behavior, not as premature delivery of the later chat/ambient feature lanes.

Compact reference:
- Detailed aux/proof references are in git history before `CAOL-DOC-HYGIENE-POST-STALKER-v0`; no current lane depends on this closed receipt.

---

## Stabilization + harness runway

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] The currently claimed locker baseline is honest enough to build on: V1/V2 stay trusted, and the active V3 slice is either properly evidenced or explicitly demoted back to the honest current state.
- [x] At least one reliable harness-driven live probe path exists on the current binary/profile/save path without stale-binary ambiguity.
- [x] That harness path reports screen/tests/artifacts as separate evidence classes instead of flattening them into one vague verdict.
- [x] At least one high-value reusable playtest scenario is documented/packaged for Schani-assisted probing instead of re-invented manually each time.
- [x] The packaged harness path no longer treats the first `lastworld.json` flip as sufficient proof of post-load gameplay readiness.
- [x] `chat.nearby_npc_basic` records recipient / artifact proof instead of only reaching dialogue and freeform-input UI.
- [x] A second named scenario contract (`ambient.weird_item_reaction`) exists and is honest about its current footing: runnable on the shipped fixture, still lacking real reaction/artifact proof.
- [x] At least one reusable scenario-setup helper exists so repeated probes stop depending on debug-menu folklore.
- [x] A compact Josef-facing testing packet exists for the pre-holiday active-testing window.

Compact reference:
- Detailed aux/proof references are in git history before `CAOL-DOC-HYGIENE-POST-STALKER-v0`; no current lane depends on this closed receipt.

---

## Locker Zone V2

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] Locker service can select up to two compatible magazines from locker supply when a managed ranged loadout needs them.
- [x] Locker service can reload supported ranged weapons from locker-zone ammo supply.
- [x] V2 ranged-readiness logic stays basic and deterministic instead of ballooning into V3 fashion/season nuance.
- [x] Deterministic coverage exists for the V2 ranged readiness / reload behavior.
- [x] Proportional runtime validation for V2 is recorded in `TESTING.md`.
- [x] Any Josef-specific follow-up checks are written down as non-blocking notes rather than treated as plan blockers.

Compact reference:
- Detailed aux/proof references are in git history before `CAOL-DOC-HYGIENE-POST-STALKER-v0`; no current lane depends on this closed receipt.

---

## Locker Zone V1

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] Older receipt preserved at final state `CHECKPOINTED / DONE FOR NOW`; no active validation target remains here.

Compact reference:
- Detailed aux/proof references are in git history before `CAOL-DOC-HYGIENE-POST-STALKER-v0`; no current lane depends on this closed receipt.

---

## Locker Zone V3

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] Seasonal dressing / winter-vs-summer wardrobe logic exists.
- [x] Weather-sensitive wardrobe choices (coats / blankets / shorts / similar) are handled deliberately rather than by V1/V2 shortcuts.
- [x] Per-NPC overrides / nuance exist without undoing the simpler V1/V2 deterministic spine.
- [x] Deterministic coverage exists for the V3 behavior that is actually implemented.
- [x] Proportional runtime validation for the currently implemented V3 behavior is recorded in `TESTING.md`.
- [x] Any Josef-specific follow-up checks are written down as non-blocking notes rather than treated as plan blockers.

Compact reference:
- Canonical contract lives at `doc/locker-zone-v3-reopen-packet-v0-2026-04-21.md`.

---

## Basecamp AI capability audit/readout packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded capability audit/readout packet exists for the current Basecamp AI surface.
- [x] The packet distinguishes player-facing spoken behaviors from internal structured actions/tokens instead of mushing them together.
- [x] The packet says plainly what board/job actions are actually supported now.
- [x] The packet says plainly whether any prompt-shaped interpretation layer still matters, or whether the current behavior is already mostly deterministic plumbing.
- [x] The packet is grounded in current code/tests/evidence strongly enough that later cleanup decisions do not rely on stale folklore.
- [x] The slice stays bounded and does not mutate into implementation work by accident.

Compact reference:
- Detailed aux/proof references are in git history before `CAOL-DOC-HYGIENE-POST-STALKER-v0`; no current lane depends on this closed receipt.

---

## Live bandit + Basecamp playtesting feasibility probe v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded live playtesting feasibility packet exists for current bandits plus current Basecamp footing.
- [x] The packet says plainly whether live playtesting is already practical on the current tree.
- [x] The packet says plainly whether overmap-bandit setup/spawn control is currently available, narrowly restageable, or still missing.
- [x] The packet separates screen/tests/artifacts cleanly instead of flattening them into one soup verdict.
- [x] Any blocker is stated as a concrete missing setup/control path rather than vague playtesting hand-wringing.
- [x] The slice stays bounded and does not turn into open-ended live playtesting theater.

Compact reference:
- Canonical contract lives at `doc/live-bandit-basecamp-playtesting-feasibility-probe-v0-2026-04-21.md`.

---

## Live bandit + Basecamp playtest packaging/helper packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded helper or first-class harness scenario exists for current-build live bandit + Basecamp restage on real existing Basecamp footing.
- [x] The helper packages the already-proved named-bandit restage seam instead of relying on remembered debug-menu choreography alone.
- [x] One fresh live run proves the helper can reach the intended setup repeatably and leaves reviewer-readable artifacts.
- [x] The packet says plainly what remains manual or ugly, if anything, instead of laundering it into magic.
- [x] The slice stays bounded and does not widen into fresh mechanics, zoning-mechanics reopen, encounter/readability judgment, or another generic feasibility lap.

Compact reference:
- Canonical contract lives at `doc/live-bandit-basecamp-playtest-packaging-helper-packet-v0-2026-04-22.md`.

---

## Bandit + Basecamp first-pass encounter/readability packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded first-pass live encounter/readability packet exists for current bandit + Basecamp footing on top of the packaged helper path.
- [x] The packet says plainly what the first hostile encounter actually looks and feels like on screen, including whether the player can read what is happening without debug folklore.
- [x] The packet distinguishes readable gameplay pressure from leftover helper/debug awkwardness instead of mushing them together.
- [x] Any blocker is stated concretely, for example spawn weirdness, posture confusion, unreadable hostility, dead pacing, or Basecamp-context ambiguity.
- [x] The packet leaves reviewer-readable artifacts strong enough that Schani can make the next higher-level product read without reinventing the probe from memory.
- [x] The slice stays bounded and does not widen into fresh mechanics, broad tuning, or speculative feature expansion.

Compact reference:
- Canonical contract lives at `doc/bandit-basecamp-first-pass-encounter-readability-packet-v0-2026-04-22.md`.

---

## Bandit + Basecamp playtest kit packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] The current named-bandit helper path has honest repeatability evidence instead of one lucky run.
- [x] The artifact/report surface for that helper path is screen-first and reviewer-readable enough that later playtest discussion does not depend on folklore.
- [x] The helper path cleans up after itself well enough that repeated probes do not leave stale game windows or obvious session clutter behind.
- [x] One purpose-built playtest fixture/save pack exists with a fast reinstall/load path for current bandit + Basecamp probing.
- [x] That fixture/save pack exposes a small named scenario family suitable for Josef playtesting and debugging rather than one brittle operator-only restage ritual.
- [x] The packet says plainly what still remains ugly or manual instead of laundering it into magic.
- [x] The slice stays cohesive and bounded rather than mutating into a general harness/world-authoring rewrite.

Compact reference:
- Canonical contract lives at `doc/bandit-basecamp-playtest-kit-packet-v0-2026-04-22.md`.

---

## Bandit + Basecamp playtest kit packet v1

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] A small named family of rich prepared-base fixtures exists for current Basecamp-management and bandit-interplay probing.
- [x] The fixtures are meaningfully management-ready rather than empty camp shells.
- [x] The player-side debugging/readability footing includes both clairvoyance mutations.
- [x] The fixture method is honest and reusable: captured save for structure, live restage for variants.
- [x] The scenario family is reviewer-readable enough that Schani, Andi, and Josef can tell which fixture is for which kind of playtest without folklore.
- [x] The packet says plainly what still remains manual, brittle, or not yet fixture-backed.
- [x] The slice stays about rich prepared fixtures rather than sprawling into a generic world-authoring platform.

Compact reference:
- Canonical contract lives at `doc/bandit-basecamp-playtest-kit-packet-v1-2026-04-22.md`.

---

## Bandit live-world control + playtest restage packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One live saveable owner exists for the relevant current bandit spawn families tied to this lane instead of leaving them as disconnected static-content / mapgen islands.
- [x] The owner keeps explicit per-site and per-spawn-tile bandit headcounts plus membership strongly enough that later control and writeback are honest rather than guessed.
- [x] The live system can actually control or dispatch those spawned bandits through the real world path instead of leaving them as disconnected `place_npcs` islands with post-hoc bookkeeping or whole-camp outings.
- [x] Dispatch size is derived from the site's live remaining population strongly enough that site-backed camps keep a home presence and later losses/shrinkage reduce future outing size instead of snapping back to folklore counts.
- [x] A bounded nearby restage path can seed a controlled bandit camp about `10 OMT` away on current build.
- [x] That restage path supports two honest modes: reviewer probe/capture may clean up, while manual playtest handoff leaves the game/session running instead of auto-terminating it.
- [x] The setup exercises the real overmap/bubble handoff plus local writeback path rather than a fake private theater.
- [x] Local outcomes change later world behavior instead of leaving the live owner stale, including later site size / dispatch capacity after kills, losses, returns, or partial contact.
- [x] A reviewer-clean perf report exists on that nearby live setup, including baseline turn time, bandit-cadence turn time, spike ratio, and max turn cost.
- [x] At least one dirtier later-world disturbance proof exists on that same nearby owned setup beyond the calm return->re-dispatch path, such as loss/missing shrinkage, save/load disturbance, or player-disruption without stale-roster reset.
- [x] The slice stays bounded: no giant generic map-authoring empire, no full hostile-human rewrite, no fake harness-only integration, and no faction-grand-strategy detour.

Compact reference:
- Canonical contract lives at `doc/bandit-live-world-control-playtest-restage-packet-v0-2026-04-22.md`.

---

## Multi-site hostile owner scheduler packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] The live hostile-owner seam can run `2-3` simultaneous hostile sites on one world without collapsing them into one shared fake camp brain.
- [x] Each hostile site keeps its own anchor/home site, roster/headcount, active outing or contact state, remembered pressure/marks, and persisted writeback state.
- [x] Site-backed camps still keep a home presence while losses continuously shrink later outings instead of snapping back to folklore counts.
- [x] One site's losses, return state, or contact pressure do not silently rewrite another site's state.
- [x] Repeated same-target pressure can overlap occasionally without turning into routine multi-site dogpile coalition behavior.
- [x] Save/load stays honest for multiple hostile owners at once instead of only for the single easiest happy-path site.
- [x] The slice stays bounded: no faction grand strategy, no dozens-of-families explosion, and no magical shared omniscience.

Compact reference:
- Canonical contract lives at `doc/multi-site-hostile-owner-scheduler-packet-v0-2026-04-22.md`.

---

## Hostile site profile layer packet v0

Status: CLOSED / CHECKPOINTED

Success state:
- [x] One shared hostile owner/cadence/persistence substrate exists with explicit profile rules instead of hardcoding everything to current bandit-camp assumptions.
- [x] At minimum one camp-style profile and one smaller hostile-site profile can coexist on that substrate reviewer-cleanly.
- [x] Dispatch, threat posture, return-clock, and writeback differences are profile-driven and readable rather than site-kind spaghetti.
- [x] The multi-site scheduler can consume those profiles without regressing the already-honest bandit live-owner footing.
- [x] The packet stays bounded: no giant faction-AI framework, no singleton stalker implementation, and no broad diplomacy/social-horror widening.

Compact reference:
- Canonical contract lives at `doc/hostile-site-profile-layer-packet-v0-2026-04-22.md`.

---

## Cannibal camp attack-not-extort correction v0

Status: CHECKPOINTED / DONE FOR NOW.

Success state:
- [x] Older receipt preserved at final state `CHECKPOINTED / DONE FOR NOW.`; no active validation target remains here.

Compact reference:
- Canonical contract lives at `doc/cannibal-camp-attack-not-extort-correction-v0-2026-04-24.md`.
- Canonical contract lives at `doc/cannibal-camp-first-hostile-profile-adopter-packet-v0-2026-04-22.md`.

---

## Bandit approach / stand-off / attack-gate packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One honest local approach / stand-off / attack-gate layer exists on top of the live bandit control seam.
- [x] Dispatched bandit groups can reviewer-readably choose among stalking, holding off, probing, opening a shakedown, attacking directly, or aborting.
- [x] The gate law is explicit enough that later packet work can answer `why did this become a shakedown instead of a fight` without folklore reconstruction.
- [x] Ordinary Basecamp/player pressure does not feel like instant psychic tile collapse because bandits can keep bounded stand-off behavior.
- [x] Convoy / vehicle / rolling-travel contexts are allowed to skip the polite shakedown posture when they honestly read as moving ambush opportunities on a real or harnessed travel seam.
- [x] The slice stays bounded: no pay-or-fight UI yet, no giant stealth doctrine, no radio/stalker widening, and no broad combat-AI rewrite.

Compact reference:
- Canonical contract lives at `doc/bandit-approach-stand-off-attack-gate-packet-v0-2026-04-22.md`.

---

## Bandit shakedown pay-or-fight surface packet v0

Status: CLOSED / CHECKPOINTED

Success state:
- [x] One honest player-present shakedown scene can bootstrap from the prior approach/gate packet instead of appearing as disconnected chat magic.
- [x] The initial interaction is clearly a robbery demand with a readable `pay` versus `fight` fork.
- [x] Whenever this shakedown surface is invoked, including any later reopened demand, fighting remains an explicit option rather than disappearing into one-way surrender theater.
- [x] The surrender surface uses an explicit bounded goods pool that matches scene context rather than magical remote inventory reach.
- [x] The demanded toll is painful enough to read like a real bandit shakedown instead of a decorative nuisance fee.
- [x] Paying can resolve the immediate scene without requiring perfect long-tail cargo simulation, because surrendered goods can collapse into bandit bounty/writeback honestly.
- [x] The slice stays bounded: no branching diplomacy opera, no fake debt economy, no magical remote inventory, and no unrelated convoy-combat rewrite.

Compact reference:
- Canonical contract lives at `doc/bandit-shakedown-pay-or-fight-surface-packet-v0-2026-04-22.md`.

---

## Bandit aftermath / renegotiation writeback packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One honest aftermath/writeback layer exists for player-present bandit shakedown outcomes.
- [x] Later bandit behavior can reflect scene results such as losses, wounds, loot haul, failed extraction, anger, or caution instead of resetting to folklore.
- [x] A materially weakened Basecamp defense, including a killed defender, can reopen the pressure once from a stronger bandit position with a higher demand when that is the honest read.
- [x] That harsher reopened demand still gives the player a fresh explicit `pay` versus `fight` reconsideration fork instead of hard-forcing only surrender or only combat.
- [x] The harsher reopened demand is explicit and reviewer-readable rather than hidden in vague score drift.
- [x] Bandit losses or panic can also cool or shrink later pressure, so the packet does not only ratchet cruelty upward.
- [x] The slice stays bounded: no infinite haggling loops, no giant diplomacy/reputation machinery, and no multi-camp retaliation grand strategy.

Compact reference:
- Canonical contract lives at `doc/bandit-aftermath-renegotiation-writeback-packet-v0-2026-04-22.md`.

---

## Bandit extortion-at-camp restage + handoff packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One named restage path can attract a real controlled bandit group toward Basecamp through the actual live owner/dispatch seam.
- [x] One reviewer probe/capture command and one manual handoff command exist for that same path.
- [x] The handoff path leaves the session alive at a genuinely useful extortion-at-camp moment instead of cleaning up before play starts.
- [x] The setup does not rely on fake debug-spawn theater or on moved-player/basecamp hacks that break current camp validity.
- [x] Reviewer-readable reports make it obvious which setup mode ran, which real controlled group was used, and whether the scene reached approach, stand-off, or shakedown state.
- [x] The slice stays bounded: no generic harness empire and no helper polish masquerading as the robbery chain itself.

Compact reference:
- Canonical contract lives at `doc/bandit-extortion-at-camp-restage-handoff-packet-v0-2026-04-22.md`.

---

## Bandit extortion playthrough audit + harness-skill packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One named audit/playthrough surface exists for the full Basecamp extortion chain rather than only for the first setup moment.
- [x] The packaged flow can cover first demand, fight branch, defender-loss reopen, raised-price second demand, and second `pay` versus `fight` reconsideration clearly enough for review.
- [x] Reports separate screen, tests, and artifacts/logs cleanly enough that later debugging does not devolve into folklore.
- [x] The relevant harness skill/docs are updated so another agent can discover the named paths and run them without archaeological guessing.
- [x] The slice stays bounded: no fake total-automation empire and no feature-redesign side quest hiding inside audit packaging.

Compact reference:
- Canonical contract lives at `doc/bandit-extortion-playthrough-audit-harness-skill-packet-v0-2026-04-22.md`.

---

## Bandit + Basecamp playtest kit packet v2

Status: FOLDED INTO LATER ACTIVE LANE / SUPPORTING ONLY

Success state:
- [x] Older receipt preserved at final state `FOLDED INTO LATER ACTIVE LANE / SUPPORTING ONLY`; no active validation target remains here.

Compact reference:
- Canonical helper contract still lives at `doc/bandit-basecamp-playtest-kit-packet-v2-2026-04-22.md`.
- The useful open scenario-surgery / observability work from `v2` was carried forward into `Bandit live-world control + playtest restage packet v0` instead of being killed.

---

## Post-Locker-V1 Basecamp follow-through

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] The live `DEBUG_LLM_INTENT_LOG` board/job artifact packet is made legible enough to stand beside the deterministic router proof.
- [x] The deterministic Basecamp board/job work is pruned/packaged into a cleaner upstream-ready shape.
- [x] The richer structured board/prompt treatment is extended beyond `show_board` in a deliberate next slice.
- [x] Proportional validation for each finished sub-slice is recorded in `TESTING.md`.
- [x] Any Josef-specific follow-up checks are written down as non-blocking notes rather than treated as plan blockers.

Compact reference:
- Detailed aux/proof references are in git history before `CAOL-DOC-HYGIENE-POST-STALKER-v0`; no current lane depends on this closed receipt.

---

## LLM-side board snapshot path

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] Routing proof exists on the real `handle_heard_camp_request` path.
- [x] Structured/internal `show_board` emits the richer handoff snapshot.
- [x] Spoken `show me the board` stays on the concise spoken bark path.
- [x] Deterministic evidence for that split is recorded.

Compact reference:
- Detailed aux/proof references are in git history before `CAOL-DOC-HYGIENE-POST-STALKER-v0`; no current lane depends on this closed receipt.

---

## Organic bulletin-board speech polish

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] Bulletin-board / camp-job requests can be triggered through natural player-facing phrasing instead of exposed machine wording.
- [x] Ordinary spoken answers no longer expose `job=<id>` / `show_board` / `show_job` style routing tokens.
- [x] Internal routing/debug structure can still exist where needed without leaking into normal in-world speech.
- [x] The visible answer tone sounds rough, practical, and in-world, like poor survivors making it work for another day while the dead and worse roam outside.

Compact reference:
- Canonical contract lives at `doc/organic-bulletin-board-speech-2026-04-09.md`.

---

## Combat-oriented locker policy

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] Future locker behavior strongly supports sensible common guard/combat gear: gloves, belts, masks, holsters, and the usual practical clothing/loadout pieces.
- [x] A bulletin-board / locker-surface bulletproof toggle exists and meaningfully shifts outfit preference toward ballistic gear.
- [x] Ballistic vest and plate handling becomes explicit enough to replace damaged (`XX`) ballistic components sensibly.
- [x] Clearly superior full-body battle/protective suits are preferred when appropriate instead of being split into worse piecemeal junk.
- [x] Future deterministic tests lean more toward combat/guard outfit behavior and less toward endlessly widening exotic garment edge-case law.

Compact reference:
- Canonical contract lives at `doc/locker-combat-oriented-policy-2026-04-09.md`.

---

## Bandit concept formalization follow-through

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] A follow-through packet exists that turns the loose remaining bandit logic into three explicit doc slices: bounty source/harvesting/stockpile rules, cadence/distance/fallback rules, and cross-layer interactions/worked scenarios.
- [x] Those three slices are further decomposed into narrow single-question micro-items so Andi can freeze one law at a time instead of hiding several assumptions inside one paragraph.
- [x] The packet makes the no-passive-decay footing explicit: structural bounty changes via harvesting/exploitation, moving bounty via current activity and collection, threat via real recheck, and camp stockpile via explicit consumption instead of background decay math.
- [x] Each micro-item includes a clear question plus a concrete answer shape, and the packet as a whole includes starter numbers/tables, clear scope/non-goals, and enough worked examples that later implementation planning does not have to rediscover the control law from scratch.
- [x] The result remains conceptualization/backlog work only and does not silently greenlight bandit implementation.

Compact reference:
- Canonical contract lives at `doc/bandit-concept-formalization-followthrough-2026-04-19.md`.

---

## Plan status summary command

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] A user can request a plan readout and see compact numbered `active`, `greenlit`, and `parked` summaries derived from current `Plan.md` canon.
- [x] The greenlit readout preserves execution order, with active first and any bottom-of-stack entries simply appearing last in that numbered list.
- [x] If canon headings are contradictory or missing enough structure to classify cleanly, the command warns instead of inventing certainty.
- [x] The output stays short and Discord-friendly rather than dumping whole roadmap prose.

Compact reference:
- Canonical contract lives at `doc/plan-status-summary-command-2026-04-20.md`.

---

## Bandit smoke visibility mark seam v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] A bounded smoke-specific adapter exists from current fire/smoke/wind footing, or equivalent deterministic smoke packets, into coarse overmap-readable smoke signal state for bandit logic.
- [x] Generated smoke marks or smoke-born leads feed the current bandit mark-generation / playback / evaluator seams instead of relying on hand-authored smoke lore.
- [x] Deterministic coverage proves the bounded long-range rule honestly: sustained clear-weather smoke stays meaningfully long-range, while weak/brief or degraded-condition smoke stays shorter and fuzzier.
- [x] Reviewer-readable report output exposes the smoke packet and resulting mark/lead path instead of hiding the bridge in debugger soup.
- [x] The slice stays bounded: no light/searchlight adapter, no broad visibility/concealment implementation, no global offscreen smoke sim, and no first 500-turn proof smuggled in.

Compact reference:
- Canonical contract lives at `doc/bandit-smoke-visibility-mark-seam-v0-2026-04-20.md`.

---

## Bandit overmap AI concept chain

Status: CHECKPOINTED / COHERENT PARKED SUBSTRATE

Success state:
- [x] The parked concept packet defines the broad bandit overmap actor model, signal/memory role, threat shape, and overmap-to-bubble intent cleanly enough that later planning no longer has to rediscover the premise from scratch.
- [x] The broad packet explicitly states that bounty and threat do not passively decay: structural bounty changes through harvesting/exploitation, moving bounty through fresh activity and collection, threat through real recheck, and camp stockpile through explicit consumption.
- [x] Deterministic bounty/threat scoring guidance exists with explicit camp-ledger inputs, map-mark fields, harvest/collection/recheck rules, and job-scoring logic.
- [x] Overmap mark-generation and heatmap guidance exists with explicit overmap-only mark creation, recheck/harvest rules, and threat/bounty field update logic on the shared cadence family without passive decay.
- [x] Bidirectional overmap-to-bubble handoff guidance exists with explicit entry modes, return-path state, collapse-back rules, and reuse of existing pursuit/noise-routing footing where possible.
- [x] Supporting physical-systems recon exists so the visibility/concealment slice is grounded in existing smoke, light, sound, and weather hooks instead of made-up parallel physics.
- [x] Player/basecamp visibility and concealment guidance exists with explicit signal sources, environmental filters, bounty/threat interpretation outputs, repetition/reinforcement rules, and player/basecamp exposure-reduction levers.
- [x] The visibility item explicitly allows fog/mist to suppress sight/light legibility without requiring new fog-based sound dampening rules unless later evidence says otherwise.
- [x] The broad packet explicitly prevents suspicion spirals by making camp knowledge sparse and camp-owned, ground bounty coarse/finite/non-regenerating, moving bounty actor-driven, and checked/false-lead/harvested memory able to damp repeated interest.
- [x] The broad packet explicitly resolves basecamp fairness asymmetry by allowing sustained offscreen pressure and stalking, keeping decisive full camp assault player-present only for current scope, and not requiring attack presignaling as the fairness mechanism.
- [x] The packet explicitly names the current overmap-NPC persistence/travel/companion substrate as reusable footing for stalking and intercept pressure, without pretending the existing need-driven random-NPC policy is already the finished hostile model.
- [x] The broad packet explicitly resolves handoff identity continuity by making the overmap group itself persistent, carrying only a small anchored-individual slice directly across handoffs, and treating the rest of the group as fungible mission strength.
- [x] The broad packet explicitly treats smoke/mark destinations as provisional mission leads whose goals can be continued, diverted, shadowed, or aborted by local observations instead of sacred tile commitments.
- [x] The broad packet explicitly resolves city opportunism under zombie pressure by allowing occasional risky opportunism without requiring direct bandit-versus-zombie tactical simulation, while keeping repeat attractiveness bounded by depleting bounty and sticky threat memory.
- [x] The broad packet explicitly keeps threat marks sticky enough that bands do not cheaply remote-rewrite a scary area as safe again until they return close enough to genuinely reassess it.
- [x] The broad packet explicitly resolves multi-camp dogpile behavior by keeping camps mostly independent in v1, allowing occasional overlap without turning it into routine coalition swarming.
- [x] The broad packet explicitly uses territoriality, distance burden, depletion, sticky threat, and fresh active-pressure penalties to damp repeated multi-camp convergence on the same target region.
- [x] The whole bandit concept packet is now coherent enough that it can support bounded promotion into greenlit implementation slices without hidden open seams.

Compact reference:
- Detailed aux/proof references are in git history before `CAOL-DOC-HYGIENE-POST-STALKER-v0`; no current lane depends on this closed receipt.

---

## Arsenic-and-Old-Lace social threat and agentic-world concept bank

Status: CHECKPOINTED / FAR-BACK PARKED SUBSTRATE

Success state:
- [x] One auxiliary concept-bank doc preserves the current speculative C-AOL feature families cleanly enough that later planning no longer has to rediscover them from scattered chat.
- [x] The preserved bank explicitly separates nearer playable threat/control seeds from broader social-horror and weird-world systemic ideas instead of flattening everything into one soup.
- [x] The bank explicitly preserves alarm states with natural-language yelling control, radio information warfare, writhing-stalker pressure, bounded sight avoidance, feral-camp pressure, social camouflage, hidden-psychopath survivor play, faction mythmaking, living camp mood/fracture, agentic NPC goals, and interdimensional-traveler motive hooks.
- [x] `Plan.md` points to the bank as a far-back parked concept substrate rather than as an active or greenlit implementation lane.
- [x] The preserved bank explicitly states that current bandit and Basecamp zoning footing still needs honest playtesting before these concepts earn bounded promotion.
- [x] The bank explicitly requires future revisit to happen one bounded promotion at a time instead of reopening fifty speculative threads at once.

Compact reference:
- Detailed aux/proof references are in git history before `CAOL-DOC-HYGIENE-POST-STALKER-v0`; no current lane depends on this closed receipt.

---

## Plan/Aux pipeline helper

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] A small helper can take a proposed item/greenlight and print the contract back for verification before canon files are changed.
- [x] The helper can collect corrections and then classify the item cleanly as active, parked, or bottom-of-stack.
- [x] The helper can update the relevant canon files consistently (`Plan.md`, `TODO.md`, `SUCCESS.md`, `TESTING.md` when needed, plus the auxiliary doc).
- [x] The helper reduces manual file carpentry for already-understood greenlights without bypassing the frozen workflow.
- [x] The helper can optionally generate the Andi handoff packet from the same classified contract.

Compact reference:
- Canonical contract lives at `doc/plan-aux-pipeline-helper-2026-04-09.md`.

---
