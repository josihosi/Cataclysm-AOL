# Plan

Canonical roadmap for Cataclysm-AOL.

This file answers one question: **what should the agent advance next?**
It is not a changelog, not a graveyard of crossed-off substeps, and not a place to preserve every historical detour.
Rewrite it as reality changes.

## File roles

- **Plan.md** - canonical roadmap and current delivery target
- **SUCCESS.md** - success-state ledger / crossed-off exit criteria for roadmap items
- **TODO.md** - short execution queue for the current target only
- **TESTING.md** - current validation policy, latest relevant evidence, and pending probes
- **TechnicalTome.md** - durable mechanic notes, not daily state tracking
- **doc/work-ledger.md** - compact receipt book for asks, state transitions, evidence links, and owners
- **COMMIT_POLICY.md** - checkpoint rules to prevent repo soup

If these files disagree, **Plan.md wins** and the other files should be repaired.

## Working rules for agents

- Do **not** mechanically grab the first unchecked-looking thing from some list.
- Follow the current delivery target below and move it to its **next real state**.
- Josef being unavailable for playtesting is **not** a blocker by itself.
- When a target is waiting on Josef, move to the next best unblocked target.
- If no good unblocked target remains, send Josef a short parked-options note so he can greenlight the next lane; do not just keep revalidating the old packet.
- During the current debug-proof finish stack, failed agent-side proof does **not** close or park implemented code. After the attempt budget, move implemented-but-unproven items to Josef's playtest package and continue the next greenlit debug note.
- Prefer batching human-only asks where practical. One useful packet with two real product questions beats two tiny pings.
- Keep these files lean. Remove finished fluff from `TODO.md` and `TESTING.md` instead of piling up crossed-off archaeology.
- Each real roadmap item needs an explicit success state in `SUCCESS.md` (or an equally explicit inline auxiliary) so completion is visible instead of guessed.
- Cross off reached success-state items; only remove the whole roadmap item from `Plan.md` once its success state is fully crossed off / done.
- Prefer agent-side playtesting first. Josef should be used for product judgment, feel, priority calls, or genuinely human-only checks.
- Validation should match risk:
  - docs-only change -> no compile
  - small local code change -> narrow compile/test
  - broad or risky code change, or a Josef handoff -> broader rebuild / startup harness as needed
- Follow `COMMIT_POLICY.md`. Do not let the repo turn into one giant dirty blob.

---

## Current status

Repo policy remains unchanged: `/Users/josefhorvath/Schanigarten/Cataclysm-AOL` on `dev` is the normal worktree and `josihosi/Cataclysm-AOL` is the real project/release repo. `josihosi/C-AOL-mirror` is green-dot-only.

`doc/work-ledger.md` is now the compact receipt book for meaningful asks, state changes, evidence links, owners, supersessions, held lanes, and red/non-credit proof. Use it before trimming active docs.

Detailed contracts, closure evidence, and older checkpoint history belong in `doc/*.md`, `doc/work-ledger.md`, `SUCCESS.md`, and git history. Keep this file short enough that the active stack is visible without archaeology.

---

## Active lane — CAOL-JOSEF-LIVE-DEBUG-BATCH-v0

**Status:** ACTIVE / GREENLIT DEBUG-PACKET STACK / PACKAGED FROM 2026-05-02 + 2026-05-03 JOSEF LIVE NOTES

Josef finished live testing and asked Schani to package yesterday's and today's debug notes into canon. This lane is the ordered correction stack for bandit shakedown/payment, defended-camp scout/standoff behavior, roof-z/multi-z dispatch stalls, big-camp escalation, all-light-source signaling, patrol aggression/alarm/report hygiene, writhing-stalker threat-drop timing, and NPC sorting retry debounce.

Imagination source: `doc/josef-live-debug-batch-imagination-source-2026-05-03.md`.

Contract: `doc/josef-live-debug-batch-packet-v0-2026-05-03.md`.

Handoff packet: `doc/josef-live-debug-batch-handoff-v0-2026-05-03.md`.

Test matrix: `doc/josef-live-debug-batch-test-matrix-v0-2026-05-03.md`.

Raw intake:
- `/Users/josefhorvath/.openclaw/workspace/runtime/caol-bandit-playtest-intake-2026-05-02.md`
- `runtime/josef-bandit-debug-intake-2026-05-03.md`
- `runtime/josef-locker-zone-debug-intake-2026-05-03.md`

Execution order: shakedown Pay/Fight + trade/debt correction; defended-camp scout/standoff/sight avoidance including bandit/cannibal stalking-mode LoS break and player smoke-out response; multi-z camp identity + roof/first-floor/`z=5` tower routing/throttle; hostile-camp post-scout escalation; all-light/smoke-source adapter; patrol aggression/alarm/report hygiene; writhing-stalker larger-distance/sight-avoid/threat-drop swoop plus boot-out rhythm; NPC sorting debounce; debug spawn horde/stalker/rider options; locker/basecamp equipment consistency; cannibal Monsterbone spear lore item.

Important correction: the 2026-05-02 scenic shakedown proof closed the wrong visible response contract (`Pay / Fight / Refuse`) after Josef had already reported that three choices were wrong and that Pay should open a trade/debt-style surface. Treat this as a reopened correction, not a new feature whim.

Current checkpoint: Slice 1 is green for visible Pay/Fight, actual NPC trade UI opening, whole basecamp-side payment pool, pool-derived debt/toll basis, cancel/refusal mapping, item-offer/autobalance, and paid writeback. The latest three-source proof row `.userdata/dev-harness/harness_runs/20260503_233823/` shows the same open `npc_trading::trade` / `trade_ui` Pay window containing player inventory (`binoculars`), basecamp-assigned NPC inventory (`saxophone`), and basecamp storage-zone inventory (`gold_watch`), then F1/autobalance + case-sensitive acceptance pays the debt (`shakedown_trade_ui result=paid demanded=33542 reachable=95834`). The rebuilt `7e5a506c76` master-profile rows `.userdata/master/harness_runs/20260503_213831/` and `.userdata/master/harness_runs/20260503_214015/` remain the clean master cancel/success receipts; the earlier tight `d1a4f076c8` proof rows remain supporting history. Credited rows are recorded in `doc/shakedown-pay-fight-npc-trade-ui-proof-v0-2026-05-03.md`. Slice 2 has enough current-pass evidence to continue after Josef's own tests: bandits did their thing. Keep deeper sight/smoke playtest as deferred validation/future hardening, not the next Andi blocker. Slice 3 roof/tower-z dispatch fallback is green at `doc/multi-z-roof-dispatch-fallback-proof-v0-2026-05-03.md`: deterministic multi-z identity/routing plus live `bandit.roof_z_dispatch_fallback_mcw` proof show the elevated target id is preserved while the live dispatch goal routes to reachable ground and avoids the old `route_missing` / empty-throttle silence. Slice 4 bandit hostile-camp toll escalation is green at `doc/hostile-camp-toll-escalation-proof-v0-2026-05-03.md`: deterministic roster/risk/cannibal gates plus live `bandit.hostile_camp_toll_escalation_live` run `.userdata/dev-harness/harness_runs/20260503_214648/` prove a scout-confirmed seven-member bandit camp dispatches a three-member `toll` party, preserves reserve, and records `shakedown_capable=yes` while pre-contact `shakedown=no`. Slice 5 all-light-source adapter is green only for source/weather/log adapter proof at `doc/all-light-source-live-adapter-proof-v0-2026-05-04.md`: deterministic/source coverage plus rebuilt live rows `.userdata/dev-harness/harness_runs/20260504_085531/` and `.userdata/dev-harness/harness_runs/20260504_090911/` prove fire/smoke plus terrain/furniture/item/vehicle light observations, screened leakage, searchlight semantics, raw weather/light-weather logging, and four exposed-light horde signal rows with `destination=` / `current_signal=yes` / `horde_signal_power=22`. Boundary: horde-response/devirtualization is not green; the guarded completed-save retry advanced/stabilized the player save, then `active_monsters=[]` and no nearby active `mon_zombie`. Current execution can continue to Slice 6 patrol aggression/alarm/report hygiene.

Boundary: debug correction stack only. Do not reopen unrelated closed predator/rider/locker/save-pack lanes, release packaging, full diplomacy, full vertical assault AI, broad sorting redesign, or a tile-perfect overmap light engine.

---

## Recent closed lane — CAOL-HARNESS-PORTAL-STORM-WARNING-LIGHT-v0

**Status:** CHECKPOINTED GREEN V0 / CLOSED / FRAU-ACCEPTED / HARNESS-HARDENING FOLLOW-UP

The visions sampler is relay-ready and waiting on Schani/Josef taste relay rather than more agent-side proof. Josef's harness portal-storm warning-light ask is now implemented/proven as a harness-hardening checkpoint.

Contract: `doc/harness-portal-storm-warning-light-packet-v0-2026-05-02.md`.
Proof: `doc/harness-portal-storm-warning-light-proof-v0-2026-05-02.md`.

Result: probe/handoff reports and repeatability summaries now surface a report-level `portal_storm_warning`; unallowed portal storms add a yellow contamination row to the step ledger and block silent green feature proof; explicitly allowed portal-storm scenarios stay green while retaining visible warning text; required portal-storm scenarios fail red if the required weather is missing/unknown.

Evidence: `python3 tools/openclaw_harness/proof_classification_unit_test.py` -> `Ran 13 tests ... OK`; `python3 -m py_compile tools/openclaw_harness/startup_harness.py tools/openclaw_harness/proof_classification_unit_test.py`; `git diff --check`.

Frau review: accepted green v0 from commits `74ef657057` / `8ea5546107`. Do not pull future runs back into portal-storm proof by ritual.

Boundary: harness-hardening only. Do not solve portal-storm gameplay, redesign weather, rerun old packets by ritual, or reopen closed bandit/visions/camp-locker lanes by drift.

---

## Recent closed lane — CAOL-ZOMBIE-RIDER-CLOSE-PRESSURE-NO-ATTACK-v0

**Status:** CLOSED / CHECKPOINTED GREEN V0 / BUGFIX + PRODUCT-FEEL FOLLOW-UP

Proof: `doc/zombie-rider-close-pressure-no-attack-proof-v0-2026-05-02.md`.

Result: the close-pressure no-attack seam is fixed. Root cause was the missing `aggro_character` bridge between `decision=bow_pressure reason=line_of_fire` planning and the monster gun actor's avatar-target gate. Current code marks the rider character-aggro before ready bow handoff and chooses named irregular bunny-hop/reposition pressure when too close, cooling down, blocked, or out of ammo.

Evidence: focused `[zombie_rider]` tests are green after the tainted-arrow follow-up (`207 assertions in 17 test cases`); `./just_build_macos.sh` relinked `cataclysm-tiles` for the original close-pressure checkpoint; fresh staged-but-live row `zombie_rider.live_open_field_pressure_mcw` -> `.userdata/dev-harness/harness_runs/20260502_050055/` remains behavior-shape footing for bow-pressure aggro bridge, ammo decrement, and close `too_close_bunny_hop` reposition. The current source lookup now checks `zombie_rider_tainted_bone_arrow` ammo instead of the pre-follow-up `arrow_wood` id. Caveat: staged-but-live McWilliams proof, not natural random discovery/full siege proof.

---

## Recent closed lane — CAOL-CAMP-LOCKER-ZONE-PLAYTESTS-v0

**Status:** CLOSED / CHECKPOINTED GREEN V0

Josef explicitly asked for locker zone playtests after the camp locker API-reduction lane closed. The bounded agent pass is complete and intentionally did not reopen open-ended locker ontology archaeology.

Proof/readout: `doc/camp-locker-zone-playtest-proof-v0-2026-05-02.md`.

Imagination source: `doc/camp-locker-zone-playtests-imagination-source-2026-05-02.md`.

Contract: `doc/camp-locker-zone-playtest-packet-v0-2026-05-02.md`.

Handoff packet: `doc/camp-locker-zone-playtest-handoff-v0-2026-05-02.md`.

Result: current-build harness feature proof is green after repairing the scenario evidence contract. `locker.zone_manager_save_probe_mcw` (`.userdata/dev-harness/harness_runs/20260502_041828/`) now has a green step-local ledger, matched Zone Manager UI-trace rows for `Basecamp: Locker` and created/reopened `Probe Locker` as `type="CAMP_LOCKER"`, and a saved-zone metadata audit for the persistent `Basecamp: Locker` zone. `locker.weather_wait` (`.userdata/dev-harness/harness_runs/20260502_041300/`) has a green step-local ledger and same-run `camp locker:` queued/plan/after/serviced artifacts proving real service from `locker_tiles=1` stock. `[camp][locker]` deterministic tests prove `NO_NPC_PICKUP`, off-zone, and policy-disabled boundary/exclusion guards. The Robbie e2e row remains blocked/no-credit, but it is not needed for this bounded v0 closure.

Boundary preserved: no API-reduction reopen, no broad locker/basecamp redesign, no Smart Zone/basecamp preset work, and no bandit/rider/stalker/raptor drift.

---

## Recent closed lane — CAOL-WRITHING-STALKER-PATTERN-TESTS-v0

**Status:** CLOSED / CHECKPOINTED GREEN V0

Schani promoted the queued writhing-stalker behavior-pattern follow-up into the next active lane after Josef clarified the desired rhythm: fair dread, not hidden-cheat goblin nonsense; repeated attacks with breathing room while healthy; withdrawal once badly injured.

Contract: `doc/writhing-stalker-behavior-pattern-minimap-packet-v0-2026-04-30.md`.

Imagination source: `doc/writhing-stalker-behavior-pattern-imagination-source-of-truth-2026-04-30.md`.

Proof: `doc/writhing-stalker-behavior-pattern-proof-v0-2026-04-30.md`.

Result: `tests/writhing_stalker_test.cpp` now has a compact deterministic behavior-pattern helper and trace rows covering no-magic/no-evidence, weak evidence decay, cover-route preference, light/focus counterplay, zombie-pressure guardrails, vulnerable-player strike windows, cooldown anti-spam, healthy repeated strikes, badly-injured retreat, and jitter/stuckness smell checks. No gameplay tuning/source behavior changed.

Evidence: `make -j4 tests/writhing_stalker_test.o tests LINTJSON=0 ASTYLE=0 && ./tests/cata_test "[writhing_stalker][ai]"` passed (`97 assertions in 8 test cases`); `./tests/cata_test "[writhing_stalker]"` passed (`129 assertions in 10 test cases`); trace extraction for `writhing_stalker_pattern_helper_traces_repeated_strikes_then_injured_retreat` passed and shows `shadow -> strike -> cooldown -> cooldown -> shadow -> strike -> withdraw` with retreat at `hp=50`.

Boundary: deterministic pattern proof is closed for v0. The live seam was not rerun because this slice changed tests only; existing live `monster::plan()` receipts remain the unchanged v0 game-path evidence. Do **not** reopen roof-horde, old writhing-stalker v0 closure, Smart Zone, old fire proof lanes, the multi-camp signal gauntlet, or this pattern packet without explicit Schani/Josef promotion.

---

## Recently closed lane — CAOL-WRITHING-STALKER-LIVE-FUN-SCENARIOS-v0

**Status:** CLOSED / CHECKPOINTED GREEN V0 LIVE FUN-SCENARIO PACKET

Josef explicitly greenlit live writhing-stalker fun scenarios on 2026-04-30: “YESSSS DO IT”. This lane follows the closed deterministic `CAOL-WRITHING-STALKER-PATTERN-TESTS-v0` and asks whether the stalker creates fair dread in live-shaped scenes instead of only passing tidy evaluator traces.

Contract: `doc/writhing-stalker-live-fun-scenarios-packet-v0-2026-04-30.md`.

Imagination source: `doc/writhing-stalker-live-fun-scenarios-imagination-source-of-truth-2026-04-30.md`.

Prior deterministic proof: `doc/writhing-stalker-behavior-pattern-proof-v0-2026-04-30.md`.

Closure proof: `doc/writhing-stalker-live-fun-scenario-proof-v0-2026-04-30.md`.

Green v0 result: staged-but-live McWilliams scenarios prove campfire/light counterplay, alley predator shadow routing, zombie distraction without magic, door/light escape, wounded-predator retreat, repeated strike/cooldown rhythm, and no omniscient target acquisition. Credited runs: `writhing_stalker.live_campfire_counterplay_mcw` -> `.userdata/dev-harness/harness_runs/20260430_233129/`; `writhing_stalker.live_alley_predator_mcw` -> `.userdata/dev-harness/harness_runs/20260430_233156/`; `writhing_stalker.live_zombie_distraction_mcw` -> `.userdata/dev-harness/harness_runs/20260430_233521/`; `writhing_stalker.live_zombie_distraction_no_magic_guard_mcw` -> `.userdata/dev-harness/harness_runs/20260430_233335/`; `writhing_stalker.live_door_light_escape_mcw` -> `.userdata/dev-harness/harness_runs/20260430_233405/`; `writhing_stalker.live_wounded_predator_mcw` -> `.userdata/dev-harness/harness_runs/20260430_233434/`.

Boundary: do **not** reopen monster flavor/stat/spawn footing, deterministic pattern closure, roof-horde, Smart Zone, old fire proof lanes, the multi-camp gauntlet, or these live fun rows without explicit Schani/Josef promotion. Remaining stricter fully-natural/manual discovery is future-only, not a blocker for v0.

Prior closed active lane: `CAOL-WRITHING-STALKER-PATTERN-TESTS-v0` — see `doc/writhing-stalker-behavior-pattern-proof-v0-2026-04-30.md`, `doc/work-ledger.md`, `SUCCESS.md`, and `TESTING.md`.

---

## Last closed active lane — CAOL-ZOMBIE-RIDER-0.3-v0

**Status:** CLOSED / CHECKPOINTED GREEN V0 INITIAL DEV

Josef greenlit preparing release `0.3` zombie rider for Andi, including playtests and a small map-AI funness benchmarking suite. The v0 initial-dev packet is now closed green.

Contract: `doc/zombie-rider-0.3-initial-dev-packet-v0-2026-04-30.md`.

Imagination source: `doc/zombie-rider-0.3-imagination-source-of-truth-2026-04-30.md`.

Raw intake / exact flavor text: `doc/zombie-rider-raw-intake-2026-04-30.md`.

Map-AI / funness benchmark suite: `doc/zombie-rider-map-ai-funness-benchmark-suite-v0-2026-04-30.md`.

Closure readout: `doc/zombie-rider-0.3-closure-readout-v0-2026-05-01.md`.

Result: endpoint zombie rider initial dev is green for v0: exact flavor text preserved, mature-world `GROUP_ZOMBIE` gate at `730 days`, large-body `SMALL_PASSAGE` counterplay, live-consumed local bow/withdraw/cover behavior, bounded overmap light interest and memory decay, capped rider convergence/band pressure, and five staged-but-live funness rows covering open-field terror, wounded disengagement, cover/LOS escape, lit-camp band circle-harass, and matched no-camp-light control.

Evidence: monster/evolution footing `d50715f00e`; local combat AI `4343dbdad1`; overmap light attraction `d2ffbd54c3`; convergence/band checkpoint `ce05ef44ec`; live rows `zombie_rider.live_open_field_pressure_mcw` -> `.userdata/dev-harness/harness_runs/20260501_013055/`, `zombie_rider.live_wounded_disengagement_mcw` -> `.userdata/dev-harness/harness_runs/20260501_014613/`, `zombie_rider.live_cover_escape_mcw` -> `.userdata/dev-harness/harness_runs/20260501_021656/`, `zombie_rider.live_camp_light_band_mcw` -> `.userdata/dev-harness/harness_runs/20260501_030432/`, and `zombie_rider.live_no_camp_light_control_mcw` -> `.userdata/dev-harness/harness_runs/20260501_032016/`.

Boundary: staged-but-live v0 does not claim natural random discovery, full siege/navigation behavior, year-one availability, unavoidable camp deletion, infinite rider accumulation, or broader release packaging. Do not reopen this lane, writhing-stalker lanes, bandit/horde proof lanes, Smart Zone, old fire proof lanes, release packaging, or held parked concepts by drift.

---

## Recently closed lane — CAOL-FLESH-RAPTOR-CIRCLING-SKIRMISHER-v0

**Status:** CLOSED / CHECKPOINTED GREEN V0 / PREDATOR VARIETY PACKAGE

Josef greenlit changing predator behavior variety after the zombie-rider lane, starting with flesh raptors as visible circling skirmishers instead of one-note stab-and-flee annoyance.

Contract: `doc/flesh-raptor-circling-skirmisher-packet-v0-2026-05-01.md`.

Imagination source: `doc/predator-behavior-variety-imagination-source-of-truth-2026-05-01.md`.

Result: flesh raptors now use a raptor-only live `monster::plan()` seam for readable 4–6 tile orbit/flank pressure, under-occupied arc preference, bounded swoop cadence, corridor/blocked-terrain fallback, and jitter guardrails without globally rewriting every `HIT_AND_RUN` monster or increasing equipment-destruction tedium.

Evidence: focused `[flesh_raptor]` tests cover variant footing, orbit/flank scoring, under-occupied arc preference, fallback, cadence/hysteresis, live plan consumption for `mon_spawn_raptor`, and non-raptor `HIT_AND_RUN` preservation. Credited staged-but-live rows are `flesh_raptor.live_open_field_skirmisher_mcw` -> `.userdata/dev-harness/harness_runs/20260501_052709/`, `flesh_raptor.live_crowded_arc_skirmisher_mcw` -> `.userdata/dev-harness/harness_runs/20260501_053414/`, `flesh_raptor.live_blocked_corridor_skirmisher_mcw` -> `.userdata/dev-harness/harness_runs/20260501_054807/`, and `flesh_raptor.live_equipment_frustration_comparison_mcw` -> `.userdata/dev-harness/harness_runs/20260501_062300/`. The final comparison row proves current orbit/swoop/melee debug metrics plus screenshot/OCR player-facing `flesh-raptor` / `impales` / `cut` / `bleeding` evidence from message history.

Closure review: Frau accepted v0 for agent-side close with staged-but-live caveats. Josef taste/playtest is optional/future, not a blocker; if Josef later says the raptor is still annoying, that is a taste/tuning follow-up, not proof that v0 failed.

Boundary: staged-but-live McWilliams rows are not natural random discovery. Equipment-damage tuning was not changed; equipment damage remains an observational frustration metric. Do not reopen this lane, remove every attack-and-retreat enemy, redesign eigenspectres, create a global pack-AI rewrite, or tune raptors by simply raising damage/equipment destruction unless Schani/Josef promote a follow-up.

---

## Recently closed lane — CAOL-WRITHING-STALKER-ZOMBIE-SHADOW-PREDATOR-v0

**Status:** CLOSED / CHECKPOINTED GREEN V0 / PREDATOR VARIETY PACKAGE

Josef greenlit changing both flesh raptors and the writhing stalker away from overused stab-and-flee behavior, with deterministic little-map tests, playtests, and fun metrics. Flesh raptor circling is closed green v0, and the stalker zombie-shadow predator shift is now closed green v0 for the scoped local-evidence package.

Shared imagination source: `doc/predator-behavior-variety-imagination-source-of-truth-2026-05-01.md`.

Contract: `doc/writhing-stalker-zombie-shadow-predator-packet-v0-2026-05-01.md`.

Result: the stalker now has explicit confidence components for evidence/interest, zombie pressure, quiet-side/cutoff opportunity, and counterpressure from light/focus/open exposure. The live shadow destination consumes the quiet-side cutoff scorer, so ordinary zombies pressing one side can make the stalker choose a shadow/cutoff tile where the zombies are not, without granting omniscient target acquisition.

Evidence: focused `[writhing_stalker]` tests prove pressure-side scoring, ambiguous split-pressure restraint, retreat-route cutoff bias, pressure gating, overmap-interest helper admission, light/focus suppression, and preservation of old no-omniscience/cooldown/injured-retreat constraints. `writhing_stalker.live_quiet_side_zombie_pressure_mcw` -> `.userdata/dev-harness/harness_runs/20260501_071548/` logs the local-evidence-only path from east/front zombie pressure (`pressure_x=3`) to a west/quiet-side cutoff tile (`quiet_x=-1`, `chosen_rel_x=-1`, `chosen_rel_y=-4`) with `overmap_interest=no`; proof note: `doc/writhing-stalker-zombie-shadow-live-quiet-side-proof-v0-2026-05-01.md`. Later sampler audit found an `ERROR GAME` wall-location backtrace in this row's `probe.artifacts.log`, so it is dirty/caveated for Josef-facing optical footing unless rerun clean. `writhing_stalker.live_escape_side_zombie_retreat_mcw` -> `.userdata/dev-harness/harness_runs/20260501_071940/` proves the cleaner local-evidence-only retreat row from north/front zombie pressure (`pressure_y=-3`) to a south/escape-side cutoff tile (`quiet_y=1`, `chosen_rel_y=4`) with `overmap_interest=no`; proof note: `doc/writhing-stalker-zombie-shadow-live-escape-side-proof-v0-2026-05-01.md`.

Closure caveat: v0 proves local-evidence zombie-shadow behavior through staged-but-live McWilliams rows. Live overmap-interest wiring/logging remains unclaimed unless a later packet promotes it; Josef taste/playtest is optional future feedback, not a v0 blocker.

Boundary: do not remove all attack-and-retreat enemies, do not fold eigenspectres into this package, do not make zombie proximity a magical target buff, and do not give the stalker omniscient backstab magic. The anti-redundancy refactor must preserve this approved zombie-shadow behavior shape.

---

## Closed recent lane — CAOL-WRITHING-STALKER-BEHAVIOR-SEAM-REDUCTION-v0

**Status:** CLOSED / CHECKPOINTED GREEN V0 / ANTI-REDUNDANCY PACKAGE

Josef greenlit targeted anti-redundancy packages after Schani's `master...dev` redundancy audit. The first cleanup package was the writhing-stalker behavior seam reduction, preserving the closed green zombie-shadow predator behavior.

Imagination source: `doc/anti-redundancy-packaging-imagination-source-of-truth-2026-05-01.md`.

Contract: `doc/writhing-stalker-behavior-seam-reduction-packet-v0-2026-05-01.md`.

Closure checkpoint: seam inventory is recorded in the contract doc, the live planner seam-consumption test proves `monster::plan()` reaches quiet-side cutoff behavior, and `src/monmove.cpp` now routes writhing stalker / zombie rider / flesh raptor target-response planning through a single named `targeted_live_plan_adapter` dispatch before the generic hostile/flee destination fallback. No behavior-tree or special-attack seam honestly owns this destination-planning response today; product-specific stalker judgment remains explicit in the writhing-stalker evaluator and quiet-side scorer.

Evidence: `git diff --check`; `make -j4 tests/writhing_stalker_test.o tests src/writhing_stalker_ai.o LINTJSON=0 ASTYLE=0 && ./tests/cata_test "[writhing_stalker]"` -> `All tests passed (192 assertions in 15 test cases)`; adapter spillover guard `./tests/cata_test "[zombie_rider],[flesh_raptor]"` -> `All tests passed (231 assertions in 21 test cases)`.

Boundary preserved: this was cleanup/refactor, not tuning. Closed writhing-stalker v0, zombie-shadow proof rows, bandit/horde/camp claims, flesh raptors, and eigenspectres remain closed unless explicitly promoted.

---

## Closed recent lane — CAOL-CAMP-LOCKER-EQUIPMENT-API-REDUCTION-v0

**Status:** CLOSED / CHECKPOINTED GREEN V0 / ANTI-REDUNDANCY PACKAGE

Shared imagination source: `doc/anti-redundancy-packaging-imagination-source-of-truth-2026-05-01.md`.

Contract: `doc/camp-locker-equipment-api-reduction-packet-v0-2026-05-01.md`.

Closure checkpoint: the camp locker API-reduction package is closed at the current scoped v0 boundary. The implementation now keeps camp policy explicit while deferring the audited item, wearability, body coverage/layer, reload/ammo, scoring, carried-cleanup, live-state, medical-readiness, and zone-boundary truths to existing engine APIs where an honest equivalent exists. The package deliberately stops here: remaining camp-specific choices such as enabled slots, bulletproof/weather-sensitive preference, kept-supply policy, and camp-storage boundaries are policy, not accidental duplicate ontology.

Evidence: `git diff --check`; `make -j4 tests/faction_camp_test.o tests src/basecamp.o LINTJSON=0 ASTYLE=0`; `./tests/cata_test "[camp][locker]"` -> green at closure. Last code checkpoint was `6a0f003dfe Reduce camp locker armor resistance scoring`.

Boundary preserved: cleanup/refactor only. This does not redesign basecamp missions, Smart Zones, general NPC equipment selection, outfit tuning, or any bandit/rider/stalker feature. Further camp-locker work requires a newly promoted follow-up with a concrete seam, not more open-ended ontology archaeology.

---


## Closed recent lane — CAOL-BANDIT-SIGNAL-ADAPTER-REDUCTION-v0

**Status:** CLOSED / CHECKPOINTED GREEN V0 / ANTI-REDUNDANCY PACKAGE

Shared imagination source: `doc/anti-redundancy-packaging-imagination-source-of-truth-2026-05-01.md`.

Contract: `doc/bandit-signal-adapter-reduction-packet-v0-2026-05-01.md`.

Closure proof: `doc/bandit-signal-adapter-reduction-proof-v0-2026-05-02.md`.

Closure checkpoint: the live `fd_fire` / `fd_smoke` observation path now builds a `bandit_mark_generation::local_field_signal_reading` and routes through `adapt_local_field_signal_reading()` before producing smoke/light projections. Basic field/time/weather/exposure mapping is now adapter-shaped; bandit mark scoring, site memory, live dispatch, and horde tuning remain custom/unchanged.

Evidence: `git diff --check`; `make -j4 tests/bandit_mark_generation_test.o src/bandit_mark_generation.o obj/do_turn.o LINTJSON=0 ASTYLE=0`; standalone adapter probe linked against `src/bandit_mark_generation.o` / `obj/bandit_dry_run.o`; `make -j1 tests LINTJSON=0 ASTYLE=0 && ./tests/cata_test "[bandit][marks]"` -> `All tests passed (236 assertions in 18 test cases)`.

Boundary preserved: cleanup/refactor only. This does not redesign bandit live-world dispatch, roster state, structural bounty, camp-map memory, signal tuning/ranges, horde behavior, or a generic overmap event bus. Existing roof-fire/live-signal expectations remain classified as preserved behavior, not newly tuned behavior.

---

## Recent closed lane — CAOL-WRITHING-STALKER-THREAT-DISTRACTION-HANDOFF-v0

**Status:** CLOSED / CHECKPOINTED GREEN V0 / FRAU-ACCEPTED

Josef live-tested the writhing stalker after the earlier hit-fade and zombie-shadow packets and reported that it is still not satisfactory. The bad contrast is now explicit: with three NPC allies / high visible threat it does not retreat into stalking distance, and at night outside it can stand near a house/window without attacking or making a legible move.

Imagination source: `doc/writhing-stalker-threat-distraction-handoff-imagination-source-2026-05-02.md`.

Contract: `doc/writhing-stalker-threat-distraction-handoff-packet-v0-2026-05-02.md`.

Handoff packet: `doc/writhing-stalker-threat-distraction-handoff-handoff-v0-2026-05-02.md`.

Deterministic checkpoint: `doc/writhing-stalker-threat-distraction-deterministic-checkpoint-v0-2026-05-03.md`.

Live/staged proof: `doc/writhing-stalker-threat-distraction-live-staged-proof-v0-2026-05-03.md`.

Raw live-watch note: `/Users/josefhorvath/.openclaw/workspace/runtime/caol-live-watch-20260502/writhing-stalker-live-watch-notes-2026-05-02.md`.

Goal: add/refine stalker overmap threat/opportunity evaluation, reality-bubble anti-loiter behavior, and overmap/bubble handoff memory so high-threat daylight/three-NPC situations retreat into stalking mode about `3` OMTs back, night/outside reachable-player situations attack or reposition instead of garden-gnome loitering, and zombie/distraction entering the player/NPC tile enables dark-square approach/strike without omniscience.

Current checkpoint: deterministic evaluator/live-plan seam coverage is green for threat retreat, stalking-distance intent, dark reachable anti-loiter, zombie-distraction/no-omniscience, handoff/writeback, and existing stalker guarantees. Current-build live/staged proof is green for all three remaining rows: high-threat/allies retreat/stalk (`writhing_stalker.live_high_threat_allied_light_retreat_stalk_mcw` -> `.userdata/dev-harness/harness_runs/20260503_021310/`), zombie/distraction shadow-then-strike (`writhing_stalker.live_zombie_distraction_mcw` -> `.userdata/dev-harness/harness_runs/20260503_031247/`), and night/outside reachable bad-loiter anti-gnome strike (`writhing_stalker.live_anti_gnome_bad_loiter_mcw` -> `.userdata/dev-harness/harness_runs/20260503_025712/`). Frau accepted this as closure-ready agent-side staged/live feature-path evidence; door opening did not land and remains out of scope unless separately promoted.

Door-opening line: allowed only as a narrow optional escalation if needed — unlocked/simple doors, slow/noisy/interruptible, darkness/distraction/commitment gated, and suppressed under high threat. Do not turn the stalker into a burglar or locked-door solver.

Frau review note: safe claims are deterministic seam coverage plus current-build staged/live behavior rows; do not claim natural random discovery, full natural retreat pathing, broad house navigation, door opening, burglar/locked-door solving, or general ecosystem behavior.

Boundary: closed v0 packet. Do not reopen all stalker v0 work, flesh raptors, zombie riders, bandits, the save-pack card, or natural random discovery by drift.

---

## Handoff boundary — CAOL-JOSEF-PLAYTEST-SAVE-PACK-v0

**Status:** WAITING FOR NEXT GREENLIGHT / JOSEF HANDOFF CARD READY / OPTIONAL BANDIT CONTRAST READY / NOT CURRENT ANDI LANE

Josef greenlit turning the relay-ready visions sampler into a concrete playtest save pack. The product now is not another proof receipt: it is a small labelled set of current-build saves or handoff sessions Josef can actually load and play.

Imagination source: `doc/caol-josef-playtest-save-pack-imagination-source-2026-05-02.md`.

Contract: `doc/caol-josef-playtest-save-pack-packet-v0-2026-05-02.md`.

Handoff packet: `doc/caol-josef-playtest-save-pack-handoff-v0-2026-05-02.md`.

Working playtest card: `doc/caol-josef-playtest-save-pack-card-v0-2026-05-02.md`.

Promoted from: `CAOL-VISIONS-PLAYTEST-SAMPLER-v0` and its relay card `doc/caol-visions-josef-playtest-card-v0-2026-05-01.md`.

Goal: prepare labelled playable entries for Basecamp AI / camp locker usefulness, bandit pressure / shakedown / basecamp contact, cannibal camp pressure, flesh raptor skirmishing, zombie rider predator/counterplay, and writhing stalker hit-fade / zombie-shadow behavior. Each entry needs a short “what to try” card, current-build load/start-state evidence, portal-storm warning status, proof footing, and plain staged-vs-natural caveats. Include Josef's thematic contrast pass for stalker and bandit behavior: no-fire/no-signal, fire/smoke/light signal, and high-threat/resistant setup should produce visibly different reads or be marked caveated/blocked. Before threat-contrast rows are credited, audit camp NPC membership: unassigned current-save NPCs are not camp threat evidence; low-threat rows may remove/kill/despawn extras, while high-threat rows must spawn or repair NPCs as properly camp-assigned members.

Current card checkpoint: `doc/caol-josef-playtest-save-pack-card-v0-2026-05-02.md` now lists six ready staged rows (camp locker weather/service, bandit first-demand contact, cannibal night pressure, flesh raptor crowded-arc skirmisher, zombie rider cover/wounded contrast, writhing stalker hit-fade/light/zombie-side pressure) plus explicit caveated/omitted rows for bandit contrast/camp-threat and camp/NPC assignment. Post-card no-signal repair `bandit.live_world_nearby_camp_no_signal_control_mcw` -> `.userdata/dev-harness/harness_runs/20260502_134959/` proves a cleaned low-threat/no-loose-NPC no-signal control, but it does not claim camp-threat membership and does not expand the first Josef card by itself. Smoke/fire signal is repaired through the materially different guarded `bandit.mixed_signal_coexistence_mcw` path -> `.userdata/dev-harness/harness_runs/20260502_155058/`, with green wait/step ledgers, portal clear, signal scout dispatch from camp `@151,39,0`, separate structural scavenge from camp `@160,39,0`, and manual saved-overmap active-member cross-references for members `4` and `9`; direct `bandit.player_lit_fire_signal_wait_mcw` attempt `20260502_154828/` remains blocked at startup UI and is not credited. Flesh raptor current footing was repaired by relaxing the stale exact crowded-arc tile expectation and rerunning `flesh_raptor.live_crowded_arc_skirmisher_mcw` -> `.userdata/dev-harness/harness_runs/20260502_141246/` green/portal-clear. Camp-pressure assignment is repaired as auxiliary proof: `bandit.active_outside_dogpile_block_live` -> `.userdata/dev-harness/harness_runs/20260502_144842/` is green/portal-clear and requires `active_member_ids=[4,5]` with `active_members_all_found_in_saved_overmap=true`. Pure high-threat hold is repaired as auxiliary proof: `bandit.high_threat_low_reward_holds` -> `.userdata/dev-harness/harness_runs/20260502_145429/` is green/portal-clear with green guarded wait and the same-run risk/reward hold artifact. After the later zombie-rider tainted-arrow/rider behavior commits, the rider cover/wounded card rows were refreshed on current `dev` with `.userdata/dev-harness/harness_runs/20260502_232133/` and `20260502_232214/`, both green and portal-clear after `./just_build_macos.sh > /tmp/caol-savepack-post-rider-build-20260502.log 2>&1` exited `0`. Andi's v0 save-pack prep is now at a state boundary: Schani can relay the six-entry card as-is and may include the optional staged bandit contrast card; no ready rows or old blocked bandit rows need ritual reruns.

Boundary: save-pack prep and product-taste handoff only. Do not implement new gameplay unless a hard blocker prevents a save from loading or being playable. Do not reopen closed v0 lanes by drift, do not create release packaging, and do not make Josef inspect logs as the primary playtest activity.

---

## Closed recent lane — bandit scenic shakedown chat openings

**Status:** CLOSED AS SCENIC-UI PROOF / SUPERSEDED BY `CAOL-JOSEF-LIVE-DEBUG-BATCH-v0` FOR SHAKEDOWN RESPONSE + PAYMENT CONTRACT

Josef asked on 2026-05-01 for the bandit shakedown to use a normal chat window, become more scenic, and have a selection of bandit openings.

Contract: `doc/bandit-scenic-shakedown-chat-window-openings-packet-v0-2026-05-01.md`.

Proof: `doc/bandit-scenic-shakedown-chat-window-openings-proof-v0-2026-05-02.md`.

Result: the live shakedown UI now uses the normal `dialogue_window` surface instead of the bare `uilist`, with contextual opening IDs/summaries/barks for basecamp pressure, warning from cover, weakness read, roadblock toll, and reopened higher-demand. Its staged proof is still useful for the scenic-dialogue path and cannibal/no-shakedown separation, but its `Pay / Fight / Refuse` response contract and pay-branch behavior are now explicitly superseded by `CAOL-JOSEF-LIVE-DEBUG-BATCH-v0`: Josef's stranded 2026-05-02/03 notes require visible Pay/Fight only and a trade/debt-style payment surface.

Boundary: scenic-dialogue proof only. Do not use this closed proof to claim the final shakedown response/payment contract; that correction now belongs to `CAOL-JOSEF-LIVE-DEBUG-BATCH-v0`.

---

## Recent closed detail — harness portal-storm warning light

**Status:** CHECKPOINTED GREEN V0 / HARNESS-HARDENING FOLLOW-UP

Josef reported on 2026-05-02 that portal storms sometimes seem to break harness runs and asked for a “flashing light”.

Contract: `doc/harness-portal-storm-warning-light-packet-v0-2026-05-02.md`.
Proof: `doc/harness-portal-storm-warning-light-proof-v0-2026-05-02.md`.

Result: report-level `portal_storm_warning` now lands in probe/handoff reports and repeatability summaries; unallowed portal storms yellow the step ledger as `yellow_step_portal_storm_contamination`; opt-in portal-storm scenarios remain visible but green when explicitly allowed.

Boundary: harness-hardening only. Do not solve portal-storm gameplay, redesign weather, rerun old packets by ritual, or reopen closed bandit/visions/camp-locker lanes by drift.

---

## Recent closed detail — zombie rider close-pressure no-attack fix

**Status:** CLOSED / CHECKPOINTED GREEN V0 / BUGFIX + PRODUCT-FEEL FOLLOW-UP

Josef live-watched `zombie_rider.live_open_field_pressure_mcw` on 2026-05-02 and then quit the game so Andi could get the package. The watched handoff seed is `.userdata/dev-harness/harness_runs/20260502_015857/`, but it is yellow/inconclusive due to runtime-version mismatch, so use it as a bug seed and player-observed taste note rather than closure proof.

Imagination source: `doc/zombie-rider-close-pressure-no-attack-imagination-source-2026-05-02.md`.

Contract: `doc/zombie-rider-close-pressure-no-attack-packet-v0-2026-05-02.md`.

Handoff packet: `doc/zombie-rider-close-pressure-no-attack-handoff-v0-2026-05-02.md`.

Goal/result: fixed the zombie rider close-pressure/no-attack seam. The important smell was `decision=bow_pressure`, `reason=line_of_fire`, and `line_of_fire=yes` at distance around `4-5`, while Josef saw no actual attack. The plan seam did not force `aggro_character`, and `gun_actor` refuses avatar shots while `!z.aggro_character`; current code marks aggro before ready bow handoff, and close/indoor pressure chooses irregular bunny-hop/reposition destinations instead of polite loitering.

Evidence: `doc/zombie-rider-close-pressure-no-attack-proof-v0-2026-05-02.md`; live row `.userdata/dev-harness/harness_runs/20260502_050055/`.

Boundary: bugfix/product-feel follow-up only. Do not reopen all `CAOL-ZOMBIE-RIDER-0.3-v0`, and do not break wounded disengagement, cover/LOS counterplay, camp-light banding, or no-light controls.

---

## Closed receipt — CAOL-WRITHING-STALKER-HIT-FADE-RETREAT-DISTANCE-v0

**Status:** CLOSED / CHECKPOINTED GREEN v0 / PRODUCT-TUNING + BEHAVIOR FIX FOLLOW-UP

Proof/readout: `doc/writhing-stalker-hit-fade-retreat-distance-proof-v0-2026-05-02.md`.

Josef live-watched the writhing stalker on 2026-05-02 and greenlit a focused follow-up for its attack/retreat rhythm. The watched handoff seed `.userdata/dev-harness/harness_runs/20260502_015032/` remains yellow/debug footing because of runtime-version mismatch.

Closed behavior: the stalker no longer uses generic `HIT_AND_RUN`; it owns a bounded burst/fade cadence. Favorable vulnerable pressure can permit a short 2-4 hit burst, while light/focus/allied support/injury push earlier/farther caution. Post-burst live planning targets about `8+` tiles of retreat when pathing allows.

Credited evidence: original flesh-raptor `HIT_AND_RUN` source reference; deterministic `[writhing_stalker]` suite green (`206 assertions in 17 test cases`); current-build `cataclysm-tiles` build green; staged-but-live McWilliams feature proof `.userdata/dev-harness/harness_runs/20260502_113738/` with `feature_proof=true`, `verdict=artifacts_matched`, `decision=strike`, `decision=withdraw`, `decision=cooling_off`, `burst=0/2`, `burst=1/2`, `burst=2/2`, `retreat_distance=8`, and `cooldown=yes`.

Caveat: staged-but-live feature proof, not natural random discovery or final human taste for exact scariness/forgiveness.

---

## Closed backlog receipt — targeted anti-redundancy packages

**Status:** CLOSED / CHECKPOINTED GREEN V0 SET / NEEDS FRESH PROMOTION FOR MORE

These were queued cleanup/refactor contracts with targeted tests; they are not current `TODO.md` work.

Shared imagination source: `doc/anti-redundancy-packaging-imagination-source-of-truth-2026-05-01.md`.

All originally queued v0 anti-redundancy packages in this block are now closed/checkpointed or need a freshly promoted concrete seam before further work.

Boundary: these packages do not reopen closed writhing-stalker, bandit, horde, Smart Zone, or camp claims by drift. The larger bandit live-world roster/mission-state reduction remains future audit territory, not part of this targeted package set.

---

## Held / parked lanes that must not disappear

**Status:** PARKED / HELD VISIBILITY ONLY

These are visible here only because they affect future selection. Do not treat them as active without explicit Schani/Josef promotion.

- `CAOL-GITHUB-RELEASE-v0` — **HELD / NOT PROMOTED**. Normal GitHub-download release work remains held until Schani/Josef explicitly promote it. Contract: `doc/github-normal-download-release-packet-v0-2026-04-25.md`.
- `CAOL-BANDIT-OVERMAP-AI-CONCEPT-v0` — **PARKED / COHERENT SUBSTRATE**. Parent concept: `doc/bandit-overmap-ai-concept-2026-04-19.md`; bounded promoted slices need their own IDs/contracts.
- `CAOL-AOL-SOCIAL-THREAT-BANK-v0` — **PARKED / FAR BACK**. Anchor: `doc/arsenic-old-lace-social-threat-and-agentic-world-concept-bank-2026-04-22.md`. Writhing stalker has now been promoted out of this bank into its own active packet; the rest remains parked.

---

## Recent closed receipts

Use `doc/work-ledger.md` and the linked aux docs for exact evidence, caveats, and supersessions. Do not reopen these by drift.

- `CAOL-ROOF-HORDE-NICE-FIRE-v0` — closed/checkpointed green v0; focused nice roof-fire horde playtest using source player-created roof-fire chain `.userdata/dev-harness/harness_runs/20260429_172847/`, named scenario `bandit.roof_fire_horde_nice_roof_fire_mcw`, and green run `.userdata/dev-harness/harness_runs/20260430_191556/`. Proof reaches live roof-fire horde signal plus saved horde retarget/destination, `last_processed=5267242`, and `moves=8400`; caveats: `tracking_intensity=0` and horde-specific timing `not instrumented`. Proof doc `doc/roof-fire-horde-nice-roof-fire-proof-v0-2026-04-30.md`.
- `CAOL-WRITHING-STALKER-v0` — closed/checkpointed green v0; first playable rare singleton zombie-adjacent predator with deterministic interest/latch/approach/opportunity/withdraw substrate, live monster-plan seam, exposed-retreat proof, shadow/strike/cooldown proof, no-omniscience negative control, and mixed-hostile metrics/tuning readout. Caveat preserved: horde presence/setup is proven in `.userdata/dev-harness/harness_runs/20260430_181748/`, but direct horde movement/retarget cost is not instrumented and is future-only unless explicitly promoted.
- `CAOL-BANDIT-STRUCT-BOUNTY-v0` — closed/checkpointed green v0; closure readout `doc/bandit-structural-bounty-phase-7-closure-readout-2026-04-30.md`; green run `.userdata/dev-harness/harness_runs/20260430_115157/`; non-credit run `.userdata/dev-harness/harness_runs/20260430_114106/`.
- `CAOL-SZM-LIVE-LABEL-v0` — closed green live coordinate-label proof; run `.userdata/smart-zone-audit-live-20260429e/harness_runs/20260429_225644/`.
- `CAOL-CANNIBAL-NIGHT-RAID-v0` — closed cannibal camp night-raid behavior checkpoint; contract `doc/cannibal-camp-night-raid-behavior-packet-v0-2026-04-28.md`.
- `CAOL-CANNIBAL-CONFIDENCE-PUSH-v0` — closed confidence-uplift proof; matrix `doc/cannibal-camp-confidence-push-live-playtest-matrix-v0-2026-04-29.md`.
- `CAOL-BANDIT-CAMP-MAP-RISK-REWARD-v0` — closed scoped live/product checkpoint; matrix `doc/bandit-live-product-playtest-matrix-v0-2026-04-28.md`.
- `CAOL-HARNESS-PROOF-FREEZE-v0` — closed/checkpointed process rule; proof-freeze matrix `doc/c-aol-harness-proof-freeze-matrix-v0-2026-04-28.md`.
- `CAOL-REAL-FIRE-SIGNAL-v0` and `CAOL-ROOF-HORDE-SPLIT-v0` — closed actual-playtest proof bundles for real-fire signal response and split-run roof-fire horde response.
- `CAOL-BANDIT-LIVE-SIGNAL-SITE-BOOTSTRAP-v0` — superseded partial lane; real player-fire signal response is closed by `CAOL-REAL-FIRE-SIGNAL-v0`, while remaining range/decay/scoring questions are future-only unless explicitly promoted.

---

## Documentation discipline

If the structure starts bloating again, apply this rule:
- `Plan.md` should be readable in a minute
- `TODO.md` should show only the current execution queue
- `TESTING.md` should show only current policy, latest relevant evidence, and pending probes
- `COMMIT_POLICY.md` should stop the dirty tree from becoming a lifestyle

If a sentence exists only to remember that something used to be true, it probably belongs in git history, not here.
