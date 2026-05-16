# C-AOL live AI performance imagination source of truth - 2026-04-28

## Finished picture

The player can keep playing while several hostile overmap intelligences are alive at once. A cannibal camp may stalk and attack at night, one bandit camp may scout and return home, another may remember a target and wait for an opening, and the game still feels like Cataclysm rather than a turn-based spreadsheet falling down the stairs.

The important lived test is not that a tiny evaluator loop finishes quickly. The important test is that a real save, with multiple active hostile sites and live signal/dispatch/local-contact work, can advance time without obvious turn-lag spikes, runaway log spam, or superlinear behavior as the number of active overmap AIs rises from one to two to three to four.

## What should be visible

A reviewer should be able to see:

- how many hostile overmap AI sites are active;
- which profiles are active, e.g. bandit camp and cannibal camp;
- which jobs are alive, e.g. scout, stalk, return-home, attack window, hold/no-opening;
- how long the measured live wait/turn window lasted in game time;
- wall-clock timing for the measured window and/or per-cadence slices;
- whether the second, third, and fourth active sites add roughly bounded work or trigger a suspicious jump;
- whether reports/logs stay compact enough to diagnose behavior without becoming their own performance problem.

## Good outcome

The good outcome is a small performance packet that says, with evidence: here is the baseline, here is 2/3/4 active overmap AIs, here are the measured costs, here are the hot paths or no-hot-path finding, and here are any bounded follow-up fixes. If a cost is ugly, the packet should make the ugliness findable instead of burying it under “probably fine”.

## Failure smells

- Turn or wait latency grows much faster than the active-site count.
- Every active site scans too much of the world every cadence.
- Signal matching, local-gate checks, pathing, or sight/exposure checks run per turn when they only need cadence/event timing.
- Debug/report text becomes a performance cost or hides the actual cost.
- A deterministic test claims performance while the live game path remains unmeasured.
- The audit optimizes code before identifying the measured bottleneck.

## Boundary

This is a performance audit and playtest packet, not a new behavior-design lane. It may add instrumentation, benchmark helpers, and small low-risk fixes only when the measurements point at them. It should not rewrite bandit/cannibal behavior, widen lore, or treat one load-only harness run as proof that live play stays smooth.
