# Test-vs-game implementation audit report packet v0 (2026-04-26)

Status: active / greenlit now.

## Normalized contract

**Title:** Test-vs-game implementation audit report packet v0

**Request kind:** Josef greenlight / active audit-and-report package

**Summary:** Josef wants the hollow-code problem attacked directly: current C-AOL tests, especially the bandit AI / camp tests, have pass conditions that may prove a deterministic seam, authored playback packet, or harness setup without proving that the same win-condition is actually wired into live gameplay. This packet makes Andi produce a concrete report that maps test claims to real game consumers, identifies deterministic-only or fake-live gaps, and names the exact next implementation package when the bridge is missing.

## Why this is active first

This is the umbrella truth-check before implementing the rest of the greenlit stack. It should prevent Andi from adding one more test-shaped trophy that proves the stage directions while the actor never enters the game. Very elegant, very useless, like a silver spoon in a soup nobody cooked.

## Scope

1. Audit the current high-risk C-AOL test lanes, prioritizing:
   - bandit camp / hostile-site / live-world tests;
   - bandit mark-generation, smoke, light, visibility, weather, horde, playback, and handoff tests;
   - Basecamp locker / armor / medical readiness tests;
   - Smart Zone Manager and zone option tests where new greenlit work touches them.
2. For each audited lane, produce a reviewer-readable mapping:
   - test file / scenario name;
   - pass condition;
   - source function/class under test;
   - live game producer, if any;
   - live game consumer, if any;
   - evidence class: unit/evaluator, deterministic playback, save serialization, harness live path, screen proof, log/artifact proof, real source hook, or public release artifact;
   - verdict: live-wired, deterministic-only, playback-only, harness-only, stale/ambiguous, or hollow/misleading.
3. Trace the important bandit smoke/light/weather path explicitly:
   - whether tested smoke/light packets respect weather and light conditions;
   - whether those packets are produced from real map fire/smoke/light in live play;
   - whether live bandit dispatch consumes those marks/leads;
   - whether horde attraction consumes visible light/fire or only existing sound/JSON effect paths.
4. Check whether the currently tested win-condition actually applies in-game:
   - locate the call path from live code into the tested logic;
   - if the path is absent, say so plainly and assign it to the relevant greenlit implementation packet;
   - if the path exists only in harness/probe setup, label the harness bias.
5. Write the report in repo canon:
   - preferred report path: `doc/test-vs-game-implementation-audit-report-2026-04-26.md`;
   - update `TESTING.md` only with the compact live validation policy and the report result, not a whole novel.

## Non-goals

- Do not implement all follow-up packages in this audit packet.
- Do not rename every old test just to look busy.
- Do not delete useful deterministic tests because they are not live proof; relabel them honestly instead.
- Do not claim a test is live-wired merely because the same file or helper name appears in production code.
- Do not use Josef playtesting as a substitute for tracing the code path.
- Do not publish releases, change repo policy, or touch Lacapult.

## Success state

- [ ] A concrete report exists at `doc/test-vs-game-implementation-audit-report-2026-04-26.md` or an explicitly equivalent path.
- [ ] The report covers the highest-risk bandit AI / camp lanes, including `tests/bandit_mark_generation_test.cpp`, `tests/bandit_playback_test.cpp`, `tests/bandit_live_world_test.cpp`, and the live dispatch path through `src/do_turn.cpp` / `src/bandit_live_world.cpp`.
- [ ] The report separately classifies smoke, light, weather, horde attraction, site bootstrap, dispatch, local handoff, and scout behavior by evidence class.
- [ ] Each audited pass condition says whether the tested logic is produced/consumed by live gameplay, deterministic playback only, harness setup only, or currently hollow/missing.
- [ ] Every hollow/missing bridge is assigned to one of the greenlit packages or marked as a new follow-up if it does not fit.
- [ ] `TESTING.md` gets a compact update summarizing the audit result and preserving the rule that tests cannot impersonate live implementation.
- [ ] The report names the first implementation package Andi should execute next after the audit.

## Suggested audit method

Start with grep/call-path inspection, not vibes:

- `tests/bandit_*`, `tests/faction_camp_test.cpp`, `tests/clzones_test.cpp` for pass conditions;
- `src/bandit_mark_generation.cpp`, `src/bandit_playback.cpp`, `src/bandit_live_world.cpp`, `src/do_turn.cpp`, `src/overmapbuffer.cpp`, `src/sounds.cpp`, and relevant map/fire/smoke/light/weather code for live consumers/producers;
- harness scenarios under `tools/openclaw_harness/` for live proof claims and fixture bias;
- `TESTING.md`, `SUCCESS.md`, and packet docs for wording that may overclaim.

## Handoff note for Andi

This is not a paperwork chore. It is the map of where the tests touch the game and where they only touch a painted cardboard door. Produce the report first, then move to the first implementation packet named by the report.