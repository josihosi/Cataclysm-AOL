# Josef testing packet — hackathon runway harness footing (2026-04-06)

Status: ready for a short non-blocking human spot-check.

Important scope note:
- this packet is **harness footing only**
- it is **not** evidence that the future hackathon feature "chat interface over dialogue branches" is already implemented
- likewise, the ambient probe mentioned below is **not** evidence that the future tiny ambient-trigger NPC model exists yet

This packet exists so Josef can spend a few focused minutes on the highest-value live question instead of reconstructing setup folklore.

## What to try

### 1. Nearby-NPC freeform chat path

Preferred packaged path:
- `python3 tools/openclaw_harness/startup_harness.py probe chat.nearby_npc_basic`

If Josef wants to poke it manually instead of running the packaged probe:
- load `dev` / `Sandy Creek`
- `C`,`t` to open nearby talk
- `Esc` to leave the talk window
- `C`,`b` to open freeform chat
- say a short line like `Hello there.`
- optional follow-up feel checks: ask for jobs or ask to trade

## Expected behavior

- the interaction should land on a nearby NPC instead of drifting into the wrong control path
- the current packaged proof hit **Bruna Priest** as the recipient
- a short freeform player utterance should produce an NPC reply instead of disappearing silently
- the result should feel like an actual survivor reply, not pure bureaucratic mush

## What already passed

- packaged harness report:
  - `.userdata/dev-harness/harness_runs/20260406_092352/probe.report.json`
- screen/artifact split was captured successfully on a runtime-compatible build
- startup autoload passed on captured build `6dcb5b91f7-dirty`; repo `HEAD` differed only in non-runtime paths, so this was not a stale-binary false verdict
- artifact log recorded the expected recipient + prompt/response packet:
  - `prompt Bruna Priest (req_0)`
  - `response Bruna Priest (req_0)`
- current recorded reply body:
  - `Hello Danial. Stay close, it's safer that way.|follow_close|equip_gun|hold_position`

## Suspicious edges to watch

- the harness still warns when `CATA_API_KEY` is unset; this packaged run still produced a prompt/response pair, but the missing key remains a runtime footgun worth watching
- the packaged proof only locks the greeting/freeform recipient path today; quest/trade follow-ups are still product-judgment checks rather than packaged proof
- do **not** treat the other scenarios as part of this ask yet:
  - `ambient.weird_item_reaction` is runnable but still has no `ambient target` / `ambient response` artifacts
  - `locker.weather_wait` is still honestly blocked because the shipped fixture does not contain a real `CAMP_LOCKER` zone

## Why this is the packet

This is the smallest live human check that now stands on top of real harness evidence instead of folklore:
- startup/profile/save path is packaged
- screen/tests/artifacts are separated
- recipient/artifact proof exists
- the remaining question is now mostly feel and follow-up usefulness, which is finally a Josef-shaped question instead of a setup problem
