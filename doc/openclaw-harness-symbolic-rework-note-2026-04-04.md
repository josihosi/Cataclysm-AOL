# OpenClaw harness symbolic rework note (2026-04-04)

This is a parked design note saved as auxiliary context for `Plan.md`.
Source: Josef passed along a ChatGPT Pro reasoning note during Discord discussion on 2026-04-04.

Status: **parked reference, not active queue**.
Use this when/if the harness gets revisited after the current finish lines.

## Extra local lesson from 2026-04-04 testing

A real harness rework should explicitly help with this failure mode too:
- live in-game behavior can work even when the expected reply is not visible in the logs being checked
- deterministic tests, live probes, and logging visibility are different evidence classes and should not be conflated
- changing probe method mid-investigation (GUI live probe vs terminal/PTTY probe vs test snapshot) can invalidate conclusions if the harness does not make the layer obvious
- future harness work should make it easy to answer three separate questions cleanly:
  1. did the feature work on-screen?
  2. did the deterministic contract/test pass?
  3. where was the artifact/log for that specific path written?

This strengthens the case for explicit UI-state hooks, action-result contracts, and better artifact routing instead of relying on ad-hoc screen reading or assuming all reply paths land in the same log.

## Raw note

Here is the design note I would hand to OpenClaw.

Do not build a screenshot-first harness for c-aol. Public game-playing demos often use pixels because the emulator is a black box. AOL is already the opposite. The fork already has a symbolic interface: the game talks to tools/llm_runner/runner.py, the current npc_harness.py is explicitly a smoke harness for selector resolution, snapshot/prompt assembly, and runner I/O, and the snapshot already carries compact structured state such as inventory, follow mode, short conversation memory, an ASCII map, and a creature legend with threat info. So the missing piece is not perception. It is reliable control-state and observability. Because ASCII is already symbolic, do not demote it back into pixels.

The repo plan already points to the right order. It says to build an action-status / failure-reason layer before a larger automation harness, and to make the first harness deterministic, branch-aware, save/script driven, and explicit about menu differences. That is exactly right. A harness that only replays keys will just make the current “menus opened, something weird happened” problem faster. A harness that can say “blocked because no path,” “blocked because item not visible,” or “succeeded and equipped melee weapon” becomes useful immediately.

The public Pokémon work points the same way. Anthropic describes Claude Plays Pokémon as a loop with pixel input, button/function tools, and persistent memory, and their public starter repo includes emulator control plus memory reading. A separate FireRed project hit almost exactly your current pain: programmatic input was unreliable, it fell back to keyboard automation and OCR, and that became a major limitation. The broader LLM game-agent literature also keeps converging on the same three parts: perception/action interface, memory, and reasoning. For AOL, you own the game code, so there is no reason to accept black-box emulator constraints when you can expose state directly.

I would also avoid treating the current NPC pipeline as if it were already a full player-agent. Right now the in-game LLM path is centered on the NPC intent runner and player sentence input, with speech variants and a constrained single-line action format. That is a very good pattern to reuse, but it is still NPC-intent plumbing, not a general autonomous player-control layer. So the right move is to transpose the architecture, not the exact surface.

What I would build is a three-layer harness.

Layer 1 is the thing you already have: keep npc_harness.py as the fast contract test. That should stay the place where snapshot changes, prompt changes, parser changes, and selector/summary changes get regression-tested in seconds, without launching the whole game. Extend it with a golden corpus of scenarios and expected parsed action lines.

Layer 2 is the real missing piece: a tiny in-engine driver API. Keep it boring. JSON over stdin/stdout, a named pipe, or a localhost socket is enough. The key calls should be things like get_ui_state, load_profile, load_save, advance_turns, send_player_sentence, dump_npc_snapshot, get_recent_events, spawn_debug, and get_last_action_result. The one call that matters most is get_ui_state(): it should tell the harness whether the game is in main_menu, load_screen, map_play, npc_dialogue, inventory, pickup, debug_menu, popup, or death_screen, and also expose menu title, visible options, selected index, hotkeys, and modal stack. Store startup profiles as data, not hardcoded Python branches. Once every step is gated by explicit state, “it keeps opening menus” becomes a normal finite-state-machine bug, not an LLM problem. This also matches your plan for deterministic startup profiles, prepared saves, and artifact capture.

Longer-term, expose scenario setup directly instead of driving the debug menu with keys. spawn_item, spawn_npc, spawn_monster, teleport, set_time, set_weather, and similar hooks are far less brittle than menu automation. Your own plan already treats debug-menu scenario setup as a later harness phase; direct hooks are the cleaner version of that idea.

Layer 3 is optional and thin: desktop automation or screenshots only for bootstrapping and debugging. I would not make that the primary control loop. For ASCII mode, a render-buffer dump is much better than a screenshot anyway: dump the character grid plus attributes, not pixels. That removes zoom/OCR issues and preserves the semantic structure of the UI.

The second core addition is the action result contract. I would make every semantic action produce a structured result like queued, in_progress, succeeded, blocked, failed, or partial, plus a reason_code, a target, optional spoken_feedback, and a few debug facts. Your Plan already singles out look_around, look_inventory, and attack=<target> as the first places to do this. That is exactly right. Once those results exist, the harness can assert on outcomes instead of guessing from screen text.

If the longer-term goal is “have GPT playtest as the player,” give it a typed player action vocabulary, not raw keys. Use actions like move N, open E, pickup "wood axe", talk "Rubik", say normal "...", equip "glock", wait 5, use_item "bandage" on self. Then let a deterministic executor map those to exact key sequences and retries. That follows the same idea already in AOL’s constrained NPC action format, and it matches the TechnicalTome’s compute-lean hierarchy: use the model for intent, use deterministic code for high-frequency local execution.

For autonomous exploratory playtesting, add a harness-side notebook instead of bloating the raw snapshot forever. Anthropic’s own agent notes on Pokémon make the case that persistent notes are what preserve coherence over thousands of steps. AOL already has the beginnings of that idea in background summaries plus short recent-conversation and overheard-memory blocks. So keep the in-engine snapshot compact, and let the harness maintain a separate run notebook with goals, failed attempts, map discoveries, and scenario-specific assertions.

My answer to the repo’s “inside the repo vs external glue” question is: tiny C++ shim inside, most orchestration outside. The engine should answer “where am I?” and “what just happened?” The external harness should own branch profiles, scenarios, artifacts, logs, and LLM adapters. That is the cleanest split.

On transposing this to upstream CDDA: yes, but only the thin generic pieces. AOL is a fork of CDDA, and your current release model already relies on carrying curated AOL changes across different upstream targets. The parts that transpose cleanly are generic developer hooks: ui_state, action_result, scripted load/save/startup profiles, and direct debug/setup commands. The AOL-specific parts—tools/llm_runner, prompt templates, background-summary generation, NPC-flavored snapshot content—should stay fork-local unless upstream explicitly wants them. That split keeps the port surface smaller and makes future syncing much less painful.

A minimal first slice could look like this:

load_profile("master")
load_save("rubik_smoke")
get_ui_state()              -> {"screen":"map_play", ...}
send_player_sentence("Pick up that axe, fast.", mode="say")
advance_turns(1)
get_last_action_result("Willy Norwood")
dump_npc_snapshot("Willy Norwood")

The first four implementation steps I would pick are these. Add get_ui_state() inside the game. Add action_result plus reason codes for look_around, look_inventory, and attack. Add one data-driven startup profile that reaches map_play from the main menu without blind sleeps. Then add two or three real smoke scenarios that assert on structured result objects and capture artifacts on failure. That is the shortest path from “interesting LLM prototype” to “useful automated playtest harness,” and it matches the implementation order already sketched in your Plan.
