# Local OpenVINO Python Runner LLM Intent Layer (Follower NPCs) Plan

## Goal
Add a local (no-network) "LLM Intent Layer" for follower NPCs in Cataclysm: AOL. The player can yell a sentence (talk system: `C + b`, default hotkey) and each hearing follower sends a non-blocking request to a local OpenVINO Python GenAI runner on this machine's NPU. The NPC keeps normal AI while the request runs. Responses map to a small, validated intent whitelist that temporarily overrides NPC behavior for a TTL, then resumes normal AI. Errors/timeouts/invalid JSON are ignored.

## Trigger
- Existing talk system -> "Yell a sentence" in `src/npctalk.cpp` (`NPC_CHAT_SENTENCE`).
- On yell submit, for each follower who can hear, enqueue a local LLM request with a snapshot payload.

## Local Model Runner (NPU, Python)
- Model directory (default): `C:\Users\josef\openvino_models\Phi-3.5-mini-instruct-int4-cw-ov`.
- Python venv directory (default): `C:\Users\josef\openvino_models\openvino_env`.
- Use OpenVINO GenAI Python API with `device="NPU"` and CPU fallback disabled (equivalent to `ENABLE_CPU_FALLBACK="NO"` in `test_downloaded_models.py`).
- Keep the pipeline warm and reused across requests (single worker thread or single process).
- Runner is a local process (stdin/stdout JSON or local HTTP); no API keys, no external services.

### Diagnostics (Minimal)
- Collect NPU-only usage (no CPU/GPU metrics).
- Log build time (pipeline compile) and total load time (model + pipeline ready).
- Emit tokens/sec for generation.
- Log token counts: prompt tokens, generated tokens, total tokens.

### Runner Interface
- The game calls a local Python runner with a JSON request and receives JSON response.
- Default transport is stdin/stdout line-delimited JSON (one request per line, one response per line), with `request_id` echoed back.
- The C++ side stays responsible for request queuing, timeouts, strict JSON validation, and intent whitelisting.

### Local Install
- Do not move models; keep them under `C:\Users\josef\openvino_models`.
- The Python runner uses the venv at `C:\Users\josef\openvino_models\openvino_env`.

## Prompt + Response Schema (JSON Envelope + CSV Payload)
Transport is JSONL (one request per line, one response per line). The LLM output
is a compact CSV payload inside the JSON envelope to keep tokens low.

Prompt should force JSON-only output and include constraints:
```
You are a game NPC response engine. Return ONLY strict JSON.
Return JSON with a single key "csv".
The "csv" value is one CSV line with fields:
1) speech text (always quoted)
2) action1
3) action2 (optional)
4) action3 (optional)
Actions are single tokens (no whitespace or commas).
```

Example output:
```
{"csv":"\"Sure, I'll guard here.\",guard_area"}
```
CSV validation (C++ side):
- JSON must have only `csv` and optionally `request_id` (if echoed back).
- The CSV line must have 2-4 fields.
- Field 1 must be double-quoted; inner quotes must be escaped as `""`.
- Fields 2-4 must be action tokens: `[a-z0-9_]+` (no whitespace, no commas).
- If CSV parse fails, ignore response and keep normal AI.
Actions (will be implemented later):
- Convert to existing NPC actions/activities.
- Apply override for `ttl_turns` or until finished/interrupted.
- On timeout, error, or invalid JSON: drop response and keep normal AI.

## Intent Whitelist (Later)
Map intents to existing behavior (no new AI primitives):
- `guard_area`: assign guard mission at current location.
- `look`: look around for items (might be called differently)
- `move_to`: use `goto_to_this_pos` to walk to a location.
- `follow_player`: set follow attitude/mission.
- `clear_overrides`: clear follower rule overrides.
- `idle`: change follower rules.
- `wield`: wield an item (might also be called differently)
- `attack with weapon` ??
- `shoot with weapon` ??

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

## Payload Plan (Snapshot Data)
We are sending a custom ASCII snapshot and a Legend.
Here an example.

```
<Legend> (something like):

- ... open area
0 ... obstructive area (movement speed > 100)
6 ... obstructed area
[a - z] ... creature
[A - Z] ... obstructed creature
| ... You

</Legend>

<Map> (7x7):

-------
0000000
--a----
oob|---
6666---
66666--
666666-

| ... npc_personality, npc_state, npc_emotions
| ... ruleset, inventory_summary
a ... User "Name", "Utterance", npc_opinion
b ... Creature name, threat stat

</Map>

```

Send compact JSON. Avoid large payloads; summarize items and cap list sizes.
Suggested short keys (request JSON):
- `id`: request_id
- `t`: turn
- `u`: player_utterance
- `nid`: npc_id
- `nn`: npc_name
- `np`: npc_pos
- `st`: npc_state
- `em`: npc_emotions
- `per`: npc_personality
- `op`: npc_opinion
- `th`: threats
- `fr`: friendlies
- `rs`: ruleset
- `inv`: inventory_summary
- `map`: ascii_map_snapshot

- `request_id`: unique id per request
- `turn`: current game turn
- `player_utterance`: text the player yelled
- `npc_id`, `npc_name`, `npc_pos`
- `npc_state` ("mood" surrogate):
  - morale level (e.g. `Character::get_morale_level()`)
  - hunger, thirst, pain, stamina, sleepiness, HP%, encumberance
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
- `ruleset`:
  - follower rules: engagement, aim, flags/overrides
  - mission + attitude
- `inventory_summary` (required, capped):
  - wielded weapon, ammo count
  - top 3-5 usable items (meds/tools) with count
  - top 3-5 combat items (guns, melee, grenades, molotovs, etc.)
  - bool: bandage possible?
  - bool: painkiller available
  - bool: desinfect possible

## Data Sources (Code)
- Mood: `Character::get_morale_level()` and hunger/thirst/pain/stamina in `src/character.h`.
- Personality: `npc::personality` in `src/npc.h`.
- Opinion: `npc_opinion` in `src/npc_opinion.h`.
- Threats/Friendlies: `npc_short_term_cache` in `src/npc.h`, populated in `src/npcmove.cpp`.
- Map info: `map` queries around `npc::pos_bub()` (`src/map.h`/`src/map.cpp`).
- Inventory summary: `Character` inventory in `src/character.h` and `item_location`.
- Ruleset: `npc_follower_rules` in `src/npc.h`; mission/attitude in `src/npc.h`.

## Token limits
Max tokens should be set dynamically. to the next power of 2, according to payload size, if possible, idk :)
