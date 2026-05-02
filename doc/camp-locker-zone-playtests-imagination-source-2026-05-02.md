# Camp locker zone playtests imagination source (2026-05-02)

The finished scene is not another refactor receipt. Josef can watch or review a small set of camp-locker situations and believe the actual zone feature behaves like something a player can use: a CAMP_LOCKER zone contains useful stock, the camp worker treats that zone as locker supply, forbidden/no-pickup overlap is respected, and weather or equipment pressure produces visible sensible outcomes instead of only passing helper math.

The player-facing picture is practical: “I made a locker zone; my camp worker used the right stuff, left the wrong stuff alone, and the evidence is visible enough that I do not need to read C++ tea leaves.” The proof should separate deterministic service checks from live/harness footage, saved-state inspection, screenshots/OCR, and any Josef manual play card.

This packet should feel like a product check on the locker-zone feature, not a new wardrobe opera. If the live harness can drive the zone manager or existing locker scenarios reliably, use it. If the harness blocks on UI or fixture limits, make that a named blocker and write a concise Josef playtest recipe instead of pretending a unit test proved the on-screen zone path.

Good outcome smells:
- existing locker-zone scenarios run on the current build without stale-binary ambiguity;
- at least one proof touches real CAMP_LOCKER zone stock, not only a synthetic helper list;
- one row checks boundary behavior, especially NO_NPC_PICKUP / non-locker stock not being swallowed as locker supply;
- one row checks the practical “worker got better / carried junk got cleaned / weather gear made sense” result;
- screenshots or saved-state audits name the visible or persisted fact they prove;
- Josef gets a short manual card only for the parts that truly need human taste or currently blocked GUI driving.

Bad outcome smells:
- “all [camp][locker] tests passed” used as a substitute for zone playtest evidence;
- raw JSON or fixture metadata laundered into a player-facing claim;
- broad basecamp redesign, Smart Zone work, NPC fashion tuning, or more anti-redundancy archaeology sneaking into the packet;
- replaying old stale runs without proving current build/load footing;
- leaving Josef with log archaeology instead of a short playtest recipe.
