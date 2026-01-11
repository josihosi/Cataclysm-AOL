# Local OpenVINO Python Runner LLM Intent Layer (Follower NPCs) Plan

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
- clear_overrides: clear follower rule overrides.
- idle: pause/stand still.

### Crash Avoidance Notes
We previously crashed when intent overrides touched NPC AI state during load.
Guardrails:
- Do not mutate AI state during save/load or before g is fully initialized.
- Apply intent changes only during normal turn processing, with null checks.
- Keep overrides transient and non-serialized unless explicitly needed later.

### Implementation Steps (short)
1) Add intent override state to NPC (non-serialized for now).
2) Apply actions inside npc::move() with strict guards and TTL.
3) Validate and drop invalid responses; never hard-fail in AI loop.
4) Keep debug logs for: response payload, parsed action, applied behavior.

## Snapshot Format (Reference)
The SITUATION block is finalized and should not be reworked unless behavior regresses.

## Open Questions
- Should any intent be allowed to interrupt combat AI, or only when safe?
- Should overrides serialize for save/load, or always reset?
