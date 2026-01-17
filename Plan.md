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
### NPC Background Summaries (LLM)
Pre-generate short LLM summaries for all background story entries and include them in the snapshot.

#### Background story routing (preset vs random NPCs)
- Background stories live in `data/json/npcs/Backgrounds/*.json` as `talk_topic` entries, and are wired in `data/json/npcs/Backgrounds/backgrounds_table_of_contents.json` using `npc_has_trait` conditions to route to the correct story topic.
- Random NPCs typically get a background trait from their `npc_class` definition (`data/json/npcs/classes.json`) via trait groups like `BG_survival_story_*` (defined in `data/json/npcs/BG_trait_groups.json` and backed by `data/json/npcs/BG_traits.json`). Those traits are exactly what the table-of-contents conditions check.
- Preset/unique NPCs can override their chatbin topics or traits in their `type: "npc"` JSON entries, which also feeds the same talk_topic pipeline.

#### Best hook points for summaries/hijack
- Central data anchor for summaries: `data/json/npcs/Backgrounds/backgrounds_table_of_contents.json` (trait-to-talk_topic routing). If a trait is referenced there, we can map it to a stable summary entry even as stories evolve.
- Central runtime hook for dialogue rewrite: `dialogue::dynamic_line()` and `dialogue::gen_responses()` in `src/npctalk.cpp` (one place to intercept every topic's line/choices).
 
Specific pipeline sources:
- Story text: `data/json/npcs/Backgrounds/*.json` (`talk_topic` + `dynamic_line`).
- Background group mapping: `data/json/npcs/BG_trait_groups.json` (e.g., `BG_survival_story_*`).
- Assignment sources: `data/json/npcs/classes.json` and other NPC class files under `data/json/npcs/**`,
  plus `data/json/professions.json` (`npc_background`).
Implementation sketch:
- Resolve each background story entry → generate a 1–2 sentence summary once (offline/pre-gen).
- Store summaries alongside the source stories (new JSON file keyed by talk_topic id).
- At runtime, resolve each NPC’s background group → select the active story topic(s) → look up the summary.
- Inject the summary into the SITUATION block as `background_summary:` for the speaking NPC.
Josefs Senf:
We need a little Python summarizer.
It should detect these jsons. Maybe we could python-grep for `BG_survival_story_*`, and work off all files that pop up automatically, after a check whether or not they exist yet.
Lets take the name of that survival story json as file name and _summay_short.
With local LLM ("C:\Users\josef\openvino_models\qwen3-8b-int4-cw-ov"):
Feed the LLM responses or player selected text and responses, and tell it to create the following:

'''
responses etc?
<System>
You must analyze the personality of this fictional Character.
In 5 distinct words, describe them.
And in 5 more words, mirror distinct expressions.
</System>
'''
Something like that.
The output, thinking cut out, should be saved in _survivor_name and 

We must also pull the profession somehow...


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
