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

Explain the candidate fit, evidence ceiling, lifecycle, binding, and readiness. Querying never
launches. If no executable selection exists, use the returned facts to choose whether to build,
repair, create, rebind, or deliberately run an isolated zero-credit diagnosis. Do not weaken the
question or combine incompatible footing. A stale executable may support an explicitly isolated
harness diagnosis only; current-product conclusions require a source-matching executable.

For a selected playtest, the coordinator brief and matching validated charter are the execution
request. The registry token is single-use technical authority, not human permission:

```sh
python3 tools/openclaw_harness/scenario_registry_cli.py registry-launch <selection-token>
```

Launch revalidates source, executable, scenario, world, ownership, and runtime. Missing charter,
stale binding, fixture defects, or tool defects are agent-owned repair when the outcome remains in
scope. The worker may change strategy, repair, obtain fresh authority, and rerun without another
human request.

## Operate and finish

Observe current native state, choose actions, and preserve receipts and contradictions. A first
divergence is a diagnostic anchor, not an automatic stop: inspect it, repair, improvise, rerun, or
finish according to the outcome. Stop only when the claim is settled or continuation requires a
real external decision, unavailable capability, irreversible user-data risk, binding change, or
materially different owner outcome.

At the honest boundary, seal `run.witness` and call `run.finish`. State the smallest conclusion
supported by cited immutable evidence; do not invent facts or promote the evidence ceiling.
`registry-record-witness` persists the witness and `registry-review-witness` records the
coordinator's separate causal judgment.

Inspect continuity with:

```sh
python3 tools/openclaw_harness/scenario_registry_cli.py registry-status
```

Report startup, feature outcome, contradictions, evidence ceiling, and cleanup separately.
