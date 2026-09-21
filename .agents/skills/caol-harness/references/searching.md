# Search by question

Start from the response you are investigating, not a guessed universal JSON root:

- A collected `look` response has `.response.current_input` and `.response.result` in the CLI
  output. Other actions can expose `observation` or `terminal_observation` instead of `result`.
  Copy `current_input.source_selector` and `actions_selector` from that response.
- `controls` has `.result.availability` and top-level `.evidence_logs`; there is no `.response`
  wrapper. Availability is session permission, not proof that the current menu accepts an action.
- `inspect` takes the returned selector relative to the retained inner response, without the CLI's
  `response.` prefix. Its answer is in `.slice`. Compact `.preview` is a display aid, not part of
  the retained selector. Follow `.selector` to retrieve omitted or complete data.
- Ordinary successful replies omit startup/process diagnostics. Request them deliberately with
  `cockpit_file_bridge.py response-status --session-dir SESSION --request-id REQUEST --diagnostics`
  when a failure or recovery decision actually needs them.

In the map below, `SOURCE` means the returned `source_selector`, not literal text to type.
Field names under World facts are starting points; another input owner exposes its own facts.

| Question | Search starting point |
| --- | --- |
| Which menu/prompt owns input, and what can I select? | Read `current_input`; inspect its `actions_selector` with `--contains NAME` for a target or `--limit N` to page. Inspect `SOURCE.surface.facts` for prompt text and owner-specific facts. |
| What operations are permitted, and where are this run's logs? | `controls`: `.result.availability`; `.evidence_logs.entries` supplies exact paths, scope and query arguments. |
| What is the player's condition, equipment or position? | Inspect `SOURCE.surface.facts.avatar_status`, `avatar_effects`, or `avatar`; select the relevant child once its fields are known. |
| What is nearby: characters, terrain, objects, effects or zones? | Inspect World fact selectors `visible_entities`, `visible_local`, `minimap`, `overmap`, or `visible_zones`. Filter a list by name/type, then check identity, coordinates and relevant fields. Use larger map detail when immediate neighbours do not cover the question. |
| What did the game say? | `messages --contains TEXT` searches the displayed observation. For an earlier response, inspect its messages selector with `--request-id REQUEST --contains TEXT`. Correlate message time and actor; retained fixture history may be present. |
| What happened in an earlier action or causal event? | Use its request ID with `inspect`; for logs, use the exact query arguments from `controls`, then filter run/event/actor and project relevant fields with repeated `--select`. Follow returned evidence handles for deeper detail. |

For example, after substituting the session, retained request and returned selector:

```sh
python3 tools/openclaw_harness/play_cli.py --session SESSION inspect ACTIONS_SELECTOR --request-id REQUEST --contains wait
python3 tools/openclaw_harness/play_cli.py --session SESSION inspect FACTS_SELECTOR --request-id REQUEST
python3 tools/openclaw_harness/play_cli.py --session SESSION messages --contains "Saving game"
```

Pin `--request-id` when inspecting earlier evidence: otherwise `inspect` uses the last retained
response, which may have changed since the observation you meant. Use `--contains` on lists; it
matches serialized row text case-insensitively, not an exact field predicate. A target-name match
can return several actions; choose by action ID/label and enabled state, not target name alone. Retrieve that narrow
list and use `jq`/Python when the question needs an exact coordinate, identity, or numeric condition.
For example, a retrieved local-tile list can be projected with
`jq '.slice[] | select(.dx == 1 and .dy == 1) | {terrain, furniture, fields}'`.
Those coordinates illustrate a destination, not a fixed setup requirement.

`--offset`/`--limit` page lists only: omit them for an object or scalar. A valid filtered list with
`matched: 0` and `.slice: []` means no match in that selected evidence. An unavailable-selector
error means the path cannot be retrieved; check the owner and returned selector. For saved JSON,
inspect `keys` at the relevant parent after a missing-path `null`; it does not prove absence.
Check `.ok`/`.error` before interpreting `.slice`, and use `page.next_offset` when more matches matter.
Historical `log-query` can return `field_unavailable` for individual projected fields when records
have different shapes; narrow the request/event or inspect the matching record's structure.

## Focused playtest evidence

### Correlated events and omitted values

Use `play_cli.py --session SESSION evidence --actor-id character:N --event actor_observed`
for observed actor positions/status and game time. `--actor-name`, `--request-id`, `--run-id`,
`--process-instance`, `--where FIELD=JSON`, `--contains TEXT` and repeated `--select FIELD`
can narrow the same event envelope. For speech, find the utterance in the runner request payload,
then query the exact request and process instance for completion. Native application remains a
separate observed turn and reply. Older logs may lack that correlation; null means unavailable.

The result's `snapshot` is immutable. Continue its `rows` using the provided SHA, selector and
offset with `evidence_display.py`. Concurrent appends cannot shift that snapshot. Every retained
source range has exact byte/hash evidence; event IDs identify evidence records, not inferred game
causes. `payload` preserves producer details. Actor observation rows do not infer pursuit, order
eligibility, or changes between observations.

All supported player/bridge retrieval stdout fits the declared 8192-byte presentation budget.
For any `omitted` value, use its `evidence.sha256` with:

```sh
python3 tools/openclaw_harness/evidence_display.py --sha256 HASH --offset 0 --limit 20
python3 tools/openclaw_harness/evidence_display.py --sha256 HASH --selector FIELD --export NEW_FILE.json
```

Objects page key/value entries; arrays page elements; strings page exact characters. An oversized
page can itself return exact nested handles. Every response's `presentation.full_evidence` also
recovers the complete result. Export writes full JSON to a new file, never a bulk stdout dump.
Metadata and errors obey the same bound. The guarantee covers these CLIs, not arbitrary shell
commands. The budget is presentation only; no simulation, retention or verification limit follows.

Ordinary collected actions already show changes and interruptions. Use `look` for explicit full
refresh, `inspect` for targeted native detail and `controls` for command recipes. No hand-written
map-suppression pipeline is needed. Intermediate chain observations and receipts remain retained.

### Journal citation packaging

When a helper would simplify citation extraction, supply the exact session, journal request,
claimed actions and known observation/action IDs. Ask for citation IDs, exact check paths relative to `entry.value`, relevant values and run/actor/frame
identities, including contradictions and unsupported claims. The primary owns the verdict and
witness submission; retrieving evidence does not authorize replay, game input or a new test.

Inspect the journal shape first. `result.evidence_journal.entries` may be an archive-sequence
reference object, not an array; list pagination does not apply to that object. Follow its supplied
selector through the supported response inspection route. If local reconstruction is needed for
search, process it locally and print only a citation index or selected values, never the whole
journal. Validate selected citations through retained evidence before building witness checks.
Keep request IDs and citation IDs distinct, and do not prepend `entry.value` to a check path that
is already defined relative to that value. A before/after position pair demonstrates only the
observed displacement; it does not by itself establish follow eligibility or a gameplay defect.
