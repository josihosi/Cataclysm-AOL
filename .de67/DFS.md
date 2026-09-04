# Semantic-Surface Cockpit and CAOL Feature Package DE-67 Functional Specification

Status: Refrozen
WEC: `.de67/WEC.md`
Source baseline: `Cataclysm-AOL-hostile-ecology-dev | dev | c6616640a768625ce2673de56f336ae387ee0fc9 | upstream/master d40cebf345cf5f042e847570d3a12ab014e6ce11 | relevant semantic/package worktree manifest SHA-256 0a51ebb4559948eba1e4ff9bd3b0175caaff5935dbbabebc4e319f6723364810 | hostile-ecology refinement manifest SHA-256 615d2333a71afb2cc8b5231638fde666c7b72864638fd750d0ee0da2d4517a38 | re-inspected 2026-09-03`

## Document authority

This document is the mechanistic product contract derived from the user-owned WEC, the durable
owner-suggestion ledger, and the inspected production code. It composes the accepted semantic
cockpit with the compatible unfinished CAOL feature package: an enabling refreeze may compact
accepted footing, but it does not become a delivery ceiling or erase an owner-authorized product
outcome that remains unproved. It is not a task-dispatch plan. If this document conflicts with
current code about what code does, re-inspect the code. If it conflicts with the WEC or a scoped
owner mutation about what the product should do, the user wins.

Status markers:

- `[x]` means the behavior is present in the production path with proportionate evidence.
- `[ ] 🔴 R-...` means the behavior is missing, wrong, or unproved. The stable red item is
  implementation work.

The source baseline includes the inspected worktree bytes for the semantic-surface core, LLM-intent
and hostile-camp sources, cockpit and registry tools, hostile-camp benchmark, the named package
scenarios, and the canonical established-base fixture manifests. Their worktree bytes, not only
`HEAD`, are bound by the manifest digest above. Unrelated dirty and untracked paths are outside this
DFS.

## Functional contract

CDDA is the semantic authority. The CDDA input owner publishes one semantic surface for the frame
that currently owns input. The cockpit replaces its presentation with that frame, exposes only that
frame's valid actions, submits a namespaced semantic action with the exact frame ID, and receives a
receipt from the exact frame that consumed or rejected it.

```text
native input owner becomes active
-> CDDA publishes the top semantic surface and surface stack
-> cockpit presents only that surface and its valid actions
-> caller submits exact frame ID plus a namespaced semantic action and any stable target ID or
   structured parameters that the advertised action schema requires
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

The downstream product slice uses that accepted operation layer in the audited established-base
world: one current-source package exercises living NPC/camp behavior and hostile-ecology causation,
keeps independent verdicts for each behavior, and leaves reusable mechanical and gameplay-feel
evidence. The cockpit may operate and observe the route, but semantic transport success cannot stand
in for the gameplay transition being claimed.

Within that package, bandit and cannibal camps share one production route from physical signal to
staffed observation, camp memory, scout investigation and report, authorized response, and physical
travel. The report may choose distinct policies. A cannibal night raid waits at the rally until
night and remains committed after departure. A normal bandit contact reserves a shakedown before
generic hostility can attack; payment reserves a safe departure, while refusal, incomplete payment,
or player attack releases the group to combat. A favorable rolling-travel ambush remains a separate
accepted direct-combat branch.

## Project language and terminology

The canonical terms are **semantic surface**, **input owner**, **surface stack**, **frame ID**,
**stable ID**, **valid actions**, **receipt**, and **native binding**. The implementation and tests
must use those terms for the corresponding concepts.

Actions are namespaced by the owning surface, for example `world.wait`, `overmap.close`,
`inventory.apply`, `dialogue.choose`, and `menu.cancel`. A native keybinding, menu letter, list
index, pixel position, and rendered coordinate are not stable IDs. “Current screen,” “OCR mode,”
and “key fallback” are not synonyms for semantic surface support.

Action IDs listed in this document are representative native bindings, not exhaustive whitelists.
A supported semantic surface must expose every semantic operation that its adapter supports and the
current native owner permits. The valid actions must let automation navigate, complete, or yield
from every state that the adapter can reach. If a missing binding can trap automation in the native
owner, that owner is unsupported and must expose no executable actions until the binding exists.

For the hostile-ecology refinement, use **physical signal**, **staffed observer**, **camp memory**,
**scout report**, **night raid**, **shakedown**, **payment**, **refusal**, and **rolling-travel
ambush**. “Leaves at night” means departure from the rally is night-gated; it does not impose an
arrival-time night check. “Kills only after refusal” applies to normal shakedown contact and does
not prohibit the accepted rolling-travel ambush.

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
| Shared hostile-camp signal route | `src/do_turn.cpp :: live_bandit_staffed_camp_signal_reads`, `overmap_npc_move`; `src/bandit_live_world.cpp :: record_staffed_camp_signal_observations`, `advance_structural_bounty_maintenance`, `plan_hostile_operation_with_authorized_response` | A staffed observer can convert a physical signal into stable camp memory; ordinary maintenance can send a scout, accept a policy-bearing report, authorize a bandit shakedown or cannibal night raid, and dispatch physical travel. The route is profile-neutral until report policy is applied. | Current dirty source inspection plus accepted R-027 signal-memory and ordinary-response evidence. |
| Cannibal signal shortcut | `src/do_turn.cpp :: dispatch_live_cannibal_signal_contacts`, `overmap_npc_move` | On dispatch cadence, a nearby signal can directly plan and apply a cannibal site dispatch without staffed observation, camp memory, scout travel, or report acceptance. | Direct production call path at `overmap_npc_move`; this conflicts with the additive WEC. |
| Night-raid departure | `src/do_turn.cpp :: advance_live_bandit_hostile_approaches` | A cannibal raid in `rallying` waits until night before route assignment and transition to `approaching`. The later approach/contact path has no second night gate, so daylight after departure does not cancel the operation. | Source inspection and existing rally-at-night tests; dawn-before-contact remains unproved. |
| Shakedown and generic hostility | `src/do_turn.cpp :: open_live_bandit_shakedown_surface`, `live_bandit_prepare_paid_return`, `live_bandit_commit_paid_return`, `live_bandit_choose_fight`, `game::do_turn`; `src/bandit_live_world.cpp :: is_active_shakedown_parley_member`; `src/npc.cpp :: npc::guaranteed_hostile`; `src/npcmove.cpp :: npc::regen_ai_cache`, `npc::move` | Pay uses the native trade interface and atomically starts `returning_home`; Fight starts combat. However `game::do_turn` runs NPC movement before the periodic aftermath path opens the demand, and generic faction hostility can populate the player as hostile or set `NPCATT_KILL`. The current parley exception protects an allied patrol's view of a shakedown member, not the member's relationship to the player, and it ends before paid departure completes. | Source ordering and hostility predicates; focused existing demand/payment scenarios begin from prepared contact and do not prove natural first-contact or paid-departure safety. |

## External research sweep

No external research was necessary. The user-owned WEC settled the vocabulary and safety boundary,
and the repository contained the relevant owners and precedent. No external claim alters this DFS.

## Mechanistic requirements

### 1. Semantic surface stack and frame identity

<!-- DE67:DFS-SLICE:BEGIN id=R-SURFACE-001-S001 claim=R-SURFACE-001 -->

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

<!-- DE67:DELIVERY-STATUS:BEGIN claim=R-SURFACE-001 -->
- [x] R-SURFACE-001 — The live Tiles and curses routes prove the renderer-neutral semantic surface stack, exact top-owner
  exclusivity, fresh frame IDs, breadcrumbs, and actionless unsupported hard stop through the live
  Tiles and curses routes.
  - DFS slices: `R-SURFACE-001-S001`
  - Preserved implementation: commit `c6616640a7` contains the stack core. The shared worktree adds
    the game/run-scoped world lifetime, real wait/alarm child scopes, actionless unsupported scopes,
    native descriptor-bound requests and receipts, game-thread consumers, native wait bindings,
    wake-only delivery, and a curses PTY. Focused semantic, transport, and changed-object checks
    passed in the producing attempts; none of that evidence is promoted to live renderer proof.
  - Recovered footing: production `surface_descriptor` records are authoritative over later legacy
    frames, and source-bound fixture `r_surface_001_world_start_v1` has no persisted activity,
    auto-move route, active local monsters, or active-outside bandit owner. The completed 38
    semantic-step tests, three fixture contracts, install receipts, saved-state audit, and
    zero-credit ceiling remain preserved despite the deadline miss.
  - Current uncertainty: World and wait already work in both renderers. Source-bound Tiles run
    `20260901_013056_58fa9f4976d947428603007ca3e76c88` additionally proves Inventory, wet-flashlight
    Activate, the native YESNO child, safe YES, and restored item-menu ownership. The distinct curses
    build and terminal-native startup prove source, process, runtime, and World identity without GUI
    focus, but a matching `world.inventory` request still receives no native receipt because curses
    remains inside its blocking input wait. Signal, stdin-only polling, blocking `getch()`, FIFO,
    Unix-datagram, and loopback-TCP wake strategies are retired by fresh source-bound counterexamples.
    Closure-031 preserves three listeners published after `listen()`, three refused connections,
    the isolated rebuild, and 39 passing focused tests. Closure-032 replaced that failed network
    route with an anonymous pipe created before process launch. The new route passed 40 semantic-step
    tests, 6 launch-binding tests, and compilation of the changed curses object. The source-bound
    curses executable then failed to link because its existing object set referenced `play_music`,
    `TranslationManager`, and `sounds::sound_enabled` inconsistently. This recoverable build failure
    prevented the first live receipt, so it earns no parity credit. The next worker must rebuild a
    coherent source-bound curses executable, obtain one exact semantic receipt as a zero-credit
    bootstrap, and independently rerun the complete nested curses route against the preserved Tiles
    report. Physical keys, screenshot-guided input, and the non-prompt brazier-drop route remain
    outside proof.
  - Evidence locations: retained Tiles and curses footing is under
    `build_logs/r_surface_001_closure_010/`, `build_logs/r_surface_001_closure_011/`,
    `build_logs/r_surface_001_closure017/`, and the bound harness-run directories named in durable
    gap revisions. Closure-031 evidence is in its three `20260901_0418*` through `20260901_0420*`
    harness runs and the immutable worker finding.
  - Subtasks:
    - [done] preserve-common-surface-proof :: Keep the shared stack, matching World/wait/unsupported proof, native Inventory and prompt owners, stable item identity, exact receipts, and restored-parent behavior.
    - [done] preserve-tiles-nested-proof :: Keep the fresh source-bound Tiles Inventory, wet-flashlight, YESNO, safe-YES, and restored-menu transcript.
    - [done] preserve-curses-runtime-footing :: Keep the distinct curses build, terminal-native source/process/runtime/World identity, matching request, focused checks, and zero-credit ceiling.
    - [finding] retire-failed-wake-strategies :: Preserve the source-bound signal, stdin, blocking-getch, FIFO, Unix-datagram, and loopback-TCP counterexamples without treating any one failed strategy as an outcome exit.
    - [done] establish-curses-semantic-wake :: Keep the anonymous-pipe implementation and its 40 semantic-step tests, 6 launch-binding tests, and changed-object compile.
    - [done] repair-source-bound-curses-build :: Keep the coherent source-bound curses executable produced by closure-033.
    - [done] bootstrap-anonymous-pipe-receipt :: The rebuilt executable produced an accepted native world.inventory receipt through the inherited pipe.
    - [done] validate-and-judge-renderer-parity :: The fresh nested curses transcript matches the preserved Tiles evidence after excluding run-specific identities.
  - Acceptance: durable acceptance 1 uses closure-033 after every required gap in closure sequence 6 closed.
  - Final proof: `.userdata/r-surface-001-inventory-prompt-curses/harness_runs/20260901_050125_2b12535db356419b8761e63b5e18fb79/probe.report.json` is feature-path proof and 46 focused harness tests passed.
  - Durable acceptance: #1 via `R-SURFACE-001-closure-033`; SQLite evidence is authoritative.
<!-- DE67:DELIVERY-STATUS:END -->
<!-- DE67:DFS-SLICE:END id=R-SURFACE-001-S001 claim=R-SURFACE-001 -->

### 2. Native semantic request and receipt path

<!-- DE67:DFS-SLICE:BEGIN id=R-SURFACE-002-S001 claim=R-SURFACE-002 -->

Mechanism:

- Files and symbols: extend `src/semantic_surface.h/.cpp`; integrate request delivery with
  `src/input_context.cpp :: input_context::handle_input` and
  `src/input.h :: input_manager::get_input_event`; provide wake-only support in
  `src/sdltiles.cpp`, `src/wincurse.cpp`, and `src/ncurses_def.cpp`; replace the physical-dispatch
  branch in `tools/openclaw_harness/semantic_broker.py :: SemanticStepChannel` and
  `startup_harness.py :: dispatch_semantic_input`; keep `CockpitRunChannel` as the public transaction
  guard.
- Entry point: the cockpit submits `semantic_action_request { run_id, surface_id, frame_id,
  request_id, action_id, stable_id?, parameters? }` to the live CDDA process.
- Inputs: only fields advertised by the current descriptor are accepted. Private key sequences are
  not part of the descriptor or request.
- Preconditions: request run, surface, frame, action namespace, target stable ID, and parameter
  schema match the current top scope. The request ID has not been consumed.
- Transition: the top native owner resolves the semantic action through its registered native
  binding and invokes the same native behavior used by local input. It emits one accepted or
  rejected receipt before publishing the resulting fresh frame.
- Concurrency and ordering: `semantic_surface_manager::submit_request` validates the transport
  envelope and queues the request. Only the game-thread top scope validates or rejects the semantic
  action. Request arrival makes the queue observable to a blocking
  `input_manager::get_input_event` implementation but does not manufacture an `input_event`, action
  descriptor, key, mouse event, or timeout. The game thread asks the current top scope to consume the
  queued request before it processes another physical event. CDDA serializes request consumption,
  native state mutation, receipt emission, and successor frame publication in that order.
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

Implementation status:

<!-- DE67:DELIVERY-STATUS:BEGIN claim=R-SURFACE-002 -->
- [x] R-SURFACE-002 — The source-current native route proves that only the active input owner consumes semantic requests and returns exact receipts without keyboard, mouse, focus, or screenshot control.
  - DFS slices: `R-SURFACE-002-S001`
  - Known foundation: R-SURFACE-001 now provides the shared surface manager, live renderer parity, native descriptor-bound requests, exact receipts, and the anonymous-pipe wake route for curses.
  - Current uncertainty: a fresh source-current curses binary proves remapped no-focus execution, stale, unadvertised, wrong-surface, and interrupted transport without state change. Duplicate replay writes the repeated request and wake durably but does not emit the second identical receipt after Inventory opens. Timeout-zero polling, cached delivery, owner activation, and pre-block ingestion are disproved as complete fixes. The next strategy must instrument the actual ncurses `select` and wake branch during Inventory blocking read before choosing another repair.
  - Proof route: use the production cockpit request path with a nondefault local keybinding. Prove accepted native execution without foreground focus. Then prove stale, duplicate, unadvertised, wrong-surface, and transport-interrupted requests do not change game state and return the exact required result. The route must contain no OS key injection.
  - Subtasks:
    - [done] audit-native-request-route :: The audit identified backend-side request consumption and an executable physical-input fallback as the first divergences.
    - [done] restore-native-owner-consumption :: Renderer backends now ingest and wake only, while `input_context::handle_input()` lets the active top owner consume pending requests.
    - [done] remove-physical-dispatch-fallback :: Semantic frames no longer expose `action_inputs`, and legacy non-descriptor frames fail closed instead of invoking Peekaboo.
    - [done] prove-remapped-binding :: The source-current curses run accepted semantic Inventory after its physical binding changed to `z`, without foreground or physical input.
    - [done] prove-rejection-matrix :: The source-current matrix proves stale, duplicate, unadvertised, wrong-surface, and interrupted transport without unintended state change.
    - [done] verify-native-only-transport :: The descriptor route now uses only request JSONL and the inherited wake pipe, and focused Python and native tests pass.
    - [done] judge-r-surface-002 :: Closure tasks 001 through 003 independently closed every gap, and durable acceptance 1 settled the claim.
  - Acceptance: durable acceptance 1 uses closure-003 after all three gaps in closure sequence 2 closed.
  - Final proof: run `20260901_062147_6936ef4aad934189826b83b042bfb41a` is source-current feature-path proof and 40 focused semantic-step tests pass.
  - Durable acceptance: #1 via `R-SURFACE-002-closure-003`; SQLite evidence is authoritative.
<!-- DE67:DELIVERY-STATUS:END -->
<!-- DE67:DFS-SLICE:END id=R-SURFACE-002-S001 claim=R-SURFACE-002 -->

### 3. Shared generic menu and prompt instrumentation

<!-- DE67:DFS-SLICE:BEGIN id=R-SURFACE-003-S001 claim=R-SURFACE-003 -->

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

Implementation status:

<!-- DE67:DELIVERY-STATUS:BEGIN claim=R-SURFACE-003 -->
- [x] R-SURFACE-003 — The shared native menu and prompt family is proven on the accepted semantic request boundary.
  - DFS slices: `R-SURFACE-003-S001`
  - Known foundation: R-SURFACE-001 and R-SURFACE-002 now prove renderer-neutral ownership, native-only request delivery, exact receipts, and live Tiles and curses operation.
  - Current uncertainty: the rebuilt native suite passes 14 cases and 96 assertions. Fresh source-bound run `20260901_074656_d9d04d0153e74d93a643fee9d0bee4d5` proves World to Debug Functions to Game to Show debug message through stable IDs and exact native receipts. The final feature guard remains red only because the required native action deliberately logs `ERROR: Test debugmsg`, which the generic guard does not yet classify as an expected scenario outcome.
  - Subtasks:
    - [done] read-r-surface-003-contract :: The authorized slice defines uilist, query popup, string input, stable identity, failure behavior, nested ownership, and a real ordinary debug-menu proof.
    - [done] audit-menu-prompt-coverage :: The audit found partial popup work and no uilist or string-input semantic family.
    - [done] implement-missing-common-family :: Shared semantic wiring now exists for uilist, query popup, string input, and native debug-menu entry.
    - [done] prove-live-common-family :: The authoritative detached run proves the ordinary debug-menu chain through three exact native stable-ID receipts.
    - [done] judge-r-surface-003 :: Closure tasks 001 through 003 closed the native-family, live-menu, and fail-closed gaps before durable acceptance 1.
  - Acceptance: durable acceptance 1 uses closure-002 after all three gaps in closure sequence 2 closed.
  - Final proof: run `20260901_080338_00e8f9d5a1d847a8bbd15e9d55d3c8c4` is source-current feature proof, and the native suite passes 15 cases with 102 assertions.
  - Durable acceptance: #1 via `R-SURFACE-003-closure-002`; SQLite evidence is authoritative.
<!-- DE67:DELIVERY-STATUS:END -->
<!-- DE67:DFS-SLICE:END id=R-SURFACE-003-S001 claim=R-SURFACE-003 -->

### 4. World semantic surface

<!-- DE67:DFS-SLICE:BEGIN id=R-SURFACE-004-S001 claim=R-SURFACE-004 -->

Mechanism:

- Files and symbols: move the world publisher from the draw-time
  `openclaw_harness_semantic_initial_world_frame_if_ready` path into the actual world input scope
  surrounding `game::get_player_input` and `game::handle_action`; reuse the inspected visibility,
  minimap, overmap-cell, entity, and zone builders in `src/handle_action.cpp`; read structured
  messages from `src/messages.h/.cpp`.
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

Implementation status:

<!-- DE67:DELIVERY-STATUS:BEGIN claim=R-SURFACE-004 -->
- [x] R-SURFACE-004 — The live Tiles and curses routes prove the renderer-neutral World owner, complete native payload, child exclusivity, and exact child-return receipts.
  - DFS slices: `R-SURFACE-004-S001`
  - Known foundation: R-SURFACE-001 through R-SURFACE-003 now prove renderer-neutral ownership, native-only request delivery, exact receipts, and the common native menu and prompt family.
  - Final proof: closure-001 settles the World source contract. Closure-002 settles the Tiles child-return gap with source-bound run `20260901_084803_c48c48ab407b4bb2b4aae7a32ad3b867`. Closure-003 settles the matching curses child-return gap with source-bound run `20260901_085008_8c8797aa81674128abe4b32aae7b66ff`. Each renderer proves World, Inventory, fresh World, Debug menu, and fresh World through four exact receipts. Durable acceptance 1 settles the whole claim after all three gaps closed.
  - Subtasks:
    - [done] read-r-surface-004-contract :: The authorized slice defines the renderer-neutral World owner, complete native payload, logical actions, child yielding, and full retained messages.
    - [done] audit-focused-owner :: The audit found empty real-owner payload, four actions, a draw-time synthetic owner, and missing authoritative messages.
    - [done] implement-focused-surface :: The real owner now publishes the complete World payload and eight native logical actions, and the draw-time publisher is removed.
    - [done] prove-live-focused-surface :: The bounded trace is repaired, 40 tests pass, and source-bound Tiles and curses runs each prove the full child-return chain.
    - [done] close-tiles-child-return :: Keep the source-bound Tiles child-return transcript, four exact receipts, fresh World frames, complete retained messages, and no-overflow proof.
    - [done] close-curses-child-return :: Keep the source-bound curses child-return transcript, four exact receipts, fresh World frames, child exclusivity, and green feature-path proof.
    - [done] judge-r-surface-004 :: Durable acceptance 1 preserves all three closed gaps and settles the whole claim.
  - Durable acceptance: #1 via `R-SURFACE-004-closure-003`; SQLite evidence is authoritative.
<!-- DE67:DELIVERY-STATUS:END -->
<!-- DE67:DFS-SLICE:END id=R-SURFACE-004-S001 claim=R-SURFACE-004 -->

### 5. Overmap semantic surface

<!-- DE67:DFS-SLICE:BEGIN id=R-SURFACE-005-S001 claim=R-SURFACE-005 -->

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

<!-- DE67:DELIVERY-STATUS:BEGIN claim=R-SURFACE-005 -->
- [x] R-SURFACE-005 — The live Tiles and curses routes prove the renderer-neutral Overmap owner, discovered-only payload, stable targets, exact native receipts, and fail-closed hidden terrain.
  - DFS slices: `R-SURFACE-005-S001`
  - Known foundation: R-SURFACE-001 through R-SURFACE-004 prove renderer-neutral ownership, native-only requests, exact receipts, the common menu family, and live World child-return behavior.
  - Final proof: The source contract, Tiles route, and curses route are independently closed. Tiles run `20260901_092728_1c7efc46c4904460a53759d6ec17f664` and curses run `20260901_094019_a6462e09cd8f4481abc4400634209a9c` prove matching native Overmap behavior, exact receipts, hidden-terrain omission, and fresh World restoration. Durable acceptance 1 settles the whole claim after all three gaps closed.
  - Subtasks:
    - [done] read-r-surface-005-contract :: The frozen slice names `overmap_ui::display`, discovered-only state, stable targets, current native actions, exact receipts, child exclusivity, and fail-closed controls.
    - [finding] retire-worker-route-decision :: Exploration-001 stopped after an ordinary worker ran the coordinator policy. Preserve this process-boundary finding without treating it as a product outcome.
    - [done] audit-focused-owner :: The audit found no semantic scope at `overmap_ui::display` and no native World-to-Overmap entry binding.
    - [done] implement-focused-surface :: The active owner now publishes discovered-only state and stable targets and consumes native overmap actions with fail-closed validation.
    - [done] prove-live-tiles-surface :: The fresh Tiles route proves native entry, cursor and destination behavior, hidden-terrain omission, exact receipts, close, and restored World.
    - [done] close-overmap-source-contract :: The active loop owns publication and consumption, exposes discovered stable targets, hides World actions, and rejects invalid requests before movement.
    - [done] close-tiles-overmap-route :: The source-bound Tiles transcript proves the native route, exact receipts, hidden-terrain omission, and fresh World restoration.
    - [done] prove-curses-overmap-route :: The isolated source-bound curses run proves matching native behavior, child exclusivity, hidden-terrain omission, and fresh World restoration.
    - [done] judge-r-surface-005 :: Durable acceptance 1 preserves all three closed gaps and settles the whole claim.
  - Durable acceptance: #1 via `R-SURFACE-005-closure-003`; SQLite evidence is authoritative.
<!-- DE67:DELIVERY-STATUS:END -->
<!-- DE67:DFS-SLICE:END id=R-SURFACE-005-S001 claim=R-SURFACE-005 -->

### 6. Inventory semantic surfaces

<!-- DE67:DFS-SLICE:BEGIN id=R-SURFACE-006-S001 claim=R-SURFACE-006 -->

Mechanism:

- Files and symbols: instrument `src/inventory_ui.h/.cpp :: inventory_selector` and its derived
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
- Persistence/compatibility: item identity stays owned by `item_uid` and existing save
  serialization. The semantic adapter creates no second item identity.

Implementation status:

<!-- DE67:DELIVERY-STATUS:BEGIN claim=R-SURFACE-006 -->
- [x] R-SURFACE-006 — The live Tiles and curses routes prove the renderer-neutral inventory-selector family, stable UID targeting, nested ownership, exact receipts, and fail-closed invalid identities.
  - DFS slices: `R-SURFACE-006-S001`
  - Known foundation: R-SURFACE-001 through R-SURFACE-005 prove renderer-neutral ownership, native-only requests, exact receipts, common prompts, World, and Overmap. Persistent item UIDs already provide the authoritative item identity primitive.
  - Final proof: All four closure gaps are independently closed. Current-source tests prove exact UID behavior, collated same-purpose entries, disabled actions, wrong identity rejection, nested restoration, and successor receipts. The curses and Tiles routes canonically ingest with mechanically valid witness evidence and accepted cleanup. Durable acceptance 1 settles the whole claim.
  - Subtasks:
    - [done] read-r-surface-006-contract :: The contract requires stable UID selection, complete mode-valid actions, nested ownership, exact receipts, and fail-closed stale or moved controls.
    - [done] audit-inventory-selectors :: The audit classified pick, container, unload, ammo, multiselect, compare, drop, insert, pickup, wield, wear, examiner, and trade families.
    - [done] harden-pick-uid-selection :: Pick selection now uses authoritative UID lookup and rejects a target that no longer equals the current selector entry.
    - [finding] retire-partial-selector-strategy :: Exploration-001 stopped after one safety repair. Preserve its evidence without treating missing repository work as a product finding.
    - [done] implement-shared-inventory-adapter :: Pick, ammo, and base multiselect now share UID-backed common and mode-specific actions with exact current-entry validation.
    - [finding] retire-base-selector-strategy :: Exploration-002 stopped before derived-loop integration and live proof. Preserve its implementation without treating the remaining work as a product finding.
    - [done] implement-derived-inventory-surfaces :: Compare, drop, insert, pickup, wield, wear, examiner, unload, contain, and trade now use the shared UID-backed semantic adapter.
    - [finding] retire-derived-selector-strategy :: Exploration-003 stopped after derived integration and before executable proof. Preserve its implementation and successful object build.
    - [done] add-uid-selector-test :: The focused native test covers UID action advertisement, native selection mapping, item removal, and stale-UID rejection without native input.
    - [finding] retire-broad-test-build-strategy :: Exploration-004 stopped during an unrelated broad stale-object rebuild before the focused test executed.
    - [done] execute-uid-selector-test :: The isolated current-source selector test passes 10 assertions for UID advertisement, native selection, removal, and stale rejection.
    - [finding] retire-aggregated-duplicate-fixture :: Two planks merge into one native selector entry, so that fixture cannot prove duplicate-entry identity.
    - [done] prove-distinct-nested-selector-controls :: Two non-aggregated child items prove UID controls, details, contents, filter, reset, cancel, and removed-item rejection in 39 assertions.
    - [done] create-inventory-proof-fixture :: The zero-credit inventory fixture and active Tiles scenario are admitted by the registry.
    - [finding] retire-missing-charter-strategy :: Exploration-006 stopped when the admitted scenario lacked a validated witness charter and launch token.
    - [done] establish-inventory-launch-authority :: The validated charter, first-run binding, structured parameters, and source-bound Tiles build now produce an executable route.
    - [done] prove-live-filter-reset :: The live Tiles selector accepts filter and reset with exact same-selector receipts.
    - [finding] expose-details-receipt-defect :: The UID details action reaches the native route but produces no receipt when the nested item-info child opens.
    - [done] repair-details-child-receipts :: Details now receipts into an exclusive item-info child whose semantic close restores Inventory with exact succession.
    - [done] prove-live-nested-inventory-route :: Live Tiles evidence covers filter, reset, details, contents, invalid UID rejection, and derived item-menu entry.
    - [finding] expose-reader-event-cap :: The repaired live route reached the separate 64-event reader cap before final report sealing.
    - [done] seal-bounded-live-report :: The fresh bounded route exercises the suffix reader and preserves the successful Inventory and child transitions with accepted cleanup.
    - [finding] expose-derived-menu-owner-divergence :: The advertised derived menu frame is stale while a different native surface owns input, so close fails safely as wrong-surface.
    - [done] repair-derived-menu-owner-divergence :: The item-aware scope remains the native owner while its uilist renders without publishing a competing generic menu scope.
    - [done] prove-live-inventory-family :: Live Tiles evidence covers common actions, nested children, invalid UID rejection, derived item-menu entry and cancel, and exact restoration receipts.
    - [done] close-inventory-source-family :: Shared and derived modes use authoritative live UID validation, native enabled state, and active-owner-only publication.
    - [done] close-inventory-stable-controls :: Current-source tests prove distinct collated UIDs, exact targeting, invalid identity rejection, disabled actions, nested restoration, and successor receipts.
    - [finding] retire-unbound-tiles-run :: The claimed Tiles repair run has empty transition events and cannot bind targets or receipts to its run and steps.
    - [done] close-tiles-inventory-family :: The fresh source-bound Tiles route proves the supported selector family with exact run and step-bound evidence and accepted cleanup.
    - [done] close-curses-inventory-family :: The isolated source-bound curses route proves native UID actions, canonical ingestion, mechanically valid witness evidence, and accepted cleanup.
    - [done] judge-r-surface-006 :: Durable acceptance 1 preserves all four closed gaps and settles the whole claim.
  - Durable acceptance: #1 via `R-SURFACE-006-closure-009`; SQLite evidence is authoritative.
<!-- DE67:DELIVERY-STATUS:END -->
<!-- DE67:DFS-SLICE:END id=R-SURFACE-006-S001 claim=R-SURFACE-006 -->

### 7. Dialogue semantic surface

<!-- DE67:DFS-SLICE:BEGIN id=R-SURFACE-007-S001 claim=R-SURFACE-007 -->

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

<!-- DE67:DELIVERY-STATUS:BEGIN claim=R-SURFACE-007 -->
- [x] R-SURFACE-007 — The focused dialogue-response surface is proven on the accepted native ownership and stable-target foundations.
  - DFS slices: `R-SURFACE-007-S001`
  - Known foundation: R-SURFACE-001 through R-SURFACE-006 prove renderer-neutral ownership, native-only requests, exact receipts, shared prompt families, World, Overmap, and stable-UID inventory selectors.
  - Final proof: Source controls, the Tiles route, and the curses route are independently closed. Durable acceptance 1 uses closure-003 after all three sequence-2 gaps closed.
  - Subtasks:
    - [done] read-r-surface-007-contract :: The contract requires response-owned runtime IDs, native dialogue ownership, condition rechecks, exact receipts, and fail-closed stale or wrong-speaker choices.
    - [done] audit-dialogue-owner :: The audit found hotkey and index selection with no semantic scope or stable response identity.
    - [done] implement-dialogue-surface :: Generated responses now own stable IDs and the native dialogue loop publishes and consumes stable-ID actions with condition and speaker validation.
    - [finding] retire-implementation-only-strategy :: Exploration-001 stopped after object compilation and before executable tests or production transcripts.
    - [done] establish-dialogue-proof-bootstrap :: Current Tiles, automatic-dialogue fixture, scenario, charter, first-run binding, and detached live-session launch are ready.
    - [finding] retire-pre-stabilizer-route :: Exploration-002 stopped after adding the required stabilizer transform and before rerunning the repaired fixture.
    - [finding] retire-time-only-dialogue-trigger :: Exploration-003 proved that twelve in-game hours of native waiting does not open the required dialogue surface.
    - [done] bind-native-dialogue-trigger :: The source-owned follower route reliably opens a real dialogue frame through exact native menu receipts.
    - [finding] isolate-dialogue-consumption-defect :: Exploration-004 proved that an advertised stable-ID response request reaches the bridge but receives no native receipt.
    - [finding] retire-input-context-hypothesis :: Exploration-005 proved that leaving wake behavior to the dialogue loop does not restore the missing receipt.
    - [done] repair-dialogue-consumption :: The focused owner consumes the advertised stable-ID request and emits an exact accepted receipt before synchronous native modal effects.
    - [done] implement-nested-dialogue-modal :: The `npc_rules_menu` child now publishes and consumes native cancel ownership and exits through the native path.
    - [finding] isolate-talker-selector-divergence :: Exploration-007 received an exact accepted Talk to receipt but restored the world instead of opening the expected selector or dialogue.
    - [finding] retire-child-successor-flag-strategy :: Exploration-008 received an exact accepted selector receipt and then repeated the same selector in frame 12. Its child-successor flag repair built successfully but did not change the live result.
    - [done] stabilize-talker-selection :: Fresh Tiles run `ef67ed60e62e46c799fecaab7da2fd32` advances an exact accepted talker selector receipt from frame 11 to distinct dialogue frame 13.
    - [done] repair-control-request-retry :: Request IDs now include stable targets and parameters, and a rejected native request releases the unchanged observation for a different advertised choice. The focused suites pass 40 and 46 cases.
    - [done] build-native-control-fixture :: The native fixture now supplies duplicate response labels, a disabled response, and a named speaker without synthetic state mutation.
    - [done] repair-dialogue-receipt-and-journal :: Immediate accepted dialogue receipts bind the first fresh child frame, and pre-dispatch rejections are retained in the witness journal.
    - [done] prove-dialogue-controls :: Fresh sealed witnesses prove duplicate IDs, disabled, missing, wrong-speaker, index, and hotkey rejection while valid stable IDs advance dialogue.
    - [done] prove-dialogue-renderers :: Independent source-bound Tiles and curses runs prove exact receipts, nested rules ownership, restoration, and accepted cleanup.
    - [done] close-dialogue-source-controls :: Current source, a forced compile, 73 focused tests, and both production journals prove stable IDs and fail-closed controls.
    - [done] close-tiles-dialogue-route :: Sealed Tiles run `10b85ef421c740a08caebced5fe4cec7` proves stable selection, all controls, nested ownership, restoration, and cleanup.
    - [done] close-curses-dialogue-route :: Sealed curses witness `f751...4689` proves equivalent native-only behavior, exact receipts, restoration, and accepted cleanup.
    - [done] judge-r-surface-007 :: Durable acceptance 1 preserves all three closed gaps and settles the whole claim.
  - Durable acceptance: #1 via `R-SURFACE-007-closure-003`; SQLite evidence is authoritative.
<!-- DE67:DELIVERY-STATUS:END -->
<!-- DE67:DFS-SLICE:END id=R-SURFACE-007-S001 claim=R-SURFACE-007 -->

### 8. Direction and targeting semantic surfaces

<!-- DE67:DFS-SLICE:BEGIN id=R-SURFACE-008-S001 claim=R-SURFACE-008 -->

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

<!-- DE67:DELIVERY-STATUS:BEGIN claim=R-SURFACE-008 -->
- [x] R-SURFACE-008 — Direction and targeting screens expose and consume their own stable native choices.
  - DFS slices: `R-SURFACE-008-S001`
  - Known foundation: R-SURFACE-001 through R-SURFACE-007 prove exclusive native ownership, exact requests and receipts, stable menu, item, and dialogue identities, and equivalent Tiles and curses routes.
  - Final proof: Source controls, both Tiles routes, and both curses routes are independently closed. Durable acceptance 1 uses closure-010 after all three sequence-2 gaps closed.
  - Subtasks:
    - [done] audit-direction-target-owners :: The audit traced both native loops, their candidate sources, and the absence of a dedicated R-SURFACE-008 harness route.
    - [done] implement-direction-surface :: The direction child publishes horizontal and allowed vertical choices and rejects semantic controls without falling through to raw input.
    - [done] implement-target-surface :: The target child publishes visible state and frame-local coordinate and opaque candidate bindings with current native actions.
    - [done] prove-fail-closed-targeting :: Source validation rejects hidden, moved, stale, out-of-range, invalid, screen-coordinate, and parent-World controls before cursor mutation.
    - [finding] retire-implementation-only-route :: Exploration-001 stopped after Tiles and curses object builds because no dedicated live scenario existed. Preserve the implementation and compile logs without treating the missing repository route as external authority.
    - [done] create-direction-target-routes :: Native Fire entry, zero-credit fixtures, and four source-bound Tiles/curses manifests now exist, and the Tiles build passes.
    - [finding] isolate-target-session-startup-stall :: Run `2e343d188e634121bbd864f8ed923e5c` reached the startup HUD but never opened a cockpit session or published the first target descriptor.
    - [done] prove-tiles-target-basics :: The live Tiles route proves target ownership, cursor movement, parent-action rejection, native cancel, and fresh World restoration.
    - [finding] isolate-candidate-observation-handoff :: Exploration-003 exposed an unknown-or-stale response because the client acted on an unregistered handed-off observation.
    - [done] close-tiles-target-route :: Sealed run `b4d13c8e8bb245cdb63b44f294f7a8bd` proves registered opaque candidate selection, parent-action rejection, native cancel, fresh successors, and cleanup.
    - [done] close-tiles-direction-route :: Sealed witness `7a320f...` proves direction-only ownership, parent-action rejection, accepted east choice, fresh World restoration, and cleanup.
    - [done] repair-curses-executable-selection :: Registry launch now honors the scenario-declared executable instead of forcing the Tiles binary.
    - [finding] retire-root-curses-rebuild-strategy :: Two root rebuilds failed during SDL linking and left no usable current root curses executable.
    - [done] repair-inventory-first-frame :: Inventory semantic ownership now begins after the first redraw so live stable item bindings are not immediately stale.
    - [done] prove-direction-target-renderers :: Current-source Tiles and curses witnesses prove direction and target ownership with exact receipts, fresh successors, and cleanup.
    - [done] close-direction-target-source-controls :: Closure-001 confirms the current source contract and matching live fail-closed behavior.
    - [finding] retire-unbound-route-review-attempts :: Closure-002 and closure-003 were abandoned because reused workers were not recognized as roster-bound. Preserve the existing Tiles and curses evidence and keep both gaps open.
    - [done] close-tiles-direction-target-routes :: Closure-004 independently confirms the sealed Tiles direction and target witnesses.
    - [finding] expose-curses-evidence-binding-gap :: Closure-005 found source identity mismatch, yellow reports, skipped native cleanup, and missing registry ingestion in the prior curses artifacts.
    - [finding] retire-post-exit-cleanup-strategy :: Closure-006 matched the new build and repaired bare-process recognition, but the direction process exited before positive cleanup and no witness was ingested.
    - [done] establish-immutable-curses-proof-lane :: Closure-007 built and bound a private isolated source, executable, registry, and ready direction selection without shared mutation.
    - [finding] retire-bootstrap-only-isolated-lane :: Closure-007 returned after creating the immutable proof prerequisite but before consuming it for independent direction or target validation.
    - [done] ingest-isolated-curses-witnesses :: Closure-008 canonically ingested proved direction and target witnesses from the immutable lane.
    - [finding] expose-isolated-cleanup-credit-gap :: Both isolated probe reports record `already_exited` and no native exit credit, so the ingested witnesses cannot close revision 2.
    - [done] close-curses-direction-proof :: Closure-009 ingested witness `d2ccf7...` with exact direction receipts and positive cleanup of the live native process.
    - [finding] retire-target-handoff-midrun :: Closure-009 returned after launching target bridge `19d1ce...` but before observing, acting, terminalizing, or ingesting the target route.
    - [done] close-curses-direction-target-routes :: Closure-010 preserves direction witness `d2ccf7...` and adds target witness `9fa8e3...` with matched identities, exact receipts, positive cleanup, and canonical ingestion.
    - [done] judge-r-surface-008 :: Durable acceptance 1 preserves all three closed gaps and settles the whole claim.
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

Implementation status:

<!-- DE67:DELIVERY-STATUS:BEGIN claim=R-SURFACE-009 -->
- [x] R-SURFACE-009 — Every required discovered input owner operates semantically, and every new or incomplete owner stops automation without fallback.
  - DFS slices: `R-SURFACE-009-S001`
  - Known foundation: R-SURFACE-001 through R-SURFACE-008 prove exclusive native ownership, native-only requests, stable identities, common and focused owner families, exact receipts, and Tiles/curses parity.
  - Final proof: Four independent revision-1 gaps are closed. The source gate classifies all 73 discovered direct-input sources. Fresh source-bound Tiles debug-spell and map-editor routes prove stable native actions, exact receipts, restoration, and cleanup. The curses map-editor route proves renderer parity. Fresh ingested unsupported run `20260901_203737_669628fd73db436ca5d51d9e6f09509d` proves stable DEBUG_CONSOLE ownership, zero actions, zero submitted requests, no parent fallback, and accepted cleanup.
  - Subtasks:
    - [done] inventory-native-input-owners :: The source inventory classifies 73 direct input files as supported, backend or transport, or incomplete.
    - [done] enforce-owner-coverage-boundary :: The executable inventory test fails on newly discovered unclassified direct-input files and passes its three focused cases.
    - [done] implement-required-owner-routes :: Focused map-editor and debug-spell scopes now map stable semantic choices to their existing native actions.
    - [done] hard-stop-incomplete-owners :: Input context publishes an actionless unsupported scope with owner breadcrumbs and reason instead of inheriting parent or raw-input behavior.
    - [finding] retire-inventory-only-strategy :: Exploration-001 stopped after inventory and hard-stop implementation while required debug and map-editor operation remained unimplemented.
    - [done] repair-spell-registry-bootstrap :: A fresh Tiles run loads Magiclysm, publishes the debug-spell owner, and accepts stable spell selection with an exact receipt.
    - [done] repair-debug-spell-second-action :: Run `20260901_201638_13d5fab102444959aaa45c490d4b9924` proves stable Magiclysm selection, increment, close, exact receipts, and restored World.
    - [done] prove-curses-map-editor :: The source-bound curses route proves native movement, close receipts, structured gates, and confirmed process cleanup.
    - [done] stabilize-unsupported-owner-lifetime :: A direct probe shows DEBUG_CONSOLE remains the top owner with zero actions and no inherited parent actions.
    - [done] repair-and-prove-unsupported-contract :: Green report `20260901_202344_40d4b402d2ff4affb3452e52dbf9b9b5` proves stable DEBUG_CONSOLE ownership, zero actions, zero submitted requests, and no parent fallback.
    - [done] close-source-owner-coverage :: Closure-001 proves every discovered direct-input source is classified and the executable gate rejects new unclassified owners.
    - [done] close-required-owner-operation :: Closure-002 proves fresh Tiles debug-spell and map-editor operation with exact receipts, restoration, and cleanup.
    - [done] close-renderer-owner-parity :: Closure-003 proves the equivalent source-bound curses map-editor route with exact receipts and cleanup.
    - [done] close-unsupported-owner-hard-stop :: Closure-004 proves stable actionless unsupported ownership with zero requests and no forbidden fallback.
    - [done] judge-r-surface-009 :: Durable acceptance 1 preserves all four closed gaps and settles the whole claim.
  - Acceptance: durable acceptance 1 uses closure-004 after all four revision-1 gaps closed.
  - Durable acceptance: #1 via `R-SURFACE-009-closure-004`; SQLite evidence is authoritative.
<!-- DE67:DELIVERY-STATUS:END -->
<!-- DE67:DFS-SLICE:END id=R-SURFACE-009-S001 claim=R-SURFACE-009 -->

### 10. Cockpit active-surface projection and end-to-end proof

<!-- DE67:DFS-SLICE:BEGIN id=R-SURFACE-010-S001 claim=R-SURFACE-010 -->

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

<!-- DE67:DELIVERY-STATUS:BEGIN claim=R-SURFACE-010 -->
- [x] R-SURFACE-010 — The cockpit replaces its active presentation from the exact top descriptor across World, Overmap, Inventory, Dialogue, Menu/Prompt, Direction, Target, and Unsupported surfaces.
  - DFS slices: `R-SURFACE-010-S001`
  - Known foundation: R-SURFACE-001 through R-SURFACE-008 prove the renderer-neutral ownership, native request, stable identity, receipt, and focused projection primitives needed by the integrated route.
  - Wake condition: R-SURFACE-009 is durably accepted and the resulting source, executable, scenario, and renderer bindings are current. Until then this outcome remains visible but does not outrank the active coverage work.
  - Final outcome: Current source and focused tests enforce exact descriptor-only projection. Fresh source-bound Tiles, Overmap, Dialogue, curses, and Unsupported witnesses prove stable identities, isolated actions, exact receipts, complete breadcrumbs, restored ownership, renderer parity, and hard-stop behavior.
  - Subtasks:
    - [done] preserve-transaction-foundation :: Keep the accepted observation-use, frame-freshness, advertised-action, exact-receipt, and structured terminal-projection foundations.
    - [done] project-complete-top-surface-family :: Observation v2 and the terminal cockpit replace the active view from the exact descriptor without cached parent facts or actions.
    - [finding] retire-implementation-only-strategy :: Exploration-001 completed the projection and focused tests but returned before creating or running the integrated proof route.
    - [finding] retire-stale-parent-chain-strategy :: Exploration-002 proved the nested route through Direction, then correctly failed when it reused the pre-child parent descriptor after the UI recreated that surface.
    - [finding] expose-transient-world-publication :: Exploration-003 re-observed before every action and proved that a transient World descriptor becomes externally visible before native parent restoration completes.
    - [done] repair-durable-top-owner-publication :: Transient restoration states remain private, and only the actual recreated owner restores authority.
    - [finding] retire-blanket-direction-suppression :: Exploration-004 proved that generic suppression prevents every successor instead of handing authority to the recreated item-menu owner.
    - [finding] retire-implementation-without-rerun :: Exploration-005 implemented the item-use-specific handoff and compiled changed objects but returned before source-bound feature proof.
    - [finding] expose-post-direction-owner-gap :: Exploration-006 proved the recreated item-menu receipt but found that its next native owner is still missing or replaced by an early World frame.
    - [done] repair-post-direction-confirmation-handoff :: Direction now hands authority to the actual confirmation or recreated owner with exact successor receipts.
    - [finding] expose-post-direction-publication-stall :: Exploration-007 proves native North consumption, then no true successor publishes after `choose_direction` returns.
    - [finding] isolate-manager-recreated-push-stall :: Exploration-008 proves item-use return and scope reset, then the recreated manager push blocks before descriptor emission.
    - [done] repair-manager-withheld-lifecycle :: Private unwind no longer owns a descriptor, and only the recreated menu restores authority.
    - [done] prove-nested-integrated-route :: Run `20260901_225228_47a528cca9de472e9ae0fee068a47f8d` proves the 12-transaction nested Tiles route with exact receipts and restoration.
    - [done] prove-overmap-and-dialogue-branches :: Closure-003 proves current-product source-bound Overmap and Dialogue replacement, stable targets, exact receipts, restoration, and cleanup.
    - [done] prove-renderer-and-unsupported-boundaries :: Closure-005 proves current-source curses projection parity and stable actionless Unsupported with zero submissions and no fallback.
    - [finding] retire-curses-build-only-attempt :: Closure-004 returned while the repaired ordinary curses build was still running, before executable proof.
    - [done] judge-r-surface-010 :: Durable acceptance 1 preserves all four closed gaps and settles the whole claim.
  - Acceptance: durable acceptance 1 uses closure-005 after all four revision-1 gaps closed.
  - Durable acceptance: #1 via `R-SURFACE-010-closure-005`; SQLite evidence is authoritative.
<!-- DE67:DELIVERY-STATUS:END -->
<!-- DE67:DFS-SLICE:END id=R-SURFACE-010-S001 claim=R-SURFACE-010 -->

### 11. Current-source CAOL feature playtest package

The accepted semantic cockpit is the native operation and observation layer for this package. Its
acceptance does not prove the gameplay below. Package verdicts remain independent: a combined living
world may prove one behavior, contradict another, and leave a third unobserved without collapsing
those results into one pass/fail label.

<!-- DE67:DFS-SLICE:BEGIN id=R-026-S001 claim=R-026 -->

Mechanism:

- Files and symbols: current `llm_intent`, follower and ambient NPC behavior, faction-camp request
  and zone code, Locker and Patrol behavior, hostile-camp signal and lifecycle code, flesh-raptor
  behavior, cockpit/registry tooling, and claim-scoped package scenarios and witnesses.
- Canonical footing: use the audited
  `bandit_basecamp_prepared_base_v1_2026-04-22` save/profile pair while its current-source audit
  remains true. It contains the Bugchaser base, bulletin board, `CAMP_FOOD`, `LOCKER`, `STORAGE`, two
  `PATROL` zones, and the staffed NPC population needed by the living-base routes. Repair or replace
  repository-owned fixture, scenario, registry, executable, or observation bindings when current
  evidence disproves them.
- Living-base branch: exercise follower free-text and ambient LLM NPC behavior with snapshots,
  basecamp request context, Smart Zone Manager, Locker and Patrol behavior, and save/reload
  continuity through current native semantic owners. Add a package-specific action or observation
  bridge only after a current route proves the accepted semantic surfaces cannot express the
  required product interaction or receipt.
- Threat-and-causality branch: exercise overmap light, smoke, and significant-sound causation;
  bandit and cannibal discovery, stalking, contact, attack, and return lifecycles; and flesh-raptor
  behavior. Use positive and negative controls that distinguish signal-driven behavior from ordinary
  drive, proximity, elapsed time, or debug setup. The integrated package must incorporate the
  coherent shared-route and contact-lifecycle proof in R-029 without merging its verdict with other
  package claims.
- Package result: retain mechanical receipts, gameplay-feel observations, source/executable/world/
  actor binding, cleanup, contradictions, and independent claim verdicts in reusable witness
  artifacts. Route ordinary product or harness defects through a durable claim-scoped findings
  intake and keep unaffected observations.
- Scope boundary: writhing stalkers and zombie riders remain excluded pending another owner design
  pass. Debug interventions and fixture preparation are allowed as zero-credit setup; they do not
  prove natural discovery, causation, ecology, combat, lifecycle, or gameplay feel.
- Package guide: describe the current fixture, bindings, launch/repair route, claim questions,
  credit boundaries, expected artifacts, independent verdicts, and known limitations. It must not
  present the semantic cockpit itself as final gameplay proof.

Implementation status:

<!-- DE67:DELIVERY-STATUS:BEGIN claim=R-026 -->
- [ ] 🔴 R-026 — No current-source integrated CAOL feature package yet binds the living-base,
  bandit, cannibal, signal-control, and flesh-raptor families through one audited established-base
  footing with independent mechanical, causality, feel, persistence, and cleanup evidence plus a
  usable package guide.
  - DFS slices: `R-026-S001`
  - Accepted footing: R-SURFACE-001 through R-SURFACE-010 remain durably accepted and provide native
    World, Overmap, Inventory, Dialogue, Menu/Prompt, Direction, Target, and Unsupported operation.
    They do not close this gameplay claim.
  - Preserved evidence: the established-base audit and older standalone follower, ambient, Locker,
    Patrol, significant-sound, light/smoke, bandit, cannibal, and flesh-raptor reports remain useful
    route evidence at their recorded source and evidence ceilings. The earlier bandit lifecycle run
    `73ddab45` is baseline behavior, not current fire causation, camp behavior, or LLM proof.
  - Current uncertainty: the canonical fixture aliases still resolve, but the present registry has
    no integrated package witness. Several older hostile-camp scenarios are stale, quarantined, or
    contradicted; current standalone scenario eligibility cannot substitute for combined,
    claim-bound current-source qualification. Existing prepared-contact shakedown and cannibal
    signal scenarios remain route evidence only: they do not prove the natural shared response path,
    first-demand ordering, safe paid departure, or dawn-after-departure commitment required by
    R-029. The ordinary worker may repair those routes or invent a materially different package
    route.
<!-- DE67:DELIVERY-STATUS:END -->
<!-- DE67:DFS-SLICE:END id=R-026-S001 claim=R-026 -->

<!-- DE67:DFS-SLICE:BEGIN id=R-027-S001 claim=R-027 -->

Mechanism:

- Files and symbols: `src/bandit_live_world.cpp :: record_staffed_camp_signal_observations`, the
  camp signal-lead memory and normal hostile-camp drive/response path, current significant-sound and
  overmap signal sources, and claim-bound observation/control scenarios.
- Entry point: production cadence samples an eligible at-home staffed-camp observer; no harness
  callback may fabricate the observation.
- Transition: current source applies real observer range, line of sight, elevation, weather, source,
  and channel rules, records a stable `camp-signal:` lead, deduplicates unchanged observations,
  ages memory, and leaves the ordinary drive owner responsible for any response.
- Proof split: a zero-credit observation/bootstrap may create a physical source and validate the
  route once. Independent validation must then begin from the saved source and prove the camp-owned
  observation, bounded no-signal/out-of-range/blocked/dedup/aging controls, and an ordinary
  non-scripted response without querying the unchanged bootstrap as its own proof.
- Failure behavior: timestamp adjacency, debug setup, player knowledge, direct state mutation,
  ordinary proximity, or a lead without a bound observer/source/channel cannot prove signal
  causation. A valid observation does not by itself prove a normal response.

Implementation status:

<!-- DE67:DELIVERY-STATUS:BEGIN claim=R-027 -->
- [x] R-027 — Independently validate the current staffed-camp structural-signal observation and
  memory path from a saved physical source through bounded controls and an ordinary response.
  - DFS slices: `R-027-S001`
  - Preserved implementation: `record_staffed_camp_signal_observations` uses eligible at-home
    observers, real sight/hearing/weather/elevation constraints, stable `camp-signal:` source and
    channel identity, deduplication, aging, location memory, and unchanged normal drive ownership.
    Closure-013 repaired the isolated runtime and retained focused source evidence.
  - Preserved bootstrap: exploration-013 saved a source-bound physical `fd_fire` west of the audited
    camp. It did not execute the scheduler or observe a lead, so it earns no feature credit and must
    not be queried again unchanged as its own validation.
  - Current evidence: current-source runs prove that the staffed camp records real smoke and light
    from a saved fire. Separate saved-world runs prove that opaque terrain and an absent source add
    no lead. Run `76bc9842…1f2481c` proves that a source at range 12, beyond the range cap 5 and
    without line of sight, leaves the named lead and ordinary drive unchanged. Run
    `68e24…3e4e0` proves that repeated in-range observations keep the same smoke and light lead IDs
    and keep the total lead count at three. Run `63371e2b…f309c` proves that two unsupported leads
    become stale at age 415 while preserving `last_seen=9245` through native save. The remaining
    product uncertainty is whether a material change to the same visible source updates the same
    lead without duplication and whether the ordinary camp drive produces a response. The retired
    wait-menu strategy and its broker repairs remain diagnostic evidence. The next route starts
    from `r027_in_range_saved_lead_v001_20260902`, binds the actual source offset `[0,11,0]`, changes
    the fire intensity from 3 to 2, and uses a direct production cadence. The first replacement
    attempt validated the manifest and setup-only capture contract, then proved that the saved fire
    was still intensity 3. A second run removed the fire and proved the tile was empty. It then
    exposed a semantic-owner defect: the outer map editor stays advertised while the native field
    and intensity menus own input. The next worker must factor the editor scope construction into a
    reusable factory, yield before `brush.select_field()`, and recreate the editor scope after the
    nested menus return. That repair now works. A clean run saved and independently audited the fire
    at intensity 2, and a separate native-pause run refreshed the same two lead IDs from minute 9245
    to 9250 without increasing the lead count. The lifecycle stream is monotonic. Canonical capture
    now uses one prepared certification binding across the bridge, child, native sidecar, and report.
    Canonical run `7bfe4de5…b65d4` is internally green. It proves the same smoke lead at source
    `(140,50,0)` refreshed at minute 9250 with lead count three, persisted fire intensity 2,
    monotonic lifecycle, feature proof, and accepted cleanup. Only the ordinary camp response to a
    valid retained lead remains. Live run `3fd920fc…0a100` proves the ordinary camp drive selected
    the retained light lead and committed a structural sortie with actors 4 and 18. Independent
    native test `bandit_live_world_retained_signal_dispatch_survives_save_round_trip` passes 23
    assertions and proves the operation, target lead revision 2, active lead, and outbound actors
    persist through production save/load. Combined report SHA `17058a17…328dfb` keeps live behavior
    and independent persistence evidence separate.
  - Subtasks:
    - [done] implement-camp-owned-signal-memory :: Preserve eligible observer selection, physical constraints, stable source/channel leads, deduplication, aging, location memory, and normal drive ownership.
    - [done] bootstrap-saved-physical-source :: Preserve the zero-credit fire placement/save artifact and its exact source/world/executable ceiling.
    - [done] rebind-independent-validation :: The preserved source now installs into a disposable profile and runs through a source-current executable with native semantic control.
    - [done] prove-production-observation :: Production cadence recorded new smoke and light leads for the named camp, observer, physical source, location, and real environmental checks.
    - [done] repair-independent-runtime :: One run identity now binds registry authority, native startup, cockpit, bridge, and controller; stable wait-menu choices and atomic prompt handling pass focused tests.
    - [done] audit-clean-safe-footing-save :: Run 38b5bb20…05b517 moved only the avatar within the same overmap tile; authoritative snapshots preserve the fixed fire and protected camp state at SHA c7424906…f300.
    - [done] isolate-validation-damage :: The replacement fixture and direct cadence route avoid the unrelated attacked interruption without crediting setup as behavior.
    - [done] prove-bounded-causal-controls :: Canonical green proof preserves absent, blocked, out-of-range, unchanged deduplication, aging, and changed-source update controls with exact identities and cleanup.
    - [done] prove-normal-response :: Ordinary drive selected the retained light lead, dispatched actors 4 and 18, and independent production save/load preserved the active operation and outbound state.
    - [done] format-claim-witness :: Green changed-observation and combined ordinary-response reports preserve exact evidence classes, bindings, controls, persistence, and cleanup.
  - Durable acceptance: #1 via `R-027-closure-008`; SQLite evidence is authoritative.
<!-- DE67:DELIVERY-STATUS:END -->
<!-- DE67:DFS-SLICE:END id=R-027-S001 claim=R-027 -->

<!-- DE67:DFS-SLICE:BEGIN id=R-028-S001 claim=R-028 -->

Mechanism:

- Files and symbols: `tools/hostile_camp_benchmark.py`, the package's current-source build and
  scenarios, native timing/counter sources, process CPU/RSS observation, and profiler artifacts only
  after a reproducible regression or gameplay-feel concern identifies a useful question.
- Comparison: run paired baseline/feature workloads from equivalent fixture state. Include combined
  established-base and hostile-ecology work plus rendered local transitions; do not extrapolate from
  an isolated synthetic loop.
- Measurements: preserve frame pacing, relevant update latency and native counters, process CPU and
  RSS, source/executable/scenario identity, repeated observations sufficient to expose variability,
  and the associated feel result. Derive any threshold from a named product requirement or measured
  baseline; do not invent one.
- Failure behavior: a benchmark that omits rendering, uses unmatched worlds/builds, hides variance,
  reports only an average, or profiles before reproducing the workload cannot qualify the package.

Implementation status:

<!-- DE67:DELIVERY-STATUS:BEGIN claim=R-028 -->
- [x] R-028 — Qualify the current package's performance under paired combined workloads including
  rendered local transitions, mechanical context, and gameplay feel.
  - DFS slices: `R-028-S001`
  - Known footing: `tools/hostile_camp_benchmark.py` and earlier timing artifacts can seed a route,
    but they do not bind the completed cockpit, current package workload, rendered transitions, and
    current executable strongly enough to close this claim.
  - Current evidence: fresh baseline report
    `20260903_183103_938dff78278a4cd8926c2ba74a1f856b` and feature report
    `20260903_183420_7782a0e532dd43ce9d449bb06eb0a404` use equivalent prepared state and the same
    rebuilt Tiles executable. Both runs have green semantic ledgers, rendered gameplay traces,
    accepted save-and-quit receipts, and observed native exit of the original game process. The
    feature run also records two live two-site hostile-camp cadence measurements. The earlier matched
    reports preserve 263 raw renderer samples per run, comparable process CPU and memory context, and
    shared long pacing gaps. Together this evidence proves no material difference was observed on
    this paired route. It does not define a product threshold or make a universal guarantee.
  - Closure status: four review attempts ended because the worker service returned HTTP 404 before
    examining the reports. Those attempts produced no product finding and closed no gap. The service
    recovered during `R-028-closure-005`. That review closed `performance-verdict` because the matched
    reports support only the route-bounded conclusion that no material difference was observed. The
    reports record the same long pacing gaps, infer no product threshold, and make no universal
    guarantee. `R-028-closure-006` closed `measurement-fidelity` because the reports preserve raw live
    renderer distributions, native hostile-camp counters, process context, semantic receipts,
    variability, and controlled harness cleanup. `R-028-closure-007` then found that both reports
    ended through harness-issued `SIGTERM` with `native_exit_credit: false`. This means they do not
    prove clean native termination. `paired-route-binding` remains open. The next route must preserve
    the valid binding and measurement evidence while producing a fresh matched pair that saves and
    quits in-game and records each exact process exit. `R-028-closure-008` produced that pair and
    closed `paired-route-binding`. All three closure gaps are now closed. Preserve every immutable
    report and the failed all-semantic counterexample. Do not replay any failed worker attempt.
  - Subtasks:
    - [done] bind-current-workloads :: Versioned baseline and feature scenarios use equivalent fixture state, one current build, rendered local transitions, and combined hostile-camp work.
    - [done] preserve-mechanical-context :: Both reports bind source, executable, world, scenario, active behavior, semantic receipts, cleanup, and the gameplay-feel interpretation.
    - [done] capture-frame-and-update-cost :: Each run preserves 263 renderer samples, and the feature run preserves two live native cadence measurements.
    - [done] capture-process-cost :: Both reports preserve comparable process CPU and memory observations without treating them as frame cost.
    - [done] compare-without-invented-gates :: The paired distributions support no material observed difference at median and p95 without inventing a product threshold.
    - [done] profile-reproduced-regressions :: No repeatable regression or feel concern appeared, so no profiler question was created and no profile was run.
    - [done] publish-performance-verdict :: The matched reports preserve variability, mechanics, feel, cleanup, and the limit that this is not a universal guarantee.
  - Durable acceptance: #1 via `R-028-closure-008`; SQLite evidence is authoritative. The acceptance
    command reported that the DFS baseline is missing after it wrote the acceptance. Policy must route
    that projection defect without creating a second acceptance.
<!-- DE67:DELIVERY-STATUS:END -->
<!-- DE67:DFS-SLICE:END id=R-028-S001 claim=R-028 -->

### 12. Coherent bandit and cannibal signal response and contact lifecycle

<!-- DE67:DFS-SLICE:BEGIN id=R-029-S001 claim=R-029 -->

Mechanism:

- Files and symbols: `src/do_turn.cpp :: overmap_npc_move`,
  `live_bandit_staffed_camp_signal_reads`, `dispatch_live_cannibal_signal_contacts`,
  `advance_live_bandit_hostile_approaches`, `open_live_bandit_shakedown_surface`,
  `live_bandit_commit_paid_return`, and `live_bandit_choose_fight`;
  `src/bandit_live_world.cpp/.h :: record_staffed_camp_signal_observations`,
  `advance_structural_bounty_maintenance`, `plan_hostile_operation_with_authorized_response`,
  `choose_local_gate_posture`, and the operation-scoped relationship query;
  `src/npc.cpp :: npc::attitude_to`, `npc::guaranteed_hostile`;
  `src/npcmove.cpp :: npc::regen_ai_cache`,
  `npc::move`; focused native tests and claim-bound natural package witnesses.
- Shared discovery owner: remove the `dispatch_live_cannibal_signal_contacts` production call and
  retire the cannibal-only distance shortcut. Both camp profiles must enter through a physical
  signal read by an eligible staffed observer, stable camp memory, ordinary scout assignment and
  physical investigation, a returned scout report, response authorization, rally, and physical
  response travel. Profile policy may select `bandit_shakedown` or `cannibal_night_raid` only after
  the same report boundary. Direct report, knowledge, dispatch, actor, or contact fabrication is not
  an alternate production route.
- Night-raid commitment: a cannibal operation in `rallying` may assign its target route and become
  `approaching` only while it is night. That departure transition is the sole night gate. Once the
  operation has left the rally, dawn does not cancel, return, or stall it and contact does not
  re-check night.
- Operation-scoped player relationship: the hostile-operation owner must classify each exact member
  as shakedown parley, combat-released, or paid departure from persisted operation ID, generation,
  membership, phase, and outcome. That relationship is authoritative before faction dislike,
  `attitude_to`, `guaranteed_hostile`, AI-cache classification, `NPCATT_KILL`, targeting, movement,
  or another offensive action. It does not make the faction globally friendly and it does not mask
  unrelated hostiles.
- Normal shakedown ordering: on first normal committed contact, every operation member remains in
  parley and takes no offensive action until the demand surface is presented. Choosing Fight,
  cancelling or failing to complete the required payment, or attacking a member records the fight
  outcome and atomically releases the exact operation group to combat. No time gap may leave one
  member in generic hostility while the contact still claims parley.
- Payment and departure: choosing Pay continues to use the existing forced native trade interface;
  no automatic theft is required. Only completed payment records the paid outcome and atomically
  transitions the exact operation group to `returning_home`. The paid-departure relationship
  suppresses renewed player targeting and offensive action through later NPC turns and save/reload
  until the members are home or the operation is otherwise normally completed. It does not erase
  unrelated combat or create a faction-wide truce.
- Rolling-travel branch: preserve `choose_local_gate_posture` behavior that may select immediate
  combat for a favorable rolling-travel scene. This branch is not normal shakedown contact and need
  not present Pay or Fight.
- Natural proof: extend, rather than overwrite, the existing package. From a source-bound current
  binary and save, create a physical player signal through ordinary product input and observe the
  staffed observer, camp memory, outbound scout, physical investigation, returned report, authorized
  profile response, and physical response travel. Claim credit begins only at production behavior;
  setup helpers may establish footing but cannot create the report, response, contact, or verdict.
  Preserve exact source, signal, camp, observer, lead, scout, report, operation, actor, turn, and
  receipt identities across artifacts and native save/reload boundaries.
- Required branches and controls: the normal-bandit branch must show the demand before every
  offensive event, completed payment, safe paid departure over subsequent ordinary NPC turns and a
  native save/reload, plus separate refusal or incomplete-payment and player-attack routes that enter
  combat. A control preserves a favorable rolling-travel direct ambush. The cannibal branch must
  show daylight rally hold, night departure, a deliberate transition to daylight before contact,
  continued approach, and attack. A matching no-signal or unstaffed/blocked observation control must
  not create the route.
- Failure behavior: a direct cannibal dispatch, helper-created report, direct state edit, prepared
  local contact alone, startup/load proof, dialogue screenshot without turn ordering, paid log line
  without subsequent safe NPC turns, night contact without daylight crossing, or aggregate package
  verdict cannot close the claim. Preserve earlier scenarios and evidence at their existing ceilings.

Implementation status:

<!-- DE67:DELIVERY-STATUS:BEGIN claim=R-029 -->
- [ ] 🔴 R-029 — Bandit and cannibal camps do not yet have a proved coherent natural
  signal-to-response route with correct night-raid commitment and operation-scoped shakedown,
  combat, and paid-departure ordering.
  - DFS slices: `R-029-S001`
  - Accepted footing retained: R-027 remains accepted because its evidence proves the shared staffed
    observation, stable camp memory, controls, and ordinary response mechanics it claims. This
    refinement does not invalidate that narrower result. Existing night-rally, Pay/Fight, trade,
    persistence, and rolling-travel scenarios remain focused route evidence at their recorded
    setup and source ceilings.
  - Current implementation and accepted branch footing: the cannibal-only production shortcut is
    removed; exact members persist parley, combat-release, and paid-departure relationships; generic
    hostility yields to that operation owner. Current runs prove demand-before-choice, Fight release
    for the exact group, unrelated-hostile isolation, and paid safe return through reload and later
    ordinary turns. Preserve those results at their focused ceilings.
  - Current continuation: durable receipt `513ff62a4910ccc0fd93d40bc7d6ccd72312b8079f1a48ec380facd3c8507a65`
    binds the current natural-route run and artifacts. It accepts the launch, World observation pair,
    and first duration cycle through minute 8222 without ecology credit. The first open boundary is
    the missing response for `r029-dispatch-keep-watch-4m`; after repair, natural routes for both
    profiles, daylight-after-cannibal-departure, refusal/incomplete payment, player attack, Fight
    terminal behavior, rolling ambush, persistence, cleanup, and independent verdicts remain unproved.
<!-- DE67:DELIVERY-STATUS:END -->
<!-- DE67:DFS-SLICE:END id=R-029-S001 claim=R-029 -->

### 13. Quiet, indexed, lossless agent evidence

<!-- DE67:DFS-SLICE:BEGIN id=R-030-S001 claim=R-030 -->

Mechanism:

- Scope: repository-owned CAOL registry, build/runtime status, cockpit, witness, and diagnostic
  commands plus the Phase-3 worker result/continuation boundary. This is an evidence transport and
  context-shape claim; it does not change gameplay truth or the evidence class of any observation.
- Artifact-backed quiet output: a command that can produce a large status, history, transcript, or
  witness payload preserves the complete output as a digest-bound artifact and prints a compact
  receipt by default. Explicit full retrieval remains available and lossless. Quiet output may omit
  payload content, never the identities, digest, evidence ceiling, contradictions, or lookup footing
  required to recover it.
- Pinpoint retrieval: evidence stores expose exact selectors for the fields they own, including
  scenario/run/binding, event/evidence class, actor/action, native receipt, first divergence, and
  verdict. A narrow query returns only matching compact records; a caller explicitly asks for the
  complete record or artifact. No caller must deserialize the whole registry or search prose merely
  to find one known identity.
- Worker brief: a successor receives the outcome, current frontier, compact continuation receipt,
  no-replay work, first open boundary, exact bindings and entrypoints, and a progressive read plan
  that names why each source might matter. The plan is not a quota or mandatory command sequence;
  broader reads follow only when evidence makes them material.
- Durable continuation: every worker-owned completion, finding, or abandonment is preceded by one
  validated identity-bound receipt containing the outcome or divergence, changes, tests/live acts,
  evidence ceiling, bindings, stable journal identities, artifact paths and digests, accepted and
  active work, first open boundary, narrow queries, and entrypoints. The terminal transition cites
  that receipt, so a successor does not reconstruct state from chat or log archaeology.
- Measurement: bytes, record counts, checkpoint counts, elapsed time, and retrieval shape are
  diagnostic evidence used to locate bulk or ambiguity. They are not invented limits, quotas,
  retention rules, or substitutes for causal correctness.
- Failure behavior: compact-only output without the complete artifact, full output without a stable
  digest/reference, filters that silently scan and return unrelated history, a receipt with identity
  or digest drift, a terminal worker transition without its receipt, or a successor that must replay
  accepted work cannot close this claim.

Implementation status:

<!-- DE67:DELIVERY-STATUS:BEGIN claim=R-030 -->
- [x] R-030 — Make CAOL and DE67 worker evidence quiet by default without losing any full-fidelity
  artifact: compact command receipts, indexed journal lookup, outcome-sized progressive briefs, and
  durable successor continuation must preserve exact identity and binding while the full digest-bound
  payload remains explicitly retrievable.
  - DFS slices: `R-030-S001`
  - Known footing: the guarded Phase-3 method now validates and stores identity-bound worker receipts,
    queries compact projections by exact continuation/evidence identities, and generates successor
    packets from the current frontier plus a reasoned read plan. Backfilled receipts preserve R-026
    and R-029 without replaying their accepted work.
  - Current evidence: `registry-status` is quiet by default and keeps its complete result in a
    digest-bound artifact. `runtime-status` now does the same and supports exact executable binding
    selection, explicit full output, and exact artifact retrieval. A live missing-executable check
    returned receipt `6bdf85e5…2202`, and exact retrieval recovered `build_required`. Two focused
    tests, Python compilation, and diff validation passed. Receipt `5e3ed2b8…eae9` preserves this
    worker result. The earlier 4,236,128-character output remains the counterexample that this route
    fixes.
  - Current uncertainty: exact cross-surface selectors, the remaining CAOL skill contract, and a
    fresh successor continuation remain unproved. Two unrelated full CLI startup tests still fail
    because the canonical `cli.json` file is absent. The full cockpit bridge suite also has existing
    intermittent asynchronous ready and cleanup races outside the focused receipt proof.
  - Latest result: the remaining CAOL surface gap is closed by task `R-030-closure-001`. The shared
    cockpit bridge keeps compact status and exact slices, recovers full cockpit, witness, and
    diagnostic responses only through the receipt digest, and rejects digest mismatch, tampering,
    and response-namespace path escape. Tasks `R-030-closure-002` and `R-030-closure-003` then reached
    the same W1 clock-only wait before selector work began. Tasks `R-030-closure-004` and
    `R-030-closure-005` targeted the separate skill-and-contract gap but received the same clock-only
    wait before any repository work. Receipt `d271124b…c859c` preserves the latest attempt. It proves
    that the old dispatch packet blocked the assigned repository route. It does not judge any open
    product gap.
  - Route change: the owner approved recovery after correcting the worker routing loop. A fresh
    neutral worker may now inspect and change the repository for its assigned outcome. The completed
    registry, runtime, and shared cockpit receipt work must not be replayed.
  - Closed gap: task `R-030-closure-006` aligned the CAOL skill and focused contract test with the
    implemented compact, full, query, and digest-bound recovery routes. Four focused tests and
    `git diff --check` passed. Receipt `731f21ca…d4822` preserves the exact result. The missing
    `tools/openclaw_harness/scenarios/cli.json` file still blocks two unrelated broad startup tests.
  - Closed gap: task `R-030-closure-007` proved all 14 required exact receipt selectors. Each
    positive filter returned the bound compact receipt, and a wrong-actor control returned none.
    Ninety harness tests and focused CAOL checks passed. Receipt `54e16f59…d2258c` preserves the
    result.
  - Accepted claim: task `R-030-closure-008` used a fresh packet, verified its digest, recovered the
    exact prior selector receipt, and continued without replaying accepted work. Receipt
    `0e2afae0…a96625` preserves the result. Every required R-030 closure gap is closed. Durable
    acceptance 1 now settles R-030.
  - Subtasks:
    - [done] measure-existing-context-path :: Preserve the 4,236,128-character/223-entry registry counterexample and the 151/6 checkpoint continuity evidence as diagnostics, not limits.
    - [done] generate-progressive-worker-packet :: Phase-3 worker packets now carry the outcome, current frontier, compact receipt, exact bindings/entrypoints, and a reasoned progressive read plan.
    - [done] persist-query-worker-continuation :: Identity-bound worker receipts are artifact-validated, durably stored, terminal-gated, and narrowly queryable; R-026 and R-029 have backfilled continuations.
    - [done] make-caol-command-output-quiet :: Registry, runtime, cockpit, witness, and diagnostic commands now return compact receipts and preserve verified full artifacts.
    - [done] index-pinpoint-evidence :: Fourteen exact worker-receipt filters return only the bound receipt, and a wrong-actor control returns no result.
    - [done] align-caol-skill-and-contract-tests :: The CAOL skill and focused test now describe and enforce the implemented registry, runtime, and shared cockpit quiet, full, query, and recovery behavior.
    - [done] prove-lossless-successor-route :: A fresh worker verified its packet digest, recovered the exact prior receipt, and continued from the recorded boundary without replay or log archaeology.
  - Durable acceptance: #1 via `R-030-closure-008`; SQLite evidence is authoritative.
<!-- DE67:DELIVERY-STATUS:END -->
<!-- DE67:DFS-SLICE:END id=R-030-S001 claim=R-030 -->

## Competing systems and override direction

| State or action | Readers | Writers / competing owners | Authoritative decision |
|---|---|---|---|
| Active semantic surface | cockpit readers; terminal projection | native world loop, `uilist`, popup, inventory, dialogue, overmap, direction, target, editors | The top `semantic_surface_scope` alone owns truth. Push transfers authority to child; pop invalidates child and republishes parent with a fresh frame. Rendering and `input_context` category do not override it. |
| Frame and receipt identity | `CockpitRunChannel`, broker, evidence readers | semantic manager versus legacy debug-log parser | CDDA creates frame IDs and receipts. Consumers validate but never mint or repair them. A receipt records the requested frame identity and the identity of the frame that consumed or rejected the request. Duplicate request IDs are idempotent; stale frames reject. |
| Semantic action binding | cockpit caller; native owner | Python `action_inputs`, OS key injection, local physical input | The exact top native owner is authoritative. Physical local input may still operate the game for a human but cannot count as or fabricate a semantic receipt. Python key translation yields and is removed from the executable cockpit route. |
| Menu choice identity | cockpit menu projection | `uilist` index/hotkey, callback, semantic stable ID | The entry-owned stable ID selects. Index, hotkey, screen position, and label may render but never bind an action. Unsupported callbacks are visible and disabled semantically. |
| Inventory item identity | inventory projection and action resolver | item UID, invlet, item name, item location hints | Existing `item_uid` is authoritative. Location is revalidated; no fallback retargets by invlet, name, row, or coordinate. |
| Dialogue response identity | dialogue projection and `dialogue::opt` | response vector position/hotkey and new response token | The response-owned stable ID is authoritative for the current dialogue scope. Conditions are rechecked immediately before effects. |
| Target identity | targeting projection | character ID, scope-local candidate token, cursor, name/coordinate guesses | Character ID or the target scope's opaque token for the same tracked creature is authoritative. Hidden/moved/stale targets reject rather than retarget. |
| Parent versus child actions | cockpit action list | cached world frame, child frame, legacy fallback | Child always wins while topmost. The parent remains breadcrumb context only and exposes no action or executable payload. |
| Unsupported owner | cockpit stop state | parent cache, raw key, OCR, guessed Escape | Unsupported is authoritative and actionless. It stops automation until a supported adapter exists. |
| NPC LLM intent | NPC behavior code and tests | `llm_intent`, cockpit semantic manager | `llm_intent` remains authoritative for NPC policy. It neither consumes cockpit actions nor changes semantic surface ownership. |
| Package claim verdict | coordinator, owner, reusable finding intake | combined-run summary, semantic-surface acceptance, stale standalone report | Each gameplay claim owns its mechanical, causality, feel, persistence, and cleanup verdict. Mixed outcomes remain independent; cockpit acceptance is enabling footing only. |
| Staffed-camp signal memory | hostile-camp drive and response | camp observer, player observation, debug setup, ordinary proximity | The eligible production camp observer and stable source/channel lead own observation truth. The ordinary drive remains authoritative for response; setup and adjacency receive no causality credit. |
| Hostile-camp discovery route | camp drive, scout/report response, package evidence | shared staffed-observer route, cannibal distance shortcut, helper-created report | The shared physical signal → staffed observer → camp memory → scout investigation → scout report route alone owns bandit and cannibal discovery. Profile policy begins after report acceptance; the cannibal shortcut is retired. |
| Night-raid commitment | cannibal hostile operation and travel | rally night gate, contact-time clock, generic drive | Night controls the atomic `rallying` → `approaching` departure only. The persisted operation owns commitment afterward; dawn before contact cannot override it. |
| Shakedown member versus player | NPC AI, dialogue/trade, hostile-operation travel | exact operation relationship, faction dislike, NPC attitude/cache, local gate | The persisted operation-scoped relationship wins before generic hostility: parley blocks offensive action until demand outcome; Fight/incomplete payment/player attack releases combat; paid departure blocks renewed aggression until normal return completion. Rolling-travel ambush is an explicit separate posture. |
| Package performance | product qualification and feel review | isolated microbenchmark, unmatched build/world, profiler | A paired current-source workload with rendered transitions owns comparison. Profiling follows a reproducible question and does not replace it. |
| Agent evidence projection | coordinator, successor worker, focused evidence query | bulk command stdout, chat summary, free-text checkpoint history, immutable artifact store | The compact identity-bound receipt owns continuation; stable indexed selectors locate evidence; digest-bound artifacts own full fidelity. Compact and full forms must resolve to the same identities and bytes. |

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
| `R-SURFACE-001` | Nested owners push/pop one exclusive renderer-neutral stack. | Native unit/integration results and Tiles/curses descriptors with fresh IDs. | Draw-created world scope, cached parent actions, unsupported owner with any action, or reused frame ID fails. |
| `R-SURFACE-002` | Exact top owner consumes a semantic request without OS input. | Native request/receipt transcript bound to binary and run. | Key injection, changed keymap dependency, focus dependency, duplicate state change, missing requested-frame identity, missing actual receipt-frame identity, or synthetic receipt fails. |
| `R-SURFACE-003` | Ordinary menus/prompts operate by stable entry ID across presentation changes. | Native menu-family test and real ordinary menu transcript. | Index, hotkey, label, disabled entry, incomplete action set, trapped partial support, or universal Escape success fails. |
| `R-SURFACE-004` | World publishes complete facts/actions only while world owns input. | Bound world/child/world transcript on Tiles and curses. | Draw-only publication, missing messages, parent action in child, or private key binding fails. |
| `R-SURFACE-005` | Active overmap cursor/selection/action/close route is semantic. | Native overmap transcript with discovered-state assertions. | World preview credited as overmap, hidden terrain leak, coordinate guess, or world action in overmap fails. |
| `R-SURFACE-006` | Inventory items and selector operations use stable IDs through base and derived selector routes. | Native inventory transcripts and item-state results for selection, details/filtering, mode commit, nesting, and permitted cancellation. | Invlet, row, duplicate label, incomplete mode actions, trapped partial support, moved/destroyed UID retarget, or stale frame success fails. |
| `R-SURFACE-007` | Dialogue response is chosen by stable response ID and condition recheck. | Native dialogue transcript with topic/effect result. | Index/hotkey, regenerated token, wrong speaker, or disabled response success fails. |
| `R-SURFACE-008` | Direction and target owners expose their own candidates and consume valid choices. | Native use-item and ranged-target transcripts. | `DEFAULTMODE` treated as world, hidden/moved target retarget, screen coordinate, or parent action succeeds fails. |
| `R-SURFACE-009` | Every required discovered input owner operates semantically; any newly discovered or incomplete owner hard-stops. | Source coverage inventory plus bound live traversal results for required owners and an unsupported control. | Treating required coverage as unsupported, arbitrary menu quota, OCR adapter, parent fallback, raw key, or unclassified owner fails. |
| `R-SURFACE-010` | Cockpit replaces views through nested, overmap, inventory, dialogue, target, and unsupported transitions. | One source-bound end-to-end structured artifact plus renderer-equivalence assertions. | Merged parent data/actions, screenshot evidence alone, synthetic/mocked surface, or legacy dispatcher credit fails. |
| `R-026` | One current-source established-base package exercises living-base, signal, bandit, cannibal, and flesh-raptor behaviors with independent verdicts and a reusable guide. | Bound current fixture/build/scenarios; native semantic receipts; claim-scoped mechanical, causality, feel, persistence, contradiction, and cleanup artifacts. | Standalone stale runs, cockpit acceptance alone, debug setup, one aggregate verdict, excluded creatures, or unbound prose fails. |
| `R-027` | A staffed camp independently observes a saved structural signal, retains a valid lead, passes bounded causal controls, and reaches an ordinary response. | Production-cadence observer/source/channel receipts, saved-source validation, controls, memory evidence, and normal-drive response. | Reusing the unchanged bootstrap, direct mutation, timestamp adjacency, ordinary proximity, player-only observation, or observation without response fails. |
| `R-028` | Paired current-source workloads qualify combined package cost including rendered local transitions and gameplay feel. | Matched build/world/scenario identity, frame pacing/update latency/native counters, CPU/RSS, variability, and feel; profiler only after reproduction. | Unmatched runs, isolated synthetic loop, no rendering, average-only summary, invented threshold, or profiler-only evidence fails. |
| `R-029` | Both camp profiles use the natural shared signal/scout/report route; cannibals wait to depart at night and continue through dawn; normal bandits demand before attacking, leave safely after payment, and fight only after release, while rolling-travel ambush remains direct. | One current-source, identity-bound natural route per profile plus branch/control artifacts: physical signal and staffed observation, camp memory, physical scout/report/response travel, night departure then daylight contact for cannibals, and bandit first-demand ordering, native payment, later safe turns and reload, refusal/incomplete-payment and player-attack combat, rolling-travel ambush, persistence, and cleanup. | Direct cannibal dispatch, fabricated report/state/contact, startup/load, prepared-contact-only proof, screenshot-only ordering, paid writeback without later safe turns, night-only contact, loss of rolling ambush, or aggregate verdict fails. |
| `R-030` | Large CAOL and worker result paths are quiet by default, preserve full fidelity, support exact pinpoint retrieval, and resume from a durable outcome-sized receipt. | Reproduce a former bulk route; compact default receipt; digest-equal full artifact retrieval; exact indexed field query; rejected digest/identity drift and terminal-without-receipt controls; fresh successor packet continuing at the recorded first open boundary. | Dropped full payload, unbound artifact, broad prose/log search, unrelated query results, arbitrary output quota, undocumented interface, or replay of accepted work fails. |

The existing run `20260826_135902` is retained as focused evidence that the old wait prototype can
emit separate frames and exact receipts. It cannot close any broad red item because its action was
still resolved through the physical binding path and it did not prove generic ownership,
renderer-neutrality, broad surface coverage, or unsupported hard stop.

## Freeze record

- Status: Refrozen
- Frozen source baseline: `dev` at `c6616640a768625ce2673de56f336ae387ee0fc9`,
  `upstream/master` at `d40cebf345cf5f042e847570d3a12ab014e6ce11`, plus the inspected
  semantic/package worktree manifest SHA-256
  `0a51ebb4559948eba1e4ff9bd3b0175caaff5935dbbabebc4e319f6723364810`, cross-checked
  2026-09-02. The additive hostile-ecology reinspection binds current dirty worktree bytes for
  `src/do_turn.cpp`, `src/bandit_live_world.cpp`, `src/bandit_live_world.h`, `src/npc.cpp`,
  `src/npcmove.cpp`, `tests/bandit_live_world_test.cpp`, and
  `bandit.r027_ordinary_response_v003_mcw`, `cannibal.r008_fire_signal_roof_lifecycle_mcw`,
  `bandit.extortion_first_demand_pay_success_save_mcw`,
  `bandit.extortion_first_demand_fight_mcw`, and
  `tmp.bandit_rolling_travel_attack_gate_probe_1860` through manifest SHA-256
  `615d2333a71afb2cc8b5231638fde666c7b72864638fd750d0ee0da2d4517a38`, inspected 2026-09-03.
- User-owned choices: CDDA is semantic authority; active presentation follows the input owner;
  child actions hide parent actions; actions and receipts bind exact frames; stable identities
  replace screen positions and menu letters; shared generic instrumentation and focused adapters use
  one protocol; coverage has no arbitrary menu count; unsupported owners stop automated play; raw-key
  and screenshot fallback are prohibited; Tiles and curses share the same surfaces.
- Evidence-implied refinements: the existing wait route is narrow precedent only; `item_uid` is the
  inventory identity primitive; current draw-time `DEFAULTMODE` activation cannot own semantic
  truth; blocking graphical and curses input backends require a wake-only semantic notification;
  action examples cannot limit a supported owner's mode-valid actions; the existing `MESSAGE_LIMIT`
  owns world-message retention; current cockpit transaction guards remain useful; `llm_intent` is a
  separate protected policy owner; the `R-SURFACE-*` namespace and its canonical DFS slices keep
  this contract distinct from earlier durable claims and give each active claim one dispatchable
  mechanistic route.
- Owner-authorized package choices: the accepted semantic cockpit is enabling footing rather than a
  final gameplay outcome; use the established-base fixture while current; combine living-base and
  hostile-ecology work where useful but judge claims independently; require causal signal controls,
  current bindings, mechanical and feel evidence, reusable findings, and a package guide; include
  follower/ambient LLM behavior, request context, Smart Zone Manager, Locker, Patrol, light, smoke,
  significant sound, bandit/cannibal lifecycles, and flesh raptors; exclude writhing stalkers and
  zombie riders pending another design pass.
- Owner-authorized hostile-ecology refinement: preserve the complete package and its history;
  bandits and cannibals use the same physical signal, staffed observer, camp memory, scout/report,
  and travel route; profile policy may diverge only after the report; retire the cannibal-only
  distance shortcut; night gates cannibal departure rather than contact; preserve the
  rolling-travel ambush and forced Pay/Fight trade surface; normal shakedown members cannot attack
  before demand or after accepted payment while returning home. Shared player-light work, writhing
  stalkers, zombie riders, automatic theft, and arrival-time night checks remain outside this change.
- Owner-authorized evidence-shape refinement: preserve full digest-bound artifacts while making
  routine CAOL and worker-result output compact and exactly queryable; hand successors an
  outcome-sized receipt and progressive read plan; treat context/retrieval shape as a first-class
  diagnostic concern without inventing quotas, retention rules, or a new evidence class.
- Evidence-implied package refinements: current source already implements staffed-camp signal
  observation and memory, so R-027 now asks for independent validation rather than a new bridge; its
  surviving selectable scenario has an older executable binding; several earlier threat scenarios
  are stale or contradicted; current semantic Dialogue/Menu/Prompt/World/Overmap owners should be
  tried before adding a package-specific interaction bridge; zero-credit bootstrap must be followed
  by independent validation.
- Evidence-implied hostile-ecology refinements: accepted R-027 remains green because its narrower
  physical observation, memory, control, and ordinary-response evidence is not causally invalidated.
  Current source still calls a parallel cannibal dispatcher, although the shared route can authorize
  both report policies. Rally departure is night-gated and later approach is not, but the dawn edge
  lacks proof. The normal turn runs NPC movement before the shakedown aftermath gate, generic
  hostility can independently target the player, and the existing parley exception neither owns the
  participant-to-player relationship nor persists through paid departure. Existing focused
  shakedown scenarios therefore retain route value without closing R-029.

After freeze, automation may close an existing red item only after its named proof passes and must
remove its red marker. It may make an evidence-implied nonmaterial clarification, or append only a
uniquely implied same-contract mechanism, ownership/proof detail, and necessary stable red claim
after a verified production finding. Existing claim identities, text, accepted work, and acceptance
strength remain fixed. Product intent, project language, permissions, user-visible behavior, and
materially different design choices return to DE-67-2 and the user.
