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

### Manual suggestion — on-time findings bypass mutation cadence

Source: manual

Submitted during the T01 acceptance watch.

**Short verdict:** worker-finding lane became a mutation escape hatch

**Diagnosis:** `T01-M1` through `T01-M4` all ended on time with `R-001` still red and zero miss
units. The findings were not identical noise—two produced bounded product checkpoints and each named
a changed causal seam—but the deadline-only mutation cadence therefore cannot fire even after four
unresolved attempts on the same claim. A coordinator can remain permanently busy and apparently
healthy by converting every incomplete task into an on-time finding.

**Suggested mutation:** Add a progress-stagnation review independent of deadline misses. A worker
finding remains useful only when the coordinator names the contradicted premise, preserves any
accepted frontier, and changes the next causal route. Repeated findings on one red claim without an
accepted DFS transition must not authorize an unlimited retry loop; use an owner-supplied run fuse
when one exists, and otherwise require a mutation review before repeating the same route. Do not
turn incidental metadata or longer handoffs into the progress test.

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

### T01-M2 — natural homeward motor split the exact pair

Source: worker finding

Deadline evidence: `T01-M2` reported an on-time `unexpected` finding before its
2026-08-10 20:29:43 CEST deadline; cumulative misses remained zero.

**Short verdict:** competing per-member movement escaped pair cohesion

**Diagnosis:** The first contradicted premise was that releasing the assembled pair to the homeward
motor preserves coherent paired travel until the atomic boundary crossing. On clean exact binary
`4abe539461+SDL3`, the unchanged scenario naturally dispatched members 4/5, crossed outbound,
dematerialized, and rematerialized the same return at `(164,34,0)`. The selector then admitted a
jointly reachable boundary pair, but the ordinary per-member motor moved member 4 roughly 119 map
squares to its departure while member 5 remained at its original return staging square. Current
source explicitly treats assembly as a one-time gate and retains `cohesion_assembled` throughout
homeward travel, so the cohesion planner issues no regroup order after the split. The atomic commit
correctly refused to fire because both members were not at their departures. This is an
implementation gap under the existing DFS ownership/cohesion contract, not a DFS specification gap.
Direct evidence: `build_logs/de67/T01-M2/return-owner.selector.log` and
`.userdata/dev-harness/harness_runs/20260810_201057/probe.report.json`.

**Suggested mutation:** No guideline mutation is due without a deadline incident. Replan the product
owner so the exact pair cannot make unbounded independent homeward progress before its atomic
boundary transition, and add a focused control reproducing the asymmetric-arrival route.

Disposition: applied as an implementation classification; keep `R-001` red, preserve the unchanged
scenario and identities, and require changed source plus the focused asymmetric-arrival control
before another natural probe.

### T01-M3 — homeward abstract resume never reacquired the loaded owner

Source: worker finding

Deadline evidence: `T01-M3` reported an on-time `unexpected` finding before its
2026-08-10 20:54:05 CEST deadline; cumulative misses remained zero.

**Short verdict:** homeward materialization/recenter gate remained unresolved

**Diagnosis:** The first contradicted premise was that the unchanged initial window would hand the
assessed exact pair back from abstract return to a loaded homeward owner after the new bounded-pair
motor passed focused proof. Exact commit `0d082eda34` naturally completed the observing handoff,
paired ingress crossing, and observing dematerialization, then entered `returning_report` and
`returning_home`; however every remaining production attempt rejected materialization with `loaded
bubble lacks paired entry or staging positions`. The same rejection preceded the later successful
handoff in T01-M2, so the new cohesion patch is not source evidence of an observing regression.
Current source can scan a later route/recenter candidate only behind resume-state predicates, while
the terminal rejection does not emit those predicates or the initial entry/staging counts. This is
an implementation plus tooling gap already covered by the DFS recentered-transfer proof route, not
a new DFS mechanism. Direct evidence: `build_logs/de67/T01-M3/live-owner.selector.log` and
`.userdata/dev-harness/harness_runs/20260810_203544/probe.report.json`.

**Suggested mutation:** No guideline mutation is due without a deadline incident. Preserve the
focused bounded-pair candidate, add the smallest materialization discriminator for resume flags and
pair counts, and fix the normal assessed-return recenter gate only if those source-grounded facts
uniquely identify it before rerunning the unchanged scenario.

Disposition: applied as an implementation/tooling classification; keep `R-001` red and retain
`0d082eda34` as a coherent checkpoint because it closes the independently reproduced pair-split
mechanism without claiming the natural T01 verdict.
