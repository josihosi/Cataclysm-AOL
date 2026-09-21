# Operate an existing live session

## Waiting redesign in progress

The short waiting interface is `tools/openclaw_harness/play` (`play.cmd` on Windows).
Set `CAOL_PLAIN_WAITING=1` in the launcher environment before creating a fresh registry session;
this selects the plain record path before the controller starts. Existing diagnostic sessions
cannot be converted by pointing `play` at them.
Bind `CAOL_PLAY_SESSION` to the exact launched session directory and add
`tools/openclaw_harness` to that worker's PATH. Use an absolute directory for both; each worker
keeps its own environment, with no shared current-session pointer. The selected Python must be
the same supported interpreter used by the harness (on the Mac, put Homebrew first in PATH).

Run `play look`, then `play wait 5m` (or an advertised duration such as `1h`). `play stop` asks
the native activity to pause. Answer the displayed confirmation with `play yes` or `play no`.
`play look` also collects an outstanding command without resending it. `play quit` requests
session termination; it does not by itself establish native save/exit proof.

Replies and `SESSION/playtest.txt` are plain text. The waiting client keeps current input state
instead of archived display snapshots; the bridge retains only its latest compact response for
recovery. The plain controller skips the legacy evidence database and receipt-history exports.
Native event-trace removal and fresh native validation are still pending, so
this checkpoint must not be presented as the completed logging redesign.

## Existing general player interface

Native gameplay here means dispatch through the game's own semantic owners. Run the player CLI
in the game worktree, locally or over SSH; it does not require a desktop-control connection.
For a registry-launched file-backed session, use the persistent player client:

Ordinary play replies show game changes, input choices, outcomes and actual performance alarms.
Receipts and routine telemetry stay in the session. Use `--diagnostics` before the command to
display them, or `request-result --request-id ID` to retrieve an earlier request. Activity pause
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

When the scenario declares a save/reload continuation, finish the saved segment with `finish`;
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

The default view names the current input owner, source selector and action selector, together with
its available navigation. Large action catalogs
show five distinct targets plus controls; use the supplied selector to search or page further targets.
It includes player health, needs, stamina, named effects and weapon state, immediate neighbouring
tiles, a terrain map, nearby entities and grouped recent messages. Omitted detail retains exact
selectors and paging. Archived history remains retrievable; references are storage handles, not
missing evidence.

## Process exit and cleanup

Saving and returning to the main menu differs from quitting the application. An actionless
`process_exited` observation reports the bound process outcome, not save durability or feature
success. After `finish`, `collect` reports actual cleanup separately.

Observe current native state, choose actions, and preserve receipts and contradictions. A first
divergence is a diagnostic anchor, not an automatic stop: inspect it, repair, improvise, rerun, or
finish according to the outcome. Stop only when the claim is settled or continuation requires a
real external decision, unavailable capability, irreversible user-data risk, binding change, or
materially different owner outcome.
