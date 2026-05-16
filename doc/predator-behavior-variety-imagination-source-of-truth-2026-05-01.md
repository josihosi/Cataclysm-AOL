# Predator behavior variety imagination source of truth — 2026-05-01

This source covers Josef's greenlight to change both flesh raptors and the writhing stalker away from plain stab-and-flee repetition, with deterministic map tests, live/playtest rows, and fun metrics.

## Finished scene

The player can tell two predator families apart by how the room changes.

Flesh raptors are visible circling skirmishers. They do not just stab, sprint away, and repeat like a tax form with claws. They keep 4–6 tiles when terrain allows, drift around the player toward open or under-defended arcs, avoid piling into the same zombie blob, and periodically swoop in. They are annoying because they shape space, not because they destroy your coat and call that design.

The writhing stalker is not another raptor. It is the silence on the side where the zombies are not. It uses overmap light interest plus reality-bubble sound/smell/sight evidence to stalk, gets more confident when ordinary zombies are close to the player, and waits for divided attention. If the player runs, it may already be near the escape side. If the player commits to fighting zombies, it tries to occupy the quiet/back side and strike from there. Light, focus, open exposure, and deliberate clearing of space still counter it.

## Outside-visible behavior

- Flesh raptors visibly circle or hold skirmish range before committing, especially in open space.
- Flesh raptors do not jitter every turn; they keep an orbit intention for a short cadence and fall back gracefully in corridors or blocked terrain.
- Flesh raptors may still swoop and withdraw, but the main readable pattern is circling pressure rather than binary hit-and-run.
- Writhing stalker confidence rises with zombie pressure near the player, but only when the stalker has honest local evidence or overmap interest footing.
- Writhing stalker prefers the under-occupied / quiet side of the fight: the side where the zombies are not, especially shadow/cover/cutoff tiles.
- Writhing stalker strikes when zombie pressure divides attention, not by omniscient teleport or guaranteed backstab.

## Boundaries

- Do not remove every attack-and-retreat enemy from the game. The goal is variety, not a crusade against all skirmishers.
- Do not rewrite eigenspectres/eigenwhatever as part of this package unless separately promoted.
- Do not make flesh raptors smarter by making them more equipment-destroying or more tedious.
- Do not give the writhing stalker literal player-facing mechanics unless the engine already supplies an honest seam; use zombie-pressure side, quiet side, recent movement, light/LoS, and terrain instead.
- Do not let either behavior become a jitter ballet. Hysteresis/cooldowns are part of the fantasy.

## Failure smells

- The raptor is still just `HIT_AND_RUN`, only with a different comment.
- The stalker becomes a second flesh raptor instead of an attention predator.
- The tests prove helper math but not the live monster path.
- Fun metrics only count damage or success and ignore frustration: equipment destruction, unavoidable hits, endless kiting, jitter, or no-counterplay cases.
- Open terrain is interesting but corridors become broken, stupid, or infinite dance loops.

## Review questions

- Can a player describe the difference between a raptor and stalker after one scary encounter?
- Did the implementation reduce anti-fun without sanding away danger?
- Which deterministic map test proves the intended pattern, and which live/playtest row proves it survives actual play?
- What metric would make us say “this is annoying, not fun” even if the monster is technically working?
