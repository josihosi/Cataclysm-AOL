# Spoken camp craft final signoff checklist

Gameplay signoff target:
- `4a39c70ac7` — `Narrow spoken camp craft signoff packet`

Behavior-bearing commits already in the packet:
- `696f5c8b61` — main spoken camp craft resolution fix
- `1df9e378c8` — tiny blocked-bark punctuation trim

Already revalidated agent-side:
- `make -j4 tests`
- `./tests/cata_test "[camp][basecamp_ai]"`
- `make version TILES=1 -j4 cataclysm-tiles`
- `python3 tools/openclaw_harness/startup_harness.py start --profile dev --world 'Sandy Creek'`
- latest startup harness run: `.userdata/dev/harness_runs/20260404_172835`
- zero recorded debug popups

## What Josef needs to do

Run exactly this trio in the started `dev` / `Sandy Creek` game:

1. `craft 5 makeshift bandages`
   - expect: positive deterministic path
   - good result: clean pin / no weird chatter / no crash

2. `craft boiled`
   - expect: clarification instead of a silent guess
   - good result: concise human-readable ambiguity list

3. `craft 5 bandages`
   - expect: blocked deterministic path
   - good result: no crash, matched recipe / blocker sufficiently clear, no punctuation stupidity like `tools..`

## Human judgment questions

If the mechanics work, only judge these:
- Does the bark sound human/clear enough rather than bureaucratic?
- When heard text and matched recipe differ, is the wording still understandable?
- Did the tiny punctuation cleanup actually kill the dumb `tools..` artifact in live play?

## If it passes

Then the remaining work is repo hygiene only:
- final public-facing squash / commit curation
- final humanized PR description
- actual PR opening

## If it fails

Please note which probe failed and whether it was:
- wrong routing
- wrong clarification
- blocked wording still ugly
- crash / popup / debug nonsense
- some new piece of absurd theater
