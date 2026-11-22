# Human todo

1. **Meet a trade caravan on the road.** The commune sends one merchant and one guard on foot to the Refugee Center with a finite load of food. Use the Free Merchant dialogue and ordinary barter. You can meet the same people on the road, trade, and follow them to their destination. If they leave with six tins and you buy two, the remaining four reach the center, even across a save and reload. A radio rendezvous can come later.
2. **Let selected faction camps send strong scavenging parties.** Each participating settlement uses its own reports of likely loot and observed danger to choose a tile. Send a party strong enough for the risk while keeping a reserve at home. Reuse the [bandit dispatch planner](../src/bandit_live_world.cpp) and [shared party travel](../src/overmap_party_travel.h). Extend `claim_finite_resource_units` so all camps draw from the same tile. A successful expedition depletes its loot and zombie population once. Surviving members carry the haul, and the settlement receives it when they return.

   - **Never generated.** Save the visit so first map generation produces fewer eligible items and zombie spawns. Account for any zombies already present as overmap entities when the party clears them.
   - **Generated and offscreen.** Take from the items that remain after earlier visits and resolve combat against the actual stored zombies, including lightweight horde entities. Reuse the [camp patrol combat calculation](../src/mission_companion.cpp) with each target counted once.
   - **Player present.** Let the NPCs fight and collect items in ordinary play. If the player leaves mid-visit or reloads, continue from the surviving zombies and remaining goods. Hordes arriving later can make the tile dangerous again.

3. **Tune how far overmap factions hear loud activity.** Start with existing gunfire and one proposed sustained source, a running diesel engine. Compare a quiet vehicle with the diesel on the same road. Check where a nearby camp investigates, where a distant one hears nothing, and what sustained noise costs in turn time.

[Player suggestions and release discussion](https://www.reddit.com/r/cataclysmdda/comments/1y2l8mw/cataclysm_arsenic_and_old_lace_v200/)
