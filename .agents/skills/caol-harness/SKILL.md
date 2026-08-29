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

If no executable selection exists, follow the query result's `next_action`
when present. A current contradiction routes to one query-bound command:
`registry-repair-bootstrap --query-id <query_id>`. The registry re-derives the
manifest, route, red verification, original typed request, and current binding;
do not scrape or combine those identifiers manually. This authority is not an
ordinary selection token and the contradiction remains fail-closed until a
repair run supersedes it. Otherwise open the inert draft, read its one
`closest_candidate` or `create_scenario` action plus satisfied and missing
requirements, repair one scenario declaration, then rebuild, reconcile, and
repeat the same query. Never combine footing or weaken the query.

Keep this loop concrete enough for a Luna worker: requirement, observed value,
missing value, file to change, rebuild, and repeated query. Do not launch a
draft. Do not describe setup-only evidence as gameplay proof.

## Explicit selected launch and report

For a ledger item marked `Playtest witness: required`, first read the coordinator-authored charter
named in the live brief. Pass that JSON to the selected or detached launch with
`--witness-charter`. The worker owns observation, native action choice, repair, rerun, and finish;
the descriptor supplies authority plus the generic `WITNESS / FINISH` boundary only. Do not turn
the charter into a gameplay script or load a scenario-specific proof matrix.

At the honest stop condition, seal the cockpit journal with `run.witness`, then submit the smallest
cited witness with `run.finish`. Preserve contradictions and unknowns. The finalized report binds
the charter, scenario/source/executable/run authority, native observations/actions/receipts/deltas,
interruptions, cleanup, ceiling, journal digest, and witness validation. Record it with
`registry-record-witness`; coordinator judgment is a separate `registry-review-witness` event.
Neither operation may invent absent facts or promote evidence.

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
