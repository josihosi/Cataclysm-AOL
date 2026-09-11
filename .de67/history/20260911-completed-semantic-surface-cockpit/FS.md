# Semantic-Surface Cockpit and Fresh CAOL Feature Package FS — Functional Specification

Status: Refrozen
WEC: `.de67/WEC.md`
Source baseline: `Cataclysm-AOL-hostile-ecology-dev | dev | 1bfcf283417d63ae407bc66fc9950a90a68dd5b5 | tree 1bbb62eebc15bc887c133b26055f8b5f8720c5ec | inspected 2026-09-06 | no tracked product changes; unrelated untracked run.witness.json excluded`

## Document authority

Owner refinement 2026-09-10: activation, sound and expanded E4B command testing are authorized
against inspected `dev` source at `1bc4925cf2da52883f4230254876b696996e4ad0`; red sections below are required work,
not implementation or gameplay acceptance. Earlier source-bound receipts retain their original identity.

One canonical `FS.md` describes code behavior: data, functions, ownership, interactions and errors. `DFS.md` is its hash-bound compatibility pointer; `DE67:DFS-SLICE` remains the stable selector syntax. `work-ledger.md` owns assignments and delivery state, projected from durable acceptance. Historical proof remains at its existing artifact, receipt or Git identity.

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


<!-- DE67:DFS-SLICE:BEGIN id=DE67-MAINT-CADENCE-S001 claim=DE67-MAINT-CADENCE -->

### Coordinator review cadence maintenance

- DE67-MAINT-CADENCE — Schedule periodic evidence-led workflow review after an inclusive
  20–50 completed worker windows under the existing once-per-terminal-attempt counting semantics.
  This owner refinement (`5e457436075e`, 2026-09-10) supersedes the temporary 10–20 range and
  introduces no worker cap. New cycles persist cadence version 3 and draw uniformly across both
  inclusive bounds. The next review chooses a useful improvement from current evidence; legacy
  random-lane metadata does not prescribe the inquiry.
- Preserve completed task history and the cumulative count. An old not-yet-due cycle keeps its
  cumulative start and clamps its saved interval into 20–50 once, without a new random draw.
  Already-due cycles (including an elapsed boundary without its due marker) and resolved cycles
  remain unchanged. Reopening preserves the resulting schedule. Storage continues to accept old
  interval/version values so migration does not rewrite historical proof or cancel a due review.
- Verify inclusive draw bounds, once-per-terminal-attempt counting, pending-cycle progress,
  due/resolved history, foreign-key integrity, and persistence across a fresh reopen. Leave active
  workers, product acceptance and the existing rare-review capability rules unchanged.

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
  loops named by this FS.
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

The final package evidence map also binds the newly authorized R-029 activation/sound results and
the R-037 local-LLM command coverage report, preserving each result's independent scope and limits.

<!-- DE67:DFS-SLICE:END id=R-026-S001 claim=R-026 -->

<!-- DE67:DFS-SLICE:BEGIN id=R-027-S001 claim=R-027 -->
Current obligation: R-033 requires fresh observation/control/world-boundary evidence and R-029
    requires the full natural signal/scout/report/response route. Historical green does not close them.

<!-- DE67:DFS-SLICE:END id=R-027-S001 claim=R-027 -->

<!-- DE67:DFS-SLICE:BEGIN id=R-028-S001 claim=R-028 -->
Current obligation: R-036 requires fresh integrated comparison with the qualified harness.

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

Owner refinement, gate `5a3101b86a3b` (2026-09-09): an otherwise ready cannibal raid leaves
after sundown, using the existing game-night predicate `is_night(calendar::turn)` rather than
a fixed clock hour or a promised travel duration. Night is an initial-departure condition only.
Once departed, it continues toward the target and attacks on arrival even after sunrise. Dawn
must not reset its phase, return it to rallying, clear its committed route, or make it wait for
another night. This commitment survives save/load and exact-world process replacement; unrelated
valid loss/return conditions retain their meaning.

The existing production route is `src/do_turn.cpp :: advance_live_bandit_hostile_approaches`,
called by `overmap_npc_move`. It already contains a night-gated `rallying -> approaching` call,
following abstract-owner, cursor, member, route and rally-position checks. The phase validator in
`src/bandit_live_world.cpp` also permits `waiting_night -> approaching`, but the live loop currently
selects only rallying/approaching operations. Required implementation must make the eligible
night departure and later approach/contact reachable through the existing authoritative route,
including any reachable waiting-night state, without introducing a second distance dispatcher.
The observed operation that stayed rallying overnight does not prove which prerequisite failed;
reconcile the complete caller chain and exact recorded state before selecting the repair.

Once departed, the persisted operation and physical route remain authoritative through dawn;
contact has no second night gate. Abstract/local handoffs retain site, generation, operation,
member IDs and route progress. Generic travel may not advance a concurrently locally owned actor.
Verification distinguishes pre-departure daylight holding, eligible night departure, and a route
that crosses dawn and still reaches attack, including reload continuity. Setup and a phase write
alone do not establish physical movement or attack. The separately promoted activation and sound repairs are specified in R-029-S002/S003.

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

<!-- DE67:DFS-SLICE:END id=R-029-S001 claim=R-029 -->

<!-- DE67:DFS-SLICE:BEGIN id=R-029-S002 claim=R-029 -->
#### 🔴 Authorized repair: active hostile contact and single movement ownership

R029-F004 is promoted to implementation and fresh native verification. In
`src/do_turn.cpp::materialize_committed_bandit_shakedown`, preserve the exact active hostile
reservation, operation/member IDs and shared NPC objects. Validate the complete party and empty
in-bounds placements before moving inactive members; place the party, then call canonical
`game::load_npcs()` once. Its tracker insertion and `on_load` handling remain authoritative;
manual tracker insertion, cloning NPCs or broad `reload_npcs()` are not the activation route.
Require every intended living member to be the same object, active, in the reality bubble and
present exactly once in the creature tracker before local parley, attack or stalking can proceed.
An already valid active party is idempotent: no re-placement or attitude reset. A missing member,
blocked placement or partial/rejected admission remains an explicit recoverable failure with no
local-interaction success credit; an inactive overmap record returned by `find_npc` is insufficient.
Use the same active-member predicate at downstream local-contact and sight-avoid gates.

Chosen movement arbitration: retain inherited strategic orders, but exclude exact IDs belonging
to an active `committed_contact` hostile reservation with `simulation_owner::local` from generic
`overmap_npc_move` travel. Derive the exclusion from current operation/owner identity, alongside
the existing abstract-approach exclusion; do not globally stop NPC AI or clear unrelated missions.
The exclusion ends on authoritative return/terminal handoff, so paid return and surviving members'
return routes continue normally. Keep the existing local stalking/hold-off motor and the rolling
ambush exception to normal first-contact negotiation. Normal parley and successful Pay must retain
their established protection against premature or renewed player-directed attacks.

Verification binds exact party pointers/IDs, active tracker membership and single movement owner,
including stale inherited travelling orders and repeated admission ticks. Fresh native routes
must establish a normal refusal and player-initiated attack against a real active target, an
attributable melee/ranged combat consequence where applicable, casualty/aftermath and surviving
return state, plus affected save/reload continuity. Include failed admission and unrelated-NPC
controls. The accepted rolling gate/control/reload and paid-return receipts remain valid at their
original ceilings; rerun only affected integration boundaries. Setup, an attack posture, target
selection, input acceptance and rendered text do not prove actual combat or aftermath.
<!-- DE67:DFS-SLICE:END id=R-029-S002 claim=R-029 -->

<!-- DE67:DFS-SLICE:BEGIN id=R-029-S003 claim=R-029 -->
#### 🔴 Authorized repair: fresh sound information versus physical investigation

R029-F002 now has a promoted semantic correction. A distant bang is fresh, uncertain information;
a site actually investigated is temporarily known. Keep the existing three-hour sound horizon
and six-hour physical-check cooldown. Correct their event semantics instead of making a detected
sound automatically compel a sortie or raising drive to bypass missing candidates.

In `record_staffed_camp_signal_observations`, a newly sensed sound lead uses the sound's emitted
minute for `first_seen_minutes`/`last_seen_minutes` and leaves `last_checked_minutes` and
`last_scouted_minutes` absent. When refreshing the same lead, preserve any existing physical-check
and scouting timestamps: distant information must neither reset, extend nor erase those clocks.
Apply that preservation before both unchanged-payload comparison and upsert, since upsert replaces
the lead. Re-reading the same event does not renew its lifetime or create revision churn; a genuinely
new sound can refresh the observation time. Preserve other signal channels' existing behavior.
Actual physical investigation/arrival or structural checks retain their existing timestamp writers.

Sound strength is positive only before emitted minute + 180, and unsupported evidence ages at that
boundary. A true check at T suppresses repeat investigation before T + 360, then permits ordinary
eligibility again. The normal five-minute cadence, readiness, pair, risk, route and mission-slot
requirements continue to decide whether an eligible lead is acted on. A favorable ready-camp
case must show that the corrected sound can reach candidate evaluation and the existing scouting
route before expiry; unfavorable cases may correctly decline with an attributable reason.

Existing saves may contain detection-written `last_checked_minutes` that cannot be distinguished
safely from an earlier real check after later observations. Preserve ambiguous historical stamps
until their finite cooldown expires; do not erase potentially real investigation evidence based
only on the last outcome label. New detections must not perpetuate that legacy delay. No new save
field or invented timer is required for this repair.

Verify event age 179/180, check age 359/360, repeated identical reads, a genuinely new sound,
refresh after a real investigation, and save/reload of new and legacy-stamped leads. Bind a fresh
source-bound sound -> memory -> eligible candidate -> ordinary scout/investigation consequence
under valid conditions, with an expired/cooldown control and realistic uncertainty in the result.
Report the first later rejecting gate if another premise blocks the route; detection, a score or
an injected lead alone does not prove investigation. Preserve accepted detection and signal controls.
<!-- DE67:DFS-SLICE:END id=R-029-S003 claim=R-029 -->


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


The owner accepts the established camp-craft gameplay result with the explicit historical
source-binding-file limitation described by receipt
`33656681a25465607762bc4efcf348ecd0dea6ff183a2a85888b673604254944`.
The missing `ff285f904d73126f5ec052fc5b16237612c7ce158e08dfd95dd2424a3d839c50`
bytes remain unavailable; the retained legacy file has a different hash. Future immutable receipt
archival is repaired. Acceptance must state that limit and must not fabricate provenance or replay
camp crafting solely to recover it. This exception does not waive new command-level local-LLM
proof required separately by R-037.

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

<!-- DE67:DFS-SLICE:END id=R-033-S001 claim=R-033 -->

### 18. Fresh persistence and continuation

<!-- DE67:DFS-SLICE:BEGIN id=R-034-S001 claim=R-034 -->

`savegame_json.cpp` writes NPC identity, rules, assignment, mission, inventory, patrol flag and
monster movement state. Camp policy, zones and `overmap_global_state` preserve their authoritative
stores; `site_record`, report/decision and hostile-operation serialization preserve the ecology
identity chain described in R-029. `PlayerClient.collect` recognizes the declared saved-world
continuation in a new process/generation; old frame grants are discarded and the fixture is not
reinstalled. Native saving, original-process exit, restored state and later behavior are distinct facts.

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

<!-- DE67:DFS-SLICE:END id=R-035-S001 claim=R-035 -->

### 20. Fresh integrated performance

<!-- DE67:DFS-SLICE:BEGIN id=R-036-S001 claim=R-036 -->

`ProcessPerformance`, `sample_owned_session` and `PlayerClient.performance` bind samples to process
identity, run and binding; PID reuse, session end and changed owners reject attribution. CPU is a
process-core percentage; a mixed-context interval is labeled mixed. `compare_records` uses an
explicit workload label but that label cannot prove comparability. Native renderer timing, camp
cadence counters and action completion measure different costs and must remain separate from
controller/bridge memory, retained evidence size and NPC-runner/model work.

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
- R-MAINT-COMPACT-IO — Existing build and native/query interfaces return useful bounded
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
- R-MAINT-RESULT-REUSE — Selected accepted facts and remaining boundaries enter the next
  prepared worker packet through the existing context tool, with immutable references and current
  source/evidence dependencies. Sol judges validity; tools do not invent acceptance or proof.
  - Proof: an isolated actual packet preserves generation-0 save/exit and assigns only the missing
    generation-1 continuation, retains independent contributions, rejects stale dependencies and
    replaces obsolete current conclusions without destroying their archived revisions. Demonstrate
    the interface's next relevant use without replaying accepted product evidence solely for adoption.
<!-- DE67:DFS-SLICE:END id=R-MAINT-RESULT-REUSE-S001 claim=R-MAINT-RESULT-REUSE -->

## Owner-authorized advisory integration — review d1cfc813605b

<!-- DE67:DFS-SLICE:BEGIN id=R-MAINT-CONSULT-S001 claim=R-MAINT-CONSULT -->
`integrations/openclaw_advisory/advisory_consult.py` sends a task-bound request through the
configured Gateway route and correlates its non-authoritative reply. The request binds current
coordinator ownership, workspace, lineage, run, task, assignment revision, sender, recipient and
correlation identity. Stale, rebound, duplicate, unavailable, echo and authority-escalation cases
must remain distinguishable. A transport acknowledgement or timeout leaves the request unresolved;
only a correlated reply changes its durable state to replied. Advice cannot promote the owner queue,
change acceptance, grant repair authority or impersonate an owner decision. The existing adapter's
focused tests and retained harmless round trip discriminate these boundaries without resending it.

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
Supervisor open-work/completion, provenance and every guard baseline resolve the same contract
and ledger. Legacy `selected_lane='DFS.md'` and path arguments retain their receipt identities while
validation inspects canonical FS content: a changed target with stale pointer fails, and a valid
pointer cannot hide protected content or slice changes. `coordinator_supervisor::_fresh_deadline_harness` loads the installed delivery writer for
post-review projection. An explicitly authorized service restart may create a new runtime epoch;
completed work and evidence retain their original identities. Runtime PID and journal-owner
continuity are not functional requirements. Migrate status baselines without resetting proof or clocks. Extract behavior embedded in old status blocks into its existing
slice before relocating tracking; retaining an empty slice ID alone does not preserve its contract.

Prove on isolated copies: ordered multi-slice dispatch and independent same-claim assignments;
old references/issued packets unchanged; missing/conflicting/cross-claim rejection; durable accept
and reopen with a functional-only FS, including the current COMPACT-IO and RESULT-REUSE status-block
counterexamples; supervisor completion/open-work and old receipt/provenance compatibility. Verify
the migrated functional content actually reaches packet callers. Current consumer references are in
`state/review-incident-prompt-delivery-20260908/fs-consumers.md`. This is an owner-authorized
representation migration, not authority to weaken evidence, change product outcomes or launch a process.

<!-- DE67:DFS-SLICE:END id=R-MAINT-FS-MIGRATION-S001 claim=R-MAINT-FS-MIGRATION -->


<!-- DE67:DFS-SLICE:BEGIN id=R-MAINT-REVIEW-CONTEXT-S001 claim=R-MAINT-REVIEW-CONTEXT -->
### Shared mutator conversation

Owner-directed undo, 2026-09-09: this section supersedes the separate-review-context
experiment. Its prior receipts remain historical evidence, not a requirement to restore it.

With `persistent_mutator` enabled, `codex_app_server_runner.py::run` resumes the same
`MutatorSession.thread_id()` for owner messages and supervisor mutation reviews. The existing
owner thread remains canonical when reading state from the split-context version. Review
invocations carry their current instructions into that conversation; completed reviews remain
history. Thread selection does not require a gate ID or create a separate review conversation.

The existing workspace lock preserves one mutator invocation. Owner messages reach the active
mutator through the existing relay and mailbox interfaces, including during reviews. Pending
input, uncertain-delivery recovery, correlated replies, current mutation authority, and supervisor
restart ownership retain their existing semantics. Conversation continuity does not create
another mutation owner or replay a completed request.

Refine coordinator and worker context to support useful decisions and effective work. Keep
Josef's conversation in the mutator's context, including during reviews.

<!-- DE67:DFS-SLICE:END id=R-MAINT-REVIEW-CONTEXT-S001 claim=R-MAINT-REVIEW-CONTEXT -->


## Optional checkpoint command context

<!-- DE67:DFS-SLICE:BEGIN id=R-MAINT-CHECKPOINT-CONTEXT-S001 claim=R-MAINT-CHECKPOINT-CONTEXT -->

`work_context.py::context_view()` should make the supported checkpoint command discoverable for
one explicitly queried live task. From the same authoritative task/worker snapshot used for its
current bindings, expose an optional `deadline_harness.py checkpoint-worker` argument-array template
with the state, lineage, task and actual current worker filled in. Leave the descriptive kind and
evidence for the coordinator to supply. This is an available command, never a required next action
or a grant of authority. An unknown, terminal or unclaimed task must not acquire an invented worker
binding. Retrieval performs no checkpoint or delivery-state transition.

Execution retains the existing task/worker ownership check, so a template retrieved before ownership
changes is rejected when stale. Integrate the concise handle into the existing context response;
no duplicate prompt syntax, new receipt, universal command framework or checkpoint requirement.
Verify exact emitted bindings, absence of a live-worker template when inapplicable, no retrieval
mutation of authoritative state, and stale-template rejection. The owner proposal's rejected and
corrected task-064 commands provide the before-change counterexample; later natural use establishes
whether corrective exchanges decrease, not a synthetic gameplay replay.

<!-- DE67:DFS-SLICE:END id=R-MAINT-CHECKPOINT-CONTEXT-S001 claim=R-MAINT-CHECKPOINT-CONTEXT -->


<!-- DE67:DFS-SLICE:BEGIN id=R-037-S001 claim=R-037 -->
### 🔴 R-037 — Complete supported NPC command coverage through the local LLM

Establish a source-bound coverage manifest for every supported primary intent and secondary action
in `src/llm_intent.cpp::allowed_actions` / `parse_csv_payload`, the prompt templates, and their
actual `npcmove.cpp` consumers. The current primary catalog is `wait_here`, `hold_position`,
`follow_close`, `follow_far`, `equip_gun`, `equip_melee`, `equip_bow`, `panic_on`, `panic_off`,
`look_around`, `look_inventory`, `idle`, `attack=<target>`, and `move=<dx>,<dy>` with wait/hold
arrival behavior. The inventory-selection route additionally supports wear, wield, activate and
drop. Reconcile any source changes or discrepancies before testing; do not silently omit a command
because a UI menu or the queued-action enum lacks it. Ambient speech and camp-request routes retain
their separate existing receipts and are not substitutes for this command catalog.

Use E4B through the actual local Ollama/model runner as the primary playtest model, recording
exact model/quantization identity, prompt and
configuration, request/recipient IDs, returned text, parse result, applying native turn and concrete
consequence. Use controlled, appropriate scenarios and natural direct requests; do not inject or
replace model output, force a parser result or promise deterministic model behavior. Distinguish an
LLM choosing the wrong action from parser rejection, dispatch deferral and native execution failure.
Existing direct-state/unit tests and successful generated replies are useful controls, not end-to-end
command proof. On a failed NPC-LLM playtest, record the observed failure in the bug list with its
exact actor, request, model, intended/observed behavior, source/run and first known divergence;
then retry the same bug case through the existing OpenAI API route using `gpt-4.1-mini`, the existing runner's low-cost non-reasoning default. Preserve equivalent native premises and the prompt's semantic
content, recording any necessary provider-format differences. Both primary and secondary requests
must use the selected provider for that comparison. Fix a broken test setup enough to reach the
intended route before claiming a model comparison; if the API route or credentials are unavailable,
retain the precise setup blocker and original failure without claiming the retry ran.

Configure the existing API backend/provider/model fields rather than changing the response contract:
`LLM_INTENT_BACKEND=api`, provider `openai`, and `LLM_INTENT_API_MODEL=gpt-4.1-mini`.
First verify the selected API Python imports `any_llm`, the child receives a configured credential
without printing it, and a bounded runner request returns the expected response shape. This is
transport setup only; the failed case still requires its native retry. Use the existing secret
lookup through the configured environment-variable name or secure store; do not unload the owner's
Ollama server as an API cleanup side effect.

Use the owner's existing API-key configuration or standard runtime secret environment; keep secret
values out of prompts, terminal output, checked-in files and evidence logs. The owner authorizes
these low-cost failure-comparison calls. Record actual API model/configuration, usage and resulting
native consequence. An API success narrows the investigation but does not erase the E4B failure or
count as a local-model pass; failure on both routes may indicate an integration/game issue and must
retain its causal uncertainty. API control and local verdicts remain independently visible.

The coverage manifest must provide a verdict and exact evidence handle for each command and each
material execution branch: both follow distances; wait versus hold and release/arrival; gun, melee
and existing silent/ranged equipment preferences (`equip_bow`) with appropriate inventory; panic on/off across turns; look-around selection
through actual ground/item pickup; inventory wear/wield/activate/drop; dynamic movement through
path progress and wait/hold arrival; and an attack against the named target through native melee
and ranged engagement/consequence. `idle` means no injected intent and continued ordinary AI,
not an invented action. Include valid and missing/stale target or item cases where they change
behavior, plus relevant ally eligibility, danger, mission/Patrol and other movement-owner controls.
Owner clarification 2026-09-11: `equip_bow` uses the game's existing silent/ranged weapon
preferences and ordinary eligibility/ranking/fallback. It does not guarantee an exact bow or
crossbow, override normal weapon choice, or imply a shot. Bows are internally `GUN` items;
that type and selector name do not mean firearms-only. Retaining another weapon is not by itself
a defect. Preserve the actual model output, native selection and consequence as separate facts.

Melee equipment selection alone does not prove a melee attack. Record damage or another attributable
native combat outcome, rather than inferring a hit from posture or a model promise.

Reuse valid current-source proof only for the precise command, model route and conditions it
establishes. Group compatible cases in isolated native scenarios without imposing a worker count,
fixed retry budget or one scenario per command. Repair repository-owned observation/fixture routes
when needed; preserve all failed outputs and source-bound contradictions. Any newly discovered
unpromoted gameplay behavior defect remains an explicit finding with its causal path. Do not remove
commands, narrow intended behavior or relabel failures to obtain a green coverage report. Preserve
process ownership and exact cleanup, and keep fixture preparation/OCR/rendered text at zero proof
credit. Stateful consequences use the existing persistence contract; transient LLM queues are not
made durable by this testing requirement.

Earlier E2B/E4B qualification proves local runner operation at its stated ceilings, not a quality
ranking or full command coverage. `tools/openclaw_harness/QUALIFICATION.md` and the September 5–6
E2B nonthinking/E4B runner artifacts retain that distinction. No matched comparative verdict is
currently established; the expanded command report must not imply model superiority from different
scenarios or cold-versus-warm timings. Record useful behavioral quality, grounding and latency
observations with their actual sample limits.
<!-- DE67:DFS-SLICE:END id=R-037-S001 claim=R-037 -->
