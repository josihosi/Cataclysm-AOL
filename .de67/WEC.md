# C-AOL Harness Cockpit

*User intent and language brief*

## User outcome

An LLM worker understands and operates live Cataclysm through one compact playtest cockpit. It
selects or creates a suitable scenario, prepares deterministic setup, sees the avatar's visible
surroundings, performs meaningful game actions, responds to changed conditions, and receives honest
evidence. It must not reconstruct gameplay from OCR, raw logs, registry tokens, frame offsets, or
incidental entities. The harness must support sustained complicated playtests and improve when runs
identify missing sight, actions, setup, or expensive recovery.

Playtesting C-AOL must also preserve proof that already exists, make failures easy to diagnose, and
make the final integrated test easier to run. The parts may be tested separately while they are
built, but final automated acceptance must prove one continuous round.

This applies to C-AOL as a whole. Hostile ecology is the first demanding vertical slice because
actors cross between the overmap and the reality bubble, survive save and relaunch boundaries, and
must still complete a long causal lifecycle.

## Interface and flow

The cockpit flow is:

```text
frontier -> scenario -> capability -> game -> run -> gap
```

Ordinary flow:

1. Read the proof frontier.
2. Search scenario SQLite.
3. Select a compatible scenario or create and validate a new scenario.
4. Prepare deterministic setup.
5. Observe visible game state.
6. Perform player actions.
7. Receive changes plus a fresh observation.
8. Continue, finish, or report a reusable capability gap.

## Initial context

The coordinator supplies only behavior under test, required evidence class, unresolved proof gap,
relevant binding, forbidden proof shortcuts, and available harness capabilities. Do not preload the
complete manual, catalog, repository history, or raw prior logs.

## Worker-facing experience

Approximate names only:

- `frontier.get`
- `capability.search` and `capability.describe`
- `scenario.search`, `scenario.describe`, `scenario.create`, `scenario.validate`, `scenario.select`,
  and `scenario.prepare`
- `game.observe`
- `game.look(target_or_area?)`
- `game.act(intent,target?,parameters?,recovery_policy?)`
- `run.status` and `run.finish`
- `gap.report(blocked_intent,missing_observation_or_action,evidence,reusable_outcome,affected_scenarios,observed_cost)`

Phase 2 chooses the smallest compatible technical mechanism. Every call returns one structured
result. The worker does not parse subprocess output for success.

## Scenario selection

The worker searches scenario SQLite. SQLite supplies scenario purpose, starting state, actors,
fixture/save identity, compatible bindings, capabilities, evidence authority, prior scoped results,
and setup limitations. The worker judges fit, selects a scenario, and records why. Reveal summaries
first and details on demand.

If no scenario fits, the worker may create and validate a deterministic new scenario. Scenario
validation proves setup only.

The agent chooses the latest compatible diagnostic capsule and records why it chose it. A diagnostic
capsule or replay has zero final-certification credit. Checkpoint segments may not be spliced
together to manufacture a passing round.

## Deterministic setup

Never depend on incidental creatures, NPCs, items, terrain conditions, or interruptions as test
preconditions. Required entities live in the loaded save or are explicitly spawned through debug
setup on a selected free tile.

The optional toolbox may support creature and NPC follower spawning, stable target selection,
items, zones, furniture, supplies, enclosed shelters, basecamps, distraction-safe targeted removal,
fire/brazier setup, `roof_on_fire`, and other reusable preparation. Every intervention is recorded
and earns no proof for manufactured state or behavior.

## Observation

After every meaningful action or interruption, return a fresh avatar-centred observation. Support
observe and look on demand.

The default observation contains a compact local map, avatar position and z-level, activity,
currently visible creatures, NPCs, items, interactables, terrain, furniture, fields, hazards,
relevant light, weather, time, active modal or interruption, available actions, and material
changes. Reveal only currently perceivable state, not offscreen global truth. A non-visible entity
has no invented explanation. Focused detail is requested as needed. Do not stream every game turn.

## Stable handles

Relevant creatures, NPCs, items, and interactable locations have stable run-scoped handles. Map
markers may change. Never silently retarget stale handles. Reject a stale handle and return a fresh
observation.

## Actions and transactions

The normal proof-bearing surface is bounded player intent such as look, move, wait, interact, talk,
target, attack, use-item, or start, stop, or continue an activity. The worker owns scenario strategy.
The harness owns native UI, input, receipts, and safe recovery.

Each transaction is:

```text
fresh precondition -> native dispatch -> expected interruption handling -> confirmed postcondition -> fresh observation
```

Success returns state changes, interruptions or responses, proof-bearing events or evidence effect,
and observation. Failure returns the first unrecovered divergence, expected and observed state,
recovery attempted, game usability, and next calls. Keep registry tokens, offsets, keystrokes, OCR
guesses, and reconciliation internal.

## Recovery

An action declares a recovery policy. The harness resolves expected safe interruptions, records
them, confirms the next state, and continues. Return control for a material gameplay choice,
unauthorized response, unconfirmed recovery, feature failure, or dishonest continuation. Do not
hide feature failure.

## Scenario macros

Scenario macros are permitted only for setup or recovery. Record every constituent action and
intervention. A macro never proves the behavior under test.

## Knowledge placement

Use a compact coordinator frontier, scenario facts and compatibility in SQLite, a queryable
capability catalog with contracts, examples, recovery, and proof effects, a selected scenario brief,
live observation, transaction, and evidence, and a durable reusable capability-gap record. Use
progressive disclosure, not full manuals upfront.

## Harness improvement

Every suggested reusable capability becomes ordinary improvement work and is preserved. Its owner
may schedule it by interrupting, finishing the current route, doing subsequent work, or combining
related work. Suggestions do not trigger mutation.

Eligible gaps include missing sight, action, or setup; repeated manual interruption or log
interpretation; duplicated state ownership; token-heavy polling; special-case workarounds; and
missing structured failures. Ordinary workers implement and validate these improvements.

## Trust the agent twice

When a worker struggles, first assume the harness or task failed to expose needed state, action, or
outcome. Prefer removing instructions and improving a reusable interface over adding rules,
warnings, or scenario-specific workarounds. Worker error is interface evidence before capability
evidence.

## Proof-preserving development and final gates

During development, agents can run small focused tests and diagnostic replays without repeating
unrelated gates that are already proven on the same compatible binding. The system records what was
proven, where the first divergence occurred, and which compatible diagnostic capsule is the best
place to investigate from.

When the implementation reaches a proven certification boundary, the automated certification gate
runs the entire required lifecycle as one bound, uninterrupted round. Ordinary save, quit, and
relaunch are allowed when they are part of that same round. Checkpoint rollback, segment splicing,
code or data changes, fixture or scenario changes, replacement worlds, and replacement player or
actor identities are not allowed.

After automated certification passes, Josef performs a separate Windows feel pass in ordinary
play. Exploratory free play may happen earlier, but it does not replace either final gate.

- **Automated certification gate** — one continuous bound round proves the complete required
  lifecycle and all named proof gates.
- **Windows feel gate** — a separate ordinary-play pass on Windows establishes whether the result
  feels understandable, coherent, and enjoyable.

Neither gate substitutes for the other.

## Continuity and binding

One certification round has one scenario lineage and one compatible binding. The binding must cover
the relevant code, data, executable, harness, fixture, scenario, world or save, player identity, and
identity-bearing ecology actors.

Across every reality-bubble crossing and every permitted save, quit, and relaunch, each
identity-bearing actor keeps the same durable identity and has exactly one authoritative simulation
owner.

Offscreen aggregate simulation may represent populations, resources, pressure, or probability. It
may not substitute for actor-level lifecycle evidence when the claim concerns a particular actor or
group completing a transition.

Any relevant binding change invalidates the certification round and requires a fresh continuous
round. It does not erase useful focused proof or diagnostic history. It only removes
final-certification credit from incompatible evidence.

## Diagnostics

A failed run reports the first causal divergence, the last proven gate, the expected and observed
states, the relevant actor identities and ownership state, the selected compatible diagnostic
capsule, and the smallest next probe. It does not flood the report with repeated transport actions
or identical log lines.

The system distinguishes setup support, build proof, synthetic proof, focused feature proof,
automated continuous-round certification, and Windows feel evidence. A built binary, startup
screenshot, helper result, or focused test earns only its own evidence class.

## Mutation boundary intent for the DFS method only

If an independent mutation occurs, the reviewer may inspect accumulated runs and gaps for
overlapping observers, dispatchers, clocks, modal recovery, targets, proof classifiers, missing
sight or actions, repeated reasoning, token-heavy recovery, and planned additions that create
special controllers. The reviewer decides keep, merge, or retire and changes DFS or ledger only.
The reviewer never changes code. The reviewer then stops, and a fresh ordinary coordinator
implements the result.

Do not place coordination, models, or mutation procedure into the product DFS if prohibited.
Preserve the underlying product ownership and interface outcomes.

## Efficiency

Reliable outcome comes first. Then reduce tokens through compact initial context, progressive
discovery, deltas instead of repeated history, event-based rather than per-turn observations,
suppressed unchanged polling, UI mechanics inside transactions, one structured result per call,
focused detail, durable SQLite and catalog knowledge, and conversion of repeated reasoning into
reusable capabilities.

Measure token cost, repeated calls, waiting, polling, and manual recovery. Token minimization must
not remove necessary truth.

## Prototype checks

- The worker searches SQLite and selects a scenario.
- The worker queries only relevant capabilities.
- The worker deterministically prepares required entities.
- The worker receives an avatar-centred observation.
- The worker performs one player action.
- An ordinary interruption is handled.
- A fresh observation returns.
- A missing expected zombie dog is simply not visible, and the worker does not wait for an
  incidental dog.
- A required dog is deterministic setup.
- A missing harness capability becomes one reusable gap.
- Later ordinary tooling work can consume that gap.
- A long activity does not stream every turn.

## Critical source-inspection requirement

Inspect the existing harness and existing in-game LLM perception binding, including
`src/llm_intent.cpp`, semantic frames, scenario registry, action dispatch, modal handling, proof
classification, tests, and planned optional additions. Identify every competing reader, writer, and
owner. Make explicit code-grounded keep, merge, or retire decisions.

Prefer reuse and shared ownership of the existing game-native visible-world snapshot over a
parallel harness perception system. The frozen DFS must combine this cockpit with the existing
hostile-ecology proof requirements rather than discard them.

## Protected LLM-intent regression boundary

This is a harness-only product change. Existing LLM-intent behavior is protected. The cockpit must
not hijack, intercept, replace, or change the LLM-intent request path, prompts, NPC snapshot
semantics, target handling, action parsing or execution, timing, or NPC behavior.

Inspect `src/llm_intent.cpp` as an existing game-native perception pattern and as a source of
neutral game-state concepts. Prefer a harness-only adapter that reads the same authoritative game
state over coupling the cockpit to the NPC LLM-intent path.

A shared lower-level observation primitive is permitted only when neutral code owns it and direct
regression proof shows that existing LLM-intent observable output and behavior remain unchanged.
The cockpit must not become a second owner of LLM-intent state.

## Project language and terminology

Use these terms consistently:

- **Cockpit** — the compact worker-facing interface for scenario discovery, deterministic setup,
  live observation, action transactions, evidence, and reusable gaps.
- **Diagnostic capsule** — a bound preserved state used to investigate from a known point.
- **Diagnostic replay** — a run from a diagnostic capsule that earns no final-certification credit.
- **Continuous certification round** — the single bound execution used by the automated
  certification gate.
- **Automated certification gate** — the machine-verifiable final integrated gate.
- **Windows feel gate** — Josef's separate ordinary-play judgment gate.
- **First divergence** — the earliest failed causal expectation.
- **Binding** — the complete identity of the code, runtime, data, harness, scenario, world, and
  actors relevant to evidence compatibility.
- **Authoritative simulation owner** — the one layer currently allowed to advance an
  identity-bearing actor.
- **Intervention** — a recorded setup or recovery mutation that earns no proof for the state or
  behavior it manufactured.
- **Capability gap** — a durable reusable record of a missing observation, action, setup, recovery,
  or structured failure surface.

Avoid calling a diagnostic replay a resume of final certification. Avoid calling assembled segments
a continuous round. Avoid calling setup validation gameplay proof. Avoid calling registry tokens,
frame offsets, key bindings, raw logs, or OCR a worker-facing gameplay observation.

## Decisions

- The cockpit is the one compact worker-facing playtest interface.
- The cockpit is harness-only and preserves the existing LLM-intent observable contract.
- The worker owns scenario strategy. The harness owns native UI, input, receipts, and safe recovery.
- The scenario registry and capability catalog use progressive disclosure.
- Required entities are deterministic setup, never incidental preconditions.
- A fresh avatar-centred observation follows every meaningful action or interruption.
- Focused tests preserve useful development proof but never replace the final continuous round.
- Diagnostic capsules are agent-selected recommendations, not automatic authority.
- Checkpoint rollback and segment splicing receive zero final-certification credit.
- Normal save, quit, and relaunch may occur inside one continuous round when the binding and
  identities remain unchanged.
- Final acceptance has two separate gates: automated certification and Windows feel.
- Hostile ecology is the first vertical slice, but the resulting playtesting system is for all
  C-AOL.
- Identity-bearing actors retain durable identity and exactly one owner across overmap,
  reality-bubble, and persistence transitions.
- Reusable capability gaps become ordinary improvement work. They do not trigger mutation.
