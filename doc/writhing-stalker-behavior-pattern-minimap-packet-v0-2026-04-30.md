# CAOL-WRITHING-STALKER-PATTERN-TESTS-v0 — primitive minimap behavior-pattern packet

## Classification

**CLOSED / CHECKPOINTED GREEN V0**

This packet was greenlit by Josef, then Schani promoted/refined it from queued primitive minimap tests into fun-scenario pass/fail benchmarks after `CAOL-MULTI-CAMP-SIGNAL-GAUNTLET-v0` closed.

Imagination source: `doc/writhing-stalker-behavior-pattern-imagination-source-of-truth-2026-04-30.md`.

Closure proof: `doc/writhing-stalker-behavior-pattern-proof-v0-2026-04-30.md`.

## Why this exists

`CAOL-WRITHING-STALKER-v0` is closed green for first playable footing: monster JSON/spawn footing, deterministic AI substrate, live monster-plan seam, shadow/strike/cooldown proof, exposed-retreat proof, no-omniscience proof, and mixed-hostile metrics.

The missing confidence layer is not “is the monster real?” but “does the repeated-turn behavior look like stalking instead of a state machine twitching in a coat?” Josef clarified the desired feel: the stalker should attack more than once when the window stays good, then run when badly injured rather than fleeing after one successful swipe or waiting passively until damaged.

## Scope

Build a small primitive minimap / ASCII / equivalent deterministic behavior-pattern harness for writhing-stalker repeated-turn behavior.

The harness should support tiny controlled layouts with:

- opaque wall / cover / clutter cells;
- open bright exposure cells;
- player location and simple player state flags;
- writhing-stalker location, HP band, cooldown state, and last decisions;
- optional zombie-pressure / distraction marker;
- repeated turn stepping with a compact trace of decision, route, distance, strike count, retreat state, and reason.

Equivalent implementation is acceptable if it produces the same readable behavior trace and deterministic assertions. Do not spend time making a cute renderer if a small C++ unit helper or harness scenario gives stronger proof faster.

## Required behavior patterns

### Pattern A — no evidence means no beeline

A stalker behind walls/open space without believable local evidence must not target, shadow, strike, or cooldown as if it knows the player magically exists.

### Pattern B — weak/fading evidence decays

Weak or stale evidence may create interest, but it should not become permanent omniscient pursuit. Trace should show interest/latch decay, leash break, hold, or ignore rather than eternal chase.

### Pattern C — cover/dark route beats direct route

When a cover/edge route and a direct open route are both available, the stalker should prefer shadowing via cover/edge and avoid direct-line beelines.

### Pattern D — exposure/focus makes it hold or withdraw

Bright exposure, clear focus, or forced exposed approach should make the stalker hold/withdraw instead of charging because the shortest line is convenient.

### Pattern E — vulnerability opens strike windows

A nearby vulnerable player state — bleeding, hurt, low stamina, distracted/noisy, and/or zombie pressure — should open strike windows, especially near cover/clutter.

### Pattern F — repeated attacks are possible but not spammy

If the player stays vulnerable and the stalker remains healthy, the trace should show a plausible cycle:

`shadow/approach -> strike -> cooldown/reposition -> shadow/approach -> strike`

The expected result is not “exactly two attacks” or any hard-coded theater beat. The expected result is that one strike does not permanently end the hunt, while cooldown prevents same-turn or immediate repeat spam.

### Pattern G — badly injured stalker retreats

When stalker HP crosses the badly-injured threshold, it should withdraw even if the player is vulnerable. Josef’s desired feel is: a few nasty attacks if healthy and the opening persists, then self-preservation once badly injured.

Current v0 code uses `stalker.hp_percentage() <= 55` as the live hurt threshold. This packet may either prove that threshold produces the desired feel or propose a tighter threshold/tuning change with evidence.

### Pattern H — repeated turns look like stalking, not jitter

Across enough repeated turns, the compact trace should show coherent state progression: no oscillation between contradictory destinations, no permanent stuck hold with an obvious cover route, no repeated strike spam, and no instant omniscient chase.

## Non-goals

- Do not reopen monster flavor/stat/spawn footing unless a behavior test exposes a directly relevant bug.
- Do not rerun the old mixed-hostile soup as ritual.
- Do not require rich long-term nemesis memory, overmap tracking history, or a full monster blackboard for this packet.
- Do not replace the current multi-camp gauntlet as active work unless Schani/Josef explicitly promote it.
- Do not call a static decision-unit test alone enough if it cannot show repeated-turn behavior.

## Success state

- [x] A named primitive minimap/ASCII/equivalent behavior-pattern test helper exists.
- [x] The helper emits or records a compact repeated-turn trace suitable for debugging behavior feel.
- [x] Tests cover no-evidence/no-beeline, weak evidence decay, cover/edge route preference, exposure hold/withdraw, vulnerability strike window, repeated attack cadence, injured retreat, cooldown anti-spam, and jitter/stuckness smell checks.
- [x] Current code passed; evidence was recorded and behavior was left unchanged.
- [x] No tuning/implementation change was needed; no-omniscience, cooldown, and exposed-withdraw constraints remain guarded.
- [x] Focused writhing-stalker tests ran green. No fresh harness/live probe was needed because this was a test-only closure and the unchanged live seam already maps through `writhing_stalker::evaluate_live_response`.
- [x] `Plan.md`, `SUCCESS.md`, `TESTING.md`, `doc/work-ledger.md`, and `andi.handoff.md` were updated for closure.

## Evidence expectations

At minimum, final reporting should include:

- test names / command run;
- before/after code or tuning summary if changed;
- compact trace excerpt for the repeated-attack and injured-retreat rows;
- strike count and retreat trigger in the trace;
- confirmation that no-evidence and cooldown anti-spam still pass;
- any live-seam caveat if deterministic helper coverage outpaces `monster::plan()` coverage.

## Known traps

- A single `strike` decision proves the strike window exists, not that the creature behaves like a stalker over time.
- A cooldown that blocks immediate spam is good; a cooldown that accidentally ends the whole hunt is not.
- “Hurt” should mean the stalker is in danger, not that a paper cut makes the monster flee like a wet pigeon.
- Do not make it braver by making it omniscient. That would be cheaper, naturally, and therefore suspicious.
