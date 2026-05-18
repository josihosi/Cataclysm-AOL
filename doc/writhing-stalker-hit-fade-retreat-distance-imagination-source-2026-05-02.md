# Writhing stalker hit-fade retreat distance imagination source — 2026-05-02

The finished scene is a stalker that commits in bursts, then actually leaves breathing room.

The player should feel a short horrible sequence: the writhing stalker closes, strikes once or a few times depending on pressure, then backs off far enough that the room changes. It should not step one or two tiles away and hover like a nervous shop assistant. The current taste target from Josef is at least about 8 tiles of retreat after the attack sequence, using the original flesh raptor hit-and-run feel as reference — explicitly not the newer circling flesh-raptor retrofit.

Stress should shape persistence. In bright light, with allied NPCs, or under strong counterpressure, the stalker should probe less and disengage sooner/farther. In darkness, distraction, zombie pressure, or high target vulnerability, it can commit harder: roughly 2-4 attacks before the retreat, not endless melee spam. The retreat should be visible and readable as predator caution, not random pathing or cowardice.

This packet also needs a reference check against original flesh raptor behavior. The point is not to clone raptors wholesale. The point is to recover the old hit-fade rhythm: attack pressure, then meaningful withdrawal distance, then possible later re-engagement.

Failure smells:
- retreat after striking is only a tiny shuffle;
- the stalker oscillates A/B near the player instead of disengaging;
- light plus multiple allies does not increase caution or retreat bias;
- the fix turns stalker into constant melee spam or a harmless runner;
- the implementation copies the new circling raptor pattern instead of the old hit-and-run feel;
- old raptor reference is asserted from memory rather than checked.

Review questions:
- After a burst, does the stalker back off about 8+ tiles when space allows?
- Does pressure/stress change the number of attacks before retreat?
- Does light plus allied support cause faster/farther disengagement?
- Does the result feel like old flesh-raptor hit-and-fade rather than new orbiting skirmisher behavior?
