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

### Closed validation receipt - CAOL-BANDIT-SIGNAL-ADAPTER-REDUCTION-v0

`CAOL-BANDIT-SIGNAL-ADAPTER-REDUCTION-v0` is closed/checkpointed green v0. Contract: `doc/bandit-signal-adapter-reduction-packet-v0-2026-05-01.md`; proof/readout: `doc/bandit-signal-adapter-reduction-proof-v0-2026-05-02.md`; imagination source: `doc/anti-redundancy-packaging-imagination-source-of-truth-2026-05-01.md`.

Credited evidence:
- Source path now names `bandit_mark_generation::local_field_signal_reading`, `local_field_signal_projection`, and `adapt_local_field_signal_reading()` as the local field/time/weather adapter from live `fd_fire` / `fd_smoke` readings into existing smoke/light projections.
- `src/do_turn.cpp::observe_live_bandit_field_signals_near_player()` now builds that adapter reading and consumes the resulting smoke/light projections, rather than open-coding a second packet mapping in the live path.
- Light observations with positive horde power still flow through `signal_live_hordes_from_light_observations()`, which calls `overmap_buffer.signal_hordes(...)`.
- Focused tests `bandit_mark_generation_local_field_adapter_preserves_smoke_light_and_horde_seams` and `bandit_mark_generation_local_field_adapter_keeps_smoke_only_from_signaling_light_hordes` prove fire+smoke mapping, smoke-only non-light behavior, and horde-power preservation through the existing light projection seam.

Gates: `git diff --check`; `make -j4 tests/bandit_mark_generation_test.o src/bandit_mark_generation.o obj/do_turn.o LINTJSON=0 ASTYLE=0`; standalone adapter probe linked against `src/bandit_mark_generation.o` / `obj/bandit_dry_run.o`; `make -j1 tests LINTJSON=0 ASTYLE=0 && ./tests/cata_test "[bandit][marks]"` -> `All tests passed (236 assertions in 18 test cases)`.

Caveat: cleanup/refactor only. No new bandit dispatch, horde tuning, roof-fire, structural-bounty, or generic overmap-event-bus claim is made.

### Closed validation receipt - CAOL-HARNESS-PORTAL-STORM-WARNING-LIGHT-v0

Portal-storm warning-light is implemented/proven and Frau-accepted green v0 from commits `74ef657057` / `8ea5546107`. Contract: `doc/harness-portal-storm-warning-light-packet-v0-2026-05-02.md`; proof/readout: `doc/harness-portal-storm-warning-light-proof-v0-2026-05-02.md`.

Validation gate:
- `python3 tools/openclaw_harness/proof_classification_unit_test.py` -> `Ran 13 tests ... OK`;
- `python3 -m py_compile tools/openclaw_harness/startup_harness.py tools/openclaw_harness/proof_classification_unit_test.py` -> pass;
- `git diff --check` -> pass.

Credited evidence before close:
- [x] portal-storm positive detection row from saved `dimension_data.gsav` metadata;
- [x] normal-weather negative control row;
- [x] report/summary warning-light output via report-level `portal_storm_warning` and repeatability text summary;
- [x] unallowed portal-storm contamination affects the step/report ledger visibly as `yellow_step_portal_storm_contamination` and blocks feature proof;
- [x] allowed/intentional portal-storm scenario path stays green when explicitly permitted as `green_step_portal_storm_expected_allowed`.

Interpretation: if `portal_storm_warning.classification = contaminating`, rerun under controlled non-portal-storm weather unless the scenario intentionally tests portal storms.

### Active validation target - CAOL-JOSEF-PLAYTEST-SAVE-PACK-v0

`CAOL-JOSEF-PLAYTEST-SAVE-PACK-v0` is active. Contract: `doc/caol-josef-playtest-save-pack-packet-v0-2026-05-02.md`; imagination source: `doc/caol-josef-playtest-save-pack-imagination-source-2026-05-02.md`; handoff: `doc/caol-josef-playtest-save-pack-handoff-v0-2026-05-02.md`.

Working playtest card: `doc/caol-josef-playtest-save-pack-card-v0-2026-05-02.md`.

Validation shape: playable save/handoff preparation, not broad proof rerun. Use existing green staged/live rows as footing where possible, but each ready entry needs a current-build load/start-state check, portal-storm warning classification, and a short Josef-facing instruction card.

Required entries:
- Basecamp AI / camp locker usefulness.
- Bandit pressure / shakedown / basecamp contact.
- Cannibal camp pressure.
- Flesh raptor skirmisher behavior.
- Zombie rider predator/counterplay.
- Writhing stalker hit-fade / zombie-shadow behavior.

Evidence required before handoff:
- [x] stalker contrast rows distinguish no-fire/low-threat close-strike-fade, fire/light counterplay, and zombie/escape-side pressure or mark blocked/caveated. Ready rows: hit-fade `20260502_125231/`, campfire/light `20260502_131246/`, and escape-side zombie pressure `20260502_131315/`, all portal clear. Later no-fire hit-fade rerun `20260502_131232/` is blocked on time setup and is labelled caveated instead of rerun.
- [x] bandit contrast rows distinguish no-signal, fire/smoke/basecamp signal, and high-threat/resistant setup or mark blocked/caveated. Ready card row is only first-demand contact `20260502_124854/`; no-signal `20260502_125743/`, standoff `20260502_131536/`, high-threat `20260502_131616/`, and fire/smoke signal remain caveated/manual future until camp/NPC assignment is repaired/audited.
- [x] camp/NPC preflight audits membership assignment and marks loose-NPC rows no-credit. Current audit on `20260502_131616/saved_world/McWilliams` found the saved live-world site has `member_count=5`, `ready_at_home_count=5`, `active_outside_count=0`, and `active_member_ids=[]`, while a full overmap NPC scan sees `observed_npc_count=16` including follower and `hells_raiders` loose NPC records. No camp-threat credit from those loose NPCs.
- [x] flesh raptor broader taste target is omitted/caveated rather than treated as current-ready: older green footing exists (`20260501_052709/`, `20260501_053414/`, `20260501_062300/`), current broader attempts are no-credit (`20260502_131124/`, `20260502_131806/`, `20260502_131900/`), and the narrow blocked-corridor green row `20260502_132217/` is not enough to make the whole raptor entry first-card ready.
- [x] each inventoried entry has a save path, handoff command, or exact setup/load path in `doc/caol-josef-playtest-save-pack-card-v0-2026-05-02.md`.
- [x] each ready entry has a current-build load/start-state result from `probe`, `handoff`, startup, or an equivalent narrow check. Ready rows: locker `20260502_131015/`, bandit first-demand contact `20260502_124854/`, cannibal `20260502_131103/`, zombie rider `20260502_131914/` and `20260502_132136/`, writhing stalker `20260502_125231/`, `20260502_131246/`, and `20260502_131315/`.
- [x] each ready entry records `portal_storm_warning` status and any runtime warning caveats. All ready rows above are portal clear.
- [x] each entry cites proof footing without laundering staged rows into natural discovery claims.
- [x] omitted/blocked entries are labelled with the missing primitive instead of hidden.
- [x] final Josef-facing card is short enough to play from: `doc/caol-josef-playtest-save-pack-card-v0-2026-05-02.md`.

Card checkpoint: `doc/caol-josef-playtest-save-pack-card-v0-2026-05-02.md` is the Josef-facing index. It marks five ready staged rows green from fresh inspected reports (locker, bandit first-demand contact, cannibal, zombie rider cover/wounded, writhing stalker hit-fade/light/zombie-side) and separately labels flesh raptor, bandit contrast/camp-threat, and camp/NPC assignment as omitted/caveated rows. The save-pack can be handed to Josef without pretending every old monster postcard got a fresh passport stamp.

Prior sampler footing: `CAOL-VISIONS-PLAYTEST-SAMPLER-v0` remains useful source material (`doc/caol-visions-josef-playtest-card-v0-2026-05-01.md`), but it is no longer the active deliverable.

### Closed validation receipt - CAOL-ZOMBIE-RIDER-CLOSE-PRESSURE-NO-ATTACK-v0

`CAOL-ZOMBIE-RIDER-CLOSE-PRESSURE-NO-ATTACK-v0` is closed/checkpointed green v0. Proof/readout: `doc/zombie-rider-close-pressure-no-attack-proof-v0-2026-05-02.md`; contract: `doc/zombie-rider-close-pressure-no-attack-packet-v0-2026-05-02.md`.

Credited evidence:
- root cause named: `bow_pressure + line_of_fire=yes` set a plan destination but did not force `aggro_character`; `gun_actor` refuses non-monster avatar targets when `!z.aggro_character`.
- `git diff --check && make -j4 tests/zombie_rider_test.o tests src/monmove.o LINTJSON=0 ASTYLE=0` passed.
- `./tests/cata_test "[zombie_rider]"` passed: `199 assertions in 16 test cases`.
- `./just_build_macos.sh > /tmp/caol-zombie-rider-tiles-build3.log 2>&1` exited `0`; `cataclysm-tiles` linked.
- Fresh live row `zombie_rider.live_open_field_pressure_mcw` -> `.userdata/dev-harness/harness_runs/20260502_050055/`: `feature_proof=true`, `verdict=artifacts_matched`, `green_step_local_proof`, no abort/runtime warnings; saved active-monster audit requires `ammo={"arrow_wood":18}`; live plan shows `aggro_before=no aggro_after=yes`, bow ammo decrement, and close `decision=reposition reason=too_close_bunny_hop` pressure.

Caveat: staged-but-live McWilliams feature proof, not natural random discovery/full siege-navigation proof.

### Closed validation receipt - CAOL-CAMP-LOCKER-ZONE-PLAYTESTS-v0

`CAOL-CAMP-LOCKER-ZONE-PLAYTESTS-v0` is closed green. Proof/readout: `doc/camp-locker-zone-playtest-proof-v0-2026-05-02.md`; contract: `doc/camp-locker-zone-playtest-packet-v0-2026-05-02.md`; imagination source: `doc/camp-locker-zone-playtests-imagination-source-2026-05-02.md`; handoff packet: `doc/camp-locker-zone-playtest-handoff-v0-2026-05-02.md`.

Credited evidence:
- `locker.zone_manager_save_probe_mcw` -> `.userdata/dev-harness/harness_runs/20260502_041828/`: `feature_proof=true`, `verdict=artifacts_matched`, `step_ledger_status=green_step_local_proof`; UI trace matches `name="Basecamp: Locker" type="CAMP_LOCKER"` and created/reopened `name="Probe Locker" type="CAMP_LOCKER"`; saved-zone audit `audit_saved_existing_locker_zone_after_save.metadata.json` reports `required_state_present` for the persistent `Basecamp: Locker` `CAMP_LOCKER` zone in `#Wm9yYWlkYSBWaWNr.zones.json` / temp mirror.
- `locker.weather_wait` -> `.userdata/dev-harness/harness_runs/20260502_041300/`: `feature_proof=true`, `verdict=artifacts_matched`, `step_ledger_status=green_step_local_proof`; same-run service artifacts match `camp locker: queued`, `plan for`, `after`, and `serviced`, with `locker_tiles=1`, `candidates=1`, `changed_slots=1`, `applied=true`, worker pants swapped to `cargo pants`, and old `antarvasa` returned to locker stock.
- Deterministic gate `./tests/cata_test "[camp][locker]"` passed from `/tmp/caol-locker-zone/camp_locker_tests_rerun.log` (`2147 assertions in 78 test cases`) and covers `NO_NPC_PICKUP`, off-zone, and policy-disabled boundary/exclusion behavior in `camp_locker_zone_candidate_gathering`.

Caveats retained:
- `locker.package5_robbie_e2e_verified_mcw` -> `.userdata/dev-harness/harness_runs/20260502_034951/` blocked before service artifacts and receives no credit.
- The newly created `Probe Locker` is credited from UI trace/reopen proof, while the disk-file saved-zone audit covers the existing persistent `Basecamp: Locker` row. Full human weather-feel taste can still be playtested from the optional card in the proof doc, but it is not a blocker for this v0 evidence closure.

### Closed validation receipt - CAOL-WRITHING-STALKER-LIVE-FUN-SCENARIOS-v0

`CAOL-WRITHING-STALKER-LIVE-FUN-SCENARIOS-v0` is closed/checkpointed green v0. Contract: `doc/writhing-stalker-live-fun-scenarios-packet-v0-2026-04-30.md`; imagination source: `doc/writhing-stalker-live-fun-scenarios-imagination-source-of-truth-2026-04-30.md`; prior deterministic proof: `doc/writhing-stalker-behavior-pattern-proof-v0-2026-04-30.md`; closure proof: `doc/writhing-stalker-live-fun-scenario-proof-v0-2026-04-30.md`.

Credited evidence:
- `writhing_stalker.live_campfire_counterplay_mcw` -> `.userdata/dev-harness/harness_runs/20260430_233129/` proves bright/focused light counterplay withdraw/cooling and forbids strike.
- `writhing_stalker.live_alley_predator_mcw` -> `.userdata/dev-harness/harness_runs/20260430_233156/` proves cover-shadow approach, close vulnerability strike, cooldown, and re-engagement without spam.
- `writhing_stalker.live_zombie_distraction_mcw` -> `.userdata/dev-harness/harness_runs/20260430_233521/` proves zombie-pressure-assisted shadow/strike/cooldown rhythm only with evidence/vulnerability.
- `writhing_stalker.live_zombie_distraction_no_magic_guard_mcw` -> `.userdata/dev-harness/harness_runs/20260430_233335/` proves nearby zombies do not grant target acquisition through the opaque wall.
- `writhing_stalker.live_door_light_escape_mcw` -> `.userdata/dev-harness/harness_runs/20260430_233405/` proves light/focus escape breaks pressure into withdraw/cooling and forbids strike.
- `writhing_stalker.live_wounded_predator_mcw` -> `.userdata/dev-harness/harness_runs/20260430_233434/` proves injured retreat overrides greed with no strike snapback.

Gates: `make -j4 TILES=1 LINTJSON=0 ASTYLE=0`; `./tests/cata_test "[writhing_stalker]"` -> `All tests passed (129 assertions in 10 test cases)`. Live cooldown in `src/monmove.cpp` was minimally adjusted so `cooling_off` no longer refreshes `effect_run` forever.

Caveat: staged-but-live McWilliams scenarios, not fully natural random discovery; future manual discovery is optional/future-only unless promoted.

### Closed validation receipt - CAOL-ZOMBIE-RIDER-0.3-v0

`CAOL-ZOMBIE-RIDER-0.3-v0` is closed/checkpointed green v0 initial dev. Contract: `doc/zombie-rider-0.3-initial-dev-packet-v0-2026-04-30.md`; imagination source: `doc/zombie-rider-0.3-imagination-source-of-truth-2026-04-30.md`; benchmark suite: `doc/zombie-rider-map-ai-funness-benchmark-suite-v0-2026-04-30.md`; raw intake: `doc/zombie-rider-raw-intake-2026-04-30.md`; closure readout: `doc/zombie-rider-0.3-closure-readout-v0-2026-05-01.md`.

Credited footing evidence:
- Commit `d50715f00e` adds `mon_zombie_rider` with exact three-line Josef description, huge/scary-fast monster footing, pseudo `zombie_rider_bone_bow`, direct mature `GROUP_ZOMBIE` gate at `730 days`, and no normal predator/upgrade path into rider.
- Passage-seam proof `zombie_rider_large_body_small_passage_pathing` proves spawned rider actual size rejects `TFLAG_SMALL_PASSAGE` / `t_window_empty`, normal floor remains valid, normal-sized pathfinding can use the passable window seam, and rider-sized pathfinding routes around it.
- Local combat checkpoint `4343dbdad1` proves live `monster::plan()` consumption for bow shot cadence/cooldown, post-shot reposition destination, close-pressure retreat, injured withdrawal, blocked-LOS no-shot counterplay, and preservation of endpoint footing.
- Overmap light checkpoint `d2ffbd54c3` proves mature-world light attraction, early-world suppression, no-rider/no-light/weak/daylight controls, temporary memory decay, and `max_riders_drawn_by_light = 2`.
- Rider convergence/band checkpoint `ce05ef44ec` proves capped deterministic convergence, decayed-memory/no-eligible/lone-rider controls, and camp pressure postures that include `circle_harass` without wall-suicide.

Credited staged-but-live rows:
- `zombie_rider.live_open_field_pressure_mcw` -> `.userdata/dev-harness/harness_runs/20260501_013055/` (open-field bow pressure then withdrawal).
- `zombie_rider.live_wounded_disengagement_mcw` -> `.userdata/dev-harness/harness_runs/20260501_014613/` (wounded rider withdraws, no bow pressure).
- `zombie_rider.live_cover_escape_mcw` -> `.userdata/dev-harness/harness_runs/20260501_021656/` (opaque cover keeps target/no-sight/no-line-of-fire, no bow pressure).
- `zombie_rider.live_camp_light_band_mcw` -> `.userdata/dev-harness/harness_runs/20260501_030432/` (camp lights aggregate into capped two-rider band/circle-harass).
- `zombie_rider.live_no_camp_light_control_mcw` -> `.userdata/dev-harness/harness_runs/20260501_032016/` (matched no-light footing emits no horde-light or rider camp-light/band trace).

Caveat: staged-but-live McWilliams rows, not natural random discovery/full siege navigation/release packaging. Do not rerun solved rider rows by ritual unless Schani/Josef explicitly promote a stricter follow-up.

### Closed validation receipt - CAOL-FLESH-RAPTOR-CIRCLING-SKIRMISHER-v0

`CAOL-FLESH-RAPTOR-CIRCLING-SKIRMISHER-v0` is closed/checkpointed green v0. Contract / closure evidence: `doc/flesh-raptor-circling-skirmisher-packet-v0-2026-05-01.md`; imagination source: `doc/predator-behavior-variety-imagination-source-of-truth-2026-05-01.md`.

Credited deterministic footing:
- Code seam: `src/monmove.cpp` routes only base/shadow/unstable/electric/dusted/fungalized/fungal flesh raptors through `apply_flesh_raptor_plan()` after existing writhing-stalker and zombie-rider special plan hooks, before generic target pursuit.
- Helper: `src/flesh_raptor_ai.*` scores 4–6 tile lateral/orbit candidates, under-occupied arcs, held-destination hysteresis, bounded cadence swoop windows, and fallback when no readable lateral orbit exists.
- Focused tests: `tests/flesh_raptor_test.cpp` covers variant footing, lateral-over-straight scoring, crowding preference, cadence/hysteresis, corridor fallback, live `monster::plan()` consumption for `mon_spawn_raptor`, and preservation of a non-raptor `HIT_AND_RUN` monster.

Green gates:
- `make -j4 tests/flesh_raptor_test.o tests src/flesh_raptor_ai.o LINTJSON=0 ASTYLE=0 && ./tests/cata_test "[flesh_raptor]"` -> `All tests passed (61 assertions in 7 test cases)`.
- `make -j4 TILES=1 LINTJSON=0 ASTYLE=0` -> green after the committed-swoop movement bridge fix.
- `git diff --check` -> clean.
- `make astyle-diff LINTJSON=0 ASTYLE=0` could not run because `astyle` is not installed on this host.

Credited staged-but-live rows:
- `flesh_raptor.live_open_field_skirmisher_mcw` -> `.userdata/dev-harness/harness_runs/20260501_052709/` proves feature-path open-field orbit/swoop behavior with step ledger green (`6/6`), `artifacts_matched`, `feature_proof True`, no runtime warnings, and required log state present.
- `flesh_raptor.live_crowded_arc_skirmisher_mcw` -> `.userdata/dev-harness/harness_runs/20260501_053414/` proves feature-path crowded-arc behavior: a staged raptor avoids the zombie-crowded north arc and selects the under-occupied south/open arc before bounded swoops.
- `flesh_raptor.live_blocked_corridor_skirmisher_mcw` -> `.userdata/dev-harness/harness_runs/20260501_054807/` proves feature-path blocked/corridor behavior: lateral/behind orbit sample tiles sealed, east lane open, `decision=fallback reason=no_readable_lateral_orbit`, then bounded corridor swoop/committed path pressure.
- `flesh_raptor.live_equipment_frustration_comparison_mcw` -> `.userdata/dev-harness/harness_runs/20260501_062300/` proves the old-feeling/equipment-damage comparison row with step ledger green (`7/7`), `artifacts_matched`, `feature_proof True`, clean startup, no runtime warnings, cleanup terminated, current orbit/swoop/melee debug metrics, and screenshot/OCR player-facing evidence: `flesh-raptor`, `impales`, `cut!`, `You put pressure on the bleeding wound...`, and `You're bleeding!`.

Closure verdict: Frau accepted v0 for agent-side close with staged-but-live caveats. Optional Josef taste/playtest remains future-only and is not a blocker. No equipment-damage tuning changed; equipment damage remains an observational frustration metric.

### Closed validation receipt - CAOL-BANDIT-SCENIC-SHAKEDOWN-CHAT-OPENINGS-v0

`CAOL-BANDIT-SCENIC-SHAKEDOWN-CHAT-OPENINGS-v0` is closed/checkpointed green v0. Proof/readout: `doc/bandit-scenic-shakedown-chat-window-openings-proof-v0-2026-05-02.md`; contract: `doc/bandit-scenic-shakedown-chat-window-openings-packet-v0-2026-05-01.md`.

Credited evidence:
- `git diff --check` -> clean.
- `make -j4 tests src/do_turn.o src/bandit_live_world.o tests/bandit_live_world_test.o LINTJSON=0 ASTYLE=0` -> green.
- `./tests/cata_test "[bandit][live_world][shakedown]"` -> `All tests passed (136 assertions in 4 test cases)`.
- `bandit.extortion_first_demand_pay_mcw` -> `.userdata/dev-harness/harness_runs/20260502_065253/`: feature-path green, screenshot `advance_final_turn_to_first_shakedown.after.png`, `opening=basecamp_pressure`, `shakedown_surface_dialogue_window opening=basecamp_pressure responses=pay/fight/refuse`, pay path green.
- `bandit.extortion_reopened_demand_mcw` -> `.userdata/dev-harness/harness_runs/20260502_065445/`: feature-path green, screenshot `advance_final_turn_to_reopened_shakedown.after.png`, `opening=reopened_demand`, `shakedown_surface_dialogue_window opening=reopened_demand responses=pay/fight/refuse`, higher-demand reopen artifact green.
- `cannibal.live_world_exposed_sight_avoid_mcw` -> `.userdata/dev-harness/harness_runs/20260502_065927/`: feature-path green, `profile=cannibal_camp`, `active_job=stalk`, `shakedown=no`, `combat_forward=no`.
- Review screenshot copies live under `/Users/josefhorvath/.openclaw/workspace/runtime/caol-bandit-scenic-review-20260502/`.

### Closed validation receipt - CAOL-WRITHING-STALKER-HIT-FADE-RETREAT-DISTANCE-v0

`CAOL-WRITHING-STALKER-HIT-FADE-RETREAT-DISTANCE-v0` is closed/checkpointed green v0. Proof/readout: `doc/writhing-stalker-hit-fade-retreat-distance-proof-v0-2026-05-02.md`; contract: `doc/writhing-stalker-hit-fade-retreat-distance-packet-v0-2026-05-02.md`.

Credited evidence:
- original flesh-raptor reference checked from current source: flesh raptor still uses generic `HIT_AND_RUN`, whose melee hook adds `effect_run`; stalker intentionally dropped that flag because it forced one-hit fleeing before bounded bursts;
- `git diff --check && make -j8 tests LINTJSON=0 ASTYLE=0 && ./tests/cata_test "[writhing_stalker]"` -> `All tests passed (206 assertions in 17 test cases)`;
- `./just_build_macos.sh > /tmp/caol-writhing-stalker-hit-fade-tiles-build-current.log 2>&1` exited `0`; `cataclysm-tiles` linked;
- `writhing_stalker.live_hit_fade_retreat_mcw` -> `.userdata/dev-harness/harness_runs/20260502_113738/`: `feature_proof=true`, `evidence_class=feature-path`, `verdict=artifacts_matched`, `step_ledger_status=green_step_local_proof`, no runtime warnings, clear weather, and matched `decision=strike`, `decision=withdraw`, `decision=cooling_off`, `burst=0/2`, `burst=1/2`, `burst=2/2`, `retreat_distance=8`, `cooldown=yes`.

Caveats: staged-but-live McWilliams proof, not natural random discovery; yellow watched seed `.userdata/dev-harness/harness_runs/20260502_015032/` remains debug footing only because of runtime-version mismatch; final human taste is optional/future-only.

### Closed validation receipt - CAOL-CAMP-LOCKER-EQUIPMENT-API-REDUCTION-v0

`CAOL-CAMP-LOCKER-EQUIPMENT-API-REDUCTION-v0` is closed/checkpointed green v0 after the scoped camp-locker API-reduction pass. Contract: `doc/camp-locker-equipment-api-reduction-packet-v0-2026-05-01.md`; imagination source: `doc/anti-redundancy-packaging-imagination-source-of-truth-2026-05-01.md`.

Credited evidence:
- Quality-feedback line-id audit is source-inspection only and does not require compile: `basecamp.h:116` / `basecamp.h:122` are legacy basecamp resource/fuel `ammo_id` fields, and `basecamp.h:169` is the LLM craft-request `blockers` vector; none is camp locker equipment API ontology.
- First coverage-helper reduction is green: `git diff --check`; `make -j4 tests/faction_camp_test.o tests src/basecamp.o LINTJSON=0 ASTYLE=0 && ./tests/cata_test "[camp][locker]"` -> `All tests passed (2062 assertions in 71 test cases)`.
- Zone-boundary reduction is green: `collect_camp_locker_zone_items` now defers the pickup boundary to the existing `NO_NPC_PICKUP` zone API before considering `CAMP_LOCKER` stock; `camp_locker_zone_candidate_gathering` covers the overlapped-zone regression; `./tests/cata_test "[camp][locker]"` -> `All tests passed (2062 assertions in 71 test cases)`.
- Medical-readiness reduction is green: camp locker readiness/kept-supply logic now recognizes bandage/bleed supplies through existing `heal_actor` use-action metadata instead of a camp-local item id list; `camp_locker_service_readies_medical_consumables_from_locker_supply`, `camp_locker_service_preserves_carried_medical_and_caps_reserve`, and `camp_locker_service_ignores_unrelated_medical_consumables` -> `All tests passed (37 assertions in 3 test cases)`; full `./tests/cata_test "[camp][locker]"` after this slice -> `All tests passed (2066 assertions in 71 test cases)`.
- Medical use-action lookup reduction is green: the heal-supply helper now asks existing `item::get_usable_item("heal")` / `item::get_use("heal")` for the direct item use-action instead of reading `itype::get_use()` through raw item type data, while preserving the camp policy that only direct bandage/bleed supplies count as readiness stock. Gates: `git diff --check`; `make -j4 tests/faction_camp_test.o tests src/basecamp.o LINTJSON=0 ASTYLE=0`; focused medical/cleanup regressions `camp_locker_service_readies_medical_consumables_from_locker_supply,camp_locker_service_preserves_carried_medical_and_caps_reserve,camp_locker_service_ignores_unrelated_medical_consumables,camp_locker_service_dumps_carried_junk_outside_curated_locker_stock` -> `All tests passed (67 assertions in 4 test cases)`.
- Ranged-readiness reduction is green: carried compatible magazine discovery now defers to `Character::find_ammo()` / reload compatibility instead of manually visiting every worker item, while locker-side magazine/ammo selection remains camp policy; focused regression `camp_locker_ranged_readiness_ignores_magazines_installed_in_carried_guns` -> `All tests passed (15 assertions in 1 test case)`; full `./tests/cata_test "[camp][locker]"` after this slice -> `All tests passed (2081 assertions in 72 test cases)`.
- Average-coverage scoring reduction is green: `average_armor_coverage` now defers to `item::get_avg_coverage()` instead of camp-local armor-portion averaging; `git diff --check`; `make -j4 tests/faction_camp_test.o tests src/basecamp.o LINTJSON=0 ASTYLE=0 && ./tests/cata_test "[camp][locker]"` plus rerun with captured tail -> `All tests passed (2081 assertions in 72 test cases)`.
- Worker-item collection / carried-cleanup enumeration reduction is green: `collect_camp_locker_current_items`, `collect_camp_locker_worker_items`, and carried-cleanup summary collection now defer to the existing visitable `items_with()` API instead of local `visit_items()` collection loops, while worn/wielded and kept/dump policy stays explicit; `git diff --check && make -j4 tests/faction_camp_test.o tests src/basecamp.o LINTJSON=0 ASTYLE=0 && ./tests/cata_test "[camp][locker]"` -> `All tests passed (2081 assertions in 72 test cases)`.
- Direct subpart coverage API reduction is green: `armor_specifically_covers_any()` now calls `item::covers(sub_bodypart_id, false)` directly instead of building/searching `get_covered_sub_body_parts()`, while preserving explicit camp slot policy; commit `27e27372d3`; `git diff --check && make -j4 tests/faction_camp_test.o tests src/basecamp.o LINTJSON=0 ASTYLE=0 && ./tests/cata_test "[camp][locker]"` -> `All tests passed (2081 assertions in 72 test cases)`.
- Item-encumbrance scoring reduction is green: live camp locker planning passes the worker as fit context into scoring so clothing/armor encumbrance penalties use existing `item::get_avg_encumber(..., item::encumber_flags::assume_empty)` instead of camp-local armor-portion averaging; no-context helper/test calls retain the previous fallback. Regression `worker fit context uses item encumbrance scoring`; gates: `git diff --check`; `make -j4 tests/faction_camp_test.o tests src/basecamp.o LINTJSON=0 ASTYLE=0`; `./tests/cata_test "[camp][locker]"` -> `All tests passed (2082 assertions in 72 test cases)`.
- Worker-wearability candidate-filter reduction is green: live camp locker candidate collection now filters worn-slot candidates with `Character::can_wear(..., true)` before camp scoring/planning, so worker-specific direct-wearability rules reject unwearable armor/clothing while no-worker helper classification remains unchanged. Regression `camp_locker_service_filters_unwearable_armor_candidates`; gates: `git diff --check`; `make -j4 tests/faction_camp_test.o tests src/basecamp.o LINTJSON=0 ASTYLE=0`; `./tests/cata_test "camp_locker_service_filters_unwearable_armor_candidates"` -> `All tests passed (9 assertions in 1 test case)`; full `./tests/cata_test "[camp][locker]"` -> `All tests passed (2091 assertions in 73 test cases)`.
- Service live-state reuse reduction is green: the live service pre-pass and post-service summary now use `collect_camp_locker_live_state()` instead of open-coded worker item, locker zone item, candidate, plan, cleanup, ranged-readiness, and medical-readiness collection. Gates: `git diff --check`; `make -j4 tests/faction_camp_test.o tests src/basecamp.o LINTJSON=0 ASTYLE=0`; `./tests/cata_test "[camp][locker]"` -> `All tests passed (2147 assertions in 78 test cases)`.
- Worker-wieldability candidate-filter reduction is green: live camp locker weapon-slot candidate collection now filters through `Character::can_wield()` before scoring/planning, so integrated/unwieldable weapon items remain classified by item type but are rejected from worker-specific live service candidates. Regression `camp_locker_service_filters_unwieldable_weapon_candidates`; gates: `git diff --check`; `make -j4 tests/faction_camp_test.o tests src/basecamp.o LINTJSON=0 ASTYLE=0`; `./tests/cata_test "camp_locker_service_filters_unwieldable_weapon_candidates"` -> `All tests passed (9 assertions in 1 test case)`; full `./tests/cata_test "[camp][locker]"` -> `All tests passed (2101 assertions in 74 test cases)`.
- Ranged-classification reduction is green: camp locker ranged-weapon classification now defers to existing `item::is_gun()` instead of the narrower firearm-only predicate, so primitive ranged weapons are classified through engine item ontology while camp policy still decides whether the ranged slot is enabled and whether the live worker can wield the candidate. Regression adds crossbow coverage to `camp_locker_item_classification`; gates: `git diff --check`; `make -j4 tests/faction_camp_test.o tests src/basecamp.o LINTJSON=0 ASTYLE=0`; `./tests/cata_test "camp_locker_item_classification"` -> `All tests passed (45 assertions in 1 test case)`; full `./tests/cata_test "[camp][locker]"` -> `All tests passed (2102 assertions in 74 test cases)`.
- Armor-classification reduction is green: camp locker armor/clothing classification now defers its armor boundary to existing `item::is_armor()` instead of raw armor-slot lookup, preserving camp slot policy while respecting engine armor ontology. Gates: `git diff --check`; `make -j4 tests/faction_camp_test.o tests src/basecamp.o LINTJSON=0 ASTYLE=0`; `./tests/cata_test "camp_locker_item_classification"` -> `All tests passed (45 assertions in 1 test case)`; full `./tests/cata_test "[camp][locker]"` -> `All tests passed (2102 assertions in 74 test cases)`.
- Worker-context weapon-scoring reduction is green: live melee/ranged weapon scoring now uses `Character::evaluate_weapon(..., true)` when `score_camp_locker_item()` has a worker fit context, preserving the no-context fallback for helper calls while deferring live weapon value to the existing character weapon API. Regression `camp_locker_weapon_scoring_uses_character_weapon_value`; gates: `git diff --check`; `make -j4 tests/faction_camp_test.o tests src/basecamp.o LINTJSON=0 ASTYLE=0`; `./tests/cata_test "camp_locker_weapon_scoring_uses_character_weapon_value"` -> `All tests passed (4 assertions in 1 test case)`; full `./tests/cata_test "[camp][locker]"` -> `All tests passed (2106 assertions in 75 test cases)`.
- Worker-context reload-viability reduction is green: camp locker ammo candidate readiness now uses `Character::can_reload()` when worker context exists, so engine-owned reload constraints such as ammo-belt linkages gate whether locker ammo can ready a magazine/weapon while no-worker helper fallback keeps `item::can_reload_with()`. Regression `camp_locker_ranged_readiness_defers_worker_reload_rules`; gates: `git diff --check`; `make -j4 tests/faction_camp_test.o tests src/basecamp.o LINTJSON=0 ASTYLE=0`; `./tests/cata_test "camp_locker_ranged_readiness_defers_worker_reload_rules"` -> `All tests passed (6 assertions in 1 test case)`; full `./tests/cata_test "[camp][locker]"` -> `All tests passed (2112 assertions in 76 test cases)`.
- Reload-supply ammo-state reduction is green: locker reload supply selection now filters/sorts compatible supply items by existing `item::ammo_remaining()` instead of an ammo-only `is_ammo()` / `charges` heuristic, so loaded speedloaders and similar reload-compatible supplies can ready carried weapons through the existing reload API while camp policy still controls locker source/leftover handling. Regression `camp_locker_ranged_readiness_uses_existing_reload_supply_api`; gates: `git diff --check`; `make -j4 tests/faction_camp_test.o tests src/basecamp.o LINTJSON=0 ASTYLE=0`; focused regression -> `All tests passed (13 assertions in 1 test case)`; full `./tests/cata_test "[camp][locker]"` -> `All tests passed (2125 assertions in 77 test cases)`.
- Magazine total-capacity reduction is green: compatible magazine preference now composes existing item ammo-state APIs (`item::ammo_remaining()` plus `item::remaining_ammo_capacity()`) instead of camp-local `ammo_data()` / `ammo_default()` / `item::find_type()` fallback plumbing, so empty and partially loaded magazine capacity ranking stays item-owned during locker readiness selection. Regression `camp_locker_ranged_readiness_prefers_existing_magazine_capacity_api`; gates: `git diff --check`; `make -j4 tests/faction_camp_test.o tests src/basecamp.o LINTJSON=0 ASTYLE=0`; focused ranged-readiness regressions `camp_locker_ranged_readiness_prefers_existing_magazine_capacity_api,camp_locker_service_readies_ranged_loadouts_from_locker_supply,camp_locker_ranged_readiness_uses_existing_reload_supply_api` -> `All tests passed (45 assertions in 3 test cases)`; full `./tests/cata_test "[camp][locker]"` -> `All tests passed (2147 assertions in 78 test cases)`.
- Reloadability-gate reduction is green: ranged readiness/reload selection now calls existing `item::can_reload_with()` / `Character::can_reload()` to decide reloadability instead of first rejecting targets by camp-local `remaining_ammo_capacity()` prechecks, preserving camp policy for which locker supplies to move. Gates: `git diff --check`; `make -j4 tests/faction_camp_test.o tests src/basecamp.o LINTJSON=0 ASTYLE=0`; focused ranged-readiness regressions `camp_locker_ranged_readiness_uses_existing_reload_supply_api,camp_locker_ranged_readiness_defers_worker_reload_rules,camp_locker_ranged_readiness_prefers_existing_magazine_capacity_api` -> `All tests passed (32 assertions in 3 test cases)`; full `./tests/cata_test "[camp][locker]"` -> `All tests passed (2138 assertions in 78 test cases)`.
- Magazine default-ammo reduction is green: empty-magazine capacity ranking no longer reads magazine ammo-type sets in camp code; total capacity now goes through `item::remaining_ammo_capacity()`, whose item-side fallback owns the default-ammo capacity choice. Gates: `git diff --check`; `make -j4 tests/faction_camp_test.o tests src/basecamp.o LINTJSON=0 ASTYLE=0`; `./tests/cata_test "camp_locker_ranged_readiness_prefers_existing_magazine_capacity_api"` -> `All tests passed (13 assertions in 1 test case)`.
- Managed-ranged helper reduction is green: `is_camp_locker_managed_ranged_weapon()` now asks existing `item::is_gun()` directly instead of re-entering camp locker classification, while slot enablement remains camp policy. Gates: `git diff --check`; `make -j4 tests/faction_camp_test.o tests src/basecamp.o LINTJSON=0 ASTYLE=0`; `./tests/cata_test "camp_locker_service_readies_ranged_loadouts_from_locker_supply"` -> `All tests passed (18 assertions in 1 test case)`; full `./tests/cata_test "[camp][locker]"` -> `All tests passed (2138 assertions in 78 test cases)`.
- Ablative-insert compatibility reduction is green: carried cleanup now preserves armor inserts only when an existing ablative carrier pocket reports it can contain the item, instead of using the raw `CANT_WEAR` flag as local ontology; ordinary carried armor still dumps to camp storage. Gates: `git diff --check`; `make -j4 tests/faction_camp_test.o tests src/basecamp.o LINTJSON=0 ASTYLE=0`; focused carried-cleanup regression -> `All tests passed (30 assertions in 1 test case)`; full `./tests/cata_test "[camp][locker]"` -> `All tests passed (2146 assertions in 78 test cases)`.
- Ranged ammo-state readiness reduction is green: ready/loaded checks now use existing `item::has_ammo()` instead of local `ammo_remaining() > 0` / `<= 0` predicates when selecting loaded magazines and deciding whether a managed ranged weapon still needs immediate reloading. Gates: `git diff --check`; `make -j4 tests/faction_camp_test.o tests src/basecamp.o LINTJSON=0 ASTYLE=0`; focused ranged-readiness regressions -> `All tests passed (45 assertions in 3 test cases)`; full `./tests/cata_test "[camp][locker]"` -> `All tests passed (2147 assertions in 78 test cases)`.
- Shared zone-tile collection reduction is green: camp storage and camp locker tile collection now share one thin `zone_manager::get_near()` adapter plus common deterministic sorting, keeping explicit `CAMP_STORAGE` / `CAMP_LOCKER` policy while removing duplicated local zone lookup boilerplate. Gates: `git diff --check`; `make -j4 tests/faction_camp_test.o tests src/basecamp.o LINTJSON=0 ASTYLE=0`; full `./tests/cata_test "[camp][locker]"` -> `All tests passed (2147 assertions in 78 test cases)`.
- Outer-layer classification reduction is green: weather-sensitive outerwear, leg accessory, outer one-piece garment, and general clothing slot classification now use existing armor layer data (`item::has_layer({ layer_level::OUTER })`) instead of combining it with the raw `OUTER` flag as a second local outerwear ontology. Gates: `git diff --check`; `make -j4 tests/faction_camp_test.o tests src/basecamp.o LINTJSON=0 ASTYLE=0`; focused outerwear/one-piece/classification tests -> `All tests passed (100 assertions in 6 test cases)`; full `./tests/cata_test "[camp][locker]"` -> `All tests passed (2147 assertions in 78 test cases)`.
- Body-part outer-layer classification reduction is green: weather-sensitive outerwear, leg accessories, outer one-piece garments, and heavy torso classification now ask the existing body-part-specific armor layer API (`item::has_layer(..., bodypart_id)`) instead of mixing global outer-layer checks with separate coverage checks for the same body regions. Gates: `git diff --check`; `make -j4 tests/faction_camp_test.o tests src/basecamp.o LINTJSON=0 ASTYLE=0`; focused classification/outerwear/one-piece regressions -> `All tests passed (552 assertions in 6 test cases)`; full `./tests/cata_test "[camp][locker]"` -> `All tests passed (2147 assertions in 78 test cases)`.
- Body-part id helper-call reduction is green: camp locker body-part coverage/layer helpers and plan-preservation checks now accept existing `body_part_*` ids instead of reconstructing `bodypart_str_id` from repeated local string literals; camp-specific body-region policy remains explicit. Gates: `git diff --check`; `make -j4 tests/faction_camp_test.o tests src/basecamp.o LINTJSON=0 ASTYLE=0`; full `./tests/cata_test "[camp][locker]"` -> `All tests passed (2147 assertions in 78 test cases)`.
- Sub-bodypart id helper-call reduction is green: camp locker subpart coverage helper callers now pass `sub_bodypart_str_id` constants instead of reconstructing sub-bodypart ids from repeated local call-site string literals; camp-specific sub-region policy remains explicit. Gates: `git diff --check`; `make -j4 tests/faction_camp_test.o tests src/basecamp.o LINTJSON=0 ASTYLE=0`; full `./tests/cata_test "[camp][locker]"` -> `All tests passed (2147 assertions in 78 test cases)`.
- Belted-layer leg-accessory reduction is green: camp locker leg-accessory classification now asks existing armor layer data (`item::has_layer({ layer_level::BELTED })`) instead of reading the raw `BELTED` flag for strapped armor, while `BELT_CLIP` remains explicit item clip policy. Gates: `git diff --check`; `make -j4 tests/faction_camp_test.o tests src/basecamp.o LINTJSON=0 ASTYLE=0`; focused classification/leg-accessory service regressions -> `All tests passed (70 assertions in 3 test cases)`; full `./tests/cata_test "[camp][locker]"` -> `All tests passed (2147 assertions in 78 test cases)`.
- Damage-resistance scoring reduction is green: camp locker protection and ballistic-resistance scoring now consume the shared `resistances` aggregate for bash/cut/bullet armor resistance instead of issuing separate camp-local item-resistance lookups; camp-specific weighting and explicit ablative inclusion policy remain unchanged. Gates: `git diff --check`; `make -j4 tests/faction_camp_test.o tests src/basecamp.o LINTJSON=0 ASTYLE=0`; full `./tests/cata_test "[camp][locker]"` -> `All tests passed (2147 assertions in 78 test cases)`.
- Final worn-slot equip validation reduction is green: camp locker worn-slot equip application now defers the final wearability decision to existing `Character::wear_item()` instead of doing a camp-local duplicate `can_wear()` precheck first; live candidate filtering still uses explicit worker policy before planning. Gates: `git diff --check`; `make -j4 tests/faction_camp_test.o tests src/basecamp.o LINTJSON=0 ASTYLE=0`; full `./tests/cata_test "[camp][locker]"` -> `All tests passed (2147 assertions in 78 test cases)`.
- Closure gate: `git diff --check`; `make -j4 tests/faction_camp_test.o tests src/basecamp.o LINTJSON=0 ASTYLE=0`; `./tests/cata_test "[camp][locker]"` -> green.
- Closure caveat: this is cleanup/refactor only. It does not claim basecamp mission redesign, Smart Zone redesign, outfit tuning, or new product behavior. Further locker work requires a newly promoted concrete seam.

### Closed validation receipt - CAOL-WRITHING-STALKER-BEHAVIOR-SEAM-REDUCTION-v0

`CAOL-WRITHING-STALKER-BEHAVIOR-SEAM-REDUCTION-v0` is closed/checkpointed green v0. Contract: `doc/writhing-stalker-behavior-seam-reduction-packet-v0-2026-05-01.md`; imagination source: `doc/anti-redundancy-packaging-imagination-source-of-truth-2026-05-01.md`.

Credited evidence:
- Inventory checkpoint recorded the live path: `monster::plan()` target acquisition -> writhing-stalker evaluator/quiet-side scorer -> destination/effect side effects.
- Seam-consumption proof `writhing_stalker_live_plan_consumes_quiet_side_cutoff_seam` calls live `monster::plan()` and fails if the live planner stops consuming quiet-side cutoff behavior.
- Source refactor in `src/monmove.cpp` routes writhing stalker / zombie rider / flesh raptor target-response planning through a named `targeted_live_plan_adapter` dispatch before the generic hostile/flee destination fallback. No behavior-tree or special-attack seam honestly owns this destination-planning response today, so product-specific stalker judgment stays custom.

Gates: `git diff --check`; `make -j4 tests/writhing_stalker_test.o tests src/writhing_stalker_ai.o LINTJSON=0 ASTYLE=0 && ./tests/cata_test "[writhing_stalker]"` -> `All tests passed (192 assertions in 15 test cases)`; adapter spillover guard `./tests/cata_test "[zombie_rider],[flesh_raptor]"` -> `All tests passed (231 assertions in 21 test cases)`.

### Closed validation receipt - CAOL-WRITHING-STALKER-ZOMBIE-SHADOW-PREDATOR-v0

`CAOL-WRITHING-STALKER-ZOMBIE-SHADOW-PREDATOR-v0` is closed/checkpointed green v0 after flesh-raptor closure. Contract: `doc/writhing-stalker-zombie-shadow-predator-packet-v0-2026-05-01.md`; imagination source: `doc/predator-behavior-variety-imagination-source-of-truth-2026-05-01.md`.

Credited deterministic footing:
- Code seam: `src/writhing_stalker_ai.*` adds explicit confidence components for evidence/interest, zombie pressure, quiet-side/cutoff opportunity, and counterpressure from light/focus/open exposure; `src/monmove.cpp` routes live shadow destinations through `choose_quiet_side_cutoff()` and logs confidence alongside opportunity.
- Focused tests in `tests/writhing_stalker_test.cpp` prove east-side zombie pressure prefers west/quiet-side shadow candidate, split east/west pressure avoids fake precision, retreat-route bias can choose a consistent quiet cutoff, zombie pressure is ignored without local evidence or overmap-interest footing, overmap-interest footing can admit pressure confidence, and light/focus/open exposure suppresses cutoff confidence.
- Preservation tests still cover no-evidence/no-magic targetlessness, cooldown anti-spam, light/focus withdrawal, injured retreat, repeated strike rhythm, and old monster/spawn footing.

Credited staged-but-live rows:
- `writhing_stalker.live_quiet_side_zombie_pressure_mcw` -> `.userdata/dev-harness/harness_runs/20260501_071548/` proves the scoped local-evidence-only first live row's logged decision path: `overmap_interest=no`, `zombie_pressure=3`, east/front pressure (`pressure_x=3`) maps through the live shadow-destination path to west/quiet-side cutoff (`quiet_x=-1`, first matched `chosen_rel_x=-1`, `chosen_rel_y=-4`, reason `quiet_side_cutoff_preferred`). Proof note: `doc/writhing-stalker-zombie-shadow-live-quiet-side-proof-v0-2026-05-01.md`. Later sampler audit found an `ERROR GAME` wall-location backtrace in this row's `probe.artifacts.log`, so it is dirty/caveated for Josef-facing optical footing unless rerun clean.
- `writhing_stalker.live_escape_side_zombie_retreat_mcw` -> `.userdata/dev-harness/harness_runs/20260501_071940/` proves the scoped local-evidence-only retreat row: after southward retreat input, `overmap_interest=no`, `zombie_pressure=3`, north/front pressure (`pressure_y=-3`) maps through the live shadow-destination path to south/escape-side cutoff (`quiet_y=1`, first matched `chosen_rel_y=4`, reason `quiet_side_cutoff_preferred`). Proof note: `doc/writhing-stalker-zombie-shadow-live-escape-side-proof-v0-2026-05-01.md`.

Green gates:
- `make -j4 tests/writhing_stalker_test.o tests src/writhing_stalker_ai.o LINTJSON=0 ASTYLE=0 && ./tests/cata_test "[writhing_stalker][ai]"` -> `All tests passed (135 assertions in 12 test cases)`.
- `./tests/cata_test "[writhing_stalker]"` -> `All tests passed (167 assertions in 14 test cases)`.

Closure verdict:
- Deterministic zombie-pressure / quiet-side footing is green.
- Both scoped live rows are green and explicitly **do not** claim overmap-interest footing.
- V0 proves local-evidence live zombie-shadow behavior; live overmap-interest wiring/logging remains unclaimed unless a later packet promotes it.
- Old closed writhing-stalker constraints remain preserved: no omniscience, no zombie-proximity magic target acquisition, cooldown anti-spam, light/focus counterplay, and injured retreat.
- Anti-redundancy cleanup is now a separate active refactor lane and must preserve this approved behavior rather than reopening product tuning.


### Closed validation target - CAOL-WRITHING-STALKER-PATTERN-TESTS-v0

`CAOL-WRITHING-STALKER-PATTERN-TESTS-v0` is closed/checkpointed green v0. Contract: `doc/writhing-stalker-behavior-pattern-minimap-packet-v0-2026-04-30.md`; imagination source: `doc/writhing-stalker-behavior-pattern-imagination-source-of-truth-2026-04-30.md`; proof: `doc/writhing-stalker-behavior-pattern-proof-v0-2026-04-30.md`.

Credited evidence:
- Deterministic pattern helper in `tests/writhing_stalker_test.cpp`: `stalker_pattern_row`, `trace_rows`, `pattern_base_context`, `run_vulnerable_stalker_pattern`, `count_decisions`.
- Green tests: `make -j4 tests/writhing_stalker_test.o tests LINTJSON=0 ASTYLE=0 && ./tests/cata_test "[writhing_stalker][ai]"` -> `All tests passed (97 assertions in 8 test cases)`.
- Broader focused filter: `./tests/cata_test "[writhing_stalker]"` -> `All tests passed (129 assertions in 10 test cases)`.
- Trace extraction: `./tests/cata_test "writhing_stalker_pattern_helper_traces_repeated_strikes_then_injured_retreat" -s` -> `All tests passed (16 assertions in 1 test case)`.

Credited behavior-pattern facts:
- no believable local evidence -> `ignore`, `live_no_believable_evidence`;
- weak evidence latch times out via `latch_timed_out`;
- cover route is preferred over direct open route;
- bright/focused exposure withdraws with `live_exposed_and_focused_withdraw`;
- zombie pressure alone does not create a strike;
- vulnerability can create `live_vulnerability_window_strike`;
- repeated trace shows `shadow -> strike -> cooling_off -> cooling_off -> shadow -> strike -> withdraw`;
- strike count `2`, retreat trigger `hp=50`, jitter count `0`.

Caveat: this is deterministic helper proof, not a new live harness scene. No behavior code changed; the existing live seam remains `monster::plan()` -> `apply_writhing_stalker_plan()` -> `writhing_stalker::evaluate_live_response()`.

### Closed validation receipt - CAOL-MULTI-CAMP-SIGNAL-GAUNTLET-v0

`CAOL-MULTI-CAMP-SIGNAL-GAUNTLET-v0` is closed/checkpointed green v0. Contract: `doc/multi-camp-signal-gauntlet-playtest-packet-v0-2026-04-30.md`; imagination source: `doc/multi-camp-signal-gauntlet-imagination-source-of-truth-2026-04-30.md`; proof: `doc/multi-camp-signal-gauntlet-proof-v0-2026-04-30.md`.

Green harness runs:
- Challenge A: `bandit.multi_camp_structural_stress_mcw`, `.userdata/dev-harness/harness_runs/20260430_204416/` — two-camp structural stress, bounded wait, spread/no-repeat/followthrough, and perf/log stability.
- Challenge B: `bandit.mixed_signal_coexistence_mcw`, `.userdata/dev-harness/harness_runs/20260430_203757/` — live smoke/fire signal plus separate structural bounty, `candidate_reason=live_signal`, both state classes preserved.
- Challenge C: `bandit.mixed_signal_reload_resume_mcw`, `.userdata/dev-harness/harness_runs/20260430_203944/` — no-fixture reload/resume of active live-signal scout and structural scavenge groups.

Do not reopen this target without explicit Schani/Josef promotion. Caveats: two camps rather than four; staged-but-live signal source; Challenge A is not an all-camps-idle-after-harvest proof.


### Closed validation receipt - CAOL-ROOF-HORDE-NICE-FIRE-v0

`CAOL-ROOF-HORDE-NICE-FIRE-v0` is closed/checkpointed green for v0. Contract: `doc/roof-fire-horde-nice-roof-fire-playtest-packet-v0-2026-04-30.md`; imagination source: `doc/roof-fire-horde-nice-roof-fire-imagination-source-of-truth-2026-04-30.md`; closure proof: `doc/roof-fire-horde-nice-roof-fire-proof-v0-2026-04-30.md`.

Credited run: `.userdata/dev-harness/harness_runs/20260430_191556/` using scenario `bandit.roof_fire_horde_nice_roof_fire_mcw` and fixture `roof_fire_horde_split_wait_from_player_fire_v0_2026-04-29`. Source roof-fire footing remains `.userdata/dev-harness/harness_runs/20260429_172847/`, with prior split proof `.userdata/dev-harness/harness_runs/20260429_180239/`.

Credited evidence:
- Saved roof/elevated fire before/after wait: `t_tile_flat_roof` + `f_brazier` + `fd_fire`, no new fixture `fd_fire` transform, tied to the source normal player-action roof-fire chain.
- Horde before wait: `mon_zombie` at offset `[0,-120,0]`, destination self, `tracking_intensity=0`, `last_processed=0`, `moves=0`; this is footing only.
- Bounded wait: `5m`, turn delta `300` (`5266942` -> `5267242`), wait ledger green.
- Same-run live roof-fire horde signal: `bandit_live_world horde light signal ... source_omt=(140,41,1) horde_signal_power=20 ... elevated_exposure_extended=yes`.
- Horde response after wait: destination retargeted to `[3360,984,1]`, `last_processed=5267242`, `moves=8400`, status `required_state_present`.
- Cost/stability: harness wall-clock `2:34.72`, `14/14` step rows green, `1/1` wait rows green, no runtime warnings/abort; horde-specific timing `not instrumented`.

Preserved caveats: no positive `tracking_intensity` claim, no horde-specific micro-timing, no broader combat/pathfinding or natural multi-day horde discovery claim. Do not rerun this proof as ritual unless Schani/Josef explicitly promote a stricter follow-up.

### Closed validation receipt - CAOL-WRITHING-STALKER-v0

`CAOL-WRITHING-STALKER-v0` is closed/checkpointed green for v0. Receipts: `doc/work-ledger.md`, `SUCCESS.md`, raw intake `doc/writhing-stalker-raw-intake-2026-04-30.md`, imagination source `doc/writhing-stalker-imagination-source-of-truth-2026-04-30.md`, contract `doc/writhing-stalker-behavior-packet-v0-2026-04-30.md`, testing ladder `doc/writhing-stalker-playtest-ladder-v0-2026-04-30.md`, and mixed-hostile metrics packet `doc/mixed-hostile-stalker-horde-performance-playtest-v0-2026-04-30.md`.

Credited evidence:
- Monster/stat/spawn footing and rare singleton `GROUP_ZOMBIE` entry validate with JSON checks, focused tests, and `./tests/cata_test "[writhing_stalker]"`.
- Deterministic substrate in `src/writhing_stalker_ai.*` covers interest, latch decay/leash, cover/darkness approach, opportunity, withdrawal, and cooldown behavior.
- Live monster-plan seam in `src/monmove.cpp` routes `mon_writhing_stalker` through local-evidence-gated shadow/strike/withdraw planning without new persisted latch state.
- Support footing: `writhing_stalker.live_spawn_footing_mcw` -> `.userdata/dev-harness/harness_runs/20260430_161342/` and `writhing_stalker.live_plan_seam_mcw` -> `.userdata/dev-harness/harness_runs/20260430_161535/` prove saved active-monster footing, target acquisition, and the live `live_plan` seam. These support runs do not by themselves close behavior.
- Behavior proofs: `writhing_stalker.live_exposed_retreat_mcw` -> `.userdata/dev-harness/harness_runs/20260430_163626/`; `writhing_stalker.live_shadow_strike_mcw` -> `.userdata/dev-harness/harness_runs/20260430_170528/`; `writhing_stalker.live_no_omniscient_beeline_mcw` -> `.userdata/dev-harness/harness_runs/20260430_173555/`.
- Mixed-hostile metrics/tuning proof: `performance.mixed_hostile_stalker_horde_mcw` -> `.userdata/dev-harness/harness_runs/20260430_181748/`; the run proves active bandit and cannibal stalk jobs, one `mon_writhing_stalker`, and one nearby `mon_zombie` horde, then completes `500` sampled turns plus bounded `30m` wait. Metrics include avg `236.239ms/turn`, hostile cadence `total_us` max `3777`, stalker `eval_us` max `54`, no crash/stderr/debug-error flood, and the tuning readout.

Preserved caveat: Frau accepted the mixed-hostile horde-attribution caveat for v0 closure. Horde presence/setup is proven in `20260430_181748`, but direct horde movement/retarget cost is not instrumented and remains future-only unless Schani/Josef explicitly promotes stricter horde attribution. Do not rerun the same mixed-hostile soup as ritual.

### Closed validation receipt - CAOL-BANDIT-STRUCT-BOUNTY-v0

`CAOL-BANDIT-STRUCT-BOUNTY-v0` is closed/checkpointed green for v0. Closure readout: `doc/bandit-structural-bounty-phase-7-closure-readout-2026-04-30.md`. Canonical contract: `doc/bandit-structural-bounty-overmap-completion-packet-v0-2026-04-30.md`; testing ladder: `doc/bandit-structural-bounty-overmap-testing-ladder-v0-2026-04-30.md`; receipt: `doc/work-ledger.md`.

Credited live run: `.userdata/dev-harness/harness_runs/20260430_115157/`. Credited live claims: idle camp structural forest bounty dispatch without player smoke/light/direct-range bait; same-run candidate economics (`bounty=8`, `known_threat=0`, `confidence=3`, `effective_interest=11`, `decision=scavenge`); stalking-distance keep-open; arrival/harvest; `arrivals=1`; `members_returned=1`; saved harvested/no-repeat state. Non-credit run `.userdata/dev-harness/harness_runs/20260430_114106/` remains red/inconclusive and must not be reused as closure evidence.

Future structural-bounty playtests are greenlit as optional follow-ups, not blockers: live dangerous/turnback Branch A, live reload-resume, two/four-camp wait stress, mixed signal coexistence, and less-blessed natural-ish idle samples.

Recent completed validation targets have been moved to compact receipts in `doc/work-ledger.md` plus their canonical aux/proof docs. Do not re-expand them here unless one becomes the active validation target again.

Current important receipts:
- `CAOL-ROOF-HORDE-NICE-FIRE-v0` — focused nice roof-fire horde feature-path proof.
- `CAOL-SZM-LIVE-LABEL-v0` — Smart Zone live coordinate-label proof.
- `CAOL-REAL-FIRE-SIGNAL-v0` — real player fire -> bandit signal response.
- `CAOL-ROOF-HORDE-SPLIT-v0` — split-run roof-fire horde response.
- `CAOL-CANNIBAL-NIGHT-RAID-v0` and `CAOL-CANNIBAL-CONFIDENCE-PUSH-v0` — cannibal behavior closure/confidence uplift.
- `CAOL-BANDIT-CAMP-MAP-RISK-REWARD-v0` — scoped bandit camp-map risk/reward checkpoint.
- `CAOL-FUEL-RED-MULTIDROP-v0` and `CAOL-BANDIT-LIVE-SIGNAL-SITE-BOOTSTRAP-v0` — non-credit/superseded surfaces that must not be reopened by drift.

## Pending probes

Active same-lane next action is the remaining audit/refactor pass for `CAOL-CAMP-LOCKER-EQUIPMENT-API-REDUCTION-v0` after the coverage-helper, zone-boundary, medical-readiness, ranged-readiness, average-coverage, worker-item collection, direct subpart coverage, item-encumbrance, worker-wearability, service live-state reuse including the post-service summary, weapon-wieldability, ranged-classification, armor-classification, weapon-scoring, worker-context reload-readiness, reload-supply ammo-state, magazine-capacity, reloadability-gate, magazine default-ammo, managed-ranged helper, ablative-insert compatibility, ranged ammo-state readiness, shared zone-tile collection, outer/body-part layer classification, final worn-slot equip validation, direct medical use-action lookup, magazine total-capacity, body-part id, and sub-bodypart id reductions.

Required remaining audit questions:
- Which scoring, carried cleanup, and ranged ammo/magazine readiness checks are duplicate engine ontology rather than camp policy?
- Which existing item, wearability, body coverage, reload, and zone APIs can safely own those checks?
- Which behavior is deliberately camp-specific policy and must remain explicit: enabled slots, bulletproof/weather-sensitive preference, readiness supplies, camp-storage boundaries, and safe leftover returns?
- What is the smallest focused faction/basecamp/camp-locker gate that proves preservation after the next refactor?

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
