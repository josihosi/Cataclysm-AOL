# C-AOL port/cdda-master Playtest Findings - 2026-05-19

Branch: `port/cdda-master`

Purpose: capture current actual-playtest findings while porting C-AOL onto current CDDA master.

## Fixed During Playtest

- Camp residents did not route organic board questions through basecamp handling.
  - Symptom: after assigning Katharina to camp, `what needs making` fell through to ordinary NPC LLM chat.
  - Cause: `basecamp_ai::uses_basecamp_request_routing` excluded `NPC_MISSION_CAMP_RESIDENT`.
  - Fix: include camp residents in basecamp request routing.
  - Proof: `camp_request_speech_parsing` passes; `basecamp.organic_board_speech_probe_mcw` is green with screen text showing the board response.

- Old LMOE overmap terrain ids blocked live fixture loading.
  - Fix: added obsolete terrain migrations for old `lmoe_prepperquest_*` and `lmoe_under_empty_*` ids.
  - Proof: `./cataclysm-tiles --jsonverify` passes and formerly blocked fixtures load.

- NPC opinion wound evaluation used the wrong body context.
  - Fix: NPC opinion health calculation now evaluates the requested target, not the avatar globally.
  - Proof: focused limb/opinion test passes.

## Harness Proof Fixes

- Smart Zone live audit had OCR guard drift. The actual UI flow generated zones, but text guards were too brittle.
  - Updated guards around the zone picker and deferred prompt-accept proof to the metadata trace.
  - Proof: `smart_zone.live_probe_harness_audit_retry_no_setup` is green, with `placed_zones=23`.

- Organic board speech scenario expected structured board log packets even though organic replies use spoken board text.
  - Updated it to prove camp-resident routing plus screen text.

- Writhing stalker door/light escape expected one stale cooldown reason string.
  - Updated the proof to require the behavior: withdraw, then cooling off, no strike.

- Flesh raptor debug-spawn probe originally used `flesh raptor`; the live wish menu needs `flesh-raptor`.
  - New scenario `flesh_raptor.live_debug_spawn_visible_skirmisher_mcw` proves the live seam independently from stale saved fixtures.

- Batch 4 proof contracts found three stale expectations and one keymap/setup mismatch.
  - `bandit.local_sight_avoid_exposed_mcw` still expected `active_job=scout` and `recent_exposure=yes`; current behavior proves `current_exposure=yes`, `local_contact=yes`, `hold_off`, no shakedown/combat, and adjacent `sight_avoid` reposition.
  - `writhing_stalker.live_alley_predator_mcw` and `writhing_stalker.live_wounded_predator_mcw` still expected the old cooldown reason `live_latch_cooldown_blocks_relatched_pressure`; current code emits `live_cooldown_blocks_repeat_strike`.
  - Proof fixes reran green:
    - `bandit.local_sight_avoid_exposed_mcw` run `20260519_015109`
    - `writhing_stalker.live_alley_predator_mcw` run `20260519_015141`
    - `writhing_stalker.live_wounded_predator_mcw` run `20260519_015243`

- Batch 6/7 proof-contract updates:
  - `writhing_stalker.live_shadow_strike_mcw` was modernized from the stale cooldown reason to `live_cooldown_blocks_repeat_strike` and its strike window was widened from 80 to 180 turns. It still did not go green; keep it as a caution rather than claiming a proof.
  - `zombie_rider.live_open_field_pressure_mcw` required `distance=5` for the bow-pressure line. Batch 7 showed the behavior can legitimately happen at nearby bow range instead. The proof now requires `decision=bow_pressure`, `reason=line_of_fire`, `line_of_fire=yes`, and `eval_us=` without exact distance.
  - Proof fix reran green: `zombie_rider.live_open_field_pressure_mcw` run `20260519_025415`.

## Green Live Proofs So Far

- Basecamp and camp UI:
  - follower rules toggle and save persistence
  - organic board speech for `what needs making`
  - locker zone manager save
  - smart zone generation and row coordinate trace

- Patrol/scheduling:
  - unit and live patrol cache evidence for connected and disconnected patrol zones
  - sleep/shift logic covered by focused `[camp][patrol]` tests

- Bandit/extortion/live world:
  - first demand pay
  - first demand fight
  - pay-cancel-fight
  - three-source trade payment window
  - pay-success-save
  - reopened demand
  - no-signal control
  - mixed smoke/light signal coexistence
  - variable roster dispatch sizing
  - debug overmap threat spawn options
  - high-threat/low-reward hold
  - active-outside dogpile block
  - empty-camp retirement
  - hostile-camp toll escalation
  - exposed local-contact sight avoidance with adjacent reposition
  - smoked watcher scout/stalker hold-off
  - autonomous structural bounty dispatch/arrival/return
  - multi-camp structural stress/no-repeat pressure
  - mixed hostile performance with bandit and cannibal active jobs
  - four-site prestaged active hostile performance

- Monsters:
  - writhing stalker live plan seam
  - no omniscient beeline
  - campfire counterplay
  - door/light escape
  - alley-predator shadow/strike/cooldown cadence
  - wounded-predator withdraw/cooldown/no-strike cadence
  - zombie distraction
  - zombie rider open-field pressure
  - zombie rider cover escape
  - zombie rider no camp-light false control
  - zombie rider wounded disengagement
  - zombie rider camp-light band convergence
  - flesh raptor live debug-spawn skirmisher seam
  - writhing stalker anti-gnome bad-loiter strike
  - writhing stalker hit/fade, zombie-side pressure, and no-magic target guards

- Cannibal hostile-profile separation:
  - day smoke creates cannibal stalk pressure without bandit shakedown
  - daylight high-threat/no-cover holds off instead of suicide rushing
  - night local contact promotes to attack/combat-forward pack pressure without shakedown UI

## Remaining Caution Areas

- Some older live fixtures are stale or geometry-sensitive.
  - Flesh raptor open-field/crowded saved fixtures can load the metadata but fail to exercise live-plan logs.
  - Prefer the new debug-spawn raptor seam probe until those fixtures are restaged.

- Some scenarios are behavior-evidenced but still have older yellow proof contracts.
  - Patrol connected/disconnected live probes have useful cache evidence but weak per-step visible facts.
  - Locker package service has strong debug-log evidence but old step-local proof.
  - Bandit camp standoff reaches local-gate/shakedown evidence but wait-step proof remains yellow.

- Batch 4 found three remaining non-green or invalid probes.
  - `bandit.mixed_signal_reload_resume_mcw` loaded, but the reload audit lost the required live signal/scout state (`blocked_reload_lost_live_signal_scout_state`, run `20260519_013253`). Treat as a reload/state-persistence or fixture-transform gap until inspected further.
  - `bandit.local_scout_return_followthrough_mcw` reached startup, then stalled during the 2600-turn follow-through window and was terminated after about 9 minutes (`rc=-15`, run `20260519_013310`). Treat as a harness/performance or interaction-stall finding, not a green behavior proof.
  - `locker.display_toggle_probe` and `locker.create_zone_probe` are stale Sandy Creek probes: screenshots show the flow entering the action/basecamp UI instead of proving Zone Manager locker creation/display. Prefer `locker.zone_manager_save_probe_mcw`, which already proves the McWilliams locker Zone Manager path.

- `basecamp.package2_guard_probe_mcw` is not a basecamp-routing proof in its current fixture.
  - The actual log shows Katharina/Robbie on guard/hold with `assigned_camp=none`, `uses_basecamp=no`, and ordinary LLM prompts (`run 20260519_014813`).
  - This is expected from the current router, which requires `assigned_camp`; it becomes a product question only if package 2 is meant to make unassigned guard followers answer camp board/craft requests.

- Remembered-lead bandit dispatch has one semantic contract question.
  - Batch 7 `bandit.variable_roster_large_cooled_dispatch_sizing_live` and `bandit.camp_map_vanished_signal_redispatch` both produced live dispatch plans and saved active groups, but as `active_job_type=toll`, not the scenarios' expected `stalk`.
  - Observed saved state is not empty or lost: large-cooled persisted a toll group with 3 active members; vanished-signal persisted a toll group with 4 active members.
  - Treat this as behavior/product drift or stale scenario semantics until we decide whether remembered basecamp leads should become toll/shakedown-capable pressure or stay as stalk pressure in those fixtures.

- Real-fire/source-zone/roof-fire proof rows are currently not release-grade.
  - Batch 8 found multiple rows blocked before the actual hostile fire/horde behavior could be credited: missing saved `fd_fire`, missing deployed brazier furniture, missing filtered 2x4/plank UI trace, missing charged lighter preflight, and missing filtered lighter selector proof.
  - Treat these as harness/fixture setup failures until restaged with direct saved-map proof of the fire source. They do not currently prove a live hostile fire-signal regression.

## Batch 5 Additional Playtests

- `basecamp.package2_assign_camp_toolcall_probe_mcw` produced strong basecamp-routing artifacts after true camp assignment, but the old scenario still has yellow step proof.
  - Run `20260519_015411`.
  - Key artifact: `camp routing check npc="Katharina Leach" ... uses_basecamp=yes camp_found=yes assigned_camp=140,41,0 ... mission=11 ... reason=camp_grouped`.
  - Interpretation: true camp assignment works with the camp-resident routing fix; the yellow status is from missing per-step proof annotations, not from missing routing evidence.

- `locker.zone_manager_save_probe_mcw` is green and remains the preferred locker Zone Manager proof.
  - Run `20260519_015534`.
  - Proof: existing `Basecamp: Locker` plus newly created `Probe Locker` rows appear as `CAMP_LOCKER` in same-run UI trace and saved-zone audit.

- `locker.package5_robbie_e2e_verified_mcw` produced real locker service logs but remains yellow because the scenario lacks step-local proof.
  - Run `20260519_015644`.
  - Key artifact: `camp locker: servicing Robbie Knox queue_size=1`.
  - Interpretation: useful service evidence, but not release-grade proof until the step ledger is modernized.

- `zombie_rider.live_wounded_disengagement_mcw` is green.
  - Run `20260519_020136`.
  - Key artifact: `zombie_rider live_plan: decision=withdraw reason=wounded_rider_disengages`.

- `smart_zone.ui_entry_current_runtime_guard` is not a useful current proof in its existing profile.
  - Run `20260519_015915`.
  - Startup failed on `Tolna has no characters to load`; this is a stale profile/fixture problem, not a Zone Manager behavior result.
  - Continue using `smart_zone.live_probe_harness_audit_retry_no_setup`, which opens Zone Manager through the profile-local F1 binding and is green.

- `writhing_stalker.live_exposed_retreat_mcw` is not green.
  - Run `20260519_020051`.
  - The scenario spawned/staged the stalker but the log contains `target_probe ... target=no ... sees_player=no` instead of `writhing_stalker live_plan`.
  - Interpretation: this row does not currently exercise the live-plan seam; likely fixture geometry/visibility rather than a demonstrated monster AI regression.

- `writhing_stalker.live_high_threat_allied_light_retreat_stalk_mcw` did not start.
  - Startup error: `Fixture overmap-npcs transform needs an existing NPC template`.
  - Interpretation: fixture-transform gap, not gameplay evidence.

- `flesh_raptor.live_equipment_frustration_comparison_mcw` is not green.
  - Run `20260519_020202`.
  - The saved fixture contains `mon_spawn_raptor`, but the run produced no `flesh_raptor live_plan` or `melee_event` logs and failed the player-facing OCR guard.
  - Interpretation: consistent with the older saved flesh-raptor fixtures being stale/geometry-sensitive; use `flesh_raptor.live_debug_spawn_visible_skirmisher_mcw` as the current reliable raptor seam proof.

- Release readiness should not be claimed until the remaining yellow contracts are either modernized, retired, or explicitly accepted as non-release-blocking evidence gaps.

## Batch 6 Additional Playtests

- New green live proofs:
  - `bandit.scout_stalker_smoked_watcher_live` run `20260519_021711`: smoke-obscured watcher held off instead of opening direct combat.
  - `writhing_stalker.live_hit_fade_retreat_mcw` run `20260519_022249`: shadow/hit-fade behavior remained live.
  - `writhing_stalker.live_anti_gnome_bad_loiter_mcw` run `20260519_022351`: anti-gnome bad-loiter pressure reached `decision=strike`.
  - `writhing_stalker.live_escape_side_zombie_retreat_mcw` run `20260519_022413`: side-zombie pressure kept the stalker in shadow/retreat behavior.
  - `writhing_stalker.live_quiet_side_zombie_pressure_mcw` run `20260519_022438`: quiet zombie-side pressure stayed bounded.

- Strong evidence but not green proof:
  - `patrol.connected_live` run `20260519_020642`: cache log showed `workers=4 roster=2 active=2` on the connected zone cluster, but step proof remains yellow.
  - `patrol.disconnected_live` run `20260519_021104`: cache log showed disconnected clusters and `workers=2 roster=1 active=1`, but step proof remains yellow.
  - `bandit.extortion_at_camp_standoff_mcw` run `20260519_021634`: local gate reached `posture=hold_off`, `basecamp_or_camp=yes`, `current_exposure=yes`, `local_contact=yes`; old wait-step proof remains yellow.
  - `bandit.local_standoff_return_timeout_mcw` run `20260519_021747`: local standoff evidence appeared, but return-home proof did not complete.

- Blocked/stale proof rows:
  - `basecamp.follower_rules_menu_probe_mcw` and `basecamp.package2_dialog_rules_probe_mcw` both reached `Engagement rules:` in OCR text, but the older menu text guards failed. Likely proof OCR/keyflow drift.
  - `bandit.live_world_nearby_camp_source_zone_clean_normal_map_entry_mcw` failed `blocked_starting_ui_not_normal_map`.
  - `bandit.live_world_nearby_camp_visible_brazier_source_zone_gate_mcw` failed the look/open gate; inventory/deploy traces existed, but the proof did not reach a clean visible brazier inspection.
  - `writhing_stalker.live_shadow_strike_mcw` did not go green after widening the window and updating the cooldown reason. First run only shadowed; rerun had repeated `target=no` and no live-plan line.

## Batch 7 Additional Playtests

- Green bandit/live-world proofs:
  - `bandit.active_outside_dogpile_block_live` run `20260519_022905`: unresolved active outside group blocked a dogpile.
  - `bandit.empty_camp_retirement_live` run `20260519_022949`: empty site retirement was recorded.
  - `bandit.hostile_camp_toll_escalation_live` run `20260519_023033`: scout-confirmed basecamp lead promoted to toll pressure with reserve preserved.
  - `bandit.high_threat_low_reward_holds` run `20260519_023141`: high threat / low reward held pressure instead of escalating.
  - `bandit.variable_roster_tiny_dispatch_sizing_live` run `20260519_023224`: two-bandit camp committed the buddy pair.
  - `bandit.mixed_signal_coexistence_mcw` run `20260519_023435`: smoke and light packets coexisted in the live signal scan.

- Green monster proofs:
  - `writhing_stalker.live_campfire_counterplay_mcw` run `20260519_024815`: exposure/focus produced withdraw/cooling-off behavior.
  - `writhing_stalker.live_no_omniscient_beeline_mcw` run `20260519_024841`: target probes stayed `target=no`, guarding against magic acquisition.
  - `writhing_stalker.live_zombie_distraction_no_magic_guard_mcw` run `20260519_024928`: zombie distraction guard stayed non-omniscient.
  - `zombie_rider.live_open_field_pressure_mcw` run `20260519_025415`: after proof-contract fix, bow pressure and reposition both matched.
  - `zombie_rider.live_cover_escape_mcw` run `20260519_025113`: cover/no-line-of-fire probing stayed green.
  - `zombie_rider.live_camp_light_band_mcw` run `20260519_025151`: exposed camp light formed a rider band and circle-harass posture.

- Non-green rows worth tracking:
  - `bandit.variable_roster_large_cooled_dispatch_sizing_live` run `20260519_023307`: live and saved state show a persisted `toll` dispatch with 3 active members; scenario expected a 2-member `stalk` dispatch.
  - `bandit.camp_map_vanished_signal_redispatch` run `20260519_023351`: live and saved state show a persisted `toll` dispatch with 4 active members; scenario expected `stalk`.
  - `bandit.local_scout_return_preaged_mcw` run `20260519_023529`: yellow only, no artifact hits.
  - `bandit.live_world_nearby_camp_light_horde_mcw` run `20260519_023553`: signal scans fired at night, but all UI wait steps stayed yellow.
  - `bandit.live_world_nearby_camp_smoke_mcw` run `20260519_023915`: timed out at 540 seconds and did not produce a report. Treat as a harness/performance problem for that old smoke proof.
  - `flesh_raptor.live_blocked_corridor_skirmisher_mcw` run `20260519_024958`: saved fixture still failed to produce raptor live-plan/melee lines; consistent with the older flesh-raptor fixture cautions.

## Batch 8 Weird Playtests

- Green cannibal-profile weirdness:
  - `cannibal.live_world_day_smoke_pressure_mcw` run `20260519_025708`: relabeled camp-shaped source produced cannibal `stalk` pressure from live smoke, with `shakedown=no` and `combat_forward=no`.
  - `cannibal.live_world_day_smoke_persistence_mcw` run `20260519_025728`: same cannibal live-smoke pressure also passed save/writeback proof.
  - `cannibal.live_world_daylight_high_threat_negative_mcw` run `20260519_025756`: daylight/no-cover high-threat cannibals held off instead of suicide-rushing.
  - `cannibal.live_world_night_local_contact_pack_mcw` run `20260519_025847`: night plus local contact produced `posture=attack_now`, `combat_forward=yes`, and still `shakedown=no`.

- Green autonomous/background stress:
  - `bandit.structural_bounty_idle_camp_forest_town_mcw` run `20260519_025951`: an idle camp dispatched to a non-player forest structural bounty, performed stalking/arrival, and returned members.
  - `bandit.multi_camp_structural_stress_mcw` run `20260519_031331`: two camps scanned distinct structural leads, dispatched/applied both outings, recorded arrivals/returns, and avoided repeating the same lead.
  - `performance.mixed_hostile_stalker_horde_mcw` run `20260519_030108`: prestaged bandit + cannibal active jobs plus hostile local systems produced stable perf samples with no reload-needed flag.
  - `performance.four_site_prestaged_active_wait_30m` run `20260519_031542`: four active hostile sites stayed measurable, with `camp_style:stalk=1,cannibal_camp:stalk=3` and green wait proof.

- Weird but not full release proof:
  - `cannibal.live_world_exposed_sight_avoid_mcw` run `20260519_025816`: all step-local screen checkpoints were green, but zero artifact matches were captured. Not a proof until the local-gate artifact path is restored.
  - `bandit.roof_z_dispatch_fallback_mcw` run `20260519_025907`: live log proved the weird vertical fallback line, routing target `(140,39,5)` via ground `(140,39,0)`, but the saved-state contract failed because current behavior produced `job=toll` where the scenario expected `stalk`.
  - `theme.aftermath_reload_cleanup_mcw` run `20260519_030356`: produced live-world and saved metadata, but remained yellow due wait/OCR weakness and an NPC snapshot scan without required state.
  - `theme.mixed_hostile_camp_siege_mcw` run `20260519_030437`: handoff report was yellow because screen checkpoints were OCR-caveated, but the system audit matched bandit/cannibal active jobs, horde light signal, writhing stalker probe, zombie rider plan, and flesh raptor swoop/melee lines.
  - `theme.intact_camp_shakedown_fight_mcw` run `20260519_030527`: shakedown surface and the Fight branch were proven, and later local gate reached `posture=attack_now`/`combat_forward=yes`; however the dedicated `bandit_live_world shakedown_fight_advance ... attacked=yes` line was missing, so this is not a full fight-advance proof.

- Fire/source-zone/roof-fire non-proofs:
  - `bandit.live_world_nearby_camp_real_fire_tile_audit_mcw` run `20260519_030824`: failed saved target tile `fd_fire` proof; signal scan remained `signal_packet=no`.
  - `bandit.live_world_nearby_camp_real_fire_exact_items_tile_audit_mcw` run `20260519_031115`: failed before fuel/lighter/fire steps because the saved target tile did not prove deployed brazier furniture.
  - `bandit.live_world_nearby_camp_real_fire_exact_items_fuel_tile_audit_mcw` run `20260519_031151`: failed because the harness did not prove a visible selectable 2x4/plank row in filtered Multidrop.
  - `bandit.live_world_nearby_camp_source_zone_fire_writeback_mcw` run `20260519_031238`: failed preflight because the saved-player weapon was not a charged lighter.
  - `bandit.roof_fire_horde_player_action_mcw` run `20260519_031256`: reached deep setup but failed because the harness did not prove filtered highlighted item was the lighter before confirm.

## Verification Snapshot

- Incremental tiles rebuild passed.
- `./cataclysm-tiles --jsonverify` passed.
- `git diff --check` passed.
- Edited live scenario JSON syntax passed.
- No `cataclysm-tiles`, `startup_harness`, or `cata_test` processes left running after Batch 8.
- Focused behavior sweep passed:
  - `camp_request_speech_parsing,[camp][patrol],[writhing_stalker],[flesh_raptor],[zombie_rider],[smart_zone]`
  - 71 test cases, 4154 assertions.
