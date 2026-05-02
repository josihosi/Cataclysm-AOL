# Zombie rider close-pressure no-attack imagination source — 2026-05-02

The finished scene should be easy to picture without reading logs.

The player is inside or near a house with the zombie rider close enough to matter. The rider is not a decorative horse statue. If it has line of fire, it shoots. If walls, furniture, allies, or tight space block the shot, it moves like a mounted nightmare trying to keep pressure: short irregular hops, lateral shifts, and rough circling around the player group where terrain allows. It should not draw a perfect mathematical orbit, because that would look like a debug demo wearing a hat. It should feel like a huge mounted archer improvising around obstacles.

The player read should be: “this thing is dangerous and mobile; running straight is probably bad; cover and space matter.” The rider can have cadence and recovery, but not silence. When the decision trace says `bow_pressure` and `line_of_fire=yes`, visible attack pressure needs to exist: shot, damage, whiff, blocked shot reason, or a legible action-layer refusal. If no attack happens, the game must explain the missing bridge through code/tests, not vibes.

The rider is allowed to retreat or reposition when too close, blocked, wounded, or tactically disadvantaged, but that movement should still look like hostile mounted pressure. In close/indoor situations it should prefer aggressive repositioning over idle waiting. It should not become instant unavoidable death, wall-suicide, perfect orbiting, or polite loitering.

Josef also corrected the visible description direction during the watch. The rider should read as a towering blood-eyed figure on a six-legged horse/spider thing, carrying a gory bow of wet bones and sinews. Description polish belongs here only because it supports the same visible fantasy: mounted, impossible, fast, and not something you outrun.

Failure smells:
- repeated `decision=bow_pressure reason=line_of_fire` with no visible shot/attack/damage/action result;
- rider standing in or near the house without attacking, repositioning, or clearly failing for a named reason;
- pathing that creates a perfect orbit, corner jitter, or wall rubbing;
- tuning that only increases aggression numbers without naming the no-attack cause;
- reopening the whole zombie rider v0 lane instead of fixing this close-pressure/no-shot seam.

Review questions:
- In a close/indoor player + ally scene, does the rider visibly threaten the group within a few turns?
- If it does not shoot, does the trace name why and does it reposition aggressively?
- Does bunny-hop/circling pressure feel irregular and mounted rather than geometric?
- Do existing wounded retreat, cover/LOS counterplay, camp-light banding, and no-light controls still hold?
