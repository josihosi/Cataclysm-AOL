# What harness workers actually take in

Read-only audit, 21 September 2026. No harness behavior or worker instructions changed.

## Scope and measurement

Inspected current harness guidance and display implementation, actual task briefs, and local worker transcripts. Detailed measurements use the long-lived worker task `01a0c101-a0b9-72f2-b509-7302b2bb7102`, initially assigned `evidence_scope_repair`, subsequently reused for several assignments including live signal playtests and repairs. This is a concrete case study, not a representative average of all workers.

The source transcript path, frozen line-count boundary, individual output line numbers and counts are in `measurements.json`. The source task was still growing during inspection; final counts below use the saved measurement snapshot. Measurements are UTF-8 bytes, not model tokens, cached-input charges or total billed usage. System/tool context, reasoning, repeated context processing, image input and compaction costs are not quantified here. Large files on disk are not counted as model input unless returned through a tool output.

## Layers of input

| Layer | What the worker receives | Observed size / behavior |
| --- | --- | --- |
| Ambient context | Agent instructions, tool definitions, skill catalog, workspace rules and conversation history | Additional to everything below; not measured here |
| Assignment | Goal, scope, standing guidance, ownership, completion criteria, communication and mailbox commands | First prepared assignment in sampled task: 16,848 bytes |
| Harness skill | Reference map plus authority, lifecycle, recovery and evidence rules | Current `SKILL.md`: 3,763 bytes |
| Operational references | Selection/launch, live controls, movement, setup, diagnostics, witness packaging | All current harness skill markdown: 46,665 bytes; skill + selection + live + diagnostics + finish: 25,626 bytes |
| Selection and launch | Ranked scenarios, evidence ceilings, manifests, readiness, charter bindings, hashes and copyable launch arguments | Five candidate matches by default; selection/launch guide specifies artifact-backed recovery |
| Gameplay outputs | Current input owner, actions, changed facts, messages, time, native result and authority | Includes substantial extra receipt and performance metadata; concrete example below |
| Investigation | Targeted evidence queries, source/test reads, raw artifacts, GUI recovery outputs, schema/help retrieval | Can exceed the gameplay display cap; observed mixed outputs around 40 KB, some truncated |
| Closeout | Journal citation fields, witness schema and verdict, cleanup/process evidence, DE67 receipt structure | Additional evidence retrieval and formatting work; exact amount varies by assignment |

The guidance explicitly says references are conditional, not a preflight reading sequence. The 46.7 KB is an inventory, not a mandatory startup load.

## Actual four-minute wait: 7,504 bytes

Exact returned JSON is saved as `actual-output-line-14130.json`, pretty-printed for readability. Source stdout was compact JSON. Top-level value sizes, excluding their field names and punctuation:

| Component | Bytes | Contents |
| --- | ---: | --- |
| `turn_assessment` | 4,564 | Machine/process/binding identity, configuration unavailable, missing baseline, trace offsets, timing/CPU statistics, incomplete-turn event, freshness, pending operation, empty alarms/recoveries and explanatory prose |
| `response` | 1,830 | World owner, successful wait, four minutes of progress, changed facts, four “You finish waiting.” messages, authority and selectors |
| `receipt` | 495 | Binding, request/hash, response artifact/hash, schema, sequence and generation |
| `presentation` | 269 | 8,192-byte budget, immutable full-evidence hash and retrieval command |
| `request_result` | 145 | Repeated request identity and retrieval syntax |

The worker meaningfully learns: the wait reached its target, time advanced from minute 9361 to 9365, the owner is World, several observed-turn/current-minute fields changed, and four waiting messages arrived. Yet 61% of the bytes describe performance assessment. That assessment has `status: measured_unassessed`, `reason: missing_baseline`, `alarms: []` and `recoveries: []`.

Examples of literal strings repeated inside the telemetry:

```text
missing_or_invalid_turn_configuration
window_wall_span_including_inter_turn_input_gaps_not_simulation_throughput
completed_native_turn_simulation_seconds_only
Native turn boundaries measure simulation only. Input, pause, load, save and transport phases do not create simulation alarms.
```

It also repeats long absolute paths, 64-character hashes and request/run IDs. These are useful evidence locators, but displaying them many times is a separate choice from retaining them.

The next one-minute wait returns 8,022 bytes, including 4,565 bytes of telemetry. One tiny empty array is represented as:

```json
{"omitted":true,"selector":"result.terminal_observation.surface.facts.structural_signal_dispatch.loaded_source.items","type":"list","json_bytes":2,"count":0}
```

That is substantially longer than `[]`. Three-element coordinates are also replaced by omission descriptors. This comes from the generic gameplay fact presentation, not a requirement that every omitted value was too large to print.

## Repetition measured across the sampled worker

- 268 complete, parseable player-command JSON responses with top-level `turn_assessment`: **1,809,239 bytes**.
- Their `turn_assessment` values: **1,125,910 bytes**, or **62.2%** of those JSON bytes.
- Median response: **7,135.5 bytes**.
- 60 tool calls that read or mention harness markdown in a shell read command returned **591,183 bytes**. Some also read code or other files, so this is not a pure documentation byte total.
- Those calls mentioned reading the skill 23 times; live operation 19; selection/launch 19; diagnostics 18; witness/finish 16. Counts are per call/file, and can include partial reads or mixed commands.
- The sampled task recorded 22 compactions and multiple assignments. Repeated reading is observed; the record does not prove each reread was unnecessary or caused by compaction.

The per-response sample excludes multiline, truncated or wrapped JSON that did not parse by the stated method. It is not every command in the task, and is not all harness usage today.

## What is already economical

Gameplay presentation uses changed facts, action changes, message deltas, terrain previews and bounded action catalogs. The documented supported player/bridge retrieval output budget is 8,192 bytes per response, with full immutable evidence retained separately. The inspected quit response references about 5.96 MB of action history without printing it. The problem is therefore not simply that every action dumps all history.

See `tools/openclaw_harness/gameplay_display.py` for deltas, `cockpit_evidence.py:201` for fact previews/omission descriptors, and `play_cli.py:589` for adding performance assessment to the returned result.

## Most defensible reduction candidates

1. Keep detailed performance evidence retrievable while routine gameplay displays only meaningful alarms, changes or a short assessment status. This is the largest directly measured opportunity. The 62.2% figure is its current byte share, not a promised token saving.
2. Preserve instruction continuity across reused assignments so workers do not routinely reload multiple full guides. Check why rereads happen before deleting safety/evidence requirements.
3. Print small values directly, especially empty arrays and coordinate triples; reserve verbose omission handles for genuinely bulky data.
4. Reduce repeated locator and instructional boilerplate in routine replies while keeping exact recovery available.
5. Treat source/log/GUI dump size separately from the harness response budget. A bounded gameplay CLI does not bound a worker's general shell investigation.

These are assessment findings, not implemented changes. Whether individual fields can be removed safely depends on the decision the worker is making; native acceptance, usable authority, relevant game changes, contradictions and recovery identities still need to be accessible.
