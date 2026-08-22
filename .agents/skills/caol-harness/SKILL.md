---
name: caol-harness
description: Query, explain, explicitly launch, and audit C-AOL harness scenarios through the authoritative registry CLI.
---

# C-AOL Harness Registry

Use this project-scoped skill for a registry-backed harness scenario. The
registry CLI is the sole owner of scenario projection, typed selection, launch
authorization, report ingestion, and lifecycle/history inspection.

## Mandatory playtest mutations

Apply the harness mutation policy to every playtest. This is durable harness
infrastructure, not claim-specific ledger work:

- every playtest requires `DEBUG_LS` and `DEBUG_NOTEMP`;
- a non-combat playtest additionally requires `DEBUG_STAMINA` and
  `DEBUG_CARDIO`;
- an observer-character playtest additionally requires `DEBUG_CLAIRVOYANCE`
  and `DEBUG_NIGHTVISION`.

Derive the exact set from the scenario's declared `run_class` and
`observer_character`; do not infer it from the current ledger or from what the
source save happens to contain. A scenario fixture must apply the derived set
with a `player_mutations` save transform. Before launch, inspect the installed
save and require every derived mutation to be present. A legacy scenario that
does not declare enough information to derive and verify its set must be
repaired before it is selected for a playtest; legacy status is not an
exemption.

These mutations prevent irrelevant player needs, temperature, stamina, or
perception limits from interrupting the route under test. They are setup
support, not feature evidence. Existing hunger, thirst, sleepiness, or
temperature state does not invalidate external-world gameplay proof and must
not reopen, erase, or downgrade an otherwise valid run. Do not turn those
player-needs values into proof gates unless a scenario explicitly tests the
player condition itself.

## Query before launch

Run the CLI from the repository root. When the scenario declarations need to
be projected, use `rebuild`; when existing report bindings need their current
owners recomputed, use `reconcile`:

```sh
python3 tools/openclaw_harness/scenario_registry_cli.py rebuild
python3 tools/openclaw_harness/scenario_registry_cli.py reconcile
```

Before search, translate the test into observable requirements. Use existing
capability keys when they describe the requirement. Use `declared` for fixture
or setup facts. Use `run-verified` only when the query requires behavior that a
previous run already proved. Submit the typed query with `registry-query`:

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
`selection_token`.

If no executable selection exists, open the inert draft. It identifies either
one `closest_candidate` or a `create_scenario` action. Read
`satisfied_requirements` and `missing_requirements`. Do not combine footing
from several scenarios. Add or repair one scenario declaration so it contains
the missing capabilities, exact setup, and preflight checks. Then run
`rebuild`, `reconcile`, and the same query again. A missing capability key is a
scenario-authoring gap, not permission to weaken the query.

Keep this loop concrete enough for a Luna worker: requirement, observed value,
missing value, file to change, rebuild, and repeated query. Do not launch a
draft. Do not describe setup-only evidence as gameplay proof.

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
