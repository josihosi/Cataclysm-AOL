# OpenClaw Harness Technical Design

## Purpose

The OpenClaw harness is a debug-first agent integration for Cataclysm-AOL intended to:
- load prepared fixture saves,
- drive bounded test scenarios,
- detect crashes/debug interruptions/UI dead-ends,
- score both functional and qualitative outcomes,
- produce replay artifacts useful for regression, prompt iteration, and design critique.

This is **not** a first-pass attempt at full autonomous play. The first objective is a robust test operator for prepared scenarios.

## Why this is technically plausible in the current codebase

The current repo already contains several ingredients the harness can reuse:

### 1. Text-mode build already exists
- `CMakeLists.txt` exposes `CURSES` and `TILES` build options.
- Current default is text-capable (`CURSES=ON`, `TILES=OFF`).
- The `Makefile` also supports non-tiles builds.

Implication:
- The harness can target a curses/text build first instead of parsing tiled SDL output.
- This gives a terminal-oriented fallback/debug lane even if a richer structured snapshot path is added later.

### 2. Existing LLM bridge already exists
- `src/llm_intent.cpp` already implements:
  - request/response structs,
  - prompt/snapshot packaging,
  - runner configuration,
  - request/response logging,
  - external runner process management.
- `tools/llm_runner/runner.py` already supports JSON-over-stdin/stdout execution and multiple backends.

Implication:
- The harness does not need to invent a second model transport.
- It should reuse the existing request/runner logging and transport patterns wherever practical.

### 3. Existing log locations are useful replay artifacts
- `src/llm_intent.cpp` writes `llm_intent.log` under the config dir.
- The runner also has a dedicated diagnostics log path model.

Implication:
- Harness traces should follow the same operational style: append-only, user-accessible, postmortem-friendly.

### 4. Test bootstrap path already exists
- `tests/test_main.cpp` already initializes core game state, user/config/save/template dirs, world creation, avatar creation, overmap creation, and map loading.

Implication:
- Some harness components can be validated with test binaries and deterministic fixtures without driving the whole live UI.
- Parser/scheduler/snapshot/action translation code should be unit-testable under `tests/`.

### 5. NPC action execution and LLM integration points are already concentrated
- `src/llm_intent.cpp`: prompt/snapshot/parse bridge.
- `src/npc.cpp` and `src/npcmove.cpp`: intent execution and movement/action behavior.
- `src/npctalk.cpp` and `src/do_turn.cpp`: trigger points.

Implication:
- The harness can be implemented as a separate debug/testing layer without scattering logic everywhere.

## Design principles

1. **Structured observation first, raw screen second**
   - Do not make the harness depend primarily on generic terminal vision.
   - Prefer a compact, agent-facing frame assembled from game state.
   - Keep raw curses/screen capture as fallback evidence and ambiguity resolver.

2. **Action grammar first, raw key presses second**
   - The controller should output semantic actions where possible.
   - A translator layer converts those actions into concrete key sequences/menu selections.
   - This reduces token cost and makes replays comprehensible.

3. **Fixture saves over open-ended free play**
   - First target is scenario regression, not sandbox wandering.
   - Saves should be curated and named.

4. **Functional and qualitative evaluation are both first-class**
   - Passing a pipeline while producing dead, repetitive, tactically stupid behavior is not success.

5. **Fallbacks must be deterministic**
   - Parse failure, ambiguity, or harness faults should fall back to safe stop/abort/report behavior, not improvisational chaos.

## High-level architecture

The harness should be split into six layers.

### Layer A: Scenario manager
Responsibilities:
- choose fixture save,
- choose scenario script/objective,
- reset/reload run state,
- write run metadata and verdicts.

Suggested responsibilities include:
- `scenario_id`
- `lane_id` (`base_ai`, `squad_ai`, `bandit_ai`)
- save slot or save path
- target model/backend
- test objective text
- step budget / timeout budget
- run seed / replay id

### Layer B: Observation adapter
Responsibilities:
- produce an agent-facing frame for the current state.
- attach enough context for decision-making without sending full UI sludge every step.

Primary output should be a structured frame object, not plain text.

#### Proposed frame schema v0
```json
{
  "frame_id": 17,
  "mode": "world",
  "submode": "dialogue|inventory|debug|popup|main_menu|save_picker|world",
  "objective": "Ask camp NPC to craft a wooden spear and observe refusal/acceptance quality.",
  "fixture": {
    "lane": "base_ai",
    "save_id": "base_alpha",
    "scenario_id": "craft_requested_item"
  },
  "player_anchor": {
    "symbol": "@",
    "pos": [0, 0]
  },
  "minimap": [
    "....#....",
    "..N.@.z..",
    "....+...."
  ],
  "entities": {
    "friendlies": [
      { "id": "npc_12", "name": "Mara", "dir": "W", "dist": 2, "attitude": "ally" }
    ],
    "hostiles": [
      { "id": "mon_31", "name": "zombie", "dir": "E", "dist": 3, "threat": "moderate" }
    ],
    "neutrals": []
  },
  "interactables": [
    { "type": "furniture", "name": "billboard", "dir": "N", "dist": 1 },
    { "type": "zone", "name": "crafting_stockpile", "dir": "S", "dist": 3 }
  ],
  "recent_log": [
    "Mara says: I can do that if we have enough wood.",
    "You hear shuffling!"
  ],
  "dialogue": null,
  "menu": null,
  "debug": null,
  "alerts": [],
  "available_actions": [
    "move N","move E","move S","move W",
    "talk_to npc_12",
    "examine N",
    "open_inventory",
    "issue_order craft_request",
    "wait"
  ],
  "raw_capture_ref": "runs/2026-03-13/base_alpha/frame_0017.txt"
}
```

#### Required frame fields
- `mode` / `submode`:
  - essential for prompt compression and action validation.
- `objective`:
  - every frame must carry the current test goal.
- `minimap`:
  - compact local spatial summary around the player anchor.
- `entities`:
  - split into friendlies/hostiles/neutrals.
- `recent_log`:
  - last few game log lines.
- `dialogue` / `menu` / `debug`:
  - populated only when relevant.
- `available_actions`:
  - a small, state-dependent action set.
- `alerts`:
  - crash/debug/popup/ambiguous-state warnings.

#### Optional fields to defer
- exact HP/stamina/pain values,
- full inventory dumps,
- complete map render,
- giant history transcripts.

These can be added later if a scenario genuinely needs them.

### Layer C: State classifier
Responsibilities:
- identify stable UI/gameplay states,
- decide whether structured frame generation is sufficient,
- escalate to raw-screen fallback when confidence is low.

Minimum stable classes:
- `main_menu`
- `save_picker`
- `world`
- `dialogue`
- `inventory`
- `debug_menu`
- `yes_no_prompt`
- `more_prompt`
- `debug_popup`
- `crash_or_fatal`
- `unknown`

This classifier can initially be built from explicit UI titles, game state flags, or known prompt/menu markers rather than full OCR-style heuristics.

### Layer D: Action planner/controller
Responsibilities:
- read the compact frame,
- choose a semantic action,
- attach rationale/score if useful,
- stop when objective is complete, blocked, or failed.

The controller should not emit raw keys by default.

#### Proposed semantic action grammar v0
```json
{
  "action": "talk_to",
  "target": "npc_12"
}
```

Supported action families for v0:
- movement:
  - `move <dir>`
  - `wait`
- interaction:
  - `talk_to <entity_id>`
  - `examine <dir|entity_id|tile_id>`
  - `pickup <selector>`
  - `open_inventory`
  - `close_menu`
- menu/dialogue:
  - `select_option <index>`
  - `scroll_up`
  - `scroll_down`
  - `confirm`
  - `cancel`
  - `dismiss_popup`
- test/debug:
  - `debug_open`
  - `debug_spawn_item <itype_id>`
  - `debug_spawn_monster <mtype_id>`
  - `debug_teleport <x> <y>`
  - `debug_set_time <spec>`
- scenario actions:
  - `issue_order <order_kind>`
  - `set_objective_flag <name>`
  - `abort_run <reason>`

Key property:
- the action set should be **state-dependent**.
- Do not expose `debug_spawn_monster` when the harness is not in a debug-capable state.

### Layer E: UI/key translation layer
Responsibilities:
- convert semantic actions into key sequences,
- navigate menus and confirmation prompts,
- keep reusable macros for stable flows.

Examples:
- `load_save base_alpha`
- `dismiss_debug_popup`
- `open_debug_spawn_monster`
- `spawn_item_by_id spear_wood`
- `return_to_world_view`

This layer is where UI brittleness belongs. Keep it out of the planner.

### Layer F: Replay + evaluation layer
Responsibilities:
- persist every observation, action, translation, and verdict,
- compute functional/qualitative scores,
- make comparisons across models/prompts/scenarios practical.

Minimum artifact set per run:
- run manifest (`json`)
- frame stream (`jsonl`)
- raw screen captures for relevant frames
- translated key stream
- final verdict / scorecard
- error and popup transcript if any

## Observation strategy: structured snapshot before terminal scrape

The preferred path is to expose an explicit agent snapshot built from game state, similar in spirit to the current NPC snapshots, but aimed at player/test-harness control.

### Why this is better than raw TUI parsing
- lower token cost,
- clearer semantics,
- less breakage from cosmetic UI changes,
- easier model comparison,
- easier deterministic test coverage.

### Raw curses capture still matters
Raw capture should still exist for:
- debugging harness failures,
- postmortems,
- unknown state fallback,
- visual verification when structured extraction disagrees with reality.

## Mode-specific frame payloads

### World mode
Include:
- compact minimap,
- visible entities,
- nearby interactables,
- recent log,
- current objective,
- available world actions.

### Dialogue mode
Include:
- speaker identity,
- recent dialogue lines,
- selectable options,
- whether free text exists or only choices,
- relevant recent world/log context.

### Inventory/menu mode
Include:
- menu title,
- visible entries only,
- filter/search text if any,
- current cursor/highlight,
- available selection actions.

### Debug mode
Include:
- debug menu title/path,
- visible options,
- search/filter state,
- current cursor/highlight,
- high-level warning that actions here mutate the scenario.

### Popup/crash mode
Include:
- popup type,
- text body,
- allowed dismissal actions,
- severity flag.

## Fixture save strategy

Do not rely on one giant sandbox save. Use curated fixtures.

### Lane 1: Base AI fixtures
Need:
- stable base,
- billboard,
- supplies,
- several NPCs with differentiated roles,
- repeatable craft/fetch/store tasks.

Initial scenarios:
- `craft_requested_item`
- `fetch_missing_component`
- `store_item_in_zone`
- `billboard_mission_discussion`
- `reasonable_refusal`

### Lane 2: Squad AI fixtures
Need:
- several followers,
- nearby raid structure/town,
- clear geometry for positioning tests.

Initial scenarios:
- `breach_building`
- `hold_chokepoint`
- `flank_hostile`
- `regroup_after_split`
- `retreat_wounded_ally`

### Lane 3: Bandit AI fixtures
Need:
- preserved world state with nearby bandit presence,
- or a debug-constructable equivalent once reliable.

Initial scenarios:
- `bandit_scouts`
- `bandit_demand`
- `road_ambush`
- `bandit_disengage`

## Evaluation model

Each run must produce both functional and qualitative judgments.

### Functional metrics
- scenario reached target state Y/N
- crash/fatal/debug popup encountered
- menu dead-end encountered
- pathing or action failure encountered
- replay consistency on rerun
- step budget exceeded Y/N

### Qualitative metrics
- `tactical_quality` (1-5)
- `character_believability` (1-5)
- `initiative` (1-5)
- `narrative_interest` (1-5)
- `obedience_balance` (`too_obedient|good|too_stubborn`)

### Notes field
A freeform human/agent critique should always be present. This is the main bridge from regression testing to actual game design iteration.

## Where this should live in the codebase

### Likely new code areas
- `src/openclaw_harness.*`
  - scenario management
  - frame assembly
  - action grammar validation
  - replay writing
- `src/openclaw_ui_adapter.*`
  - mode classification
  - semantic-action to key translation
  - popup/debug menu handling
- `tests/openclaw_harness_*`
  - parser tests
  - frame schema tests
  - state classifier tests
  - action translation tests

Exact file naming can change, but the harness should remain isolated from core gameplay logic as much as possible.

### Existing files likely to be touched lightly
- `src/llm_intent.cpp`
  - only if reusing request transport/logging helpers is cleaner than duplicating them.
- `src/npctalk.cpp`, `src/do_turn.cpp`, `src/npc.cpp`, `src/npcmove.cpp`
  - ideally not for v0 harness control unless specific reusable snapshot helpers are extracted.
- build system files
  - to optionally compile the harness in debug/test builds.

## Testing strategy

### Unit tests
Add focused tests for:
- frame schema generation,
- state classification,
- menu/popup recognition,
- action grammar validation,
- semantic-action to key translation,
- replay manifest writing.

Suggested names:
- `tests/openclaw_harness_frame_test.cpp`
- `tests/openclaw_harness_classifier_test.cpp`
- `tests/openclaw_harness_actions_test.cpp`
- `tests/openclaw_harness_replay_test.cpp`

### Integration tests
Add scenario-light integration tests where possible:
- construct minimal game state,
- emit a harness frame,
- validate action translation and fallback behavior.

### Live-run validation
Live UI-driven validation is still needed for:
- save loading,
- debug menu navigation,
- real popup handling,
- crash capture,
- full fixture replay.

## Failure handling policy

If the harness cannot confidently classify state or translate an action, it should:
1. capture raw evidence,
2. emit an `unknown_state` or `translation_failure` event,
3. stop or safely abort the run,
4. write an actionable verdict.

Do **not** let ambiguity silently degrade into random keypress nonsense.

## Delivery order

### HV0-A: compile and logging skeleton
- create harness module,
- create replay/run manifest output,
- validate config-dir artifact writing.

### Current startup-harness slice on `dev`
There is now a first macOS-oriented startup harness script at:
- `tools/openclaw_harness/startup_harness.py`

Current scope:
- branch-aware profile/userdir resolution
- existing world/save detection
- fixture save capture/install/list helpers
- launch game with the correct `--userdir`
- autoload a world with saves via `--world` when possible
- otherwise drive the minimal `New Game -> Play Now! (default scenario)` path
- copy `debug.log` deltas into the run artifact directory, capture popup screenshots, and attempt popup ignore with `i`
- detect failure via process exit or startup timeout
- detect success via `config/lastworld.json` updating with a world + character

Example dry-run plan:
- `python3 tools/openclaw_harness/startup_harness.py plan --profile master`
- `python3 tools/openclaw_harness/startup_harness.py start --profile master --dry-run`

Example fixture operations:
- `python3 tools/openclaw_harness/startup_harness.py list-fixtures --profile master`
- `python3 tools/openclaw_harness/startup_harness.py capture-fixture base_alpha --profile master --overwrite`
- `python3 tools/openclaw_harness/startup_harness.py install-fixture base_alpha --profile master --replace`

Notes:
- non-dry-run startup currently requires Peekaboo permissions for Screen Recording and Accessibility
- the first profile config lives at `tools/openclaw_harness/profiles/master.json`
- `port/*` branch variations are expected later via per-profile config rather than one giant hardcoded key script

## Next in-game smoke candidates (current `master` / `Sandy Creek` save)
The first live gameplay smoke ideas discovered after startup success are:
- `C`, `t` to talk to Ricky Broughton
- `a`, `a` inside the quest-first chat window variant to reach the ruleset window
- `C`, `b` to enter a player utterance and submit it
- later, debug-menu path `}`, `s`, `p`, `O` to spawn Rubik for a more curated named-NPC test

These are not fully scripted yet. The next harness work should determine the minimum reliable observables around them:
- is the intended NPC actually nearby?
- can we confirm the chat window contents/state before pressing follow-up keys?
- should we scrape window contents via screenshot/OCR or settle for log-based confirmation first?

### Empirical note from current live probes
A live `C+b` probe in the current save did work, but the utterance routed to **Ricky Broughton**, not Rubik. So recipient selection should be treated as state-dependent and confirmed from `llm_intent.log`, not assumed from rough folk rules about ambient-vs-follower priority.

A small harness-oriented control lookup now lives at:
- `tools/openclaw_harness/CONTROL_LOOKUP.md`

### HV0-B: frame schema + state classifier
- implement frame object,
- implement stable mode classification,
- add unit tests.

### HV0-C: action grammar + translator
- implement semantic action set,
- implement key translation/macros,
- add menu/popup helpers.

### HV0-D: fixture save loading
- support named fixture loads from main menu,
- verify replayable scenario reset.

### HV0-E: first scenario lane
- implement one Base AI and one Squad AI scenario loop,
- add scorecard output.

### HV0-F: debug support
- implement debug-menu navigation helpers,
- support item/monster spawning and scenario setup assistance.

### HV0-G: qualitative evaluation pass
- make scorecard and critique output practical for model/prompt comparison.

## Non-goals for v0
- full autonomous survival play,
- deep inventory optimization,
- perfect understanding of every UI screen,
- replacing existing deterministic AI,
- broad always-on runtime integration.

v0 succeeds if it becomes a reliable scenario test pilot and critique tool.
