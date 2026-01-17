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

### Debug run example
Run a single topic with retries and verbose IO (use the OpenVINO venv Python):
```powershell
C:\Users\josef\openvino_models\openvino_env\Scripts\python.exe tools\llm_runner\background_summarizer.py --only-topic BGSS_CODGER_STORY1 --force --retry-invalid 2 --debug-io --include-responses
```
*** End Patch"}googassistant to=functions.apply_patch mg${json
