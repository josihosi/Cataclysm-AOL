---
name: caol-harness
description: Query, explain, launch, operate, and audit C-AOL playtests through the authoritative registry and cockpit.
---

# C-AOL harness

Use the registry for scenario projection, typed selection, technical launch authority, report
ingestion, and lifecycle history. Use the cockpit/TUI for live observation, native action, receipts,
witness, and finish. The coordinator supplies the outcome and compact charter; the worker owns how
to make the proof work.

## Setup and interventions

Scenario setup exists to remove irrelevant friction, not to prove gameplay. Choose mutations,
fixtures, debug tools, or repairs that help the assigned outcome. Record every applied transform or
intervention and give manufactured state zero feature credit. Non-combat or observer runs may
benefit from the debug needs, temperature, stamina, cardio, clairvoyance, nightvision, cloak, or
invisibility controls, but no blanket set is required. Verify only the setup facts the selected run
actually depends on.

Fictional spotting, injury, or death is gameplay evidence rather than external safety. Wait and
movement operations expose three choices:

- `stop_on_interruption` — cautious default;
- `handle_classified_non_dangerous` — recover known flavour or harmless prompts;
- `ignore_danger_and_interruptions` — continue through danger/interruption prompts and receipt
  what was handled.

The permissive choice requires no cloak, scenario permission, or supervisor approval. Cloaking may
still be useful. Exact-identity creature zapping is allowed as a zero-credit diagnostic/setup
intervention; it cannot prove natural route, ecology, combat, lifecycle, qualification, or
certification behavior.

## Select and launch

Translate the proof question into typed requirements, then query with:

```sh
python3 tools/openclaw_harness/scenario_registry_cli.py registry-query --query-json '<request>'
```

The result shows five ranked matches by default (`--page-size` changes that presentation). Each
match gives its fit, evidence, lifecycle, and manifest binding. Follow `page.next` to browse the same
saved result; paging does not rerun selection or issue another token. `full_result` retrieves the
complete evaluation when a specific uncertainty needs it. The selected token belongs only to
`selected_scenario_id`, not to every displayed candidate. Refine the query to choose a different fit.

Explain the candidate fit, evidence ceiling, lifecycle, binding, and readiness. Querying never
launches. If no executable selection exists, use the returned facts to choose whether to build,
repair, create, rebind, or deliberately run an isolated zero-credit diagnosis. Do not weaken the
question or combine incompatible footing. A stale executable may support an explicitly isolated
harness diagnosis only; current-product conclusions require a source-matching executable.

For a selected playtest, the coordinator brief and matching validated charter are the execution
request. The registry token is single-use technical authority, not human permission:

Use the returned `next_action`: a ready selected route supplies its launch argument array,
including the witness charter and, for a live cockpit, `registry-detached-launch` with a new session
path. Do not pre-create that directory. A build, repair, or missing-charter response identifies the
prerequisite instead. Saved query readiness is a snapshot; launch revalidates current state.

Launch revalidates source, executable, scenario, world, ownership, and runtime. Missing charter,
stale binding, fixture defects, or tool defects are agent-owned repair when the outcome remains in
scope. The worker may change strategy, repair, obtain fresh authority, and rerun without another
human request.

## Operate and finish

Native gameplay here means dispatch through the game's own semantic owners. Run the player CLI
in the game worktree, locally or over SSH; it does not require a desktop-control connection.
For a registry-launched file-backed session, use the persistent player client:

```sh
python3 tools/openclaw_harness/play_cli.py --session SESSION look
python3 tools/openclaw_harness/play_cli.py --session SESSION act ACTION [--target STABLE_ID]
python3 tools/openclaw_harness/play_cli.py --session SESSION controls
python3 tools/openclaw_harness/play_cli.py --session SESSION messages --contains TEXT
python3 tools/openclaw_harness/play_cli.py --session SESSION call --request REQUEST.json
python3 tools/openclaw_harness/play_cli.py --session SESSION collect
python3 tools/openclaw_harness/play_cli.py --session SESSION cancel --reason "stop this pending request"
python3 tools/openclaw_harness/play_cli.py --session SESSION inspect SELECTOR --limit 5
python3 tools/openclaw_harness/play_cli.py --session SESSION journal --reason "What this run established"
python3 tools/openclaw_harness/play_cli.py --session SESSION finish --witness FILE
```

`controls` is read-only, even while a request is pending. It provides copyable wait/movement
requests, native recipe semantics, danger choices, and session permissions after `look`/`collect`.
An ordinary interruption stops only the macro: inspect its terminal observation and partial
progress, then choose the next native action. All action and observation failures leave the game
running. Ownership or receipt failures revoke stale input grants: `look` again before choosing an
action. A failed command does not authorize quitting, cleanup, replay, or a replacement game.
To end without making a gameplay claim, use `quit --reason "your reason"`, or send
`{"action":"run.quit","stop_reason":"your reason"}` through `call --request`. A client disconnect also leaves the game running. Only explicit native
quit, `run.quit`, `run.finish`, or requested bridge cleanup ends it.
`messages` reads the displayed observation as JSON, including quoted speech; it defaults to the
latest matching page and does not send game input. `controls` also identifies the central NPC
prompt, response and runner logs, which are shared across runs and need exact event correlation.
Scripted Dialogue choices and free-text speech are different native routes. A model request needs
its utterance, hearer, time and resulting reply or error correlated; automatic `prewarm` requests
only load the model and do not establish an NPC conversation.
When the scenario declares a save/reload continuation, finish the saved segment with `finish`;
`collect` reports `reentered`, then `look` exposes the restored world's new owner. The bridge handles
the declared process replacement without reinstalling the fixture. `quit` ends the entire scenario
and skips that continuation. Saving alone does not establish new-process persistence.
`performance` reads retained CPU/RSS and action intervals; `performance --sample-seconds 1`
works even while game input is pending. `--offset 0 --limit 5` pages exact records.
Use `--tag "comparable workload" --save-baseline FILE`, then `--tag "comparable workload"
--baseline FILE` to compare. Tags assert comparability; CPU is process core percentage,
not host load, and high CPU during simulation is not itself a regression.

For an existing structured `game.*` macro, put its complete request object (including `action`
and its recipe) in `REQUEST.json` and use `call --request`. The client preserves the request;
the cockpit checks the operation and recipe against the session. It supplies no recipe defaults.
Collect the response once and continue from its terminal observation, or use `look` to reassess.

The client owns request IDs, binding, pending responses and the last displayed frame. A pending
action needs `collect`, never resubmission. Use `cancel` to stop a pending request cooperatively; it remains available while another CLI is waiting. Then collect the original request and look again. Cancellation leaves the game running. Input already emitted can have an unknown outcome, and a native receipt already written remains evidence. Choose from the current surface's actions; supply
`--target` only when that action advertises a stable ID. A rejected stale owner needs a fresh
`look` before deciding what to do. Nested menus are game state, not necessarily failures.

The default view names the current input owner and its available navigation. Large action catalogs
show five distinct targets plus controls; use the supplied selector to search or page further targets.
It includes player health, needs, stamina, named effects and weapon state, immediate neighbouring
tiles, a terrain map, nearby entities and grouped recent messages. Omitted detail retains exact
selectors and paging. Archived history remains retrievable; references are storage handles, not
missing evidence.
Trade panes can include nearby ground, vehicle, camp or companion items as well as carried items.
An item listed on one party's side does not prove that actor is carrying it. Follow placement
prompts through their actual outcome and inspect inventory or pickup/location facts before claiming
possession. Item UIDs can change after transfer or process reload; compare item type, count, location
and actor identity. Disabled zones may be absent from visible World zones; inspect the zone manager
to distinguish a disabled zone from a missing one.
Saving and returning to the main menu differs from quitting the application. An actionless
`process_exited` observation reports the bound process outcome, not save durability or feature
success. After `finish`, `collect` reports actual cleanup separately.

The journal returns citation IDs and exact witness fields. Preserve JSON types in checks:
`false` differs from `"false"`. Inspect the cited entry to obtain its actual field paths.
The cockpit exposes structured movement/wait macros through `call --request` and diagnostic
retrieval through `inspect`; use those when the proof question needs them rather than reconstructing transport bookkeeping
for ordinary native actions.

Observe current native state, choose actions, and preserve receipts and contradictions. A first
divergence is a diagnostic anchor, not an automatic stop: inspect it, repair, improvise, rerun, or
finish according to the outcome. Stop only when the claim is settled or continuation requires a
real external decision, unavailable capability, irreversible user-data risk, binding change, or
materially different owner outcome.

At the honest boundary, seal `run.witness` and call `run.finish`. State the smallest conclusion
supported by cited immutable evidence; do not invent facts or promote the evidence ceiling.
Stop reasons and witness text are your own conclusions, not independent game observations.
Reconcile them with the resulting native state and any later messages, including outcomes revealed
when a nested interaction returns to World. When
one run settles independent claims differently, submit a `caol-playtest-witness-bundle-v1`: each
claim keeps its own verdict, while bound product or harness defects name affected and explicitly
unaffected claims. Continue useful observation after a defect when the remaining causal footing is
clean. The coordinator records ordinary defects in `.de67/debug-findings.md`; reserve the durable
capability-gap history for missing reusable observation, action, or setup interfaces.

`registry-record-witness` persists the witness and `registry-review-witness` records the
coordinator's separate causal judgment.

Inspect registry continuity with a compact, artifact-backed receipt by default.  Pass one or more
exact `--manifest-id` values when those identities are already known; `--include-state` is an
explicit lifecycle projection, not a prose search:

```sh
python3 tools/openclaw_harness/scenario_registry_cli.py registry-status \
  --manifest-id <exact-manifest-id>
```

The receipt's digest is the only full-recovery handle.  Retrieve its complete registry payload
with `registry-artifact --sha256 <receipt-digest>`, or use `--full` only when this invocation
itself needs the complete status payload.  `runtime-status` has the same default receipt and
explicit routes through `runtime-status-artifact --sha256 <receipt-digest>` and `--full`.

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

Query evidence before rendering it. The same bridge CLI provides:

```sh
python3 tools/openclaw_harness/cockpit_file_bridge.py log-query \
  --session-dir <session-dir> --request-id <bridge-request-id>
python3 tools/openclaw_harness/cockpit_file_bridge.py log-query \
  --path <exact-debug-or-jsonl-log> --run-id <run-id> --event surface_receipt
```

Session queries inspect retained responses; `--path` queries an exact log and may be repeated.
Filter with `--frame-id`, native `--request-id` on logs, or `--where 'field="value"'` (JSON values);
`--select <dot.path>` retrieves only selected semantic fields; `--contains <text>` narrows matching
records before rendering. Default pages contain 20 records;
counts and `page.next_offset` disclose every remaining match. These are presentation choices, not
proof limits. Parse failures and records lacking run identity are counted separately, never silently
attributed to a run; query `--event unparsed` or `--event text` to inspect them.
Every returned row has a path, byte offset, length, and SHA-256 for `record-artifact`; it verifies
that exact range before returning raw text and parsed evidence, or selected fields. Rotation or
replacement produces a hash error. Raw files remain unchanged. The CLI help and response retrieval
metadata expose these routes; whole-record text searches and line tails are unnecessary for field
discovery.

Report startup, feature outcome, contradictions, evidence ceiling, and cleanup separately.
