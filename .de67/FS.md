# Affordable playtests and safer C-AOL subsystem changes — Functional Specification

Status: Refrozen — 2026-09-19
WEC: `.de67/WEC.md`
Source baseline: `Cataclysm-AOL-hostile-ecology-dev | dev | c2ad7514a37a76e547f9ba44d1ff9a43cc79f17e | inspected 2026-09-19`
Method baseline: `/Volumes/CodexBulk/Schanigarten/workspaces/de67-lab | a6125b4a70adf641e185b34dbd33118e48d884e3 | inspected 2026-09-19`

## Authority and outcome

This is the single canonical FS; `.de67/DFS.md` is its SHA-256-bound compatibility pointer. The installed `specification.resolve()` and workspace setup support this arrangement. Do not create a second full specification. The WEC defines intended behavior; current code defines the inspected starting point. A proposed symbol below is an implementation starting point, not an extra behavioral obligation.

Preserve the prior zombies-and-light contract and its accepted evidence. Its verbatim WEC, FS and pointer are in `.de67/history/20260919-zombies-light-before-affordable-playtests/`, verified by `archive.json`. Durable acceptance remains in the existing SQLite state and work ledger; this refreeze does not invalidate, fabricate or rerun that acceptance. New regressions and proof obligations have new IDs. The only revised old obligation is the remaining `R-ZL-PLAYTEST` route, explicitly changed by the owner from naturally occurring stalker to debug-spawned stalker followed through ordinary play.

Deliver lower total playtest effort, an honest stalking-to-city-attack account, and tested subsystem boundaries supporting small cleanups. Keep product changes in C-AOL on `dev`; method/Telescope changes belong to the separate de67 source. Neither side silently absorbs the other's ownership. Pit Crew and Reflex Pilot are optional, deferred experiments, in that order. Production success does not depend on either experiment proving useful.

The inspected product tracked tree was clean before Phase 2; unrelated untracked runtime/evidence remains. The method tree has unrelated dashboard edits in `integrations/dashboard/{README.md,de67_dashboard.py,test_de67_dashboard.py}`; preserve them. Record the actual implementation starting commit and relevant dirty state again for each delivered slice. Phase 2 changed no product code/tests and ran no new gameplay.

## Language, coordinates and truth

Keep **writhing stalker**, **zombie rider**, **rider band**, **physical light**, **light emission**, **escaping light**, **detection**, **recognition**, **local**, **abstract**, **reality bubble**, **OMT**, **Telescope**, **Pit Crew**, and **Reflex Pilot**. Production remains C++, JSON and existing Python tooling with Catch2/unittest conventions; no NPC-intelligence or general agent framework is introduced.

`tripoint_bub_ms` belongs only to the current map origin. Historical/durable positions use `tripoint_abs_ms`; project with `coords::project_to` and convert current positions with `map::get_bub/get_abs`. One OMT is two submaps horizontally; do not scale z like x/y. Use `calendar::turn`, `time_point/time_duration` and the existing turn conversion APIs, never count planner invocations as elapsed time. Existing code's comment calling a game turn six seconds is not authority for a new time unit.

Emission, perceived clues, recognized prey and debug oracle facts are different information classes. A clue's original actor/source, observed time and expiry survive copying; reading or relaying never refreshes them. Setup and synthetic final-state construction give no credit for the production transition they bypass. Test-only observation seams may expose state but cannot set the outcome being proved.

## Current code map and ownership — S-OWNERS

| State/action | Readers and production callers | Writers / competing owners | Authoritative rule |
|---|---|---|---|
| Item/stationary emission | `physical_light::collect_item_emitters/collect_stationary_emitters/index_loaded_z_sources`; `do_turn.cpp::observe_loaded_z_light_sources`; native `lightmap.cpp` | Item power/containment, map terrain/furniture/field, vehicle part and monster state | Discovery is read-only; ground-item deduplication does not suppress other source kinds. Absolute location, luminance and provenance belong to each emitter. |
| Fresh light sample | `sample_and_deliver_live_light_for_advancing_turn`, later `overmap_npc_move` through `live_light_samples_already_taken_this_turn` | `game::do_turn`, static light cache/gate in `do_turn.cpp` | Only elapsed game time admits fresh sampling; consumer maintenance does not refresh old evidence. Current five-minute producer gate is an observed conflict with the retained brief-exposure contract, classified under R-CAOL-LIGHT-TURNS. |
| Recipient light memory | Horde light adapter, bandit/cannibal signal readers, stalker/rider policy | `horde_map::attract_entities_to_light`, staffed signal recording, predator light receivers; sound and direct sight compete | Shared optical eligibility, separate reaction owners. Direct recognized prey supersedes weaker old light/sound; duplicate sample identity is idempotent. Memory ages even after emission ends. |
| Local/abstract monster ownership | `game::shift_monsters`, `overmapbuffer::spawn_monster/despawn_monster`, `overmap::move_hordes` | Creature tracker versus horde node/payload; on-load/unload and evolution | Commit the destination owner before removing the source. Failed placement reinserts exact extracted node. Predator actor ID/epoch and last-advanced turn prevent duplicate ownership/advancement. Ordinary monsters retain their native identity/payload semantics. |
| Stalker movement and pressure | `monster::plan` → `apply_writhing_stalker_plan`, `writhing_stalker_live_context`, quiet-side destination | Typed `writhing_stalker::persistent_state`; transient static pressure samples; real melee/projectile resolution | Typed state owns approach/retreat/burst and absolute evidence. Attack entry spends attempts; walking does not. Transient perception cannot grant unseen coordinates or survive into another world as fresh evidence. |
| Rider movement/bands | Local planner and `overmapbuffer::reconcile_rider_band_encounters` | Pursuit state versus `overmap_global_state::zombie_rider_bands`; casualty/evolution/handoff writers | Registry owns membership, actor fields are revisioned projections. Only credible local reciprocal sight and reachable encounter currently form a new band; abstract distance alone fails closed. Sharing retains observation age. |
| Bandit operation/member ownership | `do_turn.cpp` materialize/dematerialize, homeward/ingress motor, `overmap_npc_move` | `bandit_live_world` world/site/member records, NPC projection leases and operation receipts | Existing operation-specific preflight, token/version and commit rules govern partial progress/rollback. New helpers may query identical eligibility; they do not become transfer owners. |
| R022 setup transaction | `game::do_turn` → `openclaw_harness_r022_item_spawn_bridge` | Environment run/transaction IDs; `debug_item_spawn_transaction/cleanup` | Setup receipts remain zero-credit. Current function-static once flag is process-scoped; a run-scoped relocation must retain exactly-once successful effects and truthful failure/cleanup. |
| Pending native input/frame | Harness player/cockpit and bridge submit/collect/status | Native input owner, request response store, frame/session binding | One submitted request remains the only action until acceptance/completion is resolved. Collection is read-only. A matching returned frame can be reused; stale authority cannot authorize another action. |
| Evidence / optional semantic selection | Harness exact queries; Telescope gather/assemble | Immutable native artifacts; Telescope typed classification and metadata cache | Exact retrieval owns known facts; Jev selects bounded candidates and may abstain, but never certifies acceptance. Original handles and contradictions survive compaction. |

## Retained accepted gameplay contract

The following stable slices preserve the accepted contract at its original evidence ceiling. The work ledger records each acceptance #1 and its exact closing task/receipt; no fresh obligation below is credited by these checkmarks. Existing unsupported UPS/multimag fixture rows remain explicitly unsupported, not secretly converted to tested features. A current contradiction receives its own regression claim.

<!-- DE67:DFS-SLICE:BEGIN id=R-ZL-LIGHT-EMISSION-S001 claim=R-ZL-LIGHT-EMISSION -->
### Working emission
- [x] R-ZL-LIGHT-EMISSION — Carried and stationary light use actual emission facts without borrowed exposure or phantom power.
Production owners are `src/physical_light.{h,cpp}`, carrier-aware item power/containment and native lightmap emission. Keep held/worn/gunmod/ground provenance, non-consuming queries, dimming, opaque-pocket exclusion, enabled/available vehicle parts and per-source absolute location. A gun's forwarding root is not a second gunmod light. Current player-tile coverage is a new R-CAOL-LIGHT-FIXTURE obligation; historical emission acceptance is unchanged.
<!-- DE67:DFS-SLICE:END id=R-ZL-LIGHT-EMISSION-S001 claim=R-ZL-LIGHT-EMISSION -->

<!-- DE67:DFS-SLICE:BEGIN id=R-ZL-LIGHT-OPTICS-S001 claim=R-ZL-LIGHT-OPTICS -->
### Physical optical eligibility
- [x] R-ZL-LIGHT-OPTICS — Physical light reaches each observer through geometry independently of terrain recognition.
Keep source-specific emission/escape and observer-specific detection in `physical_light`, `bandit_mark_generation` and the live light adapters. Clear glass and actual apertures may transmit; opaque coverings, sealed floors/walls and incompatible paths do not. Elevation helps only when obstruction clears. Glow detection does not identify a carrier or reveal the player's hidden current coordinates. Preserve accepted cross-level and physical-approach evidence; no new renderer or unconditional elevation bonus.
<!-- DE67:DFS-SLICE:END id=R-ZL-LIGHT-OPTICS-S001 claim=R-ZL-LIGHT-OPTICS -->

<!-- DE67:DFS-SLICE:BEGIN id=R-ZL-LIGHT-CONTINUITY-S001 claim=R-ZL-LIGHT-CONTINUITY -->
### Finite physical-light consequences
- [x] R-ZL-LIGHT-CONTINUITY — Brief genuine exposure can create a finite remembered investigation without immortal refresh or sound bypass.
Eligible ordinary hordes, staffed bandit/cannibal observers, stalkers and riders consume real optical evidence through separate policies. Turning off, moving, depletion, occlusion or unloading ends fresh observation; memory may persist only to its original expiry. No recipient is created merely because light exists. Direct prey evidence wins over weaker old clues; sound stays sound. The present producer's five-minute admission and cache lifetime are newly audited under R-CAOL-LIGHT-TURNS, not excused by this historical acceptance.
<!-- DE67:DFS-SLICE:END id=R-ZL-LIGHT-CONTINUITY-S001 claim=R-ZL-LIGHT-CONTINUITY -->

<!-- DE67:DFS-SLICE:BEGIN id=R-ZL-STALKER-S001 claim=R-ZL-STALKER -->
### Stalker opportunity and commitment
- [x] R-ZL-STALKER — The stalker exploits a credible opening, reaches contact and counts attacks instead of approach decisions.
`writhing_stalker::persistent_state` and `apply_writhing_stalker_plan` own notice → follow/search → committed approach → actual attack → retreat/cooldown. Daylight is caution, never an absolute veto when perceptible same-target zombie pressure supplies an opening; darkness permits solitary opportunities. Visibility is not counterpressure; actual hostile attempts, including misses, can be. Keep weak combat stats, elapsed-time lifecycle, committed absolute waypoints and no hidden-player reacquisition. History-coordinate regressions are separately required below.
<!-- DE67:DFS-SLICE:END id=R-ZL-STALKER-S001 claim=R-ZL-STALKER -->

<!-- DE67:DFS-SLICE:BEGIN id=R-ZL-RIDER-PURSUIT-S001 claim=R-ZL-RIDER-PURSUIT -->
### Rider pursuit
- [x] R-ZL-RIDER-PURSUIT — A rider maintains physical pressure instead of routine post-shot retreat or failed-annulus withdrawal.
Keep `apply_zombie_rider_plan` and `zombie_rider_overmap_ai::rider_pursuit_state` acquisition, bow cooldown/empty-bow pursuit, direct evidence and finite last-seen search. A powerful rider can shoot while closing and reach contact before ammunition exhaustion. Terrain, occupancy and mount-sized passages constrain movement; no hidden target coordinate, half-health retreat cliff or teleporting fallback.
<!-- DE67:DFS-SLICE:END id=R-ZL-RIDER-PURSUIT-S001 claim=R-ZL-RIDER-PURSUIT -->

<!-- DE67:DFS-SLICE:BEGIN id=R-ZL-RIDER-IMPACT-S001 claim=R-ZL-RIDER-IMPACT -->
### Rider contact
- [x] R-ZL-RIDER-IMPACT — A physical approach produces a paid, mitigated contact attack with recovery.
Retain the current native attack actor and movement-readiness integration in `mattack_actors.cpp`, `monster.cpp`, `monmove.cpp` and rider content. No extra free attack, wall penetration, unsupported mount separation or permanent downed refresh. Preserve stationary/blocked/narrow/vehicle/already-downed and multiple-rider controls. Contact impact remains the chosen initial mechanic; a run-through redesign is not cleanup.
<!-- DE67:DFS-SLICE:END id=R-ZL-RIDER-IMPACT-S001 claim=R-ZL-RIDER-IMPACT -->

<!-- DE67:DFS-SLICE:BEGIN id=R-ZL-PREDATOR-LIFECYCLE-S001 claim=R-ZL-PREDATOR-LIFECYCLE -->
### Durable predator actors
- [x] R-ZL-PREDATOR-LIFECYCLE — One predator identity continues between local and abstract owners.
`predator_lifecycle_state`, heavy `horde_entity` payloads, transfer functions and savegame readers/writers retain actor ID, handoff epoch, HP/ammo, phase, observation/waypoints and last advancement. Failed transfer retains the source; same-turn handoff cannot advance twice. Initially abstract actors progress and materialize legally. New repeated-handoff and failure-interleaving coverage below extends, rather than reopens, this proof.
<!-- DE67:DFS-SLICE:END id=R-ZL-PREDATOR-LIFECYCLE-S001 claim=R-ZL-PREDATOR-LIFECYCLE -->

<!-- DE67:DFS-SLICE:BEGIN id=R-ZL-EVOLUTION-S001 claim=R-ZL-EVOLUTION -->
### Evolution and ammunition
- [x] R-ZL-EVOLUTION — Normal new-world evolution respects the configured season gate and initializes rider ammunition once.
`monster::try_upgrade/poly/on_load` and horde evolution retain the eight-configured-season gate through intermediate-type catch-up. Direct spawn and genuine transition initialize ammunition once; reloading never refills spent arrows. Preserve explicit saved `upgrades:false`. The owner declined old-predator backward-compatibility migration; no state retrofit/re-enable is authorized.
<!-- DE67:DFS-SLICE:END id=R-ZL-EVOLUTION-S001 claim=R-ZL-EVOLUTION -->

<!-- DE67:DFS-SLICE:BEGIN id=R-ZL-RIDER-BANDS-S001 claim=R-ZL-RIDER-BANDS -->
### Persistent physical rider bands
- [x] R-ZL-RIDER-BANDS — Credible encounters form persistent bands and share only observed evidence.
`overmapbuffer::reconcile_rider_band_encounters` reads local/abstract snapshots and updates the `zombie_rider_bands` registry. Membership survives separation, casualties, transfer and save/load; actor cached references reconcile to registry revision. Equal destinations do not establish a meeting. Relaying preserves source actor, original position/time/expiry, never unseen moving prey. Preserve current fail-closed abstract encounter behavior; this work does not add abstract route inference.
<!-- DE67:DFS-SLICE:END id=R-ZL-RIDER-BANDS-S001 claim=R-ZL-RIDER-BANDS -->

<!-- DE67:DFS-SLICE:BEGIN id=R-ZL-ENCOUNTERS-S001 claim=R-ZL-ENCOUNTERS -->
### Encounter and description evidence
- [x] R-ZL-ENCOUNTERS — Early stalker and late rider availability and descriptions have their accepted natural-route evidence.
Keep `zed_misc.json`, zombie groups, normal spawn/evolution and translated descriptions. The new debug-stalker behavioral account does not replace or claim natural prevalence evidence. Do not repeat encounter-frequency work or alter balance merely to make the remaining account easier.
<!-- DE67:DFS-SLICE:END id=R-ZL-ENCOUNTERS-S001 claim=R-ZL-ENCOUNTERS -->

<!-- DE67:DFS-SLICE:BEGIN id=R-MAINT-ASTRA-WORKER-S001 claim=R-MAINT-ASTRA-WORKER -->
### Existing optional worker capability
- [x] R-MAINT-ASTRA-WORKER — The previously accepted optional worker/dashboard capability retains its original scope.
This is preserved identity for existing accepted method work, not a new change in this FS. Its durable evidence and current mutable operating choices remain in the existing work ledger/runtime surfaces. No product regression requires a dashboard edit.
<!-- DE67:DFS-SLICE:END id=R-MAINT-ASTRA-WORKER-S001 claim=R-MAINT-ASTRA-WORKER -->

<!-- DE67:DFS-SLICE:BEGIN id=R-ZL-CREATURE-SPRITES-S001 claim=R-ZL-CREATURE-SPRITES -->
### Separate creature sprites
- [x] R-ZL-CREATURE-SPRITES — The separately packaged stalker/rider sprites retain their accepted native visual integration.
Keep the tracked `data/mods/caol_creature_sprites_tileset/` package, `mod_tileset` route and creature IDs separate from Ultica's base files. Historical debug-spawn visual proof is visual evidence only. No new creature, art campaign or sprite replay is required here.
<!-- DE67:DFS-SLICE:END id=R-ZL-CREATURE-SPRITES-S001 claim=R-ZL-CREATURE-SPRITES -->

## Product regression and cleanup sequence

First establish player-tile/fixture safety, then repeated handoffs, then turn-driven light, then history/coordinate lifetimes. Supporting cleanups follow the tests that protect them. This is causal dependency, not a run quota. An investigated suspicion that is already fixed or covered receives an evidence-backed disposition, not a compulsory production edit. All delivered cleanups, including fixtures and mechanical moves, need executed behavioral tests before and after.

<!-- DE67:DFS-SLICE:BEGIN id=R-CAOL-LIGHT-FIXTURE-S001 claim=R-CAOL-LIGHT-FIXTURE -->
### Player-tile discovery and reliable restoration
- [ ] 🔴 R-CAOL-LIGHT-FIXTURE — The real loaded-source index retains stationary light beneath the player and touched fixtures restore global definitions on every exit.
**Current evidence.** `src/physical_light.cpp::index_loaded_z_sources` calls `collect_item_emitters(carrier, here, 0)`, then continues the entire tile loop when `p == carrier_pos`. That avoids a duplicate ground item but also bypasses terrain, furniture and fields. Source inspection confirms the control-flow defect; the failing behavioral regression has not yet been executed. `physical_light_stationary_records_are_source_bound` mutates global `ter_t/furn_t::light_emitted` and restores them after `REQUIRE` calls, so assertion unwinding can skip restoration. `src/cata_scope_helpers.h::restore_on_out_of_scope` already exists.

**Change boundary/mechanism.** First add a real-index fixture with walkable luminous terrain or furniture at an absolute tile, not an impassable fixture through which the avatar cannot move. Capture original definitions with the existing scope guard before mutation. Move the actual avatar beside → onto → beside the source; call `index_loaded_z_sources` each time. Place a powered ground lamp at the same tile. Constrain only the ground-item loop to skip the already-collected player tile; stationary collection must execute there. Do not change emission power, geometry, other z-level scanning or native movement. Restore touched terrain/furniture/map fixture and global fields on normal and exceptional exits. Avoid a new restoration framework.

**Proof.** Proposed Catch cases `physical_light_loaded_index_keeps_player_tile_stationary_sources` and `physical_light_fixture_restoration_survives_unwind` in `tests/physical_light_test.cpp`; retain `physical_light_stationary_records_are_source_bound`, `physical_light_loaded_z_index_finds_remote_loaded_level_and_excludes_depleted_source`, `physical_light_loaded_z_index_handles_ground_gun_light_mod`. Assert same absolute position/kind/provenance/luminance in each stationary result, ground lamp exactly once, and original definition values after a safely caught simulated assertion-unwind exception. Where testing Catch's actual abort path, isolate an expected-failure subprocess rather than leave a deliberately failing normal case. Demonstrate the player-tile case fails on the preceding behavior and passes after the focused fix; guard cleanup needs normal/exception observations. Execute the affected suite repeatedly in shuffled order with reported seed, and verify nonzero selected cases. V-CPP is the command route.
<!-- DE67:DFS-SLICE:END id=R-CAOL-LIGHT-FIXTURE-S001 claim=R-CAOL-LIGHT-FIXTURE -->

<!-- DE67:DFS-SLICE:BEGIN id=R-CAOL-HANDOFF-S001 claim=R-CAOL-HANDOFF -->
### Repeated real ownership transitions
- [ ] 🔴 R-CAOL-HANDOFF — Repeated local/abstract crossings and save/load preserve one owner, identity and resource accounting, including rejected and interleaved transfers.
**Current evidence.** `overmapbuffer::spawn_monster` extracts horde nodes, prepares predator state in a disposable copy, tries exact predator placement, and reinserts the original node on failure. `despawn_monster` snapshots first and reports insertion success. `game::shift_monsters` must remove the local actor only after that success. `horde_entity::ensure_predator_payload` retains heavy predator state; ordinary lazy evolution remains a separate path. Existing `tests/horde_map_test.cpp` cases cover lazy payload, duplicate abstract identity and failed prepared handoff, but the latter constructs a prepared copy and is not a complete production transfer/reload loop.

**Mechanism.** Extend existing real-entrypoint fixtures, not a parallel transfer API. Begin without combat, regeneration/consumption or unrelated time advancement; cross out/in more than once with a save/load between crossings. At every completed or refused boundary count local tracker plus loaded horde containers and match surviving predator durable IDs, epochs, absolute positions, HP/ammo, typed state and band registry. Ordinary zombie identity is its native payload/accounting identity, not an invented durable predator UUID. Repeat a completed request at the real caller: no second insertion, removal, ammo/resource debit or resurrection. Then add legitimate elapsed abstract time and compare invariant accounting while permitting genuine movement/tactical state changes.

**Bandit cases.** Exercise production `materialize_live_bandit_structural_handoffs`, `dematerialize_live_bandit_structural_handoffs`, `complete_live_bandit_homeward_boundary_steps` and their tokenized `bandit_live_world` commit paths. Cover blocked partner, destination invalidation after preflight, member death during pending departure, failed transfer followed by retry, and light/sound/boundary event order. Preserve the existing all-or-nothing pair transaction and exact persisted-crossing rollback in S-BANDIT below; do not replace a rejected complete-pair handoff with partial ownership. Compare member IDs, owner token/operation, inventory and resource totals independently of log prose.

**Proof.** Proposed cases `monster_repeated_reality_bubble_handoff_round_trip`, `predator_failed_materialization_retries_without_duplicate_owner`, and `bandit_departure_interleavings_preserve_owner_and_resources` extend `horde_map_test.cpp`/`bandit_live_world_test.cpp`; use actual transfer functions and normal serializers/readers. Keep existing shared-owner/token, reload, transition-receipt, complete-pair rollback and failed-preflight tests identified in S-BANDIT. Native continuation proof must bind source, actor and actual boundary events; manually assembling final local/abstract structures is insufficient. Run V-CPP with `[hordes]`, `[predator]`, `[bandit][live_world]` (focused names first), and relevant rider/stalker lifecycle cases.
<!-- DE67:DFS-SLICE:END id=R-CAOL-HANDOFF-S001 claim=R-CAOL-HANDOFF -->

<!-- DE67:DFS-SLICE:BEGIN id=R-CAOL-LIGHT-TURNS-S001 claim=R-CAOL-LIGHT-TURNS -->
### Turn-driven observation, memory and ordering
- [ ] 🔴 R-CAOL-LIGHT-TURNS — Real advancing turns detect eligible brief exposure, retain finite memory after source-off and expire it without redraw/query refresh.
**Confirmed starting discrepancy.** `do_turn.cpp::live_light_samples_for_current_turn` calls `begin_advancing_turn(turn, calendar::once_every(time_between_npc_OM_moves))`; its consumer rejects a cache from any other turn. Thus the current producer cannot sample a lamp on/off entirely between five-minute boundaries. `physical_light_samples_only_on_admitted_advancing_turns` proves a standalone gate only. This conflicts with retained R-ZL-LIGHT-CONTINUITY; it is a new current-code regression, not grounds to erase historical acceptance. Establish the executable failure before changing behavior.

**Production sequence.** In `game::do_turn`, fields/items/explosions and prior sounds process first, then map/vision caches, then the advancing-turn light owner, then local `monmove`, then ordinary/event-triggered overmap maintenance. Within `sample_and_deliver_live_light_for_advancing_turn`, rider encounter reconciliation precedes rider memory aging, fresh sample retrieval, horde/stalker/rider/cannibal delivery and staffed observer recording. Keep that order explicit. Fresh sampling must be admitted once for each real advancing turn; the later five-minute camp/overmap maintenance reads an already-produced packet and retains its separate dispatch cadence. No repeated scan/delivery from an inspection at the same game time.

**State/lifetime.** Retain original sample ID, absolute location, observed turn and expiry across recipient copies/save/load. Extinguishing/hiding/depleting/unloading stops new samples, not immediate legitimate memory. Clear or rebind transient light cache/gate on an actual world/load timeline boundary so equal numeric turns from another world cannot borrow an old sample or suppress a new one. Do not reset durable recipient memories just because the bubble shifts. Use the real game/load owner; no new global timer or second scheduler.

**Proof.** Proposed `live_light_advancing_turn_observation_and_expiry` and `live_light_turn_order_and_world_reset` exercise the actual turn owner or an extracted production facade invoked by that owner, not only `turn_sample_gate`. A real lamp on for a full advancing turn between maintenance boundaries → eligible optical recipient → timestamped clue → native reaction/movement; off/hidden source → no new timestamp → retained original memory → expiry. Include no-observer, occluded and sound-only controls; ordinary horde and each changed staffed/predator recipient adapter must be covered. Repeat observation/redraw without elapsed time and assert no new sample/memory. Include save/load or bubble variation and an order-sensitive encounter-plus-light case: reconciliation establishes the real band before light delivery and relayed/direct evidence precedence remains correct. Source/binary-bound compact native evidence supplements focused tests; do not commission a broad replay of accepted light accounts. V-CPP and V-NATIVE apply.
<!-- DE67:DFS-SLICE:END id=R-CAOL-LIGHT-TURNS-S001 claim=R-CAOL-LIGHT-TURNS -->

<!-- DE67:DFS-SLICE:BEGIN id=R-CAOL-HISTORY-S001 claim=R-CAOL-HISTORY -->
### Perception history and coordinate lifetimes
- [ ] 🔴 R-CAOL-HISTORY — Actor replacement, bubble shifts and world/load boundaries cannot turn obsolete perception into current pressure or rider knowledge.
**Inspected risk.** `monmove.cpp::writhing_stalker_pressure_memories` is a process-static map keyed by `Creature::get_identity()`. Outer size is capped at 512 by arbitrary map-entry removal; each retained memory's `samples` grows with new actor/target IDs without age pruning. Samples store `tripoint_bub_ms` and are compared with the current bubble target coordinate up to three turns later; a shifted origin can manufacture closing movement. The loop also updates samples when `visible` is false. These are source-confirmed lifetime/coordinate defects; their concrete gameplay regressions remain to be executed. `observed_attacks` in `writhing_stalker_ai.cpp` has its own bounded transient registry and fresh creature IDs; do not replace this working identity mechanism. `overmap_global_state::clear` already clears rider bands/light memory; retain that behavior.

**Mechanism.** Historical pressure positions use absolute map-square coordinates and observation time; convert only current qualified positions for local quiet-side consumers. Retain a sample only while it can contribute under the existing three-turn perceptual horizon, matching observer, target and live actor identity. Write observed positions only on actual eligible perception, not hidden-state reads. Repeated same-turn reads are idempotent. Drop expired/dead/replaced identities and invalidate same-turn qualified caches on map/world identity changes; preserve the existing tactical horizon rather than invent a new retention balance. Make transient perception reset at the existing game/world lifecycle explicit, with the smallest narrow owner API if needed. Persistent stalker approach/retreat and rider band/pursuit data retain existing serialization and are not converted into this ephemeral cache.

**Proof.** Proposed `writhing_stalker_pressure_history_uses_absolute_observed_positions`, `writhing_stalker_pressure_replacement_expires_old_samples`, `predator_transient_history_does_not_cross_worlds` and extended `zombie_rider_*band*` cases: fixed absolute actor/target positions plus a bubble-origin shift cannot invent closure; real observed approach still qualifies; invisible motion cannot seed later history; replace actors at roughly constant population and confirm no stale identity pressure and retained sample count tracks the active horizon rather than cumulative replacements. Save/reload retains required typed state/relationships but drops/revalidates transient local caches. Use the supported same-process world clear/setup route, or document the exact unsupported boundary while proving its actual reset owner directly. Assert identity/position/age/owner/relationship, not identical nondeterministic AI decisions. Any intentional expiry change is a separately reproduced behavioral fix, not hidden in extraction.
<!-- DE67:DFS-SLICE:END id=R-CAOL-HISTORY-S001 claim=R-CAOL-HISTORY -->

<!-- DE67:DFS-SLICE:BEGIN id=R-CAOL-LIGHT-EXTRACT-S001 claim=R-CAOL-LIGHT-EXTRACT -->
### Share discovery and extract cohesive turn responsibilities
- [ ] 🔴 R-CAOL-LIGHT-EXTRACT — Duplicated light discovery and cohesive turn implementation are simplified with executed behavior preserved.
**Boundary.** After R-CAOL-LIGHT-FIXTURE and R-CAOL-LIGHT-TURNS are passing, share the duplicated terrain/furniture/field, vehicle-light and luminous-monster enumeration in `collect_stationary_emitters` and `index_loaded_z_sources`. Existing loops differ in spatial selection (radius versus all loaded z); preserve that selection and source provenance while sharing emission eligibility. Ground item dedup remains independent. Do not consolidate sound, optical recognition and reaction policy into a universal source owner.

Move the cohesive sample-cache/discovery/delivery implementation out of `do_turn.cpp` into a narrow production module (proposed `live_light.{h,cpp}`) only to the extent its dependencies permit a clear entrypoint. `game::do_turn` keeps an explicit call at the same ordered boundary; later overmap readers receive the same-turn immutable sample view. Rider reconciliation remains before memory aging/delivery at exactly the existing advancing-turn frequency; extracting it is not permission to batch, defer or duplicate it. New module names are replaceable; one time owner and preserved interfaces are binding.

**Proof.** Run the real loaded-index beside/onto/away test through both formerly separate discovery routes using identical relevant fixtures. Run the actual-turn, no-elapsed-time, world-reset, order-sensitive band/light and recipient-expiry tests before and after mechanical extraction. Compare independent emitter sets/state transitions and deterministic work counters where available, not textual helper names. Build the affected targets and run related `[physical_light]`, `[hordes][light]`, rider/stalker and staffed-light tests. No mechanical-cleanup completion by compilation alone; no artificial red baseline is demanded for a behavior-preserving move.
<!-- DE67:DFS-SLICE:END id=R-CAOL-LIGHT-EXTRACT-S001 claim=R-CAOL-LIGHT-EXTRACT -->

<!-- DE67:DFS-SLICE:BEGIN id=R-CAOL-SETUP-LIFETIME-S001 claim=R-CAOL-SETUP-LIFETIME -->
### Harness setup has a run lifetime
- [ ] 🔴 R-CAOL-SETUP-LIFETIME — R022 item setup is removed from ordinary turn implementation without suppressing later runs or duplicating intervention effects.
**Current behavior.** `openclaw_harness_r022_item_spawn_bridge(avatar &)` is called at `game::do_turn` startup. Its function-static `dispatched` becomes true before `debug_item_spawn_transaction`; an environment transaction ID triggers apple quantity 3 at player offset (+4,0,0), followed by cleanup and transaction/identity/cleanup log receipts. A failed attempt is currently not automatically retried within the process. The run ID labels the receipt but does not scope the once flag.

**Required mechanism.** Move the small adapter beside its existing transaction owner in `src/wish.cpp`, declared through `src/debug_menu.h` (proposed `debug_menu::process_harness_item_setup`). Keep only its explicit call at the existing loaded-world/pre-input boundary in `game::do_turn`, so extraction does not move setup ahead of map/avatar readiness. Reset its narrow transient run state through `game::setup` when a new world lifecycle starts. Bind its attempt state to the actual run plus transaction ID, not process lifetime or the turn count. Repeated calls for the same completed attempt return/retain its receipts and cannot spawn or clean twice. A subsequent supported scenario/run in the same process gets its own state. Preserve current no-implicit-retry behavior on an ambiguous or rejected attempt: retain exact result/cleanup, and require an explicit new or supported idempotent retry transaction after authoritative outcome inspection. The actual `src/wish.cpp` transaction rejects occupied destinations, tags ordinal item identities, and calls tagged cleanup on placement/audit failure; it is not itself a replay registry. A new ID is not permission to lose tagged items or overwrite a receipt. Termination clears ephemeral ownership only after preserving the transaction outcome; no harness env means no setup side effects.

**Proof.** Extend `tests/debug_item_spawn_transaction_test.cpp` and `tools/openclaw_harness/r022_item_spawn_adapter_test.py` through the production adapter: repeated same-run call, next run in same process, absent config, rejected transaction, cleanup failure, explicit supported retry, and termination/reentry. Assert item identities/counts/owner and receipt references independently; verify unrelated untagged items survive cleanup. Proposed `r022_setup_is_run_scoped_and_receipted` covers the adapter. The normal accepted transaction and unchanged gameplay turn have before/after tests. No setup action gains native gameplay credit and no ordinary test suite contains deliberate failure.
<!-- DE67:DFS-SLICE:END id=R-CAOL-SETUP-LIFETIME-S001 claim=R-CAOL-SETUP-LIFETIME -->

<!-- DE67:DFS-SLICE:BEGIN id=R-CAOL-BANDIT-QUERIES-S001 claim=R-CAOL-BANDIT-QUERIES -->
### Narrow bandit eligibility queries
- [ ] 🔴 R-CAOL-BANDIT-QUERIES — Identical eligibility checks share named queries without changing operation-specific transfer behavior.
After R-CAOL-HANDOFF, identify exact duplicated predicates among `active_local_contact_member`, committed shakedown membership, structural handoffs and homeward motor in `do_turn.cpp` and matching `bandit_live_world` queries. Consolidate only predicates with the same inputs, owner and meaning; leave distinct operation/reservation/departure conditions at their callers. A proposed narrow query may report a member's existing eligibility; it must not mutate resources, transfer ownership or perform rollback. Avoid Boolean-switch helper frameworks.

**Proof.** Each affected real caller must execute eligible/ineligible cases before and after extraction, including blocked partner, stale token/preflight, destination invalidation, failed transfer/retry, death and established complete-pair rollback behavior from S-BANDIT. Preserve exact member counts and inventory/resource accounting, not only a returned Boolean. Reuse adequate existing `bandit_live_world_test.cpp` tests and add caller coverage only for missing paths. If investigation finds no genuinely identical useful duplication, document that disposition and deliver the protective tests without forced production edits.
<!-- DE67:DFS-SLICE:END id=R-CAOL-BANDIT-QUERIES-S001 claim=R-CAOL-BANDIT-QUERIES -->

<!-- DE67:DFS-SLICE:BEGIN id=R-CAOL-DIAGNOSTICS-S001 claim=R-CAOL-DIAGNOSTICS -->
### Diagnostics stay outside decisions
- [ ] 🔴 R-CAOL-DIAGNOSTICS — Lengthy movement diagnostics can be separated and timing labels describe the measured interval without affecting play.
Inspect `monmove.cpp` timing blocks in rider destination selection, `apply_writhing_stalker_plan` and specialized movement before moving formatting. The stalker `eval_us` surrounds live-context construction plus policy evaluation, not the later complete planner/movement; other blocks must be labeled by their own actual start/end. Keep native evidence field meaning and parsers stable or update their exact consumers together. Guard diagnostic-only formatting/collection with the existing enabled-logging predicate; do not introduce per-turn costly strings on a disabled path. Logging cannot query/mutate gameplay in a way that changes action choice.

**Proof.** Proposed `predator_diagnostics_preserve_decision_and_disabled_path` plus affected native parser tests compare the same controlled seed/state with logging enabled and disabled: destination, action/attempt state, resources, time and ownership match. Assert required structured field semantics and that disabled mode omits diagnostic-only work. Timing tests check scope association or instrumentation boundaries, never exact wall-clock duration or prose. Mechanical formatting extraction is validated before/after separately from any intentional label correction.
<!-- DE67:DFS-SLICE:END id=R-CAOL-DIAGNOSTICS-S001 claim=R-CAOL-DIAGNOSTICS -->


### Existing bandit transaction policy — S-BANDIT

`bandit_live_world::commit_local_pair_handoff(site, plan, bind_member, rollback_member)` validates active outing, expected cursor, operation kind, next epoch and monotonic minutes, constructs a candidate site, then binds every member. It adds each attempted member to the rollback list before invoking its callback. Any false result or exception rolls back touched members in reverse order; `site = candidate` happens only after all binds succeed. Same committed snapshot returns `unchanged`; mismatched/stale snapshot is rejected. `commit_local_pair_dematerialization(site, plan, quiesce_member, rollback_member)` follows the same complete-pair policy, applying casualty/cargo and time-driven supply only to the candidate before commit. Physical pair movement may progress asymmetrically before the transfer; that is not partial owner transfer.

`world_state::rollback_persisted_crossings(tokens)` acts only on an exact site/run/activity/generation/epoch/cursor/prior/next-owner match, restores prior owner, decrements the matching epoch, clears the abstract-origin local snapshot where applicable, and clears the crossing. Callback and durable acknowledgement are distinct boundaries. Preserve them through save failure and retry.

`hostile_camp_local_handoff_binds_the_complete_pair_transactionally` (`tests/bandit_live_world_test.cpp`) already covers many preflight, second-member callback rollback, unchanged duplicate, retry and persistence cases. Also retain `bandit_live_world_simulation_owner_handoff_is_shared_and_token_checked`, `bandit_live_world_persists_local_handoff_eligibility_before_cohesion`, `bandit_live_world_transition_receipts_bind_owner_and_pair_facts`, and `bandit_live_world_rejects_a_completed_return_after_reload_and_redispatch`. Add only the missing real adapter interleavings: actual pending-member death, actual failed bind/retry, and save/load at the physical-callback/acknowledgement boundary.

For monster transfers reuse `predator occupied exact handoff retains abstract owner`, `predator submap-load handoff rejects an existing local owner`, `predator local abstract local transfer retains its authoritative payload` in `monster_test.cpp`, and `predator cross-overmap handoff cannot advance twice in one turn` in `overmap_test.cpp`. Their one-crossing/helper coverage does not itself close the requested repeated sequence.

## Six efficiency improvements

Shared proof contract **S-EFF-COMPARE**: compare the same completed diagnostic/action/resume outcome with the same source/build, frozen evidence or equivalent scenario state, objective and information. Keep correctness, exact citations, contradictions and action authority constant. Record fresh and cached agent usage separately where available, provider input/output tokens, provider calls, tool/model turns, follow-up retrievals, controls/observe calls, elapsed time and rescued/retried work. Use existing usage/run artifacts. If usage is unavailable, state that limitation and do not call bytes or shorter output a token saving. The prior 15,498 input + 819 output Jev trial totaled 16,317 and established no net saving. Each improvement needs a comparable completed outcome and fewer relevant unnecessary operations; the combined six need measured lower total effort without degraded coverage. No new run quota or numerical acceptance cap is introduced.

C-AOL owns its native interfaces; de67 owns reusable invocation/discovery/continuation guidance and the Telescope integration. Product cleanup claims above may not edit de67 or implement Telescope. Updating guidance means changing the actual context-producing source and verifying its delivered example, not only editing this FS.

<!-- DE67:DFS-SLICE:BEGIN id=R-EFF-WAIT-S001 claim=R-EFF-WAIT -->
### Pending response waits
- [ ] 🔴 R-EFF-WAIT — Waiting for an already-submitted native request stays within one tool execution until useful progress, terminal failure or the actual task deadline.
**Present mechanism.** `tools/openclaw_harness/play_cli.py::PlayerClient.submit` saves pending request before `Bridge.send_request`; `collect(wait_seconds)` polls exact response identity and returns without resubmission at its deadline. CLI `--wait-seconds` defaults to 1. `cockpit_file_bridge.py` owns immutable request/response envelopes and binding checks. The principal gap is invocation guidance, not missing asynchronous transport.

**Change.** In the actual harness live-operation examples and method playtest instructions, use the existing finite wait option with remaining task time and keep launch plus necessary process polling in a single `functions.exec`. Return promptly on meaningful response/state change, cancellation/new owner input, process/bridge death or deadline. Tool responsiveness limits may require yielding the still-running execution; they never authorize resubmitting the action or an in-game wait. Continue the same request ID after an external timeout, inspecting acceptance/completion first. Add runtime glue only if a concrete test shows the existing route cannot provide this behavior; do not change the gameplay action API or the task clock.

**Proof.** Extend `play_cli_test.py` and retain `cockpit_file_bridge_test.py::test_delayed_response_collection_never_resubmits_request_identity`, collectible admission and monotonic transition tests. A controlled delayed bridge response, process death, cancellation and deadline expiry must return the correct request/terminal identity with exactly one submitted input. Exercise the published invocation, count reasoning/tool turns on the same delay, and preserve full receipt retrieval. V-PY-HARNESS and S-EFF-COMPARE apply; a sleeping toy loop is not proof of the actual client route.
<!-- DE67:DFS-SLICE:END id=R-EFF-WAIT-S001 claim=R-EFF-WAIT -->

<!-- DE67:DFS-SLICE:BEGIN id=R-EFF-FRAME-S001 claim=R-EFF-FRAME -->
### Reuse the returned observation
- [ ] 🔴 R-EFF-FRAME — The next legitimate action reuses a valid action-result frame without a redundant look.
`PlayerClient.collect` already retains the result observation, `frame()` rejects pending/terminal/absent state, and `act()` binds the current frame. `gameplay_display.display` supplies deltas; explicit `look` submits `game.observe`. Keep these owners and improve the actual playtest examples/continuation instructions. Reuse only when the last result belongs to current session/process generation and native input state and contains the needed facts. Prompt transitions, rejection with no valid successor, reentry, missing data or stale authority require supported refresh/recovery. Static controls documentation is not current action authority; do not blindly batch moves.

**Proof.** Extend `play_cli_test.py` at the real client level and retain `test_final3_style_five_step_observe_act_chain_keeps_ids_and_observations_unique`, `test_action_successor_observation_id_handoff_never_serializes_empty_id` (`cockpit_file_bridge_test.py`), `test_descriptor_action_does_not_reread_same_cycle_legacy_frame` and `test_latest_superseding_prompt_accepts_once_and_replay_is_stale` (`cockpit_observation_test.py`). In the same action/prompt/rejection/reentry sequence, assert next action references the correct successor, stale frame rejects, required refresh happens, and no extra observe is sent when unnecessary. Count observe calls and actual total work via S-EFF-COMPARE.
<!-- DE67:DFS-SLICE:END id=R-EFF-FRAME-S001 claim=R-EFF-FRAME -->

<!-- DE67:DFS-SLICE:BEGIN id=R-EFF-EVIDENCE-S001 claim=R-EFF-EVIDENCE -->
### Exact projected evidence first
- [ ] 🔴 R-EFF-EVIDENCE — Known run/request/actor/event questions use existing exact filters and selected fields before bulk retrieval.
`PlayerClient.evidence` → `evidence_events.query` freezes source bytes, records offsets/lengths/hashes, filters identity and projects `--select`. `cockpit_file_bridge` provides `log-query`, response slices/status/artifacts. Reuse those interfaces in actual investigation guidance and examples; do not add a second evidence cache. A rejection example projects identity, time, accepted/error/rejection/outcome and retrieval handle rather than whole nested receipts.

Correlation requires exact `(run_id, process_instance, request_id)`, not nearby timestamps or spoken text. Missing selected fields remain explicitly unavailable/unknown. Source handles recover the full record and omissions, including contradictions; compact views are not absence evidence. Preserve append-safe immutable snapshot and record hash semantics.

**Proof.** Extend `evidence_display_test.py`/`evidence_events` coverage through `play_cli evidence` with an exact rejection query: compact and full records yield the same independent accepted/rejected/outcome meaning and exact identity, while absent fields stay unknown and every citation resolves to original bytes. Include source change/tampering, unrelated adjacent events and a relevant contradiction. V-PY-HARNESS and S-EFF-COMPARE measure both initial projection and necessary follow-up reads.
<!-- DE67:DFS-SLICE:END id=R-EFF-EVIDENCE-S001 claim=R-EFF-EVIDENCE -->

<!-- DE67:DFS-SLICE:BEGIN id=R-EFF-SEMANTIC-S001 claim=R-EFF-SEMANTIC -->
### Jev only for useful semantic selection
- [ ] 🔴 R-EFF-SEMANTIC — Telescope helps distinguish competing explanations without model calls for exact identifiers or known flags.
Method files: `integrations/jev_telescope/README.md`, `de-67-3/scripts/instruction_context.py::telescope_guidance`, the actual harness invocation examples, and `telescope.py::request_body/validate_response/evaluate`. Existing typed candidate classification, independent counterevidence question, unknown-ID rejection, stale revalidation and baseline fallback are useful. Keep them; no generic prompt-to-JSON replacement.

Exact ID/failure/status/rejection queries go first through R-EFF-EVIDENCE with zero provider calls. A genuinely ambiguous question may send its bounded relevant candidate pool and competing hypothesis to Telescope; returned labels/IDs guide original-source inspection, never become a certified bug or completion. Explicit irrelevant/abstain is valid. Off has no calls; shadow records comparison while returning baseline; on uses validated selection. Provider failure retains deterministic retrieval, not another paid model.

**Proof.** Test the delivered discovery context and the real adapter with known-field versus competing-explanation examples. Stub call counters prove exact lookups do not invoke the provider. Preserve `test_malformed_unknown_partial_and_invalid_probabilities_fallback`, `test_all_irrelevant_is_valid_abstention_not_failure`, counterevidence and off/shadow tests. Compare frozen evidence with exact-filter and Jev routes, counting provider usage and follow-up retrievals (S-EFF-COMPARE). Do not repeat the previous paid trial without explicit live configuration; stub tests prove routing, not net semantic usefulness.
<!-- DE67:DFS-SLICE:END id=R-EFF-SEMANTIC-S001 claim=R-EFF-SEMANTIC -->

<!-- DE67:DFS-SLICE:BEGIN id=R-EFF-CANDIDATES-S001 claim=R-EFF-CANDIDATES -->
### Candidate coverage before provider selection
- [ ] 🔴 R-EFF-CANDIDATES — Telescope's harness pool removes equivalent rows before submission and retains relevant late observations and contradictions within honest scan limits.
**Confirmed gap.** `integrations/jev_telescope/harness_adapter.py::search` takes source-order rows and stops once `max_candidates` is full. `assemble` deduplicates after the provider call. Thus early irrelevant/duplicate rows crowd out later useful evidence and still cost provider tokens. `telescope.py::gather` already has deterministic lexical ranking, but the adapter does not apply it.

**Mechanism.** Start from R-EFF-EVIDENCE's exact run/actor/request/event filter and field projection. Within the configured byte/row/time scan budget, verify each candidate's source handle, build a bounded shortlist using deterministic query relevance, and deduplicate before `request_body`. Exact original record identity and identical projection for the same event may collapse, with original handles retained. Different run/process/request/actor, observation time, result or contradiction must remain distinct even if prose is identical. Never claim to know omitted rows are irrelevant. A candidate count limits the final provider pool, not the opportunity to inspect later rows within the existing scan budget. Preserve counts/reasons for exclusions, duplicates, scan truncation and final-pool truncation, plus an exact expansion route. Retain post-selection stale/tamper revalidation. No automatic limit increase, semantic duplicate model, or shadow evidence store.

**Proof.** Extend `test_harness_adapter.py` with proposed `test_pool_deduplicates_before_provider`, `test_late_relevant_row_survives_irrelevant_prefix`, `test_distinct_observations_and_counterevidence_survive_pool`, and `test_scan_and_pool_truncation_are_distinct`. Inspect the actual stub request, not only assembled output. Include absent matches/abstention and source changes between selection/retrieval. Existing `test_roots_limits_and_deduplication` is post-selection coverage and does not close the new gap. Use bounded matching pools with independent answer/counterevidence anchors and total work comparison; fewer rows alone is not success.
<!-- DE67:DFS-SLICE:END id=R-EFF-CANDIDATES-S001 claim=R-EFF-CANDIDATES -->

<!-- DE67:DFS-SLICE:BEGIN id=R-EFF-CONTINUATION-S001 claim=R-EFF-CONTINUATION -->
### Continue from current bindings
- [ ] 🔴 R-EFF-CONTINUATION — A context reset resumes the unfinished question without reconstructing static guidance or replaying pending input.
Use the existing worker handoff/checkpoint and `tools/openclaw_harness/work_context_provider.py::select_current_results/export_current_results` where current selected receipts are exported. Keep a small current note containing session/process generation and binding, pending request ID or none, retained frame/evidence handles, unresolved question and next decision. Replace obsolete current tactics while preserving immutable prior evidence. No new continuation database, competing acceptance ledger or mandatory full transcript/WEC reload.

On resume, validate current native session/action authority and source dependencies, collect the pending request before any new action, and reuse static controls guidance only as documentation. A changed generation invalidates old frame/grants; retain accepted prior receipts at their old scope and refresh/rebind through existing reentry. Use current-results export for explicit selected receipts/conclusions; it must not infer which findings are accepted.

**Proof.** Extend `play_cli_test.py`, `cockpit_live_session_test.py`, `semantic_step_test.py` and `work_context_test.py` current-results export tests with pending-response context reset and changed-generation reentry. Assert exactly one native action submission, no stale frame execution, old evidence still retrievable, and the next diagnostic decision uses the current outcome. Deliver the actual generated worker continuation example and compare repeated controls/source reads plus total effort with the same outcome (S-EFF-COMPARE).
<!-- DE67:DFS-SLICE:END id=R-EFF-CONTINUATION-S001 claim=R-EFF-CONTINUATION -->

<!-- DE67:DFS-SLICE:BEGIN id=R-ZL-PLAYTEST-S001 claim=R-ZL-PLAYTEST -->
## Revised remaining playtest: follow into a city

- [ ] 🔴 R-ZL-PLAYTEST — The remaining stalker follows the player into natural city pressure and chooses its own opportunistic attack after honestly recorded debug-only stalker setup.

**Retained frontier.** Existing integrated accounts and accepted focused light, stalker, rider, lifecycle, evolution, band, encounter and sprite work are not reopened. The current ledger's remaining gap is the stalking-to-opportunistic-attack account. The already accepted actionless activity-resume repair is a premise, not work to repeat. Old task names containing `natural-stalker-opportunism` remain historical identifiers; the owner now explicitly permits debug spawning that stalker.

**Simplest credible setup.** Add one scenario/charter/profile declaration (proposed `r_zl_stalker_follow_city_debug_setup_mcw`) using existing `startup_harness.py` mixed `debug_spawn_monster` then `cockpit_live_session` support. Reuse `writhing_stalker.live_spawn_footing_mcw.json` for the single-stalker intervention and a verified ordinary city world/profile from the `r_zl_freeplay_city042_day_mcw` family if suitable. Inspect its setup before reuse: natural city zombies, no injected zombie group, no prewritten pressure/attention/contact/attack. `writhing_stalker.live_zombie_distraction_mcw` debug-stages zombies and is unsuitable. Existing `r_zl_stalker_natural_opportunity_001/012_mcw` forbids the newly authorized setup and cannot silently remain the proof contract.

The single debug stalker spawn receives a native intervention receipt with `gameplay_credit:false`. It occurs in the same world/actor continuity as subsequent ordinary play. Ordinary player movement must first establish actual stalker following, then reach city zombies already present through natural world generation/spawning. While those zombies visibly pressure or distract the player, the same stalker chooses its approach/attack through normal AI. Do not inject a lead, opportunity score, destination, attention, contact, hit or attack outcome. If it does not follow or act despite a genuine opportunity, preserve the observation and causal code finding; setup success is not a pass.

**Authority and proof.** The registry query/charter/manifest binds this hybrid setup-plus-behavior route, with honest evidence ceiling. Technical scenario/bootstrap/rebinding repair is in scope; no old stale authority or fabricated supersession. `debug_spawn_monster_intervention_receipt` identifies setup, followed by native request/result journal, actor identity/absolute position and turn timeline for following, natural zombie footing, pressure and actual AI attack resolution entry. A miss can demonstrate a native attack attempt, as in retained focused acceptance; a selected intent or move towards the player cannot. Record the player's understandable “horrible moment” account and distinguish observed versus inferred timing.

**Executable proof.** Proposed harness `r_zl_stalker_follow_city_contract_test.py` validates one authorized stalker spawn, rejects debug zombie injection/outcome injection, retains setup receipt and requires connected native behavior evidence. V-NATIVE provides the actual Mac Tiles account using current source/build, profile, run/world/player/stalker IDs and graceful PID/birth-bound cleanup. Discuss the short setup/evidence explanation above in the handoff before commissioning play; the owner's chosen sequence already settles product setup, so no second approval is needed. No new run quota, zombie count, rare natural-stalker search or four-account replay campaign.
<!-- DE67:DFS-SLICE:END id=R-ZL-PLAYTEST-S001 claim=R-ZL-PLAYTEST -->


## Deferred experiments and release gates

**G-PRODUCTION:** R-EFF-WAIT, R-EFF-FRAME, R-EFF-EVIDENCE, R-EFF-SEMANTIC, R-EFF-CANDIDATES, R-EFF-CONTINUATION, revised R-ZL-PLAYTEST and all R-CAOL-* slices above have their named acceptance evidence, including lead dispositions and executed cleanup tests. The combined comparable-outcome efficiency report must identify savings or unresolved measurement honestly. “Everything done” means this agreed scope, not every repository TODO. Already accepted R-ZL claims remain premises.

Only after G-PRODUCTION may Pit Crew implementation/evaluation proceed. Reflex Pilot follows a completed, bounded Pit Crew evaluation, including a negative usefulness conclusion; a negative result does not require an improvement campaign or block Reflex indefinitely. Missing credentials or unverified funding semantics leave paid usefulness unverified while allowing credential-free preparation after the prerequisite gate. Neither experimental result is required for production completion.

### External evidence — S-JEV-DOCS

Official TypeSafe pages were fetched on 2026-09-19 via HTTPS (the browser extraction failed; direct `curl` succeeded). These are version-sensitive API evidence, not permission to spend.

| ID / source identity | Relevant fact | Effect on design |
|---|---|---|
| E-JEV-API-20260919 — [HTTP API](https://docs.typesafe.ai/api.md) | `POST https://api.typesafe.ai/v1/systemone`, Bearer auth, state + named typed questions; Choice has closed criteria, returned choice/probabilities/confidence; usage has input/output tokens. | Reuse Telescope's existing typed adapter; Pit Crew and Reflex select allowed candidate IDs or explicit none, then code validates. Never request arbitrary generated commands. |
| E-JEV-ERRORS-20260919 — [Python exceptions](https://docs.typesafe.ai/sdk/python/api/exceptions.md) and HTTP API error table | Structured status/body/headers/request ID are available; 401 is auth, 429 rate limit, 529 overload. The inspected index/API/error pages do not document exhausted funds/expired prepaid codes or a balance endpoint. | R-EXP-PIT-FUNDS must retain structured safe error classification; no inference that 429 means empty funds. Real provider funding shutdown remains unverified until authoritative semantics are obtained. |
| E-JEV-CHOICE-20260919 — [Choice](https://docs.typesafe.ai/primitives/choice.md), [function-calling cookbook](https://docs.typesafe.ai/cookbooks/function_calling.md) | Typed selection works over explicit options; code owns execution. Confidence reflects distribution, not authority or correctness. | Use complete action candidates in Reflex, not independent action/target selections. Include abstain/no useful intervention. Cookbook demo thresholds and budgets are not product limits. |
| E-REUSE-TELESCOPE — method commit above, `telescope.py::{provider,bounded_provider,validate_response,evaluate}` and `harness_adapter.py::{verify,assemble}` | Existing bounded child HTTP call, no implicit retry, ID/probability validation, immutable handles and fallback. Funding latch and cross-call budget are absent. | Reuse these small mechanisms; add one optional guard at the actual request boundary, and fix the confirmed pre-provider pool gap R-EFF-CANDIDATES. No second retrieval framework. |

A real paid request is not needed to discover these public contracts. If provider semantics remain undocumented at delivery, the plugin must remain off/effectively unavailable for paid modes, with a precise missing contract; do not simulate funding support and call it live-proved.

<!-- DE67:DFS-SLICE:BEGIN id=R-EXP-PIT-FUNDS-S001 claim=R-EXP-PIT-FUNDS -->
### Pit Crew request admission and durable funding shutdown — deferred
- [ ] 🔴 R-EXP-PIT-FUNDS — Optional Jev integrations stop admitting paid requests after confirmed provider exhaustion or the configured local budget, across worker restart and package update.
**Prerequisite:** G-PRODUCTION. **Owner:** de67 source, optional integration/adapter; no C-AOL gameplay changes. Existing Telescope `max_calls` is per evaluation, not a persistent funding budget. `_provider_child` currently collapses HTTP errors to a status string and cannot identify a structured funding code.

**Mechanism.** Add a small shared optional provider guard beside the existing Telescope adapter (proposed `integrations/jev_telescope/provider_guard.py`), usable by Pit Crew without enabling Telescope. Configuration distinguishes mode (`off/shadow/on`) from effective state (`ready/disabled_funds/disabled_budget/auth_error/transient_open`). Owner configuration specifies nonsecret funding-scope ID, shared durable state location, call/spending limits, concurrency, request/input bounds, timeout and bounded retry/backoff. No values here are invented acceptance thresholds. Off never touches the paid endpoint.

Use a process-safe SQLite transaction/atomic admission record at a configured owner-controlled state path outside the installed skill directory and outside disposable agent context. All local workers/workspaces sharing that funding scope resolve the same guard file. Under one admission lock check latch/budget and reserve the request; the guarded adapter is the only permitted HTTP dispatch point. Queued work holds no independent permission to send: recheck the latch immediately before dispatch under the same admission boundary. Confirmed documented provider exhaustion latches `disabled_funds`, invalidates queued/retry admission, records one state change and prevents subsequent calls. Set `disabled_budget` separately when locally configured allowance is exhausted. Already accepted/in-flight requests may still cost tokens; account them and report bounded concurrency exposure rather than promise zero overrun. Persist attempted reservations even when provider usage is unknown after a timeout.

Before implementing the funds classifier, obtain current official documentation or an authoritative provider response contract for exhausted credits/prepaid expiry and safe structured fields. Unknown funding error semantics are not a 429 heuristic. Preserve status and approved nonsecret structured code/request ID through the child boundary; exclude raw bodies/credentials from logs. Auth failures enter a separate non-retrying state; transient 429/529/timeouts use configured bounded backoff/circuit state with all retries re-admitted. Malformed responses fall back without invented findings or paid-provider substitution.

Only explicit owner re-enablement after replenishment clears the funds latch. No periodic paid probes, automatic purchases, overages or credential borrowing. An explicit owner-requested bounded validation call can be admitted after re-enablement. Share a state-change advisory through existing coordination; do not let each worker rediscover empty funds. Cross-machine limit: a local SQLite file is not a distributed global lock. Paid dispatch for a shared scope must remain on its configured owning host through existing coordination, or remote paid mode remains unavailable until an existing authenticated shared admission route is proven. Do not create a distributed service merely to hide that limit.

**Proof.** Proposed `integrations/jev_telescope/test_provider_guard.py` uses provider stubs and real guard persistence/concurrent callers. Assert counters at the actual HTTP adapter: no new dispatch after a committed latch; queued calls and retries stop; in-flight outcomes remain accounted; restart and copied-package update preserve latch; two workers sharing scope see one shutdown notice; off calls zero; explicit reset works; local-budget, auth, 429/529, timeout and malformed cases remain distinct. Preserve normal de67/Telescope fallback for every failure. Verify funding classification only against the later authoritative contract; stub labels alone do not prove real provider semantics. V-PY-METHOD applies.
<!-- DE67:DFS-SLICE:END id=R-EXP-PIT-FUNDS-S001 claim=R-EXP-PIT-FUNDS -->

<!-- DE67:DFS-SLICE:BEGIN id=R-EXP-PIT-NOTICES-S001 claim=R-EXP-PIT-NOTICES -->
### Pit Crew evidence notices and normal packaging — deferred
- [ ] 🔴 R-EXP-PIT-NOTICES — A normally packaged optional Pit Crew produces useful, bounded evidence advisories while ordinary coordination remains authoritative and operational.
**Prerequisite:** G-PRODUCTION; R-EXP-PIT-FUNDS before any paid shadow/on evaluation. **Owner:** proposed `de67-lab/integrations/jev_pit_crew/`, shipped with the normally copied de67 skill, plus a minimal optional call from the existing coordination event boundary. Keep optional imports/dependencies isolated, no background service when disabled, no credentials required for base installation. `README.md`/`RELEASE_PROMOTION.md` package the whole source/phase/scripts/references/assets/integrations tree; a linked standalone service is insufficient.

**Inputs and owners.** `worker_library.py::_audit` writes task-bound incremental event JSONL; its `_notice` uses `agent_mailbox.enqueue(workspace, "coordinator", sender, text)`. Existing durable findings/task changes/evidence references supply compact incremental candidate input. Use per-source cursor + event identity/hash so restart is replay-safe; a replaced/truncated source invalidates that cursor and requires provenance-safe recovery. Do not repeatedly load full transcripts. Mechanical task identity/status, whether independent verification was explicitly intended, and evidence freshness are code facts. Jev judges only relevant-new-evidence, potentially duplicated investigation, or evidence challenging an active assumption.

Build compact active-task candidates with stable IDs, objective/assumption and original evidence handle. A bounded Choice selects one complete relationship/candidate notice or `none`; multiple independent eligible relationships may be evaluated within the configured admission budget without a generated explanation requirement. Validate exact IDs and relationship class, recheck task/evidence freshness after response, deduplicate by original event + recipient/task + relationship, apply configured cooldown, then enqueue a fixed-template advisory identifying the possible relationship and original evidence. Existing updates remain their own carriers; the plugin never filters them away. A durable emission record plus mailbox identity prevents duplicate publication after retry; uncertain delivery is reconciled, not resent blindly.

Only the existing coordinator inbox receives notices. Pit Crew cannot assign, interrupt, terminate, approve, rewrite the specification or mark completion. Long build duration, hard work or deliberate repeat verification is not enough to claim an agent is stuck. Weak or unclear relationships produce no notice. Off has zero provider/notice activity; shadow records the validated recommendation/usage without enqueue; on enqueues only after all checks. Slow calls run off the normal coordination critical path using the existing bounded optional provider boundary; base behavior survives absent/broken/unfunded integration.

**Tests/evaluation.** Proposed `test_pit_crew.py` exercises the three relationships, none, independent verification, long work, unknown/stale IDs, changed evidence, cursor restart/source replacement, dedup/cooldown, concurrent/uncertain notice delivery, off/shadow/on and every guard failure. Observe the real mailbox, not a formatter result. Test normal package copy from a clean checkout and core operation with integration absent/unconfigured/broken. Start a shadow comparison on recorded/controlled events with independent relevance labels. Measure useful/irrelevant/missed relationships, induced follow-up effort, calls/tokens, latency and end-to-end outcomes. Only explicit configuration/funds permit a bounded enabled live experiment. A negative usefulness result is a valid completed evaluation, not a reason to expand scope. Deliver activation/deactivation, budget and exact post-top-up reset instructions plus provider uncertainties. V-PY-METHOD and the package's existing isolation tests apply.
<!-- DE67:DFS-SLICE:END id=R-EXP-PIT-NOTICES-S001 claim=R-EXP-PIT-NOTICES -->

<!-- DE67:DFS-SLICE:BEGIN id=R-EXP-REFLEX-S001 claim=R-EXP-REFLEX -->
### Jev Reflex Pilot — deferred C-AOL harness experiment
- [ ] 🔴 R-EXP-REFLEX — A small optional native-action pilot is compared honestly with the current reasoning player and a meaningful deterministic baseline.
**Prerequisite:** G-PRODUCTION and bounded Pit Crew evaluation disposition. **Owner:** C-AOL `tools/openclaw_harness/`, on an isolated development branch/worktree after the gate; not the de67 Telescope or NPC intelligence. Proposed `reflex_pilot.py`, focused tests/config and comparison entrypoint. Keep disabled by default; no automatic merge, default-player replacement or inference that a demo merits promotion. Use shared guard concepts only through a small adapter if compatible; never make a product cleanup depend on de67 internals.

**First slice.** Use one already-qualified scenario whose current native action surface supports an observable movement/interruption/recovery objective with ordinary visible targets and independent checks. The `play_cli`/semantic movement route is the starting interface. Select its exact ready scenario/binding after prerequisite changes land; an old registry ID is not a current qualification. This is a bounded scenario-selection fact, not an unchosen product design. Do not add game mechanics to create an objective. Its two small goal-directed priorities profiles are Literalist and Indecisive, provided the chosen route actually exercises straight objective pursuit and legitimate reversal/interruption; collection/Magpie remains deferred.

**Loop.** Read the retained player-visible observation and advertised legal actions from `PlayerClient`/bridge. Code forms complete candidates (action + stable target + all required parameters) under objective/constraints, never independent action/target choices. Send compact current observation, relevant recent outcomes, profile and candidate IDs to the official typed Choice endpoint with `abstain`. Treat in-game text as data. Exclude hidden oracle, debug/setup instructions and unseen internal world state from all provider inputs. Revalidate selected ID, parameters and frame/session generation immediately before `PlayerClient.act/call`. If a request is pending, collect it; never select or submit another action until resolved. Submission records identity before transport. Timeout → inspect acceptance/completion → continue same request, never replay side effects.

Stop at checkpoint, completion, terminal failure or configured total call/input/action/turn/elapsed/recovery budget. Abstention, invalid choices, stale/contradictory observation, unfamiliar action surface or stalled progress triggers bounded existing recovery then reasoning-agent escalation. Count rescue work in the same experiment budget and label rescued runs separately. No endless invalid-choice loop. Off uses existing player with zero calls; shadow records recommendations while the existing player owns action; enabled alone may execute validated choices with explicit fallback. External transmission needs explicit configuration even in shadow. Credentials remain outside code/logs.

**Independent proof/evaluation.** Proposed `reflex_pilot_test.py` stubs the provider and exercises valid complete action, abstain, unknown ID, incompatible parameters, malformed answer, stale frame, pending timeout, duplicate prevention, budgets, fallback and mode isolation through the actual PlayerClient seam. Inspect serialized requests to prove hidden setup/oracle exclusion. Record selection, harness acceptance, actual native consequence and independent verdict as separate fields/handles. Accepted command or model explanation is not completion.

Before seeing comparison results, define independently checked objective completion and meaningful recovery/transition coverage. Proposed `reflex_pilot_compare.py` runs the existing reasoning player, a simple deterministic policy where the action surface makes it meaningful, and Jev against equivalent build/scenario/objective/available information/profile/overall budget, paired starting states and relevant reported seeds/repeats. Compare confirmed defects separately from suspicions, invalid/repeated actions, stalls, escalations, total agent/provider usage and full elapsed time including latency/rescue. Action count/survival/short prompts are not test-quality proxies; exact replay of nondeterminism is not promised. Stub success proves integration only. With explicit configured credentials/funds, run a bounded live comparison; otherwise report usefulness unverified. Unsuitable Jev behavior is a valid experimental result. Deliver reproducible commands, starting commit, exact tests/results and limits. V-PY-HARNESS/V-NATIVE apply.
<!-- DE67:DFS-SLICE:END id=R-EXP-REFLEX-S001 claim=R-EXP-REFLEX -->

## Executable validation and evidence

### V-CPP: product behavior tests

The existing Makefile `tests` target recursively builds `tests/cata_test`; `tests/CMakeLists.txt` also exposes `cata_test-tiles`/CTest. The current executable's `--help` was executed during Phase 2: Catch 2.13.10 supports `--order rand`, `--rng-seed`, `--rng-seed-fuzz`, `--list-test-names-only` and `--user-dir`. List-only selection reported 22 physical-light, 38 stalker and 44 rider cases in that existing binary; this confirms selectors, not a current-source behavioral pass. The horde tag is `[hordes]`, not `[horde]`.

From the product root, the verified target/argument shape is:
```sh
make -j8 TILES=1 SOUND=1 RELEASE=1 LOCALIZE=1 LANGUAGES=all LINTJSON=0 ASTYLE=0 TESTS=1 tests
./tests/cata_test "[physical_light]" --list-test-names-only
./tests/cata_test "[physical_light]" --reporter compact --order rand --rng-seed 20260919 --rng-seed-fuzz 2 --user-dir .userdata/phase3-physical-light-tests
./tests/cata_test "[hordes],[predator]" --reporter compact --rng-seed 20260919 --user-dir .userdata/phase3-handoff-tests
./tests/cata_test "[writhing_stalker],[zombie_rider]" --reporter compact --rng-seed 20260919 --user-dir .userdata/phase3-predator-tests
./tests/cata_test "hostile_camp_local_handoff_binds_the_complete_pair_transactionally" --reporter compact --rng-seed 20260919 --user-dir .userdata/phase3-bandit-tests
```
Commands are execution routes, not mandates to rebuild/run every suite for every edit. Use exact affected names first, then related tags/integration subset justified by touched owners; no invented narrow object path. The example repeat count is a useful initial contamination probe, not a completion quota. Capture full logs, selected case/assertion count, seed, exit code and current source/binary identity. Catch list mode may return the case count as its exit status; no-match output is not a passed suite. A confirmed bug requires safe prior-behavior failure and fixed-behavior success; use an isolated temporary checkout/object comparison without altering unrelated work. Mechanical cleanup requires passing before and after, not an artificial failing test.

Use astyle 3.1 with `.astylerc` for changed C++; newer astyle rejects old options. Linux/Windows/macOS compatibility is binding. `.github/workflows/matrix.yml` and `msvc-full-features.yml` define the platform build routes; use their supported dependencies/configuration and targeted behavior tests on relevant platforms. Mac native execution plus platform-independent tests is not a claim that Windows/Linux ran. Record exact environmental limits and remaining platform proof; do not mark an affected platform route done on compilation alone. No release-branch refresh, README rewrite, migration campaign or upstream merge is required for these slices.

### V-PY-HARNESS and V-PY-METHOD

Python commands run from their owning repository. This Mac's unqualified `python3` is older and lacks `tomllib`; use the existing `/opt/homebrew/opt/python@3.14/bin/python3.14` for the verified modern harness/runtime entrypoints. On other supported hosts use their configured compatible Python.

```sh
PYTHONPATH=tools/openclaw_harness /opt/homebrew/opt/python@3.14/bin/python3.14 -m unittest play_cli_test cockpit_file_bridge_test cockpit_observation_test evidence_display_test semantic_step_test cockpit_live_session_test work_context_test
PYTHONPATH=tools/openclaw_harness /opt/homebrew/opt/python@3.14/bin/python3.14 -m unittest r022_item_spawn_adapter_test
/opt/homebrew/opt/python@3.14/bin/python3.14 -m unittest discover -s integrations/jev_telescope -p 'test_*.py'
```
The first two commands belong to C-AOL; the last belongs to de67-lab. Select named cases for the changed behavior first. New test/module names in this FS are proposed and must exist before their command is reported as executed. Provider/network stubs are appropriate for transport, mode and budget tests; they cannot prove live semantic usefulness. Tests must assert the actual request, mailbox, frame, state or evidence postcondition rather than merely inspecting helper text.

### V-NATIVE: relevant product consequence

The actual registered route is `scenario_registry_cli.py registry-query --query-file <typed-question.json>`, then the returned current validated launch/charter/profile authority. The CLI's help and commands were verified; querying is inert. `startup_harness.py` declarations and direct dry runs are preparation, not permission to bypass registry launch binding. Build source-bound Mac Tiles through:
```sh
/opt/homebrew/opt/python@3.14/bin/python3.14 tools/openclaw_harness/build_source_bound_macos.py --renderer tiles
/opt/homebrew/opt/python@3.14/bin/python3.14 tools/openclaw_harness/play_cli.py --session <current-session> --wait-seconds <remaining-bounded-wait> collect
```
Use the exact returned session and advertised action/frame/targets. Preserve source/dirty-tree, executable, scenario/fixture, run/world/player/actor, process birth and game-time identities, intervention receipts, contradictions and graceful cleanup. Screenshots/OCR/startup dismissal are not feature proof. Mac GUI recovery follows the existing verified Peekaboo socket route. A relevant native test observes movement/attack/delivery/transfer through real owners; it must not write a desired final state and call that a transition.

### Completion and optional performance

Each red claim's section names the outcome, owner, transition, independent observables, controls and artifacts that close it. A cleanup lacking its executed behavioral evidence remains red. Reports state confirmed/dismissed/already-fixed/already-covered leads, mechanical versus behavioral edits, exact commands/case counts, source/binary binding, accepted result and unverified remainder. Preserve assertions; classify pre-existing failures honestly.

Where existing instrumentation makes it inexpensive, retain deterministic work counts or an explicit optimized benchmark for blocked bandit pairs, rider pair reconciliation, light scanning with much loot, predator owner searches, repeated handoffs and retained histories. `reconcile_rider_band_encounters` scans local/loaded abstract riders and pairs; `index_loaded_z_sources` traverses loaded tiles/items. Measure those real intervals separately from delivery/pathfinding. No speculative algorithm rewrite, fixed timing assertion, invented cap or mandatory soak campaign. Randomized tests report seed/sequence; expensive soaks stay opt-in.

## Freeze record

- Status: Refrozen 2026-09-19 after source recheck against the unchanged product baseline. Durable acceptance projection and native workspace preparation are recorded in the work ledger.
- Source identities: product and method commits above; Phase-2 WEC/spec/archive/runtime preparation only. Existing dashboard dirty files and untracked product evidence are preserved.
- User-owned decisions: six promoted efficiency outcomes; one debug stalker followed into natural city zombies; ordered C-AOL regression/cleanup work; Pit Crew then Reflex as gated optional experiments; later Phase-3 launch authorized after this handoff.
- Evidence-implied design decisions: retain existing exact retrieval and pending/frame owners; pre-provider candidate filtering in the adapter; player-tile dedup limited to ground items; complete-pair atomic bandit rollback; absolute transient pressure samples; actual-turn regression for the five-minute producer; run-scoped R022 setup with no implicit ambiguous retry.
- Accepted scope: all historical stable slices and durable receipts preserved. R-ZL-PLAYTEST changes only its owner-approved remaining setup/behavior proof. New R-CAOL/R-EFF/R-EXP obligations get no inherited acceptance.
- External uncertainties: documented provider funding-exhaustion semantics remain unverified; optional paid modes require them before live admission. No credentials or paid endpoints were used in Phase 2.

After freeze, only the selected phase skill's evidence-bound closure, nonmaterial clarification and uniquely implied same-contract append-only expansion routes apply. Preserve stable identities, accepted scope and proof strength. Product intent, vocabulary, permissions, balance and materially different designs remain owner decisions. Runtime coordination/model/clock policy belongs outside this FS.

<!-- DE67:DFS-SLICE:BEGIN id=R-EFF-REGISTRY-COST-S001 claim=R-EFF-REGISTRY-COST -->
## Current registry queries without accumulated-history cost

Implementation status:

- [ ] 🔴 R-EFF-REGISTRY-COST — Routine scenario selection and exact evidence retrieval use compact current data without repeatedly rebuilding or returning the registry's accumulated history.

**Owner outcome.** Josef requested immediate control of harness registry and token overhead on
2026-09-19. Separate storage bytes, internal reads/latency, serialized artifacts and model-visible
output. Keep complete historical evidence retrievable without injecting it into ordinary decisions.

**Current evidence.** `.userdata/openclaw_harness/scenario_registry.sqlite3` is 709,316,608 bytes;
606 current manifests contain about 4.76 MB of declaration/validation JSON. The selection path in
`scenario_registry_store.py::build_registry_query_candidate_snapshot` calls `_current_route_evidence`
for every candidate, with repeated historical decoding and resolution/binding lookups. A retained
read-only evaluation exceeded 30 seconds. The page audit attributes 416,043,008 bytes to `certification_round` and 208,306,176 to
`certification_round_component`: 73 rounds store repeated worktree/data/harness inventories. The
latest 6.06 MB manifest repeats roughly 3.03 MB under both `binding.components` and
`binding.authoritative_components`, then component facts are stored again. Inspect those producers
and consumers when eliminating future duplicate persistence; preserve existing evidence. Actual
agent-output contribution remains to be measured; disk size alone does not establish prompt size.

**Repair.** Inspect the existing current projections, query predicates, indexes, history writers and
compact-output routes. Remove repeated historical work and redundant stored/returned payloads where
the source proves them unnecessary. Preserve every distinct route and contradiction needed for
selection, lifecycle, current source/binding checks and token authority. Retain immutable history
and full-retrieval handles; no live-history deletion, VACUUM workaround, evidence ceiling change,
or arbitrary result cap. Exact known identities should avoid unrelated candidate/history work.
Choose the smallest supported implementation; a new database or background service is not assumed.

**Proof.** Reproduce on a frozen copy or disposable fixture with accumulated history. Compare the
same selected/rejected candidates and authority, stale/rebound negative controls, full retrieval,
query counts/latency, stored payload growth and actual returned bytes before/after. Include the
normal CLI compact path and an exact saved-query/run retrieval. Distinguish measured local changes
from unmeasured account-wide savings. No game replay or paid provider call is needed for this repair.
<!-- DE67:DFS-SLICE:END id=R-EFF-REGISTRY-COST-S001 claim=R-EFF-REGISTRY-COST -->

<!-- DE67:DFS-SLICE:BEGIN id=R-CAOL-NATIVE-REGRESSION-S001 claim=R-CAOL-NATIVE-REGRESSION -->
## Post-cleanup native regression confidence — owner addition, 2026-09-20

Implementation status:

- [ ] 🔴 R-CAOL-NATIVE-REGRESSION — The combined cleanup build preserves the affected light, actor ownership/history, setup and diagnostic behavior through substantive native regression playtests using established routes.

**Authority and priority.** Josef explicitly requests fresh regression playtesting of the completed
cleanups. This is an additional verification obligation, not invalidation of the accepted
R-CAOL implementation/test results or the successful R-ZL stalking-to-attack account. Relevant
previously tested gameplay may and should be rerun where it establishes current post-cleanup
confidence; earlier generic no-replay wording does not bar this work. G-PRODUCTION now additionally
requires this claim's acceptance before further optional Pit Crew/Reflex implementation/evaluation.
Preserve already returned optional candidates and results without replaying or discarding them.

**Current gap.** LIGHT-FIXTURE, HANDOFF, HISTORY, LIGHT-EXTRACT, SETUP-LIFETIME and DIAGNOSTICS
have substantial focused source/build/test evidence, but that evidence is not a complete native
combined-build playtest. BANDIT-QUERIES found no useful equivalent predicate to extract, so do not
invent a refactor there. LIGHT-TURNS includes native rider source-off/save/reload/expiry evidence;
reuse that route and its controls, but bind new observations to the combined cleanup source/build.
The accepted stalker attack alone does not exercise all these affected integration boundaries.

**Behavioral coverage.** Use coherent existing scenarios and ordinary native actions to establish:

- Light discovery and real-turn delivery: movement beside/onto/away from a stationary source,
  independent ground/carried emitter provenance, brief eligible exposure, source-off or occlusion,
  actual recipient behavior and finite memory/expiry. Cover affected horde, staffed and predator
  recipients at their documented distinct cadences; do not demand an immediate staffed response
  where its contract retains a slower cadence. Repeated read/redraw without elapsed time must not
  refresh evidence. Include native save/reload or bubble travel and an order-sensitive rider/light
  encounter. Existing focused tests protect exceptional/unobservable branches; they supplement,
  not replace, the native integrated account.
- Ownership and transient history: meaningful native local/abstract travel and save/reload of
  identified predators and a bandit ownership/contact route, with source-bound actor/member and
  resource observations. Preserve one owner, persistent identity/state, complete-pair forward
  ingress and legitimate survivor homeward behavior; bubble/world changes must not turn old local
  perception into fresh pressure. Cover actual reachable refusal/retry behavior and distinguish
  deterministic failure-injection tests from naturally observed native outcomes.
- Setup and diagnostics: run the existing R022 native item-setup route through its loaded-world
  boundary and supported run/reentry lifecycle, observing exact tagged effects/cleanup and no
  repeated side effects. Retain absent-configuration and failure/retry controls through existing
  focused production-adapter tests. Exercise affected predator actions with diagnostics enabled and
  disabled using an existing controlled comparison where meaningful; compare decisions, time,
  resources and evidence semantics, not wall-clock microseconds or identical nondeterministic AI.
  Share these observations with the other routes when it avoids duplicate launches.

**Execution and proof.** The starting route map is `.de67/task-logs/review-cleanup-2c228bdd5843/coverage-map.md`; its named assets exist, but their old bindings are not current launch authority. Not every low-level assertion needs a separate game run: retain focused proof for controlled/internal states while establishing the affected integrated behavior natively.

The workers choose suitable existing scenarios, charters, saves and test
selectors after checking their actual declarations and current registry authority. Setup/debug
interventions remain explicitly zero-credit; never inject the reaction or outcome being tested.
Run a current source-bound executable, reuse the affected established test suites and record
nonzero selections. Capture native request/result and event/state evidence plus a concise player-
legible account of what happened, with source/build/run/actor/time bindings, contradictions and
cleanup. Screenshots/OCR, launch success, green unit tests or this coverage plan alone cannot close
the native obligation. Inspect selected original evidence at closure. Repair demonstrated in-scope
product/harness regressions and rerun their affected proof; an inaccessible route needs a concrete
continuation or honest unsupported boundary, not a fabricated pass. Supported same-process reentry
must be distinguished from restarting an executable. Use coverage/risk to determine sufficient play,
not a fixed action/run count, compulsory new framework, or unrelated replay of every accepted feature.
<!-- DE67:DFS-SLICE:END id=R-CAOL-NATIVE-REGRESSION-S001 claim=R-CAOL-NATIVE-REGRESSION -->
