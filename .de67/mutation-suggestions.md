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
