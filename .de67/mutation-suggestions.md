# DE-67 mutation suggestion ledger

This ledger is append-only. Independent reviews and manual suggestions use ordinary Markdown, not a
fixed data schema. Every incident entry must make its short verdict easy to scan; only the current
incident requires its full diagnosis to be reread, while all earlier short verdicts are reread.

Suggested content for an incident:

- incident/task and deadline evidence;
- **Short verdict:** a compact causal label;
- **Diagnosis:** one paragraph naming the first contradicted premise and direct evidence;
- **Suggested mutation:** the guideline section and proposed change;
- disposition after coordinator review: applied, superseded, rejected, or pending, with reason.

Manual suggestions clearly say `Source: manual`. They receive consideration in the next mutation
round but no automatic authority.

A worker-finding entry says `Source: worker finding` and records expected versus observed behavior,
direct evidence, the coordinator's causal classification, and its disposition. For a DFS expansion,
also name the first contradicted DFS premise, the added red IDs, the changed mechanism/ownership/proof
sections, and why the change preserves the WEC, project language, permissions, and acceptance
strength. A worker finding is not automatically a specification gap.

## Entries

### Manual suggestion — overdefined or fragile test protocol

Source: manual

Submitted by Josef.

**Short verdict:** test overdefined / fragile protocol

Incidental dates, flags, or receipt details must not reject product evidence unless they can change
identity, the T01 verdict, or a false-green control.

Disposition: pending the next mutation round.

### Manual suggestion — missing movement observability

Source: manual

Submitted by Josef.

**Short verdict:** tooling missing

Repeated inability to observe overmap AI movement calls for the smallest useful logging or probe
capability, not identical failing runs.

Disposition: pending the next mutation round.

### Manual suggestion — coordination ownership lost

Source: manual

Submitted by Josef.

**Short verdict:** coordination ownership lost

Reading and rewriting handovers is not progress. One coordinator owns the next causal decision, and
another xhigh review requires genuinely new evidence.

Disposition: pending the next mutation round.

### T01-M1 — exact-head binary gate stopped the natural probe

Source: worker finding

Deadline evidence: `T01-M1` reported an on-time `blocker` finding before its
2026-08-10 20:33:27 CEST deadline; cumulative misses remained zero.

**Short verdict:** tooling unchecked / stale executable identity

**Diagnosis:** The first contradicted premise was that the runnable Mac executable represented the
committed preflight HEAD. The unchanged scenario reached a green gameplay HUD but the harness
correctly stopped before feature steps because the window identified `56fb35f144-dirty` while the
repository was at docs-only preflight `720e24a00a`. Source/history review found that current
`56fb35f144` already follows the jointly safe-pair gate and inclusive loaded-edge route fixes, and
the focused owner/cohesion and inclusive-edge tests passed. The missing fact is therefore natural
current-executable evidence, not a newly proved product defect or same-contract DFS gap. Direct
evidence: `build_logs/de67/T01-M1/natural-probe.log` and
`.userdata/dev-harness/harness_runs/20260810_200712/startup.result.json`.

**Suggested mutation:** No guideline mutation is due without a deadline incident. Apply the existing
Tooling check guidance by producing a clean exact-HEAD Mac executable before the next natural probe.

Disposition: applied as a tooling/evidence classification; keep `R-001` red, do not expand the DFS,
and retry only under a fresh task identity after changing executable identity.
