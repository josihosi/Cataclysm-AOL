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

Current state boundary: monster/stat/spawn footing, deterministic stalker-interest/latch/approach/opportunity/withdraw substrate, and the first live monster-plan seam are implemented and validated. `monster::plan()` consumes `writhing_stalker::evaluate_live_response()` for `mon_writhing_stalker` local-evidence gating, shadow positioning, strike targeting, and withdrawal/cooldown routing. V0 adds no new persisted latch state; it reuses existing monster effect state for short cooldown. Next execution should build live/harness proof scenes; deterministic tests remain support evidence only, not gameplay proof.

1. Build harness scenarios for:
   - `writhing_stalker.live_shadow_strike_mcw`
   - `writhing_stalker.live_no_omniscient_beeline_mcw`
   - `writhing_stalker.live_exposed_retreat_mcw`
2. Prove the live path reaches shadow/strike/withdraw behavior from the game path, not only the evaluator seam.
3. Record tuning readout: too common, too fast, too tanky, too invisible, too honest, or too stupid.
4. Run the smallest honest gates after each proof slice, then update `TESTING.md`, `SUCCESS.md`, and `doc/work-ledger.md` with exact evidence.

Do **not** implement bandit shakedown/camp logic, Basecamp economy hooks, common spawn spam, teleport ambushes, global exact scans, or long-term nemesis arcs in v0.
