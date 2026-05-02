# Andi handoff — CAOL-WRITHING-STALKER-HIT-FADE-RETREAT-DISTANCE-v0

Classification: GREENLIT / QUEUED after active camp-locker unless Schani/Josef explicitly promotes it.

Read first:
- Contract: `doc/writhing-stalker-hit-fade-retreat-distance-packet-v0-2026-05-02.md`
- Imagination source: `doc/writhing-stalker-hit-fade-retreat-distance-imagination-source-2026-05-02.md`
- Runtime notes: `/Users/josefhorvath/.openclaw/workspace/runtime/caol-live-watch-20260502/writhing-stalker-live-watch-notes-2026-05-02.md`

Core desired feel: stalker attacks in a short stress-shaped burst, about 2-4 attacks in favorable/high-stress conditions, then backs off about 8+ tiles when space allows. Reference the original flesh raptor hit-and-run feel, not the newer circling raptor retrofit.

Scope:
- check original flesh raptor hit-and-run behavior as reference;
- reproduce/minimize current stalker burst/retreat behavior;
- implement meaningful post-burst retreat distance;
- make light/allies/counterpressure reduce persistence and increase retreat caution;
- prove no A/B near-player oscillation after burst.

Non-goals:
- no whole stalker redesign;
- no new raptor-style orbiting;
- no constant melee spam;
- no harmless always-flee stalker;
- preserve no-omniscience, injured retreat, light/focus counterplay, and zombie-shadow/quiet-side guarantees.

Success bar: reference note + deterministic tests + fresh clean live/handoff proof if claiming in-game behavior.
