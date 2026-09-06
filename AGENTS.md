# Workspace command and evidence habits

Use these patterns across tasks in this checkout, alongside the global agent guidance.
Choose queries from the current question; the examples below illustrate command shapes, not a
required investigation sequence. Keep complete evidence available while bringing only useful
findings into the conversation.

## Builds and tests

Capture stdout and stderr from build, compilation, test-suite, packaging and dependency-install
commands to a log from the start. Their command type is enough to choose this route; there is no
need to discover whether their output is large by printing it first.

Create a temporary directory once for your worker, then reuse its printed absolute path in later
shell calls. Give concurrent jobs distinct log names. Reuse a job's name for another attempt when
its earlier output is no longer needed; preserve logs that support a finding or comparison.

```sh
agent_log_dir=$(mktemp -d "${TMPDIR:-/tmp}/caol-worker.XXXXXX")
printf 'worker logs: %s\n' "$agent_log_dir"

run_logged() {
    job_name=$1
    shift
    job_log="$agent_log_dir/$job_name.log"
    job_rc=0
    "$@" >"$job_log" 2>&1 || job_rc=$?
    printf 'exit=%s log=%s\n' "$job_rc" "$job_log"
    return "$job_rc"
}
```

Pass the selected command and its arguments to `run_logged`, with a job label such as `build` or
`tests`. The function and variable belong to that shell; in a fresh shell, set `agent_log_dir` to
the saved path and define the function again. This wrapper preserves the command's exit status.
Inspect the test runner's result summary or the relevant diagnostic in the log. For compiler
failures, a starting query is `rg -n 'fatal error:|error:|undefined reference|Undefined symbols' "$job_log"`.
An empty match is not success: use the exit status and the tool's own failure format to choose
another query. Read surrounding lines for a specific diagnostic as needed. Keep long-running
commands redirected when collecting their eventual result as well.

## Source and documentation searches

Use the question to choose a search shape:

- **Unknown file:** `rg --files src tests tools -g '*inspection*'` discovers filenames without
  printing their contents. Substitute the filename clue and relevant roots.
- **Known symbol:** `rg -n -F 'show_npc_inspection' src/npc_inspection.cpp` locates exact matches
  in a known file. Use `rg -l -F 'symbol' src tests` when only matching filenames are needed.
- **Relevant match found:** inspect the enclosing function, section or diagnostic. Choose the
  line range from that match and its boundaries, rather than starting with a large file dump.
- **Broad discovery:** give a Luna helper the question, likely roots and required evidence. Use
  its findings to choose the next local read; follow up on missing detail instead of repeating
  the survey in the primary context.

Search generated output separately from source. Select the relevant run, session, artifact or
log before searching its contents. A line limit such as `head` does little for single-line JSON
or very long compiler commands. When a result is truncated, refine its scope or extract the
needed fields rather than increasing the output allowance.

## Structured evidence and repeated work

For JSON, JSONL and databases, use the available query interface or a parser to filter by exact
identity and project the fields needed for the decision. Prefer selected columns to `SELECT *`
and selected records to dumping a history table. Inspect schema or field names first when they
are unknown. Keep the original artifact and its path so omitted fields remain retrievable.

Reuse known paths, symbols, commands and helper findings from the current task. Reopen them when
a change, contradiction or missing detail gives a reason. In a handoff, pass those entrypoints
and the unresolved question so the next worker can continue from the evidence already gathered.
These habits support complete investigation and verification; a compact result is useful only
when it still answers the question and exposes material uncertainty.
