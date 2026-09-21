# Waiting interface: owner-reviewed contract and coverage

The owner selected dangerous interruption skipping as the default on 2026-09-21.
This replaces the earlier proposal to default to stopping on interruption.

Commands to implement:

```
play wait 5m         # continue through interruptions, including danger
play wait 5m safe    # continue only through known harmless interruptions
play wait 5m stop    # return each interruption to the worker
play stop           # explicitly ask to stop; never automatically reject this request
```

An automatic response records the actual cause and decision once, in plain text.
It preserves elapsed game time, new game messages, failures, and performance alarm
transitions. It must collect the native result before reporting that continuation
succeeded. It must not restart the entire requested duration after an interruption.

Illustrative output, not a measured playtest:

```
You were hurt! Continued waiting (ignore mode).
Waited 5 minutes. Ready.
```

```
Waiting interrupted: You were hurt!
Stop waiting → play yes
Continue once → play no
Ignore this distraction → play ignore
```

Performance monitoring applies to all modes. Mean completed waiting-turn processing
time above 10 ms over 100 turns emits the measured value and:
`Tell the coordinator: waiting performance needs attention.` Input, transport,
loading and saving are excluded. Automatic prompt handling must not consume or hide
an alarm between displayed replies.

## Native interruption inventory

`src/enums.h` defines these 19 distraction types. All route through shared native
activity handling except the separate portal-storm query. Coverage must enumerate
the enum so a new type cannot silently escape the tests.

| Types | Native handling and required coverage |
| --- | --- |
| noise, pain, attacked | Activity stop/continue/ignore; retain cause, no English keyword classifier |
| hostile_spotted_far, hostile_spotted_near | Same choices; danger must not be treated as harmless |
| talked_to, motion_alarm, weather_change | Same choices; harmlessness cannot be inferred solely from the type name |
| asthma, dangerous_field, hunger, thirst, temperature, oxygen, withdrawal | Same choices; include health/environment threats in dangerous default |
| mutation, eoc, craft_step_complete | Same choices; scripted notices may precede the activity query |
| portal_storm_popup | YES0/YES1/YES2 all lead to the same native continuation and suppression for this activity |

Additional input paths: plain stop-waiting YES/NO, notice acknowledgement/cancel,
chained notice then activity query, distraction-manager menu, scripted choice menus,
death/process exit, unsupported native owner, stale input and failed transport.
Unknown choices must be shown with exact executable commands, not guessed from
their display order. A missing supported continuation is an interface failure to
report, not a successful skipped interruption. Game termination is not continuation.

## Current evidence and implementation gaps

- `game::cancel_activity_or_ignore_query` already implements continue once and ignore
  this distraction. Ignoring applies to the current activity and its backlog.
- `game::portal_storm_query` continues after any of its three YES variants.
- Native query popups advertise their choices and stable targets, including IGNORE
  and MANAGER. The new plain renderer currently exposes only YES/NO and simple
  acknowledgement/cancel, so it is incomplete.
- `CockpitRunChannel.wait_with_danger_handling` defines the three existing modes.
  Its semantic danger helper uses English text fragments; its safe recovery bridge
  also has legacy-frame-specific behavior. Reusing it without examination is not
  sufficient coverage.
- `WaitingPlayer` currently dispatches primitive waits without those mode policies.
  Default-dangerous behavior is therefore a requirement, not yet delivered.
- A Luna native run reached a scheduled notice, dismissed it through its advertised
  Cancel action, and reached a second Stop waiting? prompt. This proves the chain
  exists, not that automatic handling works. That session is being closed.
- Earlier native runs proved normal completion, manual YES/NO and real performance
  alarms. They do not prove mode-dependent automatic handling.

Before completion, exercise every distraction type against all three policies,
explicit manual stop under the dangerous default, chained prompts, remaining-time
continuation, unsupported owners, pending/stale replies, and alarm preservation.
Use native Mac Luna proof for the integrated route, distinguish controlled fixtures
from natural gameplay, audit generated disk records, and deliver the full transcript
with exact character/byte counts. Do not generalize beyond waiting before review.
