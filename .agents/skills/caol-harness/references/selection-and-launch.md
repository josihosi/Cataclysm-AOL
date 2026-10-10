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
ready binding needs no rediscovery or rebuild. Compare product-source hashes with product-source
hashes: `runtime_source_sha256` also covers harness/fixture changes and need not equal
`product_source_sha256`. A difference between those domains alone is not executable staleness.
When native headers/layouts change, a source/executable hash alone does not prove dependent
objects were rebuilt. Do not clone a compiled build prefix into a differently named prefix:
its `.d` targets can still name the old path (and older dependencies may name the wrong object
target). Use a fresh empty build prefix when dependency provenance is uncertain; otherwise
verify the actual dependency targets and changed-header consumers before reusing objects.
R015 mixed old `game.o` with the new site layout and crashed; R016 clean dependency rebuild
plus same-save native World/action recovered it. Exact evidence:
`build_logs/first-smoke-016/startup-crash-diagnosis-checkpoint.md`,
`full-build/object-provenance.json` and `native-recovery-result.json` in that directory.
This is a changed-header/cache condition, not a mandatory clean rebuild on every playtest.
A stale executable may support an explicitly isolated
harness diagnosis only; current-product conclusions require a source-matching executable.

For an ordinary selected playtest, one outcome brief supplies context. The typed query and
optional `--scenario-id` select the save/scenario; outcome prose is not a second selector.
Select the build for this run with `registry-query` or `registry-bootstrap`
`--run-build-receipt`, or `--executable` for an ordinary build. The resolved run binding carries
that exact build through readiness and launch. Legacy scenario executable/build fields do not
select it, so the same saved scenario can be used with a separately selected coherent build.
The registry token remains single-use technical authority, not human permission.

Use the returned `next_action`: a ready selected route supplies its launch argument array and,
for a live cockpit, `registry-detached-launch` with a new session path. Do not pre-create that
directory. A legacy witness charter is optional journal metadata; matching duplicated prose is
not a launch prerequisite. The actual run journal remains evidence, with acceptance judged
separately. Saved query readiness is a snapshot; launch revalidates current state.

For a task-local saved start, carry `saved_world_snapshot` from the verified current checkpoint
and the chosen destination profile in the selected source. When its initial `steps` are exactly
one `native_semantic_bootstrap` followed by one `cockpit_live_session`, startup reuses those
source-bound steps directly; no duplicate `post_relaunch` declaration or new label is needed.
The snapshot is still copied/validated through the existing saved-world path, with fixture
installation disabled. An explicit `post_relaunch` retains its own reentry steps and actual
initial `terminal_save_step_label`. Routes containing setup or additional steps need that explicit
contract so startup cannot accidentally replay setup or guess which actions to omit. Declaration
lint/staging checks the declared saved-start shape; launch preflight also checks a continuation
requested through CLI flags before claiming authority. It never derives a saved turn or rewrites
source bytes. A useful read-only check is:

```sh
python3 tools/openclaw_harness/scenario_registry_cli.py --json lint-declarations "$SCENARIO_SOURCE"
```

Use the returned current checkpoint and exact retained save, not an older starting save merely
because its label is familiar. Source declarations and native save evidence remain separate.

If a launch supplies `--profile`, use the profile's name under `.userdata`, not its absolute path.
The startup helper sanitizes slashes into a new name; an absolute path can silently select an
empty userdir. Before game input, compare the effective userdir and intended save hashes and
require a same-run World descriptor. The [game manual](../GAME-MANUAL.md#check-the-effective-profile)
records the failed R010 example and its evidence.

Launch revalidates source, executable, scenario, world, ownership, and runtime. Stale binding,
fixture defects, or tool defects are agent-owned repair when the outcome remains in
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

For a focused Tiles-linked C++ test, reuse the configured source-bound build's compile/link inputs
or its existing test target. A standalone default test make can omit SDL3 flags and Tiles objects;
that is a build setup failure, not a game regression. The recovered R048 command and inputs are
retained in `build_logs/first-smoke-048/r048-route-probe-build/build.full.log`.
