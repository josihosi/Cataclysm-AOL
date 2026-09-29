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

A new action batch is a harness change until demonstrated on a disposable start. Sol tests its
command ordering, pending/collect handling, changed input owner and safe-wait interruption;
Luna then uses the exact worker-facing recipe and checks native receipts and resulting state.
Compare with the same actions issued individually at a compatible start: action order, elapsed
turns, position, interruption boundary and evidence coverage must agree at their intended scope;
random game outcomes need not be identical. Check harmless chatter continues, near danger or
actual harm returns control, and no later action runs after rejection or an unexpected menu.
A timed-out response is collected by its request ID, never blindly retried. Test recovery from
that boundary without duplicate input or skipped actions. Exercise failure cases in automated
harness tests rather than manufacturing hazards in the product proof run.

Keep batch validation and gameplay verdicts separate in the existing task result. If delivery,
ordering or capture is uncertain, preserve the first mismatch and continue the game investigation
with verified individual controls from the last known state (or a retained copy when needed).
Do not diagnose NPC behavior from an uncertain batch, or change AI while repairing that uncertainty.
If Luna misuses a batch, inspect the actual command/output and fix the ambiguous interface or
recipe; prefer one tested existing macro over a longer model-authored shell sequence. Replay the
failed interaction before promoting the batch. Unverified batching remains optional and cannot
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

Reuse the same fire/wait/actor-proof sequence, adding the ordinary ladder route to the roof
brazier. In the normalized R040 map, the roof ladder is `[3149,3454,1]`; the roof watch tile is
`[3155,3448,1]`. Verify those tiles against the current native map. Native `>` at the watch tile
was accepted as input but did not descend; the same action on the ladder changed z=1 to z=0 at
turn 5,295,928. Walk to the ladder before changing floors, and return upstairs to the roof watch
tile after any necessary ground-floor action. Read [visibility over time](smoke-light-visibility.md)
and verify the actual roof tile, then ignite only if needed. Observe developing fire/plume, weather
and each channel's admission
separately. The controls remain reusable when a product bug prevents the intended response;
mark the unproved gameplay stage rather than discarding the procedure.

Use [Save a checkpoint](live-operation.md#save-a-checkpoint) for native saving and skipped writes;
no movement or other setup action is part of that segment.

For the owner-requested no-southern-city setup and optional bandit camp, use the [rural roof variant](rural-roof-replay.md). It reuses these controls; its edited fixture and outcomes remain separate.

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
