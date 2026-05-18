# Multi-camp signal gauntlet imagination source — 2026-04-30

## Lived scene

The map is no longer a tidy one-camp toy problem. Several hostile sites exist, the player or world produces tempting signals, structural bounties still exist, and time passes long enough for the live overmap systems to make choices.

A good result feels like hostile groups are alive but not stupid:
- multiple camps can notice opportunities without every camp dogpiling the same tile;
- structural bounty and live player smoke/fire/light signals coexist without one signal class overwriting all judgment;
- active groups stay bounded, explainable, and persisted instead of multiplying into invisible soup;
- a reload in the middle does not erase the plot;
- the metrics say the system stayed cheap enough to play.

This is a challenge playtest, not a feature rewrite by default. If the gauntlet breaks, the useful output is the exact weak seam: dogpile selection, stale active outing, bad priority order, save/load loss, bad no-repeat state, or performance churn.

## Failure smells

- One blessed camp works, but two or four camps turn into duplicate dispatch spam.
- Structural bounty and live signal handling both fire, but all camps pick the same target with no reason.
- Active groups disappear or duplicate after reload.
- “Stress” is only a setup audit with no bounded wait, no active outing state, and no metrics.
- The proof claims natural behavior while relying on invisible forced outcomes.
- The run is green only because it avoided the hard coexistence case.

## Review questions

- Did the gauntlet actually involve multiple camps/sites and at least two signal/bounty classes?
- Can a reviewer explain why each active camp/group chose its target or held back?
- Did save/reload preserve enough state to continue the story?
- Are cost and log spam acceptable, or did the world become a CPU goblin convention?
