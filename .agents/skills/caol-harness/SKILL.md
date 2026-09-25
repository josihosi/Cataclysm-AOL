---
name: caol-harness
description: Query, explain, launch, operate, and audit C-AOL playtests through the authoritative registry and cockpit.
---

# C-AOL harness

Use the registry to select and launch a scenario and the cockpit/player CLI to operate its native
input owners. The coordinator supplies the outcome and compact charter; the worker owns the
strategy and evidence. Run CLI commands from the game worktree.

## Live playtesting belongs to Luna

Only GPT-6 Luna may launch and operate live harness sessions. A Sol or Astra worker must spawn
a GPT-6 Luna subagent for playtesting instead of running the session itself. The parent keeps
implementation and analysis responsibility; Luna receives the desired outcome, setup and
relevant existing evidence, and returns the command/output transcript plus blockers and bloat.
This applies to live playtesting, not code edits, builds, automated tests or reading saved logs.

## Choose the guidance for the current decision

Read the relevant theme when its situation arises. This is a reference map, not a sequence to
complete before playing. An existing live session can start with live operation; return to this
map when a new interaction, uncertainty or closeout question needs another theme. Linked pages
contain the detailed commands and evidence boundaries for that situation.

| Situation or question | Read |
| --- | --- |
| Choose a scenario, resolve readiness or binding, build, launch, inspect registry status | [Selection and launch](references/selection-and-launch.md) |
| Operate an existing session: look, act, controls, collect, cancel, macros, save/reload | [Live operation](references/live-operation.md) |
| Choose movement, separation or elapsed time; distinguish a weak experiment from a failure | [Behavior and movement](references/behavior-and-movement.md) |
| Apply setup interventions, handle danger, use free-text speech, inspect item ownership or zones | [Setup and interactions](references/setup-and-interactions.md) |
| Retrieve omitted facts, correlate NPC/log events, inspect receipts or compare performance | [Evidence and diagnostics](references/evidence-and-diagnostics.md) |
| Produce journal-cited conclusions, finish a run, separate claim verdicts or check cleanup | [Witness and finish](references/witness-and-finish.md) |

For a specific missing field or implementation location, the [search map](references/searching.md)
points to targeted queries. Item activation and placement examples are in
[setup interaction](references/setup-interaction.md). The [R-029 signal boundary](references/r029-signal-evidence.md)
is relevant when interpreting that feature's signal evidence.

## Keep these distinctions during play

- Choose actions from the current native input owner and its advertised stable targets. Collect
  a pending request rather than submitting it again; refresh after stale authority.
- Keep in-game safe mode off for live playtests. Check its current state after entering World,
  use the advertised `world.toggle_safemode` action only when it is on, and verify the game
  reports it off before movement or waiting. Recheck after save/reload or a blocked move;
  toggling twice would turn it back on. See [live operation](references/live-operation.md).
- When startup or interaction stops making expected progress without explanation, inspect the
  owned window and relevant native/crash logs. Distinguish loading, a modal error, semantic-ready
  and exited; a live PID is not health. Preserve PID/birth, exact error and log reference before
  safely dismissing an understood non-destructive prompt through the verified Peekaboo route;
  verify resulting state/exit. This is conditional recovery, not a polling or screenshot ritual.
- An interruption or failed command leaves the game running. Cancellation stops a request;
  quitting, finishing and declared save/reload continuation have different lifecycle effects.
- Setup and debug interventions have zero natural-gameplay credit. Match conclusions to actual
  native receipts, observed behavior and the run's evidence ceiling. Transport success, startup
  and calculation completion each establish less than feature success.
- Compact views retain handles to complete evidence. Missing displayed detail does not establish
  absence. Choose a targeted local query or Luna extraction by the question and total work, then inspect
  cited evidence rather than duplicating the investigation.
- Keep observations and contradictions through recovery. At closeout, distinguish the feature
  result, remaining uncertainty and actual cleanup; use the witness theme when reaching that point.
- For fire, smoke and hostile-camp journeys, use the player's ordinary brazier and lighter actions,
  verify a real burning tile, then give distant responses enough in-game time. Quicksave and inspect
  camp/dispatch state at useful checkpoints while keeping an inconclusive session live. See
  [item activation and placement](references/setup-interaction.md) and
  [behavior and movement](references/behavior-and-movement.md); the proven controls are indexed in
  `build_logs/harness-fixes-matrix20/INDEX.md` (row 03).
- In the prepared closed-room and open-room smoke/light saves, the brazier, firewood and charged
  lighter are already supplied. Go straight to lighter **Activate** and choose the brazier at
  **Light where?** Do not use a smokebomb or open a bulk pickup on planks or splintered wood as
  routine setup. Only investigate fuel after a concrete ignition failure. If the owner has
  already lit the fire in the current session, verify `fd_fire` and continue observation without
  repeating ignition. Row 03 above is the native control example.
