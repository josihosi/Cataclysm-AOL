# Writhing stalker threat/distraction deterministic checkpoint v0 — 2026-05-03

Lane: `CAOL-WRITHING-STALKER-THREAT-DISTRACTION-HANDOFF-v0`

## Claim

The current checkpoint implements deterministic and live-plan-seam footing for the writhing stalker threat/distraction packet, but it does **not** close the lane. The remaining closure bar is current-build live/staged proof for the high-threat, night/window/outside, and zombie/distraction rows.

## WIP decision

Kept the inherited WIP after inspection; did not revert it. The continuation fixed the dark+distraction allied-support penalty, made burst-limit withdrawal win before zombie-distraction special cases, gated zombie-only special cases away from vulnerable-player opportunity logic, and cleaned up a redundant burst-count read in `monmove.cpp`.

## What landed

- Added explicit threat/handoff intent states for overmatched stalking, shadowing, opportunity probe, committed ambush, and spent disengage.
- Kept `evaluate_threat_state` as the canonical threat/distraction state machine; `evaluate_threat` is only the live/legacy facade that normalizes compact fields before delegating to that scorer.
- Added high-threat daylight/allied retreat behavior: bright/focused target plus about three allies makes the stalker withdraw into overmatched stalking intent instead of close pressure.
- Added dark reachable anti-loiter handling: repeated bad-position night/outside reachable loiter resolves into strike/reposition/retreat logic instead of indefinite window-gnome behavior.
- Added zombie-distraction behavior that can promote valid local-evidence dark-square strike/probe without granting target knowledge when evidence is absent.
- Added monster diagnostic value handoff/writeback for intent, reason, cooldown, threat memory, stalk distance, burst count, and bad-loiter count.
- Preserved existing burst/fade, focus/light counterplay, injured retreat, cooldown anti-spam, zombie-shadow, and quiet-side coverage in the focused writhing-stalker suite.

## Gates

- `git diff --check` — clean.
- `make -j4 src/writhing_stalker_ai.o obj/monmove.o tests LINTJSON=0 ASTYLE=0` — green.
- `./tests/cata_test "[writhing_stalker]" --reporter compact` — passed all 23 test cases / 264 assertions.
- `./tests/cata_test "[zombie_rider],[flesh_raptor]" --reporter compact` — passed all 24 test cases / 268 assertions.

Local logs:

- `/tmp/caol-writhing-stalker-threat-build-final3.log`
- `/tmp/caol-writhing-stalker-threat-tests-final3.log`
- `/tmp/caol-writhing-stalker-threat-spillover-final3.log`

## Open proof / blocker packet

No current-build live/staged row is credited for this new packet yet. Existing writhing-stalker harness scenarios cover older rows (`live_exposed_retreat`, `live_shadow_strike`, `live_zombie_distraction`, `live_quiet_side_zombie_pressure`, etc.), but they do not exactly prove the new high-threat three-allies/stalking-distance/overmap-handoff contract.

Live/staged rows still required before closure:

1. High-threat/allies row: day/bright or strong visibility plus about three friendly NPCs produces `live_high_threat_allied_light_retreat_stalk` / overmatched stalking rather than same-tile loiter.
2. Night/window/outside row: reachable night/outside bad-position loiter produces attack/reposition/retreat with anti-gnome reason, not static window decor.
3. Zombie/distraction row: valid local evidence plus zombie/hostile pressure on the player/NPC tile produces dark-square approach/strike or a precise blocker; missing evidence remains no-omniscience/no-credit.
4. Handoff persistence row: save after the live response and inspect active-monster values for handoff intent/reason/cooldown/threat-memory/stalk-distance writeback.

Door opening did not land in this checkpoint.
