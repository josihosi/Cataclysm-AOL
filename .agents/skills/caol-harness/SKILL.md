---
name: caol-harness
description: Query, explain, launch, operate, and audit C-AOL playtests through the authoritative registry and cockpit.
---

# C-AOL playtests

## The world we are testing

C-AOL is a dangerous survival game with room for dark humor. Judge a crew’s actions by what its members can perceive, what they want and what they can physically do.

Start with the gameplay question. A hungry survivor looking for a pot can try another house. A scout being tested for offscreen travel has to cover that leg outside the reality bubble. Choose the setup that lets you tell the difference. Defeat, retreat or death can be a genuine outcome.

## Live playtesting belongs to Luna

**Chaperone.** GPT-6.1 Terra normally briefs one direct GPT-6 Luna per game. Give the player the question, starting save and build, who owns input, setup freedoms and what would settle the test. Verify that the running game exposes any measurements you need. Keep the current brief up to date and relay changes to the same player. Sealed packets retain the original assignment.

**Player.** Luna alone launches the live session and operates native input. Use the controls the cockpit currently advertises. Collect a pending request before submitting another. Follow the story through ordinary setbacks. If something unexpected happens, preserve the scene or observation, investigate it with the chaperone and continue while the test still answers its question. [Live controls and recovery](references/live-operation.md) · [Setup permissions](references/setup-and-interactions.md)

Debug setup can remove an incidental obstacle. Waiting out a portal storm in shelter or moving the avatar away from an unrelated threat can let a test continue. Keep ordinary sight when the measured code reads avatar vision. Never move the avatar to load a stalled offscreen party and make it advance. Test repeated bubble crossings separately with the same moving actors and route. [Movement diagnostics](references/behavior-and-movement.md) · [Storm interruptions](references/long-running-soak.md#interruptions-and-lightweight-observations)

## Find a playtest

Use the [world workshop](references/world-workshop.md) to inspect a 40-OMT radius, zoom to 20, prepare a disposable world, launch a scratch game, or keep a useful run as a searchable example.

From the checkout, browse the registry with a family or search term:

```sh
python3 tools/openclaw_harness/startup_harness.py list-scenarios --family human-signal-journeys
```

Choose a variant whose starting state fits the question. Some catalogue families describe recipes without a ready fixture. On selection, follow the returned `next_action` to bind the actual build and save. [Catalogue search](references/searching.md#search-the-playtest-catalogue) · [Selection and launch](references/selection-and-launch.md)

For evac shelter roof access, check the [locked-door and Halligan procedure](references/setup-and-interactions.md#reach-an-evac-shelter-roof).

## Prepared setups: choose by the behavior being tested

For fire, scout and robbery questions, the [prepared camp setups](GAME-MANUAL.md#josefs-two-brazier-base-save) explain which window layout, light and nearby actors to use. For a full journey, start where the behavior begins. A compatible checkpoint can answer a narrower question. Preserve the owner's original save. [Reusable scenario recipes](references/playtest-suite.md) · [Writing a scenario](GAME-MANUAL.md#compose-a-useful-scenario)

## Finishing a run

Record the starting conditions, first significant observation and what remains open. Check another path if it could have produced the same result. For example, payment after a crew reaches the player establishes the payment interaction. Offscreen travel needs evidence from the offscreen leg. For slow turns, examine individual spikes as well as the sustained timing. [Evidence and diagnostics](references/evidence-and-diagnostics.md) · [Performance findings](references/witness-and-finish.md#performance-findings-in-ordinary-playtests)

When a run ends, Luna saves a needed continuation and quits. The chaperone verifies exit of the owned game and helpers and removes disposable run output. A worker returning does not establish process exit. [Witness and finish](references/witness-and-finish.md)
