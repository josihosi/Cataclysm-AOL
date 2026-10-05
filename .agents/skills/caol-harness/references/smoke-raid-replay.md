# Smoke → cannibal raid

**Outcome:** real smoke causes scouting, return/report and a raid that physically attacks.
A raid killing the player is success for this arm; passive pacing is not.
Expect the lit fire to develop smoke, an eligible camp to notice it, and scouts to investigate
before a later raid. One weak sample or an observation before the next camp scan is not failure;
continue while the situation develops, checking changes rather than assuming guaranteed growth.

**Evidence boundary:** the [suite coverage table](playtest-suite.md#current-coverage-and-next-complete-pass)
links retained results and current gaps. Ordinary-view attacks and attributed kills are proved at
their bound run/build scope; a saved-contact continuation does not prove fresh smoke admission.
Reuse the controls below with the selected build, not historical timing or actor IDs.

## Start

Coordinator supplies the selected scenario/session, start-save identity, current completed
setup and remaining outcome. Include `RUN` (native run directory), `PROFILE` and `RUN_ID`
from that binding for the queries below. Registry selection reuses its profile; it does not
create a disposable copy. Use a preserved-copy manifest for independent starts.

For the normalized closed-window setup: indoor brazier `[3159,3449,0]`, camp OMT
`[131,143,0]`. Closed windows isolate smoke from exterior light. Verify current state once;
a continuation may already have fire, scouts or raid knowledge. Keep ordinary vision for this
arm. Use the current observation/menu; collect any pending request before issuing another.

Before long waits, apply the shared [mutation setup](setup-and-interactions.md#mutation-setup-for-suite-runs): verify avatar `DEBUG_LS`, add only if missing, retain ordinary vision. Exact saved-regression starts retain their declared setup.

## Play

1. **Make smoke if needed.** Walk beside the brazier, select the charged lighter in inventory,
   Activate, choose the brazier direction, confirm fuel if prompted. Resolve IDs from each
   returned menu. Check native ignition/fire, then smoke and camp admission. Already burning:
   skip ignition. [Exact fire controls](../GAME-MANUAL.md#fire-in-a-prepared-brazier).
   If the first sample is rejected, use [visibility over time](smoke-light-visibility.md) to distinguish
   evolving fire/smoke strength from a fixed range, terrain or cross-z boundary.
2. **Advance through the current stage.** Use an explicit `safe` wait sized to the next known
   gate or observation question. Thirty minutes is a starting example, not a required cadence;
   a proved quiet interval can be longer. Inspect the returned result before deciding the next
   batch. A direct `act wait.5m` chooses the native duration and does not run the safe-wait
   distraction policy; the R040 thirst prompt followed that direct action. Player-safe waiting
   does not promise to stop for a distant NPC's combat or death.
3. **Inspect meaningful changes.** Use the decision points below. Read the returned frame and
   retrieve only missing operation/actor evidence, preferably in one related query batch.
   Preserve source handles and performance alarms; investigate actual turns over100ms through
   the existing performance task. Avoid an automatic look/save/full-roster sequence.
4. **Let the assault happen.** Inspect first harm and its source. For deliberate combat-through-
   outcome, use `play wait 1m ignore` or the currently advertised native duration. Ignore permits
   damage/death. If already inside a wait submenu, use its advertised action; do not submit a
   World macro against it. A checkpoint or first harm does not finish the combat arm. If a raid
   is rallying, check actual member tiles at rally and night eligibility; continue when a future
   gate is expected. Finish with physical attack and the requested outcome, or a diagnosed gate
   contradiction that prevents progress.

Record/Ignore recoverable debug dialogs and continue. If actors stop progressing, inspect their
physical saved positions and travel orders before more waiting. **Abstract route/member OMTs
can be projections, not NPC locations.** Preserve the first failing checkpoint for a same-setup
repair. Native death is evidence even if the last player save predates it.

## Decision points for efficient repeats

These are questions to answer when the stage changes, not mandatory stops or separate tool calls.
A single returned result may answer several. Skip already-proved setup on a scoped continuation;
a fresh end-to-end replay still needs its own evidence.

| When the worker should inspect | Read together; decide the next batch |
| --- | --- |
| First source admission / scout dispatch | Actual channel/source and recipient site, operation and fresh scout IDs. Continue toward the expected watch/report gate; one weak initial puff alone is not failure. |
| Report / response decision | Report senses and freshness, capability decision or exact rejection, next eligible time. Distinguish routine-outing cooldown from hostile-response gate. Do not repeat ignition to fix a downstream rejection. |
| Response party reserved / departure | Resolve actual raider IDs, capture coverage, physical positions and route/owner. Preserve a reusable pre-encounter checkpoint when the pending question concerns combat, sleep or entry. The earlier scouts need not be the raiders. |
| First encounter / floor transition / sleep / harm / casualty | For affected actors: physical position/z, selected action/target, movement, HP and source/reason. Choose continuation or diagnose contradiction. Shorten observation spacing while the causal question is open; keep recording between inspections. |
| Terminal outcome or concrete stall | Explain every required actor's outcome and evidence gaps. Stop unchanged waiting only when the gate contradicts expected progress; a future night gate, checkpoint or first hit is not completion. |

Batch settled native controls only while their preconditions and input owner remain valid. Do not
queue a whole journey past unresolved menus or faction decisions. Batch independent read-only
queries; return a compact change summary with exact raw handles, not repeated full snapshots.
Collect one pending request with the existing bounded wait option; do not resubmit the action or
poll a completed result hoping for new game state. Use `look` only when a fresh observation is needed.

Before a longer encounter batch, ensure recording covers the actual reserved actors and relevant
turns, including damage/death and sleep edges when those are the question. If capture is missing,
repair/select it before advancing rather than reconstructing lost evidence afterward. Event-triggered
macro return is an improvement target, **not a currently promised CLI capability**: until supported,
use bounded waits and inspect the retained changes at each return.

### Counted actions after the R043 pilot

The [R043 control result](../../../../build_logs/first-smoke-043/result.md) validated
`play repeat world.autoattack --count 10` in a deliberately quiet, no-hostile disposable scene.
It completed ten accepted actions, including collection of one pending exact request; a matched
run with ten individual Tabs reached the same turn. `play repeat --resume` continues only that
recorded request after a pending result. Check requested/completed/unused counts, stop reason and
raw receipt handles before another action. A separate single Tab attempted a strike on a nearby
zombie, spent moves and stamina, and missed without advancing the displayed turn. Native
`world.pause` passed one turn; a ten-pause native batch has not been run. The accepted native
control receipts remain distinct from the packaged report: R043 currently has a false-red clean-exit
classification under investigation; do not call that report fully clean until corrected.

Use a counted repeat only when those discrete actions are the intended behavior and the World
owner, safe mode, nearby creatures and interruption policy have been checked. Tab can attack any
hostile in native reach, so it is not a passive raid wait or a substitute for `safe`/`ignore`
duration waits. Keep changing menus, faction choices, travel, first harm and combat under the
appropriate individual controls. This pilot does not validate a multi-action raid script.

### Validate a batch before relying on it

Using an existing supported wait/move/trade batch, increasing its duration within advertised
controls, or merging adjacent same-policy waits is ordinary play, not a new harness implementation
or a reason for a separate validation run. Use the [shared wait batching rule](../GAME-MANUAL.md#wait-and-observe).
Inspect its returned progress and interruptions on the useful run. Keep detailed recipes and
current native targets; a shell chain that ignores changed owners is not supported batching.

When the execution mechanism itself changes, Sol tests the affected command ordering,
pending/collect handling, changed input owner and interruption behavior. Luna then adopts it on
the next useful arm. Compare against retained individual-action evidence or automated controls
where sufficient; another matched native replay is needed only for a material remaining behavior
gap. Expected action order, elapsed turns, position and interruption boundaries must hold; random
game outcomes need not be identical. Check harmless chatter continues, near danger or
actual harm returns control, and no later action runs after rejection or an unexpected menu.
A timed-out response is collected by its request ID, never blindly retried. Test recovery from
that boundary without duplicate input or skipped actions. Exercise failure cases in automated
harness tests rather than manufacturing hazards in the product proof run.

Keep batch validation and gameplay verdicts separate in the existing task result. If delivery,
ordering or capture is uncertain, preserve the first mismatch and continue the game investigation
with verified individual controls from the last known state (or a retained copy when needed).
Do not diagnose NPC behavior from an uncertain batch, or change AI while repairing that uncertainty.
If Luna misuses a batch, inspect the actual command/output and fix the ambiguous interface or
recipe; prefer one tested existing macro over a longer model-authored shell sequence. Repair
query/schema/checker mistakes against retained artifacts; replay the affected interaction only
when its actual behavior remains uncertain or changed execution requires native proof. Unverified batching remains optional and cannot
hold up reliable individual play. No new generic batching framework is required.

A reusable checkpoint means confirmed native save **plus retained complete save bytes and manifest**
through the existing snapshot procedure; a quicksave receipt alone is overwritten by the next save.
Preserve snapshots at useful branch/regression boundaries, not every unchanged wait. Save without
movement. Keep independent pre-signal baselines intact; do not claim a continuation proves discovery.

## Clairvoyance raid replay

For the suite's viewer variant, use a **fresh separate closed-window viewer arm**.
An exact saved-casualty regression has its own narrower scope. Reuse Start/Play above; do not repeat that setup in a second
script. Record both `DEBUG_CLAIRVOYANCE` and `DEBUG_CLAIRVOYANCE_PLUS`, apply only missing
ones through live controls, then `world.pause` once and verify both active. The prior viewer
[run](../../../../build_logs/first-smoke-021/r021-clean-closed-dual-clair-viewer-final-witness.json)
verified this without movement. Walk beside the unlit indoor brazier and use ordinary lighter
ignition. Do not move before saving or relight an already burning fire.

Use the shared decision points and batching rule above for this viewer arm. Clairvoyance does not
replace actor-bound proof or justify extra inspection after every unchanged interval.

Check each raider and both defenders before the first casualty as well as afterward: target,
visibility, orders/duty, selected action and actual movement/HP changes. Casey fought in the prior
run; Tilda and the two other raiders need individual explanation, not an assumption that leader
loss caused all passivity. Continue through fighting. Inspect first harm, then use the combat wait
above to reach the requested outcome. Every capable surviving attacker must participate; track
leader loss without requiring the fresh run to reproduce a scripted casualty. A defender kill is
valid combat, not failure, but does not by itself prove remaining raiders function. If a survivor
stalls, compare physical records and eligibility before another unchanged wait. The saved R021
leader-death continuation separately tests that exact regression. Keep viewer proof distinct from
ordinary-view exposure/combat proof; retain only observed actions and attributed outcomes.

## Roof variant

For disposable roof runs, use [Get and verify Debug Life Support](setup-and-interactions.md#get-and-verify-debug-life-support) before long waits. Do not toggle an active trait off or add the debug mutation bundle. This is survival setup, not signal proof; keep ordinary vision unless running the named viewer variant.

Reuse the same fire/wait/actor-proof sequence. For the normalized house, use
[Josef's base-save route and actions](../GAME-MANUAL.md#josefs-two-brazier-base-save): the real
ladder ascent, roof fire approach, current lighter controls and maintenance checks live there.
Check actual geometry, fuel, weather and each channel's admission separately. A product bug
can leave a gameplay stage unproved without invalidating the successful access/ignition controls.

The ordinary R051 roof run on 2026-10-04 used this checked route from its observed start
`[3156,3449,0]`: southwest, south three times, southwest, west five times to the real ladder
`[3149,3454,0]`; `world.level_up` to `[3149,3454,1]`; east, northeast, north twice, northeast
four times to `[3155,3447,1]`; south once to the working tile `[3155,3448,1]`. Confirm the live
position after each segment and use the actual ladder. From the working tile the unlit brazier
`[3156,3447,1]` is northeast. The run used the current lighter Activate action, answered the live
“Light where?” prompt with northeast, and then observed `f_brazier`, `fd_fire`, and hot-air fields.
Some local snapshots lacked `fd_smoke` even though the bound overmap sample recorded actual smoke
admission; check both the local fire and the native signal packet. The full stage sequence and
raw event handles are in the [R051 roof result](../../../../build_logs/first-smoke-068/r051-roof-fire-result.md).

### Exact command batches from the retained roof route

The successful run's `semantic.requests.jsonl` records the ordered native action IDs, not a shell
transcript. These are the exact action arguments in that record, using the current `play_cli.py`
spelling. Supply `SESSION` from the fresh selected launch. Run one action at a time and confirm the
listed endpoint; collect an outstanding result and re-observe before continuing after an
interruption or prompt.

| Batch | Entry state | `play_cli.py` action arguments, in order | Endpoint |
| --- | --- | --- | --- |
| `ground_to_roof_ladder` | `[3156,3449,0]` | `act world.move.southwest`; `act world.move.south` ×3; `act world.move.southwest`; `act world.move.west` ×5 | Ground ladder `[3149,3454,0]` |
| `climb_real_ladder` | `[3149,3454,0]` on the ladder | `act world.level_up` | `[3149,3454,1]` |
| `roof_ladder_to_working_tile` | `[3149,3454,1]` | `act world.move.east`; `act world.move.northeast`; `act world.move.north` ×2; `act world.move.northeast` ×4; `act world.move.south` | `[3155,3448,1]`; brazier `[3156,3447,1]` northeast |

The command prefix and one exact invocation are:

```sh
python3 tools/openclaw_harness/play_cli.py --session "$SESSION" act world.move.southwest
```

Replace only the final action with the next listed action argument. Do not reuse the historical
lighter UID or menu IDs: select the currently observed lighter and live Activate action, choose the
current direction target `northeast`, and answer `yes` only if the current fuel prompt offers it.
The distinct downstairs path starts at `[3156,3449,0]`, uses `act world.move.east` twice to
`[3158,3449,0]`, then targets brazier `[3159,3449,0]` east; it is not a stair route.

#### R050 exposed-roof fuel and light sample

This actual R050 run used the already-burning exposed roof brazier `[3156,3447,1]`. The adjacent
physical firewood-source tile `[3156,3448,1]` held 242 planks in two stacks. The player picked one
plank and dropped it back on that source tile; native maintenance later moved splintered wood onto
the brazier. This is an ordinary local-fuel control, not an ignition or debug-fuel procedure.

| Batch | Entry state | Recorded action arguments | Endpoint |
| --- | --- | --- | --- |
| `roof_source_pick_one_plank` | Player `[3155,3448,1]`; source tile east `[3156,3448,1]` | `act world.move.east`; `act world.pickup`; `act direction.choose --target pause` (Here/current tile); `act inventory.increase_quantity --target CURRENT_PLANK_UID`; `act inventory.commit` | One plank selected from the physical local stack; turn 5216321→5216323 |
| `roof_source_drop_plank` | Player `[3156,3448,1]` with the plank | `act world.drop`; `act inventory.toggle --target CURRENT_PLANK_UID`; `act inventory.commit`; `act world.move.west` | Plank dropped on the source tile beside the brazier; player returned to `[3155,3448,1]` at turn 5216328 |

The two recorded selector UIDs were `2193089` for pickup and `4262880` for drop. Rebind both to the
current plank rows; do not reuse those historical UIDs. The [pickup requests](../../../../.userdata/first-smoke-050-rural-bandit-only-20260930/harness_runs/20261005_030511_62c60c818e6340e2acbe0113e87ce5c2/semantic.requests.jsonl)
and [drop requests](../../../../.userdata/first-smoke-050-rural-bandit-only-20260930/harness_runs/20261005_030938_6a718470eb0c4ac9ad636260603dd79a/semantic.requests.jsonl)
bind the inputs; the corresponding [pickup steps](../../../../.userdata/first-smoke-050-rural-bandit-only-20260930/harness_runs/20261005_030511_62c60c818e6340e2acbe0113e87ce5c2/semantic.steps.jsonl)
and [drop steps](../../../../.userdata/first-smoke-050-rural-bandit-only-20260930/harness_runs/20261005_030938_6a718470eb0c4ac9ad636260603dd79a/semantic.steps.jsonl)
show acceptance. The native refill at turn 5216418 is in the [first continuation steps](../../../../.userdata/first-smoke-050-rural-bandit-only-20260930/harness_runs/20261005_032618_3060089e77714996b514e3b9cf4532f1/semantic.steps.jsonl).

For the observed 900-turn sample, the save at 5216328 began with `fd_fire` but no smoke. The logged
start action was `act world.wait`; the current “Wait a while” menu entry was chosen before
`wait.5m`. That wait was interrupted after 90 turns by the native stop-waiting prompt; the current prompt's
YES stopped that wait and the state was saved at 5216418. After reload, each selected wait began
with `act world.wait`; the current “Wait a while” menu entry was chosen before `wait.5m` ×2,
`wait.1m` ×3, and `wait.20s`. Then `act world.pause` ×10 advanced the remaining 10 turns. The
interval ended at 5217228. Do not treat the menu's chosen duration as elapsed time; use the
returned turn and re-observe any prompt.

The production-channel query is the exact `scan_id` `4a87fd6267e05739793dfd15dd0b9b049f58a7d146133dd7867b7341995740cf:2`
in [R008's channel records](../../../../.userdata/first-smoke-050-rural-bandit-only-20260930/harness_runs/20261005_033322_b955f5e076ef4013972fc7b7147dbab8/r008.production.channels.jsonl).
At minute 7750, `live_smoke@3156,3447,1` and `live_light@3156,3447,1` both had
`observed=true`, `isolated=true`, `signal_origin=local_field`, and consumer
`bandit_live_world.signal_scan`. This is a positive light-and-smoke observation during the interval,
not proof of continuous emission. The endpoint save at 5217228 still had intensity-1 fire but no
`fd_smoke`. A search of `debug.final.log` alone missed the structured light row; use the bound
production-channel record for this claim. The exact action and final save records are in the [900-
turn steps](../../../../.userdata/first-smoke-050-rural-bandit-only-20260930/harness_runs/20261005_033322_b955f5e076ef4013972fc7b7147dbab8/semantic.steps.jsonl)
and [saved World](../../../../.userdata/first-smoke-050-rural-bandit-only-20260930/harness_runs/20261005_033322_b955f5e076ef4013972fc7b7147dbab8/lastworld.after.json).

The retained run's full `wait.2h` intervals began at minutes 7738, 7858, 7978, 8098, 8278, and
8398; `wait.1h` began at 8218. The final two-hour choice was interrupted at minute 8424 by the
dangerously-close prompt. For a safe, bounded two-hour batch, the current CLI spelling is:

```sh
python3 tools/openclaw_harness/play_cli.py --session "$SESSION" wait 2h safe
```

This is a maximum useful interval, not proof that it fully elapsed or that a distant scout event
interrupted it. Read returned elapsed game time and query the current native site/actor state at the
chosen endpoint. After first attributed combat harm, use `wait 1m ignore` only when the intent is to
let that combat continue. The original action and timing records are the bound roof run's
[semantic requests](../../../../.userdata/r051-r067-roof-fire-comparison-20261004/harness_runs/20261004_230703_95d5cf2e0afc4867893479a76595852a/semantic.requests.jsonl)
and [semantic steps](../../../../.userdata/r051-r067-roof-fire-comparison-20261004/harness_runs/20261004_230703_95d5cf2e0afc4867893479a76595852a/semantic.steps.jsonl); the interpreted route and endpoints are in the
[R051 roof result](../../../../build_logs/first-smoke-068/r051-roof-fire-result.md).

For the cadence used there, choose the supported `Wait a while` action with a useful long interval
and recheck the saved/current signal, same-ID scouts, and current activity when that interval
returns or an event interrupts it. The actual run used two-hour wait choices; native progress
included minute-7757 smoke admission, minute-7800 scout dispatch, minute-7860 watch arrival,
minute-7995 physical return, minute-8040 report delivery, minute-8100 follow-on dispatch, and
minute-8420 physical contact. An interval choice is not proof that the whole interval elapsed.
At minute 8424 the first “dangerously close” prompt was answered YES to stop and inspect Marion.
After the native attacked prompt and attributed first hit, IGNORE was deliberately chosen so this
lethal-combat arm could continue; the same actor later delivered the attributed fatal hit. Use
IGNORE only when intentionally allowing this combat outcome to unfold. The run briefly selected
the wrong `Light Step` mutation and removed it before selecting exact Debug Life Support. In this
run the correct trait was activated at minute 7978, after the first two two-hour wait choices, so
this is not evidence of pre-wait Life Support setup. Life Support did not clear the recorded “Very
thirsty” / “Dehydrated” labels. Follow the setup/get control above and confirm the exact trait
before long waits instead of repeating that menu mistake.

This evidence is a cannibal smoke-found assault and death, not a bandit or indoor/closed-room
result; the scouts' selected target in this run was not an exact-three-OMT watch proof. Keep those
claims on their own evidence arms.

Use [Save a checkpoint](live-operation.md#save-a-checkpoint) for native saving and skipped writes;
no movement or other setup action is part of that segment.

For the owner-requested no-southern-city setup and optional bandit camp, use the [rural roof variant](rural-roof-replay.md). It reuses these controls; its edited fixture and outcomes remain separate.

## Open-window comparison controls

These are two different saved geometries. The owner-prepared open-reference profile has 55
identity-checked files but no hostile camp; its window route is signal-only. The R050 real-camp
profile has the added bandit site, but its `[3163,3450,0]` reference coordinate is floor and nearby
window faces are blocked. Opening its actual wood door does not make it an open-window test.
Keep the bound starts separate. The task-local byte/terrain comparison and complete action-source
handles are in [`R051 open-start notes`](../../../../.de67/task-logs/r051-next-open-start.md).

The exact `play_cli.py` prefix used for these retained native receipts is:

```sh
python3 tools/openclaw_harness/play_cli.py --session "$SESSION" act ACTION_ID
```

| Batch | Entry | Action IDs in order | Endpoint / result |
| --- | --- | --- | --- |
| R050 real-camp working tile | `[3156,3449,0]` | `world.move.east` ×2 | `[3158,3449,0]`, west of brazier `[3159,3449,0]` |
| R050 ordinary ignition | `[3158,3449,0]` | `world.inventory`; `inventory.select` current lighter; `inventory.item_menu.choose` current Activate; `direction.choose` east; `prompt.choose` yes only for the live wood prompt | Real brazier `[3159,3449,0]`; inspect `fd_fire` and source-bound channel separately |
| R050 real-camp door | `[3156,3449,0]` | `world.move.east` ×2; `world.move.south` ×5; `world.move.west`; `world.move.southwest` | The southwest action opens the closed wood door at `[3156,3455,0]` but returns `no_progress` at `[3157,3454,0]`; confirm the current map/message before continuing |
| R050 return to source | `[3157,3454,0]` after the door action | `world.move.east` ×2; `world.move.north` ×5; `world.move.west` | `[3158,3449,0]` |
| Open-reference window | `[3158,3451,0]` in its saved geometry continuation | `world.move.east` ×4; `world.move.northeast` | `[3162,3451,0]`; the northeast action opens window `[3163,3450,0]` and leaves the avatar in place |
| Open-reference return | `[3162,3451,0]` after opening | `world.move.west` ×4; `world.move.north` | `[3158,3450,0]`; the earlier one-minute sample was too short and had no positive candidate |
| Open-reference full signal interval | Window `[3163,3450,0]` verified open; confirm current `fd_fire` and available real wood first | `world.wait`; choose current `Wait a while`; `wait.30m` | On the bound signal-only retry this advanced 1,800 native turns; smoke became positive at minute 8485 and stayed positive through 8505, while light stayed absent. Eight native `You drop your splintered wood on the brazier` messages corresponded with the prepared pile decreasing 856→848. This is this save's ordinary refuel evidence, not a promise for other saves. Retain the start/end turns and inspect interruptions; no 1-minute polling |
| R050 natural observation interval | Current World; signal already verified | `world.wait`; choose the current `Wait a while` menu option; `wait.2h safe` | In the bound run the interval was interruptible; collect/reobserve prompts. Smoke was first observed at 7757, physical scout return at 8060, and report delivery at 8100; those times are evidence, not reusable gates |

Use only the row matching the exact loaded start and current observed terrain. The R050 player’s
first south input opened a Tilda interaction menu; cancel that current menu and reobserve before
choosing a route. Do not transfer the reference-window command to R050 where the target is floor.
Opening a real window, observing `fd_fire`, or seeing smoke alone does not establish an accepted
light channel. The signal-only interval was run from a survivor-camp reference without a hostile
site; it proves its observed smoke source packet, not camp discovery or a journey.

## Retrieve only the proof needed

Use the bound run's logs. Add the relevant operation/actor/time filter when more than one party
matches; retain returned byte/hash handles. These query shapes were tested against the saved runs.

```sh
# Same-ID physical return
python3 tools/openclaw_harness/cockpit_file_bridge.py log-query \
  --path "$RUN/transition.events.jsonl" \
  --where 'transition="structural_member_physical_return"' \
  --select game_minutes --select actor_ids --select outcome --limit 5

# Raid party and the lead it responds to
python3 tools/openclaw_harness/cockpit_file_bridge.py log-query \
  --path "$RUN/transition.events.jsonl" --where 'transition="follow_on_dispatch"' \
  --select game_minutes --select operation_id --select actor_ids --select target_lead_id --limit 5

# Actual damage source; correlate numeric source ID with that raid party
rg -n -F 'component=avatar_damage_source' "$PROFILE/config/debug.log" | rg -F "run_id=$RUN_ID "

# Actual terminal outcome; correlate turn and avatar with damage
python3 tools/openclaw_harness/cockpit_file_bridge.py log-query \
  --path "$RUN/semantic.native.events.jsonl" --where 'kind="terminal"' \
  --select game_turn --select payload.avatar_id \
  --select payload.actual_death --select payload.suicide --limit 5
```

For scout/report transitions and signal channels, use the bound transition/channel records
and selected fields; [tested historical selectors](../../../../build_logs/first-smoke-011/r011-r001-filled-smoke-raid-protocol-candidate.md#observed-action-and-proof-segments)
show the actual schema. Historical sequence numbers are not reusable filters.
The damage trace requires a build with the harness-only R017 hook. Missing source attribution
is an evidence gap, not proof that no attack occurred. Names, gunshots and shot counters alone
do not establish the killer. Do not infer gore without an observed native record.

## Return and maintain

Report outcome/first divergence, working action/query corrections, evidence handles and current
save/session ownership. Close through the existing native lifecycle when finished.
Update this recipe only where actual use changed it; keep detailed chronology in the run report.
