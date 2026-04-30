# Writhing stalker imagination source of truth — 2026-04-30

## Core picture

A slim, shaking figure. Grey arms wrapped around its chest, hugging its twitching and writhing body. The teeth are not the point. The eyes are: half-crazed, half-panicked, steering around in prehensile madness until the player feels watched before they feel attacked.

The writhing stalker is a rare first-generation zombie-adjacent singleton predator. It is not a bandit, not a camp, not a faction, not a Basecamp economy actor, and not a zombie brute with a prettier hat. It should feel like one wrong animal using the same world-signals as the smarter systems, but interpreting them through appetite, fear, fixation, and opportunity.

## Player-facing fantasy

The player should not mainly experience the stalker as a stat block.

They should experience:

- a glimpse near the edge of light;
- a bad feeling around town edges, trees, alleys, and clutter;
- a thing that seems to notice human evidence without becoming omniscient;
- an ambush when the player is tired, bleeding, noisy, distracted, or boxed in by zombies;
- a short violent cut/bleed strike rather than a wrestling match;
- a retreat when exposed, hurt, or fully focused;
- later dread if the player keeps feeding it signals.

The creature is allowed to be unfair-feeling in mood. It is not allowed to be mechanically unfair in information, spawning, or teleporting. Ja, horror, not tax fraud.

## What it wants

The stalker treats the overmap as scent/opportunity, not bounty/economics.

Interest sources:

- **Human/player evidence:** strongest latch source when believable and recent.
- **Night light:** strong lure when exposed, especially at building/town/forest edges.
- **Town/building edges:** good hunting clutter and shelter.
- **Forest/brush cover:** movement and approach safety.
- **Road/city margins:** transit scent, not permanent home.
- **Zombie pressure:** not a threat to the stalker by default; it can make the player easier prey.
- **Smoke/fire:** weak or indirect; smoke may imply human activity but should not be treated like food.

## Desired behavior loop

1. Spawn rarely as a singleton in plausible first-generation / weird-zombie contexts.
2. Wander through coarse overmap interest without requiring direct player bait.
3. Notice a believable human/player mark and create a personal latch with a leash.
4. Shadow indirectly through cover, darkness, building edges, or clutter.
5. Avoid direct open-sight beelines where possible.
6. Wait for opportunity: player hurt, bleeding, low stamina, noisy, sleeping/resting, crafting distracted, engaged with zombies, or stuck in clutter.
7. Strike fast with cut/bleed pressure, maybe a bite, then avoid prolonged boxing.
8. Withdraw when hurt, exposed, brightly lit, or directly focused.
9. Re-latch only if the player keeps producing evidence; otherwise cool off and vanish back into the world.

## Stat and combat feel

Initial target band, subject to tuning:

- HP: 85–100
- speed: 115–125
- dodge: 4–5
- armor: low
- attacks: cut/stab/bleed pressure, light bite only if useful
- morale/AI: predator-skittish, not berserker-stupid
- count: singleton; repeated encounters should be rare and legible

It should be scary because of timing, approach, and retreat. If it wins by being a fast meat wall, the design failed.

## Architecture shape

The bandit structural-bounty work is an inspiration, not a parent class.

Shared lower-level substrate may eventually exist:

- overmap interest marks;
- light/terrain/human route inputs;
- freshness and confidence;
- bounded scan/cadence/performance rules.

But interpretation differs:

- bandits read interest as bounty/risk/manpower economy;
- the stalker reads interest as scent/appetite/fixation/opportunity;
- zombies around a target may increase stalker opportunity rather than threat;
- no camp reserve, no harvesting, no shakedown, no faction dispatch, no camp-map economy.

## Hard non-goals

Do not implement:

- a bandit camp profile with stalker labels;
- omniscient permanent player tracking;
- teleporting ambushes;
- common spawn-group spam;
- a tanky boss monster;
- Basecamp/economy interactions;
- exact global monster/loot scans;
- broad milestone rewrites before a bounded playable v0 exists.

## V0 success picture

A green v0 means:

- the monster exists with the intended silhouette/stat/combat threat;
- spawn rarity/singleton rules prevent ordinary clutter spam;
- deterministic tests prove latch/opportunity/retreat rules at the AI-decision seam;
- live/harness playtest proves at least one believable stalk-and-strike scene from a real save;
- negative live proof shows no omniscient beeline or immediate repeat spam;
- documentation names what remains future-only instead of pretending the nightmare has learned opera in one sitting.
