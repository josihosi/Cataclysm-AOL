# Local LLM Intent Layer (Follower NPCs) Plan
# Plan summary
We are integrating a local LLM into Cataclysm to let allied NPC followers react
to player shouts. The game sends an NPC-centric snapshot, the LLM replies with a
single-line speech + action list, and the NPC applies those actions for a few
turns using existing AI behaviors. The system is async (no frame stalls), guard
railed for safety, and optimized for fast prompt iteration.

## Update summary (latest)
- Added build-time background summarizer that writes per-story summary JSON files under `data/json/npcs/Backgrounds/Summaries_short`.
- Added a new [LLM] option for the summary model directory and injected `your_profession` + `background_summary` into the LLM snapshot.
- Summarizer is non-fatal on missing model or deps and only fills in missing entries.

## Current Status (Done)
- Local runner wired up (stdin/stdout JSON), warm pipeline, metrics logged.
- Snapshot + prompt in place; LLM receives a compact SITUATION block and returns a single-line action response.
- Debug logging captures snapshots, responses, and raw failures for prompt tuning.
- NPC speech is surfaced in-game when parsing succeeds.

## Next: AI Implementation (In-Game Intent Actions)
Goal: expand LLM actions to cover combat/movement behaviors.

### Intent Whitelist
- guard_area: assign guard mission at current location.
- follow_player: set follow attitude/mission.
- idle: no-op (speech only).
- equip_gun: allow guns + wield best gun.
- equip_melee: disallow guns + wield best melee (or unwield gun).
- equip_bow: allow guns + dissallow loud weapons.
- attack=<target> to attack a certain target.

#To-Do:
### Runner stability on Linux (LLM)
- Detect Windows-style `LLM_INTENT_PYTHON` or `LLM_INTENT_MODEL_DIR` and show a clear in-game error before spawning the runner.
- Default Linux-friendly [LLM] values when the options file is missing or stale:
  - `LLM_INTENT_PYTHON=python3`
  - `LLM_INTENT_DEVICE=AUTO`
  - `LLM_INTENT_FORCE_NPU=false`
- Document WSL/Linux setup: install `any-llm` for API mode and verify with `runner.py --self-test`.
- Note that the llm_runner must NOT require an API key to be set, since some uses do not require an API key.
- Add a minimal runner health-check option that can be triggered from the [LLM] menu (future UI hook).

#Later to-Do, not now:
- Look
- Throw grenades
- Move instructions
- LLM Finetuning
Finetuning/Distilling would increase speed and accuracy. Is that legal?
### Complete NPC Dialogue/Interaction Overhaul??
lol

### API LLM (Any-LLM)
- Add options for API usage (Use API call instead, API key env var name, provider, model).
- Warning: API calls will cost money.
						  
#### Technical: how dialogue options are built (current architecture)
- Dialogue data is loaded at startup from `type: "talk_topic"` JSON across `data/json/npcs/**` (including `data/json/npcs/Backgrounds/*.json`) into the `json_talk_topics` map in `src/npctalk.cpp` via `load_talk_topic()`.
- A conversation starts in `avatar::talk_to()` (`src/npctalk.cpp`), which creates a `dialogue` with two `talker`s and pulls an initial topic stack from `talker_npc::get_topics()` (`src/talker_npc.cpp`).
- `talker_npc::get_topics()` picks the NPC's `dialogue_chatbin` topics (first_topic, talk_friend, talk_leader, etc. in `src/dialogue_chatbin.h`), then falls back to `npc::pick_talk_topic()` (`src/npctalk.cpp`) which selects a stranger/friend topic based on personality/opinion.
- For each topic, `dialogue::dynamic_line()` calls `json_talk_topic::get_dynamic_line()` and `dialogue::gen_responses()` calls `json_talk_topic::gen_responses()` to build the visible player responses from JSON, filtering by dialogue conditions and attaching effects/trials.
- The response's effect sets the next topic; the loop continues until `TALK_DONE`, with `TALK_NONE` popping the stack. This is the central hook if we ever want to intercept or rewrite dialogue selection globally.
