# Hackathon Chat Dialogue Todo

Date: 2026-04-18

This file is intentionally brief.
Research, prompt drafting, and design notes should live elsewhere.
Delete completed clutter instead of turning this into a graveyard.

## 1. Stage 1 - Minimum Viable Demo

Active follow-up after first playtest:
- Investigate the `build_and_run.cmd` / run-phase segmentation fault report and determine whether it is a startup crash, exit-time crash, or helper-script/runtime issue.
- Verify that API-backed chat now streams progressively in the dialogue UI.
- Verify that streamed text shows only NPC speech and stops cleanly before the `|` delimiter.
- Verify that non-API backends still fall back cleanly to non-streaming chat behavior.
- Verify that the transcript now preserves player-gray and NPC-white speaker separation in actual play.
- Verify that sandbox trade actually opens and no longer gets trapped inside a quest-status branch pocket.
- Verify that quest-status and quest-offer sandbox actions route to the right follow-up state.
- Verify that recruit tone tracks the surfaced inclination note.
- Verify that fresh talk starts now wipe short rolling chat memory while keeping one compact persistent relationship-memory line.
- Verify that repeated lines like `keep your hands where I can see them` are reduced by the prompt/memory hardening.
- Verify that work/quest wording now stays honest unless a real legal hidden action exists.
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
- Chat action surface is now split into `sandbox actions` and `branch actions`.
- Sandbox trade, quest-status, quest-offer, identity, and recruit hooks are now exposed separately from branch-local responses.
- The chat log and prompt now print sandbox actions and branch actions separately.
- Fresh conversation start now clears short rolling chat memory and keeps a compact relationship-memory line.
- Chat transcript rendering now preserves per-speaker colors instead of flattening older lines to gray.
- Fresh opener turns now use opener-only context with separate inspectable prompt and honest log labeling.
- API-only chat streaming is now wired for progressive NPC text display.
- The streaming contract is now a pipe-delimited one-line reply: `say text | tool_id`.

## 2. Stage 2 - Quality Pass

- Improve the chat prompt for more naturalistic survivor dialogue.
- Improve current-topic authored-context injection.
- Tune opener quality, brevity, and mood-setting.
- Make identity, trade, and work asks feel acceptable, and let rumors/background land softly.
- Improve the "someone is talking to you" feel if there is a cheap path, for example streaming or a truer inline input. This becomes the likely next lane once trade works reliably.
- Add selective medium voice/story cards for demo-important NPCs if needed.
- Run the narrowest honest compile/test pass for the stage.

## 3. Stage 3 - Stretch

- Improve nearby authored-context retrieval beyond the immediate topic pocket.
- Improve recent thematic memory and continuity across turns.
- Add more expressive but still restrained environmental/physical beats.
- Widen the hidden action layer carefully if the core is stable.
- Expand selective voice/story cards only where they clearly improve the demo.
