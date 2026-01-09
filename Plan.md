# Local OpenVINO Python Runner LLM Intent Layer (Follower NPCs) Plan

## Goal
Add a local (no-network) "LLM Intent Layer" for follower NPCs in Cataclysm: AOL. The player can yell a sentence (talk system: `C + b`, default hotkey) and each hearing follower sends a non-blocking request to a local OpenVINO Python GenAI runner on this machine's NPU. The NPC keeps normal AI while the request runs. Responses map to a small, validated intent whitelist that temporarily overrides NPC behavior for a TTL, then resumes normal AI. Errors/timeouts/invalid JSON are ignored.

## Trigger
- Existing talk system -> "Yell a sentence" in `src/npctalk.cpp` (`NPC_CHAT_SENTENCE`).
- On yell submit, for each follower who can hear, enqueue a local LLM request with a snapshot payload.

## Local Model Runner (NPU, Python)
- Model directory (default): `C:\Users\josef\openvino_models\Mistral-7B-Instruct-v0.2-int4-cw-ov`.
- Python venv directory (default): `C:\Users\josef\openvino_models\openvino_env`.
- Use OpenVINO GenAI Python API with `device="NPU"` and CPU fallback disabled (equivalent to `ENABLE_CPU_FALLBACK="NO"` in `test_downloaded_models.py`).
- Keep the pipeline warm and reused across requests (single worker thread or single process).
- Runner is a local process (stdin/stdout JSON or local HTTP); no API keys, no external services.

### Runner Interface
- The game calls a local Python runner with a JSON request and receives JSON response.
- Default transport is stdin/stdout line-delimited JSON (one request per line, one response per line), with `request_id` echoed back.
- The C++ side stays responsible for request queuing, timeouts, strict JSON validation, and intent whitelisting.

### Local Install
- Do not move models; keep them under `C:\Users\josef\openvino_models`.
- The Python runner uses the venv at `C:\Users\josef\openvino_models\openvino_env`.

## Prompt + Response Schema (Strict JSON)
Prompt should force JSON-only output and include constraints:
```
You are a game NPC intent engine. Return ONLY strict JSON.
Schema: { "intent": "...", "args": { ... }, "ttl_turns": 120, "npc_say": "..." }
Allowed intents: guard_area, move_to, follow_player, clear_overrides, idle.
If unsure, respond with {"intent":"idle","args":{},"ttl_turns":60,"npc_say":""}.
```
Behavior:
- Validate JSON strictly.
- Strip model artifacts like `<think>...</think>` if present (same logic as `test_downloaded_models.py`).
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
- Use a worker thread + queue on the game side.
- Worker only touches snapshot data (no live game state).
- Main loop polls response queue and applies overrides safely.

## Options/Config
- `LLM_INTENT_ENABLE` (bool)
- `LLM_INTENT_PYTHON` (path to `python.exe`, default `C:\Users\josef\openvino_models\openvino_env\Scripts\python.exe`)
- `LLM_INTENT_RUNNER` (path to runner script, default `tools\llm_runner\runner.py`)
- `LLM_INTENT_MODEL_DIR` (path, default to the Mistral directory)
- `LLM_INTENT_DEVICE` (default `NPU`, NPU-only for LLM intents)
- `LLM_INTENT_TIMEOUT_MS` (per request)
- `LLM_INTENT_FORCE_NPU` (bool, fail if NPU not available; no CPU fallback)

## Files To Edit
1. `src/npctalk.cpp`
   - Hook `NPC_CHAT_SENTENCE` to enqueue local LLM requests for hearing followers.
2. `src/llm_intent.h`, `src/llm_intent.cpp`
   - Intent manager: queues, worker thread, timeouts, strict JSON validation, whitelist mapping.
3. `src/llm_intent_runner.h/.cpp` (new)
   - External runner adapter (process management + JSON IO).
4. `src/npc.h`, `src/npc.cpp`
   - Store per-NPC override state: intent id, args, ttl, request_id, npc_say.
   - Apply/clear override; tick TTL in `npc::process_turn()`.
5. `src/npcmove.cpp`
   - If override active, use mapped actions and bypass normal AI until done/expired.
6. `src/options.cpp`
   - Toggle LLM intent layer and configure local model settings.
7. `tools/llm_runner/` (new)
   - Python runner script and helper docs.

## Estimated LoC
- `src/npctalk.cpp`: +30 to +50
- `src/npc.h`: +30 to +50
- `src/npc.cpp`: +80 to +140
- `src/npcmove.cpp`: +40 to +80
- `src/llm_intent.h/.cpp`: +250 to +350
- `src/llm_intent_runner.h/.cpp`: +180 to +260
- `tools/llm_runner/`: +120 to +220
- `src/options.cpp`: +20 to +40
- Total: ~600 to 950 LOC

## Payload Plan (Snapshot Data)
Send compact JSON. Avoid large payloads; summarize items and cap list sizes.

- `request_id`: unique id per request
- `turn`: current game turn
- `player_utterance`: text the player yelled
- `npc_id`, `npc_name`, `npc_pos`
- `npc_state` ("mood" surrogate):
  - morale level (e.g. `Character::get_morale_level()`)
  - hunger, thirst, pain, stamina, sleepiness, HP%
  - key effects (brief list, capped)
- `npc_emotions` (derived summary):
  - fear, anger, stress, confidence (coarse buckets)
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
- `inventory_summary` (required, capped):
  - wielded weapon, ammo count, reload status
  - carried weight/volume percent
  - top 3-5 usable items (meds/tools) with count
  - top 3-5 combat items (grenades, molotovs, etc.)
  - top 3-5 healing items

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
- If `LLM_INTENT_FORCE_NPU` is set and NPU is unavailable, disable the layer and log once.
