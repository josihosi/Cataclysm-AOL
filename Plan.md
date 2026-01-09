# LLM Intent Layer (Follower NPCs) Plan

## Goal
Add an MCP-based "LLM Intent Layer" for follower NPCs in Cataclysm: AOL. The player can yell a sentence (talk system: `C + b`, default hotkey) and each hearing follower sends a non-blocking request to an MCP server over HTTP. The NPC keeps normal AI while the request runs. Responses map to a small, validated intent whitelist that temporarily overrides NPC behavior for a TTL, then resumes normal AI. Errors/timeouts/invalid JSON are ignored.

## Trigger
- Existing talk system -> "Yell a sentence" in `src/npctalk.cpp` (`NPC_CHAT_SENTENCE`).
- On yell submit, for each follower who can hear, enqueue an MCP request (HTTP) with a snapshot payload.

## MCP Tool
- MCP tool name: `npc.intent_from_speech`
- Transport: HTTP
- API key: `CATA_API_KEY` environment variable

## Response Schema (Strict JSON)
Example:
```
{ "intent": "guard_area", "args": { ... }, "ttl_turns": 120, "npc_say": "..." }
```
Behavior:
- Validate JSON strictly.
- Validate intent + args against a whitelist.
- Convert to existing NPC actions/activities.
- Apply override for `ttl_turns` or until finished/interrupted.
- On timeout, error, or invalid JSON: drop response and keep normal AI.

## Intent Whitelist (Initial)
Map intents to existing behavior (no new AI primitives):
- `guard_area`: assign guard mission at current location.
- `move_to`: use `goto_to_this_pos` to walk to a location.
- `follow_player`: set follow attitude/mission.
- `clear_overrides`: clear follower rule overrides.
- `idle`: clear override and allow normal AI.

## Async Requirement
- Request must be non-blocking.
- Use worker thread for HTTP.
- Worker only touches snapshot data (no live game state).
- Main loop polls response queue and applies overrides safely.

## Files To Edit
1. `src/npctalk.cpp`
   - Hook `NPC_CHAT_SENTENCE` to enqueue MCP requests for hearing followers.
2. `src/llm_intent.h`, `src/llm_intent.cpp`
   - Intent manager: queues, worker thread, timeouts, strict JSON validation, whitelist mapping.
3. `src/mcp_client.h`, `src/mcp_client.cpp`
   - HTTP client for MCP tool call `npc.intent_from_speech`.
   - Read API key from `CATA_API_KEY`.
4. `src/npc.h`, `src/npc.cpp`
   - Store per-NPC override state: intent id, args, ttl, request_id, npc_say.
   - Apply/clear override; tick TTL in `npc::process_turn()`.
5. `src/npcmove.cpp`
   - If override active, use mapped actions and bypass normal AI until done/expired.
6. `src/options.cpp` (optional but recommended)
   - Toggle LLM intent layer and configure endpoint/timeouts.

## Estimated LoC
- `src/npctalk.cpp`: +30 to +50
- `src/npc.h`: +30 to +50
- `src/npc.cpp`: +80 to +140
- `src/npcmove.cpp`: +40 to +80
- `src/llm_intent.h/.cpp`: +250 to +350
- `src/mcp_client.h/.cpp`: +200 to +300
- `src/options.cpp`: +20 to +40
- Total: ~650 to 1,000 LOC

## Payload Plan (Snapshot Data)
Send compact JSON. Avoid large payloads; summarize items.

- `request_id`: unique id per request
- `turn`: current game turn
- `player_utterance`: text the player yelled
- `npc_id`, `npc_name`, `npc_pos`
- `npc_state` ("mood" surrogate):
  - morale level (e.g. `Character::get_morale_level()`)
  - hunger, thirst, pain, stamina, HP%
  - key effects (brief list)
- `npc_personality` (from `npc::personality`):
  - aggression, bravery, collector, altruism
- `npc_opinion` (from `npc_opinion`):
  - trust, fear, value, anger
- `threats`: top N visible hostiles
  - name, type, distance, threat rating (or HP%)
- `friendlies`: top N visible allies
  - name, type, distance
- `local_map`:
  - small radius grid (5x5 or 7x7) of terrain/furniture/field flags
- `ruleset`:
  - follower rules: engagement, aim, flags/overrides
  - mission + attitude
- `inventory_summary` (optional):
  - wielded weapon, ammo count
  - top 3-5 usable items (meds/tools)

## Data Sources (Code)
- Mood: `Character::get_morale_level()` and hunger/thirst/pain/stamina in `src/character.h`.
- Personality: `npc::personality` in `src/npc.h`.
- Opinion: `npc_opinion` in `src/npc_opinion.h`.
- Threats/Friendlies: `npc_short_term_cache` in `src/npc.h`, populated in `src/npcmove.cpp`.
- Map info: `map` queries around `npc::pos_bub()` (`src/map.h`/`src/map.cpp`).
- Inventory summary: `Character` inventory in `src/character.h` and `item_location`.
- Ruleset: `npc_follower_rules` in `src/npc.h`; mission/attitude in `src/npc.h`.

## Failover Behavior
- Timeout, error, invalid JSON, or non-whitelisted intent => ignore and continue normal AI.
- Overrides end on TTL expiry, interruption, or completion, then normal AI resumes.
