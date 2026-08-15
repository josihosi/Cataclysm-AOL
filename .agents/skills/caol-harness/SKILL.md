---
name: caol-harness
description: Query, explain, explicitly launch, and audit C-AOL harness scenarios through the authoritative registry CLI.
---

# C-AOL Harness Registry

Use this project-scoped skill for a registry-backed harness scenario. The
registry CLI is the sole owner of scenario projection, typed selection, launch
authorization, report ingestion, and lifecycle/history inspection.

## Query before launch

Run the CLI from the repository root. When the scenario declarations need to
be projected, use `rebuild`; when existing report bindings need their current
owners recomputed, use `reconcile`:

```sh
python3 tools/openclaw_harness/scenario_registry_cli.py rebuild
python3 tools/openclaw_harness/scenario_registry_cli.py reconcile
```

Construct or request a typed query, then submit it with `registry-query`:

```sh
python3 tools/openclaw_harness/scenario_registry_cli.py registry-query --query-json '{"requirements":[{"key":"player.injured","op":"eq","value":false,"minimum_evidence":"declared"}],"preferences":[]}'
```

The query vocabulary is deliberately small: the top-level object has
`requirements` and `preferences`; each predicate has a capability `key`, an
operator (`eq`, `contains`, `present`, `absent`, or `range`), and an optional
evidence floor (`declared`, `inspected`, or `run-verified`). `eq` and
`contains` use `value`; `range` uses `minimum` and/or `maximum`.

Explain the returned candidate hard results, evidence states, lifecycle and
route evidence before proceeding. A successful selection returns a
`selection_token`. If no executable selection exists, the result names an
inert draft whose `executable` field is `false`; report the unmet footing and
stop there.

## Explicit selected launch and report

Do not launch from a query or from a draft. Only after an explicit request to
run the selected scenario, invoke the returned token:

```sh
python3 tools/openclaw_harness/scenario_registry_cli.py registry-launch <selection-token>
```

`registry-launch` owns the token, source, route, and runtime revalidation and
the canonical probe route. Its finalizer ingests the resulting
`probe.report.json` only after accepted cleanup. Read the finalized report as
separate startup and feature verdicts, and report its cleanup outcome.

Inspect continuity through the existing status owner; it includes lifecycle,
relation, verification, evidence, and retirement history. There is no separate
history subcommand:

```sh
python3 tools/openclaw_harness/scenario_registry_cli.py registry-status
```
