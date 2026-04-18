# TESTING

Current validation policy and current evidence only.

This file is not a trophy wall.
Remove stale or completed fluff instead of stacking crossed-off test history forever.

## Validation policy

Use the **smallest evidence that honestly matches the change**.

- **Docs-only change** -> no compile
- **Small local code change** -> narrow build and the narrowest relevant test target
- **Broad or risky code change** -> broader rebuild and relevant filtered tests
- **Before a Josef handoff** or after suspicious stale-binary behavior -> rebuild the real targets and rerun the real smoke path

### Agent vs Josef

Schani should do agent-side playtesting first whenever the harness or direct in-game probing can answer the question.
Josef should be asked only for:
- product judgment
- tone/feel
- human-priority choices
- genuinely human-only interaction

Josef being unavailable is **not** a blocker by itself.
Josef self-testing is **not** a plan blocker and does not belong in the active success state as a gate.
If Josef-specific checks are useful, write them down as non-blocking notes so he can run them later from his own list.
If another good agent-side lane exists, keep moving there.
If several human-only judgments are likely soon, batch them instead of sending tiny separate asks.

### Anti-churn rule

Do not keep rerunning the same startup or test packet when it is no longer the missing evidence class.
If startup/load is already green, and the missing proof is live behavior, then the next probe must target live behavior.
If a target is merely waiting on Josef, do not keep revalidating it unless the code changed.

---

## Current relevant evidence

### Hackathon chat dialogue Stage 1

Current honest state:
- Stage 1 implementation is now in progress on the hackathon branch.
- The active code slice currently targets:
  - `[LLM]` toggle
  - response-area chat input
  - LLM opener
  - freeform reply
  - one hidden dialogue action
  - dedicated prompt file
  - dedicated chat log
  - safe fallback seam
- A narrow WSL object compile passed after the first full Stage 1 vertical slice landed:
  - command:
    `make -j8 TILES=1 SOUND=1 RELEASE=1 LOCALIZE=1 LANGUAGES=all LINTJSON=0 ASTYLE=0 TESTS=0 obj/tiles/options.o obj/tiles/npctalk.o obj/tiles/dialogue_win.o obj/tiles/llm_intent.o`
  - log:
    `build_logs/hackathon_chat_stage1_narrow_20260418.log`
- The first narrow compile initially failed on local `npctalk.cpp` seam mistakes:
  - private `dialogue` helper access
  - missing local whitespace-trim check
  - private quit-helper access
- Those local issues were fixed and the rerun passed.
- First real user playtest on `2026-04-18` was a functional success:
  - game started
  - chat mode worked
  - the overall feel was reported as highly atmospheric
- The first real playtest also surfaced concrete Stage 1 follow-up issues:
  - `?` input behavior in chat entry
  - chat UI overlap when trade opens
  - chat entry should move under `Your response:`
  - repeated signature-line phrasing in replies
  - fake-seeming work or quest wording without a real quest-log action
- Current log read on the work or quest issue:
  - `config/llm_dialogue_chat.log` showed no real mission-offer or mission-accept hidden action on the relevant turn
  - the model replied with freeform text and `tool=""`
  - so the immediate fix is prompt or routing honesty, not a broken quest-tool execution on that exact turn
- Additional run note from Josef:
  - `build_and_run.cmd` later reported `/usr/bin/bash: line 1: 434 Segmentation fault ./$RUN_EX`
  - current `config/debug.log` does not yet pin this to startup, in-chat runtime, or exit
- A follow-up narrow fix pass compiled cleanly on `2026-04-18` after landing:
  - response-area chat input in `dialogue_window`
  - prompt hardening for repeated lines and fake work or quest wording
  - hidden-tool hinting in the chat packet
  - chat-memory dedupe for near-identical NPC replies
  - opener reset on real topic changes triggered by hidden actions
  - command:
    `make -j8 TILES=1 SOUND=1 RELEASE=1 LOCALIZE=1 LANGUAGES=all LINTJSON=0 ASTYLE=0 TESTS=0 obj/tiles/dialogue_win.o obj/tiles/npctalk.o obj/tiles/llm_intent.o`
  - log:
    `build_logs/hackathon_chat_stage1_fixpass_narrow_20260418.log`
- A second follow-up narrow compile also passed on `2026-04-18` after landing:
  - separate `sandbox actions` plus `branch actions`
  - compact relationship-memory injection
  - fresh-conversation short-memory reset
  - transcript color preservation for chat history
  - updated readable chat prompt/log sections for sandbox versus branch actions
  - command:
    `make -j8 TILES=1 SOUND=1 RELEASE=1 LOCALIZE=1 LANGUAGES=all LINTJSON=0 ASTYLE=0 TESTS=0 obj/tiles/dialogue_win.o obj/tiles/llm_intent.o obj/tiles/npc.o obj/tiles/npctalk.o`
  - log:
    `build_logs/hackathon_chat_sandbox_memory_colors_narrow_20260418.log`
- A third narrow compile also passed on `2026-04-18` after landing the opener split:
  - opening turns now use opener-only context
  - opening turns no longer receive the current authored branch line
  - opening turns no longer receive branch actions
  - chat logging now distinguishes opening-prompt versus reply-prompt setup
  - command:
    `make -j8 TILES=1 SOUND=1 RELEASE=1 LOCALIZE=1 LANGUAGES=all LINTJSON=0 ASTYLE=0 TESTS=0 obj/tiles/llm_intent.o`
  - log:
    `build_logs/hackathon_chat_opener_split_narrow_20260418.log`
- A fourth narrow compile also passed on `2026-04-18` after landing the first API-only chat streaming attempt:
  - dialogue chat requests can now emit partial API events
  - the chat UI progressively redraws streamed NPC `say` text
  - non-streaming paths remain the fallback for non-API backends
  - command:
    `make -j8 TILES=1 SOUND=1 RELEASE=1 LOCALIZE=1 LANGUAGES=all LINTJSON=0 ASTYLE=0 TESTS=0 obj/tiles/dialogue_win.o obj/tiles/llm_intent.o obj/tiles/npctalk.o`
  - log:
    `build_logs/hackathon_chat_streaming_narrow_20260418.log`
- Deterministic runner probing on `2026-04-18` showed the API transport itself is fine:
  - the OpenAI runner emitted real partial events for both long and short requests
  - a short guarded reply streamed in about `1.1s`
  - a longer atmospheric reply streamed in about `1.56s`
  - the failure mode was therefore not "no streaming", but the brittle JSON-shaped stream contract
- The active streaming rework now switches chat-mode output to a pipe-delimited one-line contract:
  - `say text | tool_id`
  - opening turns use `say text |`
  - the goal is to let the UI stream visible NPC speech directly and stop cleanly at the delimiter

### Smart Zone Manager v1

Current honest state:
- the one-off Smart Zone Manager v1 code remains real in the tree, not aux-doc theater
- the Basecamp inventory-zone prompt path still exists in both honest UI seams:
  - initial `CAMP_STORAGE` placement in Zone Manager
  - later position/stretch edits of that same Basecamp zone
- the deterministic proof still covers the active honest packet in `tests/clzones_test.cpp`, including:
  - expected stamped layout on a representative indoor basecamp fixture
  - outdoor rotten placement beyond a simple wall ring
  - too-small-zone failure
  - non-destructive refusal when a required bed tile is already occupied by a non-basecamp zone
  - repeatability on the same layout
- fresh current-head rebuild on 2026-04-09 succeeded:
  - `make -j4 tests` -> `build_logs/freeze_prep_tests_20260409.log`
  - `make -j4 TILES=1 cataclysm-tiles` -> `build_logs/freeze_prep_tiles_20260409.log`
- fresh deterministic recheck on rebuilt current HEAD passed:
  - `./tests/cata_test "[zones][smart_zone][basecamp]"`
  - log: `build_logs/freeze_prep_smart_zone_tests_20260409.log`
  - result: `All tests passed (2825 assertions in 4 test cases)`
- the too-small-zone failure path is therefore **implemented and presently re-verified**; it is not a hand-wavy TODO
- fresh proportional live proof now exists on rebuilt current HEAD `fce15a6c5a` at `.userdata/dev-harness/harness_runs/20260409_140439/`:
  - prepared-save seam: reinstall the McWilliams fixture into `dev-harness`, move existing `*zones*.json` out of the save, then run `smart_zone.live_probe_mcw_prepped`
  - prompt screenshot: `.userdata/dev-harness/harness_runs/20260409_140439/confirm_zone_end.after.png`
  - saved/reopened layout screenshot: `.userdata/dev-harness/harness_runs/20260409_140439/reopen_zones_manager_for_layout_capture.after.png`
  - screen evidence: the Smart Zone Manager prompt fired on the real UI path, and the saved/reopened Zone Manager list still showed `Basecamp: Storage` plus the expected stamped smart-zone entries
  - artifacts/logs: the harness verdict is `inconclusive_no_new_artifacts`, which is expected here because this proof class is screen/UI evidence rather than `llm_intent.log` output

### Meaning

- Smart Zone Manager v1 still survives a fresh rebuild + live rerun on current HEAD
- the lane is good enough for the current pre-freeze goal and should stay closed unless code/runtime evidence breaks it
- the next action is not more smart-zone archaeology; it is Josef's debug round and then a freeze decision

### Assigned-camp board/log leak recheck

Current honest state:
- the preserved-baseline assigned-camp board/log fix still holds on rebuilt current HEAD
- fresh deterministic recheck passed:
  - `./tests/cata_test "[camp][basecamp_ai]"`
  - log: `build_logs/freeze_prep_basecamp_ai_tests_20260409.log`
  - result: `All tests passed (332 assertions in 1 test case)`
- fresh live assigned-camp probe now exists on rebuilt current HEAD `fce15a6c5a` at `.userdata/dev-harness/harness_runs/20260409_140655/` via `basecamp.package2_assign_camp_toolcall_probe_mcw`
- screen evidence:
  - screenshot: `.userdata/dev-harness/harness_runs/20260409_140655/wait_for_job_followup_reply.after.png`
  - visible in-game replies stay organic on the player-facing message log
  - the old raw leak shape does **not** appear on-screen: no visible `board=show_board`, `details=show_job=1`, or `next=job=1`
  - the player's own typed structured probe text `job=1` still appears as the player utterance, which is expected for this harnessed internal-path check
- artifacts/logs:
  - artifact log: `.userdata/dev-harness/harness_runs/20260409_140655/probe.artifacts.log`
  - the internal structured path still records `board=show_board`, `details=show_job=1`, `approval=waiting_player`, and `next=job=1`, which is the intended observability split
- known weird-but-non-blocking remainder:
  - the same artifact packet still records a nearby Robbie Knox response after the Katharina exchange
  - that is recipient/chatter noise on the live probe, not the old raw player-log leak returning

### Meaning

- the assigned-camp board/log leak fix still survives a fresh rebuilt-current-head live check
- current truth is the intended split: organic player-facing message log, structured internal artifact log
- do not reopen follower/basecamp routing work from this packet unless Josef's final round disproves one of those claims

---

## Pending probes

Hackathon chat dialogue Stage 1:
- review the next `config/llm_dialogue_chat.log` packet as a dedicated trade-refusal debug session
- classify the reported `build_and_run.cmd` segfault as startup, runtime, or exit-time
- verify that `?` input now works in the response-area chat entry
- verify that trade or another tool-backed UI no longer overlaps a floating chat popup
- verify that speaker colors clearly separate player gray from NPC white
- verify that chat-history behavior feels intentional and closes cleanly
- verify that repeated warning lines are reduced in real turns
- verify that work or quest wording stays honest unless a real legal hidden action exists
- verify that trade now routes through `sandbox_trade` when the player clearly asks for it
- verify that the chat log cleanly distinguishes `SANDBOX ACTIONS` from `BRANCH ACTIONS`
- verify that fresh talk start shows compact relationship memory instead of stale prior-turn carry-over
- verify that opener turns no longer log or behave like replies to the hidden authored branch line
- verify that API chat now streams progressively and stops cleanly before the `|` delimiter in the dialogue window
- verify that topic-changing hidden actions now feel readable with the opener reset
- then do the next playtest build

Older frozen packets:
- none right now beyond the preserved evidence below

Do not rerun these packets as ritual.
Only reopen them if:
- code affecting smart zoning or assigned-camp board routing changes
- a reviewer disputes the prepared-save seam or the assigned-camp live probe shape
- runtime evidence disproves one of the current claims

---

## Reusable commands

Use these when they are actually the missing evidence, not as ritual.

### Fresh full test rebuild on this Mac
- `make -j4 tests`

### Fresh tiles relink on this Mac
- `make -j4 TILES=1 cataclysm-tiles`

### Smart-zone deterministic check
- `./tests/cata_test "[zones][smart_zone][basecamp]"`

### Basecamp routing deterministic check
- `./tests/cata_test "[camp][basecamp_ai]"`

### Startup/load smoke for later live proof
- `python3 tools/openclaw_harness/startup_harness.py start --profile dev --world 'Sandy Creek'`

### Current smart-zone live probe
- `python3 tools/openclaw_harness/startup_harness.py install-fixture mcwilliams_live_debug_2026-04-07 --profile dev-harness --fixture-profile live-debug --replace`
- move the installed McWilliams `*zones*.json` files out of `.userdata/dev-harness/save/McWilliams/`
- `python3 tools/openclaw_harness/startup_harness.py probe smart_zone.live_probe_mcw_prepped`

### Current assigned-camp board/log recheck
- `python3 tools/openclaw_harness/startup_harness.py probe basecamp.package2_assign_camp_toolcall_probe_mcw`

## Local build caveat

On this Mac, treat top-level `make -j4 tests` as the reliable path for a fresh `cata_test`.
Avoid treating `make -C tests cata_test` as authoritative here; it has been a repeated source of toolchain/stale-build nonsense.
Also: if you actually need a fresh tiles binary, use `make -j4 TILES=1 cataclysm-tiles`; plain `make cataclysm-tiles` is not an honest rebuild path here.
