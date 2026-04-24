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

### Bandit overmap-proof rule

For the remaining bandit AI proof packets, single-turn deterministic checks are **not** enough by themselves.
The honest bar now includes real overmap-side multi-turn scenario proof, up to `500` turns where needed, with explicit per-scenario goals and tuning metrics.

---

## Current relevant evidence

Active probe obligation: `Bandit shakedown pay-or-fight surface packet v0`.
The approach / stand-off / attack-gate packet now closes honestly on current build. The shakedown packet now has a first deterministic surface helper on the owned-outing seam, but the next missing evidence is still a player-present shakedown scene wired from the completed `open_shakedown` gate branch instead of disconnected chat magic.

Fresh evidence for the active shakedown packet:
- deterministic surface: `src/bandit_live_world.{h,cpp}` now exposes `shakedown_goods_pool`, `shakedown_surface`, `build_shakedown_surface(...)`, and `render_shakedown_surface_report(...)` behind an `open_shakedown` local-gate decision
- deterministic proof: `tests/bandit_live_world_test.cpp` proves the first blunt shakedown surface reports a readable bark, explicit `pay` and `fight` options, a 35%-style painful demanded toll, Basecamp/camp inventory reach for camp scenes, current vehicle reach for off-base scenes, and direct-ambush bypass for rolling-travel contexts
- validation: `make -j4 tests`, `./tests/cata_test "[bandit][live_world][shakedown]"` (29 assertions), `./tests/cata_test "[bandit][live_world]"` (352 assertions / 14 test cases), and `git diff --check -- src/bandit_live_world.h src/bandit_live_world.cpp tests/bandit_live_world_test.cpp` passed
- honest gap: this is not yet a live player-present UI/trade invocation; the surface still needs to be connected to the real local/player-present seam before this packet can be called closed

Fresh evidence for the now-closed approach/gate packet:
- deterministic surface: `src/bandit_live_world.{h,cpp}` exposes `local_gate_input`, `local_gate_decision`, `local_gate_posture`, `choose_local_gate_posture(...)`, and `render_local_gate_report(...)` on an active owned outing
- posture coverage: `tests/bandit_live_world_test.cpp` proves the gate can choose `stalk`, `hold_off`, `probe`, `open_shakedown`, `attack_now`, and `abort` from dispatch strength versus local threat/opportunity inputs, and now checks the rendered rolling-travel `attack_now` / `combat_forward` report fields
- live seam attachment: `src/do_turn.cpp` samples Basecamp/camp-adjacent and rolling-travel local gate input from the real player-present dispatch path before assigning overmap NPC goals
- live Basecamp stand-off proof: `.userdata/dev-harness/harness_runs/20260424_035742/` uses `tmp.bandit_basecamp_standoff_gate_probe_1860` on the claimed nearby-owned-site/Basecamp save, advances across the `30_minutes` dispatch cadence, then saves the world; `dimension_data.gsav` records member `4` outbound with `last_writeback_summary = "dispatch hold_off toward player@140,41,0 via goal@140,43,0"`
- live rolling-travel proof: `.userdata/dev-harness/harness_runs/20260424_034924/` uses `tmp.bandit_rolling_travel_attack_gate_probe_1860` on the claimed nearby-owned-site road-travel save, advances across the same `30_minutes` dispatch cadence, then saves the world; `dimension_data.gsav` records member `4` outbound with `last_writeback_summary = "dispatch attack_now toward player@140,42,0 via goal@140,42,0"`, and decompressed `overmaps/o.0.0` records the matching NPC `goalx/goaly/goalz = 140/42/0` with an `omt_path` beginning at `[140,42,0]`
- harness note: both generic verdicts are `inconclusive_no_artifact_match`, but the evidence class is copied saved-world inspection; cleanup terminated the launched current-build game in both runs
- validation: `make -j4 TILES=1 SOUND=0 LOCALIZE=0 LINTJSON=0 ASTYLE=0 TESTS=0 obj/tiles/do_turn.o obj/tiles/version.o cataclysm-tiles`, `./tests/cata_test "[bandit][live_world][approach_gate]"` (36 assertions), both live probes above, and `git diff --check -- src/do_turn.cpp src/bandit_live_world.cpp tests/bandit_live_world_test.cpp tools/openclaw_harness/scenarios/tmp.bandit_rolling_travel_attack_gate_probe_1860.json` all passed

Pending proof shape for the active packet:
- wire the deterministic shakedown surface into one player-present scene from the real local gate seam
- prove the live/readable surface starts from `open_shakedown` and exposes the blunt robbery demand plus explicit `pay` / `fight` options
- keep the active proof honest about goods-pool reach: Basecamp-side scenes may include reachable Basecamp goods, off-base scenes must not
- do not claim aftermath/writeback, higher demand renegotiation, or convoy combat behavior in this packet

### Recently closed lane - Cannibal camp first hostile-profile adopter packet v0

- canonical packet: `doc/cannibal-camp-first-hostile-profile-adopter-packet-v0-2026-04-22.md`
- implementation surface: `src/bandit_live_world.{h,cpp}` now has explicit `owned_site_kind::cannibal_camp` and `hostile_site_profile::cannibal_camp`, routes `cannibal_camp` overmap-special anchors to that profile, and tracks `cannibal_hunter`, `cannibal_butcher`, and `cannibal_camp_leader` templates on the shared live-world substrate.
- profile behavior: cannibal camp keeps a larger home reserve than bandit camp, uses the shared `30_minutes` cadence, applies tighter pressure/return-clock behavior, and writes back as hungry camp pressure instead of pretending to be ordinary bandit camp pressure.
- dedicated rare anchor/theme: `data/json/overmap/overmap_terrain/overmap_terrain.json`, `data/json/overmap/overmap_special/specials.json`, `data/json/mapgen/hells_raiders/cannibal_camp.json`, `data/json/mapgen_palettes/cannibal_camp.json`, `data/json/npcs/factions.json`, `data/json/npcs/cannibals/{npc,classes}.json`, and `data/json/itemgroups/cannibal_camp.json` now define a real cannibal-camp site family instead of attaching the profile to vapor.
- deterministic side-by-side proof: `tests/bandit_live_world_test.cpp` test `bandit live world keeps cannibal camp separate from bandit camp ownership` proves a bandit camp and cannibal camp claim separate site IDs/profiles/footprints, dispatch independently, serialize active outing state separately, and preserve cannibal profile writeback fields across load.
- fresh validation:
  - `python3 -m json.tool` passed for the new cannibal-camp mapgen, palette, NPC, class, and item-group JSON files
  - `./tests/cata_test "[bandit][live_world][cannibal]"` passed: 39 assertions in 1 test case, after the first JSON consistency issue was fixed
  - `./tests/cata_test "[bandit][live_world]"` passed: 287 assertions in 12 test cases
  - `make -j4 tests && ./tests/cata_test "[bandit][live_world]"` exited 0 on the current dirty tree
  - `git diff --check` produced no whitespace/style errors
- no heavier live probe was run for this packet because the packet validation expectation preferred deterministic second-family substrate proof plus data-load consistency; richer live-play belongs after the local approach/gate layer starts using the dispatched groups.

### Recently closed lane - Hostile site profile layer packet v0

- canonical packet: `doc/hostile-site-profile-layer-packet-v0-2026-04-22.md`
- implementation surface: `src/bandit_live_world.{h,cpp}` now persists explicit `hostile_site_profile` state and routes reserve/dispatch capacity, threat/posture bias, return-clock lean, and default writeback pressure through profile rules instead of raw site-kind folklore
- deterministic side-by-side proof: `tests/bandit_live_world_test.cpp` test `bandit live world dispatch rules are driven by hostile site profile` proves a `bandit_camp` using `camp_style` and an `mx_bandits_block` using `small_hostile_site` can both dispatch from the same substrate with distinct reserve, retreat, return-clock, and pressure behavior
- fresh narrow validation:
  - `make -j4 tests` exited 0 and rebuilt/linked the test binary on the current dirty tree
  - `./tests/cata_test "[bandit][live_world][profile]"` passed: 21 assertions in 1 test case
  - `./tests/cata_test "[bandit][live_world]"` passed: 248 assertions in 11 test cases
  - `make -j4 obj/bandit_live_world.o` reported the source object up to date after the test build
  - `git diff --check -- src/bandit_live_world.h src/bandit_live_world.cpp tests/bandit_live_world_test.cpp` produced no whitespace/style errors
- no heavier live probe was run for this packet because the packet validation expectation preferred deterministic substrate/profile proof and the side-by-side behavior is visible without full live encounter theatrics

### Recently closed lane - Multi-site hostile owner scheduler packet v0

- canonical packet: `doc/multi-site-hostile-owner-scheduler-packet-v0-2026-04-22.md`
- deterministic multi-owner save/load and writeback separation lives in `tests/bandit_live_world_test.cpp` via `bandit live world keeps several hostile sites independent across save and writeback`
- fresh current-build live scheduler proof lives under `.userdata/dev-harness/harness_runs/20260424_003005/`:
  - scenario `tmp.bandit_multi_site_two_site_dispatch_probe_1860` used fixture chain `tmp_bandit_live_world_multi_site_two_claimed_centered_2026-04-24` -> `tmp_bandit_live_world_multi_site_two_claimed_2026-04-24`
  - the run disabled safe mode, advanced `1860` turns across the `30_minutes` scheduler cadence, save-quit cleanly, copied the saved world, and auto-terminated the launched game
  - copied-save inspection shows `overmap_special:bandit_camp@140,51,0` at `headcount = 14`, member `4` as `state = outbound`, `active_group_id = overmap_special:bandit_camp@140,51,0#dispatch`, `active_target_id = player@140,43,0`, and remembered mark/pressure kept on that site
  - the same copied save separately shows `overmap_special:bandit_camp@140,44,0` at `headcount = 7`, member `18` as `state = local_contact`, `active_group_id = overmap_special:bandit_camp@140,44,0#dispatch`, the same target, and its own remembered mark/pressure kept on that site
  - the generic harness verdict was `inconclusive_no_new_artifacts`, but the evidence class was copied saved-world inspection, not debug-log narration
- fresh narrow validation for this final slice passed via `make -j4 TILES=1 SOUND=1 LOCALIZE=0 LINTJSON=0 ASTYLE=0 TESTS=0 obj/tiles/do_turn.o cataclysm-tiles`, `python3 -m py_compile tools/openclaw_harness/startup_harness.py`, `python3 tools/openclaw_harness/startup_harness.py list-scenarios`, and `git diff --check`

### Recently closed lane - Bandit live-world control + playtest restage packet v0

- canonical packet: `doc/bandit-live-world-control-playtest-restage-packet-v0-2026-04-22.md`
- the first owner/headcount substrate is now landed on current tree:
  - `src/bandit_live_world.{h,cpp}` defines the saveable live owner ledger with explicit site/member/spawn-tile records
  - `map::place_npc` now claims tracked `bandit_camp`, `bandit_work_camp`, `bandit_cabin`, `mx_looters`, and `mx_bandits_block` spawns into `overmap_global_state.bandit_live_world`
  - save/load coverage now exists for that ledger through `tests/bandit_live_world_test.cpp`
- the first bounded control seam is now landed on current tree too:
  - `bandit_live_world::plan_site_dispatch(...)` and `apply_dispatch_plan(...)` turn one owned nearby site into a real bounded scout dispatch plan backed by the current saved member roster
  - site-backed camps now keep one member home by rule while micro-sites can still commit their full small roster, because dispatchable capacity comes from `site_record::dispatchable_member_capacity()` instead of folklore counts
  - `bandit_live_world::update_member_state(...)` now shrinks site and spawn-tile headcount when a member becomes `dead` or `missing`, so later writeback can reduce future dispatch capacity continuously instead of snapping back to the original claim count
  - site records now also persist one active outing summary (`active_group_id`, `active_target_id`, `active_member_ids`), and `bandit_live_world::apply_return_packet(...)` can apply a bounded handoff return packet back onto those exact owned members after save/load, clearing the active outing only when survivor accounting matches the packet instead of folklore-guessing the losses
  - `overmap_npc_move()` can now apply the dispatch plan, mark the chosen members outbound, and hand those actual NPCs a normal `NPC_MISSION_TRAVELLING` overmap route toward the nearby player target
- fresh narrow validation for the earlier reserve/writeback seam passed via:
  - touched-object compile of `obj/bandit_live_world.o`
  - exact test-object compile of `tests/bandit_live_world_test.cpp`
  - `git diff --check`
- the first bounded live aftermath observer is now landed on current tree too:
  - `overmap_npc_move()` now inspects active owned outing members on the real NPC seam and records bounded `local_contact`, `dead`, and home-return observations instead of leaving the persisted outing summary inert once the dispatched bodies enter play
  - `bandit_live_world::resolve_active_group_aftermath(...)` converts those observations into a bounded return packet only after every active member is resolved, so the owner no longer has to folklore-reconstruct which exact members died or made it back
  - dead observed members can now shrink site/spawn-tile headcount on the live seam before the outing clears, while fully resolved aftermath still reuses the exact-member return-packet writeback path
- fresh narrow validation for that live-aftermath hook passed via:
  - touched-object compile of `obj/tiles/bandit_live_world.o`
  - touched-object compile of `obj/tiles/do_turn.o`
  - exact current-build compile of `tests/bandit_live_world_test.cpp`
  - `git diff --check`
- earlier owner-ledger-only proof also passed on the current tree via:
  - touched-object compile of `obj/{bandit_live_world,mapgen,overmapbuffer,savegame}.o`
  - standalone filtered run `./tests/cata_test_bandit_live_world "[bandit][live_world]"`
- the harness mode split is now real too, and the helper surface grew one bounded nearby-restage transform:
  - `tools/openclaw_harness/startup_harness.py handoff <scenario>` now reuses packaged scenario setup but writes `handoff.report.json`, records `cleanup.status = deferred_handoff`, and leaves the launched game alive instead of running `cleanup_game_process(...)` on success
  - fixture manifests can now also apply bounded `seed_overmap_special_near_player` transforms alongside `player_mutations` and `player_near_overmap_special`, and the landed nearby-restage names stay `bandit_live_world_nearby_camp_v0_2026-04-22` plus `bandit.live_world_nearby_camp_mcw`
  - narrow helper validation now passes via `python3 -m py_compile tools/openclaw_harness/startup_harness.py`, `python3 tools/openclaw_harness/startup_harness.py install-fixture bandit_live_world_nearby_camp_v0_2026-04-22 --profile andi-nearby-seed-check --fixture-profile live-debug --replace`, plus current-build `probe` / `handoff` on `bandit.live_world_nearby_camp_mcw`
  - the seeded helper now keeps the player/basecamp footing at `om_pos [140, 41, 0]` and copies a real `bandit_camp` footprint onto `target_abs_omt [140, 51, 0]`, so the nearby site is about `10 OMT` away in the same overmap instead of by flinging the player across the world
  - fresh current-build nearby-restage proof now lives under `.userdata/dev-harness/harness_runs/20260422_205630/` and `.userdata/dev-harness/harness_runs/20260422_205658/`; both runs stayed runtime-current (`version_matches_runtime_paths = true`), the probe auto-cleaned with `cleanup.status = terminated`, the handoff run stayed alive with `cleanup.status = deferred_handoff`, and the old `validate_camps()` / `invalid map position` load error no longer appears
- fresh current-build `500`-turn save proof on that same nearby helper now lives under `.userdata/dev-harness/harness_runs/20260422_214610/`: the run advanced `500` turns, save-quit cleanly back to the menu, and `dimension_data.gsav` persisted `"bandit_live_world": { "owner_id": "hells_raiders_live_owner_v0", "sites": [] }`, which means the current seed-only nearby restage still has no claimed owned site/member roster to dispatch or write back
- the disposable moved-player bootstrap retry was also not an honest workaround yet, but the current defect class is now grounded more precisely than “old popup vibes”: save inspection after `.userdata/dev-harness/harness_runs/20260422_221150/` showed `player.location = [3372,1212,0]` for target OMT `[140,50,0]` while top-level load anchor fields still stayed at `levx/levy = 275/77`, so the helper had rewritten the player position without actually moving the map-load anchor onto the seeded site footing
- the helper bug is now fixed narrowly in `tools/openclaw_harness/startup_harness.py`: `player_near_overmap_special` also rewrites top-level `om_x/om_y/levx/levy/levz` while preserving the old player-to-bubble offset, and fresh install-only validation via `python3 -m py_compile tools/openclaw_harness/startup_harness.py`, `python3 tools/openclaw_harness/startup_harness.py install-fixture tmp_bandit_live_world_nearby_site2_bootstrap --profile andi-nearby-site2-fixcheck --fixture-profile live-debug --replace`, plus direct save inspection now shows `target_location = [3372,1212,0]` with matching `target_load_anchor = { om_x: 0, om_y: 0, levx: 276, levy: 96, levz: 0 }`
- fresh current-build corrected-anchor ownership proof now lives under `.userdata/dev-harness/harness_runs/20260422_224132/`: disposable scenario `tmp.bandit_nearby_site2_bootstrap_save_probe` loaded beside the seeded nearby camp on the corrected anchor, saved immediately, and `dimension_data.gsav` now serializes one owned site `overmap_special:bandit_camp@140,51,0` with `headcount: 14`, the full `(140..141, 51..52)` footprint, and 14 explicit claimed member ids/spawn tiles; that closes the nearby-owned-site bootstrap blocker and shows the seed-only path was missing real nearby generation rather than silently dropping a spawned claim
- one changed diagnostic retry still sharpened the other half of the distinction instead of rerunning ceremony: immediate seed-only save probe `.userdata/dev-harness/harness_runs/20260422_220046/` again serialized `bandit_live_world ... sites: []`, while direct string inspection found no nearby bandit roster/template markers in `#Wm9yYWlkYSBWaWNr.sav.zzip`; that points the current blocker more toward “nearby roster never spawned/generated here” than “roster spawned and claim silently missed it” on the seed-only path
- fresh current-build live dispatch proof now exists on the honest claimed nearby footing, and it killed one more bad folklore assumption on the way:
  - the earlier `320`-turn probes were not disproving dispatch; they were only advancing about `5m20s` because `.` waits are one-second turns on current build, while the live dispatch gate inside `overmap_npc_move()` only evaluates every `30_minutes`
  - disposable `1860`-turn road-footing probe `.userdata/dev-harness/harness_runs/20260422_231413/` (`tmp.bandit_owned_site2_road_dispatch_probe_1860`) now saves `active_group_id = overmap_special:bandit_camp@140,51,0#dispatch`, `active_target_id = player@140,42,0`, `active_member_ids = [4]`, member `4` as `state = outbound`, and the matching overmap NPC on mission `10` with goal `(140,42)` and a live `omt_path` length of `8`
  - disposable `1860`-turn basecamp-footing probe `.userdata/dev-harness/harness_runs/20260422_232225/` (`tmp.bandit_owned_site2_basecamp_dispatch_probe_1860`) proves the same seam on the real basecamp tile too: member `4` stays the dispatched outbound scout, `active_target_id = player@140,41,0`, and the matching NPC carries mission `10` with goal `(140,41)` and a live `omt_path` length of `9`
- `tools/openclaw_harness/startup_harness.py` also needed one honest long-run hardening to make those real cadence probes possible: `advance_turns(...)` now batches long repeated `.` input instead of firing one giant uninterrupted stream, and current narrow helper validation still passes via `python3 -m py_compile tools/openclaw_harness/startup_harness.py` plus rerun `probe tmp.bandit_owned_site2_road_dispatch_probe`
- fresh current-build nearby local-contact proof now exists on the same honest basecamp footing too:
  - disposable `3600`-turn basecamp probe `.userdata/dev-harness/harness_runs/20260422_234628/` pushed the dispatched scout into the live player bubble hard enough to record `Giuseppe Bachman gets angry!`, `Giuseppe Bachman picks up a flaking rock.`, and the safe-mode survivor ping in `#Wm9yYWlkYSBWaWNr.sav.zzip`
  - materially changed safemode-off follow-up `.userdata/dev-harness/harness_runs/20260423_000656/` then saved the exact same owned outing with member `4` as `state = local_contact`, `last_writeback_summary = local contact near player@140,41,0`, while the matching overmap NPC sat near the basecamp bubble at `[3372,1007,0]` on mission `7` / previous `10` with empty `omt_path`
  - that is honest proof that the newly landed live local-contact observer fires on the real nearby owned-site setup instead of only existing as code plus deterministic aftermath tests
- `tools/openclaw_harness/startup_harness.py` now also has one bounded save-inspection helper for this lane: scenario field `capture_world_after` copies the post-save world into `saved_world/` under the run dir, and narrow helper validation passed via `python3 -m py_compile tools/openclaw_harness/startup_harness.py`, `python3 tools/openclaw_harness/startup_harness.py probe tmp.bandit_owned_site2_basecamp_aftermath_probe_7200_safemode_off --dry-run`, and `git diff --check`
- fresh snapshot-preserving aftermath evidence now splits cleanly into blocker and resolution runs:
  - copied-save run `.userdata/dev-harness/harness_runs/20260423_042618/` still preserves `active_group_id = overmap_special:bandit_camp@140,51,0#dispatch`, `active_target_id = player@140,41,0`, `active_member_ids = [4]`, and member `4` in `state = local_contact` with `last_writeback_summary = local contact near player@140,41,0` after the full `7200`-turn wait
  - materially changed player-away continuation run `.userdata/dev-harness/harness_runs/20260423_054050/` kept the same copied stuck snapshot but only advanced `1860` turns after moving the player `20 OMT` south of site2; that still preserved the active outing, so it was honest evidence that the continuation path needed more runway rather than a menu-level shrug
  - materially changed player-away continuation run `.userdata/dev-harness/harness_runs/20260423_055255/` then advanced `3600` turns from that same copied stuck snapshot and finally resolved the exact scout back onto the owned site ledger: member `4` saved as `state = at_home` with `last_writeback_summary = return withdrawn from player@140,41,0`, while `active_group_id`, `active_target_id`, and `active_member_ids` all cleared
  - the after-step screenshots for these runs are still secondary; the honest evidence class here is the copied `saved_world/` inspection, not whether the menu looked tidy
- fresh current-build same-site post-writeback follow-through now also exists on that same honest nearby footing:
  - fixture `tools/openclaw_harness/fixtures/saves/live-debug/tmp_bandit_live_world_post_writeback_snapshot_2026-04-23/` started with `active_group_id = ''`, `active_target_id = ''`, `active_member_ids = []`, and member `4` at `state = at_home` with `last_writeback_summary = return withdrawn from player@140,41,0`
  - continuation probe `.userdata/dev-harness/harness_runs/20260423_082832/` then advanced `1860` turns from that post-writeback snapshot back on the original basecamp footing and saved the same owned site with `active_group_id = overmap_special:bandit_camp@140,51,0#dispatch`, `active_target_id = player@140,41,0`, `active_member_ids = [4]`, and member `4` back in `state = outbound` with `last_writeback_summary = dispatch scout toward player@140,41,0`
  - the generic harness verdict stayed `inconclusive_no_new_artifacts`, but the honest evidence class here was the copied `saved_world/` inspection, and that save inspection is enough to close the calm return->re-dispatch question
- fresh reviewer-clean perf evidence now also exists on the same honest nearby footing:
  - `.userdata/dev-harness/harness_runs/20260423_004225/` / `advance_1_turn`: `count 1`, `total_s 0.566692`, `avg_ms 566.692`, `max_batch_turn_ms 566.692`
  - `.userdata/dev-harness/harness_runs/20260423_004253/` / `advance_120_turns`: `count 120`, `total_s 27.980205`, `avg_ms 233.168`, `max_batch_turn_ms 233.168`
  - `.userdata/dev-harness/harness_runs/20260423_004349/` / `advance_1860_turns`: `count 1860`, `total_s 436.095456`, `avg_ms 234.460`, `max_batch_s 28.394650`, `max_batch_turn_ms 236.622`
  - `.userdata/dev-harness/harness_runs/20260423_012819/` / `advance_4200_turns`: `count 4200`, `total_s 983.073795`, `avg_ms 234.065`, `max_batch_s 28.599248`, `max_batch_turn_ms 238.327`
  - derived ratios on the same current-build packet stay flat after startup overhead: `single_vs_wait = 2.430x`, `single_vs_cadence = 2.417x`, `cadence_vs_wait = 1.006x`, `stress_vs_cadence = 0.998x`, `cadence_spike_ratio = 1.009x`, `stress_spike_ratio = 1.018x`
- fresh current-build dirty later-world disturbance proof now also exists on the same honest nearby footing:
  - raw-local-contact continuation `.userdata/dev-harness/harness_runs/20260423_194416/` resumed from fixture `tmp_bandit_live_world_local_contact_raw_2026-04-23`, killed the exact local-contact scout, advanced `10` turns, and saved the owned site with `headcount = 13`, member `4` as `state = missing` with `last_writeback_summary = return broken from player@140,41,0 (missing)`, home `spawn_tile [3371,1230,0]` at `headcount = 0`, and the next active outing already rotated to `active_member_ids = [5]`
  - the generic harness verdict again stayed `inconclusive_no_new_artifacts`, but the honest evidence class was the copied `saved_world/` inspection, and that save is enough to close the dirty-disturbance bar without pretending the log had to narrate it
- required evidence on the now-closed live-world lane ended up mixing live-world control proof, restage proof, and perf proof:
  - [x] fresh current-build live proof that the new dispatch seam actually drives that honestly owned nearby site in play instead of stopping at compile-time route plumbing
  - [x] fresh current-build live proof that the newly landed **real** local-contact hook actually fires on that owned nearby setup instead of only existing as code plus deterministic return-packet tests
  - [x] fresh current-build proof that that same nearby setup resolves beyond first `local_contact` into exact-member aftermath/writeback instead of saving the outing mid-contact forever
  - [x] fresh current-build proof that the nearby controlled-restage handoff path leaves the session alive for playtesting instead of auto-terminating after setup
  - [x] fresh reviewer-clean evidence that the nearby setup exercised the real overmap/bubble handoff plus local writeback path instead of stopping at a code-landed-but-unplayed observer seam
  - [x] a concrete perf packet on that nearby setup using baseline single-turn cost, wait/pass-time cost, bandit-cadence turn cost, spike ratio, and max turn cost
  - [x] at least one explicit dirty later-world disturbance proof on that same nearby owned setup, via the loss/missing shrinkage continuation above
- the live-world lane is now honestly closed for current canon: bootstrap, dispatch, local-contact, exact-member writeback, calm same-site re-dispatch, dirty loss/missing follow-through, handoff support, and the reviewer-clean perf packet all exist on current build without widening into generic hostile-human empire work
- the useful landed helper substrate from the old `v2` lane stays relevant here rather than wasted:
  - `tools/openclaw_harness/startup_harness.py` already resolves fixture-manifest `save_transforms`
  - the current bounded shipped transform kinds are `player_mutations`, `player_near_overmap_special`, and `seed_overmap_special_near_player`
  - install/startup/probe/handoff reports already surface `applied_save_transforms`
  - the first mutation-backed hostile-contact preset already exists at `tools/openclaw_harness/scenarios/bandit.basecamp_clairvoyance_contact_audit_mcw.json`
  - the first nearby-restage preset at `tools/openclaw_harness/scenarios/bandit.live_world_nearby_camp_mcw.json` is now honest nearby-restage substrate instead of a load-breaking moved-player fakeout
- but do **not** let that helper substrate masquerade as the next answer either:
  - the next missing proof is no longer single-site ownership/control existence or later-world disturbance on one camp; it is small independent multi-site owner state across save/load without coalition mush
  - the missing playtest bar still includes a handoff/save path that stays legible after setup instead of collapsing back to menu theater when the scene gets interesting
  - the broader closeout still needs ugly-interaction coverage on the real nearby live setup, not abstract helper elegance or folklore about what probably happened off-screen

### Latest closed lane - Bandit + Basecamp playtest kit packet v1

- canonical packet: `doc/bandit-basecamp-playtest-kit-packet-v1-2026-04-22.md`
- the prepared-base family is now closed honestly:
  - save/profile alias pair `tools/openclaw_harness/fixtures/{saves,profiles}/live-debug/bandit_basecamp_prepared_base_v1_2026-04-22/`
  - save/profile alias pair `tools/openclaw_harness/fixtures/{saves,profiles}/live-debug/bandit_basecamp_clairvoyance_v1_2026-04-22/`
  - named audit scenarios `tools/openclaw_harness/scenarios/bandit.basecamp_prepared_base_audit_mcw.json` and `tools/openclaw_harness/scenarios/bandit.basecamp_clairvoyance_audit_mcw.json`
- the key closeout change is method honesty, not more fixture sprawl:
  - `bandit_basecamp_clairvoyance_v1_2026-04-22` no longer needs its own copied save payload
  - the fixture now reuses `bandit_basecamp_prepared_base_v1_2026-04-22` and applies a bounded manifest-level `player_mutations` restage for `DEBUG_CLAIRVOYANCE_PLUS` and `DEBUG_CLAIRVOYANCE`
  - `startup.result.json` / `probe.report.json` now surface that restage in `fixture_install.applied_save_transforms`
- fresh current-build closeout proof:
  - load/capture audit under `.userdata/dev-harness/harness_runs/20260422_172658/`
  - temp install proof via `python3 tools/openclaw_harness/startup_harness.py install-fixture bandit_basecamp_clairvoyance_v1_2026-04-22 --profile andi-v1-check --fixture-profile live-debug --replace`
  - post-load save inspection on the installed dev-harness world still shows both clairvoyance mutations in `traits`, `mutations`, and `cached_mutations`
- honest remaining rough edge from `v1` stays explicit:
  - mutation-screen OCR still clearly catches `Debug Clairvoyance Super` better than the second entry
  - that is an on-screen capture limitation, not a missing installed mutation state

### Latest closed lane - Bandit + Basecamp playtest kit packet v0

- canonical packet: `doc/bandit-basecamp-playtest-kit-packet-v0-2026-04-22.md`
- starting helper/readability footing came from the already-closed helper and first-pass packets:
  - `tools/openclaw_harness/scenarios/bandit.basecamp_named_spawn_mcw.json`
  - `tools/openclaw_harness/scenarios/bandit.basecamp_first_pass_readability_mcw.json`
  - fresh current-build helper proof under `.userdata/dev-harness/harness_runs/20260422_132353/`
  - fresh current-build first-pass readability proof under `.userdata/dev-harness/harness_runs/20260422_144921/`
- fresh harness-side repeatability/reporting/cleanup proof lives under `.userdata/dev-harness/harness_runs/20260422_151547/`, via `python3 tools/openclaw_harness/startup_harness.py repeatability bandit.basecamp_named_spawn_mcw`
  - the packaged helper reran three times on the same McWilliams footing
  - `filter_bandit_template.after` matched the expected filtered-bandit menu lines on all three runs
  - each probe report records a `cleanup` block, and all three repeatability runs exited with `cleanup.status = terminated`
  - the run stayed runtime-current (`version_matches_runtime_paths = true`) while `version_matches_repo_head = false`, which is expected here because the tree changed only in harness/scenario files and no fresh tiles rebuild was required for this slice
  - the new `repeatability.summary.txt` / `repeatability.report.json` surface is screen-first enough to show the remaining rough edge honestly: the right-panel anger line only OCR-matched on one of the three runs, so the operator can see capture sensitivity directly instead of pretending the generic `inconclusive_no_new_artifacts` verdict settled it
- the fast-reload pack now exists as a thin alias pair on top of the captured McWilliams live-debug footing:
  - save fixture alias: `tools/openclaw_harness/fixtures/saves/live-debug/bandit_basecamp_playtest_kit_v0_2026-04-22/manifest.json`
  - profile snapshot alias: `tools/openclaw_harness/fixtures/profiles/live-debug/bandit_basecamp_playtest_kit_v0_2026-04-22/manifest.json`
  - `startup_harness.py` now resolves manifest-only `source_fixture` and `source_snapshot` aliases so the pack can reuse the captured footing without another full copied save/profile blob
- fresh live proof on that pack alias footing lives under:
  - `.userdata/dev-harness/harness_runs/20260422_152650/` via `python3 tools/openclaw_harness/startup_harness.py probe bandit.basecamp_playtestkit_restage_mcw`
  - `.userdata/dev-harness/harness_runs/20260422_152819/` via `python3 tools/openclaw_harness/startup_harness.py probe bandit.basecamp_playtestkit_readability_mcw`
  - both runs stayed runtime-current (`version_matches_runtime_paths = true`) and both cleanup blocks ended in `cleanup.status = terminated`
  - the pack-family restage run still shows stable filtered-bandit menu proof on the alias footing
  - the pack-family readability run still shows the same honest encounter shape on the alias footing: immediate read remains cluttered/weak, while the eight-turn right panel captures `Bandit gets angry!`, gunfire, taunts, and survivor hits cleanly enough to confirm the readability sibling still works through the fast-reload path
- the packet now says its rough edges plainly and stops cleanly where it should: captured-save footing, named-NPC debug spawn dependency, screen-first/no-new-debug-log evidence, mixed immediate anger OCR, and no extra richer variants because those belong to `v1` and `v2`

### Latest closed lane - Bandit + Basecamp first-pass encounter/readability packet v0

- canonical packet: `doc/bandit-basecamp-first-pass-encounter-readability-packet-v0-2026-04-22.md`
- fresh current-build proof lives under `.userdata/dev-harness/harness_runs/20260422_144921/`
- the bounded live readability helper for this packet is now `tools/openclaw_harness/scenarios/bandit.basecamp_first_pass_readability_mcw.json`
- the packet used the already-closed helper seam and then captured immediate plus short-horizon screen artifacts (`immediate_{full,local,right_panel}` and `after_8_turns_{full,local,right_panel}`) instead of pretending the generic artifact verdict could answer the product question by itself
- immediate on-screen read is weak: the spawn mostly reads as one more purple nearby-NPC name plus old movement/saving clutter, so the player does not get a strong first-pass spatial read on why this new person is the threat
- eight turns later the encounter is unmistakably real and dangerous, but still mostly through the right-panel log rather than clean spatial staging: `Heath Griffith, Bandit gets angry!`, taunts, safe-mode survivor ping, gunfire, and the deaths of Katharina Leach and Robbie Knox all land in the log
- honest verdict: the encounter is mechanically present and dangerous enough to justify more follow-through, but first-pass readability is weak/confusing enough that the correct next step is playtest-surface polish rather than another setup/readability feasibility lap
- no deterministic tests were added or rerun because this packet stayed on live probe / artifact work only; fresh live proof came from the current tiles binary and `python3 tools/openclaw_harness/startup_harness.py probe bandit.basecamp_first_pass_readability_mcw`

### Latest closed lane - Live bandit + Basecamp playtest packaging/helper packet v0

- canonical packet: `doc/live-bandit-basecamp-playtest-packaging-helper-packet-v0-2026-04-22.md`
- landed helper: `tools/openclaw_harness/scenarios/bandit.basecamp_named_spawn_mcw.json`
- fresh current-build proof lives under `.userdata/dev-harness/harness_runs/20260422_132353/`
- current-build proof is honest: `window_title = Cataclysm: Dark Days Ahead - 7ab535f0c7`, `version_matches_repo_head = true`, and `version_matches_runtime_paths = true`
- reviewer-readable helper artifacts now exist directly on the run path:
  - `filter_bandit_template.after.{png,screen_text.txt}` showing the filtered `bandit` menu entries
  - `post_spawn_settle.after.{png,screen_text.txt}` showing `Joshua Wilkes, Bandit gets angry!`
- forced rebuild was used here not because the packet needed gameplay-code validation, but because an earlier stale tiles binary made the first helper proof carry an avoidable asterisk before handoff
- honest remaining rough edges: the packaged path still uses the named-NPC debug spawn surface, still depends on the captured McWilliams fixture, and the generic probe artifact verdict is still less useful than the screen/OCR companions for this seam
- no deterministic tests were added or rerun because this packet landed as harness/helper/docs work only

### Latest closed lane - Live bandit + Basecamp playtesting feasibility probe v0

- canonical packet: `doc/live-bandit-basecamp-playtesting-feasibility-probe-v0-2026-04-21.md`
- fresh current-build startup proof lives under `.userdata/dev/harness_runs/20260422_002122/`, with build head `5af2fb80d8-dirty`, `version_matches_repo_head = true`, and `version_matches_runtime_paths = true`
- bounded live restage proof lives under `.userdata/dev-harness/harness_runs/20260422_002329/`, showing named NPC debug spawn of hostile `Stefany Billings, Bandit`
- honest verdict: current-build bandit + Basecamp live playtesting is practical now, but reviewer-clean packaged overmap-bandit harness footing is still absent

### Latest closed lane - Basecamp AI capability audit/readout packet v0

- canonical packet: `doc/basecamp-ai-capability-audit-readout-packet-v0-2026-04-21.md`
- honest verdict: the player-facing spoken Basecamp surface is still a narrow deterministic craft-request plus board/job router, while the richer prompt-shaped layer is mostly snapshot/planner packaging rather than core spoken command interpretation
- no compile or live probe was required because this was a current-tree capability readout, not a fresh runtime behavior claim

### Latest closed lane - Locker Zone V3

- canonical packet: `doc/locker-zone-v3-reopen-packet-v0-2026-04-21.md`
- honest verdict: hot/cold outerwear bias, hot/cold legwear bias, moderate-temperature seasonal dressing, rainy moderate-weather rainproof preference, and bounded worker-specific wardrobe preservation now all land on the same real locker seam
- current focused validation: `make -j4 tests` and `./tests/cata_test "[camp][locker]"`

### Closed bandit readiness train

Use the auxiliary packet docs for the detailed proof shape.
The canonical closed packets are:
- `doc/bandit-overmap-local-handoff-interaction-packet-v0-2026-04-21.md`
- `doc/bandit-elevated-light-and-z-level-visibility-packet-v0-2026-04-21.md`
- `doc/bandit-overmap-benchmark-suite-packet-v0-2026-04-21.md`
- `doc/bandit-weather-concealment-refinement-packet-v0-2026-04-21.md`
- `doc/bandit-overmap-local-pressure-rewrite-packet-v0-2026-04-21.md`
- `doc/bandit-long-range-directional-light-proof-packet-v0-2026-04-21.md`
- `doc/bandit-bounded-scout-explore-seam-v0-2026-04-21.md`
- `doc/bandit-repeated-site-revisit-behavior-packet-v0-2026-04-21.md`
- `doc/bandit-scoring-refinement-seam-v0-2026-04-21.md`
- `doc/bandit-moving-bounty-memory-seam-v0-2026-04-21.md`

Current honest summary:
- the playback/proof surface is now checkpointed through the handoff, visibility, benchmark, weather, pressure-rewrite, long-range-light, scout/explore, moving-memory, scoring, and repeated-site packets
- the bandit proof family has current deterministic coverage on the tree, with `./tests/cata_test "[bandit]"` as the broad filtered confidence pass when code in that family changes
- no further rerun is warranted until a fresh greenlight or contradictory evidence appears

---

## Pending probes

A live probe is still greenlit, but the next probe must answer control/restage questions on top of the now-landed owner ledger instead of reopening the already-closed fixture-method argument.

- Do **not** rerun the first-pass readability packet ceremonially now that its product question has an honest answer.
- Do **not** keep rerunning the closed thin `v0` pack or the closed `v1` load audits unless a new live-control helper specifically needs that regression footing.
- The nearby ownership/bootstrap question is now answered yes on current build via `.userdata/dev-harness/harness_runs/20260422_224132/`, and the scout-reaches-the-bubble question is already answered too: `.userdata/dev-harness/harness_runs/20260422_234628/` plus `.userdata/dev-harness/harness_runs/20260423_000656/` prove real nearby local contact, while `.userdata/dev-harness/harness_runs/20260423_055255/` now proves exact-member aftermath/writeback beyond that first contact state.
- The first missing live proof after this owner slice is no longer dispatch/control existence, basic writeback existence, or calm same-site re-dispatch; it is one dirtier later-world follow-through on the same nearby site, such as a live loss/missing path that shrinks later outing size or a save/load/player-disruption proof that keeps the owned roster honest.
- The current `seed_overmap_special_near_player` helper is still not enough by itself for this packet; seed-only saves still land as `sites: []`, so copied terrain alone is not owned roster truth.
- When the next harness/probe helper lands, give it one named scenario or command path for reviewer use instead of laundering it through the old thin-pack names.
- If the control/restage work surfaces a real blocker, name it concretely instead of laundering operator annoyance into vague harness vibes.

---

## Reusable commands

Use these when they are actually the missing evidence, not as ritual.

### Narrow locker deterministic pass after a code slice lands
- `make -j4 tests`
- `./tests/cata_test "[camp][locker]"`

### Plan status summary command
- `python3 tools/plan_status_summary.py --self-test`
- `python3 tools/plan_status_summary.py /plan`
- `python3 tools/plan_status_summary.py /plan active`
- `python3 tools/plan_status_summary.py /plan greenlit`
- `python3 tools/plan_status_summary.py /plan parked`

### Bandit dry-run evaluator seam foundation
- `make -j4 tests`
- `./tests/cata_test "[bandit][dry_run]"`

### Fresh tiles rebuild only if a later combat-policy handoff really needs live proof
- `make -j4 TILES=1 cataclysm-tiles`

### Current live playtest-packaging/helper seam
- `make -B -j4 TILES=1 cataclysm-tiles`
- `python3 tools/openclaw_harness/startup_harness.py probe bandit.basecamp_named_spawn_mcw`

### Closed first-pass encounter/readability seam
- `make -B -j4 TILES=1 cataclysm-tiles`
- `python3 tools/openclaw_harness/startup_harness.py probe bandit.basecamp_first_pass_readability_mcw`

### Current active playtest-kit footing
- `make -B -j4 TILES=1 cataclysm-tiles`
- `python3 tools/openclaw_harness/startup_harness.py probe bandit.basecamp_named_spawn_mcw`
- `python3 tools/openclaw_harness/startup_harness.py repeatability bandit.basecamp_named_spawn_mcw`
- `python3 tools/openclaw_harness/startup_harness.py probe bandit.basecamp_playtestkit_restage_mcw`
- `python3 tools/openclaw_harness/startup_harness.py probe bandit.basecamp_playtestkit_readability_mcw`
- `python3 tools/openclaw_harness/startup_harness.py probe bandit.basecamp_first_pass_readability_mcw`
- `python3 tools/openclaw_harness/startup_harness.py probe bandit.basecamp_clairvoyance_contact_audit_mcw`
- preserve the screen/OCR artifacts that show repeatability, readability, cleanup behavior, and later pack-backed variants

## Local build caveat

On this Mac, treat top-level `make -j4 tests` as the reliable path for a fresh `cata_test`.
Avoid treating `make -C tests cata_test` as authoritative here; it has been a repeated source of toolchain/stale-build nonsense.
Also: if you actually need a fresh tiles binary, use `make -j4 TILES=1 cataclysm-tiles`; plain `make cataclysm-tiles` is not an honest rebuild path here.
