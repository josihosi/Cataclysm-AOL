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
source metadata explicitly. The playtest owner keeps gameplay input and causal judgment;
Luna handles bulky retrieval, log correlation and artifact comparison from the known run/request
identities and evidence handles. Ask the concrete playtest question; use its returned facts,
references and uncertainty to choose the next action. Compact known lookups can stay local.
When answering an ordinary playtest question still needs awkward manual extraction, tell the
coordinator which question the current view could not answer. Together, use the existing query
or improve that view so subsequent playtests do not repeat the same archaeology. Retrieve history
only when missing detail can change the decision.
See the [search map](searching.md).

## Messages and correlated logs

`messages` reads the displayed observation as JSON, including quoted speech; it defaults to the
latest matching page and does not send game input. `controls` discovers the bound run's native
and transition logs, profile diagnostics, and shared NPC logs with exact paths, availability,
scope and copyable query arguments. Missing files or metadata mean unavailable evidence, not
absence of ecological activity. Shared logs need exact event and identity correlation.

For an apparent NPC action such as door entry, bashing or retreat, follow the [game manual's
assessment method](../GAME-MANUAL.md#assess-an-observed-action). A screenshot or OCR is only a
navigation clue; cite native events, terrain and saved actor/operation state for the conclusion.

## NPC decisions during playtests

For NPC behavior tests, enable the existing run-bound decision trace at scenario launch and name
the actors/behavior being examined in the brief. Use it for cannibal and bandit journeys and, as
coverage is implemented, camp duties/defense and stalker pursuit. This is harness instrumentation,
never normal-game logging. Do not infer support merely because an actor appears on screen.

**Available in builds containing R025/R041:** `raid_actor_trace: true` enables the run-bound
recorder; explicit `decision_trace_actor_ids` select local NPCs, including scouts, defenders and
peaceful bandits, with an optional turn window. Without explicit selection, automatic coverage is
limited to supported hostile local members. Select the actual current party, not historical IDs.
R041 adds applied damage, confirmed death and sleep/wake edges. Exact sleep cause may remain null.
Abstract travel and monster decision-making are not covered by the local NPC hook; combine existing
transition records and actor saves. A monster appearing as a damage endpoint is not a monster AI
trace. Check the bound build and emitted scope before relying on any of these capabilities.

Use the [suite evidence map](playtest-evidence-map.md) for each arm's proof and the remaining
implementation gaps. Existing `play evidence --decisions` provides compact interval retrieval;
R041's handoff supplies the tested syntax and completeness fields.

Read decisions at a meaningful phase change, unexpected behavior or outcome; no extra stop after
every turn. Ask: which actor chose what, against whom, what could it see, where was it going, and
was fear/danger or a group gate involved? Correlate numeric actor IDs and turns with actual movement,
damage and saved state. Group intent is not an individual action. Trace visibility is diagnostic
knowledge and must not be fed back into an actor's gameplay knowledge.

Use the existing `log-query` on the bound `semantic.native.events.jsonl`, filtering
`event="raid_actor_action"` and `npc_id=ID`, then selecting `game_turn`, `action`, `target`,
`target_visible`, `position_abs`, `goto_abs`, `path_length`, `path_next_passable`, `panic` and
`flee` as relevant. Query `raid_site_search` for the same operation/time when group behavior matters.
Check `raid_trace_repeat` and `raid_trace_truncated`: absent rows are not proof of inactivity;
unchanged spans may be compacted and a retained live game may not have flushed its final summary.
Keep exact source handles rather than copying whole logs into the worker context. A view limit is
separate from native capture truncation. Use the existing normal-CLI decision view before writing a task-local query script.

R023's retained native example is documented in
[the ergonomics audit](../../../../.de67/task-logs/decision-trace-ergonomics-20260928/report.md):
Lloyd moved and attacked with panic0 despite earlier uncertainty from the screen. Use that as a
query/schema example, not an assumption that every future actor behaves correctly.

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

For the published cross-source route, start with a narrow `play_cli evidence` projection:

```sh
python3 tools/openclaw_harness/play_cli.py --session <session> evidence \
  --run-id <run> --process-instance <process> --request-id <request> \
  --actor-id <actor> --actor-name <name> \
  --select event,run_id,process_instance,request_id,actor_id,actor_name,\
payload.payload.accepted,payload.payload.rejection_reason,payload.payload.outcome
```

The response keeps `accepted`, rejection and outcome rows under the same exact tuple while
preserving unavailable fields explicitly. Recover the response's `presentation.full_evidence`
handle for the complete query result, then resolve each returned `source` handle (`path`,
`offset`, `length`, `sha256`) through `record-artifact` or its retained `retained_raw` handle.
If a source changes, the path citation fails hash verification; the retained raw handle remains
the original bytes. For an honest S-EFF comparison, report projection subprocess invocation and
stdout bytes, necessary operational follow-up count and returned bytes, and their combined total
against the bulk subprocess invocation and stdout bytes. Keep citation-integrity reads separate
when they only verify provenance rather than resolve the decision; include them in a second
combined targeted total. `scanned_records`/`scanned_bytes` describe source work, not CLI output
cost. Unavailable sources remain reported as unavailable rather than being treated as no matches.

For legacy exact log queries, use `cockpit_file_bridge.py log-query --path PATH` with
`--run-id`, `--process-instance`, `--request-id`, `--actor-id`, `--actor-name`, and `--event`
for exact identities/events, then `--where FIELD=JSON` and `--select FIELD` for a narrow
projection. This reader also parses prefixed native transition records; do not feed those lines
straight to `json.loads`. Filter their actual top-level fields, not an assumed `data.*` wrapper.
Reuse a tested query from the current handoff; investigate its schema only when the question or
source format changes. For example, to inspect one rejected request while keeping a contradictory
acceptance field visible:

```sh
python3 tools/openclaw_harness/cockpit_file_bridge.py log-query --path <log> \
  --run-id <run> --process-instance <process> --request-id <request> \
  --actor-id <actor> --event rejection \
  --select actor_id --select actor_name --select run_id --select process_instance \
  --select request_id --select accepted --select rejection_reason --select outcome
```

The returned snapshot supports stable continuation; each row retains an exact byte-range
handle for `record-artifact` recovery. Missing projected fields are reported as unavailable,
not silently omitted.
Session queries verify retained response receipts. Every original record has an offset, length and
SHA-256 for `record-artifact`; replaced bytes fail verification. Full responses and selected values
use the same byte-bounded presentation and immutable export route as the player CLI.
