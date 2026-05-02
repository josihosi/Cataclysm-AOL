# TODO

Short execution queue only.

Remove finished items when they are done.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active lane: `CAOL-WRITHING-STALKER-THREAT-DISTRACTION-HANDOFF-v0`.

Current execution item: live/staged validation for the writhing-stalker threat/distraction checkpoint.

Contract: `doc/writhing-stalker-threat-distraction-handoff-packet-v0-2026-05-02.md`.

Imagination source: `doc/writhing-stalker-threat-distraction-handoff-imagination-source-2026-05-02.md`.

Handoff packet: `doc/writhing-stalker-threat-distraction-handoff-handoff-v0-2026-05-02.md`.

Current footing:
- Deterministic implementation checkpoint is green: `doc/writhing-stalker-threat-distraction-deterministic-checkpoint-v0-2026-05-03.md`.
- Gates: `git diff --check`; `make -j4 src/writhing_stalker_ai.o obj/monmove.o tests LINTJSON=0 ASTYLE=0`; `./tests/cata_test "[writhing_stalker]"`; `./tests/cata_test "[zombie_rider],[flesh_raptor]"`.
- Door opening did not land and remains optional/narrow only if later needed.

Required next work:
- Current-build high-threat/allies row: day/bright or strong visibility + about three friendly NPCs should produce `live_high_threat_allied_light_retreat_stalk` / overmatched stalking rather than same-tile loiter.
- Current-build night/window/outside row: reachable night/outside bad-position loiter should produce attack/reposition/retreat with anti-gnome reason, not static window décor.
- Current-build zombie/distraction row: valid local evidence plus zombie/hostile pressure on the player/NPC tile should produce dark-square approach/strike or a precise blocker, while missing evidence stays no-omniscience/no-credit.

Non-goals: no burglar stalker, no locked-door solving, no omniscience, no closed-lane reopen by drift, no save-pack/card reruns by ritual.
