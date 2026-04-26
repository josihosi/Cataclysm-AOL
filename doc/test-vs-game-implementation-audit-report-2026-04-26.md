# Test-vs-game implementation audit report (2026-04-26)

Status: audit complete / docs checkpoint.

Canonical packet: `doc/test-vs-game-implementation-audit-report-packet-v0-2026-04-26.md`.

## Executive result

The high-risk bandit stack is split in two:

- **Live-wired substrate:** hostile-site ownership, save/load, nearby-player dispatch from owned NPCs, local gate posture, shakedown surface, pay/fight writeback, and camp locker / smart-zone UI entry points have real game consumers.
- **Deterministic/playback-only signal layer:** smoke packets, light packets, weather-adjusted bandit mark projection, generated mark ledgers, repeated-site reinforcement, route sightings, long-horizon benchmark reports, and most `scout` tuning packets are not currently produced from live map/fire/light/weather/sight data and are not consumed by live dispatch.

The next implementation package should be **`Bandit live signal + site bootstrap correction v0`**. It should wire live map/fire/smoke/light/weather/cadence data into the mark/scout substrate, then make live dispatch consume that substrate instead of only the current nearby-player envelope.

## Evidence vocabulary used here

- **live-wired:** traced from a live producer into a live game consumer.
- **deterministic-only:** unit/helper/evaluator proof with no live producer/consumer bridge.
- **playback-only:** authored scenario/proof packet proof; useful, but not live gameplay.
- **harness-only:** live-ish fixture/probe proof that depends on a controlled save/scenario rather than natural gameplay.
- **stale-ambiguous:** claim depends on old prose or evidence whose current-source bridge is unclear.
- **hollow-misleading:** test/proof can pass while the gameplay claim is absent.

## Required seam classifications

| Seam | Verdict | Evidence class | Source seam | Live producer | Live consumer | Notes / next package |
|---|---|---|---|---|---|---|
| Smoke-derived bandit marks | **playback-only** | deterministic playback | `bandit_mark_generation::adapt_smoke_packet`, `advance_state`, `emit_leads`; exercised by `tests/bandit_mark_generation_test.cpp` and `tests/bandit_playback_test.cpp` | None found in production. `rg` found `bandit_mark_generation` only in its own source/header and playback/tests. | Playback evaluator only: `bandit_playback::run_checkpoint_at` pushes authored `smoke_packet` signals before `bandit_dry_run::evaluate`. | Assign to `Bandit live signal + site bootstrap correction v0`: create a live smoke/fire source hook and a live consumer path. |
| Light-derived bandit marks | **playback-only** | deterministic playback | `bandit_mark_generation::adapt_light_packet`; directional/elevated/portal-storm playback packets | None found in production. Real light exists in `src/lightmap.cpp`, but no bridge creates `light_packet`. | Playback evaluator only. Live dispatch never reads generated light marks. | Same next package. Tests prove adapter math, not gameplay. |
| Weather modifiers on smoke/light bandit marks | **deterministic-only** for adapter math; **playback-only** when asserted through authored scenario frames | unit + playback | `smoke_weather_*`, `light_weather_penalty`, `describe_*`; tested with authored weather bands | Live weather exists (`get_weather().weather_id`), but no production code maps it into bandit smoke/light packets. | Playback reports and unit assertions only. | Same next package. Weather is real in engine, fake-fed in bandit marks. |
| Horde attraction | **live-wired** for sound; **deterministic-only** for bandit horde-pressure evaluator inputs; **hollow-misleading** if claimed for visible light/fire | live source hook for sound; evaluator inputs for bandit pressure | `sounds::get_signal_for_hordes`, `sounds::process_sounds`; bandit dry-run `monster_pressure_add` / playback pressure scenarios | Live sound centroids in `src/sounds.cpp`; weather attenuates via `get_weather().weather_id->sound_attn`. No direct live visible-light/fire horde hook found. | `overmap_buffer.signal_hordes` for sound. Bandit evaluator consumes authored pressure only. | Do not claim light/fire draws hordes unless a new hook is implemented. If desired, split a `Visible fire/light horde attraction bridge v0`; otherwise keep horde proof limited to sound. |
| Site bootstrap from hostile NPC spawns | **live-wired** for NPC-placement bootstrap; missing for abstract pre-load bootstrap | live source hook + save serialization | `map::place_npc` calls `bandit_live_world::claim_tracked_spawn`; `world_state` serializes via `savegame.cpp` | Live mapgen NPC placement on tracked overmap specials/extras (`bandit_camp`, `bandit_work_camp`, `bandit_cabin`, `cannibal_camp`, `mx_looters`, `mx_bandits_block`). | `overmap_buffer.global_state.bandit_live_world`, later read by live dispatch. | It only claims when NPCs are actually placed/loaded. Unvisited hostile sites are not proven to become owners naturally; this belongs to `Bandit live signal + site bootstrap correction v0`. |
| Dispatch from owned hostile site | **live-wired for nearby-player pressure** | live source hook + deterministic tests + harness evidence from prior packets | `bandit_live_world::plan_site_dispatch`, `apply_dispatch_plan`; live call in `steer_live_bandit_dispatch_toward_player` | `overmap_buffer.global_state.bandit_live_world.sites`; candidate if player is within 10 OMT and site has dispatchable members. | `do_turn.cpp` every `30_minutes` through `overmap_npc_move`; writes NPC `goal`, `omt_path`, `NPC_MISSION_TRAVELLING`. | This is real, but target production is primitive: live dispatch does not consume mark-generation smoke/light/route ledgers. |
| Local handoff / local gate | **live-wired for active owned outings** | live source hook + deterministic tests + harness screen/log packets | `live_bandit_make_gate_input`, `choose_local_gate_posture`, `open_live_bandit_shakedown_surface`, `resolve_active_group_aftermath` | Active dispatch group members and player context (`rolling_travel_scene`, basecamp/camp proximity, local contact, goods pools). | `note_live_bandit_aftermath` in `overmap_npc_move`; shakedown UI, NPC attitude/mission/writeback. | Real for currently active groups. It does not make smoke/light scouts real by itself. |
| Scout dispatch / return / sight-avoid | **mixed: live-wired for nearby-player dispatch and active-group return writeback; playback-only or missing for authored scout intelligence and sight-avoid** | live source hook + playback benchmark | `bandit_dry_run::evaluate`, `job_template::scout`, `plan_site_dispatch`, `make_nearby_target_lead`, `note_live_bandit_aftermath`, `resolve_active_group_aftermath`, playback `bounded_explore` / handoff scenarios | Live: nearby player target envelope and active member observations. Playback: authored `bounded_explore`, smoke/light, human-route, local-return packets. No audited live player-sight/line-of-sight avoidance producer. | Live: `steer_live_bandit_dispatch_toward_player` applies scout handoff; `note_live_bandit_aftermath` can send local-contact members home and apply return packets. Playback: report/evaluator only. | Nearby-player scout and return/writeback are gameplay. Smoke/light/explore scouts are playback-only, and sight-avoid remains missing unless implemented as a real steering/local-contact gate. |

## Audited high-risk pass conditions

| Test / scenario family | Representative pass condition | Source seam | Live producer | Live consumer | Evidence class | Verdict | Assignment |
|---|---|---|---|---|---|---|---|
| `tests/bandit_mark_generation_test.cpp` smoke tests (`smoke_adapter`, `smoke_weather_refinement`, vertical smoke) | Smoke stays long-range but bounded; weather reports fuzz/reduction; vertical smoke stays nearby-visible without extra range. | `adapt_smoke_packet`, `smoke_weather_effect`, `advance_state`, `render_report`. | None found. Authored `smoke_packet` only. | None in live dispatch. Playback can emit leads from generated marks. | unit/evaluator | **deterministic-only** | `Bandit live signal + site bootstrap correction v0` |
| `tests/bandit_mark_generation_test.cpp` light tests (night/searchlight, portal storm, elevated/z, concealment) | Light leaks remain bounded; exposed/elevated light outruns hidden/contained light; weather/terrain reductions are reported. | `adapt_light_packet`, `light_concealment_packet`, `advance_state`. | None found. Real lightmap has dynamic light but no packet bridge. | Playback/evaluator only. | unit/evaluator | **deterministic-only** | `Bandit live signal + site bootstrap correction v0` |
| `tests/bandit_mark_generation_test.cpp` human-route/repeated-site/moving-memory/confirmed-threat tests | Route activity and repeated corroboration can produce bounded marks; moving leads persist briefly; confirmed threats stay sticky. | `adapt_human_route_packet`, `apply_repeated_site_reinforcement`, moving memory helpers. | None found from live NPC travel/traffic/sightings. | Playback/evaluator only. | unit/evaluator | **deterministic-only** | `Bandit live signal + site bootstrap correction v0` or later `live route sighting bridge` follow-up if split. |
| `tests/bandit_playback_test.cpp` generated smoke/light/weather proof packets | Authored packets generate leads and produce scout/hold-chill behavior across checkpoints. | `bandit_playback::run_checkpoint_at`: authored packets -> `adapt_*` -> `advance_state` -> `emit_leads` -> `bandit_dry_run::evaluate`. | Authored playback frames. | Playback evaluator/report. | deterministic playback | **playback-only** | Same next package; keep as regression once live bridge exists. |
| `tests/bandit_playback_test.cpp` long-horizon benchmark suite (`first_500`, overmap benchmark, budget reports) | Ticks 0/20/100/500 keep scout pressure bounded and cool back to hold/chill. | Playback scenario frames and benchmark checks in `src/bandit_playback.cpp`. | Authored frames. | Playback report only. | deterministic playback | **playback-only** | Useful tuning guard, not live proof. |
| `tests/bandit_playback_test.cpp` local handoff interaction packet | Entry/return packet remains readable; local loss rewrites corridor/withdrawal. | `bandit_pursuit_handoff` payload/return helpers plus playback packet. | Authored abstract group state. | Playback report; live aftermath uses related return-packet functions only after real active group observations. | playback + partial live seam | **playback-only for packet; live-wired for reused return helpers** | Keep wording split. |
| `tests/bandit_live_world_test.cpp` site claim/save/load tests | One or several hostile sites claim bounded ledgers and serialize independently. | `claim_tracked_spawn`, `world_state` serialize/deserialize, `site_record`. | Live `map::place_npc` calls `claim_tracked_spawn` during mapgen NPC placement. | `overmap_buffer.global_state.bandit_live_world`, live dispatch. | unit + live source hook + save serialization | **live-wired, with bootstrap caveat** | Correct unvisited/lazy bootstrap in next package. |
| `tests/bandit_live_world_test.cpp` dispatch plan tests | Owned members produce bounded scout dispatch plan, mark selected member outbound, keep home reserve, apply profile rules. | `plan_site_dispatch`, `apply_dispatch_plan`, profile rules. | Live sites created by `claim_tracked_spawn`; live player target from `steer_live_bandit_dispatch_toward_player`. | Live NPC mission/goal/path writes in `do_turn.cpp`. | unit + live source hook | **live-wired for nearby-player target** | Extend consumer to live marks in next package. |
| `tests/bandit_live_world_test.cpp` local gate/cannibal/shakedown/aftermath tests | Gate chooses hold/probe/open_shakedown/attack; cannibals attack not extort; pay/fight writes back and aftermath cools/reopens. | `choose_local_gate_posture`, `build_shakedown_surface`, `apply_shakedown_outcome`, `resolve_active_group_aftermath`. | Live active group observations in `note_live_bandit_aftermath`; goods pool from avatar/NPC/ground/vehicle inventory. | Live UI menu, messages, NPC attitude, member state, saved `site_record`. | unit + live source hook; prior harness screen/log proof | **live-wired** | No hollow bridge found here; evidence must keep screen/log separate from deterministic tests. |
| `tests/faction_camp_test.cpp` camp locker classification/planning/service/readiness tests | Locker classifies gear, chooses weather/armor/ranged-readiness upgrades, services one worker, preserves/dumps carried items, keeps carried bandages/ammo/magazines, and exposes repeat/probe cost. | `basecamp::{process_camp_locker_downtime,service_camp_locker}`, `plan_camp_locker_loadout`, `collect_camp_locker_live_state`. | Live NPC downtime calls `basecamp::process_camp_locker_downtime` in `npcmove.cpp` for assigned camps. | Live worker equipment/inventory and camp storage zones. | unit + live source hook | **live-wired substrate; queued package gaps remain** | Existing tests support locker service, armor/ranged readiness, carried-item preservation, and cost probes. They do **not** fully close the queued medical consumable readiness, generic armor blocker-removal, or job-spam debounce/exception packages; those still need their named deterministic proofs. |
| `tests/clzones_test.cpp` Smart Zone Manager Basecamp tests | Expected Basecamp storage zones are placed deterministically; too-small/non-destructive cases are handled. | `auto_place_basecamp_smart_zones` in `src/clzones.cpp`. | Live `zone_manager_ui.cpp` offers Smart Zone Manager when creating/moving `CAMP_STORAGE`. | Live zone manager / saved zones. | unit + UI source hook | **live-wired, not screen-proven in current test** | `Smart Zone Manager v1 Josef playtest corrections` for manuals/magazines/auto-eat/auto-drink option proof and harness save inspection. |
| Existing horde sound path | Loud sound centroids attract hordes with weather attenuation and min/max caps. | `sounds::get_signal_for_hordes`, `sounds::process_sounds`, `overmap_buffer.signal_hordes`. | Live sound events. | Overmap horde signal system. | live source hook | **live-wired** | Do not use this as proof that bandit smoke/light marks or visible fire/light attract hordes. |

## Concrete code-path findings

### 1. Mark generation has no production caller

`src/bandit_mark_generation.{h,cpp}` is currently consumed by `src/bandit_playback.cpp` and tests. A source search outside those files did not find production callers for `adapt_smoke_packet`, `adapt_light_packet`, `advance_state`, `signal_input`, `smoke_packet`, or `light_packet`.

The important playback path is:

1. authored `scenario_frame` contains `smoke_packets`, `light_packets`, `human_route_packets`, and direct `mark_signals`;
2. `bandit_playback::run_checkpoint_at` adapts viable packets into `signal_input`;
3. `advance_state` writes a generated mark ledger;
4. `emit_leads` turns marks into evaluator leads;
5. `bandit_dry_run::evaluate` chooses scout/hold/etc.;
6. renderers produce proof reports.

That is a good deterministic rehearsal. It is not live gameplay.

### 2. Live dispatch does not read generated smoke/light/route marks

The live dispatcher is `steer_live_bandit_dispatch_toward_player` in `src/do_turn.cpp`, with planning and writeback helpers in `src/bandit_live_world.cpp`. It:

- scans `overmap_buffer.global_state.bandit_live_world.sites`;
- considers sites within `10` OMT of the player;
- builds a target id like `player@x,y,z`;
- calls `bandit_live_world::plan_site_dispatch`;
- finds actual NPCs by id;
- computes an overmap path;
- applies the dispatch plan to the site;
- writes NPC `goal`, `omt_path`, and `NPC_MISSION_TRAVELLING`.

`plan_site_dispatch` does use `bandit_dry_run::evaluate`, and the winning job can be `scout`, but its live lead is `make_nearby_target_lead`: a synthetic nearby-player envelope. It is not a smoke/light/weather/repeated-site mark emitted from live world data.

### 3. Site ownership is real but only after materialization

`map::place_npc` calls `bandit_live_world::claim_tracked_spawn` with the NPC template, spawn tile, current overmap special, current map extra, and a special-footprint lookup. That is a real source hook.

Caveat: this does not prove a hostile overmap special becomes an owned site before its NPCs are placed/loaded. The next package should decide whether the correct behavior is lazy ownership materialization from overmap special placement, first nearby load, or a bounded scanner. At the moment, tests that manually call `claim_tracked_spawn` prove the substrate; the live bridge starts at NPC placement.

### 4. Hordes are live-wired through sound, not through bandit visibility marks

`src/sounds.cpp` computes sound centroids, attenuates with `get_weather().weather_id->sound_attn`, applies `min_vol_cap`, `hordes_sig_div`, `min_sig_cap`, and `max_sig_cap`, then calls `overmap_buffer.signal_hordes`.

That proves live sound-to-horde attraction. It does not prove:

- visible light attracts hordes;
- smoke/fire creates bandit marks;
- bandit horde-pressure evaluator inputs are fed by real hordes;
- bandit smoke/light playback packets interact with horde movement.

### 5. Camp systems are less hollow than the bandit signal layer

The high-risk camp tests are mostly deterministic, but their seams have real consumers:

- camp locker downtime is called from `npc::worker_downtime` through `basecamp::process_camp_locker_downtime`;
- locker service mutates real NPC equipment/inventory and camp storage tiles;
- Smart Zone Manager is offered by `zone_manager_ui.cpp` when a faction `CAMP_STORAGE` zone is created or moved, then calls `auto_place_basecamp_smart_zones`.

These still need targeted follow-up tests for the greenlit corrections, but they are not fake-live in the same way as the smoke/light bandit mark packets.

## Hollow / missing bridge list

1. **Live smoke/fire -> `smoke_packet` -> bandit mark ledger.** Missing.
2. **Live light/searchlight/elevation/weather -> `light_packet` -> bandit mark ledger.** Missing.
3. **Live weather/time/terrain concealment -> bandit packet modifiers.** Missing as a producer; deterministic adapter exists.
4. **Live human route sightings -> `human_route_packet` / moving-memory marks.** Missing.
5. **Generated mark ledger -> live dispatcher target selection.** Missing; current dispatcher uses nearby-player envelope only.
6. **Unvisited hostile site -> owned `site_record` before NPC placement.** Not proven; current source hook starts at `map::place_npc`.
7. **Visible fire/light -> horde attraction.** Not present in audited code; current horde bridge is sound.

## Next implementation package

Execute **`Bandit live signal + site bootstrap correction v0`** next.

Minimum success shape for that package:

- one live-owned hostile site can exist without relying solely on the player stepping onto a generated camp tile;
- one real smoke/fire source and one real light source create bounded live signal candidates under named weather/time conditions;
- live dispatch can choose or reject a target from those candidates with logged range/cadence/hold-chill reasons;
- no-signal control stays quiet;
- deterministic mark/playback tests remain as adapter regressions, but the claim only closes after harness/log/save evidence traces the live producer and live consumer.

The camp follow-up packages remain valid, but they should queue behind this bridge because the largest false-confidence risk is currently in the bandit signal/playback layer.
