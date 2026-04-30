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

Current state boundary: monster/stat/spawn footing and deterministic stalker-interest/latch/approach/opportunity/withdraw substrate are implemented and validated: `mon_writhing_stalker` exists with rare singleton `GROUP_ZOMBIE` footing, and `src/writhing_stalker_ai.*` plus `tests/writhing_stalker_test.cpp` cover the pure decision seam. Next execution should connect the smallest live game seam; deterministic tests are support evidence only, not gameplay proof.

1. Wire only the smallest needed live seam for shadow/strike/withdraw behavior; do not inherit bandit camp economy.
2. Decide/implement whether the live seam stores new latch/cooldown state; if it does, add save/load proof, otherwise record persistence as not applicable for v0.
3. Build harness scenarios for:
   - `writhing_stalker.live_shadow_strike_mcw`
   - `writhing_stalker.live_no_omniscient_beeline_mcw`
   - `writhing_stalker.live_exposed_retreat_mcw`
4. Run the smallest honest gates after each code slice, then update `TESTING.md`, `SUCCESS.md`, and `doc/work-ledger.md` with exact evidence.

Do **not** implement bandit shakedown/camp logic, Basecamp economy hooks, common spawn spam, teleport ambushes, global exact scans, or long-term nemesis arcs in v0.
