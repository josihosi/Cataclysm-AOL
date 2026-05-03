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
- Current-build high-threat/allies staged-live row is green: `writhing_stalker.live_high_threat_allied_light_retreat_stalk_mcw` -> `.userdata/dev-harness/harness_runs/20260503_021310/` (`feature_proof=true`, `verdict=artifacts_matched`, green step ledger, portal clear; same-run live_plan line reports `decision=withdraw`, `reason=live_high_threat_allied_light_retreat_stalk`, `allied_support=3`, `persistent=yes`, `stalk_omt=3`).
- Gates: `git diff --check`; `make -j4 src/writhing_stalker_ai.o obj/monmove.o tests LINTJSON=0 ASTYLE=0`; `./tests/cata_test "[writhing_stalker]"`; `./tests/cata_test "[zombie_rider],[flesh_raptor]"`.
- Door opening did not land and remains optional/narrow only if later needed.

Required next work:
- Current-build night/window/outside row: reachable night/outside bad-position loiter should produce attack/reposition/retreat with anti-gnome reason, not static window décor.
- Current-build zombie/distraction row: valid local evidence plus zombie/hostile pressure on the player/NPC tile should produce dark-square approach/strike or a precise blocker, while missing evidence stays no-omniscience/no-credit.

Non-goals: no burglar stalker, no locked-door solving, no omniscience, no closed-lane reopen by drift, no save-pack/card reruns by ritual.
