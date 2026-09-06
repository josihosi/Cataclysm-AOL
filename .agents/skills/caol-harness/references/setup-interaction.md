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

The reusable isolated scenario `r029.setup_information_diagnostic_mcw` and its matching brief/charter
under `tools/openclaw_harness/charters/r029-setup-information-diagnostic*.json` exercise this route
without ecology credit. It uses a separate profile and semantic startup, avoiding profile startup
keys that can open an unrelated menu before cockpit attachment.
