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

### Correlated NPC conversation

Start with the exact log paths advertised by `controls`. Query the known request, actor and
run/time window; a runner-local ID such as `req_0` is not globally unique. Retrieve the request
and reply together once, then expand only missing fields. Use `log-query --path PATH
--request-id REQUEST` where that log exposes request identity, adding repeated `--select FIELD`
arguments for the question. For text logs, extract the matching request/reply block locally by
its verified timestamp and actor. Do not substitute overlapping tails of several logs for this
correlation. Preserve the utterance, reply/order, actor identity and correlation pointers; a reply
alone does not prove subsequent NPC behavior. Distinguish no match from an unavailable field.

### Repeated movement responses

The following read-only display projection was tested on retained look, movement, pause and NPC
inspection responses. It leaves unknown shapes intact and retains `current_input`, surface actions,
top-level receipts, interruptions and warnings. It replaces named map blocks and duplicate
observation metadata with retrievable selectors. Use it when those maps are not needed for the
next decision; expand the selector when terrain or routing matters. Keep the full retained response.

```sh
python3 tools/openclaw_harness/cockpit_file_bridge.py response-status \
  --session-dir "$SESSION" --request-id "$REQUEST" |
python3 -c '
import json,sys
d=json.load(sys.stdin)
r=d.get("response",d)
def ref(v,p):
    return {"projection_omitted":True,
            "selector":v.get("selector",p) if isinstance(v,dict) else p}
for key in ("observation","terminal_observation","result"):
    o=r.get(key)
    if not isinstance(o,dict) or not isinstance(o.get("surface"),dict):
        continue
    for k in ("advertised_actions","advertised_action_details","receipt","compact_log"):
        if k in o:
            o[k]=ref(o[k],key+"."+k)
    facts=o["surface"].get("facts",{})
    for k in ("minimap","overmap","visible_local"):
        if k in facts:
            facts[k]=ref(facts[k],key+".surface.facts."+k)
json.dump(d,sys.stdout,separators=(",",":"))
'
```

World `current_input.controls` can be empty while `observation.surface.actions` contains offered
actions: retain both. Standard compact output may already replace movement coordinates or NPC
orders with references. Retrieve decision-relevant values explicitly, for example:

```sh
python3 tools/openclaw_harness/cockpit_file_bridge.py response-slice \
  --session-dir "$SESSION" --request-id "$REQUEST" \
  --selector receipt.native_receipt.after_absolute_ms
# For an NPC inspection response, use its returned orders selector, such as:
# observation.surface.facts.diagnostic_orders
```

These selectors were verified on retained evidence, not live gameplay. The projection samples
were successful responses; they do not establish behavior during an actual interruption. If a
receipt or log contains unique decision-relevant detail, expand it before acting.

### Journal citation packaging

Give Luna the exact session, journal request, claimed actions and known observation/action IDs.
Ask for citation IDs, exact check paths relative to `entry.value`, relevant values and run/actor/frame
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
