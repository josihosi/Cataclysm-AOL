# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active queue: **Hackathon chat dialogue Stage 1**

Current execution order:
1. playtest API-only chat streaming:
   - opener and reply text should appear progressively, not all at once
   - streamed text should show only NPC speech, stopping cleanly before the `|` delimiter
   - the model should return `say | tool` instead of JSON for chat mode
   - non-API backends should keep the old non-streaming behavior
2. classify the reported run-phase segfault if it reproduces
3. run the next playtest around:
   - fresh conversation opener behavior
   - trade still opening
   - sandbox action selection versus branch action selection in `config/llm_dialogue_chat.log`
   - compact relationship memory showing up instead of stale old chat carry-over
4. if streaming feels good, treat this dialogue slice as demo-ready
