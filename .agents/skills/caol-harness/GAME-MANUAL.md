# C-AOL game manual

This is the skill's reusable game knowledge and assessment manual. Use the section matching the
current question; do not read every reference before playing. The original run logs, saves,
receipts and code remain the evidence. Add a finding here when it changes how a later test
should be run or judged. Do not paste a worker transcript or duplicate a run report.

## Start with a story; reuse proven command batches

Before a first run, provide the scenario story, compatible starting state, intended observable
outcome and relevant known controls. Mark borrowed or untried segments honestly. After a successful
run, distill its actual command/output transcript into concrete ordered batches alongside that
story. Preserve CLI syntax and parameter names, not just instructions such as “drop the gold.”
Use existing supported wait/move/selection mechanisms; a list of commands is not automatically
an executable batch-file format.

Each reusable segment gives its compatible entry state, exact commands with only live values
parameterized, the existing query that obtains those values, and its useful endpoint query.
Late-bind current session/frame/UID/letter/prompt values; never copy historical authority.
Break at a decision, unexpected prompt, rejection, danger or performance alarm. Preserve pending
request identity and collect it rather than replaying submitted input. Merge adjacent compatible
waits with no intervening decision and stop once the outcome is proved.

The actual player's start handoff contains only the selected build/save/scenario, current
checkpoint and delta, relevant story and command segments, and remaining proof. Link accepted
history and original receipts for retrieval; omit obsolete builds, superseded diagnosis, completed
branches and duplicated manuals from the ordinary start. Keep any historical fact that changes
this run's decision or evidence ceiling. Parent and input helper receive the same current segment.
Repair a mistaken query against retained evidence; do not replay accepted gameplay to manufacture
a recipe. Distill other successful arms from their existing logs as their controls are needed.

Batching must retain performance visibility. Existing collection reads run-bound native turn
telemetry while input is pending and retains alarms and recoveries across internal polls. Examine
its `turn_assessment`, including transient alarms; an unavailable trace or unconfigured assessment
is not a clean performance result. Native turn timing, CPU/RSS and command/transport latency are
different measurements. Preserve scenario-bound thresholds and raw evidence. Report slow turns
and stalls to the coordinator before blindly extending another batch. Collection now returns a
new alarm promptly while retaining the pending request; this returns control to the worker, not
an automatic cancellation of native gameplay. Inspect the evidence and collect that same request.
The existing waiting alarm checks a 100-turn mean against 100 ms; it is not an individual-turn
spike alarm. General spike/slow/stall assessment requires the scenario-owned performance config;
without it, those measurements remain unassessed. Read-only `performance` remains available while
a request is pending. Monitoring must not require splitting every quiet native wait into short
gameplay actions.

## Contents and agent routing

| Current question | Go to |
| --- | --- |
| What did an NPC actually do, and why? | [Assess an observed action](#assess-an-observed-action), then [Evidence and diagnostics](references/evidence-and-diagnostics.md). |
| What should each NPC watch interval check? | [NPC interval check](#npc-interval-check). |
| I am in the standardized base; what can I assume? | [Standardized base orientation](#standardized-base-orientation). |
| What should a realistic loot trip and return check? | [Survival trip assessment](#survival-trip-assessment). |
| How should I move, wait, or handle an interruption? | [Wait and observe](#wait-and-observe), then [Behavior and movement](references/behavior-and-movement.md). |
| How do I show the player more without changing signal provenance? | [Viewing setup](#viewing-setup), then [Setup and interactions](references/setup-and-interactions.md). |
| How do I create or adapt a scenario? | [Selection and launch](references/selection-and-launch.md), its scenario-authoring links below, and [Verified save templates](#verified-save-templates). |
| Why did a prepared save open to a blank screen or Main Menu? | [Check the effective profile](#check-the-effective-profile), then [Selection and launch](references/selection-and-launch.md). |
| A save reload opens the known item-sorting debug dialog and the CLI stays at Loading | [Dismiss the known reload dialog](#dismiss-the-known-reload-dialog). |
| Which save should start a smoke or light arm? | [Verified save templates](#verified-save-templates). |
| Must I recheck a prepared save's entire initial scene? | [Reuse a verified baseline](#reuse-a-verified-baseline). |
| How do I light, extinguish or check a prepared fire? | [Fire in a prepared brazier](#fire-in-a-prepared-brazier), then [Item activation](references/setup-interaction.md). |
| How should the CLI show the game? | [Readable play output](#readable-play-output). |
| Which game facts have already been learned? | [Current reusable findings](#current-reusable-findings). |
| What should I record, revise, or prune? | [Add, prune and correct notes](#add-prune-and-correct-notes). |
| Are detailed logs confined to harness sessions? | [Logging gate audit pending](#logging-gate-audit-pending). |

For scenario authoring, reuse the existing registry and a relevant source manifest returned by
its exact scenario query. [CONTROL_LOOKUP](../../../tools/openclaw_harness/CONTROL_LOOKUP.md)
is the detailed control catalogue; [the playtest matrix](../../../build_logs/harness-fixes-matrix20/INDEX.md)
retains verified interaction examples. Link those originals instead of copying old controls.
Validate a candidate through `scenario_registry.py::validate_manifest` before selection; record setup
changes separately from native behavior. The scenario-consolidation ledger item owns broader
catalogue reduction; this manual does not create a parallel registry or erase variants.

The [skill entry point](SKILL.md) routes launch, live controls, evidence, setup and closeout.
Only Luna operates live sessions. Sol can analyze source, logs and saved data or repair code.

## Standardized base orientation

### Josef's two-brazier base save

Use a disposable copy of the [verified template](#verified-save-templates). The closed-window
archive is the basic unlit start; progressed raid saves carry their own fire, knowledge and
actor history. Match the selected save identity and current World before applying this map.
The normalized house spans `[3148,3440,0]` through `[3163,3455,0]`. Coordinates below describe
that geometry, not every procedural house. After play begins, old positions and fields are
history; check the current player, hazards and pending input before walking.

| Place | Native tile | Use |
| --- | --- | --- |
| Ground-floor brazier | `[3159,3449,0]` | Indoor fire; closed windows make outside discovery smoke-only. |
| Verified downstairs working tile | `[3158,3449,0]` | Brazier is East from here. |
| Roof ladder, ground endpoint | `[3149,3454,0]` | Walk here to ascend, rather than using a staircase that leads down. |
| Roof ladder, upper endpoint | `[3149,3454,1]` | Confirm this tile after `world.level_up`. |
| Roof working/watch tile | `[3155,3448,1]` | Roof brazier is Northeast from here. |
| Roof brazier | `[3156,3447,1]` | Inspect actual fuel and fire before ignition. |

### Walk upstairs and approach the roof fire

From the current ground-floor tile, inspect native terrain and take a passable local route to
`[3149,3454,0]`. The recorded successful route does not supply a fixed downstairs walk from
an arbitrary starting position. At the ladder use `world.level_up`, then check the player is
on `[3149,3454,1]`. A down staircase elsewhere is not the roof ladder.

From that verified roof endpoint, six East and six North steps reached `[3155,3448,1]` in
the successful run. Check hazards and passability as you walk; use the coordinates to adapt
if your position differs. Do not step onto the fire. Native proof is in
[R040's route checkpoint](../../../build_logs/first-smoke-040/continuation-checkpoint-20260929.md):
receipt67 ascended at turn5,295,930; receipts75–97 reached the watch tile at turn5,295,951.
The same ladder supported descent at turn5,295,928. Level-down from roof shingles at the
watch tile was a no-op and is a recovery note, not a route step.

R051 applied this route on the basic-template continuation: native `world.level_up` reached
`[3149,3454,1]` at turn5,216,278. From the roof working tile, the current lighter menu opened
**Light where?**, Northeast began `ACT_START_FIRE`, and turn5,216,353 showed the roof brazier's
`fd_fire` plus the successful ignition message. The compact
[native action records](../../../.de67/task-logs/r051-verified-roof-actions-20261002.json)
retain exact response paths and hashes. That run advertised item-action1 successfully; the
old downstairs rejection of that ID is not a general ban. Always use the current menu.

### Light, maintain or extinguish the fire

Inspect the intended tile with the [World Look Cursor](#fire-in-a-prepared-brazier), rather
than searching Inventory for the furniture. Check actual wood/fuel and a charged lighter.
For an unlit brazier, use the lighter's current **Activate** menu target, then the current
**Light where?** direction: East from the downstairs working tile, Northeast from the roof
working tile. Read any firewood-source prompt and confirm the prepared fuel. Verify native
`fd_fire` at the actual brazier. Historical menu IDs are not reusable controls. The
[fire procedure](#fire-in-a-prepared-brazier) retains the verified lighter and extinguishing
receipts, current control pattern and residual-smoke checks.

For a maintained roof fire, retreat safely without stepping on fire and remain near it.
Check the actual stock, source-zone coverage and whether fuel is replenished as native time
advances. Josef expects the original templates to contain enough wood and notes that leaving
for downstairs can prevent nearby auto-refuelling. These are maintenance checks, not proof
of the exact refuelling radius or a reason to resize zones before a failure is observed.
If wood remains but replenishment fails, retain the player/fire/source positions and current
zone facts for diagnosis. Fire presence alone does not prove exterior smoke or light reach;
follow the [signal visibility checks](references/smoke-light-visibility.md).

To extinguish, stand adjacent and open the brazier's native Examine menu, choose the current
enabled **Extinguish fire** entry, then verify `fd_fire` is absent. Smoke and heat may remain;
check their decay before calling the signal off. Do not replace this with an Inventory search.

### Wait, save and return

Use [wait and observe](#wait-and-observe) for the intended next stage and interruption mode.
Check actual game time, relevant actor IDs and meaningful changes; repeated unchanged motion
or a rejected route needs inspection. For long disposable roof observations, verify
`DEBUG_LS` is active as survival setup; do not toggle it off. Keep ordinary vision for ordinary
detection proof.

Use [Save a checkpoint](references/live-operation.md#save-a-checkpoint) without a movement
step. A skipped quicksave is not a fresh write: verify the saved turn. On returning from a
trip, check current doors, both braziers, remaining fuel and actual signals. For loading errors,
use [the known recovery](#dismiss-the-known-reload-dialog).

## Assess an observed action

Start with the behavior being tested, not its status label. The coordinator's short charter
should identify the current stage, expected observable next outcome, important actors and the
relevant section here. For a committed raid, progress means approach through the terrain and
pursuit/attack of valid occupants; “committed_contact” alone is only a phase. Scout concealment
is a different stage. Give workers a trajectory and evidence boundary, not an action script.

Use `play_cli evidence` or the bound log query for exact run/actor/turn facts; `controls` gives
source handles and available queries. Follow the response's full-evidence handle when its compact
summary omits a needed fact. The [evidence reference](references/evidence-and-diagnostics.md)
explains retrieval and [live operation](references/live-operation.md) explains current owners,
menus and pending requests. A missing field is unknown, not proof that nothing happened.

## NPC interval check

At a meaningful transition or when progress becomes doubtful, compare a relevant actor's
previous and current native turn-bound tile/action with the expected outcome. Use local tiles
for nearby entry/combat; an OMT cannot distinguish inside from outside a house. For a raid,
retain the initial ally/attacker IDs so a missing member is noticed. Retrieve terrain/door,
mission/goal/path, gate inputs, health or saved roster when needed to explain the divergence.
A saved-state claim requires a verified fresh saved turn. Do not dump every field on every tick.

Repeated two-tile repositioning without assault, a hold-off gate during committed attack, or a
missing roster member changes the next action from more waiting to targeted inspection. Preserve
the scene and report the observed divergence, its original handles and the unresolved question
to the coordinator. It preserves the unfinished outcome and routes the repair. A progressing
actor may revisit a tile while fighting or avoiding an obstacle; a justified retreat is not a
stall. Compare actions and state changes, not a position pair alone.

Example: run06099f… had NPC5 at `[3149,3442,0]` and `[3150,3442,0]` on successive observations,
committed raid contact but `hold_off`/`combat_forward=false`, and NPC3 absent from the later roster.
That contradicts expected assault and warrants inspection without another hour of waiting.
It does not prove the entry route or who killed the Casey-named corpse. First local tile was
retained only after arrival; the viewer had clairvoyance. Original report:
[retained finding](../../../.de67/task-logs/review-owner-b52302a9f956-20260926/mutation-suggestions.md.before).
Use this as a reasoning example, not as the current state of another session.

Screenshots can reveal where to investigate; native actions/saves/logs establish movement,
combat and identity. If the needed fact is unavailable, name the exact missing observation and
improve its existing retrieval/trace route rather than adding repeated warnings or fabricating it.

## Survival trip assessment

Use a separate disposable save for a travel arm. Record the initial save identity, intended
signal, player setup and relevant NPC IDs before leaving. A long stationary no-fire control
must stay unlit and unmoved; travel would change its question.

For a movement or loot-trip arm, verify Safe mode **Off** after loading. Use recorded
`DEBUG_LS` and, when that arm permits altered visibility, `DEBUG_CLOAK`; keep an ordinary
visibility arm separate. Clear incidental blocking zombies with the authorized recorded
debug HP-to-zero or melee setup so a routine route is not abandoned as a false blocker.
Locate the actual doors in the north-center part of this house from current native
terrain and coordinates; do not aim a walk into a wall or closed curtain. Pass through
the doors by checked local steps, use overmap travel for distance, and return to local
steps near the building when overmap travel cannot choose the entrance. After a rejected
move, inspect the tile and player position before the next action. Repeating a failed
wall/furniture move is a route diagnosis, not progress. Record the successful correction
here when a worker finds one.

At departure, while away in each relevant campaign phase, and after returning, compare the
actual saved player OMT and local tile, carried loot, health, hunger, thirst, sleep and active
debug traits. Follow scouts, raiders and camp allies by stable actor ID even when outside player
sight. Check each actor's tile, mission or job, target/action if exposed, health and roster
presence. Check camp site knowledge, report source, outing members, route and dispatch against
what the player and signals actually exposed. A phase label alone cannot establish pursuit or
combat; a missing actor requires a fresh roster and death/corpse inquiry.

For the prepared two-brazier base, inspect fuel, fire and smoke at the indoor and roof tiles
before departure and on return. If the intended signal went out, use the ordinary lighter
interaction from [Light the brazier](#light-the-brazier), then verify the native fire/smoke and
saved turn. Already burning needs no ignition. Do not project the departure fire state onto
the return scene. A proposed auto-walk/relight macro must check the current tile, fuel, fire,
danger and targeting before input; stop on an unexpected scene.

`DEBUG_LS` holds hunger, thirst and sleepiness stable; `DEBUG_CLOAK` is a separate invisibility
trait in `data/json/mutations/debug.json`. Record them separately because invisibility can
change hostile sight. Melee skill or an axe can be recorded survival setup. Debug HP-to-zero
on incidental travel zombies is setup and cannot prove native combat. To assess ordinary
survival, include an interval without life support and read player status rather than assuming
that a debug-aided trip tested food or water management. Preserve encounters, damage and
interruptions; use the wait mode that matches the fact being observed.

Use existing run-bound descriptors, exact event handles and fresh saves first. If an actor
action or attacker is missing, request one harness-enabled trace scoped to the IDs and turns
that decide the question. Avoid normal-game verbose logs and per-turn full-state dumps. At
closeout, retain raw evidence for open claims and prune only verified redundant temporary
exports, as described in [Logging gate audit pending](#logging-gate-audit-pending).

## Smoke and light visibility

For brazier range formulas, fire growth, sampling/dispatch timing, terrain and cross-level
visibility, use [Smoke and light over time](references/smoke-light-visibility.md). An early weak
packet is not the mature fire's maximum; compare changed samples before ending a visibility test.

## Wait and observe

Choose the existing wait mode for the observation you need. These short CLI examples use a
five-minute interval only as an illustration; use the interval relevant to the test.

Merge adjacent waits with the same interruption policy when no action, observation or decision is
needed between them: `play wait 1h safe` twice becomes `play wait 2h safe`. Likewise, use one
advertised three- or six-hour duration instead of several shorter waits when only the endpoint
matters. The native duration must be available in this scene (hour choices require a watch);
use the largest supported duration that fits the intended interval if the exact total is not offered.
The short CLI maps `2h` to native `wait.2h`; it does not invent arbitrary menu durations.
A longer wait still uses the chosen interruption policy. Collect a pending request; do not issue
another wait while it is running. After an interruption, use actual elapsed time to choose the
remaining interval rather than restart the full duration. Do not merge across a required fuel
intervention, decision, signal-on/off timing measurement, or unresolved prompt. Safe mode here
protects the player; it does not promise detection of every distant NPC event.

Read the resulting frame and retained event evidence at the next useful decision. No automatic
look/save/roster sweep between quiet chunks, and no further wait once the requested outcome is
established. Use the existing save/finish route at that point. Proven ordinary batching needs no
separate trial run; new harness execution behavior needs its own affected controls.

- `play wait 5m safe` (`handle_classified_non_dangerous`) handles recognized harmless
  interruptions and returns control on native near-hostile, pain or attack warnings, actual
  damage, unknown safety or unavailable recovery. Use it when you need to inspect the first
  attack, health change or other consequential event. Earlier R013 companion speech stopped a
  wait before the typed-distraction repair; preserve that old run as history, not current policy.
  A direct `play act wait.5m` chooses the native duration and bypasses this safe-wait handler.
- At a known harmless `Stop waiting?` prompt, select its current `IGNORE` option (`play ignore`,
  or native `I` through the verified input route) when repeated chatter should stop. On this
  native activity prompt, `NO` continues only this time; `IGNORE` continues and remembers that
  distraction **type** for the current activity and its backlog. Reobserve the surface and
  current state before continuing. Assess a new proximity threat or damage prompt separately.
- `play wait 5m stop` (`stop_on_interruption`) deliberately stops at each interruption when
  every one matters to the observation. It stops easily during routine waits.
- `play wait 5m ignore` (`ignore_danger_and_interruptions`) continues through supported
  danger and damage prompts. Use it deliberately when the test should observe combat unfolding,
  including a possible death. Bare `play wait 5m` currently selects this permissive mode; spell
  out the mode so the intended observation is clear.

For discrete turn actions, the [R043 counted-action pilot](references/smoke-raid-replay.md#counted-actions-after-the-r043-pilot)
shows the supported exact-request repeat and its limits. It does not replace these duration waits
for passive raid observation; Tab is native autoattack and can strike a reachable hostile.

A cannibal raid attacking and killing the player or camp occupants can be the expected successful
outcome. Do not classify death itself as a game bug, harness bug or failed raid test. Compare the
actual actors/actions/outcome with the test goal. Retain attacker/target identities and combat,
damage and terminal evidence where available; unknown attribution remains unknown, not proof of
a different attacker or a reason to change raid behavior. A terminal death surface may end the
wait recipe before its requested duration: inspect that surface and the native evidence before
interpreting a generic macro error as a product failure.

In the R013 signal-off wait, eating pemmican left a native **Consume item** picker open while the
bridge still owned the original one-minute wait request. The game was alive; a second collector
or new game command would have conflicted with that request. Luna identified the exact PID and
current picker, sent one Escape through the verified Mac input route, saw World and “You finish
waiting,” then collected the original request. Check the actual current picker and session
before using this recovery; the successful key delivery alone does not establish a fresh save.

Record starting and ending turns, interruptions and the observed outcome. On an inspection stop,
assess the relevant health and NPC state, then choose a continuation from current live controls;
resume only the remaining interval instead of replaying elapsed time. Collect a pending request
before issuing another input. Use meaningful quicksave/actor checks while the character is alive;
a completed death outcome does not require an impossible post-death quicksave.

After a signal creates a real roster and the fire is extinguished, a short home read of
`no_signal_source` says only that the signal is gone. It does not settle later terrain scouting.
In the R013 closed-window save, the source-backed terrain scan runs on 60-minute boundaries and
advances one of 12 nearby offsets per pass; the saved minute-7775 home read preceded the next
scan at minute 7800. Keep one safe owned session through the next relevant scan and compare the
same actor IDs, lead source, outing, return and report at fresh saved turns. Use shorter checks
around a real transition or danger, rather than relaunching after each quiet five-minute read.

Retrieve existing run-bound evidence before adding logging. Any missing diagnostic combat or
actor detail should use the existing harness-enabled capture and exact retrieval route, not
unconditional normal-game logs or repeated per-turn state dumps. Ordinary gameplay must not incur
verbose playtest tracing; a proposed capture change needs a harness-off counterexample as well as
proof that the selected harness run captures the needed fact.

## Viewing setup

Only when expanded viewing is useful, a separate disposable viewing run can use both `DEBUG_CLAIRVOYANCE_PLUS` (Debug Clairvoyance) and
`DEBUG_CLAIRVOYANCE` (Debug Clairvoyance Super). From the current World input owner, open the
advertised debug menu, choose its mutation route, filter for `clairvoyance`, inspect the current
two named entries and choose each through its fresh stable target. Close the menu, pass one real
native turn with `world.pause` or another advertised turn action, and verify both mutation IDs
and the expanded view. Do not reuse historical menu entry IDs. Record each existing or added
mutation and the before/after turn. The mutations are setup, not evidence that the fire or smoke
signal occurred. `src/do_turn.cpp::live_bandit_make_gate_input` also reads the
player's view, so clairvoyance can change local exposure and hostile hold-off behavior. Preserve
a clean no-clairvoyance copy for a concealed-player assault claim. Compare exact gate logs; being
indoors or in darkness does not by itself prove lack of exposure.

## Verified save templates

| Template | Verified contents | Use and limits |
| --- | --- | --- |
| `.userdata/reference-saves/archives/josef-basecamp-closed-windows-20260924-122642.zip` (SHA-256 `4b11469931507c49f0b373934730fed6b729b7a29b87c6f78eca0f728e8320c3`) | 45 save files; TestCamp01 at OMT `[131,143,0]`, closed windows, unlit indoor brazier `[3159,3449,0]` and roof brazier `[3156,3447,1]`; saved NPC2 Tilda has `ACT_MOVE_LOOT`, NPC3 Casey has `ACT_NULL`. No cannibal camp site is present. | Copy the archive into a disposable profile. The indoor z=0 brazier is smoke-only for outside discovery with windows closed; use verified roof or separate open-window geometry for exterior light. The archive is a baseline, not proof of hostile site eligibility or stocked player inventory. Inspect native fuel, lighter, roster and window state before action. |
| `.userdata/reference-saves/josef-basecamp-open-windows-20260924-122356/` | Owner-created corrected open-window reference. Its README says all building windows are open and the source save is unchanged; Josef also describes this comparison setup as having the building doors open. | For the later open-versus-closed signal runs, copy this save separately and verify actual windows, doors, signal source, camp setup and file identity before play. The README's window claim is owner-confirmed, not a native replay or proof that every door is open. |
| Controlled smoke geometry in `build_logs/first-smoke-008/controlled-site-profile-stage.json` | A 45-file disposable copy of that archive; only `overmaps/o.0.0.zzip` changes, adding cannibal camp footprint/terrain at `[135,137,0]`. Player save, NPC activities, eight existing camp objects and map data remain unchanged. The archived player save SHA is `037c9bf694be280cb19fe607ef691429593120245c2741d6b0906dfa8833d247`. | Recorded setup for a camp-eligible smoke arm. It adds no fire, smoke, scout knowledge or contact. Do not copy the separate migration-cancel or smokebomb transforms. Recheck hashes and semantics on each new copy. |
| Saved raid checkpoint in `build_logs/first-smoke-009/luna-handoff.json` | Minute 8734, turn 5276059, existing gen2 raid `#hostile:2`, reserved NPCs 5/7. | Continuation only. Do not call it a fresh unaware smoke start, and never overwrite the original profile. Use a separate clean start for combined light-plus-smoke or any arm contaminated by earlier fire/contact. |
| Preserved minute-8734 raid checkpoint | [Starting scene](../../../build_logs/first-smoke-010/starting-scene.json) records the 16×16 house shell, doors, braziers, player and saved NPC IDs/coordinates. Its [57-file manifest](../../../build_logs/first-smoke-010/starting-save-manifest.json) binds the preserved source profile after graceful quit. The earlier R010 [copy receipt](../../../build_logs/first-smoke-010/disposable-copy-receipt.json) binds its separate prelaunch bytes. | Copy the preserved source into a new disposable profile and match the complete manifest before using these baseline facts. The already played R010 profile has changed; do not treat it as a clean start or proof of a repaired native assault. |

These are evidence-bound examples, not the current state of any open game. Read the run's latest
save and native World before using a template; a stale instruction such as “fire not yet lit”
does not override a burning brazier in the selected session.

## Reuse a verified baseline

When a disposable save is a byte-exact copy with a retained manifest, compare its file set and
full hashes with that manifest, then verify the launched userdir, World, saved turn and source
binding. Those checks transfer the recorded initial map, NPC and inventory facts to this copy;
do not repeat a full initial-state audit merely because a new worker or game process attached.

Check mutable live facts once on entry: current safe mode, debug mutations, pending input and
actual World/turn. After a time-advancing action, use native metadata and fresh saved turns to
follow each relevant NPC by ID, including unseen actors. A changed save hash, uncertain copy,
reload error or contradiction is a reason to inspect the affected fact again. Keep baseline,
current state and planned setup separate.

## Check the effective profile

`--profile` takes a name under `.userdata`, not a filesystem path. Pass
`first-smoke-010-no-clairvoyance-checkpoint-20260926`, for example, without a directory prefix.
Before game input, verify the resolved userdir has the intended `save/TestSetup00` files and
expected save hashes, then require a same-run native World descriptor. A live PID or startup
`ok` alone does not establish that the intended world loaded.

In failed run `60f29426e764a36eb5f41da49f3310ddcbe22023b7c1313a77ad6aada6dc5a3c`, an
absolute `--profile` path was sanitized into a new directory with zero save files. The prepared
profile still had `TestSetup00`; startup recorded Main Menu and the bridge ended with
`pre_descriptor_no_progress`. See that run's `startup.result.json`, bridge `status.json`, and
`startup_harness.py::sanitize_profile_name`. Preserve the failed run and relaunch with the
profile name after confirming the effective save. The blank launch is not gameplay evidence.

## Dismiss the known reload dialog

Use the [debug error catalogue](references/debug-errors.md) for known and new dismissible
warnings: record/report, Ignore, reobserve and continue. Do not wait for coordinator triage.
A different debug message is another record to assess, not a new approval gate.

Startup can remain at Loading with no World descriptor while a dialog is open. Current CLI
startup diagnostics can expose the dialog; use its advertised recovery action when available,
or the verified Peekaboo route described in the catalogue. A live PID or delivered key alone
does not establish recovery. Preserve the log/save and check the resulting native state.

Require a same-run native World descriptor and correct userdir/save identity before gameplay
input. A successful key delivery or visible map alone is not that proof. If the bridge has
already exited, preserve its run and use a fresh source-bound session after closing the owned
game. Do not clean the owner's save or suppress the log merely to proceed with this playtest.

## Fire in a prepared brazier

Check the actual brazier tile through the **World Look Cursor**: choose the advertised
`world.look` action, move its cursor one tile east from the verified adjacent
`[3158,3449,0]` tile with `cursor.east`, then `cursor.confirm` on the brazier at
`[3159,3449,0]`. The returned World frame exposes that tile's native furniture and field
data. `f_brazier` identifies the furniture; `fd_fire` means it is burning. `fd_smoke` and
`fd_hot_air1` show smoke and heat, which can remain briefly after the fire goes out. This
Look Cursor action differs from the read-only `play look` command, which may omit local tile
fields. Inventory lists carried items and cannot tell whether the brazier is burning. Record
the game turn of each check. Avoid another ignition when `fd_fire` is already present.

From the verified adjacent tile in this saved scene, the working CLI actions were:

```text
play act world.look
play act cursor.east
play act cursor.confirm
```

Read each returned surface before the next choice. If the player is elsewhere, use the current
cursor directions instead of copying `cursor.east`. A fresh saved map and the exact tile's field
record are another check when the Look Cursor does not include local tile facts.

### Light the brazier

In the prepared disposable arm, inspect the actual brazier tile, wood/fuel, charged lighter and
window/roof geometry. From an adjacent safe tile, open the current inventory/item-use owner,
select the lighter's **Activate** action, then at **Light where?** choose the brazier with the
advertised direction target. Verify `f_brazier` and native `fd_fire` on that tile, and the matching
ignition result; `fd_hot_air1` alone is insufficient. Retreat about two tiles without stepping
on fire. If it is already burning, observe instead of igniting again. Investigate a concrete
failure before changing supplies; do not substitute a smokebomb. Row 03 of
`build_logs/harness-fixes-matrix20/INDEX.md` is the proven action sequence, while the
[item-activation reference](references/setup-interaction.md) explains current controls.

**Closed-window ground floor is smoke-only for outside discovery.** In every scenario copied
from the normalized closed-window save, a fire inside the house at z=0 does not expose light
outside. Do not call its `fd_fire` a valid exterior light packet or switch to a night-light
conclusion merely because it burns longer. Keep the smoke source burning and advance native
time toward the next useful observation. For a smoke-onset/range probe, inspect short intervals
while the signal question is unresolved. For a known working end-to-end recipe, use longer
interruptible waits and retrieve signal/admission/member evidence together at the useful endpoint;
do not make the probe's per-wait checks mandatory for every journey. Stop the narrow probe at
roster initiation or a specific observed **smoke** eligibility failure; other arms continue to their outcome. A fire-only `no_in_range_production_signal`
does not settle the smoke test: the source adapter adds `fd_smoke` to strength and gives a
persistence bonus, which can increase the projected range after smoke appears. In R013 a
fire-only cap of 6 OMT was one tile short of the target; continue or resume from the saved
burning-fire turn until actual smoke is sampled before judging that range gate. An open-window
or rooftop light test needs its own verified geometry and separate disposable run.

In the R013 downstairs example, the player stood on `[3158,3449,0]`, directly west of the
brazier `[3159,3449,0]`. Open Inventory, select the **charged lighter**, then choose the
currently advertised **activate** action in its item menu. The live menu advertised
`inventory-item-action:21`; an attempted `inventory-item-action:1` was rejected as
`stable_id_not_advertised` and did not use the lighter. At **Light where?**, choose **East**
from that player tile. The next prompt asks whether to burn the firewood source; read its
current choices and choose **YES** for this prepared fuel. Confirm `fd_fire` on the brazier
after the action, rather than treating the accepted item/menu input as ignition proof.
These historical action IDs are an example of why the worker must copy the current menu target;
the direction depends on the current player and brazier tiles.

```text
play act world.inventory
play act inventory.select --target CURRENT_LIGHTER_ID
play act inventory.item_menu.choose --target CURRENT_ACTIVATE_ID
play act direction.choose --target east
play act prompt.choose --target CURRENT_YES_ID
```

Stop after each line to read the new native surface. `east` applies only when the brazier is
one tile east; select the current direction otherwise.

When the test asks whether a brief fire admits a cannibal roster, advance at least one real
in-game minute after ignition before judging it. At the new native turn, check the exact
signal source, weather and projected reach, then inspect camp `living_total` separately from
concrete member IDs and any materialization or knowledge event. Immediate `fd_fire` only
proves ignition. When disk evidence is needed, follow [Save a checkpoint](references/live-operation.md#save-a-checkpoint); a skipped write does not justify changing the scene to force a save.
For a light-then-off test, record actual light reach and any admission at the one-minute check;
do not infer admission from the flame. After extinguishing, verify fire and residual smoke have
both ended before following the same actors.
For a closed-window smoke roster probe, check the camp first, light the ordinary fire, and
inspect after one real native minute. Continue with short waits, checking smoke packet,
admission and concrete member IDs after each. Five minutes is a checkpoint, not a stop time:
keep it lit until initiation or a specific source/gate failure is established. Extinguish only
after the requested smoke observation is complete, then check residual fields. Keep each turn,
actual signal field and concrete member ID distinct; delayed materialization is a result to
measure, not assume.

### Extinguish through Examine

For the downstairs brazier in the prepared scene, verify the player's tile is adjacent to the
actual brazier tile before opening its native Use/Examine menu with `e`. Read the menu that
appears, then select only its currently enabled letter. The native fireplace menu can offer
**Start a fire** or **Extinguish fire** according to the current `fd_fire` state and available
items; `src/iexamine.cpp::fireplace` is the source. Extinguishing needs a carried or nearby item
with bash damage. Verify `fd_fire` is absent after the selection; then follow residual smoke
and heat until the intended signal is off. An inventory search for “fire” does not open this
furniture menu. If the menu is absent, first
check adjacency, then current fire and useful items. When the current World lists
`world.examine`, `play act world.examine` opens the same native adjacent-tile chooser. Read
its returned menu and copy the current `menu.choose` target; never reuse a prior target ID.

In the current bound Mac session, Luna opened Examine with an exact-PID native `e` press through
the authorized Peekaboo bridge after confirming the adjacent tile. The next semantic menu
already supported choosing **Extinguish fire**. Read the current menu and its enabled target;
the historical `uilist-entry:66` ID below is evidence, not a reusable target. The shared
`world.examine` semantic opener has focused source tests; a separate source-bound
native CLI pilot remains open. The exact-PID `e` result is historical proof of the native menu,
not proof of that CLI pilot.

```text
/opt/homebrew/bin/peekaboo type "e" --pid PID --bridge-socket "/Users/josefhorvath/Library/Application Support/Peekaboo/bridge.sock" --json
play look
play act menu.choose --target CURRENT_ENABLED_EXTINGUISH_ENTRY
```

Replace `PID` with the session's verified game PID and the target with the currently advertised
enabled entry. In R013 this opened the brazier action menu directly, with no direction prompt.
Only Luna uses this live fallback. The normal CLI action should replace the
Peekaboo press once source-bound native verification proves it.

In the R013 disposable scene the downstairs brazier is `[3159,3449,0]`. Luna first tried from
`[3156,3449,0]`, three tiles away, and an inventory filter returned no matches. A bounded
reposition to `[3158,3449,0]` put the player beside it. The exact-PID native `e` opened
**Select an action**;
**Extinguish fire** was enabled and the existing semantic menu accepted its current
`uilist-entry:66` choice. At turn 5,216,493, the exact brazier tile no longer had `fd_fire`,
and a real quicksave confirmed that turn. Residual `fd_smoke` and `fd_hot_air1` remained;
track their decay before calling the signal off. These coordinates describe that saved scene,
not a general path for every profile.

## Readable play output

Ordinary `play` output should show the current game surface, relevant actors and hazards, and
the few choices available now. A menu-opening action should return that menu with its letter
choices in plain text, without a second command or raw transport JSON. Keep run IDs, hashes,
PIDs and broker internals behind diagnostics, journal or exact evidence retrieval unless one
of them is needed to resolve a current binding failure. When a worker repeats movement or opens
an unrelated menu without changing the intended state, stop and inspect the recent action
sequence and current game result before adding a macro or more output fields.

## Current reusable findings

| Finding | Scope and evidence | Use in later tests |
| --- | --- | --- |
| An already travelling reserved attacker can retain an obsolete rally guard that blocks loaded approach. | Failing-before and passing production path tests plus Mac build in `build_logs/first-smoke-009/worker-receipt.json`; repair in `src/do_turn.cpp`. Native assault remains a separate proof. | Check same-ID goal/path and both persistent/cached guard after reload; require physical OMT arrival before contact. |
| Debug clairvoyance has two mutations: `DEBUG_CLAIRVOYANCE_PLUS` and `DEBUG_CLAIRVOYANCE`. | `data/json/mutations/debug.json`; verified viewing setup above. | In a disposable viewing run, record both mutations, pass one real turn and verify the view. Do not credit either as a signal cause. |
| Local hostile posture reads the player's view of reserved members. Clairvoyance may change exposure and `hold_off` decisions. | `src/do_turn.cpp::live_bandit_make_gate_input` and `live_bandit_try_sight_avoid_reposition`; run `06099f7263e37a362941a1fc57bc56d550ad77539e1b8aed76d06326515a0106` is a limited viewer example. | Preserve a separate no-clairvoyance copy for concealed-player assault proof; inspect local-gate exposure logs before interpreting a visible standoff. |
| Cannibal routine members remain abstract until a valid signal admits their response. Terrain alone can create suspected leads without a scout. | `src/do_turn.cpp::live_bandit_materialize_abstract_members_for_routine`; the [R012 no-fire result](../../../build_logs/first-smoke-012/no-fire-control-result.md) ran 1,441 native minutes with 12 suspected leads, no ready observer and no outing or raid. | Treat an unlit default camp with no scouts as the careful-player baseline. For post-signal routine behavior, admit a short real signal, record any knowledge it creates, extinguish the source, then follow the same NPC IDs through visits and returns. Do not call that a no-signal discovery test. |
| Debug life support and invisibility are separate traits. | `data/json/mutations/debug.json`: `DEBUG_LS` stabilizes hunger, thirst and sleepiness; `DEBUG_CLOAK` grants invisibility. | Record each trait and one real turn after activation. A cloaked travel run changes hostile sight and cannot substitute for an ordinary visibility claim. |
| A stopped wait does not establish the intended interval. | Native wait receipts and the wait method above. | Record the interruption and elapsed turns, then usually resume the remaining wait after an ordinary distraction. Stop or adapt for a consequential threat. |

## Add, prune and correct notes

At a meaningful game transition, write a short local note with: question, run/source identity,
turn range, actor/operation IDs, exact evidence handles, observed fact, alternative explanation,
claim limit and next decision. Keep current owner instructions in the WEC/assignment, not duplicated in this manual. Promote only
reusable, verified knowledge into the table above.
When a new result corrects a note, update that row and retain the old artifact link; do not keep
two conflicting tips. Merge repeated procedure into the relevant reference page and keep this
index short. Retire source-specific warnings once a regression and native proof absorb them.
Prune stale setup advice and redundant wording during a natural handoff or closeout, while
preserving accepted evidence and the current owner contract. Never prune raw logs or save data
merely to make a claim appear clean; artifact retention follows the task's lifecycle and owner
save rules.

## Logging gate audit pending

`OPENCLAW_HARNESS_UI_TRACE` gates the detailed owner trace in `src/do_turn.cpp` and is set by
`tools/openclaw_harness/startup_harness.py` for harness children. At source build
`b76d1f3d63401f693df6b905743c37457de6c79a50d51cff2b468dc7a2e53b45`, the
`render_local_gate_report` `DebugLog(D_INFO)` calls in `src/do_turn.cpp` are unconditional.
These should be reviewed after the active native run for harness-only diagnostic gating;
do not infer that all game debug output is already isolated. Any change needs a targeted
ordinary-launch and harness-launch check, with no suppression of errors or claim evidence.
At run closeout and natural handoffs, inventory task-owned temporary traces and duplicate
exports. Retain evidence-bearing raw logs, source handles, hashes, saves and receipts for open
claims; prune redundant temporary copies only after their durable references are verified and
no worker or retained session needs them. Avoid deleting live run logs or owner saves.
