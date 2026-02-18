# Technical Tome

## Background Summarizer (tools/llm_runner/background_summarizer.py)
The summarizer pre-generates short NPC background summaries for talk topics in
`data/json/npcs/Backgrounds/*.json` and writes one pipe-delimited line per topic
into `data/json/npcs/Backgrounds/Summaries_short/<source>_summary_short.txt`.

### What it does
- Builds a trait-to-topic map by reading
  `data/json/npcs/Backgrounds/backgrounds_table_of_contents.json` and extracting
  `npc_has_trait` conditions (including nested `and`/`or` blocks).
- Indexes all `talk_topic` entries under `data/json/npcs/Backgrounds/`, collecting
  `dynamic_line` text and (optionally) response `text`.
- Sends the story text to a local OpenVINO LLM with a strict prompt:
  "return two sections separated by ' | '", five comma-separated traits, and
  one notable sentence.
- Normalizes and validates the output, then writes:
  `topic_id|your_background|your_expression|source_tag`.
  The `source_tag` is the JSON filename without a trailing `_#` suffix.

### Checks and safeguards
- Skips topics that already exist in the output file unless `--force` is used.
- If the model fails to load or output cannot be validated, it logs and skips
  without failing the build.
- Tries multiple times when output is invalid (via `--retry-invalid`).
- Sanitizes and extracts output even when the model returns `<think>` content.

### Build integration
The Makefile defines a convenience target:
- `llm-bg-summary-short` runs the script with the project-local Python
  interpreter and writes into `data/json/npcs/Backgrounds/Summaries_short`.
- The target is non-fatal (`|| true`) so missing models or deps do not break
  normal builds.

Configuration knobs:
- `LLM_SUMMARY_MODEL_DIR` (env) or `--model-dir` for the OpenVINO model.
- `LLM_SUMMARY_DEVICE` (env) or `--device` for target device (default "NPU").

## LLM Intent Actions (Behavior Notes)
- `look_around` requests up to three nearby item names for pickup targeting.
- `look_inventory` supports `wear`, `wield`, `act`, and `drop` sections.
- `panic_on` sets a forced flee state for about 20 turns and bumps panic while active.
- `panic_off` clears the flee effect and linearly caps panic over ~30 turns.
- Follow control now uses explicit action tokens: `follow_close` and `follow_far`.
- Snapshot now includes `your_follow_mode` (`follow-close`, `follow-afar`, `guard/hold`)
  so prompt logic can prefer `idle` instead of repeating an already-matching mode.
- Snapshot memory has two capped blocks per NPC:
  - `recent_conversation` (last two direct player->NPC interactions)
  - `overheard_allies` (last two nearby ally speech/action events with `npc_name`)
- Multi-hearer shouts are now serialized: when several allies hear one player utterance,
  LLM requests are dispatched one at a time so later NPC snapshots can include earlier
  NPC responses from the same shout cycle.
- Optional random calls are driven by `LLM_INTENT_RANDOM_CALL` (0-500 turns):
  each ally NPC keeps an independent jittered timer (`base +/- base/6`) and can
  trigger a spontaneous LLM request with no player utterance.

## Porting/Release Strategy (Current Plan)
- Problem statement: full branch merges from AOL `master` into very different upstreams
  (especially CTLG) produce massive recurring conflict sets and are not sustainable
  for periodic releases.
- New model: use clean upstream tips as bases and apply only AOL-relevant commits
  (patchset/cherry-pick queue), instead of merging the entire AOL history.

### Branch/source model
- Development happens on `dev`.
- Before release refresh, merge `dev` into `master`.
- `master` is the AOL source branch for release content.
- For each release target, recreate `port/*` from upstream tip every run:
  - `port/cdda-master` from `upstream/master`
  - `port/cdda-0.H` from `upstream/0.H-branch`
  - `port/cdda-0.I` from `upstream/0.I-branch`
  - `port/ctlg-master` from `upstream-ctlg/master`

### Porting execution model
- Keep upstream branches clean and freshly fetched.
- Apply AOL changes as a curated patchset:
  - common AOL commit queue (shared across targets)
  - optional per-target fixup queue (`0.H`, `0.I`, `ctlg`)
- Run build checks per target after apply/fix:
  - `just_build.cmd --unclean`
  - `just_build_linux.cmd --unclean`

### Automation policy
- Continue orchestrating branch creation, fetch, backup, and logging.
- Prefer automated flow for:
  - `cdda-master`
  - `cdda-0.I`
- Treat these as manual bootstrap targets first:
  - `cdda-0.H` (borderline conflict volume)
  - `ctlg-master` (currently extreme divergence)
- Add safety gates:
  - conflict-threshold abort with "manual required" result
  - CTLG disabled by default unless explicitly requested

### Operational notes
- Orchestrator script: `tools/porting/orchestrate_ports.ps1`
- Porting context: `tools/porting/PORTING_CONTEXT.md`
- Run logs: `tools/porting/logs/<timestamp>/`
- Logs are intentionally untracked by git (`.gitignore`).

### Patchset conflict dry-run usage
- Script: `tools/porting/simulate_patchset.ps1`
- Purpose: simulate patchset/cherry-pick replay in throwaway worktrees and report conflict counts before a real release run.
- Curated patchset run:
  - `.\tools\porting\simulate_patchset.ps1 -Fetch`
- Commit-range estimator run (example):
  - `$targets = @('cdda-master','cdda-0.H','cdda-0.I','ctlg-master')`
  - `$paths = @('src/llm_intent.cpp','src/npc.cpp','src/npc.h','src/npcmove.cpp','tools/llm_runner')`
  - `.\tools\porting\simulate_patchset.ps1 -Fetch -Targets $targets -CommitRange 'upstream/master..master' -PathFilter $paths`
- Patchset files:
  - `tools/porting/patchsets/common.txt`
  - `tools/porting/patchsets/cdda-master.txt`
  - `tools/porting/patchsets/cdda-0.H.txt`
  - `tools/porting/patchsets/cdda-0.I.txt`
  - `tools/porting/patchsets/ctlg-master.txt`

### Debug run example
Run a single topic with retries and verbose IO (use the OpenVINO venv Python):
```powershell
C:\Users\josef\openvino_models\openvino_env\Scripts\python.exe tools\llm_runner\background_summarizer.py --only-topic BGSS_CODGER_STORY1 --force --retry-invalid 2 --debug-io --include-responses
```

## Dialogue Options Architecture (Current)
- Dialogue data is loaded at startup from `type: "talk_topic"` JSON across `data/json/npcs/**` (including `data/json/npcs/Backgrounds/*.json`) into the `json_talk_topics` map in `src/npctalk.cpp` via `load_talk_topic()`.
- A conversation starts in `avatar::talk_to()` (`src/npctalk.cpp`), which creates a `dialogue` with two `talker`s and pulls an initial topic stack from `talker_npc::get_topics()` (`src/talker_npc.cpp`).
- `talker_npc::get_topics()` picks the NPC's `dialogue_chatbin` topics (`first_topic`, `talk_friend`, `talk_leader`, etc. in `src/dialogue_chatbin.h`), then falls back to `npc::pick_talk_topic()` (`src/npctalk.cpp`) which selects a stranger/friend topic based on personality/opinion.
- For each topic, `dialogue::dynamic_line()` calls `json_talk_topic::get_dynamic_line()` and `dialogue::gen_responses()` calls `json_talk_topic::gen_responses()` to build visible player responses from JSON, filtering by dialogue conditions and attaching effects/trials.
- The response's effect sets the next topic; the loop continues until `TALK_DONE`, with `TALK_NONE` popping the stack. This is the central hook if we ever want to intercept or rewrite dialogue selection globally.
