---
name: caol-harness
description: Query, explain, launch, operate, and audit C-AOL playtests through the authoritative registry and cockpit.
---

# C-AOL harness

## The world we are testing

This is a grim, dangerous zombie survival game. Cannibals, thugs and survivors pursue purposes
with the knowledge, abilities and fears they actually have. Absurd situations, dark humor and
desperate choices belong here; accidental silliness caused by disconnected AI systems does not.
A robbery crew should remain purposeful through scouting, confrontation, taking goods and leaving.
Preserve believable fear, exhaustion, uncertainty and failure rather than forcing every encounter
to succeed. Judge the scene as a player experiences it. Passing code tests cannot excuse a scene
that contradicts this vision; a mismatch calls for diagnosis, not an unauthorized redesign.

## Own a scenario through its outcome

Read testing recipes and setup instructions for the question they intend to answer, even when
worded as exact contracts. The chaperone designs a simple, believable test from that question:
what behavior matters, what would expose a failure, and what evidence would settle it? Distinguish
those essentials from incidental choices of location, distance, actors, equipment, sequence and
fixture. Choose the least costly informative setup, not the easiest passing case. Compare the actual
prepared scene with that premise and revise incidental parameters within setup authority when they
stop answering the question; the first plan is not a commitment. Exact boundaries remain essential
when the boundary itself is being tested.

The coordinator judges whether the evidence answers the intended question, rather than compliance
with a recipe. Explain material coverage limits; a justified setup variation needs no separate
approval or replay. Preserve genuine behavior, consequences, evidence, owner stops and input/custody
permissions. Do not simplify away the challenge, manufacture success or erase failed observations.
Bring a change to the requested product behavior or reserved permissions to the coordinator; routine
test design belongs to the chaperone and player. Reach the meaningful endpoint: a save, first injury
or partial report is not completion, while actual death, defeat or retreat can be. Use
[scenario intent](references/playtest-suite.md#scenario-intent-and-closed-categories) to distinguish
the question from its reusable recipe.

Use a coherent initial save for a full journey and a compatible checkpoint for a focused question;
a checkpoint does not prove skipped stages. Continue the same journey through ordinary save/reload.
Prefer [Josef's prepared two-brazier templates](GAME-MANUAL.md#verified-save-templates) where
compatible, preserving the chosen scenario's actors, camp, windows and route. A similarly named
fixture is not necessarily that template. Rough distances are setup targets: 31 OMT is fine for a
roughly 30-OMT journey. Use the actual distance to explain a range result, not to demand a restart.
Reuse a compatible, qualified mixed-load continuation when its source, build, population and setup
fit the question; additional load is not a replay and inactive actors do not prove active work.
[Selection and launch](references/selection-and-launch.md) covers the distinctions.

Use practical shelter, routes, fuel, sight and waiting positions. Avatar setup mutations and
teleportation may remove incidental effort; do not fabricate NPC action, perception, time or item
custody. Native behavior after a setup bypass can prove its downstream question, not the skipped
natural stage. See [setup and interactions](references/setup-and-interactions.md) and
[representative signal placement](references/smoke-light-visibility.md#representative-source-placement)
for the applicable setup and distance rules.

## Find a playtest

Browse from the checkout without launching a game:

```sh
python3 tools/openclaw_harness/startup_harness.py list-scenarios
python3 tools/openclaw_harness/startup_harness.py list-scenarios --family human-signal-journeys
python3 tools/openclaw_harness/startup_harness.py list-scenarios --search continuation
```

A family expresses a purpose; its exact variants describe distinct setups and link compatible
initial fixtures and preparation sources. Task-local continuations remain in their task context.
Some families, such as C8, have a recipe but no selectable variant or starting save. Maintained
membership is not readiness: quarantine, unknown or stale registry status, fixture availability,
current build and native qualification are distinct. Historical and diagnostic entries remain
searchable; exact identity does not bypass validation.

For exact-ID lookup, diagnostics, fixtures and focused searches, use the
[catalogue search guide](references/searching.md#search-the-playtest-catalogue). Select the exact
variant with typed requirements and follow the returned `next_action` through
[selection and launch](references/selection-and-launch.md); a browse or query is not a launch.
The cockpit also offers `scenario.browse` and exact-identity `scenario.select`.

## Live playtesting belongs to Luna

GPT-6.1 Terra normally chaperones each assigned story and spawns one direct GPT-6 Luna game owner.
Only Luna operates native input, with one owner per game/save and no extra player layer. Give Luna
the concrete scene's premise, behavior to observe, relevant setup freedoms and current save/build/input
binding, with recipe details only where useful. Luna handles ordinary
survival, setup, recovery, saves and exit without per-step approval. The chaperone owns test functionality through diagnosis and closeout: establish that the
prepared scene and essential observations can answer the question, resolve ordinary setup defects
within the assignment, and carry known missing prerequisites into the next attempt. A declared
setting or successful configuration write is not evidence that the running game emits the needed
measurements. Keep that responsibility with the chaperone rather than returning each routine
setup step for coordinator approval. The coordinator owns priorities, capacity and lifecycle, and the mutator owns
semantic working-contract changes unless delegated. Preserve original ownership and pending input;
a worker return or timeout does not prove process exit.

Keep the mutable chaperone brief current: the question, selected starting conditions, ownership,
remaining work and links to current results. Replace superseded tactics and build claims there;
retain historical evidence in its original result, not as competing instructions. Relay material
corrections to the original player through the existing message route; do not rewrite sealed packets.
Choose the observation that will distinguish the outcome before an irreversible transition—for
example, the current return forecast before settlement—using existing evidence rather than a new log.

## Operate from current controls

Use current advertised actions, not historical menu or actor IDs; collect a pending request rather
than resubmit it. Observe actual positions, time and conditions before concluding a behavior is
absent. Screenshots help locate controls, while native actions, events and saves support gameplay
conclusions. For safe mode, waiting, prompts, recovery and save/reload, follow
[live operation](references/live-operation.md). Treat maintained performance as part of the outcome: turns should generally stay below100ms.
Investigate observed exceedances and deterioration promptly, distinguishing individual turns from
window averages; correct costs caused by our code without waiting for another permission request.
Preserve useful baselines and follow through on causal evidence and verification: [performance findings](references/witness-and-finish.md#performance-findings-in-ordinary-playtests).

## Closing a game

Report demonstrated gameplay contradictions under valid conditions with their first divergence
and evidence, identifying unresolved causal attribution explicitly; ambiguous premises remain
suspected. Reporting a defect is distinct from completing diagnosis, accepting a repair or
terminating the assigned task. Preserve the assignment and lifecycle rules. A failed tactic alone
is not a harness blocker. An interrupted or incomplete attempt retains its remaining work, and a
finding does not erase any repair or retest obligation.

Judge the outcome separately from closing the process. Preserve a needed continuation save and its
owner; native save, process exit and test completion are distinct. The original Luna uses native
save/quit when useful, and the chaperone reconciles exact owned process exits and cleanup. Josef's
Mac mini authority permits closure or identity-bound termination of our own test processes without
another approval, while protecting useful data and other owners. Follow
[witness and finish](references/witness-and-finish.md) for evidence, reporting and cleanup.

## Choose the relevant reference

| Decision | Reference |
| --- | --- |
| Compose a scenario; fuel, survival, visibility or NPC behavior | [Game manual](GAME-MANUAL.md) |
| Choose an existing family or variant | [Catalogue search](references/searching.md#search-the-playtest-catalogue); [suite recipes](references/playtest-suite.md) |
| Bind a save/build, launch or resume | [Selection and launch](references/selection-and-launch.md) |
| Act, wait, collect, trade or save/reload | [Live operation](references/live-operation.md) |
| Mutations, setup, speech or item ownership | [Setup and interactions](references/setup-and-interactions.md) |
| Item activation/placement or Mac input fallback | [Item controls](references/setup-interaction.md) |
| Movement, separation or apparent nonprogress | [Behavior and movement](references/behavior-and-movement.md) |
| Stock, light and sustain a fire; check its signal | [Stocked source and useful scan](references/smoke-light-visibility.md#keep-the-source-alive-through-the-useful-scan); [fire and zones](GAME-MANUAL.md#normalized-firewood-zones-and-stock) |
| Native debug warning | [Debug error catalogue](references/debug-errors.md) |
| NPC events or missing evidence | [Evidence and diagnostics](references/evidence-and-diagnostics.md), [search map](references/searching.md) |
| Finish the result and close owned processes | [Witness and finish](references/witness-and-finish.md) |

Read the references needed for the current decision. The suite links faction, away/return and
companion recipes. Promote verified reusable knowledge in its owning reference; use existing task
context for run-specific facts rather than adding an approval gate or copying a protocol.
