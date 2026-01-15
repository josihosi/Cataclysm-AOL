# Local LLM Intent Layer (Follower NPCs) Plan
# Plan summary
We are integrating a local LLM into Cataclysm to let allied NPC followers react
to player shouts. The game sends an NPC-centric snapshot, the LLM replies with a
single-line speech + action list, and the NPC applies those actions for a few
turns using existing AI behaviors. The system is async (no frame stalls), guard
railed for safety, and optimized for fast prompt iteration.

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
### Throw grenades
Pretty much self explanatory
### Move instructions
Same
### LLM Finetuning
Finetuning/Distilling would increase speed and accuracy.
### Complete NPC Dialogue/Interaction Overhaul??
lol

### API LLM (Any-LLM)
- Add options for API usage (Use API call instead, API key env var name, provider, model).
- Warning: API calls will cost money.
