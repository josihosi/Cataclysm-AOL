# Semantic-Surface Cockpit DE-67 Functional Specification

Status: Refrozen
WEC: `.de67/WEC.md`
Source baseline: `Cataclysm-AOL-hostile-ecology-dev | dev | 050cff8c31274ea8dbf3d625baed10892a3e4395 | relevant inspected-source manifest SHA-256 55a4f6e8670241b3bfffad6ed52566c37358b86b8e70cfc903b3618c62122db1 | inspected 2026-08-31`

## Document authority

This document is the mechanistic product contract derived from the user-owned WEC and the inspected
production code. It replaces the prior hostile-ecology DFS while preserving only compatible,
code-grounded cockpit requirements and evidence. It is not a task-dispatch plan. If this document
conflicts with current code about what code does, re-inspect the code. If it conflicts with the WEC
about what the product should do, the WEC and the user win.

Status markers:

- `[x]` means the behavior is present in the production path with proportionate evidence.
- `[ ] 🔴 R-...` means the behavior is missing, wrong, or unproved. The stable red item is
  implementation work.

The source baseline includes pre-existing modifications to `src/handle_action.cpp`,
`tools/openclaw_harness/cockpit.py`, `tools/openclaw_harness/semantic_step_test.py`, and
`tools/openclaw_harness/startup_harness.py`. Their inspected worktree bytes, not only `HEAD`, are
bound by the manifest digest above. Unrelated dirty and untracked paths are outside this DFS.

## Functional contract

CDDA is the semantic authority. The CDDA input owner publishes one semantic surface for the frame
that currently owns input. The cockpit replaces its presentation with that frame, exposes only that
frame's valid actions, submits a namespaced semantic action with the exact frame ID, and receives a
receipt from the exact frame that consumed or rejected it.

```text
native input owner becomes active
-> CDDA publishes the top semantic surface and surface stack
-> cockpit presents only that surface and its valid actions
-> caller submits exact frame ID plus namespaced semantic action and stable target ID
-> the same native input owner consumes or rejects it
-> CDDA publishes an exact receipt and a fresh top frame
```

A child semantic surface hides all parent actions. An unsupported input-owning interface publishes
an unsupported semantic surface with no executable actions. That state stops automated play until
semantic support exists. The cockpit does not infer an action from a screen position, menu letter,
raw key, Escape, OCR, or a parent surface.

The smallest useful vertical slice is native `world.wait` opening the duration menu, the menu
exposing stable duration choices, a duration action being consumed by that exact child frame, and
the cockpit returning to a fresh world frame. This route proves the common stack, action, receipt,
and projection rules. It does not limit delivery to the wait menu or overmap.

## Project language and terminology

The canonical terms are **semantic surface**, **input owner**, **surface stack**, **frame ID**,
**stable ID**, **valid actions**, **receipt**, and **native binding**. The implementation and tests
must use those terms for the corresponding concepts.

Actions are namespaced by the owning surface, for example `world.wait`, `overmap.close`,
`inventory.apply`, `dialogue.choose`, and `menu.cancel`. A native keybinding, menu letter, list
index, pixel position, and rendered coordinate are not stable IDs. “Current screen,” “OCR mode,”
and “key fallback” are not synonyms for semantic surface support.

## Current code map

| Concern | Files and symbols | Current production behavior | Evidence |
|---|---|---|---|
| Focused semantic frames | `src/handle_action.cpp :: openclaw_harness_semantic_step_frame`, `openclaw_harness_semantic_step_receipt` | Emits run-bound JSON frames and receipts for world wait, duration menus, wait activity, movement, and selected interruption paths. It serializes private `action_inputs`. | Source inspection and existing native run `20260826_135902` show `world.wait -> wait.duration_menu -> wait.1m`, minute `8904 -> 8905`, and a fresh frame. |
| Current world-owner detection | `src/handle_action.cpp :: openclaw_harness_semantic_initial_world_frame_if_ready`, `game::draw`; `src/input_context.cpp :: input_context::get_active_context` | `game::draw` constructs a `DEFAULTMODE` activation before publishing. The active-context stack exists only for Tiles/Android builds. It is not renderer-neutral input-owner truth. | Compile guards in `input_context` and the draw call path. |
| Current semantic dispatch | `tools/openclaw_harness/semantic_broker.py :: SemanticStepChannel.act_observed`; `startup_harness.py :: dispatch_semantic_input` | Python resolves `action_inputs` and injects a physical key through the foreground UI. CDDA does not receive a semantic request or own the mapping. | Direct call path from frame parser to Peekaboo input. |
| Cockpit transaction guard | `tools/openclaw_harness/cockpit.py :: CockpitRunChannel.observe`, `act` | Enforces one-use observations, advertised actions, current-frame identity, exact receipt matching, and a fresh next frame. | `cockpit_observation_test.py` and source inspection. |
| Ordinary menu family | `src/uilist.h/.cpp :: uilist_entry`, `uilist::query`, `query_once`; `src/popup.h/.cpp :: query_popup`; `src/string_input_popup.h/.cpp` | Native controls own titles, text, choices, enabled state, selection, and logical actions, but expose no common semantic surface and no stable choice token. | Declarations and input loops. |
| Overmap | `src/overmap_ui.cpp :: overmap_ui::display` | `OVERMAP` owns origin, cursor, discovered terrain, selected location, route preview, and overmap actions. The current world-frame overmap preview is not this surface. | Display/input loop and registered actions. |
| Inventory selectors | `src/inventory_ui.h/.cpp :: inventory_selector`; `src/item.h :: item_uid`; `src/item_location.cpp` | Selector owns entries, highlight, selection, chosen count, details, and inventory actions. Items already have persistent UIDs, but no inventory semantic surface uses them. | Selector loop and `find_item_by_uid`. |
| Dialogue | `src/npctalk.cpp :: dialogue::opt`; `src/dialogue.h :: talk_response` | Dialogue owns speaker text, history, generated responses, hotkeys, trials, conditions, and effects. Responses have no stable semantic identity. | Response generation and choice loop. |
| Direction and targeting | `src/action.cpp :: choose_direction`; `src/ranged.cpp :: target_ui::run` | Each custom loop owns its cursor or candidate state. Direction uses the `DEFAULTMODE` category even while it, not world, owns input. | Native input loops. |
| Debug and map editors | `src/debug_menu.cpp`; `src/editmap.cpp`; `src/overmap_ui.cpp` | Ordinary debug menus use `uilist`; custom editors have additional input owners. No complete semantic coverage exists. | Construction and input call sites. |
| World messages | `src/messages.h/.cpp :: Messages::recent_messages` | CDDA owns structured recent messages. The current world semantic frame does not make this source a general active-surface field. | Declaration and implementation. |
| NPC LLM intent | `src/llm_intent.cpp` and its callers | Owns NPC request, parsing, target, and timing behavior. It is not the cockpit input owner. | Production call path and existing regression tests. |

## External research sweep

No external research was necessary. The user-owned WEC settled the vocabulary and safety boundary,
and the repository contained the relevant owners and precedent. No external claim alters this DFS.

## Mechanistic requirements

### 1. Semantic surface stack and frame identity

Mechanism:

- Files and symbols: add `src/semantic_surface.h` and `src/semantic_surface.cpp` with
  `semantic_surface_manager`, `semantic_surface_scope`, `semantic_surface_descriptor`,
  `semantic_action_request`, and `semantic_action_receipt`; connect the manager at the native input
  loops named by this DFS.
- Entry point: a native input owner constructs a `semantic_surface_scope` before its first frame can
  accept input and destroys it only after the owner yields or returns.
- Inputs: `surface_kind`, owner-provided breadcrumb label, structured surface state, stable entries,
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
  duplicate, missing, or malformed request is rejected with a receipt and changes no game state.
- Persistence/compatibility: surface IDs and frame IDs are runtime identities and are not save-game
  state. Each run starts a new identity domain. The mechanism is independent of Tiles, Android, and
  curses rendering.

Implementation status:

- [x] The focused wait prototype already emits run-bound frame IDs and exact receipts. This evidence
  proves the narrow identity precedent only.
- [ ] 🔴 R-001 — CDDA lacks one renderer-neutral semantic surface stack with top-owner exclusivity,
  fresh frame identity, breadcrumbs, and fail-closed unsupported ownership.
  - Code gap: `src/handle_action.cpp` emits selected frames, while
    `input_context::get_active_context` is Tiles/Android-only and `game::draw` creates a temporary
    `DEFAULTMODE` activation. Parent and unsupported-owner safety is not a common invariant.
  - Required mechanism: implement the manager and RAII scope above; require explicit scopes at
    supported native loops; publish an actionless unsupported scope for every other input owner.
  - Proof: nested world, inventory, prompt, and unsupported-owner tests show the exact surface stack,
    fresh push/pop frame IDs, no parent actions in children, and no executable action for unsupported
    ownership in Tiles and curses builds.

### 2. Native semantic request and receipt path

Mechanism:

- Files and symbols: extend `src/semantic_surface.h/.cpp`; integrate request delivery with
  `src/input_context.cpp :: input_context::handle_input`; replace the physical-dispatch branch in
  `tools/openclaw_harness/semantic_broker.py :: SemanticStepChannel` and
  `startup_harness.py :: dispatch_semantic_input`; keep `CockpitRunChannel` as the public
  transaction guard.
- Entry point: the cockpit submits `semantic_action_request { run_id, surface_id, frame_id,
  request_id, action_id, stable_id?, parameters? }` to the live CDDA process.
- Inputs: only fields advertised by the current descriptor are accepted. Private key sequences are
  not part of the descriptor or request.
- Preconditions: request run, surface, frame, action namespace, target stable ID, and parameter
  schema match the current top scope. The request ID has not been consumed.
- Transition: the top native owner resolves the semantic action through its registered native
  binding and invokes the same native behavior used by local input. It emits one accepted or
  rejected receipt before publishing the resulting fresh frame.
- Postconditions: the receipt contains request ID, run ID, consuming or rejecting surface ID and
  frame ID, action ID, accepted state, rejection reason when present, and resulting frame ID when a
  fresh frame exists.
- Failure behavior: transport loss does not synthesize acceptance. Duplicate request IDs return the
  recorded result without applying the action twice. No path translates the request to a keyboard
  event, Escape, menu letter, mouse coordinate, or screenshot-guided control.
- Persistence/compatibility: requests and receipts are run-scoped runtime records. Their protocol is
  common to graphical and terminal rendering. `llm_intent` remains a separate NPC policy system and
  cannot intercept or fabricate cockpit receipts.

Implementation status:

- [x] `CockpitRunChannel.act` already rejects unknown, used, unadvertised, stale, and receipt-mismatched
  actions and requires a fresh next frame.
- [ ] 🔴 R-002 — The production route still resolves semantic actions to physical keys outside CDDA
  instead of letting the exact native input owner consume them.
  - Code gap: `SemanticStepFrame.action_inputs`, `SemanticStepChannel.act_observed`, and
    `startup_harness.dispatch_semantic_input` make Python and foreground key injection the binding
    authority.
  - Required mechanism: deliver semantic requests to `semantic_surface_manager`; let only the top
    owner execute its registered binding; remove `action_inputs` from the public and executable
    route; preserve the cockpit transaction guards and exact receipts.
  - Proof: a native integration test changes a nondefault local keybinding, successfully performs
    the same semantic action without foreground focus, and rejects stale, duplicate, unadvertised,
    wrong-surface, and transport-interrupted requests without game-state change. The route contains
    no OS key injection.

### 3. Shared generic menu and prompt instrumentation

Mechanism:

- Files and symbols: extend `src/uilist.h/.cpp :: uilist_entry`, `uilist::query`, and
  `query_once`; extend `src/popup.h/.cpp :: query_popup`; extend
  `src/string_input_popup.h/.cpp`; use the common semantic surface manager.
- Entry point: each ordinary menu or prompt constructs its semantic scope from the same native data
  it renders and checks before its input loop.
- Parameters: each executable entry stores an opaque `stable_id` assigned when that entry object is
  created. It remains unchanged across filtering, sorting, scrolling, selection, and redraw. A
  caller may supply a domain ID. Otherwise the entry owns a scope-local opaque token; it is never
  derived from vector index, display order, hotkey, or coordinates.
- Inputs: menu title and text, entry label and description, enabled state, current selection, native
  logical action, prompt constraints, and text value come from the native control.
- Preconditions: only actions that the native control can execute in its current state are
  advertised. A callback-defined operation without an explicit semantic binding is visible but not
  executable.
- Transition: `menu.select`, `menu.choose`, `menu.cancel`, and prompt actions change native control
  state through the control's existing decision path. `menu.cancel` exists only when that owner
  supports cancellation. A text prompt accepts structured text through `prompt.submit`, subject to
  its native constraints.
- Postconditions: the next frame reflects selection, filtering, validation, or parent restoration.
  The exact child frame receipts the action.
- Failure behavior: disabled, missing, duplicate, stale, or constraint-invalid stable IDs are
  rejected. No universal meaning is assigned to Escape.
- Persistence/compatibility: generic IDs are stable for the life of the native surface. Domain IDs
  may be more durable. The same structured descriptor drives Tiles and curses presentations.

Implementation status:

- [ ] 🔴 R-003 — The ordinary `uilist`, confirmation/query popup, and string-input families do not
  publish one shared semantic shape or consume stable-ID semantic actions.
  - Code gap: current controls resolve hotkeys, indexes, and input events locally and expose no
    common descriptor or receipt.
  - Required mechanism: instrument the shared native controls as above so ordinary menu coverage
    grows through the common family rather than one OCR adapter per menu.
  - Proof: native tests cover enabled and disabled entries, duplicate labels, filter/reorder/redraw,
    confirmation, cancellation availability, text validation, nested prompts, and a real ordinary
    debug menu. Stable IDs survive presentation changes; parent actions remain absent.

### 4. World semantic surface

Mechanism:

- Files and symbols: move the world publisher from the draw-time
  `openclaw_harness_semantic_initial_world_frame_if_ready` path into the actual world input scope
  surrounding `game::get_player_input` and `game::handle_action`; reuse the inspected visibility,
  minimap, overmap-cell, entity, and zone builders in `src/handle_action.cpp`; read structured
  messages from `src/messages.h/.cpp`.
- Entry point: world input becomes top owner after load or after a child scope returns.
- Inputs: avatar state, local map, visible creatures, terrain, zones, messages available to the
  player, current world mode, and currently valid native world actions.
- Preconditions: world is the exact top owner; no activity prompt, inventory, dialogue, targeting,
  direction, overmap, or other child owns input.
- Transition: namespaced world actions invoke the matching logical game action. A native action that
  opens a child completes with a receipt from the world frame and a fresh child frame.
- Postconditions: the cockpit receives the local map and current world facts only while world owns
  input. Returning from a child produces a fresh world frame.
- Failure behavior: if world state cannot be built consistently, the owner publishes no executable
  actions. Messages are selected by the native displayed/history boundary, not an invented count.
- Persistence/compatibility: world observation does not mutate or persist state. It is renderer
  independent.

Implementation status:

- [x] The current focused frame already serializes avatar, visible local cells, entities, zones,
  minimap, overmap cells, and a small set of world actions.
- [ ] 🔴 R-004 — The world frame is not owned by the actual renderer-neutral world input loop and
  does not yet satisfy the full active-surface payload and native action rules.
  - Code gap: publishing is partly driven from `game::draw` with a constructed `DEFAULTMODE` scope;
    current actions expose private key inputs and current payload omits authoritative messages.
  - Required mechanism: bind publication and action consumption to the actual world owner, complete
    the payload from native sources, and yield entirely to every child scope.
  - Proof: Tiles and curses integration tests enter world, open and close multiple child owners, and
    show correct world facts/actions only before and after the child, with fresh frame IDs and exact
    receipts.

### 5. Overmap semantic surface

Mechanism:

- Files and symbols: instrument `src/overmap_ui.cpp :: overmap_ui::display` and its
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

Implementation status:

- [ ] 🔴 R-005 — The active overmap input owner has no focused semantic adapter.
  - Code gap: the current world observation's overmap cells are a world HUD preview and omit active
    overmap cursor, selected location, route state, actions, and receipts.
  - Required mechanism: add the focused scope and native bindings at `overmap_ui::display`.
  - Proof: a production overmap route moves its cursor, selects a discovered location, exercises one
    stateful overmap action, and closes; every action is frame-bound and no world action is exposed.
    An undiscovered-location control reveals no hidden terrain.

### 6. Inventory semantic surfaces

Mechanism:

- Files and symbols: instrument `src/inventory_ui.h/.cpp :: inventory_selector` and its derived
  selectors; use `src/item.h :: item_uid` and `src/item_location.cpp :: find_item_by_uid`.
- Entry point: each selector becomes a semantic scope before selection begins and supplies its
  purpose in the breadcrumb, such as `world › inventory` or `world › inventory › use lighter`.
- Inputs: entries, item details already available to the selector, enabled state, highlighted item,
  selected quantity/count, selector mode, and the exact inventory actions that mode accepts.
- Preconditions: item-backed entries resolve to the same live item UID and location under the
  selector's native validity rules. Non-item entries have explicit scope-local stable IDs.
- Transition: `inventory.select`, `inventory.set_quantity`, `inventory.apply`, and
  `inventory.cancel` call the selector's native selection and commit paths. The stable target ID,
  never an invlet or row, selects the entry.
- Postconditions: selection/detail state or the resulting child/parent surface appears in a fresh
  frame. The exact selector frame receipts the action.
- Failure behavior: moved, destroyed, merged, inaccessible, disabled, stale, or wrong-selector items
  are rejected and are not retargeted by name, invlet, display order, or coordinate.
- Persistence/compatibility: item identity stays owned by `item_uid` and existing save
  serialization. The semantic adapter creates no second item identity.

Implementation status:

- [x] Persistent item UIDs and UID lookup already provide the authoritative identity primitive.
- [ ] 🔴 R-006 — Inventory selectors do not publish item details/selection or consume stable-UID
  inventory actions.
  - Code gap: `inventory_selector` accepts local input and display positions; the cockpit has no
    inventory surface or receipt route.
  - Required mechanism: add the focused selector adapter and bind item entries to `item_uid`.
  - Proof: a real inventory route selects and applies one item by UID through nested prompts; reorder,
    duplicate-name, moved-item, destroyed-item, and stale-frame controls never retarget another item.

### 7. Dialogue semantic surface

Mechanism:

- Files and symbols: extend `src/dialogue.h :: talk_response` with a runtime `stable_id` assigned
  when the response is created; instrument `src/npctalk.cpp :: dialogue::opt` and response
  generation.
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

Implementation status:

- [ ] 🔴 R-007 — Dialogue responses lack stable IDs and the dialogue input owner lacks a semantic
  surface and native receipt route.
  - Code gap: `dialogue::opt` generates a vector, renders hotkeys, and applies the selected index.
  - Required mechanism: assign response-owned stable IDs and add the focused dialogue adapter.
  - Proof: a production conversation chooses a response among duplicate labels, advances topics,
    enters a nested confirmation when applicable, and returns exact receipts. Regeneration,
    wrong-speaker, disabled-condition, index, and hotkey controls fail closed.

### 8. Direction and targeting semantic surfaces

Mechanism:

- Files and symbols: instrument `src/action.cpp :: choose_direction` and
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

Implementation status:

- [ ] 🔴 R-008 — Direction and targeting custom owners are semantically invisible and can be
  misclassified as world or controlled only through raw input.
  - Code gap: `choose_direction` uses `DEFAULTMODE`; `target_ui::run` owns rich state but exposes no
    semantic descriptor or receipt.
  - Required mechanism: add explicit focused scopes and stable direction/coordinate/candidate
    bindings at the native loops.
  - Proof: production use-item direction and ranged-target routes expose only their child actions,
    choose valid targets, and reject hidden, moved, out-of-range, stale, and parent-world actions.

### 9. Broad input-owner coverage and hard stop

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
  only when their state/action model cannot be represented by the ordinary family.
- Transition: a supported owner publishes its semantic surface. An unimplemented owner publishes
  `unsupported` with breadcrumbs, diagnostics identifying the owner, and no valid actions.
- Postconditions: automated play either has exact semantic actions for the top owner or is stopped.
- Failure behavior: absence of an adapter never falls through to parent actions, raw input, OCR, or
  guessed cancellation.
- Persistence/compatibility: coverage is renderer neutral and includes useful debug and map-editor
  routes under their existing native authorization.

Implementation status:

- [ ] 🔴 R-009 — The repository has no enforceable coverage boundary for all discovered
  input-owning interfaces, including custom debug and map-editor owners.
  - Code gap: current traces and focused wait instrumentation cover selected interfaces; many direct
    input loops neither publish a supported surface nor force an unsupported hard stop.
  - Required mechanism: add the source coverage inventory/test, apply shared instrumentation to the
    ordinary family, add necessary focused adapters, and make every remaining owner actionless and
    unsupported.
  - Proof: source inventory and live traversal of discovered menu families show that every reached
    owner is supported or hard-stopped; injected unsupported-owner and nested-parent controls expose
    zero executable actions. No arbitrary interface count is an acceptance gate.

### 10. Cockpit active-surface projection and end-to-end proof

Mechanism:

- Files and symbols: update `tools/openclaw_harness/semantic_state.py`,
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

Implementation status:

- [x] The current cockpit enforces observation use, frame freshness, advertised-action membership,
  exact receipt matching, and a fresh next frame. The terminal cockpit is a structured projection of
  the same `CockpitService` route.
- [ ] 🔴 R-010 — The cockpit does not yet replace its active presentation across the required
  semantic surface families using a CDDA-native request route.
  - Code gap: current observation schema is world-centric, while modal handling still uses separate
    logs, OCR, or physical dispatch paths.
  - Required mechanism: project the common top descriptor without parent merging, remove executable
    raw-key fallback, render all supported families and unsupported stop state, and preserve exact
    transaction guards.
  - Proof: one bound native run traverses
    `world › inventory › use item › choose target › confirmation › world`, plus overmap and dialogue,
    and proves exact presentation replacement, breadcrumbs, stable IDs, valid-action isolation,
    accepted/rejected receipts, renderer parity, and unsupported hard stop.

## Competing systems and override direction

| State or action | Readers | Writers / competing owners | Authoritative decision |
|---|---|---|---|
| Active semantic surface | cockpit readers; terminal projection | native world loop, `uilist`, popup, inventory, dialogue, overmap, direction, target, editors | The top `semantic_surface_scope` alone owns truth. Push transfers authority to child; pop invalidates child and republishes parent with a fresh frame. Rendering and `input_context` category do not override it. |
| Frame and receipt identity | `CockpitRunChannel`, broker, evidence readers | semantic manager versus legacy debug-log parser | CDDA creates frame IDs and receipts. Consumers validate but never mint or repair them. Duplicate request IDs are idempotent; stale frames reject. |
| Semantic action binding | cockpit caller; native owner | Python `action_inputs`, OS key injection, local physical input | The exact top native owner is authoritative. Physical local input may still operate the game for a human but cannot count as or fabricate a semantic receipt. Python key translation yields and is removed from the executable cockpit route. |
| Menu choice identity | cockpit menu projection | `uilist` index/hotkey, callback, semantic stable ID | The entry-owned stable ID selects. Index, hotkey, screen position, and label may render but never bind an action. Unsupported callbacks are visible and disabled semantically. |
| Inventory item identity | inventory projection and action resolver | item UID, invlet, item name, item location hints | Existing `item_uid` is authoritative. Location is revalidated; no fallback retargets by invlet, name, row, or coordinate. |
| Dialogue response identity | dialogue projection and `dialogue::opt` | response vector position/hotkey and new response token | The response-owned stable ID is authoritative for the current dialogue scope. Conditions are rechecked immediately before effects. |
| Target identity | targeting projection | character ID, scope-local candidate token, cursor, name/coordinate guesses | Character ID or the target scope's opaque token for the same tracked creature is authoritative. Hidden/moved/stale targets reject rather than retarget. |
| Parent versus child actions | cockpit action list | cached world frame, child frame, legacy fallback | Child always wins while topmost. The parent remains breadcrumb context only and exposes no action or executable payload. |
| Unsupported owner | cockpit stop state | parent cache, raw key, OCR, guessed Escape | Unsupported is authoritative and actionless. It stops automation until a supported adapter exists. |
| NPC LLM intent | NPC behavior code and tests | `llm_intent`, cockpit semantic manager | `llm_intent` remains authoritative for NPC policy. It neither consumes cockpit actions nor changes semantic surface ownership. |

Ownership transfer is atomic at scope push and pop. The manager publishes the transferred owner and
frame before accepting its first semantic request. One request may be consumed once. A request never
replays against the next frame, even when the same action ID remains valid there.

## Acceptance and proof

For every red ID, proof has this shape:

```text
source-bound binary and fixture
-> authoritative native input owner
-> exact semantic request and state transition
-> exact receipt plus fresh top frame
-> immutable structured run artifact
-> positive and fail-closed controls pass
```

| Red ID | Outcome test | Required evidence | False-green controls |
|---|---|---|---|
| `R-001` | Nested owners push/pop one exclusive renderer-neutral stack. | Native unit/integration results and Tiles/curses descriptors with fresh IDs. | Draw-created world scope, cached parent actions, unsupported owner with any action, or reused frame ID fails. |
| `R-002` | Exact top owner consumes a semantic request without OS input. | Native request/receipt transcript bound to binary and run. | Key injection, changed keymap dependency, focus dependency, duplicate state change, or synthetic receipt fails. |
| `R-003` | Ordinary menus/prompts operate by stable entry ID across presentation changes. | Native menu-family test and real ordinary menu transcript. | Index, hotkey, label, disabled entry, or universal Escape success fails. |
| `R-004` | World publishes complete facts/actions only while world owns input. | Bound world/child/world transcript on Tiles and curses. | Draw-only publication, missing messages, parent action in child, or private key binding fails. |
| `R-005` | Active overmap cursor/selection/action/close route is semantic. | Native overmap transcript with discovered-state assertions. | World preview credited as overmap, hidden terrain leak, coordinate guess, or world action in overmap fails. |
| `R-006` | Inventory item is selected/applied by UID through nesting. | Native inventory transcript and item-state result. | Invlet, row, duplicate label, moved/destroyed UID retarget, or stale frame success fails. |
| `R-007` | Dialogue response is chosen by stable response ID and condition recheck. | Native dialogue transcript with topic/effect result. | Index/hotkey, regenerated token, wrong speaker, or disabled response success fails. |
| `R-008` | Direction and target owners expose their own candidates and consume valid choices. | Native use-item and ranged-target transcripts. | `DEFAULTMODE` treated as world, hidden/moved target retarget, screen coordinate, or parent action succeeds fails. |
| `R-009` | Every discovered/reached input owner is supported or actionless unsupported. | Source coverage inventory plus bound live traversal results. | Arbitrary menu quota, OCR adapter, parent fallback, raw key, or unclassified owner fails. |
| `R-010` | Cockpit replaces views through nested, overmap, inventory, dialogue, target, and unsupported transitions. | One source-bound end-to-end structured artifact plus renderer-equivalence assertions. | Merged parent data/actions, screenshot evidence alone, synthetic/mocked surface, or legacy dispatcher credit fails. |

The existing run `20260826_135902` is retained as focused evidence that the old wait prototype can
emit separate frames and exact receipts. It cannot close any broad red item because its action was
still resolved through the physical binding path and it did not prove generic ownership,
renderer-neutrality, broad surface coverage, or unsupported hard stop.

## Freeze record

- Status: Refrozen
- Frozen source baseline: `dev` at `050cff8c31274ea8dbf3d625baed10892a3e4395`, plus the
  inspected relevant-source worktree manifest SHA-256
  `55a4f6e8670241b3bfffad6ed52566c37358b86b8e70cfc903b3618c62122db1`, cross-checked
  2026-08-31.
- User-owned choices: CDDA is semantic authority; active presentation follows the input owner;
  child actions hide parent actions; actions and receipts bind exact frames; stable identities
  replace screen positions and menu letters; shared generic instrumentation and focused adapters use
  one protocol; coverage has no arbitrary menu count; unsupported owners stop automated play; raw-key
  and screenshot fallback are prohibited; Tiles and curses share the same surfaces.
- Evidence-implied refinements: the existing wait route is narrow precedent only; `item_uid` is the
  inventory identity primitive; current draw-time `DEFAULTMODE` activation cannot own semantic
  truth; current cockpit transaction guards remain useful; `llm_intent` is a separate protected
  policy owner.

After freeze, automation may close an existing red item only after its named proof passes and must
remove its red marker. It may make an evidence-implied nonmaterial clarification, or append only a
uniquely implied same-contract mechanism, ownership/proof detail, and necessary stable red claim
after a verified production finding. Existing claim identities, text, accepted work, and acceptance
strength remain fixed. Product intent, project language, permissions, user-visible behavior, and
materially different design choices return to DE-67-2 and the user.
