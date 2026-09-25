# Owner route correction: brazier smoke-first

At the owner's latest correction on 2026-09-24, the intended smoke-first native case is to
light a fire in the brazier inside the closed room and keep it lit, then establish that the
resulting physical smoke reaches the cannibal camp before judging the response. The earlier
staged smokebomb/inventory route is stopped and receives no behavior credit.

The owner clarified the reference already contains the brazier, wood and `SOURCE_FIREWOOD`
zone; windows and curtains are closed. The native test should light the existing brazier and
keep it supplied. No firewood or additional setup should be fabricated.

The owner further clarified the original observations used ordinary fires in closed and open
rooms, ample in-game waiting, and intermittent checks of nearby movement and camp state against
player expectations. The smoke-first run should use that patient observation style, recording
exact elapsed game time and state transitions. The first-light comparison is an ordinary fire in
an open room on a separate copy, observed in the same way.

Exact action sequence: locate the existing brazier; walk the player there through ordinary
gameplay; light it with the supplied lighter and wood; verify it is actually burning; maintain
the closed-room state; then wait and check camp/movement intermittently. If lighting fails, inspect
the actual nearby brazier and ignition options and resolve that concrete obstacle. Do not infer
supplies are missing or inject a fire.

The owner then broadened the live playtest pragmatically: light the prepared brazier now through
ordinary native player action, keep it going, and observe camp response/dispatches over the next
couple of in-game days with intermittent checks. Compare actual movement and camp state with the
owner's missed-smoke and stuck-dispatch reports. Do not let incidental coordinate/inventory
bookkeeping delay the ordinary lighting action. Retain exact elapsed game time, identities, source
channel and meaningful transitions; save/quit/close cleanly at the end. If a true gameplay blocker
appears, diagnose it and continue the assigned route where possible.

## Applied direction

- Luna was instructed to preserve exclusive ownership of the current native session and inspect
  the brazier, fuel, enclosure and source state.
- Luna acknowledged the correction and confirmed the preceding `world.pickup` prompt was canceled;
  the brazier/fuel/zone state had not yet been verified and no fire/light action had been sent.
- After the owner's exact interaction sequence arrived, Luna acknowledged it. At turn/minute
  7738 the native frame was World, with inventory/pickup prompts closed and no fire action made;
  she is locating the fixture/fuel coordinates read-only before walking to the brazier.
- If the ordinary native lighting and maintenance route is available, Luna will keep the fire
  lit and retain native source, smoke, turn and camp-admission evidence.
- No staged smokebomb is credited. First-light remains unopened until smoke-first is resolved and
  cleaned up.
- The open-room first-light arm must use its separate copy and remains deferred until smoke-first
  cleanup.
- No post-install save edit, relocation or item pickup was authorized by this correction.

## Retained evidence and current ceiling

The smoke-first attempt with the migration-cancel copy reached the cockpit and directly showed
Tilda Wray id 2 transition from `ACTIVITY / ACT_MOVE_LOOT` at game time 7737 to
`CAMP_RESIDENT / ACT_NULL` at game time 7738 after one advertised wait. The wait receipt reported
`derived_bound_exhausted`; Luna did not replay it. This is zero-credit migration-cancel evidence.

The older smokebomb path is retained only as counterevidence to that route: native inventory did
not expose the supplied item, and a read-only save comparison found no smokebomb in the active
copied player save. No item was activated or moved, and no smoke or light behavior was attempted.
These observations do not apply to the owner-directed brazier route.

At record creation, Luna is preserving the live session and performing read-only inspection of
the brazier/fuel/enclosure before any fire action. Native smoke-first behavior remains unproved.

## Current native status

After the owner's immediate-action reminder, Luna reported the live run still at World, turn
5216294 / game time 7738, avatar `[3156,3449,0]`, with no fire action yet. Exact identities:
game PID 21605 (birth Thu Sep 24 23:17:49 2026), bridge PID 21565, CLI child PID 21567, session
`selected-08dc2f997e2b40a8a4e6d9b064da72a7`, binding `a14a4c0e…96674b`, run/token
`cab1f4cb…22877f`. The last action was `direction.cancel`, closing an unselected pickup prompt.
Read-only saved-map evidence identifies `f_brazier` at `[3159,3449]`, a `SOURCE_FIREWOOD` zone
at dx `+3`, with wood and light sources nearby. Luna is proceeding to the brazier now.

## Harness action path and lifecycle update

The World action surface did not advertise `world.examine`; the corresponding semantic action
was absent and no play CLI key command existed. Before implementing a new action or bypassing
that surface, the owner identified an existing native route in
`scenarios/r_surface_010.nested_projection_tiles.json`,
`scenarios/r_surface_008.direction_tiles.json`, and
`scenarios/bandit.roof_fire_horde_player_action_mcw.json`: open inventory, select the lighter,
choose its native activate action, then choose the brazier through the semantic direction child.
`world.fire` fires a wielded weapon and is not the fire-start route. No new `world.examine`
product change was made.

The owner directed a resume of the exact saved v2 world/profile after exit verification, preserving
its player position and camp, and prohibited fixture reinstallation over the saved profile. The
next run should activate the lighter through the existing native item menu/direction route, verify
the brazier is burning, step about two tiles away, then maintain and observe. Rebuild/relaunch must
retain the saved world rather than overwrite it; if the harness cannot do that safely, record the
exact resume boundary.
