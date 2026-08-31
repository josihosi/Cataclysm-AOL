# Active Phase-3 projection

This file is the current decision surface for the semantic-surface cockpit DFS. Immutable reports
and repository history retain prior attempt evidence. An old hostile-ecology claim, completed route,
model choice, deadline, dispatch note, or review procedure is absent here because deleting it leaves
the current WEC contract neither unmet nor unproved.

## Contract boundary

CDDA must publish the semantic surface that currently owns input, expose only that frame's valid
actions, consume namespaced semantic actions through native bindings, and return exact receipts.
Stable IDs replace display positions and menu letters. A child hides its parent actions. An
unsupported input owner exposes no executable action and stops automated play. Tiles and curses use
the same semantic surfaces. Raw-key and screenshot-guided fallback cannot close any claim.

## Accepted foundations

- [x] Focused wait precedent — Existing native run `20260826_135902` emitted distinct
  `world`, `wait.duration_menu`, and fresh successor frames, accepted `wait.1m`, and advanced game
  minute `8904 -> 8905`. This is narrow identity/receipt evidence only because dispatch still used a
  physical binding.
- [x] Cockpit transaction guards — `CockpitRunChannel` rejects unknown, reused, unadvertised,
  stale, and receipt-mismatched actions and requires a fresh next frame.
- [x] Inventory identity primitive — `item_uid` plus `find_item_by_uid` is the authoritative
  persistent identity mechanism for item-backed inventory entries.
- [x] Structured terminal projection — The terminal cockpit uses the same `CockpitService` data
  route as the primitive caller. This foundation does not prove the new surface families.

## Current delivery frontier

- [ ] R-SURFACE-001 — Implement the renderer-neutral semantic surface stack, exact top-owner exclusivity,
  fresh frame IDs, breadcrumbs, and actionless unsupported hard stop.
  - DFS slices: `R-SURFACE-001-S001`
  - Earliest proof: world pushes a child menu and an unsupported child; parent actions disappear;
    pop restores a fresh world frame in Tiles and curses.

- [ ] R-SURFACE-002 — Replace Python/OS key translation with an exact native semantic request and receipt
  route owned by the current CDDA input owner.
  - DFS slices: `R-SURFACE-002-S001`
  - Earliest proof: the native wait vertical slice succeeds under a changed keymap and without
    foreground focus; stale, duplicate, wrong-surface, and interrupted requests fail without state
    change.

- [ ] R-SURFACE-003 — Instrument `uilist`, query/confirmation popups, and string-input prompts through the
  shared semantic protocol and stable choice identities.
  - DFS slices: `R-SURFACE-003-S001`
  - Earliest proof: duplicate labels survive filtering, reordering, redraw, selection, confirmation,
    cancellation availability, and text validation without index or hotkey targeting.

- [ ] R-SURFACE-004 — Move the world semantic surface to the real renderer-neutral world input owner and
  complete local map, creatures, terrain, zones, messages, and native world actions.
  - DFS slices: `R-SURFACE-004-S001`
  - Earliest proof: world/child/world transitions show complete world facts only when world owns
    input and return with a fresh frame.

- [ ] R-SURFACE-005 — Add the focused overmap adapter for discovered terrain, player/cursor positions,
  selected location, route state, and overmap actions.
  - DFS slices: `R-SURFACE-005-S001`
  - Earliest proof: cursor, selection, one stateful overmap action, and close all receive exact
    receipts; hidden terrain and world-action controls fail.

- [ ] R-SURFACE-006 — Add inventory-selector surfaces using item UID stable identities, item details,
  selection state, and mode-valid inventory actions.
  - DFS slices: `R-SURFACE-006-S001`
  - Earliest proof: production routes exercise the complete mode-valid action set for supported base
    and derived selector families, including selection by UID, details/filtering, mode-specific
    commit, nesting, and permitted cancellation; moved, destroyed, and stale targets never retarget.

- [ ] R-SURFACE-007 — Add stable response identities and a focused dialogue surface with speaker, history,
  responses, enabled state, and dialogue actions.
  - DFS slices: `R-SURFACE-007-S001`
  - Earliest proof: a duplicate-label response advances the correct topic after condition recheck;
    regenerated, wrong-speaker, disabled, index, and hotkey controls fail.

- [ ] R-SURFACE-008 — Add explicit direction and targeting surfaces with coordinates, candidates, stable
  targets, and focused actions.
  - DFS slices: `R-SURFACE-008-S001`
  - Earliest proof: use-item direction and ranged targeting expose no world actions and reject
    hidden, moved, out-of-range, stale, and rendered-coordinate targets.

- [ ] R-SURFACE-009 — Enforce broad input-owner coverage: shared ordinary-menu instrumentation and
  focused custom adapters, including useful debug/map editors, for every discovered input owner
  whose absence would leave the agent blind. Newly discovered or incomplete owners hard-stop as
  actionless unsupported until their required adapter is complete.
  - DFS slices: `R-SURFACE-009-S001`
  - Earliest proof: source inventory plus live traversal shows complete semantic operation of every
    required discovered owner; a deliberate unsupported control hard-stops with no executable
    actions. Required coverage cannot close through unsupported classification, and no arbitrary
    interface count is a gate.

- [ ] R-SURFACE-010 — Replace the cockpit's active presentation from the top descriptor across World,
  Overmap, Inventory, Dialogue, Menu/Prompt, Direction, Target, and Unsupported surfaces.
  - DFS slices: `R-SURFACE-010-S001`
  - Earliest proof: one source-bound run traverses
    `world › inventory › use item › choose target › confirmation › world`, plus overmap and dialogue,
    with exact breadcrumbs, isolated valid actions, receipts, renderer parity, and unsupported stop.

## Causal order

R-SURFACE-001 owns common surface identity and top-owner safety. R-SURFACE-002 owns the native action
boundary. R-SURFACE-003 proves the shared ordinary family on that boundary. R-SURFACE-004 through
R-SURFACE-008 add the required focused surfaces. R-SURFACE-009 proves complete semantic operation of
required discovered owners and fail-closed behavior for newly discovered or incomplete owners,
without an arbitrary menu quota. R-SURFACE-010 proves the integrated caller experience. Work may overlap only when
it preserves those ownership boundaries and does not claim an integrated result before its
prerequisites are proved.

## Evidence that cannot close the frontier

- Existing wait, movement, TUI, popup-trace, OCR, fixed-key, and screen-offset runs remain useful
  diagnostics but cannot prove native semantic dispatch or broad surface coverage.
- The world frame's overmap preview cannot close active overmap ownership.
- A helper/unit test cannot close a red item without the production route named in the DFS.
- A synthetic surface, direct state mutation, raw-key action, guessed Escape, parent fallback, or
  screenshot-only result receives no acceptance credit.
