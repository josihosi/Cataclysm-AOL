# Bandit structural bounty overmap completion — imagination source of truth

## Finished lived scene

A bandit camp no longer waits like a dumb trap for the player to light a beacon or walk into a tiny range. It feels like a local predatory settlement with its own sparse overmap memory. It knows that nearby woods may be worth a thin forage, nearby towns and buildings may be worth a cautious look, and visible or discovered people are the real prize. It does not know everything. It guesses, sends small parties, learns, harvests, gets scared, remembers, and stops wasting bodies on the same dead tile.

The player can be elsewhere and the world still breathes. A bandit camp with idle ready members may send one person to skim a forest edge. Another day it may send a scout to a town fringe. If that scout finds trouble, the place becomes scary in camp memory. If the outing reaches an easy target, that target’s structural value is spent. If the player later creates light, smoke, noise, route activity, or is directly seen, the camp’s attention can pivot from ordinary structural scavenging to mobile prey.

The important feeling is not that bandits become omniscient. It is that they become locally alive.

## Player-facing behavior

From the player’s perspective, the feature should create several believable patterns:

- Bandits sometimes leave camp for ordinary non-player reasons.
- Forest and town pressure exists even before the player announces themselves.
- Forests are low-value, low-commitment skims, not raids.
- Towns/buildings are more attractive but uncertain and possibly dangerous.
- Bandits stop repeatedly visiting the same already-harvested or scary OMT.
- A player/NPC sighting remains a moving target, not a permanent upgrade to the terrain underneath.
- Smoke/light/player activity can still override the ordinary structural loop by creating fresher, stronger evidence.
- A bandit group that is already out blocks dogpiling unless the later design explicitly allows multi-party pressure.

## World model

Each camp owns a sparse operational map. It is not a complete map of loot, monsters, or player location. It is a small ledger of leads:

- structural bounty: coarse place value, like forest/town/building;
- mobile bounty: player/NPC/camps/routes;
- threat: stalking-distance or scout-confirmed risk;
- status: suspected, scout-confirmed, active, harvested, dangerous, stale, invalidated;
- debounce fields: checked, harvested, losses, outcome, cooldown.

Bounty is easier to know than threat. Threat becomes real at stalking distance, when the camp has sent eyes close enough to judge whether the target is worth the risk. Threat then subtracts from effective bounty: if the danger makes the prize uninteresting, the party loses interest and does not have to step onto the tile. This is the central asymmetry.

## Source-of-truth rules

These rules are the product truth for the implementation:

1. Structural ground bounty is coarse, finite, and non-regenerating by default.
2. Forests/woods carry low structural bounty.
3. Towns/buildings carry medium structural bounty.
4. Dense urban or confirmed occupied sites may carry higher value, but also higher uncertainty/threat.
5. Player/NPC sightings are high mobile bounty, not terrain value.
6. Roads and corridors are intercept value only when activity creates a moving/intercept lead.
7. Broad map awareness may seed bounty leads but must not reveal quiet actors.
8. Threat is shorter-range and must be revealed at stalking distance by scouting/contact/close evidence, before arrival/harvest.
9. A structural target is consumed when the dispatch reaches the OMT for the first implementation pass.
10. Harvested, dangerous, recently checked, and false-lead states prevent repeated dumb revisits.
11. Camp routine traffic must not refresh its own suspicion/bounty.
12. Existing player smoke/light/signal behavior must keep working and should share the same memory map where possible.

## Failure smells

The feature is wrong if:

- bandits stand idle forever unless the player emits smoke/light;
- every nearby forest becomes a permanent high-value magnet;
- towns trigger suicidal raids before scout/risk confirmation;
- threat is known globally at the same radius as bounty;
- threat waits until arrival instead of being judged from stalking distance;
- discovered threat fails to subtract from bounty/interest and bandits keep pushing anyway;
- one camp keeps stepping on the same harvested tile every few hours;
- an outgoing scout creates fresh bounty merely by walking;
- mobile player/NPC bounty gets smeared into permanent ground bounty;
- saves lose active structural outings or camp-map memory after reload;
- performance scales with all OMTs in the overmap instead of bounded camp-local sampling;
- deterministic tests pass but the live do_turn path never consumes them.

## Review question

When this is done, the question should not be “did a unit test choose `scavenge` once?” It should be: if I let the world run, do bandit camps make small plausible local decisions, learn from them, stop repeating nonsense, and still remain bounded enough that the game does not become a turn-time compost heap?
