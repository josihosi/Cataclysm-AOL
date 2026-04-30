# Writhing stalker behavior-pattern imagination source of truth — 2026-04-30

## Finished scene

A survivor crosses a small broken patch of night terrain. There is a wall, a strip of cover, some open light, maybe zombies making the wrong kind of noise. The writhing stalker does not sprint in from offscreen like it read the save file. It lingers where it has reason to linger, slides through darker or cluttered approaches when those exist, holds when the player has it exposed, and commits when the player becomes weak, distracted, bleeding, tired, or trapped by other pressure.

When the opening appears, it can strike more than once. Not machine-gun nonsense. A cut, a reposition, a breath of dread, then another cut if the victim is still vulnerable and the creature is still healthy. Once the stalker is badly injured, the mood flips: self-preservation wins, and it pulls away instead of dying heroically for a cheap extra bite. Nasty, not stupid. A predator, not a kamikaze with elbows.

## Outside-visible behavior

The readable pattern over repeated turns should be:

- no evidence: no magic beeline;
- faint evidence: interest that fades or leashes instead of eternal certainty;
- cover available: shadow route preferred over open direct route;
- bright exposure / player focus: hold or withdraw;
- player vulnerability / zombie distraction: strike window opens;
- after strike: cooldown/reposition blocks spam but does not erase the hunt;
- healthy stalker plus persistent opportunity: another attack can happen;
- badly injured stalker: retreat overrides greed.

## Product boundary

This is not a full nemesis-memory design. It does not need overmap stalking history, named grudges, global pursuit, or authored horror beats. The target is a small deterministic behavior picture that can be tested before live play turns it into fog.

## Failure smells

- The stalker attacks once, then effectively becomes done even while healthy and the victim remains vulnerable.
- The stalker waits passively until damaged, then flees, producing no predator rhythm.
- The stalker repeats strikes with no cooldown or repositioning breath.
- The stalker flees from trivial damage and feels cowardly rather than cunning.
- The stalker beelines without evidence or takes open direct routes despite available cover.
- The repeated trace jitters between contradictory choices instead of producing a readable hunt.

## Review questions

- Does the trace tell a coherent little horror story without hand-waving?
- Can a reviewer see exactly when/why the second attack becomes legal?
- Can a reviewer see exactly when/why injury overrides aggression?
- Did the implementation preserve no-omniscience and anti-spam guardrails while making the creature nastier?
