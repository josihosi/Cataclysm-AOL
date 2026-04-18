# Hackathon Chat Dialogue Todo

Date: 2026-04-18

This file is intentionally brief.
Research, prompt drafting, and design notes should live elsewhere.
Delete completed clutter instead of turning this into a graveyard.

## 1. Stage 1 - Minimum Viable Demo

Active follow-up after first playtest:
- Review the next chat logs as a dedicated debug session, especially around trade refusal.
- Investigate the `build_and_run.cmd` / run-phase segmentation fault report and determine whether it is a startup crash, exit-time crash, or helper-script/runtime issue.
- Verify that `?` input now works in the response-area chat entry. <WORKS>
- Verify that tool-backed UI such as trade no longer overlaps a floating chat popup. <NPC REFUSES TO TRADE>
- Rework speaker colors so player text stays gray and NPC speech reads clearly in white.
- Clean up chat-history UX in the response-area input so it feels intentional and closes cleanly.
- Verify that repeated lines like `keep your hands where I can see them` are reduced by the prompt/memory hardening.
- Verify that work/quest wording now stays honest unless a real legal hidden action exists.
- Diagnose why an NPC may still refuse to trade even when chat otherwise works.
- Verify that real topic changes now get a fresh opener instead of dropping into silent blank-slate input.
- Review fallback behavior and make sure chat does not silently default to authored fallback more than necessary.
- Run the next playtest on the hardened slice.

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
- Chat input now renders in the dialogue response area instead of a floating popup.
- Prompt/tool/memory hardening landed for repeated-line control and work/quest honesty.
- Topic-changing hidden actions now reset the opener so the next state can speak first.
- The follow-up narrow compile checkpoint is done.

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
