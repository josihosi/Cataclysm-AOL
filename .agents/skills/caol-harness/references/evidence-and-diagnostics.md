# Retrieve evidence and diagnose

Use the smallest query that resolves the next decision. `look` explicitly refreshes surroundings;
collected actions display changed facts, removals, current input-owner transitions and outcomes.
Menu selection and usable controls change independently of game time. A fresh frame still updates
native authority even when no visual facts change. Chains retain intermediate evidence and show
their terminal observation or interruption; inspect actual progress before interpreting behavior.

`play_cli.py evidence` queries one immutable snapshot across retained cockpit, native, NPC and
runner records. Filter exact actor/run/request/event fields before projection; unavailable identities
remain null. Names and runner-local request IDs alone do not establish cross-process correlation.
Use exact evidence handles for omitted fields, complete records or stable continuation pages.

Supported player/bridge retrieval stdout is bounded to 8192 serialized UTF-8 bytes including the
newline, metadata and errors. This presentation default fits the measured ordinary World/menu
views; it limits neither game execution nor retained evidence. Every response has a complete
immutable `presentation.full_evidence` handle. Oversized values have their own handles. Retrieve
with `evidence_display.py --sha256 HASH`, optionally `--selector FIELD --offset N --limit N`, or
`--export FILE` to write the complete selected JSON to a new file. Never interpret an omission as
absence. Existing `inspect`, `messages`, `log-query` and exact record references remain available.
`log-query --snapshot HASH` keeps subsequent pages on the same source prefix and filters.

Keep routine current controls separate from help: `controls` retrieves macro recipes and evidence
source metadata explicitly. Keep targeted lookups for specific facts or known references with
the primary. Use a read-only Luna subagent for bulky extraction and broad searches across source,
documentation, logs or artifacts, so the primary receives findings rather than the bulk material.
Use `model="gpt-5.6-luna"` and `fork_turns="none"`.
Give the helper the question, known identities and evidence ceiling; it returns a concise answer
with supporting facts, exact references and material uncertainty. Follow up or inspect a cited
location narrowly instead of repeating its bulk reads. Apply this throughout the task, including
when an initially targeted lookup expands into a broad search. If helpers are unavailable, keep
local retrieval targeted and retain full evidence on disk. The primary owns gameplay input and
causal judgment. Historical traces are evidence to retrieve only when a missing detail can change
the decision. See the [search map](searching.md).

## Messages and correlated logs

`messages` reads the displayed observation as JSON, including quoted speech; it defaults to the
latest matching page and does not send game input. `controls` discovers the bound run's native
and transition logs, profile diagnostics, and shared NPC logs with exact paths, availability,
scope and copyable query arguments. Missing files or metadata mean unavailable evidence, not
absence of ecological activity. Shared logs need exact event and identity correlation.

## Performance comparisons

`performance` reads retained CPU/RSS and action intervals; `performance --sample-seconds 1`
works even while game input is pending. `--offset 0 --limit 5` pages exact records.
Use `--tag "comparable workload" --save-baseline FILE`, then `--tag "comparable workload"
--baseline FILE` to compare. Tags assert comparability; CPU is process core percentage,
not host load, and high CPU during simulation is not itself a regression.

## Bridge receipts and exact recovery

For a file-backed live cockpit, request collection returns a verified decision view by default:

```sh
python3 tools/openclaw_harness/cockpit_file_bridge.py response-status \
  --session-dir <session-dir> --request-id <request-id>
```

The response preserves run/request/frame identities, native acceptance or failure, surface kind,
scalar facts, action availability, and contradictions. Transport `ok` does not mean the native
action succeeded or its gameplay postcondition holds. Bulky fields carry selectors, types, and
sizes instead of repeated messages, maps, or nested frames. `response-slice --selector <dot.path>`
retrieves a field; paths also traverse numeric array indices and JSON-string native facts.
`--contains <text>` filters a selected array before `--offset`/`--limit` paging and preserves
original indices (for example, search decoded messages for save failures among repeated flavour).
`response-artifact` with the receipt SHA-256
recovers the full response. Both routes verify the retained artifact.

For legacy exact log queries, use `cockpit_file_bridge.py log-query --path PATH` with
`--where FIELD=JSON`, `--select FIELD`, and the returned snapshot for stable continuation.
Session queries verify retained response receipts. Every original record has an offset, length and
SHA-256 for `record-artifact`; replaced bytes fail verification. Full responses and selected values
use the same byte-bounded presentation and immutable export route as the player CLI.
