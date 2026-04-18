# Hackathon Chat Dialogue Todo

Date: 2026-04-18

This file is intentionally brief.
Research, prompt drafting, and design notes should live elsewhere.
Delete completed clutter instead of turning this into a graveyard.

## 1. Stage 1 - Minimum Viable Demo

Active follow-up after first playtest:
- Investigate the `build_and_run.cmd` / run-phase segmentation fault report and determine whether it is a startup crash, exit-time crash, or helper-script/runtime issue.
- Fix `?` input behavior in chat entry.
- Close or suspend the chat UI correctly when a tool opens another UI such as trade.
- Move chat entry under `Your response:` instead of floating over the transcript.
- Tighten the prompt and/or memory so repeated lines like `keep your hands where I can see them` do not dominate every answer.
- Fix job/quest behavior so the NPC does not improvise fake work when the real dialogue state does not support it.
- Review fallback behavior and make sure chat does not silently default to authored fallback more than necessary.
- Run the next narrow honest compile/test pass for the follow-up slice.

Trash pile / already landed:
- `[LLM]` dialogue-mode toggle exists: `branches` vs `chat`.
- Old branch dialogue path exists as fallback.
- Chat mode replaces visible response selection with freeform input.
- Conversation starts with an LLM-generated opening beat.
- Each player line gets a freeform NPC reply.
- Backend can optionally trigger one real current legal dialogue action per turn.
- Dedicated chat prompt file exists.
- Dedicated chat log file exists separate from `config/llm_intent.log`.
- First coherent compile checkpoint is done.

## 2. Stage 2 - Quality Pass

- Improve the chat prompt for more naturalistic survivor dialogue.
- Improve current-topic authored-context injection.
- Tune opener quality, brevity, and mood-setting.
- Make identity, trade, and work asks feel acceptable, and let rumors/background land softly.
- Improve the "someone is talking to you" feel if there is a cheap path, for example streaming or a truer inline input.
- Add selective medium voice/story cards for demo-important NPCs if needed.
- Run the narrowest honest compile/test pass for the stage.

## 3. Stage 3 - Stretch

- Improve nearby authored-context retrieval beyond the immediate topic pocket.
- Improve recent thematic memory and continuity across turns.
- Add more expressive but still restrained environmental/physical beats.
- Widen the hidden action layer carefully if the core is stable.
- Expand selective voice/story cards only where they clearly improve the demo.
