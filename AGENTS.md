# Workspace command and evidence habits

Apply the global contract/deletion test here. Select evidence from the current question; these
query patterns are aids, not an investigation sequence. Retain complete artifacts while returning
only the facts needed for the next decision.

## Commands

Capture stdout and stderr from builds, tests, packaging and dependency installation in a log from
the start. Reuse a task log directory, use distinct names for concurrent jobs, and preserve exit
status. Inspect the runner's own result summary and relevant diagnostics; an empty error search is
not success. Keep long-running command output redirected when collecting its result.

Use `rg --files` with a filename clue to locate unknown source, or `rg -n -F` for a known symbol.
Read the enclosing function or section after locating it. Search generated output separately from
source and select the relevant run/session/artifact first. When output is truncated, narrow the
query or extract the required fields rather than increasing the dump. Delegate a specific evidence
question to Luna when it reduces total work; do not repeat the survey yourself.

## Structured evidence and handoff

For JSON, JSONL and databases, inspect schema when unknown, filter by exact identity, and project
needed fields. A line limit does not bound a large single-line JSON record. Preserve source paths,
identities and full-retrieval handles; omitted detail is not absent evidence.

Reuse known paths, symbols, commands and findings. Reopen them for a change, contradiction or
material missing detail. Pass the current outcome, accepted frontier, unresolved question and exact
entrypoints to a successor. Historical instructions retain their original scope; the current owner
contract controls the assignment. Clear current data and retrievable history serve both execution
and review without another reporting procedure.
