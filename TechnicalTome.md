# Technical Tome

## Background Summarizer (tools/llm_runner/background_summarizer.py)
The summarizer now has **two modes**:

1. **Background topic mode**
   - pre-generates short summaries for trait/background talk topics in
     `data/json/npcs/Backgrounds/*.json`
   - writes one pipe-delimited line per topic into
     `data/json/npcs/Backgrounds/Summaries_short/<source>_summary_short.txt`
2. **Named NPC registry mode**
   - reads a declarative registry from
     `data/json/npcs/Backgrounds/summary_registry.json` (or another file passed
     via `--registry`)
   - generates summaries for `tier: auto` named NPCs by reading the listed
     `source_files`
   - writes selector-based output into
     `data/json/npcs/Backgrounds/Summaries_extra/generated_named_npcs.json`

### What the background-topic mode does
- Builds a trait-to-topic map by reading
  `data/json/npcs/Backgrounds/backgrounds_table_of_contents.json` and extracting
  `npc_has_trait` conditions (including nested `and`/`or` blocks).
- Indexes all `talk_topic` entries under `data/json/npcs/Backgrounds/`, collecting
  `dynamic_line` text and (optionally) response `text`.
- Sends the story text to a local LLM with a strict prompt requesting five
  descriptors and one notable quote.
- Normalizes and validates the output, then writes:
  `topic_id|your_background|your_expression|source_tag`.

### What the named-NPC registry mode does
- Loads a registry object with:
  - `path_root`
  - `generated_output`
  - `entries[]`
- Each registry entry carries:
  - `id`
  - `tier` (`manual` or `auto`)
  - `selectors[]` (`name:...`, `topic:...`, etc.)
  - `source_files[]` for `auto`
- Aggregates `talk_topic` lines from the listed `source_files`.
- Runs the same summarizer prompt over that aggregated text.
- Writes either legacy pipe-delimited `.txt` entries or the preferred JSON bundle
  format depending on the target file extension in `generated_output`.
- Uses provenance tags in the form:
  `local:<backend>:<model>:registry:<entry_id>`

### Runtime summary JSON schema
Phase 2 introduces a proper runtime summary-entry JSON format. The loader accepts:
- a single `npc_personality_summary` object
- an array of summary objects
- or a bundle object with `type: "npc_personality_summary_bundle"` and `entries[]`

Each summary entry may provide:
- `selector` or `selectors` for named-NPC matching
- `topic` or `topics` for background-topic matching
- `your_background` / `background`
- `your_expression` / `expression`
- `source_tag`

This means a mod can now ship one JSON bundle containing both named overrides and
trait/topic summaries instead of relying entirely on line-based text files.

### Tier intent
- **Tier 1 / manual**
  - important NPCs with authorial voice
  - can live in hand-maintained `Summaries_extra/*.json` files (preferred) or legacy `.txt` files
  - can still be listed in the registry as documentation / triage
- **Tier 2 / auto**
  - named NPCs good enough for generated summaries
  - live in `generated_named_npcs.json` by default now
  - can be regenerated from the registry

### Runtime loading and override rules
Runtime summary loading in `src/llm_intent.cpp` now:
- reloads when the active summary roots change (core data vs active mods)
- merges core + active mod + world custom-mod roots
- loads both `.json` and legacy `.txt` summary files
- prefers the newer JSON format when the same stem exists in both formats
- still loads `generated_*` files before later manual files in each summary folder
- therefore allows manual files to override generated selectors cleanly
- accepts JSON summary entries addressed by `selector` / `selectors` and/or `topic` / `topics`
- also loads `Summaries_short` / `Summaries_extra` from active mods so modded
  summary packs participate in the same mechanism

### Checks and safeguards
- Background-topic mode skips topics that already exist unless `--force` is used.
- Registry mode can dry-run with `--dry-run` to validate source paths and count
  extracted NPC lines without calling the model.
- `--only-entry` lets you regenerate one named NPC at a time.
- If the model fails to load or output cannot be validated, the script logs and
  skips without failing the whole pass.
- It retries invalid output via `--retry-invalid`.
- It sanitizes and extracts output even when the model returns `<think>` content.

### Build integration
The Makefile defines a convenience target:
- `llm-bg-summary-short` writes into `data/json/npcs/Backgrounds/Summaries_short`.
- Normal builds skip summary generation unless explicitly enabled.
- `./just_build_macos.sh --with-summary --summary-model-dir /path/to/local/model`
  enables local generation and now fails fast if the model dir is missing.

Configuration knobs:
- `LLM_SUMMARY_BACKEND` (env) or `--backend` / `--summary-backend` (`ollama` or `openvino`).
- `LLM_SUMMARY_OLLAMA_URL` + `LLM_SUMMARY_OLLAMA_MODEL` for local Ollama summaries.
- `LLM_SUMMARY_MODEL_DIR` (env) or `--model-dir` / `--summary-model-dir` for the OpenVINO model.
- `LLM_SUMMARY_DEVICE` (env) or `--device` / `--summary-device` for target device (default "NPU").
- On macOS builds, `just_build_macos.sh --with-summary` also reads the branch profile's `config/options.json` (LLM menu settings) so the in-game summary backend/model options can drive future summary builds.

### Useful commands
Dry-run named-NPC generation:
- `python3 tools/llm_runner/background_summarizer.py --registry data/json/npcs/Backgrounds/summary_registry.json --registry-tier auto --dry-run`

Generate a single named NPC:
- `python3 tools/llm_runner/background_summarizer.py --registry data/json/npcs/Backgrounds/summary_registry.json --registry-tier auto --only-entry reena_sandhu --backend ollama --ollama-model mistral`

Regenerate the auto tier:
- `python3 tools/llm_runner/background_summarizer.py --registry data/json/npcs/Backgrounds/summary_registry.json --registry-tier auto --backend ollama --ollama-model mistral --include-responses --retry-invalid 2`

Run the named-NPC smoke harness without invoking the model:
- `python3 tools/llm_runner/npc_harness.py --scenario tools/llm_runner/scenarios/rubik_trade.json --resolve-only --json`

Run the named-NPC smoke harness through the normal runner pipe:
- `python3 tools/llm_runner/npc_harness.py --scenario tools/llm_runner/scenarios/rubik_trade.json --backend ollama --ollama-model mistral`

The smoke harness intentionally tests three layers together:
- selector resolution (manual vs generated precedence)
- snapshot/prompt assembly for one named NPC
- runner I/O + response parsing using game-like pipe-separated action-line validation

## LLM Intent Actions (Behavior Notes)
- `look_around` requests up to three nearby item names for pickup targeting.
- `look_inventory` supports `wear`, `wield`, `act`, and `drop` sections.
- `panic_on` sets a forced flee state for about 20 turns and bumps panic while active.
- `panic_off` clears the flee effect and linearly caps panic over ~30 turns.
- Follow control now uses explicit action tokens: `follow_close` and `follow_far`.
- Snapshot now includes `your_follow_mode` (`follow-close`, `follow-afar`, `guard/hold`)
  so prompt logic can prefer `idle` instead of repeating an already-matching mode.
- The ASCII snapshot creature legend now includes a normalized perceived threat level
  (`threat=x/10`) next to each visible creature name so the model can see when a
  target is present but feels risky to engage.
- Snapshot memory has two capped blocks per NPC:
  - `recent_conversation` (last two direct player->NPC interactions)
  - `overheard_allies` (last two nearby ally speech/action events with `npc_name`)
- Multi-hearer shouts are now serialized: when several allies hear one player utterance,
  LLM requests are dispatched one at a time so later NPC snapshots can include earlier
  NPC responses from the same shout cycle.
- Optional random calls are driven by `LLM_INTENT_RANDOM_CALL` (0-500 turns):
  each ally NPC keeps an independent jittered timer (`base +/- base/6`) and can
  trigger a spontaneous LLM request with no player utterance.

## March 2026 Port Refresh (Completed)
- All four `port/*` branches passed in-game validation after the late-March port sweep.
- The main action prompt regained `/no_think` at the intended location (end of the main `<System>` block) across the active port branches.
- `look_around` targeted pickup was stabilized across ports:
  - `0.I` received follow-handoff hardening so item pickup can temporarily override queued `follow_close` / `follow_far` actions.
  - `0.H` received a branch-native backport of exact targeted pickup plus the same follow-handoff protection.
  - All `port/*` branches now allow targeted pickup of wieldable items instead of only stashable/wearable ones.
- Speech/control additions were ported across the active release branches:
  - follower bark when just outside normal hearing range,
  - `whisper` / `say` / `yell` sentence variants,
  - lowercase hotkeys for the sentence variants,
  - yelled bark-back with all-caps bark text,
  - UI debug messages no longer appear when only log-debug is enabled.
- `0.H` now uses real profession fallback for `your_profession` instead of defaulting to `no_past` whenever `custom_profession` is empty.

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

## Release Build Helpers (Current Defaults)
- `just_build.cmd` and `build_and_run.cmd` now skip background summary generation by default to keep matrix validation fast.
  - Use `--with-summary` when intentionally regenerating summaries.
- `just_build_linux.cmd` and `build_and_run_linux.cmd` now:
  - Skip apt dependency bootstrap by default.
  - Use `--install-deps` to run dependency install/update.
  - Disable `ASTYLE` checks during build-validation runs.
  - Skip background summary generation by default (use `--with-summary` to enable).
- This split is intentional:
  - Build matrix scripts are for compile/smoke validation speed.
  - Summary regeneration is still available as an explicit action.

## Packaging Contract (Port Releases)
- Every `port/*` release bundle must include:
  - `tools/llm_runner/**`
  - `data/json/npcs/Backgrounds/Summaries_short/**` (via `data`)
  - `README.md`, `Plan.md`, `TechnicalTome.md`, `Agents.md`
- Port branches should carry CAOL branding in README/title assets with a short compatibility disclaimer.

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

## Hierarchical LLM Control (Planned, Compute-Lean)
- Design goal: keep high strategic agency while minimizing LLM calls by using deterministic execution for most behavior.

### Control hierarchy
- `Faction level` (rare LLM calls):
  - Sets doctrine and regional objective (`raid`, `evade`, `fortify`, etc.).
- `Squad level` (moderate LLM calls):
  - Preferred batching: one call controls a micro-squad of 2-3 bandits.
  - Produces short movement intent only.
- `Actor level` (deterministic, frequent):
  - Existing AI executes pathing, combat, inventory, and local reactions.
  - LLM is only escalated on context gates.

### Overmap movement contract
- Planned minimal parser contract for overmap planner output:
  - `Stay`
  - `Move <dir>`
  - `Move <dir> <dir>`
  - `Move <dir> <dir> <dir>`
- Allowed directions: `N NE E SE S SW W NW`.
- Any malformed output resolves to `Stay` (no side effects).

### Context-gated trigger model
- Call LLM only when state delta is meaningful, e.g.:
  - threat spike, objective change, squad split, or complex player intent.
- Skip calls when deterministic policy is still valid and world delta is below threshold.
- Keep cooldown + jitter + global cap to prevent request storms.

### Deterministic safety and fallback
- Planner output is intent-only; movement motor remains deterministic.
- If route/path intent fails:
  - try deterministic repath once,
  - then clear intent and continue with local deterministic AI.
- If planner is unavailable or parse fails:
  - keep gameplay stable by using non-LLM defaults.
