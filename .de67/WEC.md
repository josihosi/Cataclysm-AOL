# WEC

*User intent and language brief*

## User outcome

The CDDA cockpit should operate the interface that currently owns input. The LLM should no longer operate “the game despite its menus.”

CDDA should publish a semantic description of the active surface. The cockpit should present only that surface’s information and valid actions.

## Intended experience

When CDDA changes input owner, the cockpit changes with it:

- World shows the local map, creatures, terrain, zones, messages, and world actions.
- Overmap shows discovered terrain, player and cursor positions, the selected location, and overmap actions.
- Inventory shows stable item identities, item details, selection state, and inventory actions.
- Dialogue shows the speaker, recent dialogue, stable response identities, and dialogue actions.
- Generic menus and prompts show their title, text, choices, enabled state, selection, and menu actions.
- Targeting and direction surfaces expose their coordinates, candidates, and choices.
- Nested surfaces show breadcrumbs such as `world › inventory › use lighter › choose target › confirm firewood use`.

The LLM chooses a semantic action. CDDA resolves the native interaction and returns a receipt from the exact surface and frame that consumed the action.

## Project language and terminology

Use these terms consistently:

- **semantic surface**: an interface frame that currently owns input.
- **input owner**: the CDDA component with authority over the current interaction.
- **surface stack**: the nested sequence of active and parent surfaces.
- **frame ID**: the identity used to reject stale actions.
- **stable ID**: an identity for an item, response, choice, target, or entry that does not depend on screen position.
- **valid actions**: the semantic actions accepted by the current frame.
- **receipt**: proof that one exact frame consumed or rejected an action.
- **native binding**: CDDA’s authoritative mapping from a semantic action to native behavior.

Use namespaced actions such as `overmap.close`, `inventory.apply`, `dialogue.choose`, and `menu.cancel`.

## Boundaries

The desired coverage is “as many menus as possible.” Do not impose an arbitrary menu count.

Shared native instrumentation should cover the ordinary menu family. Focused adapters should cover custom input owners, including:

- Overmap.
- Inventory selectors.
- Dialogue.
- Confirmation prompts.
- Direction and target selection.
- Useful debug and map-editor interfaces.

Instrument CDDA’s native UI components. Do not create a separate OCR adapter for every menu.

A child surface must hide actions from its parent surfaces. Inventory, dialogue, or a prompt must never expose world movement.

Do not assume that Escape or another raw key has universal meaning.

Do not permit screenshot-guided raw-key fallback.

The same semantic surfaces should support graphical and terminal or curses rendering.

The cockpit mockup establishes the view shape. It does not settle final styling or implementation mechanics.

## Decisions

- CDDA is the semantic authority.
- The cockpit replaces its active presentation when the input owner changes.
- Each frame exposes only actions valid for that frame.
- Actions carry a frame identity.
- Receipts identify the consuming or rejecting frame.
- Stable identities replace screen positions and menu letters.
- Shared generic instrumentation and focused custom adapters use one protocol.
- The overmap loop is a proving route, not the delivery ceiling.
- Coverage should extend across every discovered input-owning interface whose absence would leave the agent blind.
- If CDDA reaches an unsupported input-owning interface, the cockpit must expose no executable actions.
- An unsupported interface must stop automated play until semantic support exists.
- The cockpit must not fall back to parent actions or raw-key control.

## Research outcome

No online research was needed. The owner’s concept and reaction to the cockpit mockup settled the language and boundaries.

## Prototype or reaction questions

None remain.

The cockpit mockup established separate World, Overmap, Inventory, Dialogue, and nested-prompt views. The owner’s reaction expanded the intended coverage beyond an overmap-only slice.

## Handoff to DE-67-2

Target repository: `/Volumes/CodexBulk/Schanigarten/workspaces/Cataclysm-AOL-hostile-ecology-dev`

The owner reports that the wait menu already emits separate semantic frames and duration actions. Phase 2 must verify that precedent before relying on it.

Phase 2 must inspect the actual input-owner families and existing cockpit protocol. It must preserve broad menu coverage and the hard-stop behavior for unsupported interfaces.

The view study is available at `/Users/josefhorvath/.codex/visualizations/semantic-surface-cockpit.html`.

## Additive owner refinement: coherent bandit and cannibal hostile ecology

This refinement is additive. Every compatible semantic-cockpit and playtest-package clause above
remains binding; the clauses below do not replace the existing WEC, scenarios, reports, evidence,
or history.

### User outcome

Preserve the existing hostile-ecology playtest package while making cannibal and bandit responses
feel like one coherent system. Cannibals and bandits must discover and investigate player-created
signals through the same physical route. Their behavior may differ after the camp receives the
scout report.

### Intended experience

- A bandit camp and a cannibal camp use the same physical signal observation, camp memory, scout
  investigation, report, and travel model.
- A cannibal response waits at its rally until night. After the cannibals depart, the response
  remains committed even if dawn arrives before contact.
- A normal bandit contact presents the shakedown before the bandits attack. Payment ends the threat
  and lets the bandits depart safely. Refusal, incomplete payment, or a player attack starts combat.
- A favorable rolling-travel encounter may remain a direct bandit ambush without a shakedown.
- The current forced payment interface is acceptable. The feature does not need literal automatic
  item theft.
- A paid bandit group must not attack again because generic faction hostility runs before or after
  the shakedown lifecycle.

### Project language and terminology

- Use `physical signal`, `staffed observer`, `camp memory`, `scout report`, `night raid`,
  `shakedown`, `payment`, `refusal`, and `rolling-travel ambush` for the relevant states.
- `Leaves at night` means that the night check controls departure from the rally. It does not
  require another night check at contact.
- `Kills only after refusal` applies to normal shakedown contact. It does not prohibit the accepted
  rolling-travel ambush.

### Boundaries

- Preserve the complete existing playtest package, scenario manifests, reports, and evidence. Do
  not overwrite or delete earlier evidence.
- Reopen an existing claim only if the new implementation or proof requirement invalidates that
  claim's accepted evidence. Preserve the earlier status and evidence as history.
- Keep the Writhing Stalker, Zombie Rider, and shared player-light package outside this bandit and
  cannibal change.
- Do not redesign the payment interface.
- Do not add an arrival-time night check or make cannibals abandon a raid because daylight begins
  after departure.
- Do not remove the rolling-travel bandit ambush.
- Do not treat helper-created reports, direct state fabrication, or startup-only scenarios as
  natural gameplay proof.

### Decisions

- Cannibals must use the shared signal route only. Remove or retire the cannibal-only distance
  shortcut instead of repairing it as a second response model.
- Cannibals wait for night before leaving the rally, then remain committed.
- The rolling-travel bandit ambush is intentional.
- The forced Pay or Fight interface is intentional.
- The remaining concern is normal shakedown turn ordering. A bandit must not become hostile before
  the demand opens or after accepted payment while the group departs.

### Research outcome

No online research was required. The owner decisions and the current code observations define the
intended experience.

### Prototype or reaction questions

None. DE67-2 can specify the natural gameplay proof for the turn-order concern.

### Handoff to DE-67-2

- Target the Mac Mini hostile-ecology `dev` worktree. Verify its current path, branch, HEAD, and
  dirty state before authoring the specification.
- Import this WEC without replacing the existing `.de67` package.
- Inspect the shared signal route, the cannibal-only direct dispatcher, hostile-operation handoff,
  normal shakedown dialogue ordering, generic NPC hostility, paid departure, and the existing proof
  package.
- Keep existing proof artifacts. Extend the package with the smallest natural route that
  distinguishes a safe demand and safe paid departure from premature aggression.
