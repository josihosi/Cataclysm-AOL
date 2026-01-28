# Local LLM Intent Layer (Follower NPCs) Plan
# Plan summary
We are integrating a local LLM into Cataclysm to let allied NPC followers react
to player shouts. The game sends an NPC-centric snapshot, the LLM replies with a
single-line speech + action list, and the NPC applies those actions for a few
turns using existing AI behaviors. The system is async (no frame stalls), guard
railed for safety, and optimized for fast prompt iteration.

## Next: AI Implementation (In-Game Intent Actions)
Goal: expand LLM actions to cover combat/movement behaviors.

### Intent Whitelist
- guard_area: assign guard mission at current location / hold position.
- follow_player: set follow attitude/mission (close/nearby variants).
- idle: no-op (speech only).
- equip_gun: allow guns + wield best gun.
- equip_melee: disallow guns + wield best melee (or unwield gun).
- equip_bow: allow guns + dissallow loud weapons.
- attack=<target> to attack a certain target.

#To-Do:
### Reliability / Flow
- Buffer multi-step intents: when look_inventory/look_around trigger a second toolcall, preserve the first toolcall’s actions instead of overwriting them.
- Snapshot follower mode: include current follow distance/state (follow-close, follow-afar, guard/hold) so the LLM stops spamming follow_player and maps to the right in-game mode.
- Conversation memory: add the last 1–2 utterance/response pairs to the snapshot (brief, sanitized) for tone/continuity.
- Malformed-output safety: retry once with a short "fix format" prompt and strict timeout before dropping.

### Porting & Releases
- Port to CDDA stable (latest release tag) and mainline experimental with full feature parity.
- Port to CDDA-TLG (https://github.com/Cataclysm-TLG/Cataclysm-TLG) with identical LLM features.
- Automate the experimental and TLG refresh (Codex-driven) to resolve merge drift on a regular cadence.
- Integrate distribution into CDDA-Game-Launcher “kitty” installer (https://github.com/Fris0uman/CDDA-Game-Launcher) so players can install/update this fork.

#Later to-Do, not now:
- Throw grenades
- Move instructions
- LLM Finetuning
Finetuning/Distilling would increase speed and accuracy. Is that legal?


### API LLM (Any-LLM)
- Add options for API usage (Use API call instead, API key env var name, provider, model).
- Warning: API calls will cost money.
						  
 ### Complete NPC Dialogue/Interaction Overhaul??
lol
#### Technical: how dialogue options are built (current architecture)
- Dialogue data is loaded at startup from `type: "talk_topic"` JSON across `data/json/npcs/**` (including `data/json/npcs/Backgrounds/*.json`) into the `json_talk_topics` map in `src/npctalk.cpp` via `load_talk_topic()`.
- A conversation starts in `avatar::talk_to()` (`src/npctalk.cpp`), which creates a `dialogue` with two `talker`s and pulls an initial topic stack from `talker_npc::get_topics()` (`src/talker_npc.cpp`).
- `talker_npc::get_topics()` picks the NPC's `dialogue_chatbin` topics (first_topic, talk_friend, talk_leader, etc. in `src/dialogue_chatbin.h`), then falls back to `npc::pick_talk_topic()` (`src/npctalk.cpp`) which selects a stranger/friend topic based on personality/opinion.
- For each topic, `dialogue::dynamic_line()` calls `json_talk_topic::get_dynamic_line()` and `dialogue::gen_responses()` calls `json_talk_topic::gen_responses()` to build the visible player responses from JSON, filtering by dialogue conditions and attaching effects/trials.
- The response's effect sets the next topic; the loop continues until `TALK_DONE`, with `TALK_NONE` popping the stack. This is the central hook if we ever want to intercept or rewrite dialogue selection globally.
