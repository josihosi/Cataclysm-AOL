# Semantic-Surface Cockpit and Fresh CAOL Feature Package DE-67 Functional Specification

Status: Refrozen
WEC: `.de67/WEC.md`
Source baseline: `Cataclysm-AOL-hostile-ecology-dev | dev | 1bfcf283417d63ae407bc66fc9950a90a68dd5b5 | tree 1bbb62eebc15bc887c133b26055f8b5f8720c5ec | inspected 2026-09-06 | no tracked product changes; unrelated untracked run.witness.json excluded`

## Document authority

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

## External research sweep

No unresolved product ownership edge requires an external mechanism or new design in this testing
refreeze. The source, current qualification documentation and explicit owner decisions determine the
routes and limits. External Codex runtime documentation used for workspace probes is preparation,
not product evidence or a DFS dependency.

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
  - Current handoff: Durable receipt `38862ed5c00aefd8fe24b9f414eecb3fc7435791d8d74ed2c4ebef85b87f8010` completes task `R-032-fresh-camp-package-001`. Fresh native runs prove camp establishment, worker assignment, Food accounting from 0 to 48,160 kcal, Survey Expansion completion after the strict 3h+1m boundary, bounded Locker service, Patrol assignment/runtime at priority 9, and Gather Materials placement of four exact camp-owned item types at the first Storage-zone center. Looting remains correctly classified as `ACT_MOVE_LOOT`, not a companion mission. R032-F001 preserves the remaining contradiction: native Patrol priority zero was accepted, but Katharina still had `GUARD_PATROL` and an active patrol order after another native minute because cached-roster eligibility ignores the zero priority. No gameplay repair was made. OCR, terminal text and presentation were not used as proof. Owner receipt70c4632e512e promotes the Patrol release repair below; independent sibling tests and the valid enabled-Patrol proof remain accepted at their original ceilings.

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

- [ ] 🔴 R-036 — The fresh combined living-base/hostile-ecology package lacks matched performance evidence.
  - DFS slices: `R-036-S001`
  - Code/proof gap: R-028's old pair and qualification's finite CPU/RSS/action samples or parser
    allocation experiment are not fresh integrated gameplay comparison.
  - Required mechanism: pair equivalent current-source fixture states/workloads with and without
    the particular feature load being compared. Include rendered local transitions, established
    camp and hostile activity; an idle/action-only versus visible-bandit question requires matched
    input/simulation context, not two unrelated periods with those labels.
  - Proof: retain source/build/world/scenario identity, active actors/operations, native action and
    update latency, raw renderer pacing distribution, CPU/RSS, repeated comparable observations
    sufficient to expose variability and associated gameplay-feel observations. Report measured
    differences and uncertainty. Investigate profiling only if a reproduced cost/feel concern gives
    it a concrete question. Preserve independent test results if a gameplay bug prevents one load.
  - Controls: no universal FPS/RSS/time target, automatic time/RSS game kill, unmatched scene,
    average-only result, synthetic-only scorer/trace test, or profiler-only claim. Source/binary
    matching is necessary but not behavioral evidence. Cleanup records distinguish explicit finish
    from native save/quit and exact process exit; native exit credit needs that native route.
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

## Freeze record

- Status: Refrozen
- Frozen source baseline: `1bfcf283417d63ae407bc66fc9950a90a68dd5b5`, tree `1bbb62eebc15bc887c133b26055f8b5f8720c5ec`, branch `dev`, inspected 2026-09-06.
  No tracked product/test changes were present. Unrelated `run.witness.json` is excluded and preserved.
- WEC identity: SHA-256 `c5dde703601afc5562874f4fbf4548b08b0994ab0f503b5f894be7e60ef8c456`.
  The supplied file extends the previous WEC exactly; its authorized import preserves every old byte.
- User-owned choices: all compatible semantic-surface and hostile-ecology intent remains binding;
  comprehensive fresh in-scope CAOL testing supersedes old acceptance exemptions; independent
  verdicts, zero-credit preparation, exclusions, native lifecycle and the WEC fault/promotion
  boundary remain unchanged.
- Source reconciliation: native surface adapters and request transport exist; wait duration ownership
  is real; the cannibal-only dispatcher has no live caller; exact shakedown parley/paid departure
  already override generic hostility. Their stale absence descriptions are retired, not implemented
  again. The source-inventory omission and unsupported-owner coverage are explicit in R-SURFACE-011.
- Historical preservation: all prior claim IDs/statuses and acceptance strength are retained; full
  old prose, source manifests and freeze/attempt history remain at `1bfcf283417d63ae407bc66fc9950a90a68dd5b5:.de67/DFS.md` and the
  existing immutable package evidence. R-031–R-036 are fresh proof obligations, not retroactive
  rejection of R-027/R-028. No historical scenario, report, witness or durable state is replaced.
- Proof at refreeze: source inspection and 74 existing Python checks (73 pass; one source-inventory
  omission described in R-SURFACE-011). No gameplay campaign was run and no new gameplay acceptance
  is asserted. Source/path, stable-ID/status preservation, WEC identity and scoped-diff checks passed.
  Independent source reviews were resolved before freeze.

After freeze, named proof can close a red item without changing acceptance strength. Evidence may
clarify mechanisms only within the WEC. Product intent, vocabulary, required behavior and material
design alternatives remain user-owned. The WEC's promotion boundary applies to CAOL suspected bugs;
this DFS supplies no automatic gameplay-fix authority.

Owner-scoped refreeze 2026-09-07: same product outcome and stable claim/slice identities retained.
Inspected source HEAD `f4df8bf70e22bf2e1bd2f3beff264869ec26a40c`; gameplay source unchanged by this
review. Owner receipt `70c4632e512e` promotes the bounded repairs above and accepts the same-OMT
smoke exception. Existing accepted proof and full original observations remain intact; these are
repair/verification obligations, not completed gameplay results.
Current owner-contract SHA-256: `af6ce90990e4c5358aeedee16bd37ea9ebd979c3a8f2ff2aa874f234975a99df`.
