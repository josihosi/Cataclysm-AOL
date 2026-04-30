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

Current state boundary: monster/stat/spawn footing, deterministic stalker-interest/latch/approach/opportunity/withdraw substrate, first live monster-plan seam wiring, and split live spawn/target footing are implemented and validated. `monster::plan()` consumes `writhing_stalker::evaluate_live_response()` for `mon_writhing_stalker` local-evidence gating, shadow positioning, strike targeting, and withdrawal/cooldown routing; same-run harness proof now shows a debug-spawned stalker persists in saved `active_monsters`, then acquires the player and reaches the live `live_plan` seam. V0 adds no new persisted latch state; it reuses existing monster effect state for short cooldown. Next execution should prove actual behavior scenes; deterministic tests and spawn/target footing remain support evidence only, not shadow/strike/exposed-retreat closure.

1. Repair/build the next behavior-scene harness proof, starting with `writhing_stalker.live_exposed_retreat_mcw`: replace the failed wait-to-noon ritual with a reliable exposure/focus setup or explicitly classify why the current fixture cannot prove exposed retreat.
2. Build/prove the remaining live behavior controls:
   - `writhing_stalker.live_shadow_strike_mcw`
   - `writhing_stalker.live_no_omniscient_beeline_mcw`
3. Keep `performance.mixed_hostile_stalker_horde_mcw` queued from `doc/mixed-hostile-stalker-horde-performance-playtest-v0-2026-04-30.md` (bandit camp + cannibal camp + one writhing stalker + horde, with metrics), but do not let it replace the exposed/behavior proof still open.
4. Record tuning/performance readout: too common, too fast, too tanky, too invisible, too honest, too stupid, or too expensive under mixed hostile load.
5. Run the smallest honest gates after each proof slice, then update `TESTING.md`, `SUCCESS.md`, and `doc/work-ledger.md` with exact evidence.

Do **not** implement bandit shakedown/camp logic, Basecamp economy hooks, common spawn spam, teleport ambushes, global exact scans, or long-term nemesis arcs in v0.
