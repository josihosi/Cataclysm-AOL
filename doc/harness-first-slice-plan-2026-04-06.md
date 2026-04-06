# Harness first-slice plan (2026-04-06)

Status: active auxiliary for the current `Plan.md` hackathon-runway lane.

## Why this exists

The harness should stop being vague future architecture and start becoming a useful playtesting multiplier before Josef's pre-holiday testing window shrinks away.

The guiding principle from the earlier reasoning note is still correct:
- do **not** build a screenshot-first black-box agent
- do **not** start with giant autonomous play
- do build **deterministic little functions** that make live probes reproducible
- use screenshots/artifacts as confirmation layers, not as the whole control model

## Current top goal

Land one reusable, trustworthy live-probe slice that Schani can run with low ritual overhead.
That slice should already be useful for locker testing and should naturally extend toward chat and ambient-reaction testing.

## First-slice success shape

The first harness slice is good enough when it can do all of these reliably on the current binary/profile/save path:
1. reach gameplay from a named startup profile/save
2. state what probe contract is being attempted
3. perform a small deterministic action sequence
4. capture screen/tests/artifacts separately
5. report inconclusive runs honestly instead of flattening them into fake success/failure

## Do first: reusable deterministic little functions

These do not need to be perfect forever; they need to be stable enough to compose.

### A. Session/bootstrap functions
- `start_profile(profile, world)`
- `confirm_map_play()`
- `mark_artifact_boundary()`
- `capture_screenshot(label)`
- `read_recent_artifacts()`
- `advance_turns(count)`

### B. Interaction functions
- `talk_to_nearby_npc()`
  - current practical control: `C`, `t`
- `open_freeform_chat()`
  - current practical control: `C`, `b`
- `send_chat_utterance(text)`
  - type text, `Enter`, then enough turn advance to let the reply inject
- `drop_item(query_or_slot)`
- `pick_up_item(query_or_selector)`

### C. Probe-support functions
- `force_weather(...)`
  - current acceptable first implementation may still be a controlled debug/live path if no direct hook exists yet
- `note_inconclusive(reason)`
- `report_probe(screen, tests, artifacts)`

### D. Scenario-setup helpers (next wave after the first slice)
These are high-value because they turn repeated debug-menu ritual into reusable probe setup.

- `debug_spawn_item(item_id, count=1, location=near_player_or_target_tile)`
- `debug_spawn_monster(monster_id, location=near_player_or_target_tile)`
- `debug_spawn_follower_npc(template_or_name, location=near_player)`
- `assign_npc_to_camp(npc_selector, camp_selector)`
- `assign_npc_to_follower(npc_selector)`

These do not all need to land on day one, but they belong on the near harness roadmap because they directly support:
- locker setup
- chat setup
- ambient-reaction setup
- repeatable scenario staging without hand-driving the debug UI every single time

## First concrete scenarios

### 1. `locker.weather_wait`
Goal:
- force cold or hot weather
- advance enough time/turns for locker downtime
- verify that the live locker planner/service path reacts as expected

Expected evidence split:
- **screen:** visible gear/loadout change or stable visible state
- **tests:** `./tests/cata_test "[camp][locker]"`
- **artifacts/logs:** `camp locker:` before/plan/after packet in the relevant log

### 2. `chat.nearby_npc_basic`
Goal:
- start a nearby NPC conversation
- open freeform chat
- send a few canonical utterances
- verify recipient and result

Starter utterance set:
- neutral greeting / short chat opener
- ask for quests
- ask to trade

Expected evidence split:
- **screen:** chat UI / response text / visible menu state
- **tests:** deterministic chat/intent tests if relevant for the touched path
- **artifacts/logs:** who actually received the utterance and what path recorded it

### 3. `ambient.weird_item_reaction`
Goal:
- stage a slightly uncanny local event, e.g. drop a monster tooth on the floor
- verify whether any ambient NPC reaction path triggers

This scenario is partly parked behind the future tiny-model work, but it should already shape the harness interface. The shipped `basecamp_dev_manual_2026-04-02` fixture already carries nearby follower/camp state, so the current baseline can run without new assign-NPC helpers; those helpers still matter for restaging and variant probes.

Expected evidence split:
- **screen:** visible ambient reaction or lack of reaction
- **tests:** future deterministic event-classification coverage, if/when it exists
- **artifacts/logs:** event packet / trigger record / reaction record

## Important architectural constraint for the tiny ambient model

If a tiny RoBERTa-like model would need ~2 seconds to rescan the whole game log, the architecture is wrong.
The harness and later implementation should assume:
- sparse/event-gated invocation
- small event packets
- cooldown/debounce rules
- no continuous full-log rescanning on every tick

## Implementation order

1. make `locker.weather_wait` reproducible with the current proven save path
2. package the report format: **screen / tests / artifacts**
3. formalize `chat.nearby_npc_basic` around the current known controls (`C,t` and `C,b`)
4. add the first useful scenario-setup helpers so probes stop depending on hand-piloted debug-menu rituals
   - debug spawn item
   - debug spawn monster
   - debug spawn follower NPC
   - assign NPC to camp (only when alternate restaging actually needs it)
   - assign NPC to follower (only when alternate restaging actually needs it)
5. use the already-runnable ambient baseline to shape the tiny-model design, and only grow more staging helpers when the shipped fixture stops being enough

## Non-goals for this slice

- full autonomous player-agent behavior
- OCR/screenshot-first control
- giant action grammar all at once
- pretending an inconclusive run is a stable result

## Working rule for Schani

This is a good autonomous lane.
Schani should push it forward with small trustworthy steps, confirm with screenshots/artifacts when needed, and keep the evidence classes separate.
If a probe works on-screen but not in the expected log, report that as an observability gap rather than collapsing the verdict.
