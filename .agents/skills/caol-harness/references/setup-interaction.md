# Item activation and placement

`look` exposes `current_input.prompt`, `selection` and `controls` from the current native owner.
The highlighted item is separate from the available target list and from a filter string. An
unavailable selection fact is unknown; inspect the current UI rather than assuming the first action
is selected. `actions_selector` retrieves the complete matching actions with enabled state.

For a named item, open the advertised activation owner, filter by the item name, inspect the
matching selection, then confirm it. Inspect the actual placement prompt, choose the intended
destination using its available actions, and verify the resulting World facts. Bindings come from
the current interface; these examples are ingredients, not a fixed-key replay. In a native filter or text action, `act ACTION --param text=VALUE` supplies
the text parameter; the GUI filter prompt is also available. Inspect the resulting selection
before confirming: an accepted filter request alone is not proof that the visible list changed.

## GUI setup on this Mac

The installed `/opt/homebrew/bin/peekaboo` uses its default app bridge. Its `permissions status
--json` must report the capability actually being used. Read `SESSION/game-process.json` for the
bound PID and confirm that process is still the session's game. Replace PID below with that value;
never copy a historical process ID. Prefer native semantic actions where they expose the operation.

```sh
/opt/homebrew/bin/peekaboo see --pid PID --path /tmp/setup.png --json
/opt/homebrew/bin/peekaboo type "brazier" --pid PID --json
/opt/homebrew/bin/peekaboo hotkey arrow_down --pid PID --json
/opt/homebrew/bin/peekaboo hotkey return --pid PID --json
/opt/homebrew/bin/peekaboo hotkey escape --pid PID --json
/opt/homebrew/bin/peekaboo click --on ELEMENT_ID --snapshot SNAPSHOT_ID --pid PID --json
/opt/homebrew/bin/peekaboo click --coords X,Y --pid PID --json
```

`type` sends text: `{down}{return}` is literal text, not two key presses. `hotkey` sends special
keys; separate calls express a sequence, whereas comma-separated keys are simultaneous. `click`
takes `--coords X,Y`, not separate x/y flags. With PID/app/window targeting, coordinates are relative
to that target window; without targeting they are global. `--global-coords` explicitly chooses
screen coordinates with a target. Derive coordinates from the current capture/window geometry,
not arbitrary Retina scaling. Prefer a current element/snapshot when available. A successful tool
transport does not prove selection, deployment or ignition; observe the native result.

## Verify the destination

After World returns, copy `current_input.source_selector` from `look` and substitute that selector
for SOURCE, the session for SESSION, and the observed request for REQUEST. Replace 1,1 below with
the actual destination relative to the avatar in that observation (movement changes these offsets).

```sh
python3 tools/openclaw_harness/play_cli.py --session SESSION inspect SOURCE.surface.facts.visible_local --request-id REQUEST | jq 'if .ok then [.slice[] | select(.dx == 1 and .dy == 1) | {dx,dy,terrain,furniture,fields}] else . end'
python3 tools/openclaw_harness/play_cli.py --session SESSION messages --contains "fire"
```

`f_brazier` alone establishes deployment. `f_brazier` plus `fd_fire` at the destination, correlated
with the recent ignition result message, establishes the observed burning setup. Overlapping sprites,
fixture history and input transport success cannot substitute for these facts. Retain the exact
request and message time. Manufactured setup has zero ecology credit.

## Ordinary fire for smoke and camp playtests

When the owner has prepared a brazier, fuel and a firewood source zone, use them through ordinary
gameplay. From an adjacent safe tile, open inventory, select the current charged lighter, choose
its **Activate** item action, and at **Light where?** choose the brazier tile from the current
direction controls. Row 03 of `build_logs/harness-fixes-matrix20/INDEX.md` and its transcript prove
this native route: it returned World with `fd_fire` and `fd_hot_air1` on the chosen tile. Use fresh
item and direction IDs in each run. A brazier alone, an accepted command, hot air elsewhere, or a
preseeded fire does not prove this run ignited the brazier. After confirming actual fire, step
about two tiles away without stepping onto the burning tile. Check that fuel and fire persist.

The prepared smoke/light saves need no routine inventory or fuel collection. Do not open a bulk
pickup on the plank or splintered-wood stack, and do not substitute a smokebomb. If ordinary
lighter use fails, inspect that exact failure before changing setup. If the owner has already
lit the fire in the live session, attribute that input to the owner, verify `fd_fire`, and continue
the smoke/camp watch without trying to light it again.

For the owner's closed-room smoke and open-room light comparisons, use the prepared separate save
copies and ordinary fires. Keep the windows and curtains at their stated starting positions;
avoid a smokebomb or injected signal as a substitute. When fuel is genuinely absent, use only an
owner-authorized, recorded setup intervention; it earns no natural signal credit. The owner has
specifically authorized at least 1,000 splintered wood units on the tile right of the brazier if
that prepared fuel is missing. Read [behavior and movement](behavior-and-movement.md) for the
long observation and quicksave decision loop.

The reusable isolated scenario `r029.setup_information_diagnostic_mcw` and its matching brief/charter
under `tools/openclaw_harness/charters/r029-setup-information-diagnostic*.json` exercise this route
without ecology credit. It uses a separate profile and semantic startup, avoiding profile startup
keys that can open an unrelated menu before cockpit attachment.
