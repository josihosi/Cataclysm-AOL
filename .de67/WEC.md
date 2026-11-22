# WEC — CAOL seeded overmap AI campaign

*User intent and language brief · DE67 Phase 1 · 2026-11-22*

## User outcome

Build a large, repeatable randomized test campaign for CAOL's overmap AI. Start with bandit, cannibal, and Mi-go travel and the economies of their camps and towers. Exercise transitions into and out of the reality bubble, overmap-driven orders inside the bubble, and the points where native movement, perception, interactions, combat, or escape determine the next action. Extend coverage across the rest of the overmap AI, including its interactions with those systems.

Follow complete journeys through travel, interruption, return, settlement, and the next decision. A camp or party should continue to behave coherently when the player crosses its path, leaves, returns, or saves and reloads. The campaign should turn unexpected behavior into reproducible DE67 work.

## Owner decisions

- **Mac mini only.** Run campaign builds, scenario execution, replays, and associated load measurements on Josef's Mac mini. Use its existing CAOL development workspace after confirming the correct checkout and current ownership.
- **A week-long campaign is acceptable.** Plan for sustained execution and meaningful coverage. Let measured throughput, coverage, and the Mac's existing workload inform the run size.
- **Camp economies** means bandit camps, cannibal camps, and Mi-go towers or encampments. Exercise each economy under its own rules.
- Coordinate with active work on the Mac. Use isolated test worlds and disposable copies of any reference saves. The campaign should preserve enough state to resume after interruption and should be able to yield resources to existing testing.

## Intended gameplay and acceptance

An overmap order gives an actor a job. The executor with physical authority determines what the actor can do at that moment. Local danger can interrupt progress. Real new information, a sustained obstruction, or an individual retreat can change the next decision. The mission retains facts the actors have actually earned.

At a crossing, track the operation's authority, each actor's physical authority, and eventual settlement separately. A scout can be locally active under an admission lease while the pair's outing remains abstract. If the second scout has yet to qualify, its offscreen travel can continue where legal. One executor pays for each actual movement interval. A rejected crossing keeps the valid source state and gives a truthful reason. Save/reload and player reversals preserve identity, positions, wounds, carried property, and earned mission progress. The home camp and its party can have different overmap activation states. Frozen intervals preserve earned progress and suspend work.

| Scenario family | Seeded conditions and transitions | Evidence at the decision point |
| --- | --- | --- |
| Bandit and cannibal travel | Vary road, field, forest, floors, route length, starting separation, injury, and an obstruction that opens or closes. Have the player cross between the actors and reverse while only one is staged. | Check each actor's position, move funding, and admission lease. Check that pair ownership commits only when qualified, watch time starts at real arrival, and the same survivors can resume or return. |
| Local execution of an overmap order | Give an actor a valid destination. Then change a door, sightline, hazard, or perceived threat just before or after the next movement decision. Include a temporary block and a genuinely impossible route. | Check the native action and its physical prerequisites. Confirm what the actor actually perceived. Observe which events cause resumption, rerouting, refusal, or withdrawal. |
| Bandit economy | Vary starting stock, staffing, competing leads, depleted or contested resources, and physical payment goods. Seed interruptions between collection, carrier movement, home arrival, and settlement. | Account for finite resource depletion, physical item custody, supply use, and the relevant return credit separately. Revalidate item access. A result is credited once to the right camp on the applicable settlement path. |
| Cannibal economy | Start across supply-pressure thresholds with different reserves and wounded or away members. Seed a forage, an empty source, and a raid with withdrawal, partial return, or casualty. | Check which named actors are available and why, what the camp consumes or recovers, and when the return can settle. A provisional survivor arrival and final credit are separate observations. |
| Mi-go travel and response | Vary signal location, visibility, vertical approaches, spreading fire, separated members, and a member's retreat. Place obstacles near the tower or target on both sides of the loaded boundary. | Trace the same resident identities from reservation to physical search, local engagement, return, or death. Check what the tower and each body could see. An individual's retreat survives a handoff while an engaged peer keeps its own intent. |
| Mi-go economy | Start near the population ceiling and a production event. Vary available residents, return and casualty timing, obstructed placement, pod destruction, loaded versus active-but-unloaded time, and save/reload. | Track slot, local, abstract, and dead representations by ID and epoch. Only valid returned bodies become available. A successful birth creates one fresh identity. Blocked placement retains at most one due event, and a destroyed source yields no replacement. |
| Mixed load | Run several sites and parties concurrently through signals, travel, handoffs, returns, and stock or population updates. Vary active density, player travel, and the size of the inactive world. | Verify that actors actually perform their work. Track completed transitions alongside throughput, turn latency, memory, and stalled or repeated work. Measure the effect of active workload and inactive population independently. |

## Randomized campaign

**Proposal.** Begin with three connected journeys, one per faction. Use a split scout crossing, a human resource or payment return, and a Mi-go response and brood comparison as early discriminators. Retain fixed regressions and use them as starting points for seeded variations. Include positive controls that can finish and negative controls that must refuse safely. Expand to other overmap AI families and mixed scenarios as coverage becomes measurable.

Each run should name its scenario, initial state, generated parameters, and game RNG seed. Keep the generated setup and the game's randomness independently replayable so that a worker can vary one while holding the other fixed. A seed can schedule an action relative to an observed event, such as saving on the first turn that exactly one scout is staged. Record the actual event before counting the run as coverage. Seeds that miss the desired transition remain visible as misses. Count distinct setups and reached transitions as well as attempted and completed runs.

Use a roughly 1,000-run Mac pilot to measure native cost and verify failure reporting. Grow toward 10,000 runs and beyond when the scenario inventory and throughput justify it. These counts are starting proposals. Use the accepted week-scale runtime to reach varied connected states, including long economic cycles and mixed loads. Select worker concurrency from Mac measurements and active workload. Preserve completed work, pending seeds, and repeatable failures across interruption.

Capture the build and data identity, scenario and seed, resolved setup or save, executed actions, actor and operation bindings, and the first wrong event. Try a fresh-process replay. If earlier seeds are needed, retain the shortest reproducing sequence found. Group repeats by the first broken invariant and meaningful state, with the affected setups retained. A disposable known-failure control should demonstrate that the runner detects, replays, and groups a break before we rely on the large run. Keep the evidence needed for an active investigation and discard disposable successful-run output.

## Current evidence and open choices

- The Windows scout pilot is prior evidence for the method. Seeds 1000–1399 passed across 24 starting-position and obstruction setups after a sampling correction. The first pass reached only six setups despite using the same number of seeds. The new test passed seed 48151. The older seed-48151 adapter modes and ordinary-movement control fail during setup at the materializer that skips retained abstract pairs. A separate retained-entry control passed. Reconcile those old controls with current staged admission during specification. [Scout test](tests/bandit_live_world_test.cpp) · [Windows runner](tests/scout_pair_seed_sweep.py) · [Seed loop](tests/test_main.cpp) · [Admission and movement](src/do_turn.cpp)
- The human economy code has separate supply consumption, finite-resource claims, return receipts, and cargo application keys. Mi-go use a population ledger and a separate native production caller. The current production caller services a pod only while its tile is loaded and supplies elapsed time only for consecutive service turns. Compare that with the existing active-site-time requirement using matched loaded and active-but-unloaded scenarios. Source inspection identified this comparison. Campaign evidence is still needed. [Human economy](src/bandit_live_world.cpp) · [Mi-go ledger](src/migo_population.cpp) · [Native brood service](src/migo_population_native.cpp) · [Current specification](.de67/FS.md)
- **Economic balance is open.** Test correctness against existing faction rules. Record replayable starvation, dispatch-frequency, and replenishment findings for an owner decision on balance changes.

## Phase 2 handoff

Reconcile this brief with the existing WEC, FS, accepted evidence, and current work on the Mac. Check the actual dev checkout and what it already contains, including whether the Windows pilot is available there. Map the existing native callers and maintained tests to the scenario inventory. Preserve prior acceptance at its demonstrated scope. Verify undefined expectations with a connected example and distinguish a test-fixture defect from a gameplay defect before assigning fixes.

Phase 1 saves this brief for review and handoff. A later DE67 phase owns the specification, implementation, and campaign launch.
