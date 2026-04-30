# Zombie rider raw intake — 2026-04-30

## Source

Josef Discord message in `#allgemein`, 2026-04-30 23:24 Europe/Vienna.

## Exact flavor text — preserve punctuation

```text
Up on, what can only be described as, a six legged horse - or is it a spider? - a towering figure, with eyes the color of blood, holds a gory bow of wet bones and sinews.
It's movement ferocious, as the tumbling feet hasten across the terrain.
Running is out of the question.
```

Josef explicitly said the weird punctuation is an artistic choice and warned not to lose the flavor text.

## Raw concept notes

- Feature target: initial dev of release `0.3` should include the **ZOMBIE RIDER**.
- Late-game mutation timing: maybe one year is too early; maybe around two years.
- Mutation origin open question: maybe from zombie hunters, or something else.
- Movement: super fast, one of the fastest; suggested speed range `150-200`.
- They shoot.
- Combat loop: shoot and flee; flee, shoot and flee.
- They stalk, but their own threat level is much higher than the writhing stalker, so they generally attack rather than lurk forever.
- Ambient behavior: ride around and shoot.
- Grouping behavior: once two zombie riders meet, they form a band of riders.

## Initial Schani vision sketch

The zombie rider should be the late-game moment where the world stops being merely overrun and starts feeling organized by horror. It is not just a fast zombie with a ranged attack. It is mounted, mobile, predatory pressure: a blood-eyed rider on a six-legged horse-spider thing, crossing distance so fast that ordinary running no longer feels like a plan.

Unlike the writhing stalker, which is fun because it hides and chooses moments, the rider is fun because it changes the geometry of safety. Roads, open fields, long sightlines, and retreat routes become dangerous. The player should start asking: can I break line of sight, force bad terrain, get indoors, make it waste shots, damage the mount/rider enough to drive it off, or turn its speed into a mistake?

The rider should attack more readily than the stalker, but still not be a dumb turret. Good behavior smells like mounted harassment: approach, shoot, reposition, flee from bad odds, return if advantage persists, and escalate when another rider joins. A single rider is a terrifying late-game skirmisher; two riders becoming a band should feel like the map just produced a roaming catastrophe.

## Early pass/fail shape

- Preserve exact flavor text before any implementation polish.
- Establish mutation timing/origin: late-game, probably after the world has matured enough that the player has tools.
- Prove speed feels scary but not unavoidable instant death.
- Prove ranged attack has counterplay: cover, line of sight, ammo/cooldown, terrain, indoors, smoke/darkness, or forced retreat.
- Prove flee/shoot loop does not become kite-lock nonsense.
- Prove two riders meeting can form a band without spawning infinite clown cavalry.

## Addendum — overmap light pressure / rider bands

Josef added that the zombie rider should also have overmap AI attracted by light, so that in the late game careless camp light can draw riders from the wider map.

Desired product picture:

- late-game camp light is no longer just local visibility; it becomes a regional risk signal;
- a lone rider may investigate or harass from range;
- multiple riders meeting or converging should be able to form a rider band;
- if the player is careless with bright camp lighting late game, a band of riders could circle the camp;
- this should feel like mounted siege/harassment pressure, not instant unavoidable camp deletion.

Important counterplay questions:

- how far light attracts riders overmap-side;
- whether roofs/elevated lights, fires, vehicle lights, floodlights, and basecamp lighting have different pull;
- how long light memory persists after being turned off;
- how riders decide to circle/harass/withdraw rather than charging into walls;
- how rider bands avoid infinite accumulation or permanent unavoidable camping;
- how the player reads the danger before it becomes pure bullshit.

## Addendum — end-of-line mutation

Josef clarified that zombie rider should probably be an **endgame / end-of-the-line mutation**, not merely a mid/late-game annoyance.

Interpretation:

- if late zombie evolution tends toward brute-like endpoint horrors, the rider belongs at that terminal tier;
- this should be one of the map's final escalation shapes: a mature-world predator/cavalry mutation, not a year-one surprise for an under-equipped player;
- timing should likely be later than ordinary late-game specials, with year two or equivalent mature evolution as a better default than year one;
- spawn/evolution should communicate “the world has gone very wrong now,” not just “new fast archer unlocked.”

Design implication:

The rider can justify high speed, ranged harassment, overmap light attraction, and rider-band formation because it is an endpoint threat. Counterplay still matters, but the baseline expectation is endgame danger.
