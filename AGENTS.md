Current model direction, 2026-09-23: only GPT-6 Sol, GPT-6 Luna and GPT-6 Astra for new work. Coordinator GPT-6 Sol low; Luna workers preferred and mandatory for live harness playtests. No GPT-5.6 models or Terra. This supersedes historical model guidance below. Existing records remain evidence, not dispatch choices.

# Workspace command and evidence habits

Apply the global contract/deletion test here. Select evidence from the current question; these
query patterns are aids, not an investigation sequence. Retain complete artifacts while returning
only the facts needed for the next decision.

## Harness playtesting model

Only GPT-6 Luna workers may launch or operate live playtest harness sessions. Sol and Astra
workers must delegate that work to a GPT-6 Luna subagent, even when the parent owns a complex
task. Give Luna the test outcome and relevant setup, and use its transcript and findings for
analysis. Code changes, builds, automated tests and reading existing evidence remain with the
appropriate worker. See `.agents/skills/caol-harness/SKILL.md` for playtest guidance.

## Commands

Keep task logs together in the existing `build_logs/` or `.de67/task-logs/` route used by that task;
the global implementation/evidence rules govern capture and result inspection.

Use `rg --files` with a filename clue to locate unknown source, or `rg -n -F` for a known symbol.
Read the enclosing function or section after locating it. Search generated output separately from
source and select the relevant run/session/artifact first. When output is truncated, narrow the
query or extract the required fields rather than increasing the dump.
Combine `rg` queries over the same targeted files, and retain and poll returned session IDs until
the searches finish or stop them before launching replacements.

## Structured evidence and handoff

For JSON, JSONL and databases, inspect schema when unknown, filter by exact identity, and project
needed fields. A line limit does not bound a large single-line JSON record. Preserve source paths,
identities and full-retrieval handles; omitted detail is not absent evidence.

Keep current task knowledge in the existing `.de67` context/ledger surfaces. Run artifacts under
`.userdata/*/harness_runs/` and session bindings under `.userdata/*/sessions/` or
`.userdata/openclaw_harness/bridge-sessions/` are evidence, not current owner instructions.
