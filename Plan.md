# Local OpenVINO Python Runner LLM Intent Layer (Follower NPCs) Plan

# Plan summary
Hello! We are implementing an LLM into this game Cataclysm.
Heres whats happening: shouting in-game (C + b) while having an NPC follower triggers an LLM run.
A NPC centric snapshot is sent to the game, and the LLM answers and is supposed to initiate targeted actions, according to the context.
The LLM call is async, because the game cant stop for 12 seconds.
The Snapshot, LLM call etc are finished. Now its time to hook this into the in-game AI.
The LLM is supposed to determine normal NPC AI calls for the next 1-3 turns.

## Current Status (Done)
- Local runner wired up (stdin/stdout JSON), warm pipeline, metrics logged.
- Snapshot + prompt in place; LLM receives a compact SITUATION block and returns a single-line action response.
- Debug logging captures snapshots, responses, and raw failures for prompt tuning.
- NPC speech is surfaced in-game when parsing succeeds.

## Next: AI Implementation (In-Game Intent Actions)
Goal: convert LLM actions to existing NPC behaviors without destabilizing saves.

### Intent Whitelist (initial)
- guard_area: assign guard mission at current location.
- follow_player: set follow attitude/mission.
- idle: no-op (speech only).

### Crash Avoidance Notes
We previously crashed when intent overrides touched NPC AI state during load.
Guardrails:
- Do not mutate AI state during save/load or before g is fully initialized.
- Apply intent changes only during normal turn processing, with null checks.    
- Keep overrides transient and non-serialized unless explicitly needed later.   
- Only allied followers should obey LLM actions.

### Implementation Steps (short)
1) Add intent override state to NPC (non-serialized for now).
2) Apply actions inside npc::move() with strict guards and TTL (1 action per turn, up to 3).
3) Validate and drop invalid responses; never hard-fail in AI loop.
4) Keep debug logs for: response payload, parsed action, applied behavior.

## Snapshot Format (Reference)
The SITUATION block is finalized and should not be reworked unless behavior regresses.

## Open Questions
- Should any intent be allowed to interrupt combat AI, or only when safe?
- Should overrides serialize for save/load, or always reset?

## Optional To-Do
- Add per-NPC lore summaries (single LLM pass over available lore snippets, cache the result, inject as `your_lore` in SITUATION).
- Two-pass generation: low-temp actions pass + higher-temp speech pass, then merge.

## Edited files:
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
