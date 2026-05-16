# Bandit camp-map ecology source of truth (2026-04-28)

Status: SOURCE OF TRUTH FOR BANDIT CAMP-MAP MECHANIC

This is the product picture to keep returning to while implementation details get messy.
Do not treat dispatch/risk/reward math as the core mechanic by itself. Dispatch is just the hands. The per-camp map is the creature.

---

Yes. This is the right correction: **bandit AI should not be “NPCs deciding jobs”; it should be a hungry camp reading its own dirty little map.** That’s the mental model.

What I remember from the last two weeks, condensed:

- **Each camp is independent.** Own map, own memory, own hunger, own risk tolerance. Other bandit camps are mostly *threat-bearing neighbors*, not allies.
- **Bounty is broad. Threat is close.** They smell value before they understand danger. Houses, camps, smoke, light, human movement: “maybe worth something.” Threat needs closer confirmation.
- **Ground bounty is finite.** A looted farmhouse is less interesting forever unless new activity returns.
- **Human/basecamp bounty refreshes.** People keep making value: food, tools, noise, light, routines, cargo, movement.
- **Threat is sticky.** It does not decay just because time passes. It changes when someone gets close enough to recheck.
- **Signals are clues, not magic truth.** Smoke/light mean “something worth checking,” not “player here with exact inventory.”
- **Weather/terrain conceal both sides.** The player can hide, but approaching bandits can also become harder to read.
- **No invisible catastrophe.** Pressure may build, stalking may happen, tolls may happen, raids may happen — but the player should feel the buildup, not wake up to offscreen spreadsheet murder.
- **No suspicion spiral.** Bandits shouldn’t turn their own patrol churn into fresh proof forever.
- **No mega-blob heatmap.** Distinct camps, roads, city edges, and human sites should not collapse into one fake giant target.
- **When hungry and ignorant, they explore.** Not random wandering — deliberate widening of the camp’s known world.

Now the picture.

A bandit camp is a little animal on the overmap.

It has a den. Around the den is a circle of confidence: close places they know, farther places they suspect, dark places they have only stories about. Their “map” is not the player’s map. It is a greasy table map with charcoal marks: *food here once*, *lights seen there*, *bad guns there*, *two people passed this road*, *smoke north in rain*, *don’t go back to that cabin unless desperate*.

Every so often, the camp looks outward. Not constantly. Not omnisciently. The farther something is, the less often it gets reconsidered. Close surroundings tick often; far edges tick rarely. That makes the camp feel like a living place with attention, not like the game running a cursed satellite feed.

What it sees first is **bounty**.

A house cluster on the edge of town. A smoke plume. A lit window at night. A human camp’s repeated movement. A convoy route. A survivor carrying goods. These are not yet “attack targets.” They are smells. Promises. The bandits don’t know if there are five armed guards behind the curtain. They just know: *there may be something there*.

So they send a scout.

The scout is not a raid. The scout is the camp extending an eye. He creeps close enough to turn a vague bounty mark into a better mark. From far away, the camp saw “value.” From closer up, the scout may learn “value plus threat,” or “empty,” or “already stripped,” or “too dangerous,” or “humans with routine.” He watches. Maybe from two OMT out. Maybe half a day. He is not there to win. He is there to come home with a better lie than the camp had yesterday.

If it is ordinary loot — a cabin, shed, rural store, abandoned vehicle — then the loop is simple and brutal. Scout confirms. A small party goes. They take the stuff. It comes home. The mark changes: *harvested*. The map gets quieter there. That bounty is gone unless something new happens. Nice, clean ecology. The world is being eaten.

If it is a human place, the loop becomes more interesting, because humans are not a chest.

A human camp keeps producing reasons to care. Light at night. Smoke in the morning. NPCs walking routes. A vehicle returning. Stored goods. Fresh corpses maybe. Noise. Defensive improvements. The bandit map doesn’t simply say “looted.” It says: *this place lives*. That means bounty refreshes — not magically as loot spawning, but because human routine creates continuing opportunity.

Threat, though, is different. Threat is not smell; threat is scar tissue.

If a scout sees guns, traps, numbers, defenders, zombies tangled around the site, or loses a buddy, that threat mark sticks. The camp does not forget because three days passed. They may be hungry, angry, desperate, or opportunistic, but the map still says: *bad place*. To downgrade that, someone has to actually look again. Maybe the guards left. Maybe the fire went out. Maybe zombies broke the perimeter. Maybe the player drove away. Until then, old fear remains on the map.

So the camp’s decisions become a conversation between hunger and fear.

A fat camp with stores and wounded men does not raid a bright defended base because “signal detected.” It waits, scouts, taxes weak movement, shadows routes, maybe sends one man to watch. A starving camp with low stock and no better marks may risk more. A large camp can spare a stalk party. A tiny camp can only scout or steal. A camp with men already outside should not empty itself like an idiot. Sehr gut, we invented logistics, civilization collapses immediately.

The really good part is expansion.

When the camp’s map is empty and its stores are low, it starts pushing outward. First the nearby known edge. Then a little farther. Then road corridors. Then buildings just beyond normal attention. It is not “random NPC wander.” It is hunger drawing circles. The player may not see this directly at first, but eventually the signs appear: footprints, a missing stash, a scout at distance, a camp that suddenly knows about the farm road because its map expanded yesterday.

This gives the game a beautiful rhythm:

1. **The world has quiet value.**
2. **Bandits notice value before danger.**
3. **They send eyes.**
4. **Eyes make memory.**
5. **Memory changes future behavior.**
6. **Loot disappears when eaten.**
7. **Living humans keep making new marks.**
8. **Hungry ignorance expands the frontier.**

That’s the mechanic.

Not “bandits attack player.”
Not “AI director spawns enemies.”
But: **hostile camps develop local knowledge, exploit the world, remember pain, and widen their hunting ground when the cupboard is empty.**

The player-facing fantasy should be:
“I am not fighting random bandits. I am sharing a region with another hungry intelligence. If I make light, smoke, routes, stockpiles, and patterns, their map learns me.”

That’s much stronger than the current plan wording. The plan should probably be rewritten around **camp map ecology** first, and dispatch/risk/reward second. Dispatch is just the hands. The map is the creature.
