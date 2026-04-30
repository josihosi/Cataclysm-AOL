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

Current state boundary: `CAOL-BANDIT-STRUCT-BOUNTY-v0` is closed/checkpointed green for v0 in `doc/bandit-structural-bounty-phase-7-closure-readout-2026-04-30.md`. Writhing stalker is now greenlit in canon, but Andi cron and Augerl-Frau review cron remain disabled until explicitly re-enabled.

1. Add the writhing stalker monster/stat/spawn footing with rarity/singleton guardrails.
2. Add deterministic stalker-interest/latch/opportunity/withdraw helper tests before live behavior gets fancy.
3. Wire only the smallest needed live seam for shadow/strike/withdraw behavior; do not inherit bandit camp economy.
4. Build harness scenarios for:
   - `writhing_stalker.live_shadow_strike_mcw`
   - `writhing_stalker.live_no_omniscient_beeline_mcw`
   - `writhing_stalker.live_exposed_retreat_mcw`
5. Run the smallest honest gates after each code slice, then update `TESTING.md`, `SUCCESS.md`, and `doc/work-ledger.md` with exact evidence.

Do **not** implement bandit shakedown/camp logic, Basecamp economy hooks, common spawn spam, teleport ambushes, global exact scans, or long-term nemesis arcs in v0.
