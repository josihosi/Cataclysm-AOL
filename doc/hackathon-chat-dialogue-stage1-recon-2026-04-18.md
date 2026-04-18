# Hackathon Chat Dialogue Stage 1 Recon

Date: 2026-04-18

Companion files:
- `doc/hackathon-chat-dialogue-concept-2026-04-18.md`
- `doc/hackathon-chat-dialogue-todo-2026-04-18.md`

This file is Stage 1 only.
It exists to make the first implementation pass concrete.
When a Stage 1 todo item is finished and honestly validated, either delete the matching recon subsection or move it into a later archive/parked-notes file.
The numbered implementation sections below map directly to the Stage 1 bullets in `doc/hackathon-chat-dialogue-todo-2026-04-18.md`.
Recon by itself does not cross off any todo item.

No compile was run for this recon.
No gameplay/code implementation is included here.

## 0. Active Follow-up After First Playtest

These are the current live Stage 1 follow-up items after the first real in-game test.
They map to the active bullets in `doc/hackathon-chat-dialogue-todo-2026-04-18.md`.
Sections `2` through `13` below are now mostly archived implementation-prep notes from before the first landing.

### 0.1 Run-phase segfault note from `build_and_run.cmd`

Observed note:
- `/usr/bin/bash: line 1:   434 Segmentation fault         ./$RUN_EX`

What this means:
- `build_and_run.cmd` launches `cataclysm-tiles.exe` from MSYS2 `bash.exe`.
- That line means the launched game process crashed.
- It does **not** by itself mean the helper script is wrong.

Current evidence:
- `config/debug.log` currently shows a clean earlier run with `Log shutdown.` at `2026-04-18 12:08:50`.
- So the crash note is either:
  - from a later run not reflected in the current log snapshot,
  - from a startup/load crash before the log flushed fully,
  - or from an exit-time crash after a later play session.

Recommendation:
- Treat this as a real crash note, but not yet a diagnosed chat bug.
- For the next repro, capture whether it happens:
  - right on startup,
  - after loading the world,
  - after opening chat,
  - or on exit.
- If it repeats, check `config/debug.log` immediately after that exact run.

### 0.2 Chat entry rejects `?`

Observed behavior:
- The first floating popup chat entry appeared to reject `?`.

Current follow-up state:
- The popup path has now been replaced with response-area input using `string_input_popup`.
- This likely fixes the punctuation issue as part of the same seam change, but it still needs real playtest confirmation.

Recommendation:
- Re-test with `?` and a few other punctuation characters during the next playtest.

### 0.3 Trade or other tool UI overlaps the chat window

Observed behavior:
- When trade opens, the chat UI stays on screen and overlaps the other UI.

Current follow-up state:
- The floating popup input path has been removed.
- Chat entry now lives in the response area, which should eliminate the overlapping popup problem.

Recommendation:
- Re-test trade and any other tool-backed UI during the next play session to confirm the overlap is gone.

### 0.4 Chat entry should live under `Your response:`

Observed behavior:
- The floating popup works, but the right visual home is the existing `Your response:` area in the dialogue window.

Current follow-up state:
- This is now implemented.
- Chat entry is rendered in the dialogue response area via `dialogue_window`.

Engineering caution:
- Do not overbuild a whole streaming chat editor just because the input is now placed correctly.

### 0.5 Repeated signature line problem

Observed behavior:
- The NPC keeps repeating variants of `keep your hands where I can see them`.

Current log evidence:
- `config/llm_dialogue_chat.log` shows that phrase repeated in recent-memory carry-forward.
- The prompt currently asks for grounded, tense survivor speech, but it does not strongly discourage repeated signature lines.
- The current authored line on those turns stayed the same, which further reinforced repetition.

Recommendation:
- Tighten the prompt so repeated stock lines are discouraged unless the immediate situation truly changes.
- Consider reducing or deduplicating the recent-memory packet before prompt assembly.
- Current follow-up state:
  - a prompt rule against repeated warning or catchphrase lines is now in the chat prompt
  - near-identical NPC chat lines are now deduplicated before being added back into memory
- This still needs real playtest confirmation.

### 0.6 Work or quest hallucination versus real backend action

Observed behavior:
- The NPC improvised a vague job description.
- Saying `I'll do it` did not add anything to the quest log.

Current log evidence:
- On the relevant logged turn, the legal hidden actions were:
  - `What should we do now?`
  - `Any tips?`
  - `Can I do anything for you?`
  - `Want to travel with me?`
  - `Let's trade items.`
  - `Want to share some useful items with me?`
  - `Take care.`
- There was no actual mission-offer or mission-accept action in the legal hidden tool list on that turn.
- The model returned freeform text with `tool=""`.

Conclusion:
- The immediate bug is not that a quest tool failed to execute.
- The immediate bug is that the visible reply implied a concrete job despite there being no real quest-bearing action available.

Recommendation:
- Tighten the prompt so work or quest flavor must stay vague unless a real current hidden action supports it.
- The model can still sound useful and atmospheric, but it should not imply `quest accepted` or `go do X for me` when no backend tool exists.
- Current follow-up state:
  - the chat prompt now explicitly forbids fake assignment or quest-log language without a real legal hidden action
  - the hidden tool packet now includes simple semantic hints such as `trade` or `work_help`
  - topic-changing hidden actions now reset the opener so the next state can speak first
- This still needs real playtest confirmation.

### 0.7 Fallback review

Observed concern:
- The current fallback is intentionally soft, but it must not become the quiet default path.

Recommendation:
- Keep fallback for hackathon safety.
- But add a quick review pass to ensure:
  - chat success is the dominant path,
  - fallback turns are clearly logged,
  - and authored fallback text is not masking normal chat failures during playtest.

### 0.8 Trade refusal despite chat working

Observed behavior:
- The NPC can still refuse to trade even when the chat flow itself feels good.

Likely causes:
- the model saw a legal trade-capable hidden action but chose freeform text with `tool=""`
- the current topic on that turn did not actually expose a trade-capable hidden action
- fallback or topic drift interfered with the expected trade moment

Recommendation:
- Treat this as a log-first issue before changing behavior blindly.
- On the next debug pass, inspect:
  - the legal hidden actions on the trade request turn
  - the raw model output
  - whether the result used a tool or stayed purely freeform

### 0.9 Speaker colors

Observed behavior:
- The current transcript is atmospheric, but the speaker separation can be clearer.

Requested direction:
- player text should stay gray
- NPC speech should read in white

Recommendation:
- Keep the change narrow and visual only.
- Do not redesign the whole dialogue palette; just make speaker ownership easier to scan.

### 0.10 Chat-history UX

Observed behavior:
- A history action can open unexpectedly in chat input.
- The current history flow does not close cleanly enough.
- `d: delete history` reads like tooling, not in-world dialogue UX.

Requested direction:
- history can exist, but it should feel intentional
- if delete wording remains visible, it should be phrased more naturally, e.g. `We started off on the wrong foot. Let's rewind`

Recommendation:
- Decide whether the right fix is:
  - keep history but make it intentional and better-worded
  - or simplify history access so accidental opening stops happening
- Prefer the simplest behavior that still feels good in play.

### 0.11 Recommended follow-up order

1. Confirm the run-phase segfault is reproducible and classify when it happens.
2. Review the next trade-refusal logs as their own debug session.
3. Rework the response-area UX:
   - `?` input
   - speaker colors
   - chat history behavior
4. Re-test prompt and memory behavior:
   - reduced repeated warnings
   - more honest work or quest wording
5. Re-test topic-change feel after hidden actions.
6. Review fallback rate in `config/llm_dialogue_chat.log`.

## 1. Scope Rule For Stage 1

The first implementation pass should stay focused on the minimum viable demo:
- option toggle
- safe fallback
- blank-slate player input
- LLM opener
- LLM freeform reply
- optional hidden current legal action
- prompt file reviewability
- dedicated chat log

Do not pull Stage 2 or 3 concerns into the first patch unless they become necessary to make Stage 1 work at all.

## 2. Item 1.1 - Add the `[LLM]` dialogue-mode toggle

Todo reference:
- Stage 1 bullet 1 in `doc/hackathon-chat-dialogue-todo-2026-04-18.md`

Findings:
- The `[LLM]` options page already exists in `src/options.cpp:224`.
- The page is populated by `options_manager::add_options_llm()` at `src/options.cpp:2922`.
- Existing nearby LLM options include:
  - `DEBUG_LLM_INTENT_LOG` at `src/options.cpp:2944`
  - `LLM_INTENT_ENABLE` at `src/options.cpp:2956`
  - `LLM_INTENT_BACKEND` at `src/options.cpp:2968`

Recommendation:
- Add a new string-select option near `LLM_INTENT_ENABLE`.
- Recommended key: `LLM_DIALOGUE_MODE`
- Recommended values:
  - `branches`
  - `chat`
- Recommended default: `branches`

Engineering note:
- This is low risk and local to `src/options.cpp`.
- It should be done first because it gives the runtime gate for the rest of the feature.

## 3. Item 1.2 - Keep the old branch dialogue path as the hard fallback

Todo reference:
- Stage 1 bullet 2

Findings:
- The normal conversation entry is `avatar::talk_to(...)` at `src/npctalk.cpp:1933`.
- The main per-topic UI loop is `dialogue::opt(...)` at `src/npctalk.cpp:3220`.
- The current loop already handles:
  - `TALK_NONE`
  - `TALK_DONE`
  - topic stack progression
- The current quit helper already exists as `dialogue::get_best_quit_response()` at `src/npctalk.cpp:3409`.

Recommendation:
- Chat mode should only activate when all of these are true:
  - normal NPC conversation
  - not computer interaction
  - not `is_not_conversation`
  - `LLM_INTENT_ENABLE` is on
  - `LLM_DIALOGUE_MODE == chat`
- If that gate fails, use the existing branch path unchanged.
- If the chat path fails hard during a conversation turn, drop back to the old branch flow for that turn and continue safely.

Engineering note:
- The fastest safe design is not "chat forever no matter what."
- The fastest safe design is "chat when healthy, branch fallback when broken."

## 4. Item 1.3 - Replace visible response selection with blank-slate chat input

Todo reference:
- Stage 1 bullet 3

Findings:
- `dialogue::opt()` currently does all of the following in one place:
  - builds the NPC line
  - calls `gen_responses()`
  - renders the transcript
  - builds visible player response lines
  - waits for menu/key selection
- The generated response list lives in:
  - `dialogue::responses` in `src/dialogue.h:302`
  - `response_condition_exists` / `response_condition_eval` in `src/dialogue.h:306-307`
- Response presentation text is built by `talk_response::create_option_line(...)` at `src/npctalk.cpp:3117`.
- The existing transcript window labels live in:
  - `src/dialogue_win.cpp:172`
  - `src/dialogue_win.cpp:174`
  - `src/dialogue_win.cpp:203`
- Existing reusable text input widgets already exist:
  - `string_input_popup_imgui::query()` at `src/input_popup.cpp:335`
  - identifier/history support at `src/input_popup.cpp:226`
  - cancel status at `src/string_input_popup.h:263`

Recommendation:
- For Stage 1 speed, use `string_input_popup_imgui` for player chat entry instead of building a brand-new inline typing widget inside the dialogue window.
- Keep the existing transcript/history window.
- Replace the visible response-choice step with:
  - show transcript/history
  - ask for freeform player line through the popup input
- Only add chat-specific window labels if they are cheap.
- If label changes require broader churn than expected, defer them; they are not core.

Engineering note:
- Popup-based input is much faster than a bespoke inline chat-entry UI.
- It still gives the right interaction shape for a first milestone.

## 5. Item 1.4 - Add an LLM-generated opening beat when conversation starts

Todo reference:
- Stage 1 bullet 4

Findings:
- `dialogue::opt()` already constructs the first NPC-side line each turn:
  - `dynamic_line(...)` is called inside `dialogue::opt()` at `src/npctalk.cpp:3232`
  - `gen_responses(...)` follows immediately at `src/npctalk.cpp:3233`
- The current authored challenge line is then pushed into history before player choice.
- This is already the right seam for a chat opener.

Recommendation:
- On the first chat-mode turn only, call a synchronous LLM opener path before asking for player input.
- Feed it:
  - live snapshot
  - current authored topic line
  - voice/story context available in Stage 1
- Do not allow the opener to perform a hidden backend action.
- If the opener fails, use the current authored challenge line as the fallback opener and continue.

Engineering note:
- This keeps the opener atmospheric instead of stateful.
- That is the fastest stable version.

## 6. Item 1.5 - Generate one freeform NPC reply per player line

Todo reference:
- Stage 1 bullet 5

Findings:
- The current LLM runner transport is already implemented in `src/llm_intent.cpp`.
- Existing prompt/request/response seams include:
  - prompt filename constants at `src/llm_intent.cpp:167-170`
  - action prompt template at `src/llm_intent.cpp:2343`
  - prompt assembly at `src/llm_intent.cpp:2397`
  - request serialization at `src/llm_intent.cpp:2445`
  - response JSON parse at `src/llm_intent.cpp:2489`
- The current manager is asynchronous:
  - `class llm_intent_manager` at `src/llm_intent.cpp:3270`
  - `handle_request(...)` at `src/llm_intent.cpp:4049`
- The runner process code already supports direct request/response transport:
  - Windows `ensure_running` / `send_request` at `src/llm_intent.cpp:2632` / `2640`
  - POSIX `ensure_running` / `send_request` at `src/llm_intent.cpp:2949` / `2957`

Recommendation:
- Do not route chat turns through the existing async queue.
- Add a synchronous chat helper in `src/llm_intent.cpp` that reuses the existing runner/process transport.
- Keep the response contract narrow and explicit.
- Recommended Stage 1 response shape:
  - `say`
  - optional `tool`
- Prefer JSON output for chat mode, not CSV action tokens.

Existing writing footing we can reuse now:
- The current summary system already exists in `src/llm_intent.cpp`:
  - background summary cache starts at `src/llm_intent.cpp:376`
  - `Summaries_short` / `Summaries_extra` load at `src/llm_intent.cpp:732-737`
- Stage 1 should reuse that existing summary footing.
- Do not build a new story-card generation pipeline before the first checkpoint.

Engineering note:
- A synchronous helper is the most direct way to make chat feel immediate inside the talk UI.

## 7. Item 1.6 - Optionally trigger one real current legal dialogue action per turn

Todo reference:
- Stage 1 bullet 6

Findings:
- The current generated response list after `gen_responses()` is already the exact legal current dialogue surface.
- Relevant seams:
  - `dialogue::gen_responses(...)` at `src/npctalk.cpp:2278`
  - `dialogue::add_gen_response(...)` at `src/npctalk.cpp:2938`
  - `talk_response::create_option_line(...)` at `src/npctalk.cpp:3117`
- `create_option_line(...)` already resolves:
  - visible response text
  - skill/trial formatting
  - show-reason text for disabled/greyed options
  - tag parsing
- `response_condition_exists` / `response_condition_eval` tell us which generated responses are actually selectable.

Recommendation:
- For Stage 1, build hidden dialogue tools from the generated current response list, not from raw talk JSON.
- Represent each tool as:
  - stable tool id
  - cleaned response text or affordance label
  - optional unavailable reason for blocked tools
- Pass selectable responses as available tools.
- Optionally pass blocked responses with reasons as unavailable tools if helpful for model guidance.
- Keep the model to at most one hidden action per player turn.
- If the model chooses no valid tool:
  - keep the freeform reply
  - perform no side-effect
  - remain in the current conversation state

Engineering note:
- This is the most important anti-fake-mechanics seam.
- It preserves real trade/quest/training/follower behavior without exposing raw branch menus.

## 8. Item 1.7 - Add a dedicated chat prompt file in the existing prompt-file style

Todo reference:
- Stage 1 bullet 7

Findings:
- Prompt-file loading is already standardized in:
  - `src/llm_prompt_templates.h:11-14`
  - `src/llm_prompt_templates.cpp:11-20`
  - `src/llm_prompt_templates.cpp:84-91`
- Bundled prompt files live in `data/llm_prompts/`.
- Local overrides are seeded and loaded from the config-side prompt directory automatically.
- Existing bundled filenames include:
  - `npc_action_prompt.txt`
  - `npc_ambient_prompt.txt`
  - `look_around_prompt.txt`
  - `look_inventory_prompt.txt`

Recommendation:
- Follow the existing prompt-file pattern exactly.
- Add one dedicated chat prompt file for Stage 1.
- A separate opener prompt file is optional, not required.
- Good Stage 1 filename candidates:
  - `npc_dialogue_chat_prompt.txt`
  - optional later `npc_dialogue_chat_opening_prompt.txt`

Engineering note:
- This is low risk and should be done early because it keeps the prompt reviewable from day one.

## 9. Item 1.8 - Add a dedicated chat log file separate from `config/llm_intent.log`

Todo reference:
- Stage 1 bullet 8

Findings:
- Current log plumbing already writes to the shared config directory:
  - log filename constant at `src/llm_intent.cpp:98`
  - config dir helper at `src/llm_intent.cpp:101`
  - append helper at `src/llm_intent.cpp:111`
  - public wrapper `llm_intent::log_event(...)` at `src/llm_intent.cpp:4157`
- The current LLM pipeline already logs prompt/response blocks heavily through `append_llm_intent_log(...)`.

Recommendation:
- Add a sibling helper for a separate chat log file rather than mixing Stage 1 chat traces into `llm_intent.log`.
- Recommended filename:
  - `config/llm_dialogue_chat.log`
- Minimum Stage 1 log payload per turn:
  - raw assembled chat context packet
  - raw prompt text or prompt sections used
  - raw model output
  - parsed `say`
  - parsed tool selection if any
  - fallback reason if the turn drops to branch mode

Engineering note:
- For hackathon speed, the simplest useful policy is to log chat turns automatically whenever chat mode is used.
- If log noise becomes intolerable later, gating can be added afterward.

## 10. Item 1.9 - Reach one coherent checkpoint before compiling

Todo reference:
- Stage 1 bullet 9

Findings:
- The likely first-pass implementation surface is:
  - `src/options.cpp`
  - `src/npctalk.cpp`
  - `src/llm_intent.cpp`
  - maybe `src/llm_intent.h`
  - new prompt file(s) in `data/llm_prompts/`
- The compile surface grows a lot if we also touch:
  - `src/dialogue.h`
  - `src/dialogue_win.h`

Recommendation:
- Prefer local helper functions inside `.cpp` files wherever possible.
- Avoid broad header churn in Stage 1 unless it is truly necessary.
- In particular:
  - prefer keeping chat-turn logic inside `src/npctalk.cpp`
  - prefer keeping synchronous chat-runner logic inside `src/llm_intent.cpp`
  - avoid expanding public interfaces unless needed
- Do not compile until the first end-to-end vertical slice exists:
  - toggle
  - opener
  - freeform reply
  - optional hidden tool
  - fallback
  - prompt file
  - chat log

Expected narrow compile targets after that checkpoint, if the touched files stay narrow:
- Windows/MSYS2:
  - `objwin/tiles/options.o`
  - `objwin/tiles/npctalk.o`
  - `objwin/tiles/llm_intent.o`
  - plus `objwin/tiles/dialogue_win.o` only if that file gets touched
- Linux/WSL:
  - `obj/tiles/options.o`
  - `obj/tiles/npctalk.o`
  - `obj/tiles/llm_intent.o`
  - plus `obj/tiles/dialogue_win.o` only if that file gets touched

Engineering note:
- The first checkpoint should optimize for "real path exists" before "UI is elegant."

## 11. Recommended Stage 1 Build Order

1. Add the toggle.
2. Add the dedicated chat prompt file.
3. Add the dedicated chat log helper.
4. Add the synchronous chat request helper in `llm_intent`.
5. Add the chat-mode gate in the normal dialogue entry path.
6. Add the opener.
7. Replace visible selection with freeform input.
8. Map current legal responses to hidden tools.
9. Add hard fallback behavior.
10. Only then do the first compile checkpoint.

## 12. Resolved Stage 1 Decisions From Review

These were open during recon and are now settled for implementation:

### 12.1 Input style
- Popup-style text entry is acceptable for Stage 1.
- A truer inline or streaming "someone is talking to you" feel is desirable, but should be treated as Stage 2 unless it is unexpectedly cheap.

### 12.2 Chat logging policy
- The dedicated chat log should be always-on while chat mode is used during the hackathon.
- This is the practical default because fast debugging matters more than log neatness in the event window.

## 13. Default Assumptions For Stage 1 Coding

### 13.1 Input
- Use popup-style text entry for Stage 1.
- Keep the existing dialogue transcript window rather than building a new inline editor.

### 13.2 Logging
- Keep the dedicated chat log always-on while chat mode is active.
- Do not add a separate Stage 1 debug toggle unless it is unexpectedly cheap.

### 13.3 Prompt split
- Start with one main chat prompt file.
- Put opener guidance in that file first.
- Only split the opener into its own prompt file if the prompt gets muddled.

### 13.4 Writing injection
- Reuse the existing short summary/runtime snapshot footing now.
- Do not generate new medium voice/story cards before the first compile checkpoint.
- If the writing feels too generic later, add selective demo-NPC cards in Stage 2.

### 13.5 Hidden action surface
- Build hidden tools only from the current generated legal response list.
- Do not do raw JSON traversal or future-topic retrieval in Stage 1.

### 13.6 Demo priorities
- Prioritize identity, trade, and work/quest asks.
- Keep rumors/background as a softer secondary success case.
- Do not spend Stage 1 time on follower control.
