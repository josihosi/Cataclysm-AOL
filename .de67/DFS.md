# Semantic-Surface Cockpit and Fresh CAOL Feature Package FS — Functional Specification

Status: Refrozen
WEC: `.de67/WEC.md`
Source baseline: `Cataclysm-AOL-hostile-ecology-dev | dev | 1bfcf283417d63ae407bc66fc9950a90a68dd5b5 | tree 1bbb62eebc15bc887c133b26055f8b5f8720c5ec | inspected 2026-09-06 | no tracked product changes; unrelated untracked run.witness.json excluded`

## Document authority

The FS (Functional Specification) describes code behavior in terms of data, functions, ownership, interactions and errors. `.de67/DFS.md` and `DE67:DFS-SLICE` remain compatibility identifiers until the existing readers and writers are migrated; they do not name a different specification. Delivery assignments belong in `work-ledger.md`; retained status blocks currently serve the acceptance projection, not additional functional requirements.

This is the mechanistic product contract derived from the owner-authorized WEC and current source.
The WEC decides intended behavior and authorization; source decides what the implementation does.
The WEC's 2026-09-06 refinement requires fresh behavioral coverage of every in-scope CAOL family,
including historically accepted features. A red proof obligation does not by itself authorize a
CAOL gameplay repair. The WEC preserves the owner's fault responsibilities and promotion boundary.

The previous DFS is preserved exactly in Git at `1bfcf283417d63ae407bc66fc9950a90a68dd5b5:.de67/DFS.md`.
Its stable claim identities, acceptance decisions, scenarios, reports and evidence remain history.
The accepted statements below retain their historical scope; they do not assert fresh campaign
success. Dated continuation instructions in that previous version are not current product facts.
This refreeze replaces its stale code map and separates fresh proof obligations from old acceptance.

- `[x]` records historically accepted production behavior and its retained evidence.
- `[ ] 🔴 R-...` records missing, wrong, or unproved behavior at this refreeze. A proof gap can require
  testing without any gameplay implementation change.

<!-- DE67:DFS-SLICE:BEGIN id=DE67-MAINT-CADENCE-S001 claim=DE67-MAINT-CADENCE -->

### Coordinator review cadence maintenance

- [ ] DE67-MAINT-CADENCE — Temporarily schedule periodic evidence-led workflow review after an
  inclusive 10–20 completed worker attempts. Preserve the current cycle's already-counted progress,
  count each terminal attempt once, persist the schedule across coordinator restart, and leave active
  workers and product-claim acceptance unchanged. The next review chooses a useful improvement from
  current evidence; legacy random-lane metadata does not prescribe the inquiry.
- Proof: focused boundary and counting tests pass, the current lineage cycle retains elapsed progress
  while adopting the shorter interval, and a fresh status/restart observation reports the effective
  interval and next due count without interrupting workers.

<!-- DE67:DFS-SLICE:END id=DE67-MAINT-CADENCE-S001 claim=DE67-MAINT-CADENCE -->

## Functional contract

CDDA is the semantic authority. The current input owner publishes the semantic surface, surface
stack, frame ID, stable IDs and valid actions. The cockpit replaces its active presentation with
that frame and receives the receipt from the exact consuming or rejecting owner.

```text
native input owner -> current descriptor -> exact frame-bound semantic request
-> native binding -> receipt and actual successor owner -> observed gameplay consequence
```

A child hides parent actions. An unsupported interface publishes its owner and diagnostics with no
executable actions and stops automated input. There is no raw-key, OCR, guessed Escape, parent-action
or screenshot-guided fallback. The same protocol supports graphical and curses rendering. Shared
native menu instrumentation and focused custom adapters provide broad coverage without a menu quota.
The wait/overmap loops are proving routes, not a delivery ceiling.

Fresh players receive understandable situations and gameplay questions, choose useful actions,
investigate consequences and retain both mechanical and gameplay-feel evidence. The package covers
living NPC intent/context and follow/camp routing; camp establishment and missions; Locker, Patrol,
Food and Storage; bandit signal/scout/demand/payment/refusal/return; cannibal discovery/day hold/night
departure/approach through dawn; signal controls and world boundaries; persistence; flesh raptors;
and integrated performance. Ordinary CDDA actions are dependencies where these routes need them.
Writhing stalkers and zombie riders remain excluded. The shared player-light implementation is not
redesigned by the hostile-ecology refinement; its relevant signal behavior still receives fresh tests.

Bandits and cannibals discover player-created physical signals through staffed observation, camp
memory, physical scouting, scout report and travel. Policy may differ after the report. Cannibals
wait at rally for night, then remain committed through dawn. Normal bandit contact presents the
shakedown before aggression. Completed payment protects the exact group's departure; refusal,
incomplete payment or player attack releases combat. The forced Pay or Fight interface and the
favorable rolling-travel ambush remain intentional. No automatic theft or contact-time night gate
is required.

Each test has an independent result. Accepted input, valid witness structure, launch, setup, or an
old green report cannot substitute for the new gameplay result. A mixed run may support one claim,
contradict another and leave another unobserved.

## Project language and terminology

Use **semantic surface**, **input owner**, **surface stack**, **frame ID**, **stable ID**, **valid
actions**, **receipt**, **native binding**, **physical signal**, **staffed observer**, **camp memory**,
**scout report**, **night raid**, **shakedown**, **payment**, **refusal**, and **rolling-travel ambush**
as defined by the WEC. Namespaced action examples are not exhaustive whitelists. Screen position,
menu letter, hotkey, label, process address and an obsolete frame are not actionable stable IDs.
An item UID identifies the live selectable item; after native transfer or reload, rediscover the
current UID and use type/count/location/actor evidence for cross-boundary continuity.

## Current code map

All source references below are at the inspected HEAD, rather than the old dirty-tree manifests.
Tests named here supply mechanism evidence unless explicitly described as live gameplay evidence.

| Concern | Production files and symbols | Current behavior and evidence boundary |
|---|---|---|
| Surface truth and transport | `src/semantic_surface.h/.cpp :: semantic_surface_manager`, `semantic_surface_scope`, `submit_request`, `consume_top_request`, `republish_top`; `src/input_context.cpp :: handle_input` | Run-local stack, request queue and completed-request cache; top game-thread consumer owns native action. Changed publication/push/pop creates a frame; deferred receipts bind actual successors. Renderer backends wake only. |
| World and wait | `src/handle_action.cpp :: game::handle_action`, `wait` (file-local), `openclaw_harness_semantic_surface_manager`, `openclaw_harness_world_payload` | World scope is in real input handling (around 4410), not draw-time ownership. Duration owner at 2385 suppresses duplicate uilist ownership and maps `wait.1m`/`wait.5m` to native durations. Qualification records three 5-minute waits; this verifies the WEC's narrow precedent, not ecology. |
| Shared menu/prompt family | `src/uilist.cpp :: uilist::query`, `src/popup.cpp :: query_popup`, `src/string_input_popup.cpp` | Entry-owned IDs, native enabled state and prompt validation are implemented. Caller-owned scopes suppress the generic scope when a custom owner is authoritative. |
| Focused surfaces | `src/overmap_ui.cpp :: overmap_ui::display`; `src/inventory_ui.cpp :: inventory_selector` and derived selectors; `src/npctalk.cpp :: dialogue::opt`; `src/action.cpp :: choose_direction`; `src/ranged.cpp :: target_ui::run`; `src/editmap.cpp`; `src/debug_menu.cpp` | Existing focused adapters publish native state, stable targets and exact requests. Old descriptions saying these adapters do not exist are superseded. Accepted renderer witnesses are retained below; qualification is narrower than every reachable owner. |
| New inspection and terminal owners | `src/npc_inspection.cpp :: show_npc_inspection`, `resolve_npc_inspection_actor`, `npc_inspection_item_payload`; `src/end_screen.cpp`; `src/game.cpp :: game::is_game_over`; `src/do_turn.cpp :: turn_handler::cleanup_at_end` | Actor/item inspection and native terminal choices exist. The qualified NO/NO death route reaches truthful actionless `MESSAGE_LOG`. Full message-viewer operation remains unsupported. |
| Input inventory | `tools/openclaw_harness/input_owner_coverage_test.py`; `src/input_context.cpp :: unsupported_semantic_input_owner` | The file-level inventory is not per-loop completeness proof. Current test fails because `src/npc_inspection.cpp` is absent from its classified sets. The inspection owner itself has native scopes and `tests/npc_inspection_test.cpp`. |
| Cockpit and player client | `tools/openclaw_harness/cockpit.py :: CockpitRunChannel`, `CockpitService`; `startup_harness.py :: execute_semantic_act`, `refresh_semantic_step_trace`; `semantic_state.py`; `play_cli.py :: PlayerClient` | Descriptor-only dispatch, retained exact receipts, successor-timeout distinction, pending request collection, cooperative cancel, reentry generation and explicit finish. Legacy non-descriptor dispatch returns `native_surface_descriptor_required`. Failures preserve the game. |
| Evidence | `playtest_witness.py :: validate_witness_statement`, `validate_witness_bundle`, `review_witness`; `scenario_registry_store.py :: record_playtest_witness`, `review_playtest_witness`; `cockpit_file_bridge.py` | Independent claim witnesses, immutable full artifacts, exact selectors and separate causal review exist. Mechanical validity does not settle gameplay or bug responsibility. |
| Living NPC policy | `src/llm_intent.cpp :: build_snapshot_json`, `enqueue_*`; `src/npcmove.cpp :: npc::execute_llm_intent_action`; `src/npc.cpp :: llm_intent_state_map`; `src/npctalk.cpp`, `src/npctalk_funcs.cpp` | NPC request context and native action execution are separate from cockpit input ownership. Intent-map state is process-local; NPC assignment/mission/rules are saved. Actual utterance/recipient/reply must be correlated, not inferred from prewarm. |
| Camp | `src/faction_camp.cpp :: talk_function::start_camp`, `talk_function::basecamp_mission`, `basecamp::start_mission`, `camp_food_supply`, `locker_policy_ui`; `src/basecamp.cpp :: form_storage_zones`, `process_camp_locker_downtime`, `service_camp_locker`, `refresh_patrol_shift_cache`, `get_current_patrol_runtime` | Native establishment, mission/food accounting, zone-derived storage, queued Locker service and Patrol runtime exist. A prepared camp or opening the selector proves none of their natural completion. |
| Signal and response | `src/do_turn.cpp :: overmap_npc_move`, `live_bandit_staffed_camp_signal_reads`; `src/bandit_live_world.cpp :: record_staffed_camp_signal_observations`, `advance_structural_bounty_maintenance` | Five-minute signal reads and ordinary structural maintenance own observation and response. The cannibal-only dispatcher remains an unused definition, with no production call. Player-opportunity adoption is a separate discovery path; structural sound recording belongs to active-scout investigation. Proof distinguishes both from idle staffed observation. |
| Hostile contact | `src/do_turn.cpp :: advance_live_bandit_hostile_approaches`, `live_bandit_handle_hostile_shakedown_contact`, `live_bandit_commit_paid_return`, `live_bandit_choose_fight`; `src/bandit_live_world.cpp :: hostile_operation_player_relationship_for`, `choose_local_gate_posture`; `src/npc.cpp :: guaranteed_hostile`, `attitude_to` | Night gates rally departure only. Exact operation members receive parley/paid-departure overrides before generic hostility. This implementation supersedes the old claim that protection is absent; full fresh natural turn-order proof is still open. |
| Persistence | `src/savegame_json.cpp :: npc::serialize/deserialize`, `monster::serialize/deserialize`; `src/bandit_live_world.cpp :: site_record::serialize/deserialize`, `hostile_operation_state::serialize/deserialize`; `src/overmapbuffer.cpp`; `src/clzones.cpp` | Durable actors/rules/camp/zone/ecology state has production save paths. Frame identities, LLM queues and camp caches do not become durable truth. Reload must use a new process and the saved world without fixture reinstall. |
| Flesh raptors | `src/monmove.cpp :: is_flesh_raptor`, `apply_flesh_raptor_plan`; `src/flesh_raptor_ai.cpp :: choose_orbit_destination`; `tests/flesh_raptor_test.cpp` | Native monster planning selects orbit/swoop/fallback with visibility, occupancy, cadence and held-destination state. Pure scorer and staged monster tests are not fresh encounter proof. |
| Performance | `tools/hostile_camp_benchmark.py`; `tools/openclaw_harness/process_performance.py :: ProcessPerformance`, `sample_owned_session`, `compare_records`; native renderer and hostile-camp timing | Exact process CPU/RSS and native-action timings are available. Finite qualification samples and parser-allocation experiments do not qualify integrated gameplay performance. |

## Mechanistic requirements

The following semantic contracts retain their strength. Their acceptance summaries are historical;
current-source coverage gaps are explicit in R-SURFACE-011 and fresh gameplay obligations below.

### 1. Semantic surface stack and frame identity

<!-- DE67:DFS-SLICE:BEGIN id=R-SURFACE-001-S001 claim=R-SURFACE-001 -->

Mechanism:

- Files and symbols: `src/semantic_surface.h` and `src/semantic_surface.cpp` define
  `semantic_surface_manager`, `semantic_surface_scope`, `semantic_surface_descriptor`,
  `semantic_action_request`, and `semantic_action_receipt`; the manager connects at the native input
  loops named by this DFS.
- Entry point: a native input owner constructs a `semantic_surface_scope` before its first frame can
  accept input and destroys it only after the owner yields or returns.
- Inputs: `kind`, owner-provided breadcrumb label, structured surface state, stable entries,
  and owner-provided native bindings.
- Preconditions: the harness is enabled for one run; the scope has one owning native loop; a parent
  scope may exist but cannot be executable while the child is topmost.
- Transition: pushing a scope creates a new `surface_id`, adds it to the surface stack, and publishes
  a new frame ID. Any state change that changes information or valid actions publishes a new frame
  ID. Popping a scope invalidates every child frame and republishes the parent with a fresh frame ID.
- Postconditions: exactly the top scope can advertise or consume an action. The descriptor contains
  schema version, run ID, surface ID, frame ID, kind, complete breadcrumbs, payload, and valid
  actions.
- Failure behavior: an input loop without a supported explicit scope registers an `unsupported`
  scope before automation can act. Its valid-actions collection is empty. A stale, wrong-surface,
  missing or malformed request changes no game state. An exact duplicate request returns its
  recorded receipt without applying the action again; pending duplicates are not queued twice.
- Persistence/compatibility: surface IDs and frame IDs are runtime identities and are not save-game
  state. Each run starts a new identity domain. The mechanism is independent of Tiles, Android, and
  curses rendering.

Historical acceptance (full attempt history remains in the prior Git version):

Implementation status:

<!-- DE67:DELIVERY-STATUS:BEGIN claim=R-SURFACE-001 -->
- [x] R-SURFACE-001 — The live Tiles and curses routes prove the renderer-neutral semantic surface stack, exact top-owner
  exclusivity, fresh frame IDs, breadcrumbs, and actionless unsupported hard stop through the live
  Tiles and curses routes.
  - DFS slices: `R-SURFACE-001-S001`
  - Final proof: `.userdata/r-surface-001-inventory-prompt-curses/harness_runs/20260901_050125_2b12535db356419b8761e63b5e18fb79/probe.report.json` is feature-path proof and 46 focused harness tests passed.
  - Durable acceptance: #1 via `R-SURFACE-001-closure-033`; SQLite evidence is authoritative.
<!-- DE67:DELIVERY-STATUS:END -->
<!-- DE67:DFS-SLICE:END id=R-SURFACE-001-S001 claim=R-SURFACE-001 -->

### 2. Native semantic request and receipt path

<!-- DE67:DFS-SLICE:BEGIN id=R-SURFACE-002-S001 claim=R-SURFACE-002 -->

Mechanism:

- Files and symbols: `src/semantic_surface.h` and `src/semantic_surface.cpp` integrate request
  delivery with `src/input_context.cpp :: input_context::handle_input` and
  `src/input.h :: input_manager::get_input_event`. The renderer backends provide wake-only support.
  `tools/openclaw_harness/startup_harness.py :: execute_semantic_act` submits descriptor-bound JSON
  requests and native wakes; `semantic_broker.py :: SemanticStepChannel` and `CockpitRunChannel`
  validate transactions. The executable descriptor path no longer uses physical dispatch.
- Entry point: the cockpit submits `semantic_action_request { run_id, surface_id, frame_id,
  request_id, action_id, stable_id?, parameters? }` to the live CDDA process.
- Inputs: only fields advertised by the current descriptor are accepted. Private key sequences are
  not part of the descriptor or request.
- Preconditions: request run, surface, frame, action namespace, target stable ID, and parameter
  schema match the current top scope. The request ID has not been consumed.
- Transition: the top native owner resolves the semantic action through its registered native
  binding and selects/invokes the same native behavior used by local input. It records the exact
  consuming frame and emits an accepted or rejected receipt. Deferred accepted receipts are completed
  when the actual successor publishes; synchronous modal actions may receipt before opening a child.
- Concurrency and ordering: `semantic_surface_manager::submit_request` checks nonempty request
  identity, suppresses pending duplicates and replays completed receipts; the transport parser builds
  requests. Only the game-thread top scope validates or rejects the semantic
  action. Request arrival makes the queue observable to a blocking
  `input_manager::get_input_event` implementation but does not manufacture an `input_event`, action
  descriptor, key, mouse event, or timeout. The game thread asks the current top scope to consume the
  queued request before it processes another physical event. CDDA serializes consumption and native
  state mutation. `republish_top` publishes a real successor before completing a deferred receipt
  with that frame ID. Consumers bind the receipt and successor by identity, not assumed log order.
  If the successor is missing, the adapter preserves any accepted receipt, reports the missing
  successor separately and revokes stale action authority.
- Postconditions: the receipt contains request ID, run ID, requested surface ID and frame ID,
  consuming or rejecting surface ID and frame ID, action ID, accepted state, rejection reason when
  present, and resulting frame ID when a fresh frame exists. For an accepted action, requested and
  consuming identities are equal. For a stale or wrong-surface rejection, the receipt preserves the
  requested identities and identifies the current frame that rejected the request.
- Failure behavior: transport loss does not synthesize acceptance. Duplicate request IDs return the
  recorded result without applying the action twice. No path translates the request to a keyboard
  event, Escape, menu letter, mouse coordinate, or screenshot-guided control.
- Persistence/compatibility: requests and receipts are run-scoped runtime records. Their protocol is
  common to graphical and terminal rendering. `llm_intent` remains a separate NPC policy system and
  cannot intercept or fabricate cockpit receipts.

Historical acceptance (full attempt history remains in the prior Git version):

Implementation status:

<!-- DE67:DELIVERY-STATUS:BEGIN claim=R-SURFACE-002 -->
- [x] R-SURFACE-002 — The source-current native route proves that only the active input owner consumes semantic requests and returns exact receipts without keyboard, mouse, focus, or screenshot control.
  - DFS slices: `R-SURFACE-002-S001`
  - Final proof: run `20260901_062147_6936ef4aad934189826b83b042bfb41a` is source-current feature-path proof and 40 focused semantic-step tests pass.
  - Durable acceptance: #1 via `R-SURFACE-002-closure-003`; SQLite evidence is authoritative.
<!-- DE67:DELIVERY-STATUS:END -->
<!-- DE67:DFS-SLICE:END id=R-SURFACE-002-S001 claim=R-SURFACE-002 -->

### 3. Shared generic menu and prompt instrumentation

<!-- DE67:DFS-SLICE:BEGIN id=R-SURFACE-003-S001 claim=R-SURFACE-003 -->

Mechanism:

- Files and symbols: `src/uilist.h` / `src/uilist.cpp :: uilist_entry`, `uilist::query`,
  `query_once`; `src/popup.h` / `src/popup.cpp :: query_popup`;
  `src/string_input_popup.h` / `src/string_input_popup.cpp` use the common semantic surface manager.
- Entry point: each ordinary menu or prompt constructs its semantic scope from the same native data
  it renders and checks before its input loop.
- Parameters: each executable entry stores an opaque `semantic_stable_id` assigned when that entry object is
  created. It remains unchanged across filtering, sorting, scrolling, selection, and redraw. A
  caller may supply a domain ID. Otherwise the entry owns a scope-local opaque token; it is never
  derived from vector index, display order, hotkey, or coordinates.
- Inputs: menu title and text, entry label and description, enabled state, current selection, native
  logical action, prompt constraints, and text value come from the native control.
- Preconditions: only actions that the native control can execute in its current state are
  advertised. A callback-defined operation without an explicit semantic binding is visible but not
  executable.
- Transition: `menu.select`, `menu.choose`, `menu.filter`, `menu.clear_filter`, `menu.cancel`, and
  prompt actions change native control state through the control's existing decision path. Direct
  selection by stable ID replaces row-by-row screen navigation when both reach the same native
  selection state. The adapter exposes a distinct semantic action when the native owner assigns
  behavior to selection movement itself. `menu.cancel` exists only when that owner supports
  cancellation. A text prompt accepts structured text through `prompt.submit`, subject to its native
  constraints.
- Postconditions: the next frame reflects selection, filtering, validation, or parent restoration.
  The exact child frame receipts the action.
- Failure behavior: disabled, missing, duplicate, stale, or constraint-invalid stable IDs are
  rejected. No universal meaning is assigned to Escape.
- Persistence/compatibility: generic IDs are stable for the life of the native surface. Domain IDs
  may be more durable. The same structured descriptor drives Tiles and curses presentations.

Historical acceptance (full attempt history remains in the prior Git version):

Implementation status:

<!-- DE67:DELIVERY-STATUS:BEGIN claim=R-SURFACE-003 -->
- [x] R-SURFACE-003 — The shared native menu and prompt family is proven on the accepted semantic request boundary.
  - DFS slices: `R-SURFACE-003-S001`
  - Final proof: run `20260901_080338_00e8f9d5a1d847a8bbd15e9d55d3c8c4` is source-current feature proof, and the native suite passes 15 cases with 102 assertions.
  - Durable acceptance: #1 via `R-SURFACE-003-closure-002`; SQLite evidence is authoritative.
<!-- DE67:DELIVERY-STATUS:END -->
<!-- DE67:DFS-SLICE:END id=R-SURFACE-003-S001 claim=R-SURFACE-003 -->

### 4. World semantic surface

<!-- DE67:DFS-SLICE:BEGIN id=R-SURFACE-004-S001 claim=R-SURFACE-004 -->

Mechanism:

- Files and symbols: the World scope surrounds actual input handling in
  `src/handle_action.cpp :: game::handle_action`; visibility, map, entity and zone builders populate
  `openclaw_harness_world_payload`. `src/messages.h` / `src/messages.cpp` owns structured retained messages.
- Entry point: world input becomes top owner after load or after a child scope returns.
- Inputs: avatar state, local map, visible creatures, terrain, zones, the full retained player
  message history from `Messages::recent_messages( Messages::size() )`, current world mode, and
  currently valid native world actions.
- Preconditions: world is the exact top owner; no activity prompt, inventory, dialogue, targeting,
  direction, overmap, or other child owns input.
- Transition: namespaced world actions invoke the matching logical game action. A native action that
  opens a child completes with a receipt from the world frame and a fresh child frame.
- Postconditions: the cockpit receives the local map and current world facts only while world owns
  input. Returning from a child produces a fresh world frame.
- Failure behavior: if world state cannot be built consistently, the owner publishes no executable
  actions. The existing `MESSAGE_LIMIT` option bounds the native retained message history. Cockpit
  code does not add a message count or change the semantic payload based on renderer viewport size.
- Persistence/compatibility: world observation does not mutate or persist state. It is renderer
  independent.

Historical acceptance (full attempt history remains in the prior Git version):

Implementation status:

<!-- DE67:DELIVERY-STATUS:BEGIN claim=R-SURFACE-004 -->
- [x] R-SURFACE-004 — The live Tiles and curses routes prove the renderer-neutral World owner, complete native payload, child exclusivity, and exact child-return receipts.
  - DFS slices: `R-SURFACE-004-S001`
  - Final proof: closure-001 settles the World source contract. Closure-002 settles the Tiles child-return gap with source-bound run `20260901_084803_c48c48ab407b4bb2b4aae7a32ad3b867`. Closure-003 settles the matching curses child-return gap with source-bound run `20260901_085008_8c8797aa81674128abe4b32aae7b66ff`. Each renderer proves World, Inventory, fresh World, Debug menu, and fresh World through four exact receipts. Durable acceptance 1 settles the whole claim after all three gaps closed.
  - Durable acceptance: #1 via `R-SURFACE-004-closure-003`; SQLite evidence is authoritative.
<!-- DE67:DELIVERY-STATUS:END -->
<!-- DE67:DFS-SLICE:END id=R-SURFACE-004-S001 claim=R-SURFACE-004 -->

### 5. Overmap semantic surface

<!-- DE67:DFS-SLICE:BEGIN id=R-SURFACE-005-S001 claim=R-SURFACE-005 -->

Mechanism:

- Files and symbols: `src/overmap_ui.cpp :: overmap_ui::display` and its
  `overmap_draw_data_t` state with a focused semantic scope.
- Entry point: `OVERMAP` becomes active before its first render/input iteration.
- Inputs: only discovered terrain visible under native rules, player position, cursor position,
  selected location and detail, route preview when present, level/zoom/mode state, and currently
  valid overmap actions.
- Preconditions: the overmap loop is top owner. Debug-only actions require the same native debug
  authorization as local input.
- Transition: `overmap.move_cursor`, `overmap.select`, `overmap.choose_destination`,
  `overmap.add_note`, `overmap.change_level`, `overmap.close`, and other supported namespaced
  actions invoke the corresponding native overmap branch. Coordinate or entry targets carry stable
  IDs tied to the current overmap surface state.
- Postconditions: cursor, selection, preview, or parent restoration is visible in a fresh frame.
- Failure behavior: undiscovered data is not exposed. Invalid coordinates, stale selections, or
  unavailable actions are rejected without moving the native cursor.
- Persistence/compatibility: normal overmap note/destination persistence remains owned by existing
  game code. The semantic layer adds no second store.

Historical acceptance (full attempt history remains in the prior Git version):

Implementation status:

<!-- DE67:DELIVERY-STATUS:BEGIN claim=R-SURFACE-005 -->
- [x] R-SURFACE-005 — The live Tiles and curses routes prove the renderer-neutral Overmap owner, discovered-only payload, stable targets, exact native receipts, and fail-closed hidden terrain.
  - DFS slices: `R-SURFACE-005-S001`
  - Final proof: The source contract, Tiles route, and curses route are independently closed. Tiles run `20260901_092728_1c7efc46c4904460a53759d6ec17f664` and curses run `20260901_094019_a6462e09cd8f4481abc4400634209a9c` prove matching native Overmap behavior, exact receipts, hidden-terrain omission, and fresh World restoration. Durable acceptance 1 settles the whole claim after all three gaps closed.
  - Durable acceptance: #1 via `R-SURFACE-005-closure-003`; SQLite evidence is authoritative.
<!-- DE67:DELIVERY-STATUS:END -->
<!-- DE67:DFS-SLICE:END id=R-SURFACE-005-S001 claim=R-SURFACE-005 -->

### 6. Inventory semantic surfaces

<!-- DE67:DFS-SLICE:BEGIN id=R-SURFACE-006-S001 claim=R-SURFACE-006 -->

Mechanism:

- Files and symbols: `src/inventory_ui.h/.cpp :: inventory_selector` and its derived
  selectors; use `src/item.h :: item_uid` and `src/item_location.cpp :: find_item_by_uid`.
- Entry point: each selector becomes a semantic scope before selection begins and supplies its
  purpose in the breadcrumb, such as `world › inventory` or `world › inventory › use lighter`.
- Inputs: entries, item details already available to the selector, enabled state, highlighted item,
  selected quantity/count, selector mode, and the exact inventory actions that mode accepts.
- Preconditions: item-backed entries resolve to the same live item UID and location under the
  selector's native validity rules. Non-item entries have explicit scope-local stable IDs.
- Transition: the adapter maps namespaced semantic actions to the current selector's existing
  `process_input`, `on_input`, or derived `execute` branch. The action set includes selection,
  filter/reset, examine/details, contents, quantity, the selector's mode-specific commit operation,
  and cancellation when the native selector permits each operation. Derived selectors also expose
  their permitted operations, including wield, wear, pickup, drop, insert, or trade behavior when
  that selector registers and handles the corresponding native action. The stable target ID, never
  an invlet or row, selects the entry.
- Postconditions: selection/detail state or the resulting child/parent surface appears in a fresh
  frame. The exact selector frame receipts the action.
- Failure behavior: moved, destroyed, merged, inaccessible, disabled, stale, or wrong-selector items
  are rejected and are not retargeted by name, invlet, display order, or coordinate.
- Persistence/compatibility: action identity stays owned by the live `item_uid`. Native transfer or reload may change
  UIDs; callers must rediscover targets. Persistence proof compares type/count/location and actor
  identity, rather than requiring unchanged UIDs. The adapter creates no second item identity.

Historical acceptance (full attempt history remains in the prior Git version):

Implementation status:

<!-- DE67:DELIVERY-STATUS:BEGIN claim=R-SURFACE-006 -->
- [x] R-SURFACE-006 — The live Tiles and curses routes prove the renderer-neutral inventory-selector family, stable UID targeting, nested ownership, exact receipts, and fail-closed invalid identities.
  - DFS slices: `R-SURFACE-006-S001`
  - Final proof: All four closure gaps are independently closed. Current-source tests prove exact UID behavior, collated same-purpose entries, disabled actions, wrong identity rejection, nested restoration, and successor receipts. The curses and Tiles routes canonically ingest with mechanically valid witness evidence and accepted cleanup. Durable acceptance 1 settles the whole claim.
  - Durable acceptance: #1 via `R-SURFACE-006-closure-009`; SQLite evidence is authoritative.
<!-- DE67:DELIVERY-STATUS:END -->
<!-- DE67:DFS-SLICE:END id=R-SURFACE-006-S001 claim=R-SURFACE-006 -->

### 7. Dialogue semantic surface

<!-- DE67:DFS-SLICE:BEGIN id=R-SURFACE-007-S001 claim=R-SURFACE-007 -->

Mechanism:

- Files and symbols: `src/dialogue.h :: talk_response` stores a runtime `semantic_stable_id` assigned
  when the response is created; `src/npctalk.cpp :: dialogue::opt` and response generation publish
  and resolve that response-owned identity.
- Entry point: the dialogue response loop becomes top semantic owner after responses are generated.
- Inputs: speaker stable identity, speaker display name, recent native dialogue history, current
  prompt/challenge, response text, enabled/condition state, trial information already visible to the
  player, and valid dialogue actions.
- Preconditions: the response stable ID belongs to the current dialogue frame and its condition is
  still true at consumption.
- Transition: `dialogue.choose` resolves the stable ID to the same `talk_response`, rechecks native
  conditions, then invokes the existing trial/effect/topic transition. `dialogue.cancel` exists only
  when the native dialogue owner permits it.
- Postconditions: the receipt names the consuming dialogue frame; the next topic, nested prompt, or
  parent surface has a fresh frame.
- Failure behavior: regenerated, unavailable, disabled, stale, or wrong-speaker responses are
  rejected. Hotkeys and response indexes cannot identify a response.
- Persistence/compatibility: response IDs are stable for the response object's life and need not
  enter save data. NPC identity and dialogue effects remain owned by existing game state.

Historical acceptance (full attempt history remains in the prior Git version):

Implementation status:

<!-- DE67:DELIVERY-STATUS:BEGIN claim=R-SURFACE-007 -->
- [x] R-SURFACE-007 — The focused dialogue-response surface is proven on the accepted native ownership and stable-target foundations.
  - DFS slices: `R-SURFACE-007-S001`
  - Final proof: Source controls, the Tiles route, and the curses route are independently closed. Durable acceptance 1 uses closure-003 after all three sequence-2 gaps closed.
  - Durable acceptance: #1 via `R-SURFACE-007-closure-003`; SQLite evidence is authoritative.
<!-- DE67:DELIVERY-STATUS:END -->
<!-- DE67:DFS-SLICE:END id=R-SURFACE-007-S001 claim=R-SURFACE-007 -->

### 8. Direction and targeting semantic surfaces

<!-- DE67:DFS-SLICE:BEGIN id=R-SURFACE-008-S001 claim=R-SURFACE-008 -->

Mechanism:

- Files and symbols: `src/action.cpp :: choose_direction` and
  `src/ranged.cpp :: target_ui::run`, `init_window_and_input` with focused scopes.
- Entry point: each custom loop pushes a child scope before accepting its first input.
- Inputs: direction choices, allowed vertical choices, origin/cursor/destination coordinates,
  targeting mode, range, trajectory facts visible to the player, candidate entities, and currently
  valid native actions.
- Preconditions: the requested direction, coordinate, or candidate stable ID is advertised by the
  exact current frame. Character targets may bind their existing character ID. Monster and other
  candidates receive an opaque target-scope token that resolves back to the same tracked `Creature`
  while it remains a valid native candidate; the token never exposes or serializes the current
  process address. Coordinate targets use a frame-scoped coordinate token.
- Transition: `direction.choose`, `direction.cancel`, `target.move_cursor`, `target.choose`,
  `target.select_candidate`, and `target.cancel` invoke the existing loop branches.
- Postconditions: a changed cursor/candidate or the nested/parent surface is published in a fresh
  frame.
- Failure behavior: out-of-range, hidden, moved, stale, or invalid candidates are rejected without
  retargeting. The direction loop's `DEFAULTMODE` category never grants world ownership or actions.
- Persistence/compatibility: these are runtime surfaces. Existing action consequences remain owned
  by their callers.

Historical acceptance (full attempt history remains in the prior Git version):

Implementation status:

<!-- DE67:DELIVERY-STATUS:BEGIN claim=R-SURFACE-008 -->
- [x] R-SURFACE-008 — Direction and targeting screens expose and consume their own stable native choices.
  - DFS slices: `R-SURFACE-008-S001`
  - Final proof: Source controls, both Tiles routes, and both curses routes are independently closed. Durable acceptance 1 uses closure-010 after all three sequence-2 gaps closed.
  - Durable acceptance: #1 via `R-SURFACE-008-closure-010`; SQLite evidence is authoritative.
<!-- DE67:DELIVERY-STATUS:END -->
<!-- DE67:DFS-SLICE:END id=R-SURFACE-008-S001 claim=R-SURFACE-008 -->

### 9. Broad input-owner coverage and hard stop

<!-- DE67:DFS-SLICE:BEGIN id=R-SURFACE-009-S001 claim=R-SURFACE-009 -->

Mechanism:

- Files and symbols: use shared `uilist`, `query_popup`, and string-input instrumentation for the
  ordinary family; add focused adapters to custom owners in `src/debug_menu.cpp`,
  `src/editmap.cpp`, overmap editors, and every other discovered input loop whose absence would
  leave the caller blind; maintain an executable input-owner coverage test that enumerates native
  `handle_input`, `get_input_event`, and custom loop entry points against supported or explicitly
  unsupported scope registration.
- Entry point: any input owner reached while cockpit automation is active must be classified before
  it can consume an automated action.
- Inputs: the actual source-level owner inventory and live owner transitions, not screenshots or an
  arbitrary menu list.
- Preconditions: ordinary controls use the shared adapter; custom interfaces use focused adapters
  only when their state/action model cannot be represented by the ordinary family. Every discovered
  input owner whose absence would leave the agent blind is required coverage, not an optional
  unsupported classification.
- Transition: a required discovered owner publishes its supported semantic surface. An input owner
  that has not yet reached required coverage publishes `unsupported` with breadcrumbs, diagnostics
  identifying the owner, and no valid actions while its adapter remains incomplete.
- Postconditions: every required discovered owner has sufficient semantic actions to navigate,
  complete, or yield from its reachable states. Any newly discovered or still incomplete owner stops
  automated play instead of inheriting executable behavior.
- Failure behavior: absence of an adapter never falls through to parent actions, raw input, OCR, or
  guessed cancellation.
- Persistence/compatibility: coverage is renderer neutral and includes useful debug and map-editor
  routes under their existing native authorization.

Historical acceptance (full attempt history remains in the prior Git version):

Implementation status:

<!-- DE67:DELIVERY-STATUS:BEGIN claim=R-SURFACE-009 -->
- [x] R-SURFACE-009 — Every required discovered input owner operates semantically, and every new or incomplete owner stops automation without fallback.
  - DFS slices: `R-SURFACE-009-S001`
  - Final proof: Four independent revision-1 gaps are closed. The source gate classifies all 73 discovered direct-input sources. Fresh source-bound Tiles debug-spell and map-editor routes prove stable native actions, exact receipts, restoration, and cleanup. The curses map-editor route proves renderer parity. Fresh ingested unsupported run `20260901_203737_669628fd73db436ca5d51d9e6f09509d` proves stable DEBUG_CONSOLE ownership, zero actions, zero submitted requests, no parent fallback, and accepted cleanup.
  - Durable acceptance: #1 via `R-SURFACE-009-closure-004`; SQLite evidence is authoritative.
<!-- DE67:DELIVERY-STATUS:END -->
<!-- DE67:DFS-SLICE:END id=R-SURFACE-009-S001 claim=R-SURFACE-009 -->

### 10. Cockpit active-surface projection and end-to-end proof

<!-- DE67:DFS-SLICE:BEGIN id=R-SURFACE-010-S001 claim=R-SURFACE-010 -->

Mechanism:

- Files and symbols: `tools/openclaw_harness/semantic_state.py`,
  `semantic_broker.py`, `cockpit.py :: CockpitRunChannel/CockpitService`, the terminal cockpit
  projection, and their focused tests.
- Entry point: `game.observe` reads one native top-surface descriptor. `game.act` accepts only the
  exact descriptor's action schema.
- Inputs: semantic surface descriptor and receipt from CDDA. The cockpit does not merge cached
  parent facts or actions into a child.
- Preconditions: descriptor schema/run/frame identity is valid and newer than the last consumed
  transition. A public action is one of the current valid actions.
- Transition: observing replaces the active view with a surface-specific World, Overmap,
  Inventory, Dialogue, Menu/Prompt, Direction, Target, or Unsupported projection. Breadcrumbs render
  the complete surface stack. Acting uses the native semantic request route.
- Postconditions: graphical and terminal cockpit projections show the same structured facts,
  actions, frame identity, breadcrumbs, and receipt result. Unsupported surfaces show no executable
  action and stop the automated route.
- Failure behavior: malformed, missing, stale, or out-of-order native frames and receipts fail
  closed. Legacy OCR/log/key routes may remain diagnostic but cannot supply executable cockpit
  actions or acceptance evidence.
- Persistence/compatibility: public schema changes are versioned. Existing `llm_intent` behavior is
  regression-tested unchanged. The semantic protocol remains presentation independent.

Historical acceptance (full attempt history remains in the prior Git version):

Implementation status:

<!-- DE67:DELIVERY-STATUS:BEGIN claim=R-SURFACE-010 -->
- [x] R-SURFACE-010 — The cockpit replaces its active presentation from the exact top descriptor across World, Overmap, Inventory, Dialogue, Menu/Prompt, Direction, Target, and Unsupported surfaces.
  - DFS slices: `R-SURFACE-010-S001`
  - Final outcome: Current source and focused tests enforce exact descriptor-only projection. Fresh source-bound Tiles, Overmap, Dialogue, curses, and Unsupported witnesses prove stable identities, isolated actions, exact receipts, complete breadcrumbs, restored ownership, renderer parity, and hard-stop behavior.
  - Durable acceptance: #1 via `R-SURFACE-010-closure-005`; SQLite evidence is authoritative.
<!-- DE67:DELIVERY-STATUS:END -->
<!-- DE67:DFS-SLICE:END id=R-SURFACE-010-S001 claim=R-SURFACE-010 -->

### 11. Current owner coverage after qualification

<!-- DE67:DFS-SLICE:BEGIN id=R-SURFACE-011-S001 claim=R-SURFACE-011 -->

`input_owner_coverage_test.py` scans direct native input call sites by source file. On this baseline,
`test_every_direct_native_input_source_is_classified` fails for `src/npc_inspection.cpp`; the other
73 tests in the combined coverage, semantic-state and semantic-step invocation pass. The inspector
already has actor-bound native scopes (`show_npc_inspection` and `show_item`) and focused native tests.
This is a source-inventory omission, not evidence that NPC inspection is absent or that NPC gameplay
is wrong. Classification must follow the actual loops, not merely add a filename to silence failure.

The qualified post-death route also names `MESSAGE_LOG` as unsupported. The input-context boundary
must keep unknown owners actionless while focused support is incomplete. Classification alone does
not satisfy the earlier broad coverage contract: discovered input owners that leave the agent blind
need native state and complete mode-valid actions, including useful custom/debug/editor routes.
The current inventory contains mixed files such as `action.cpp`, `npctalk.cpp` and `ranged.cpp`;
file membership alone cannot prove every loop supported or safely stopped.

Implementation status:

<!-- DE67:DELIVERY-STATUS:BEGIN claim=R-SURFACE-011 -->
- [x] R-SURFACE-011 — Current native input-owner coverage is reconciled and the repaired inspector, Dialogue, and MESSAGE_LOG routes are independently validated.
  - DFS slices: `R-SURFACE-011-S001`
  - dialogue-fail-closed: Receipt a656476d9f7be158e2a63286dc10f87bcbd9e24ed1ddda1007b4afbbf8bca59f independently confirms no native dispatch for the unadvertised response, retained Dialogue ownership, accepted native receipt for the advertised response, and World restoration with matching current relevant source hashes.
  - inventory-reconciliation: Receipt a2f39142a98fd284e75f0e4a69bf35d13c6a6608add70fb3e619a5be3806accb independently confirms exact 74-source classification equality and 78 passing owner-coverage, semantic-state and semantic-step tests. It limits the conclusion to current input-owner inventory reconciliation.
  - message-log-parity: Receipt 4bad63572c8e1a1fb5623148315ece5b0c5c480f98d449de747c2df88a48b1df independently confirms current source-bound Tiles and curses viewer entry, controls, filter prompt cancellation back to MESSAGE_LOG, close to World, and explicit cleanup with passing focused suites.
  - npc-inspection-parity: Receipt 55e3dd74b64cbb6b72699b9aac6c490c705b6e059ca89763bfbf8a52ccf711d4 binds fresh current source-bound Tiles and curses runs proving inspector entry, stale item rejection, valid item child, child and parent restoration, and explicit cleanup.
  - Evidence ceiling: input-owner inventory and native semantic operation only; no CAOL gameplay or message-content claim. Earlier implementation and diagnostic receipts remain durable at their original scope.
  - Durable acceptance: #1 via `R-SURFACE-011-closure-004`; SQLite evidence is authoritative.
<!-- DE67:DELIVERY-STATUS:END -->
<!-- DE67:DFS-SLICE:END id=R-SURFACE-011-S001 claim=R-SURFACE-011 -->

### 12. Fresh package and retained acceptance

<!-- DE67:DFS-SLICE:BEGIN id=R-026-S001 claim=R-026 -->

The existing established-base package is the starting feature overview. Reuse
`tools/openclaw_harness/fixtures/saves/live-debug/bandit_basecamp_prepared_base_v1_2026-04-22/manifest.json`
and its profile only while their current source/fixture audit remains true. Its assignment transform
for actors 2 and 3 is setup. A prepared camp cannot prove establishment. Controlled derivatives and
a separate native establishment route may supply the missing preconditions without replacing the
historical package. Combined living-base/hostile workloads remain required where their interaction
is the question; independent branches need not be forced into one uninterrupted walkthrough.

`r026.living_npc_package_v001_mcw.json` and `r026.camp_zone_manager_v001_mcw.json` are existing route
seeds. The qualified `harness.living_camp_freeplay_mcw.json` is a useful exploratory view/interaction
seed but explicitly has `grants_gameplay_proof: false`. Its old witness must never be relabeled as
new product proof. New claim-bound scenarios/charters must bind the actual source, executable,
world/profile and proof question, retain all transforms and allow players to investigate outcomes.
Changing a declared evidence ceiling is not itself causal evidence.

The reusable package guide must name available scenes, meaningful player questions, current binding
and launch prerequisites, supported observation/action routes, preparation limits, independent
verdicts and evidence handles. It must preserve earlier scenarios and reports instead of replacing
them. Current qualification documentation is implementation/use evidence, not a second product ledger.

Implementation status:

- [ ] 🔴 R-026 — No current-source integrated CAOL feature package yet binds the living-base,
  bandit, cannibal, signal-control, and flesh-raptor families through one audited established-base
  footing with independent mechanical, causality, feel, persistence, and cleanup evidence plus a
  usable package guide.
  - DFS slices: `R-026-S001`
  - Code/proof gap: the existing package overview, scenario seeds and harness qualification do not
    contain the requested comprehensive fresh campaign. Earlier acceptance is retained below.
  - Required mechanism: bind fresh independent tests for R-029 and R-031 through R-036 to the current
    native harness, retaining ordinary CDDA dependencies, source/world identities, truthful
    unsupported stops and per-claim verdicts. These IDs decompose proof, not gameplay repair tasks.
  - Proof: each in-scope behavior has an independently evidenced result and all required outcomes
    pass before package closure. Contradicted or unobserved outcomes remain visible; an owner-pending
    blocker is not a pass. The guide and preserved witnesses permit another player to understand
    what was tested, reproduce its footing and investigate the same questions.
<!-- DE67:DFS-SLICE:END id=R-026-S001 claim=R-026 -->

<!-- DE67:DFS-SLICE:BEGIN id=R-027-S001 claim=R-027 -->
Current obligation: R-033 requires fresh observation/control/world-boundary evidence and R-029
    requires the full natural signal/scout/report/response route. Historical green does not close them.

Implementation status:

<!-- DE67:DELIVERY-STATUS:BEGIN claim=R-027 -->
- [x] R-027 — Independently validate the current staffed-camp structural-signal observation and
  memory path from a saved physical source through bounded controls and an ordinary response.
  - DFS slices: `R-027-S001`
  - Historical acceptance: #1 via `R-027-closure-008`. Native runs established smoke/light leads,
    absent/blocked/range controls, deduplication, aging and changed-source refresh. Run
    `3fd920fc…0a100` selected a retained light lead and dispatched actors 4 and 18; the separate
    `bandit_live_world_retained_signal_dispatch_survives_save_round_trip` test passed 23 assertions.
    Combined report SHA `17058a17…328dfb` keeps live and deterministic persistence evidence separate.
    Exact earlier artifacts and acceptance history remain unchanged in the prior DFS/package.
  - Durable acceptance: #1 via `R-027-closure-008`; SQLite evidence is authoritative.
<!-- DE67:DELIVERY-STATUS:END -->
<!-- DE67:DFS-SLICE:END id=R-027-S001 claim=R-027 -->

<!-- DE67:DFS-SLICE:BEGIN id=R-028-S001 claim=R-028 -->
Current obligation: R-036 requires fresh integrated comparison with the qualified harness.

Implementation status:

<!-- DE67:DELIVERY-STATUS:BEGIN claim=R-028 -->
- [x] R-028 — Qualify the current package's performance under paired combined workloads including
  rendered local transitions, mechanical context, and gameplay feel.
  - DFS slices: `R-028-S001`
  - Historical acceptance: #1 via `R-028-closure-008`. Baseline report
    `20260903_183103_938dff78278a4cd8926c2ba74a1f856b` and feature report
    `20260903_183420_7782a0e532dd43ce9d449bb06eb0a404` bound equivalent prepared state and one rebuilt
    Tiles executable, rendered transitions, native save/quit and original-process exits. Earlier
    paired raw distributions and native counters remain evidence. The conclusion was only that no
    material difference was observed on that route; it did not establish a universal threshold.
  - Durable acceptance: #1 via `R-028-closure-008`; SQLite evidence is authoritative.
<!-- DE67:DELIVERY-STATUS:END -->
<!-- DE67:DFS-SLICE:END id=R-028-S001 claim=R-028 -->

### 13. Coherent bandit and cannibal discovery and contact

<!-- DE67:DFS-SLICE:BEGIN id=R-029-S001 claim=R-029 -->

`src/do_turn.cpp :: overmap_npc_move` consumes significant sounds, gathers physical fields on the
five-minute cadence, refreshes staffed readiness on its existing cadence, calls
`record_staffed_camp_signal_observations`, then ordinary structural maintenance and hostile
rally/approach/return. A newly bootstrapped candidate is zero-credit registration and defers that
maintenance pass. `dispatch_live_cannibal_signal_contacts` is now an unused definition; no live
call remains. Do not restore a second cannibal distance response path.

`src/bandit_live_world.cpp :: record_staffed_camp_signal_observations` requires an eligible idle
staffed camp and ready at-home observer, validates physical reads, and normalizes durable
source/channel `camp-signal:` leads. `advance_structural_bounty_maintenance` owns scout assignment,
investigation, returned report assessment and `plan_hostile_operation_with_authorized_response` /
`apply_hostile_operation_plan_with_authorized_response`. Bandit/cannibal policy diverges at the
report assessment, not by bypassing physical observation or scout travel.

There are separate writers with different provenance: `observe_live_bandit_player_target_opportunity`
/ `adopt_observed_hostile_player_opportunities` can create a direct loaded player-at-basecamp
`player@...` terrain-opportunity lead, whereas `record_live_bandit_structural_sounds` records sound
evidence for an already active structural scout outing. The latter is active-scout investigation,
not a second idle-camp discovery mechanism. Neither substitutes for the required idle staffed
physical-signal observation -> camp memory boundary. Trace exact source/lead/report ancestry;
a preloaded `returned_report` or separately adopted player opportunity cannot prove that boundary.
Whether a naturally observed alternate discovery route violates the intended shared-route behavior
is an explicit gameplay uncertainty, not permission to remove code. Use the first divergent
identity/transition to distinguish provenance error from absent behavior.

`site_record` owns `intelligence_map`, `current_scout_report`, `camp_decision`, `acted_reports`,
`active_outing` and `active_hostile_operation`. Report identity includes revision, source generation,
source activity and application key; decision pins report and target lead/revision. The hostile
operation persists reservation/members/generation, kind, phase, report identity, payment branch and
rally. Load rejects incomplete current schemas and normalizes/deduplicates records; legacy nonlost
hostile outings migrate to safe homeward ownership. `claim_hostile_target_opportunity` accepts an
exact duplicate as `already_applied` and rejects changed identity/revision. Terminal aftermath is
applied once after return; exact duplicate receipt is read-only and mismatched replay is stale.
These boundaries forbid double dispatch, duplicate payment or outcome application on reload.

`advance_live_bandit_hostile_approaches` gates only `rallying -> approaching` by night for a raid.
Once departed, the persisted operation and physical route remain authoritative through dawn;
contact has no second night gate. Abstract/local handoffs must retain site, generation, operation,
member IDs and route progress. Generic travel may not advance a concurrently locally owned actor.

`live_bandit_handle_hostile_shakedown_contact` and `choose_local_gate_posture` distinguish normal
shakedown from favorable rolling-travel attack. The forced native payment UI remains.
`live_bandit_commit_paid_return` commits `committed_contact -> returning_home` with branch `paid`;
`live_bandit_choose_fight` records combat release for explicit refusal/incomplete payment;
technical preparation failure must remain distinct from player refusal. Player attack
calls `release_shakedown_combat_on_player_attack` for an exact parley member.
`hostile_operation_player_relationship_for` validates active hostile reservation, toll/shakedown
kind, member identity and non-dead/non-missing membership. It returns parley at committed contact,
combat release on Fight, and paid departure while returning home, including abstract return.
`npc::guaranteed_hostile` and `npc::attitude_to` consult this before faction dislike; the generic
NPC cache/player targeting and `NPCATT_KILL` paths must yield to it. This protects only this group's
relationship to the player and leaves unrelated hostiles intact.

The strongest counterexample is the first normal contact turn: generic NPC movement occurs before
some aftermath processing. A demand screenshot or a `paid` write alone cannot show protection on
that turn or the next. Bind actor HP/attitude/target/offensive events before demand, through native
trade, later ordinary turns, travel ownership changes and save/reload. One member attacking early
or resuming aggression after accepted payment contradicts that branch even if dialogue also succeeds.

Implementation status:

- [ ] 🔴 R-029 — Bandit and cannibal camps do not yet have a proved coherent natural
  signal-to-response route with correct night-raid commitment and operation-scoped shakedown,
  combat, and paid-departure ordering.
  - DFS slices: `R-029-S001`
  - Present mechanism and history: shared discovery, retired shortcut and relationship overrides
    exist. Earlier focused demand/Fight/unrelated-hostile and paid-return-through-reload evidence
    remains at its original ceiling. Old launch/wait continuation supplied no natural ecology credit.
  - Required proof: fresh source-bound player-created signal -> exact staffed observer -> camp lead
    -> physical scout investigation -> returned report -> profile response -> rally -> response
    travel -> contact. Keep bandit and cannibal identities continuous and verdicts independent.
  - Bandit outcomes: demand precedes every offensive action in normal contact; completed payment
    permits safe later turns, return and new-process reload; refusal, incomplete payment and player
    attack independently release combat. A favorable rolling-travel scene remains a direct ambush.
    Payment must transfer native value, not merely select Pay; returning safely is distinct from
    recording payment. Return completion must not reapply the old encounter on subsequent turns.
  - Cannibal outcomes: observe daylight rally hold, night departure, dawn before contact, continued
    physical approach and attack. Waiting until night contact alone leaves the dawn edge unobserved.
  - Controls: no-signal and unavailable/blocked staffed observation do not produce this signal route;
    unrelated actors retain their ordinary relationships; save/reload preserves exact active owner
    and does not replay terminal aftermath. Report causal changes separately when controls affect
    visibility, danger, timing or actor readiness.
  - False greens: `bandit.extortion_first_demand_*` raw local-contact fixtures prove only downstream
    contact branches; `bandit.local_scout_return_preaged_mcw` pre-ages sortie time. The cannibal
    night-local-contact fixture supplies a returned-report lead. None proves natural discovery or
    scouting. Setup-created signals, reports, actors, contact, darkness or deadlines earn no credit
    for the transitions they manufacture. Preserve their legitimate downstream route evidence.
- Owner-authorized repair boundary, 2026-09-07: R029-F003's accepted same-minute Pay
    must reach payment preparation/trade without being converted into player refusal by a
    technical failure. `live_bandit_prepare_paid_return` currently rejects current minute equal
    to the simulation cursor, and its caller falls through to Fight. Separate preparation and
    transaction failure from explicit refusal; preserve native value transfer and idempotent
    paid-return commit. Eligible first sight of the player OR a follower must offer negotiation
    before attack. A follower contributing goods does not prove follower-triggered contact.
    Successful payment clears this group's hostility and initiates retreat; stale combat,
    stalking, hold or strategic travel must not restore aggression or immediately demand again.
    Test both target types at same-minute contact, Pay, explicit Fight/refusal, later ordinary
    turns and reload. Preserve unrelated hostile actors and favorable rolling ambush exclusions.
  - Activation and arbitration investigation: R029-F004 proves relocation without local
    activation. `materialize_committed_bandit_shakedown` uses `Creature::setpos`; compare
    call order/outcomes with `game::load_npcs` and creature-tracker registration before a
    targeted fix. The intended actor retains one identity and one advancing simulation owner.
    Local hostile combat owns tactical movement/attacks while strategic bookkeeping retains
    continuity; stalking is an explicit single-owner exception. Inspect camp-hold, homeward
    and alternate-watch transitions individually. Negotiation and paid retreat outrank stale
    orders. Prove activation separately, then enter/contact/combat-or-stalk/disengage/leave/reenter;
    ambiguous precedence requires owner judgment, not globally disabling overmap AI.
  - Sound diagnosis correction: run `4b7c5abb…` preserved a lead last seen 8220 and last checked
    8225. At attempt 8280 the latter is inside `structural_lead_recently_checked`'s six-hour
    cooldown; `cheap_structural_outing_candidates` runs before the reported drive 347 check.
    No eligible plan means even sufficient drive cannot dispatch. The sensing writer also sets
    last_checked to observation time, with sound expiry preceding cooldown release; distinguish
    sensing from physical investigation as a candidate for owner decision. The full drive vector remains
    unavailable. Detection proof stands; no threshold change or sound-compels-scouting contract
    is authorized. Investigate remaining eligibility using the exact state and a discriminating
    control, with fixture changes counted as setup. `.de67/state/review-owner-f918d28be953/`
    retains the source predicate reproduction and exact artifact paths.
<!-- DE67:DFS-SLICE:END id=R-029-S001 claim=R-029 -->

### 14. Lossless CAOL evidence transport

<!-- DE67:DFS-SLICE:BEGIN id=R-030-S001 claim=R-030 -->
For this package, `cockpit_file_bridge.py` retains compact response status, exact `response-slice`,
full digest-verified `response-artifact`, scoped `log-query` and offset/length/hash `record-artifact`.
`PlayerClient.controls` discovers launch-published native/transition logs and shared NPC diagnostics.
Missing metadata/files mean unavailable evidence, not absent gameplay. Shared logs require explicit
run/time/actor/request correlation. `refresh_semantic_step_trace` streams retained history into its
existing recent-event window; the full disk record stays intact. Counts and page sizes are
presentation details, not proof limits or permission to discard history.

The existing witness bundle calls its defect array `findings`; that schema label is not evidence
of Josef's promotion decision. A suspected gameplay defect records expected versus observed behavior,
exact evidence, affected tests and any blocking consequence. Retain the original observation and
any later correction/decision separately, with the authorization boundary supplied by the WEC.

Witnesses and later causal judgments are separate append-only facts in `playtest_witness.py` and
`scenario_registry_store.py`. A bundle keeps each claim's verdict and identifies affected and
unaffected claims. Citations must resolve to exact typed native facts; the witness author's chronology
or stop reason is not independent evidence. Qualification's corrected fire, clothing and item-location
interpretations demonstrate why mechanical validation alone cannot establish causality.

Implementation status:

<!-- DE67:DELIVERY-STATUS:BEGIN claim=R-030 -->
- [x] R-030 — Make CAOL and DE67 worker evidence quiet by default without losing any full-fidelity
  artifact: compact command receipts, indexed journal lookup, outcome-sized progressive briefs, and
  durable successor continuation must preserve exact identity and binding while the full digest-bound
  payload remains explicitly retrievable.
  - DFS slices: `R-030-S001`
  - Historical acceptance: #1 via `R-030-closure-008`, receipt `0e2afae0…a96625`. The original broader
    acceptance and full attempt history remain in the prior Git version and durable evidence.
    Coordination-side mechanics are outside this product DFS.
  - Durable acceptance: #1 via `R-030-closure-008`; SQLite evidence is authoritative.
<!-- DE67:DELIVERY-STATUS:END -->
<!-- DE67:DFS-SLICE:END id=R-030-S001 claim=R-030 -->

### 15. Fresh living NPC intent, context and routing

<!-- DE67:DFS-SLICE:BEGIN id=R-031-S001 claim=R-031 -->

`llm_intent.cpp :: build_snapshot_json` gathers visible map/creatures, follower and remembered context;
`enqueue_*` owns request submission. `npcmove.cpp :: execute_llm_intent_action` resolves valid native
behavior; follow-close/far updates rules, and native movement/arrival processing advances the target.
`npctalk.cpp` and `npctalk_funcs.cpp` choose hearers/camp workers using actor and `assigned_camp` state.
`data/llm_prompts/` templates and `tools/llm_runner/` are NPC product inputs, not repository-agent policy.
Their current configuration and actual runner response identity belong in the evidence.

Native danger/combat, companion mission and Patrol ownership can defer or supersede ordinary LLM
movement; one once-per-turn action must not compete with another owner moving the same actor.
`npc::llm_intent_state_map` is process-local and keyed by `character_id`; destruction erases it.
Save/load preserves NPC rules, mission and assignment, not the in-flight queue or transient target.
Do not invent durable LLM-memory semantics. An observed contradiction in intended follow/camp behavior
is distinct from a correctly nonpersistent queue.

Implementation status:

- [ ] 🔴 R-031 — Living NPC intent/context, follow/stay travel and camp routing lack fresh package proof.
  - DFS slices: `R-031-S001`
  - Code/proof gap: unit parsing/action tests and qualification's named replies/orders establish
    narrower routes; neither proves the complete current-source living-base questions.
  - Required mechanism: use current World chat, Dialogue, rules and actor-inspection owners with the
    actual configured NPC runner; retain request/recipient/snapshot/reply/action and physical outcome.
  - Proof questions: does a useful free-text instruction reach the intended NPC with relevant local
    and camp context, produce an actual reply or explicit runner error, and lead to the requested
    native action? Does ordinary follow travel occur after player separation, and does stay/guard
    hold the chosen post? Do ambient and camp requests use the right recipient/context and respect
    competing danger/mission/Patrol ownership? Judge these independently and retain confusing or
    delayed outcomes as feel evidence. Use R-034 for durable continuity.
  - Controls: prewarm is not conversation; a spoken promise/order label is not movement; canned
    dialogue is not an LLM response; player proximity alone does not prove follow; a camp selector
    opening is not mission execution. Bind exact actor, utterance, time and actual consequence.
- Owner-authorized speech/craft correction: an incapable or unassigned follower must not
    falsely promise native camp crafting. Correlate actor/request/source before attributing
    the reported reply. `npctalk.cpp` filters direct address, selects ambient recipients, groups
    assigned camp listeners and invokes `handle_heard_camp_request`, then enqueues remaining
    LLM hearers. `uses_basecamp_request_routing` excludes walking followers; camp handling
    resolves a capable worker and can create a resource-blocked request. Preserve valid direct
    follower commands; a blanket camp-listener veto is not the required behavior. The capable,
    assigned recipient handles the request or gives an honest blocked result.
    Test mixed camp/follower listeners, addressed versus broadcast speech, assigned/capable
    versus unassigned actors, and available versus missing recipe resources. Spawn only required
    bandage materials through authorized fixture/debug setup after checking actor roles, recipe,
    resource ownership/location and source binding; setup has zero proof credit. Bind native
    craft request/result to the reply, keeping persistence under R-034 separate.
  - Applied camp-craft repair: receipt
    `34681646df11e45a133cb3eee507287f23719021b376dec2f2142bc6aee3189e` proves the
    eligible resident route, silence from an explicitly addressed unassigned follower, correct
    mixed-listener handling, and three follower-owned bandages returned through the advertised
    semantic camp action in fresh run `55098ec37b3868099fa04fdb5626a57a3f043ac3d30b791a44af2c811d53c33b`.
    Authorized ingredient setup has zero credit, and the earlier direct-key return is excluded.
    Free-text follow, relationship snapshots, competing ownership and R-034 continuity remain open.
<!-- DE67:DFS-SLICE:END id=R-031-S001 claim=R-031 -->

### 16. Fresh camp establishment, missions and zones

<!-- DE67:DFS-SLICE:BEGIN id=R-032-S001 claim=R-032 -->

`talk_function::start_camp` checks native site eligibility, nearby camps and blueprint collision,
then establishes through `get_basecamp`. `talk_function::basecamp_mission` resolves assigned/nearby
camp, access and bulletin board, rebuilds storage and lists native missions. `basecamp::start_mission`
checks food before assignment, records companion mission/return time/exertion, consumes food through
`camp_food_supply`/faction stock, consumes selected equipment from `src_set`, and saves camp map.
Mission return must resolve the same worker and real result; UI acceptance alone is insufficient.

`form_storage_zones` derives `src_set` from faction-scoped enabled `CAMP_STORAGE` zones. Food-zone
contents and faction food stock are different facts; loading a food zone is not proof that stock
or feeding changed. `locker_policy_ui` writes durable camp policy. `process_camp_locker_downtime`
queues eligible assigned workers; `service_camp_locker`/`service_camp_locker_impl` select native
zone candidates and execute equipment/ammo/medical readiness service. Queue, reservations, derived
inventory/cache and service timing are not a second persistent item store.

`refresh_patrol_shift_cache` validates assigned workers and `ACT_CAMP_PATROL` priority, zone geometry,
shift and alarm before choosing a plan. `get_current_patrol_runtime` synchronizes orders and retries
after invalidation; `npc::set_camp_patrol_order`/`clear_camp_patrol_order` and native movement consume
it. Mission/assignment/patrol-order state is saved, whereas shift caches/excluded-worker sets are
rebuilt. Zone revisions reject stale UI mutation. Reassignment, removal/disable of a zone, or danger
must invalidate the affected plan and yield to the correct native owner without duplicated service.

Implementation status:

<!-- DE67:DELIVERY-STATUS:BEGIN claim=R-032 -->
- [x] R-032 — Fresh camp establishment, mission completion, Food, Locker, Storage, enabled Patrol, priority-zero release, and repaired-release persistence are proved at their stated ceilings.
  - DFS slices: `R-032-S001`
  - Assignment R-032-patrol-release-repair-001: Repair promoted R032-F001 so priority zero invalidates cached membership and releases the current patrol assignment/order on the next ordinary AI update. Prove enable/patrol/disable/reassignment and relevant persistence, preserving unrelated missions and accepted enabled-Patrol proof. Source entrypoints: basecamp.cpp camp_patrol_cached_roster_is_eligible, refresh_patrol_shift_cache, sync_camp_patrol_worker_order, and npcmove.cpp patrol runtime.
  - Assignment R-032-fresh-camp-package-001: Freshly prove native establishment, actual mission completion, Food, Storage, Locker and Patrol behavior as independent results. Repair repository-owned harness, fixture, scenario, registry or observation paths when needed. Finish only when each behavior has source-bound native evidence or a named assigned-outcome exit; preparation and opened selectors remain zero-credit.
  - Applied repair: Receipt `508f2d1a263a86dc61561a26e4634ceed303338e428b4de2f2539cd857d9e5e7` proves the owner-promoted Patrol release. Priority 9 produced an active `GUARD_PATROL` order and runtime. Priority 0 followed by one ordinary update returned Katharina to `CAMP_RESIDENT`, cleared the Patrol order and guard post, and removed the active runtime. The focused test passed 18 assertions and the full test build succeeded. Five failed, diagnostic, replacement, and final runs are reconciled by exact PID/birth/broker identity. OCR and rendered text received no proof credit.
  - Durable closure: Receipt `b6f86843e4d69dbccc3410f72f687943d3ea840a3f63269a2a621382a9f444a1` independently verifies the camp-service results. Receipt `0921e53d3357b9e3f1fec55d8439cb9401fb5243fa77f1ef6eb140a3b7a49819` independently verifies the repair's identities, tests, evidence limits, material changes, and five-attempt cleanup record. Receipt `4a1ee0a32aac15442ec86e34b2f3581e8368528d9f721910aea827c3743f5a42` proves Katharina became the actual active Patrol owner, released at priority zero, accepted ordinary reassignment, saved, reloaded, and did not regain a stale order, guard mission, runtime, or cache. The first persistence setup remains inconclusive because Robbie retained the cache. Bridge birth identities are not available in the retained schema; exact game births, bindings, exits, and safe-cleanup dispositions remain preserved.
  - Assignment R-032-closure-camp-services-001: Independently verify the fresh establishment, assignment, Food, Survey completion, bounded Locker, enabled Patrol, and exact Storage-placement results at their separate evidence ceilings. Confirm the repaired priority-zero release does not erase these accepted sibling results.
  - Assignment R-032-closure-patrol-persistence-001: Freshly verify the repaired Patrol enable, actual runtime, priority-zero release, ordinary reassignment, and relevant save/reload behavior on current source. Transient caches need not persist, but no stale Patrol order, guard mission, or runtime may return after reload.
  - Assignment R-032-closure-evidence-lifecycle-001: Independently verify source and executable identities, the focused and full tests, zero-credit setup and presentation limits, changed files, and exact cleanup for every R-032 repair attempt and broker.
  - Current handoff: R-032 is accepted. The applied-repair and durable-closure receipts above supersede the original R032-F001 contradiction for current delivery. Preserve that original receipt and the inconclusive Robbie-owned persistence setup as evidence; no Patrol replay is required. The separate addressed-craft retest below is historical investigation, with the accepted current R-031 craft result governing any remaining need.

  - Addressed-craft retest: R026-F006/R-034 parser-to-durable-state continuation, distinct from generic R-032 establishment proof. R026-F006 run f74aa980 is removed from active bug intake and remains investigation here. Current `src/npctalk.cpp` strips the addressed prefix before `basecamp::handle_heard_camp_request`; that source change alone does not prove current gameplay. Recover the reported earlier successful craft conditions, use `r026.living_npc_package_v001_mcw.json` with current binding, and inspect exact post-prefix text, recipe/resources, request ID/status and durable camp craft state through a new process. A parsed but resource-blocked order is distinct from parser failure. `.de67/state/review-owner-d5004a80c4e8/resume-a7d18ef4/bug-evidence.md` retains the original run, current source and log evidence; no successful durable craft/reload receipt has yet been located. Coordinate continuity with R-034 rather than duplicate its acceptance work.
  - Subtasks:
    - [done] prove-fresh-camp-services :: Fresh native results cover establishment, assignment, Food, Survey, Locker, enabled Patrol, and Storage at separate ceilings.
    - [done] repair-priority-zero-release :: Current source releases the stale Patrol order and runtime on the next ordinary update.
    - [done] prove-repaired-release-persistence :: Native save and generation-1 reload preserved ordinary state without a Patrol order, guard mission, runtime, or cache resurrection.
    - [done] audit-camp-service-evidence :: Independent review preserved each accepted sibling result without broadening its evidence ceiling.
    - [done] audit-repair-evidence-and-cleanup :: Independent review verified code, tests, zero-credit limits, and every task-owned process disposition.
  - Durable acceptance: #1 via `R-032-closure-patrol-persistence-001`; SQLite evidence is authoritative.
<!-- DE67:DELIVERY-STATUS:END -->
<!-- DE67:DFS-SLICE:END id=R-032-S001 claim=R-032 -->

### 17. Fresh signal controls and world boundaries

<!-- DE67:DFS-SLICE:BEGIN id=R-033-S001 claim=R-033 -->

`observe_live_bandit_field_signals_near_player`, significant-sound consumption in `overmap_npc_move`,
`live_bandit_staffed_camp_signal_reads` and `record_staffed_camp_signal_observations` are the physical
source/observer owners. They use current source intensity, emission time/channel, range, LOS,
elevation and weather. Stable camp leads deduplicate/refresh the same source, age without fresh
support and remain distinct from player-only observation. Five-minute signal and ordinary structural
cadences are implementation facts, not invented deadlines for success. Candidate bootstrap supplies
no dispatch/contact/knowledge credit.

Local/abstract travel handoffs in `do_turn.cpp` and `bandit_live_world.cpp` bind member IDs, generation,
route/waypoint and simulation owner. Loaded NPC movement yields at handoff; abstract travel cannot
move locally owned members again. Reentry must materialize the same admitted group rather than clone
it or substitute another site's record. Use absolute map-square/OMT coordinates with explicit units.

Implementation status:

- [ ] 🔴 R-033 — Physical signal controls, camp memory and local/overmap boundary behavior need fresh evidence.
  - DFS slices: `R-033-S001`
  - Code/proof gap: R-027 remains accepted historically; qualification observed distinct candidate
    and no-signal records without proving an identity-continuous natural response.
  - Required mechanism: use ordinary light/fire/smoke and significant-sound actions, their actual
    production observer reads and independently identified camp leads. Test both signal source and
    sensing boundary before attributing subsequent R-029 response.
  - Proof: each relevant light/smoke/sound channel has a positive physical observation and the
    smallest discriminating no-source/blocked/out-of-range/elevation-or-world-boundary control.
    Verify unchanged-source deduplication, changed-source refresh and aging of unsupported memory.
    An unavailable staffed observer must not be credited with discovery. Cross the relevant loaded
    world/overmap boundary and show consistent source/lead/member/owner identity and route progress,
    without duplicated actors or simultaneous local/abstract advancement. Preserve per-channel and
    per-boundary results, including channels a mixed fire cannot distinguish on its own.
  - False greens: debug source placement proves setup only; timestamp adjacency, unrelated site IDs,
    adopted opportunity, absent logs, source visibility to the avatar alone, or one retained lead
    cannot prove the intended observation/response. Do not manufacture a report to get unstuck.
- Owner-accepted scope limitation: smoke on the bandit camp's own OMT (R033-F001) is excluded
    from the requested repair/acceptance obligation. Its original `blocked_line_of_sight`
    observation remains valid at that ceiling. This does not waive smoke sensing on other OMTs,
    local visual perception/aggression, or any unperformed signal/memory/world-boundary test.
- Owner-promoted repair, relay4673b7453329 / review d1cfc813605b: R033-F002 compares
    fresh signal summary text with its bounded durable representation and falsely refreshes an
    unchanged lead. Authorize only this persistence/comparison correction and focused native retest.
    Reproduce the original >256-character summary mismatch, compare canonical semantic content,
    preserve the specified observation metadata and prove a meaningful source change still updates.
    Separate unsupported aging and the accepted same-OMT exception; implementation and proof remain
    open under R-033-smoke-dedup-repair-001, with source-grounded preparation in the task context.
<!-- DE67:DFS-SLICE:END id=R-033-S001 claim=R-033 -->

### 18. Fresh persistence and continuation

<!-- DE67:DFS-SLICE:BEGIN id=R-034-S001 claim=R-034 -->

`savegame_json.cpp` writes NPC identity, rules, assignment, mission, inventory, patrol flag and
monster movement state. Camp policy, zones and `overmap_global_state` preserve their authoritative
stores; `site_record`, report/decision and hostile-operation serialization preserve the ecology
identity chain described in R-029. `PlayerClient.collect` recognizes the declared saved-world
continuation in a new process/generation; old frame grants are discarded and the fixture is not
reinstalled. Native saving, original-process exit, restored state and later behavior are distinct facts.

Implementation status:

<!-- DE67:DELIVERY-STATUS:BEGIN claim=R-034 -->
- [x] R-034 — Fresh native persistence and continued behavior are proved for the tested camp, storage, actor, authority, and lifecycle boundary.
  - DFS slices: `R-034-S001`
  - Assignment R-034-fresh-new-process-continuity-001: Produce meaningful current package state through native play, preferably reusing established camp, actor assignment/rules, Food or Storage/zone state, and a safe active service without replaying the Patrol priority-zero contradiction. Record authoritative before-save state, save through native input, exit the original process, reload the exact world in a new source-bound process without fixture reinstall, reject stale prior-process grants, compare durable identities and values, and prove later ordinary behavior rebuilds derived services without double application. Repository-owned harness, fixture, scenario, registry, observation, and implementation repairs are authorized. Finish only with source-bound native evidence for the independent persistence facts or a named assigned-outcome exit.
  - Assignment R-034-owned-process-lifecycle-002: Continue the remaining native lifecycle proof after review-f918d28be953's current-generation cleanup repair. The prior lifecycle closure attempt was abandoned; restored-state closure completed independently. Old finish for afd0c247 named PID95044 while failed replacement95452 survived until the reviewer explicitly quit it. Preserve original valid d7929b persistence/restored-state proof and this precise correction; on the next useful native lifecycle route verify current/replacement process exit and brokers, without replaying unrelated camp state tests.
  - Assignment R-034-closure-native-lifecycle-001: Independently verify native save/quit, original process exit, exact-world generation-1 reload without fixture reinstall, and distinct source, run, binding, and process identities.
  - Assignment R-034-closure-native-lifecycle-002: Preserve the accepted generation-0 lifecycle proof from receipt `794e17572b780cac27f1378aab4c8b9cffa21d6ce6b790aefa5d3b9ff6e8ca9a`. Diagnose and repair or isolate the shared `fonts.json` temporary-file rename race that blocked the replacement process. Then rerun only the generation-1 exact-world startup, native semantic continuation, and full PID/birth/broker cleanup boundary. Do not replay the accepted restored-state comparison. OCR, terminal bytes, and rendered text receive no gameplay or lifecycle proof credit.
  - Assignment R-034-closure-restored-state-001: Independently verify the authoritative pre/post camp, storage/zone, actor identity, mission/assignment, follower rule, priority, and inventory values at the stated focused ceiling.
  - Assignment R-034-closure-authority-evidence-cleanup-001: Independently verify stale prior-process authority rejection, accepted later ordinary turns, semantic-bootstrap repair, OCR and invalid-report limits, observed-only control classification, and exact cleanup.
  - Current handoff: All three finite closure gaps are closed. Receipt `070d614ef3abac30339ca7a7e271a2774fbcbddea06c8dd9c448ecff25d4bd2e` verifies the exact restored camp, storage, actor assignment, rule, priority, and inventory values. Receipt `73c163cde60d6b4ff29e22d87eb83935b80675b2285c2351ee06f3d94f732d30` verifies stale-authority rejection, accepted later native turns, the semantic bootstrap, honest evidence limits, and cleanup. Receipt `e81ca33e034bfb6b0b8f7908d25d72a125da83e42738ab97e072e0a9c6528e7a` proves a fresh native generation-0 exit and exact-world generation-1 reload without fixture restaging, followed by native continuation and exact PID/broker cleanup. The earlier shared `fonts.json` temporary-file race remains preserved as a failed attempt. OCR, terminal bytes, and rendered text received no proof credit.
  - Subtasks:
    - [done] select-current-durable-state :: A current source-bound living-base scenario exposed meaningful camp, actor, storage, zone, and policy state.
    - [done] capture-before-save :: Native evidence binds actor, camp/mission, storage items, zone, policy, and inventory state before save.
    - [done] replace-native-process :: Native save/quit closed generation 0 and generation 1 reloaded the same world without fixture reinstall.
    - [done] compare-restored-state :: Durable identities and values survived while stale prior-process authority was rejected.
    - [done] prove-continued-behavior :: Later native pauses were accepted and the tested state remained stable without duplicate item mutation.
    - [done] close-restored-state-and-authority :: Independent receipts closed restored state and authority/evidence/cleanup at their focused ceilings.
    - [done] repair-and-close-native-lifecycle :: A fresh route avoided the shared temporary-file race, proved generation-1 native continuation, and reconciled every owned process and broker.
    - [done] accept-finite-closure :: All three independent closure gaps are durably closed at their stated evidence ceilings.
  - Durable acceptance: #1 via `R-034-closure-native-lifecycle-002`; SQLite evidence is authoritative.
<!-- DE67:DELIVERY-STATUS:END -->
<!-- DE67:DFS-SLICE:END id=R-034-S001 claim=R-034 -->

### 19. Fresh flesh-raptor behavior

<!-- DE67:DFS-SLICE:BEGIN id=R-035-S001 claim=R-035 -->

`monmove.cpp :: is_flesh_raptor` selects `mon_spawn_raptor`, `_shady`, `_unstable`, `_electric`,
`_dusted`, `_fungalize`, and `mon_fungal_raptor`. The production plan requires same-Z visible target;
legal candidates use native mobility/occupancy, crowding and held destination. The present scorer
uses an orbit distance of 4–6 and phase from native turn/absolute position modulo 6; these describe
the inspected tactic, not newly chosen balance requirements. It commits a swoop destination, stores
orbit `wander_pos`/`wandf`, or yields to ordinary movement when no orbit is suitable. Native monster
movement, run effects, target visibility and save/load remain competing readers/writers of that state.

Implementation status:

<!-- DE67:DELIVERY-STATUS:BEGIN claim=R-035 -->
- [x] R-035 — Fresh native flesh-raptor orbit, blocked-route fallback, committed swoop, melee pressure, encounter feel, controls, exclusions, and cleanup are proved at their stated evidence ceilings.
  - DFS slices: `R-035-S001`
  - Assignment R-035-fresh-raptor-encounter-001: Freshly prove native flesh-raptor orbit, swoop, fallback, and encounter feel with discriminating controls. Reconcile the current executable and source before play. Repository-owned harness, fixture, scenario, registry, observation, and implementation repairs are authorized when they preserve intended gameplay. Finish only when each behavior has source-bound native evidence or a named assigned-outcome exit under valid conditions. Writhing stalkers and zombie riders remain excluded. OCR, terminal bytes, and rendered text are presentation observations only and cannot prove input, time, state, or gameplay.
  - Assignment R-035-closure-orbit-control-001: Independently verify the exact open and crowded runs prove readable lateral orbit selection and the less-crowded arc choice from source-bound native frames and plan facts. Do not credit fixture setup, OCR, terminal bytes, or rendered presentation.
  - Assignment R-035-closure-fallback-pressure-001: Independently verify the exact blocked run proves no-readable-lateral-orbit fallback followed by committed swoop cadence, melee pressure, and the stated encounter-feel conclusion under its controls.
  - Assignment R-035-closure-evidence-lifecycle-001: Independently verify source and executable identities, artifact hashes, focused tests, excluded-creature absence, evidence ceilings, manifest changes, and explicit cleanup of every R-035-owned process.
  - Current handoff: Exploration receipt `813fe7aebb3fed9ac997e21c0de41c3a5baba62d023a94444326ed4307700c75` and three independent closure receipts settle the full R-035 claim. Orbit-control receipt `bb2302cd6ea32c7ee06388c9b286fd057a16b7dae44133043e9081b977b5e349` preserves the corrected crowded request identity. Fallback-pressure receipt `28e667d1bb71499a123a4ad786209427e28a147772d49c6a9215a79c7bd26612` verifies the blocked route, committed swoop, and melee pressure. Evidence-lifecycle receipt `fcfc9de408eff0754d35e360f7d6c4bfda23125890ac4290c96e1dd2d27aa53c` verifies source and artifact identities, 7 focused cases with 61 assertions, exclusions, evidence limits, fixture changes, and cleanup. The packaged probe reports remain timing-inconclusive and are not credited. OCR, terminal bytes, and rendered text received no proof credit.
  - Subtasks:
    - [done] prove-open-and-crowded-orbit :: Fresh native runs selected readable orbit arcs and chose the less-crowded side under a controlled crowd.
    - [done] prove-blocked-fallback :: A blocked lateral route produced the named fallback and continued into pressure without orbit jitter.
    - [done] prove-swoop-combat-and-feel :: Native cadence and melee outcomes support readable circling followed by concrete pressure.
    - [done] verify-controls-exclusions-and-tests :: Fixture controls are zero-credit setup, excluded creatures are absent, and 7 focused cases with 61 assertions pass.
    - [done] close-owned-processes :: All four R-035 game attempts and their brokers were explicitly closed; the omitted exploratory process was recovered before completion.
    - [done] audit-finite-closure :: Independent orbit-control, fallback-pressure, and evidence-lifecycle checks all closed before claim acceptance.
  - Durable acceptance: #1 via `R-035-closure-evidence-lifecycle-001`; SQLite evidence is authoritative.
<!-- DE67:DELIVERY-STATUS:END -->
<!-- DE67:DFS-SLICE:END id=R-035-S001 claim=R-035 -->

### 20. Fresh integrated performance

<!-- DE67:DFS-SLICE:BEGIN id=R-036-S001 claim=R-036 -->

`ProcessPerformance`, `sample_owned_session` and `PlayerClient.performance` bind samples to process
identity, run and binding; PID reuse, session end and changed owners reject attribution. CPU is a
process-core percentage; a mixed-context interval is labeled mixed. `compare_records` uses an
explicit workload label but that label cannot prove comparability. Native renderer timing, camp
cadence counters and action completion measure different costs and must remain separate from
controller/bridge memory, retained evidence size and NPC-runner/model work.

Implementation status:

<!-- DE67:DELIVERY-STATUS:BEGIN claim=R-036 -->
- [x] R-036 — Fresh matched living-base and hostile-ecology performance evidence is complete at its finite descriptive ceiling.
  - DFS slices: `R-036-S001`
  - Assignment R-036-integrated-performance-001: Build and run one source-current matched comparison of equivalent local gameplay workloads with and without the selected combined living-base and hostile-ecology load. Bind world, scenario, source, executable, active actors and operations, native actions, update latency, renderer pacing, CPU/RSS, and repeated samples. Preserve independent correctness and gameplay-feel verdicts, variability, and uncertainty. Repair repository-owned harness, fixture, scenario, registry, or observation faults when needed. Do not infer a universal performance target, use automatic time/RSS termination, substitute unmatched scenes, or replay accepted feature behavior merely to restate it. Own every game attempt, replacement, and broker through exact PID/birth exit or an explicit retained-session handoff. Exit with a valid matched comparison at its stated evidence ceiling, or the first code-grounded repository capability gap that prevents that assigned outcome after authorized repair.
  - Applied exploration: Receipt `4771c9c22aa60427fc61f6d86105c0edacf757cbb99b116f9504a5e82af03bd7` proves a fresh current-source matched baseline and hostile-ecology comparison after two narrow harness repairs. Both workloads completed eight native steps. The result includes repeated action timing, raw renderer pacing, ten identity-bound CPU/RSS samples per workload, bounded mechanical observations, and native quit confirmation for every game PID. The finite measurements do not establish a universal regression or subjective visual feel. Durable broker graceful-exit proof is also limited to run-local writer identity and observed absence.
  - Assignment R-036-closure-matched-measurements-001: Independently verify that baseline and feature scenarios are equivalent except for the selected hostile-ecology load, and that repeated native action timing, raw renderer pacing, CPU/RSS sampling, variability, and reported comparisons remain bound to exact current source, executable, world, run and process identities. Preserve the finite descriptive ceiling and identify any unmatched workload or attribution defect. Do not rerun accepted gameplay merely to restate it.
  - Assignment R-036-closure-correctness-feel-001: Independently verify the mechanical correctness observations and the honest gameplay-feel ceiling. Determine whether the retained native and renderer evidence supports any bounded smoothness or responsiveness observation without OCR or invented thresholds. If direct perceptual evidence is required, return that exact boundary; do not broaden mechanical setup into subjective proof or replay unrelated feature behavior.
  - Assignment R-036-closure-correctness-feel-002: Close only the remaining correctness-and-feel audit after restart-normalized task 001. Verify receipt `58a9b44d38843eb30a2849a80dfe7cce7c30a2906dcc4aa16da431b4b0a0670f` against the retained baseline and feature native streams. Both fresh action windows already prove the same mechanical east move from `[3372,996,0]` to `[3373,996,0]` at minute 8159, and both exact game processes have exited. The four presentation captures are byte-identical, so they cannot prove redraw timing, smoothness, responsiveness, or subjective feel. Do not replay gameplay. Exit when the mechanical correctness result and the explicit no-subjective-feel ceiling are independently verified, or when a concrete artifact defect prevents that audit.
  - Assignment R-036-closure-evidence-lifecycle-001: Independently verify the harness repairs and focused tests, scenario/source/executable and artifact hashes, no-replay and OCR limits, every game PID/birth plus native quit route, and the exact broker lifecycle ceiling. Identify any process or artifact discrepancy without erasing valid matched-performance evidence.
  - Current handoff: The finite comparison and all three independent closure checks are complete at their stated ceilings. Receipt `f820c12cbf8a7af88b26c93e89975fee41b7d33978c3784bcb3b6354fabf843b` verifies matched native mechanical movement and confirms that byte-identical static captures do not prove smoothness, responsiveness, redraw timing, or subjective feel. The package therefore keeps subjective feel, broader statistics, and durable broker graceful-exit proof as explicit non-claims rather than overstating the finite measurements. Whole-claim acceptance is ready.
  - Subtasks:
    - [done] bind-matched-workloads :: Equivalent current-source fixture states, actions, actors and operations differ only by the selected feature load.
    - [done] capture-native-performance :: Repeated identity-bound action timing, renderer pacing and CPU/RSS samples preserve variability for both workloads.
    - [done] judge-correctness-and-feel :: Mechanical observations are retained, while subjective visual feel and universal thresholds remain unclaimed.
    - [done] reconcile-game-lifecycle :: Every game PID/birth has a native quit route and observed OS exit; broker graceful-exit durability remains limited.
    - [done] audit-finite-comparison :: Independent audits verified matched measurements, mechanical correctness, evidence limits, and lifecycle at their stated ceilings.
  - Durable acceptance: #1 via `R-036-closure-correctness-feel-002`; SQLite evidence is authoritative.
<!-- DE67:DELIVERY-STATUS:END -->
<!-- DE67:DFS-SLICE:END id=R-036-S001 claim=R-036 -->

## Competing systems and override direction

| State/action | Readers | Writers / competing owners | Authoritative decision |
|---|---|---|---|
| Active surface/frame | Cockpit, client, projections | Native scope stack, input-context fallback, cached World | Exact top native owner wins. A child hides all parent actions; obsolete transient parents remain private until real successor publication. Unknown owner hard-stops. Frames are run-local, not save truth. |
| Request/receipt | Native owner, broker, evidence | Request transport, local physical input, duplicate queue entries | Game-thread consumer alone mutates native action. Run/surface/frame/action/target validation precedes it; completed request IDs replay receipt without action. A missing successor does not erase an accepted native receipt or restore stale authority. |
| Item/response/actor target | Inventory/dialogue/inspection/target adapter | Native UID/object, regenerated responses, cached labels | Resolve exact currently advertised object and recheck validity. Reject moved/removed/stale IDs; rediscover after native transfer or reload. Never choose by label, letter or row. |
| NPC intent/turn | LLM runner, native movement, camp | `llm_intent` queue, combat, mission, Patrol, follow rules | NPC policy is separate from cockpit input. Native eligibility/danger/mission owners may override an ordinary intent; one actor cannot receive duplicate competing movement in the same turn. Durable assignment/rules win after reload; transient queues do not resurrect. |
| Camp service/resources | Mission, Locker, Patrol, crafting | Zone manager, assignment/mission writers, stock/item owners, cached plans | Native camp/zone/faction/item state owns truth. Plans and reservations derive from it and invalidate on relevant changes; actual service must revalidate actor, item and resources before consumption. No duplicate transfer/food debit on retry or reload. |
| Signal memory/report | Structural drive and response | Staffed physical observation, active-scout reads, player opportunity adoption, setup | Exact production observer/source/channel creates signal-discovery truth. Scout/report/decision owners advance response. Competing provenance must be explicit and cannot receive credit for the required shared discovery route. |
| Hostile local/abstract travel | NPC movement, overmap scheduler | Reservation, handoff/materialization, generic travel | One simulation owner per exact operation/member generation. Handoff retains route/identity and makes the old owner yield; retries cannot clone actors or double-advance them. |
| Shakedown/player relationship | `guaranteed_hostile`, `attitude_to`, AI targeting | Persisted operation phase/member/branch, faction dislike, player attack, trade | Exact parley and paid return override generic hostility. Refusal/incomplete payment/player attack releases this group. Favorable rolling ambush is separate. Terminal receipt applies once after return. |
| Night raid | Rally/approach/contact | Native clock and persisted operation | Night controls departure only. After departure, dawn does not invalidate the operation or route. |
| Save/reload | Restored actors/camp/ecology, client | Native serializers, fixture installer, runtime caches | Saved world owns durable truth. Reload replaces process/frame domain and reconstructs caches; fixture reinstall cannot masquerade as continuation. |
| Raptor movement | Monster plan/move/save | Orbit scorer, target/run effects, held destination, ordinary movement | Scorer proposes only legal native candidates; movement owns consequence. Fallback yields to ordinary AI; existing identity/state prevents a held orbit from becoming another actor's state. |
| Proof and performance | Player, evidence consumers | Native observations, witness prose, setup, aggregate result | Exact retained facts support each independent verdict. Mechanical validity and cleanup are separate from causality/feel. Matched native context owns a performance comparison, not its label. |

## Acceptance and proof

Shared proof shape:

```text
current source/executable + audited scenario/world + declared zero-credit preparation
-> native player action -> authoritative production transition
-> identity-continuous native facts and actual consequence
-> immutable evidence + independent mechanical/causal/feel/persistence/cleanup result
```

Every newly credited result binds commit/tree or relevant source digest, executable digest, fixture
and profile manifests/options, world/save, run/process/generation, actor/item/site/lead/report/
operation identities where applicable, native request/frame/receipt and game time. Record setup and
interventions at their actual time and bound later causal credit accordingly. Current bindings must
be rechecked when source, executable, scenario or world changes; no old token or source label alone
supplies current authority. Freshness means new behavior observed for this requested package after
this refreeze, not a report reingestion, historical witness reinterpretation or startup rerun.

| Red ID | Outcome test | Evidence and false-green boundary |
|---|---|---|
| R-SURFACE-011 | Current input sources are truly classified; required discovered owners operate and unsupported owners hard-stop. | Source inventory result, actual native scope/action/child proof, affected renderer route and actionless control. Filename membership or truthful unsupported status is not completed support. |
| R-026 | The complete fresh feature package is usable and all its required independent outcomes are proved. | Current scene/charter/guide and immutable independent results for R-029/R-031–R-036; no old green, single aggregate verdict or deleted history. |
| R-029 | Shared natural signal/scout/report/travel, safe normal demand/payment/return, combat-release branches, intentional rolling ambush, cannibal dawn commitment. | Exact identity and ordered native events across each independent branch/control and reload; prepared report/contact, demand screenshot or paid write alone fails. |
| R-031 | Actual NPC request/context/reply/action and follow/stay/camp routing. | Bound utterance, recipient, snapshot, runner result, physical behavior and competing-owner context; prewarm/promise alone fails. |
| R-032 | Native camp establishment, completed mission and actual Locker/Patrol/Food/Storage service. | Actor/resource/item/zone/policy/plan-to-outcome records with controls and independent verdicts; prepared camp or selector acceptance alone fails. |
| R-033 | Fresh channel sensing/memory controls and local/overmap continuity. | Source/observer/channel/lead and route/member/owner identities across controls; player-only knowledge, unrelated site logs or double ownership fails. |
| R-034 | Native new-process save/load and resumed durable behavior. | Before/save/exit/reload/later-turn evidence without reinstall or stale grants; successful cleanup or saved file alone fails. |
| R-035 | Native raptor orbit/swoop/fallback and readable encounter behavior. | Plan plus actual positions/combat in open/crowded/blocked situations; debug spawn/pure scorer alone fails. |
| R-036 | Matched combined workload cost and feel. | Raw pacing/update/action/process distributions, context, variability and bounded conclusions; labels, unmatched scenes or automatic kill limits fail. |

Preserved R-SURFACE-001–010, R-027, R-028 and R-030 acceptance remains at its original evidence
strength. The earlier wait prototype `20260826_135902` remains narrow historical evidence only.
Qualification at `54d6c00dfefafc3443f80097f5a3bf1664192348`, documented by
`1bfcf283417d63ae407bc66fc9950a90a68dd5b5`, establishes reusable harness capabilities and their
limits. It does not close this new campaign. The selected Mac scenarios do not certify full
Windows/Linux game runtime; renderer or platform claims require their own applicable production
route and source-bound executable.

## Freeze and retained evidence

Status: Refrozen. Original inspected baseline, WEC identities, source reconciliation, acceptance
scope and complete freeze history are retained exactly at `2f7d9ea2bcd7643a6e64d44e9adc703a91c6070f:.de67/DFS.md`
and `state/review-incident-prompt-delivery-20260908/baseline/DFS.md`.
The 2026-09-08 owner-scoped refinement changes functional descriptions and tooling representation;
it grants no new gameplay acceptance. Original R-SURFACE/R-027/R-028/R-030 proof retains its scope;
R-031–R-036 require fresh independent proof. Named evidence may close a requirement without weakening
it. The WEC owns intent, exclusions and gameplay-fix promotion authority; source/evidence may refine
mechanisms within that contract. No scenario, witness, receipt, clock or accepted result is replaced.

## Owner-authorized delivery tooling — cycle 9

These named maintenance slices enable the existing campaign's evidence route; they add no gameplay
acceptance, change no product outcome and confer no authority over policy, clocks, guard invariants,
owner decisions or accepted records. The cycle-9 owner queue authorizes Sol to commission repository
implementation and focused verification, with exclusive file/runtime ownership per assignment.

<!-- DE67:DFS-SLICE:BEGIN id=R-MAINT-COMPACT-IO-S001 claim=R-MAINT-COMPACT-IO -->
- [ ] 🔴 R-MAINT-COMPACT-IO — Existing build and native/query interfaces return useful bounded
  decision fields, errors/contradictions and exact full-artifact handles while retaining original
  output. Actor/active-service prerequisites and recorded startup uncertainty remain observable.
  - Proof: controlled success/failure builds retain complete logs and return honest status; historical
    Patrol frames and inactive-owner counterexamples preserve identities, priority/order and runtime;
    loading/modal/ready/exited fixtures preserve uncertainty and exact recovery handles. No gameplay
    acceptance or live-process action is implied. See the named ledger assignments for edit ownership.
  - Owner extension f5319eac0adc, consolidated with cycle 9: optional selected native views expose
    stable entities/relevant fields, source/run/frame/turn identity, freshness, unknowns and original
    handles. Before/after comparisons preserve both observations and distinguish changed, unchanged,
    added, removed, unknown and incompatible bindings; different-time views are not atomic. Existing
    event/journal queries follow recorded request/actor/source links to native acceptance, rejection
    and results, retaining pending/missing links and competing listeners/writers without inferred
    causality. Reuse inspector/cockpit facts, adding narrow read-only native instrumentation only
    when needed. No new DSL, verdict oracle, automatic setup/time advancement or receipt framework.
  - Prove crafting recipient/camp/capability, recipe/resources/tools/ownership/location and job,
    and signal observer/source/visibility/lead/content/timestamps through shared machinery. Cover
    missing fields/resources/capability, source freshness, process/actor identity incompatibility,
    same-content timestamp refresh and unrelated-writer contamination. Apply it to Patrol or Pay
    through selections/adapters only. Use original native artifacts and isolated tests first, then
    the smallest necessary source-bound native integration test. Sol supplies selected task facts
    through existing context preparation and verifies actual worker input/use; no replay for adoption.
<!-- DE67:DFS-SLICE:END id=R-MAINT-COMPACT-IO-S001 claim=R-MAINT-COMPACT-IO -->

<!-- DE67:DFS-SLICE:BEGIN id=R-MAINT-RESULT-REUSE-S001 claim=R-MAINT-RESULT-REUSE -->
- [ ] 🔴 R-MAINT-RESULT-REUSE — Selected accepted facts and remaining boundaries enter the next
  prepared worker packet through the existing context tool, with immutable references and current
  source/evidence dependencies. Sol judges validity; tools do not invent acceptance or proof.
  - Proof: an isolated actual packet preserves generation-0 save/exit and assigns only the missing
    generation-1 continuation, retains independent contributions, rejects stale dependencies and
    replaces obsolete current conclusions without destroying their archived revisions. Demonstrate
    the interface's next relevant use without replaying accepted product evidence solely for adoption.
<!-- DE67:DFS-SLICE:END id=R-MAINT-RESULT-REUSE-S001 claim=R-MAINT-RESULT-REUSE -->

## Owner-authorized advisory integration — review d1cfc813605b

<!-- DE67:DFS-SLICE:BEGIN id=R-MAINT-CONSULT-S001 claim=R-MAINT-CONSULT -->
Implementation status:

<!-- DE67:DELIVERY-STATUS:BEGIN claim=R-MAINT-CONSULT -->
- [x] R-MAINT-CONSULT — Sol can send a bounded task-bound request through the configured conversational route and receive a correlated non-authoritative reply. The adapter rejects stale, rebound, duplicate, unavailable, echo, and authority-escalation cases at the tested ceiling.
  - DFS slices: `R-MAINT-CONSULT-S001`
  - Assignment R-MAINT-CONSULT-001: Implement and test a narrow repository advisory adapter using the supported OpenClaw Gateway agent interface. `.de67/task-logs/consultation-interface.md` records the verified CLI/session route and the missing sender/task/revision/reply validation. Keep endpoint configuration host-owned, preserve existing messaging files, and merge only authorized role guidance. Validate sender against current coordinator ownership, workspace/lineage/run/task/assignment revision, correlation and duplicate retries; return advice to the originating Sol call and reject stale/cross-workspace/unbound replies. No raw Codex UUID, full transcript forwarding, queue promotion, plugin install, credential collection or new permissions. Deliver a harmless round trip on the next ordinary Sol run after isolated wrong-sender/workspace/revision/duplicate/unavailable/no-escalation tests; do not spawn a competing coordinator to prove it. A transport acknowledgement, running request or timeout leaves the task open with an exact continuation.
  - Current capability: OpenClaw exposes `agent --agent --session-key --message-file --json`; configured conversational Astra exists. No inspected interface yet authenticates Sol's assignment revision or validates its correlated advisory reply. The owner-authorized implementation route is open; no new host grant is currently identified. If existing gateway access rejects the ordinary caller, name that exact setup gap and continue independent work.
  - Applied advisory adapter: Receipt `497a50e594395f76b0bce34b2dc4c89c94dc7575b8b1cdefd339966c08e00c81` proves task-, run-, workspace-, lineage-, revision-, sender-, and correlation-bound requests and replies. Seven focused tests pass. One harmless ordinary Sol request returned a correlated reply after one explicit retry. Advice remains non-authoritative and cannot change the queue, delivery state, owner decisions, or DE67 state. The initial and retry processes exited.
  - Assignment R-MAINT-CONSULT-closure-binding-001: Independently verify coordinator, workspace, lineage, run, task, revision, sender, recipient, correlation, stale, duplicate, unavailable, echo, and no-escalation behavior from receipt `497a50e594395f76b0bce34b2dc4c89c94dc7575b8b1cdefd339966c08e00c81`. Use exact artifacts and focused tests. Do not send another live consultation or mutate owner, queue, or DE67 state.
  - Assignment R-MAINT-CONSULT-closure-roundtrip-001: Independently verify the harmless Gateway request and correlated returned reply, including the retained first unavailable attempt and single explicit retry without duplicate delivery. Do not resend the request or treat advice as authority.
  - Assignment R-MAINT-CONSULT-closure-evidence-lifecycle-001: Independently verify changed files and hashes, focused tests, durable advisory state, non-authority limits, no queue or DE67 mutation, and absence of task-owned processes. Do not launch gameplay or another consultation.
  - Advisory binding audit: Receipt `647c592a59d28d17cabc4a640198fc54151856128a034cb2f8843a45abcfe35e` independently verifies the configured coordinator, workspace, lineage, run, task, revision, sender, recipient, and reply correlation. Seven tests cover stale, duplicate, unavailable, echo, mixed-writer, and authority-escalation rejection. No request was resent and no shared state changed. The retained reply wording contains a stale-state presentation nuance, while the durable adapter state is replied. Authentication remains SQLite ownership plus pinned host guidance, not cryptographic identity, and advice remains non-authoritative.
  - Advisory round-trip audit: Receipt `d3cf244ddd2d9319936edd261432eddaeb756c3e0d85533fc5080672f2b265de` verifies exactly one durable request, its correlated Gateway reply, the retained first unavailable attempt, and one explicit retry. Seven tests pass. No request was resent, no task-owned process remains, and no queue, DFS, or DE67 state changed. Advice and authentication remain limited as stated above.
  - Advisory evidence and lifecycle audit: Receipt `fa57de6c99706172c0da381d6c7e92727a025e4c90899c3dbdaa6785f13acedb` verifies exact implementation, configuration, request, retry, durable state, SQLite lifecycle, and process identities. Seven tests and compilation pass. No consultation was resent, no gameplay or task-owned process launched, and no queue, DFS, or DE67 state changed.
  - Subtasks:
    - [done] bind-advisory-request :: Authenticate current coordinator/task and the configured conversational recipient without borrowing owner authority.
    - [done] correlate-advisory-reply :: Preserve request/revision/source attribution, deduplicate and expose stale or failed delivery.
    - [done] verify-advisory-use :: Adversarial cases and one harmless ordinary Sol round trip passed without gameplay or queue mutation.
    - [done] audit-consult-binding :: Receipt 647c592a59d28d17cabc4a640198fc54151856128a034cb2f8843a45abcfe35e verifies identity binding and fail-closed behavior.
    - [done] audit-consult-roundtrip :: Receipt d3cf244ddd2d9319936edd261432eddaeb756c3e0d85533fc5080672f2b265de verifies the retained request, reply, and explicit retry path.
    - [done] audit-consult-evidence-lifecycle :: Receipt fa57de6c99706172c0da381d6c7e92727a025e4c90899c3dbdaa6785f13acedb verifies artifacts, tests, authority limits, state isolation, and process cleanup.
  - Durable acceptance: #1 via `R-MAINT-CONSULT-closure-evidence-lifecycle-001`; SQLite evidence is authoritative.
<!-- DE67:DELIVERY-STATUS:END -->
<!-- DE67:DFS-SLICE:END id=R-MAINT-CONSULT-S001 claim=R-MAINT-CONSULT -->

Scoped refreeze 2026-09-08, review d1cfc813605b: owner relays6f6a959b0eab,4673b7453329,
f5319eac0adc refine preparation, the existing workbench assignment, R033-F002 repair and advisory
integration. Source inspected at the HEAD in `state/review-owner-d1cfc813605b/source-baseline.json`.
All existing stable claims/slices, accepted proof, clocks and independent assignments remain intact.
This record grants the named implementation route, not completed capability or gameplay proof.

<!-- DE67:DFS-SLICE:BEGIN id=R-MAINT-PROMPT-DELIVERY-S001 claim=R-MAINT-PROMPT-DELIVERY -->
Legacy slice identity retained for existing references. S002 defines the current coordinator,
continuation and reviewer prompt-loader behavior. The obsolete `run_coordinator` description and
commissioning history are retrievable at the pre-review Git baseline above and in the ledger.

Implementation status:

- [ ] 🔴 R-MAINT-PROMPT-DELIVERY — Each supervisor-launched coordinator receives the current guarded role prompt after a method promotion, without restarting or duplicating the supervisor.
<!-- DE67:DFS-SLICE:END id=R-MAINT-PROMPT-DELIVERY-S001 claim=R-MAINT-PROMPT-DELIVERY -->


## Owner-scoped retrieval evaluation — 2026-09-08

Refrozen under owner gate `c895367b1fc2`, source HEAD
`7fdb917c8e149b87634ae04a1afadacbbf90f50c` on `dev`. This adds a non-product tooling
experiment and clarifies effective prompt delivery. Prior claims, accepted proof and independent
assignments remain intact. Specification is not implementation or adoption evidence.

<!-- DE67:DFS-SLICE:BEGIN id=R-MAINT-PROMPT-DELIVERY-S002 claim=R-MAINT-PROMPT-DELIVERY -->
### Current prompt delivery

S002 supersedes S001's source description without rebinding either slice identity.
`coordinator_supervisor.py::_fresh_prompt_module` reads the installed UTF-8 source anew at an
authorized transition, compiles it and loads its prompt producers in an isolated module.
`run_child` uses the resulting coordinator or continuation producer; `run_mutation_reviewer`
uses its reviewer producer. A missing, undecodable, corrupt or load-failing source marks the run
failed before Popen. Exact gate, workspace/state/lineage/run bindings and generation-specific
restart reason flow to the sole authorized child. Owner stop, clock, policy and external-supervisor
launch ownership remain authoritative. No extra child or supervisor restart is a test dependency.

The discriminating check loads a supervisor, changes on-disk prompt text and verifies fake-runner
stdin at the next authorized transition for each role and continuation; unchanged process memory
must not select the earlier text. Invalid source must launch zero children. Live adoption means
retained input and use by a naturally authorized child, distinct from isolated rendering. Current
implementation evidence and remaining adoption/closure work are in the ledger and receipt
`d193034a63642833354c61a41d60f84bfc495c454a9a34ebf46c5184dff97579`.
<!-- DE67:DFS-SLICE:END id=R-MAINT-PROMPT-DELIVERY-S002 claim=R-MAINT-PROMPT-DELIVERY -->

<!-- DE67:DFS-SLICE:BEGIN id=R-MAINT-EVIDENCE-SEARCH-S001 claim=R-MAINT-EVIDENCE-SEARCH -->
### Experience and authority

A caller describes an unknown passage/event, optionally constrains task, run, feature, actor,
producer or revision, and receives a compact ranked page of original excerpts. Each hit identifies
why it matched, its source/occurrence, verified expansion handle, source generation/revision,
known run/task/actor bindings and freshness. Unknown attribution stays unknown; similar text never
confers proof, acceptance, current instructions or owner authority. The caller can expand context
or use existing exact queries without an indexing chore. The outcome is discovery plus an honest
usefulness evaluation; it does not require a particular vendor, positive verdict or global rollout.

### Existing sources and boundaries

Reuse `tools/openclaw_harness/cockpit_evidence.py::query/record_artifact`: source snapshots and
`{path, offset, length, sha256}` handles already bind raw records; original recovery rejects changed
bytes. `evidence_events.py::parse/envelopes` supplies native/NPC/runner event projections and null
unpublished correlations. Its `query` currently reads complete source files, so it is a reference
for semantics, not an incremental ingestion implementation. `cockpit_file_bridge.py` exposes
`log-query`, `record-artifact` and response status/artifact/slice retrieval. Keep these entrypoints
usable. Respect their distinction between a recorded request and its gameplay result.

`work_context_provider.py::session_context` deliberately follows exact referenced sessions, not an
archive walk or guessed newest run. `worker_receipt.py::compact_worker_receipt` exposes artifact
references/entrypoints; `context_library.py::catalog/show` provides revisioned section retrieval.
Bootstrap a declared source manifest from existing receipt/session/artifact inventories and explicit
selected historical sources. Register newly produced evidence automatically through the narrow
existing producer/inventory integration chosen by the worker; periodically reconcile registered
sources to recover missed notifications. Report source coverage, exclusions and unavailable roots.
Do not recursively inject the workspace/history or change existing receipt acceptance semantics.
No `.de67/no-go-zone/`, secrets, credentials, unrelated workspaces or live owner data are index inputs.

Start with logs, worker findings and investigation artifacts whose location or terminology is
unknown. Plain-text/Markdown chunks must point to original spans, not solely generated summaries.
Chunk JSONL at complete records and group meaningful adjacent evidence with constituent handles;
preserve raw unparsed records and diagnostic context. Dedupe text for embedding reuse while keeping
all occurrence identities. Repeated polling must not bury a rare diagnostic or contradictory outcome.
Code indexing is a later extension; if included after the decision, bind functions/sections to source
revisions and make current versus historical lookup explicit.

### Implementation contract

`tools/openclaw_harness/evidence_search_index.py::EvidenceIndex` owns the derived SQLite state.
`sources.path` identifies a registered original; `generations.generation_id` is globally unique,
while `generations.generation` is only a per-path revision counter. `sources.active_generation`
and `occurrences.generation_id` refer to the global ID and must resolve to that source's path.
`_new_generation` publishes the source pointer and generation atomically. `ingest` must leave
unrelated sources untouched; `coverage` joins this same global identity. Embedding cache identity
includes content hash, model/version and chunking version, while each original occurrence retains
its own source span and hash. Text deduplication must never merge occurrence identities.
Schema migration may repair pointers only using that source's generations; if a legacy index has
already lost current coverage or duplicated occurrences, recover from verified originals or expose
the degraded state. A pointer rewrite alone must not claim repaired coverage.

The semantic backend is an available local or already-authorized implementation, with visible
model/version and failure state. No new spending or upload authority is granted. Lexical fallback
remains usable but cannot establish semantic usefulness; hardcoded synonyms or stub ranking cannot
deliver the natural-language route.

For immutable completed artifacts, ingest once per content identity. For growing logs, checkpoint
only committed complete records; retain a partial trailing record for the next pass. Detect source
replacement, truncation and rotation, distinguish generations, and resume after crash without lost
or duplicate occurrences. Changed/deleted registered sources must become visibly stale/unavailable
or be refreshed; existing records cannot silently masquerade as current. Publish an index generation
only after its writes/cursor state are coherent. Rebuild from originals and reuse matching embeddings
where possible. Keep recovery/indexing automatic and independent of task lifecycle; bound resource
use with adjustable execution settings, not acceptance quotas or worker waiting requirements.

`evidence_search_query.py::EvidenceSearch.query(text, filters, offset, limit, expand)` returns
ranked verified originals and coverage. Negative offset/expansion or nonpositive limit returns
`invalid_paging`. Occurrence metadata filters apply to explicitly supported stored fields;
other keys select fields in the recovered original JSON. Recover with `record_artifact` and its
exact path/offset/length/hash before evaluating original-only fields such as `feature=crafting`.
Do not reject those fields because an occurrence row lacks them. An absent original key differs
from a present JSON null; absent keys do not satisfy an explicit null filter. Literal filters
must remain literal and any inferred interpretation must be visible and adjustable.

Results expose source/occurrence identity, source generation, match reason and expansion handle,
indexed coverage and committed position, pending catch-up and partial/degraded/no-match status.
Changed bytes or missing originals produce visible stale/unavailable evidence, never unverified
cached excerpts. Paging/expansion preserves the selected source generation. Missing model/index,
corrupt state and backend failure expose errors/degradation with the existing exact-query route.
Alternate or contradictory hits remain discoverable. Partial-index no-match cannot prove absence;
retrieval confers no current instruction, acceptance or causal authority.

### Behavioral proof

Use existing `cockpit_evidence_test.py` exact filtering/hash-tamper fixtures and
`evidence_display_test.py` append-stable/replaced-prefix fixtures as compatibility footing. Extend
focused tests against the chosen implementation for:

- Two sources A/B with local revision 1 but distinct global IDs: unchanged B re-ingestion preserves A/B once each and source-correct coverage, including database reopen and legacy migration. Initial and repeated ingestion; append catch-up after process restart; interrupted writes/partial
  records; crash between index and cursor commits; truncation, replacement, rotation and deletion.
  Verify exact occurrence counts/identities and recoverable positions, not only successful commands.
- Content reuse across multiple run occurrences, model/chunking-version mismatch and rebuild,
  unparsed records, repetitive polling with retained rare/contradictory evidence, and missing inputs.
- Natural paraphrases with little terminology overlap using the real selected semantic backend;
  misleading near-matches, conflicting outcomes and unrelated runs; exact task/run/revision/actor
  filters, original-only `feature=crafting`, absent versus explicit-null keys, mixed stored/original selectors, null identities and visible query interpretation. Controlled fixtures may test failure
  mechanics, but mocked embeddings alone cannot prove semantic utility.
- Original excerpt and surrounding-record round-trip, hash mismatch/source replacement after search,
  stable paging for a selected index generation, stale/partial coverage, no match, unavailable model,
  corrupted index, and a functioning exact/structured fallback. Preserve existing query behavior.

Complete build/test logs and source/model/fixture identities remain retrievable through existing
artifacts/receipts. Source or isolated proof grants no gameplay credit. A fresh game campaign is not
required merely to verify retrieval; use retained original evidence and normal upcoming work.

### Convenience evaluation and owner decision

After functional tests, enable explicit trial use by workers on representative difficult lookups
in their normal assignments and by Sol for coordinator-side discovery. Verify actual tool invocation,
returned originals and their use in answering the question; installed commands, selected bundles or
positive self-report alone are insufficient. Workers should be able to ask directly without first
learning source filenames, event spelling or indexing administration. Record concise friction,
misses and follow-up searches along with successes through existing results, not a parallel form.

Compare against existing exact/structured queries and bounded Luna retrieval where appropriate.
Use comparable questions and disclose prior-knowledge/order effects; separate development/tuning
examples from evaluation questions when practical. Cover enough differing cases to change a keep,
revise or retire decision, without an arbitrary example quota. Do not replay accepted gameplay.
Measure answer relevance/misses and source fidelity, elapsed effort, follow-up/helper/retry burden,
returned and ingested context, and non-overlapping full-tree cached/uncached/output token use where
available. Include indexing/embedding/search, cold and incremental cost, storage and maintenance
friction. Preserve measurement boundaries and missing accounting; bytes, latency and subjective
convenience are not substitutes for measured token savings.

Sol presents Josef a compact evidence-backed keep/adopt, revise/retest or retire recommendation,
with original handles, actual worker/coordinator use, failures, cost boundaries and tradeoffs.
An explicit owner decision precedes making retrieval the default or expanding scope. That decision
applies to this tool, not unrelated delivery. A negative evaluation and retirement decision can
complete this experiment while retaining evidence and exact-query capability. A revision decision
keeps the concrete remaining work open. A keep decision requires demonstrated functional behavior
and convenience; it does not retroactively turn unit tests into adoption evidence. Preserve accepted
work and reduce obsolete retrieval guidance if adoption makes it unnecessary.
Implementation status:

- [ ] 🔴 R-MAINT-EVIDENCE-SEARCH — Workers and Sol can discover original evidence by describing its meaning, with automatic incremental indexing and an owner decision on demonstrated usefulness before default adoption or expansion.

<!-- DE67:DFS-SLICE:END id=R-MAINT-EVIDENCE-SEARCH-S001 claim=R-MAINT-EVIDENCE-SEARCH -->


<!-- DE67:DFS-SLICE:BEGIN id=R-MAINT-CONTEXT-ROUTING-S001 claim=R-MAINT-CONTEXT-ROUTING -->
### Declared functional slices reach worker context

`policy_kernel.py::_exploration_route` finds the exact assignment's owning active ledger item,
validates its `DFS slices:` selectors through `mutation_guard` slice APIs and extracts the selected
claim-bound slices in declared order. Missing, duplicate, wrong-claim or malformed selection fails;
no first-same-claim or stale-context fallback is valid. `_dfs_worker_boundary` removes only the
projection after `Implementation status:`; all functional contract text precedes that delimiter.
Independent same-claim assignments, issued packet hashes and prepared-context revisions remain bound
to their owners. This affects context extraction, not policy rules, clocks, proof or launch ownership.

The counterexample selects S002 from a document containing earlier S001: actual packet reference
context must contain S002 and exclude S001. Multiple selectors preserve order; invalid selectors
fail before dispatch. Retained baseline and current isolated output are in
`state/review-owner-c895367b1fc2/` and `state/review-incident-prompt-delivery-20260908/`.
Implementation/acceptance progress belongs in the ledger; these tests define the functional result.

Implementation status:

- [ ] 🔴 R-MAINT-CONTEXT-ROUTING — Worker dispatch uses the ledger's declared DFS slices rather than the first same-claim slice, with validated original contract context.
<!-- DE67:DFS-SLICE:END id=R-MAINT-CONTEXT-ROUTING-S001 claim=R-MAINT-CONTEXT-ROUTING -->


<!-- DE67:DFS-SLICE:BEGIN id=R-MAINT-FS-MIGRATION-S001 claim=R-MAINT-FS-MIGRATION -->
### Functional Specification identity and delivery projection

One canonical `.de67/FS.md` holds functional behavior, source references and discriminating
acceptance criteria. Existing `.de67/DFS.md` references resolve through an explicit compatibility
mapping to that same content, never a second independently mutable specification. Keep durable
claim/slice identities and existing marker/selector syntax readable; old packets and receipts
retain their original bytes, hashes and historical meaning.

A shared resolver used by `policy_kernel`, `mutation_guard`, `deadline_harness`,
`coordinator_supervisor` and `method_provenance` selects the canonical specification or the legacy
one during migration. Conflicting dual content, missing target and malformed identity fail visibly.
Policy extraction passes the exact ordered, claim-bound slices selected by each independent ledger
assignment to the worker. It rejects missing, duplicate and wrong-claim selectors. Prepared context
revision checks remain effective; an old packet is not silently rewritten to a new specification.

Delivery assignments and current progress are projected in the existing `work-ledger.md`; historical
proof remains at its exact existing artifact, receipt, Git revision or SQLite handle. Durable SQLite
acceptance owns acceptance/reopen transitions. `deadline_harness` projects that state into the ledger
without requiring an implementation-status prose block inside the FS. Missing or invalid acceptance
evidence still fails; an absent decorative FS status block cannot obstruct otherwise valid acceptance.
Supervisor open-work/completion, provenance and guard baselines resolve the same contract and ledger.
Legacy `selected_lane='DFS.md'` records remain valid scheduling/receipt identities; a filename change
must not silently alter their meaning. Migrate status baselines deliberately, preserving accepted
claims and reopening behavior rather than resetting proof or clocks.

Prove on isolated copies: ordered multi-slice dispatch and independent same-claim assignments;
old references/issued packets unchanged; missing/conflicting/cross-claim rejection; durable accept
and reopen with a functional-only FS, including the current COMPACT-IO and RESULT-REUSE status-block
counterexamples; supervisor completion/open-work and old receipt/provenance compatibility. Verify
the migrated functional content actually reaches packet callers. Current consumer references are in
`state/review-incident-prompt-delivery-20260908/fs-consumers.md`. This is an owner-authorized
representation migration, not authority to weaken evidence, change product outcomes or launch a process.

Implementation status:

- [ ] 🔴 R-MAINT-FS-MIGRATION — One Functional Specification describes code behavior while the existing ledger and evidence surfaces own delivery tracking and proof, with compatible routing and lifecycle operations.
<!-- DE67:DFS-SLICE:END id=R-MAINT-FS-MIGRATION-S001 claim=R-MAINT-FS-MIGRATION -->
