# Select a scenario and launch

Translate the proof question into typed requirements, then query with:

```sh
python3 tools/openclaw_harness/scenario_registry_cli.py registry-query --query-json '<request>'
```

Every launch loads the API credential and installs the standard harness controls, even when
the scenario does not use them. On Mac, the launcher privately resolves a missing key from
the user's login environment after checking the secure store; workers need no special shell.
Use `/opt/homebrew/bin/python3` when Homebrew is absent from PATH. Never print the key or put it
in arguments. Loading the credential does not enable API use in the game.

A previous failed or stale run does not block another ordinary playtest of a valid, present
scenario. Select it normally with the playtest brief and charter. Prior results remain visible
in the route history; selecting the declared scenario does not turn those results into proof.
Retired or missing scenarios and incompatible executables still need their actual setup fixed.

The default reply is plain text: matching scenario names, the exact launch command, or the
unmet requirements and build command. Use global `--json` before the subcommand when a script
needs receipt fields or when diagnosing selection. It does not change selection or launch behavior.
When scripting needs JSON, capture it to a file and print only the chosen command or relevant
failure. Do not dump the whole receipt into the worker conversation for ordinary selection.
The machine-readable result shows five ranked matches by default (`--page-size` changes that presentation). Each
match gives its fit, evidence, lifecycle, and manifest binding. Follow `page.next` to browse the same
saved result; paging does not rerun selection or issue another token. Rejection causes explain
excluded candidates; their `details_argv` pages the exclusions. For a known scenario identity,
use its `details_argv`, or recover it from the saved query directly:

```sh
python3 tools/openclaw_harness/scenario_registry_cli.py registry-query-page \
  --sha256 <query-digest> --scenario-id <scenario-id>
```

This returns the exact `source_path`, manifest binding, lifecycle, saved candidate facts and next
action. Use the returned path for source inspection. For observed playtest evidence, add
`--run-id <native-run-id>` to this page query; `--receipt-id <receipt-id>` can further select a
receipt paired with that run. The compact result separates scenario declarations from run
observations. Declarations describe intended coverage, not observed success; keep absent or
unavailable run evidence visible and follow exact evidence handles for unresolved detail.
`full_result` is the verified full-result receipt;
`registry-query-artifact --sha256 <digest> --output <path>` exports it when deeper evidence is
needed. `registry-query --full` also exports to a file rather than printing bulk.
The selected token belongs only to
`selected_scenario_id`, not to every displayed candidate. Refine the query to choose a different fit.

Explain the candidate fit, evidence ceiling, lifecycle, binding, and readiness. Querying never
launches. If no executable selection exists, use the returned facts to choose whether to build,
repair, create, rebind, or deliberately run an isolated zero-credit diagnosis. Do not weaken the
question or combine incompatible footing. On this Mac, `python3 tools/openclaw_harness/build_source_bound_macos.py --renderer tiles` builds
and records the exact source/executable binding. `runtime-status` and registry readiness expose the
current binding and build entrypoint. Use it when the binding is insufficient or contradicted; a
ready binding needs no rediscovery or rebuild.
A stale executable may support an explicitly isolated
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

The three `flesh_raptor.live_*_skirmisher_mcw` probes check native planner logs.
Their executable must be built with `CPPFLAGS='-DDEBUG_INFO -DDEBUG_ENABLE_GAME'`
using `build_source_bound_macos.py` and a separate build prefix. An ordinary release
build suppresses those planner records even when combat works; keep the original
audit checks and pass the diagnostic executable through the existing `--executable` option.

## Inspect registry and runtime continuity

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
