# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active target: `CAOL-WRITHING-STALKER-v0` / playable singleton predator v0.

Links:
- receipt: `doc/work-ledger.md`
- raw intake: `doc/writhing-stalker-raw-intake-2026-04-30.md`
- imagination source: `doc/writhing-stalker-imagination-source-of-truth-2026-04-30.md`
- contract: `doc/writhing-stalker-behavior-packet-v0-2026-04-30.md`
- testing/playtest ladder: `doc/writhing-stalker-playtest-ladder-v0-2026-04-30.md`
- mixed-hostile performance playtest: `doc/mixed-hostile-stalker-horde-performance-playtest-v0-2026-04-30.md`

Current state boundary: monster/stat/spawn footing, deterministic stalker-interest/latch/approach/opportunity/withdraw substrate, first live monster-plan seam wiring, split live spawn/target footing, exposed/focused withdrawal scene proof, and midnight vulnerable-player shadow/strike proof are implemented and validated. `monster::plan()` consumes `writhing_stalker::evaluate_live_response()` for `mon_writhing_stalker` local-evidence gating, shadow positioning, strike targeting, and withdrawal/cooldown routing; same-run harness proof now shows a debug-spawned stalker persists in saved `active_monsters`, acquires the player, reaches the live `live_plan` seam, withdraws from noon-bright/focused contact, and in a separate midnight vulnerable-player scene shadows before striking then cools down. V0 adds no new persisted latch state; it reuses existing monster effect state for short cooldown. Deterministic tests and spawn/target footing remain support evidence only, not no-omniscience closure.

1. Build/prove the remaining live behavior control:
   - `writhing_stalker.live_no_omniscient_beeline_mcw`
2. Keep `performance.mixed_hostile_stalker_horde_mcw` queued from `doc/mixed-hostile-stalker-horde-performance-playtest-v0-2026-04-30.md` (bandit camp + cannibal camp + one writhing stalker + horde, with metrics), but do not let it replace the remaining behavior proofs.
3. Record tuning/performance readout: too common, too fast, too tanky, too invisible, too honest, too stupid, or too expensive under mixed hostile load.
4. Run the smallest honest gates after each proof slice, then update `TESTING.md`, `SUCCESS.md`, and `doc/work-ledger.md` with exact evidence.

Do **not** implement bandit shakedown/camp logic, Basecamp economy hooks, common spawn spam, teleport ambushes, global exact scans, or long-term nemesis arcs in v0.
