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
### LLM intent logging cleanup (done)
- Group logs by request id and emit in-order: request prompt, response, then follow-on action logs.
- Add a blank line after each pickup log entry to visually separate actions.
- Keep log entries single-line JSON for request/response blocks.

### UTF-8 encode failure on look_around output (done)
- Reproduce the utf-8 codec error from config/llm_intent.log.
- Sanitize LLM output before JSON/log serialization so invalid surrogate code points are removed or replaced.
- Ensure prompt logging uses the same sanitizer so request/response pairs always write cleanly.

### look_around/look_inventory prompt cleanup (in progress)
- Strip UI color tags and container suffixes from item display strings before sending to the LLM. (color tags stripped; container suffix cleanup pending)
- Add stable item ids to the prompt and instruct the LLM to return ids only. (pending)
- Keep corpses in the list, but make names compact and consistent for matching and logging. (name normalization added; verify corpse handling)
- Normalize prompt text to ASCII-safe characters before logging to avoid mojibake. (item name normalization added)

### NPC rules UI crash (ImGui Missing End())
- Reproduce the crash when setting NPC follower rules.
- Identify the ImGui window stack imbalance causing the assert.
- Add a guarded fix that keeps the rules UI and any nested popups balanced.
> I tested dev branch and master branch. Both have the crash on all occasions (with LLM intent first, without). I do remember distinctly that this used to work, so there is a commit somewhere where this menu worked.
> I would wager that this broke around the commit 'Linux supported!(?)' Maybe I should checkout earlier commits?
> There is now a crash report in config/crash.log 
> I know this looks like it is the menu crashing, but THIS WAS US. WE BROKE SOMETHING. 100% there is nooooo other way, ok? Its just logic. it worked. we coded. broke. MUST. BE. OUR. CHANGES.

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
