# Operate an existing live session

For short commands, put `tools/openclaw_harness` on PATH and set `CAOL_PLAY_SESSION`
to the registry-launched session directory. `play` invokes the existing player CLI:
`play look`, `play wait 5m`, `play collect`, `play stop`, `play yes`, `play no`, `play quit`.
Short wait defaults to the existing ignore-danger-and-interruptions mode; add `safe` or
`stop` to select the other existing modes. Ordinary replies and `playtest.txt` are plain
text. `look` observes the game; `collect` retrieves a pending result. Once collected, another
`collect` repeats that recorded result. Use `look` to check a running activity's current progress.
Keep the game's safe mode off during live playtests. On entering World, inspect its displayed
safe-mode state and advertised controls. If safe mode is on, use `play act world.toggle_safemode`
and verify the resulting game state says it is off. After reload or a blocked movement action,
check again. The action is a toggle, so do not issue it when safe mode is already off.
No special launch mode
or alternate transport is required. Existing long arguments remain accepted.

World controls print on the first observation and when available actions or permissions change.
Repeated `look` keeps current game facts and says `Controls unchanged`; `play controls` prints
the complete retained World controls without advancing time. A replacement game shows them again.

A menu-opening action already returns the new menu and its controls. Read that reply before
choosing the next action; a separate `look` is needed after stale/rejected input, not routinely
after every successful action. Grouped controls show a shared command template and each target's
available actions. Use the printed action names literally: for example Zone Manager closes with
`play act zone.close`, while the look cursor closes with `play act cursor.cancel`. Neither is a
World owner, so return through its advertised controls before issuing another World action.

Successful action replies are already observations. Use them to choose the next action; do not
append `look` as a routine verification step. Use `look` when fresh surroundings answer a specific
question or after rejected input. "Allowed actions unchanged" refers to the preceding control list.
Verbose item/NPC details show omitted character counts and an
existing `play inspect ...` command for the full text; retrieval does not advance the game.

Native gameplay here means dispatch through the game's own semantic owners. Run the player CLI
in the game worktree, locally or over SSH; it does not require a desktop-control connection.
For a registry-launched file-backed session, use the persistent player client:

Ordinary play replies show game changes, input choices, outcomes and actual performance alarms.
Receipts and routine telemetry stay in the session. Capture ordinary transcripts from the default
plain output; `--diagnostics` deliberately replaces it with transport JSON and is only useful when
investigating transport or telemetry. Use `request-result --request-id ID` to retrieve an earlier request. Activity pause
remains bound to the same running activity across input polls; a replacement activity or prompt
requires its own current input choice.

```sh
python3 tools/openclaw_harness/play_cli.py --session SESSION look
python3 tools/openclaw_harness/play_cli.py --session SESSION act ACTION [--target STABLE_ID]
python3 tools/openclaw_harness/play_cli.py --session SESSION wait --target-delta-game-minutes 5 --duration-action wait.5m --bound-maximum 5 --bound-basis scheduler_boundary --bound-source "chosen observation window"
python3 tools/openclaw_harness/play_cli.py --session SESSION move --east 1 --south 0 --bound-maximum 1 --bound-basis path_progress --bound-source "one chosen map square"
python3 tools/openclaw_harness/play_cli.py --session SESSION controls
python3 tools/openclaw_harness/play_cli.py --session SESSION messages --contains TEXT
python3 tools/openclaw_harness/play_cli.py --session SESSION --wait-seconds REMAINING_SECONDS call --request REQUEST.json
python3 tools/openclaw_harness/play_cli.py --session SESSION --wait-seconds REMAINING_SECONDS collect --request-id REQUEST_ID
python3 tools/openclaw_harness/play_cli.py --session SESSION resume --request-id REQUEST_ID
python3 tools/openclaw_harness/play_cli.py --session SESSION cancel --reason "stop this pending request"
python3 tools/openclaw_harness/play_cli.py --session SESSION inspect SELECTOR
python3 tools/openclaw_harness/play_cli.py --session SESSION journal --reason "What this run established"
python3 tools/openclaw_harness/play_cli.py --session SESSION finish --witness FILE
```

`controls` is read-only, even while a request is pending. It provides copyable wait/movement
requests, native recipe semantics, danger choices, and session permissions after `look`/`collect`.
An ordinary interruption stops only the macro: inspect its terminal observation and partial
progress, then choose the next native action. All action and observation failures leave the game
running. Ownership or receipt failures revoke stale input grants: `look` again before choosing an
action. A failed command does not authorize quitting, cleanup, replay, or a replacement game.
To end without making a gameplay claim, use `quit --reason "your reason"`, or send
`{"action":"run.quit","stop_reason":"your reason"}` through `call --request`. A client disconnect also leaves the game running. Only explicit native
quit, `run.quit`, `run.finish`, or requested bridge cleanup ends it.

## Save and reload continuation

When the scenario declares a save/reload continuation, first complete the native exit:
`play act world.save_quit`, confirm the save, then use the main menu's advertised
`play act main_menu.quit` and confirm if prompted. Saving to the main menu leaves the game
process alive; `finish` requires both the native save completion and process exit to enable
reentry. Record the completed segment and submit its witness with `finish` only after that exit.
`collect` reports `reentered`, then `look` exposes the restored world's new owner. The bridge handles
the declared process replacement without reinstalling the fixture. `quit` ends the entire scenario
and skips that continuation. Saving alone does not establish new-process persistence.

## Macros, pending requests and current controls

Use `wait` and `move` for ordinary player-chosen bounded operations: they take the target/offset,
native duration, bound provenance and interruption policy directly, so no temporary JSON file is
needed. The wait route uses the current advertised native wait/menu owners and never treats Pause
as a duration. For an already-defined specialized `game.*` macro, `call --request REQUEST.json`
remains available and preserves its exact request; the cockpit checks it against the session and
supplies no recipe defaults.
Give the command the finite task-time budget that remains. Submission and bounded collection then
occur in one tool execution, waking on the response, cooperative cancellation, owner/bridge death,
or the deadline. Continue from its terminal observation, or use `look` only when that observation
does not retain usable authority. If an external tool timeout interrupts the command, run
`--wait-seconds REMAINING_SECONDS collect --request-id REQUEST_ID` for that same pending request
before doing anything else; never resubmit the action or advance game time.

When a successful action response contains a complete current World/menu surface, the client
retains that result frame and the next legitimate `act` reuses its observation ID directly; no
redundant `look` is needed. Reuse is bound to the response receipt, binding and current session
generation and requires a native surface owner with advertised actions. A prompt transition,
rejected response without a successor, missing surface facts, stale generation, reentry, process
exit, cancellation, or invalid authority clears the reusable frame: follow the returned
`look`/`collect`/journal recovery route. Controls and static examples never grant live authority,
and movement must not be blindly batched.

The client owns request IDs, binding, pending responses and the last displayed frame. A pending
action needs `collect`, never resubmission. `collect` is idempotent for the latest collected
request. `resume --request-id` rebuilds only a genuinely outstanding request from its recorded
envelope without sending another action. An older recorded response stays read-only: use
`request-result --request-id` or `inspect --request-id`, never collect/resume it as if it were
new authority. Use `cancel` to stop
a pending request cooperatively; it remains available while another CLI is waiting. Then collect
the original request and look again. Cancellation leaves the game running. Input already emitted can have an unknown outcome, and a native receipt already written remains evidence. Choose from the current surface's actions; supply
`--target` only when that action advertises a stable ID. A rejected stale owner needs a fresh
`look` before deciding what to do. Nested menus are game state, not necessarily failures.

`controls`, `inspect`, `messages`, evidence and performance are read-only. They remain available
while `collect` waits and read the last retained displayed frame or exact receipt; they neither
refresh native input nor make a stale frame actionable.

When handing a session to another worker, use its recorded lifecycle state rather than a generic
`look`: `awaiting_response` or a client-side pending request requires
`collect --request-id` for that exact identity; a retained latest completed result can be replayed
with that same read-only collection; `ready` with neither requires `look`; and
`safe_to_cleanup`, `process_dead`, bridge failure, terminalization failure or reentry failure must
be reported as ended/failed rather than prodded. These are state-dependent continuations, not
interchangeable recovery suggestions.

Ordinary replies show the current menu's facts and advertised controls in plain text. World
actions show changes and messages; `play look` shows the grouped World overview and command menu.
Within the same menu, action replies list changed controls; the other controls remain as previously
shown. If controls disappear, the reply replaces the list with the allowed actions now.
`play look` prints the complete current menu. Shared item actions apply to
every listed target; a row marked `also` adds actions for that target only.
Native filtering still controls large selection lists: when filtering is required, use the
advertised filter command before choosing a target. `--diagnostics` and `inspect` retain access
to the underlying evidence. All ordinary `play` commands, including journal, inspection and
finish, use plain text; use `--diagnostics` explicitly when a program needs JSON.
The journal lists citations and offers exact detail commands rather than printing its history.
New reports reference the existing SQLite archive instead of duplicating complete streams in
JSON sidecars. Read them through `cockpit_report_reference.load_report` when full records are needed.

For evidence questions, select the needed fields with `play evidence --event EVENT --select
event,actor_name,payload.FIELD`. `--select` can be repeated. `--limit` limits records, not the size
of each record; avoid retrieving whole observations when one value answers the question.
Omitted text includes its character count and a retrieval command. New retained snapshots are
compressed; use the printed retrieval command, which also reads older snapshots.

## Process exit and cleanup

Saving and returning to the main menu differs from quitting the application. An actionless
`process_exited` observation reports the bound process outcome, not save durability or feature
success. After `finish`, `collect` reports actual cleanup separately.

Observe current native state, choose actions, and preserve receipts and contradictions. A first
divergence is a diagnostic anchor, not an automatic stop: inspect it, repair, improvise, rerun, or
finish according to the outcome. Stop only when the claim is settled or continuation requires a
real external decision, unavailable capability, irreversible user-data risk, binding change, or
materially different owner outcome.
