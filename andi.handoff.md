# Andi handoff

Active lane: `CAOL-WRITHING-STALKER-THREAT-DISTRACTION-HANDOFF-v0`.

Contract: `doc/writhing-stalker-threat-distraction-handoff-packet-v0-2026-05-02.md`.

Imagination source: `doc/writhing-stalker-threat-distraction-handoff-imagination-source-2026-05-02.md`.

Handoff packet: `doc/writhing-stalker-threat-distraction-handoff-handoff-v0-2026-05-02.md`.

Raw live-watch note: `/Users/josefhorvath/.openclaw/workspace/runtime/caol-live-watch-20260502/writhing-stalker-live-watch-notes-2026-05-02.md`.

## Current ask

The deterministic implementation checkpoint and current-build live/staged proof packet are green; next move is Frau review / closure readiness, not another ritual rerun.

Deterministic checkpoint: `doc/writhing-stalker-threat-distraction-deterministic-checkpoint-v0-2026-05-03.md`.

Live/staged proof: `doc/writhing-stalker-threat-distraction-live-staged-proof-v0-2026-05-03.md`.

Credited rows:
- high-threat/allies retreat/stalk: `writhing_stalker.live_high_threat_allied_light_retreat_stalk_mcw` -> `.userdata/dev-harness/harness_runs/20260503_021310/`;
- zombie/distraction shadow-then-strike: `writhing_stalker.live_zombie_distraction_mcw` -> `.userdata/dev-harness/harness_runs/20260503_025148/`;
- night/outside anti-gnome bad-loiter strike: `writhing_stalker.live_anti_gnome_bad_loiter_mcw` -> `.userdata/dev-harness/harness_runs/20260503_025712/`.

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

Agent-side evidence is green:
- deterministic checkpoint covers high-threat retreat, stalking-distance intent, night anti-loiter, zombie-distraction/no-omniscience, handoff/writeback tests, and preserved stalker guarantees;
- staged/live high-threat, night/outside anti-gnome, and zombie/distraction rows are all feature-path green with portal clear;
- gates: `git diff --check`; harness pycompile/JSON validation; `make -j4 tests LINTJSON=0 ASTYLE=0 && ./tests/cata_test "[writhing_stalker]" --reporter compact`; `./tests/cata_test "[zombie_rider],[flesh_raptor]" --reporter compact`.

Do not reopen save-pack prep, zombie rider, flesh raptor, bandit lanes, or old stalker v0 history by drift.
