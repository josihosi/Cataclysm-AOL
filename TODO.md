# TODO

Short execution queue only.

Remove finished items when they are done.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active lane: `CAOL-WRITHING-STALKER-THREAT-DISTRACTION-HANDOFF-v0`.

Current execution item: implement and prove the writhing-stalker threat/distraction overmap + reality-bubble handoff packet.

Contract: `doc/writhing-stalker-threat-distraction-handoff-packet-v0-2026-05-02.md`.

Imagination source: `doc/writhing-stalker-threat-distraction-handoff-imagination-source-2026-05-02.md`.

Handoff packet: `doc/writhing-stalker-threat-distraction-handoff-handoff-v0-2026-05-02.md`.

Required work:
- Add/refine explicit stalker threat/opportunity scoring for allied NPC count, daylight/bright light, sight coverage, damage/strike budget, cover/darkness, and zombie/hostile distraction.
- High-threat case: day/bright + same OMT or close overmap pressure + roughly three friendly NPCs must retreat/stalk instead of close loiter/pressure.
- Stalking mode should avoid sight tiles and hold roughly `3` OMTs back where overmap pathing permits.
- Night/outside/reachable-player case must not loiter indefinitely; it should attack, reposition to a better dark/covered approach, retreat, or log a concrete blocker.
- Zombie/distraction entering the player/NPC tile should raise opportunity and allow dark-square approach/strike without omniscience.
- Overmap-to-bubble handoff must carry intent/reason/strike-budget state; bubble-to-overmap writeback must preserve spent/retreat/cooldown/threat memory.
- Optional door opening only if bounded: unlocked/simple, slow/noisy/interruptible, dark/distraction/commitment gated, high-threat suppressed.

Evidence needed:
- Deterministic tests for high-threat retreat, night non-loiter/opportunity, zombie-distraction opportunity plus no-omniscience guard, anti-gnome timeout, and handoff/writeback memory.
- Current-build live/staged proof for high-threat retreat and night/window/outside non-loiter.
- Zombie/distraction live/staged proof or a precise blocker packet if that row cannot honestly close.

Non-goals: no burglar stalker, no locked-door solving, no omniscience, no closed-lane reopen by drift, no save-pack/card reruns by ritual.
