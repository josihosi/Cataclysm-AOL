# CAOL-WRITHING-STALKER-THREAT-DISTRACTION-HANDOFF-v0

Status: CLOSED / CHECKPOINTED GREEN V0 / FRAU-ACCEPTED

Closure note: deterministic checkpoint and current-build staged/live proof are now captured in `doc/writhing-stalker-threat-distraction-deterministic-checkpoint-v0-2026-05-03.md` and `doc/writhing-stalker-threat-distraction-live-staged-proof-v0-2026-05-03.md`. Keep claims bounded to those evidence classes; this packet does not claim natural random discovery, broad house navigation, or door/burglar behavior.

Imagination source: `doc/writhing-stalker-threat-distraction-handoff-imagination-source-2026-05-02.md`.

Raw live-watch note: `/Users/josefhorvath/.openclaw/workspace/runtime/caol-live-watch-20260502/writhing-stalker-live-watch-notes-2026-05-02.md`.

Ruleset draft: `/Users/josefhorvath/.openclaw/workspace/runtime/caol-live-watch-20260502/writhing-stalker-improved-ai-ruleset-draft-2026-05-02.md`.

## Summary

Josef's live testing after the earlier stalker packets showed the current writhing stalker is not satisfactory. It fails both ends of the intended contrast: with three NPC allies / high visible threat it does not retreat far enough into stalking, and at night outside it can stand near a house/window without attacking or making a legible move. This packet reworks the stalker's overmap threat read, reality-bubble behavior, and overmap/bubble handoff so it retreats from prepared groups, waits at stalking distance, uses zombie/distraction openings, prefers dark squares, and never becomes static window décor.

## Scope

- Add or refine an explicit stalker threat/opportunity read that considers allied NPC count, daylight/strong light, sight coverage, recent damage, strike budget, cover/darkness, and nearby zombie/hostile distraction.
- Overmatched case: same OMT / close overmap neighborhood with about three friendly NPCs plus daylight/bright focus and no distraction should choose retreat/stalk rather than close pressure.
- Stalking mode should avoid sight tiles and hold roughly `3` OMTs back where overmap pathing allows.
- Distraction case: zombies or hostile pressure entering the player/NPC tile should raise opportunity and allow a dark-square approach/strike when evidence/pathing supports it.
- Reality-bubble behavior must consume the overmap intent instead of recomputing from zero: retreating stalk, shadowing, opportunity probe, committed ambush, or spent disengage.
- Add an anti-gnome rule: visible/near-visible loitering outside a window or bad tile must resolve into attack, reposition, retreat, or a concrete logged blocker.
- Preserve the existing burst/fade rhythm: roughly 2-4 strikes depending on opportunity, then 8+ local-tile disengage or overmap stalking.
- Door opening may be implemented only as a narrow optional escalation if needed: unlocked/simple doors, slow/noisy/interruptible, darkness/distraction/commitment gated, abort under high threat.

## Non-goals

- Do not make the stalker omniscient.
- Do not make zombies grant magical target knowledge through opaque walls.
- Do not turn the stalker into a burglar, SWAT goblin, or locked-door solver.
- Do not reopen all predator behavior variety work, flesh raptors, zombie riders, bandits, or the save-pack card.
- Do not solve natural random discovery/full ecosystem spawning unless a narrow test needs a staged-but-live overmap handoff row.

## Success state

- Day/bright + same OMT or close overmap pressure + three friendly NPCs produces retreat/stalking behavior instead of close loiter/pressure.
- Stalking/retreat behavior records or proves about `3` OMT distance where overmap pathing permits, and avoids direct sight tiles rather than hovering at the defended house.
- Night/outside/reachable player no longer produces indefinite non-action: the stalker attacks, repositions to a better dark/covered approach, retreats, or logs a concrete blocker.
- Zombie/distraction entering the player/NPC tile raises opportunity and produces dark-square approach/strike behavior without omniscience.
- Overmap-to-bubble handoff carries intent/reason/strike-budget state, and bubble-to-overmap writeback preserves spent/retreat/cooldown/threat memory.
- Existing stalker guarantees still pass: no omniscience, light/focus counterplay, injured retreat, cooldown anti-spam, bounded burst/fade, and zombie-shadow/quiet-side behavior.
- If door opening is implemented, tests prove it is narrow, unlocked/simple only, noisy/slow/interruptible or equivalently costly, and suppressed under high threat.

## Testing / evidence expectation

Deterministic tests should cover the scoring and state transitions before any live claim:

- overmatched three-allies daylight/bright control;
- night/outside reachable-player opportunity;
- zombie/distraction opportunity raise with no-omniscience guard;
- anti-window/anti-gnome timeout;
- handoff intent and writeback memory/cooldown/strike-budget preservation;
- optional door-opening gate, if implemented.

Live/staged proof should include at least:

- a current-build high-threat/allies row showing retreat/stalk rather than same-tile loiter;
- a current-build night/window or night/outside row showing attack/reposition/retreat with a reason, not static loiter;
- a zombie/distraction row showing dark-square approach/strike or an honest blocker.

Use screenshot/OCR only for visible facts. Logs/artifacts must name the behavior reasons and handoff state. Do not credit old yellow runtime-mismatch watch rows as feature proof.

## Open questions

- Exact `3` OMT stalking distance may need tuning after deterministic/pathing proof.
- Door opening is approved only as a narrow optional design line; Andi should avoid it if the core no-gnome behavior can close without door work.
