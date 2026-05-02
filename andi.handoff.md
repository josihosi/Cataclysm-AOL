# Andi handoff

Active lane: `CAOL-WRITHING-STALKER-THREAT-DISTRACTION-HANDOFF-v0`.

Contract: `doc/writhing-stalker-threat-distraction-handoff-packet-v0-2026-05-02.md`.

Imagination source: `doc/writhing-stalker-threat-distraction-handoff-imagination-source-2026-05-02.md`.

Handoff packet: `doc/writhing-stalker-threat-distraction-handoff-handoff-v0-2026-05-02.md`.

Raw live-watch note: `/Users/josefhorvath/.openclaw/workspace/runtime/caol-live-watch-20260502/writhing-stalker-live-watch-notes-2026-05-02.md`.

## Current ask

Josef live-tested the writhing stalker and found the current behavior unsatisfactory.

Bad cases to fix:
- same OMT / close overmap pressure as three NPC allies and high visible threat, but the stalker does not retreat;
- night/outside/reachable-player situation, but the stalker stands near the house/window without attacking or taking a legible move;
- existing rules do not clearly hand off overmap stalking/threat intent into the reality bubble and back.

## Required behavior contrast

- Day/bright + three friendly NPCs / high threat: retreat into stalking mode, avoid sight tiles, hold roughly `3` OMTs back where pathing permits.
- Night/outside/reachable player: attack, reposition into a better dark/covered approach, retreat, or log a concrete blocker; no indefinite garden-gnome loiter.
- Zombies/threat entering the player/NPC tile: raise opportunity and prefer dark-square approach/strike, without magical target knowledge.
- After 2-4 strikes depending on opportunity: preserve bounded burst/fade and 8+ local-tile disengage.

## Door-opening boundary

Optional only. If implemented:
- unlocked/simple doors only;
- slow/noisy/interruptible or equivalent cost;
- dark/distraction/commitment gated;
- suppressed under high threat;
- no locked doors, no clever house-clearing, no burglar/SWAT-goblin behavior.

## Evidence bar

Before closure, produce:
- deterministic high-threat retreat, stalking-distance, night non-loiter, zombie-distraction, no-omniscience, anti-gnome, and handoff/writeback tests;
- current-build staged/live high-threat retreat row;
- current-build staged/live night/window/outside non-loiter row;
- current-build staged/live zombie/distraction row, or a precise blocker packet;
- preserved existing stalker guarantees.

Do not reopen save-pack prep, zombie rider, flesh raptor, bandit lanes, or old stalker v0 history by drift.
