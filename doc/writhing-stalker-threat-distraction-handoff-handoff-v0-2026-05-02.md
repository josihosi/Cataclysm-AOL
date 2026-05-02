# Andi handoff — CAOL-WRITHING-STALKER-THREAT-DISTRACTION-HANDOFF-v0

Status: ACTIVE / GREENLIT / NEXT ANDI LANE

Imagination source: `doc/writhing-stalker-threat-distraction-handoff-imagination-source-2026-05-02.md`.
Contract: `doc/writhing-stalker-threat-distraction-handoff-packet-v0-2026-05-02.md`.
Raw note: `/Users/josefhorvath/.openclaw/workspace/runtime/caol-live-watch-20260502/writhing-stalker-live-watch-notes-2026-05-02.md`.

## Task

Fix the writhing stalker contrast failure from Josef's live playtest:

- high threat / daylight / three NPC allies must push retreat + stalking distance;
- night/outside/reachable player must not become passive loiter;
- zombie/distraction on the player/NPC tile should create dark-square strike opportunity;
- overmap and reality-bubble behavior must preserve intent across handoff.

## Must preserve

- no omniscience;
- light/focus counterplay;
- injured retreat;
- cooldown anti-spam;
- bounded burst/fade and 8+ local-tile disengage;
- existing zombie-shadow/quiet-side behavior.

## Implementation shape

Start with deterministic scoring/state tests. Then wire the live path. Then prove staged-but-live current-build rows.

Expected state vocabulary can be whatever fits the code, but it must distinguish:

- overmatched retreat/stalk;
- watching/shadowing;
- opportunity probe;
- committed ambush;
- spent disengage.

Add anti-gnome behavior: if the stalker is visible/near-visible outside a window or bad position for too long, it must attack, reposition, retreat, or log the blocker.

## Door-opening line

Optional only. If you implement it, keep it narrow:

- unlocked/simple doors only;
- slow/noisy/interruptible or equivalent cost;
- only after dark/distraction/commitment;
- aborts under high threat;
- no locked doors and no clever house-clearing.

If door work risks widening the packet, defer it and close the core threat/distraction/handoff behavior first.

## Evidence bar

Required before closure:

- deterministic tests for high-threat retreat, night opportunity/non-loiter, zombie distraction opportunity, no-omniscience guard, anti-gnome timeout, and handoff/writeback memory;
- current-build live/staged proof for high-threat retreat and night/window/outside non-loiter;
- zombie/distraction live/staged proof or a precise blocker packet if the live row cannot be made honest;
- `Plan.md`, `TODO.md`, `SUCCESS.md`, `TESTING.md`, `doc/work-ledger.md`, and `andi.handoff.md` aligned to the final state.
