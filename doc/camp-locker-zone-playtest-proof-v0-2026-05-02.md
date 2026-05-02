# CAOL-CAMP-LOCKER-ZONE-PLAYTESTS-v0 proof readout (2026-05-02)

Status: CLOSED / CHECKPOINTED YELLOW V0 / JOSEF MANUAL CARD WRITTEN

This is the agent-side bounded locker-zone playtest pass Josef asked for after `CAOL-CAMP-LOCKER-EQUIPMENT-API-REDUCTION-v0`. It does not reopen locker ontology or the closed API-reduction lane.

## Gates

- Rebuilt current tiles binary before live proof because `cataclysm-tiles` was stale; post-build version: `Cataclysm Dark Days Ahead: 8b0fb096e4` (`/tmp/caol-locker-zone/tiles_version_after.txt`).
- `make -j4 TILES=1 LINTJSON=0 ASTYLE=0` completed; build log: `/tmp/caol-locker-zone/make_tiles.log`.
- `./tests/cata_test "[camp][locker]"` passed: `All tests passed (2147 assertions in 78 test cases)`; log: `/tmp/caol-locker-zone/camp_locker_tests.log`.

## Row verdicts

| Row | Claim | Scenario / run | Screen or OCR fact | Artifact / saved-state / log fact | Verdict |
| --- | --- | --- | --- | --- | --- |
| Zone footing / persistence | A real `CAMP_LOCKER` row can be driven through the current-build Zone Manager and reopened after the save prompt. | `python3 tools/openclaw_harness/startup_harness.py probe locker.zone_manager_save_probe_mcw` -> `.userdata/dev-harness/harness_runs/20260502_034812/` | macOS Vision OCR fallback on `reopen_zones_manager_for_persistence_check.after.png` reads `Probe Locker`; the same screen shows the reopened Zone Manager list. | `probe.artifacts.log` traces `zone_manager_row ... name="Probe Locker" type="CAMP_LOCKER" enabled=yes ... start_abs="3367,994,0" end_abs="3367,994,0"`; existing `Basecamp: Locker` also traces as `type="CAMP_LOCKER"`. No separate disk save-file audit was captured. | YELLOW / CREDITED FOR CURRENT-BUILD UI+TRACE FOOTING. Automated feature proof remains yellow because the scenario lacks expected-visible-fact guards. |
| Service from zone stock | The camp locker service consumes real `CAMP_LOCKER` zone stock on the real service path. | `python3 tools/openclaw_harness/startup_harness.py probe locker.weather_wait` -> `.userdata/dev-harness/harness_runs/20260502_035238/` | Screen capture proves current binary/game session only; service behavior is not a visible UI row. | `probe.artifacts.log`: `camp locker: plan for Ricky Broughton locker_tiles=1 current_items=12 candidates=1 changed_slots=1 ... locker=[pants=[cargo pants (poor fit)<pants_cargo>]]`, followed by `after Ricky Broughton applied=true worker=[... pants=[cargo pants<pants_cargo>] ...] locker=[pants=[cargo pants (poor fit)<pants_cargo>; antarvasa<antarvasa>]]`. | PASS FOR LOGGED REAL SERVICE PATH; YELLOW as player-facing proof because screen/UI does not show the service branch. |
| Boundary / exclusion | Locker candidate gathering respects `NO_NPC_PICKUP`, non-locker stock, and policy-disabled slots. | Deterministic gate `./tests/cata_test "[camp][locker]"`; source row `camp_locker_zone_candidate_gathering` in `tests/faction_camp_test.cpp`. | Not a screen row. | The test creates `Locker` and `Blocked locker` `CAMP_LOCKER` zones plus a `No NPC pickup` zone on the blocked tile; it places an off-zone helmet and a blocked-zone helmet, then asserts locker candidates include locker-zone gear and `helmet == 0`, with disabled `belt`/`bag` also excluded. | PASS DETERMINISTIC BOUNDARY GUARD. Not a live manual/UI boundary proof. |
| Practical gear / weather wait | Practical gear service can be triggered on current build; full weather/wait product feel is not honestly closed by the harness. | `locker.weather_wait` -> `.userdata/dev-harness/harness_runs/20260502_035238/` | Screen capture after forced temperature is only session footing. | The scenario uses `debug_force_temperature` to 35F and three `advance_turns`; the service log above proves a practical clothing change/dedupe. It does **not** use the real long-wait primitive or prove a human-noticeable weather gear loop. | PARTIAL / SCOPED OUT. Manual card below closes the remaining product-feel question. |
| Robbie package/e2e | Seed ground items, ask Robbie to pick them up, assign worker, then observe first locker service. | `locker.package5_robbie_e2e_verified_mcw` -> `.userdata/dev-harness/harness_runs/20260502_034951/` | Intermediate screenshots exist, but final `probe_after` capture failed. | `probe.report.json` ended `blocked_step_local_proof`; no new `camp locker:` artifacts matched. | BLOCKED / NO CREDIT. Kept as blocker evidence, not closure. |
| Startup failure retry | Re-run attempt after a shell-script mistake. | `locker.zone_manager_save_probe_mcw` -> `.userdata/dev-harness/harness_runs/20260502_035235/` | Failure screenshot could not capture a window. | `probe.report.json`: startup failed with `process_exited` / return code `-9`. | NO CREDIT. |

## Decision

The bounded agent pass is complete, but the result is deliberately yellow rather than green: current-build zone footing and service-path logs are strong enough to answer the engineering question, and deterministic tests guard `NO_NPC_PICKUP` / off-zone exclusion, but the harness scenarios do not yet provide a clean automated player-facing proof ledger for every row. In particular, there is no disk save-file audit for the newly created probe zone, no real long-wait/weather-feel row, and the Robbie e2e path blocked before service artifacts.

## Josef manual playtest card

Goal: close the remaining product-facing proof without asking Josef to read logs.

1. Launch current `dev` build (`8b0fb096e4` or newer) and load a basecamp save with followers and Zone Manager access.
2. Open Zone Manager (`Y`). Confirm an existing `Basecamp: Locker` row, or create one:
   - add zone (`a`), choose `Basecamp: Locker`, name it `Josef Locker Probe`, place it on a clear tile, exit and confirm save;
   - reopen Zone Manager and confirm the locker row is still present.
3. Put one obviously useful gear item inside the locker zone, e.g. better pants/coat/gloves/helmet, and put one tempting comparison item outside the locker zone or on a `No NPC Pickup` tile.
4. Assign a nearby follower/worker to camp work or wait until the locker service has a chance to run.
5. Inspect the worker and ground:
   - expected pass: worker equips or swaps only from the locker-zone item;
   - expected pass: the off-zone / `No NPC Pickup` comparison item remains untouched;
   - expected pass: obsolete/replaced gear returns to the locker zone or cleanup behaves visibly sensibly.
6. Optional weather-feel check: in cold/rain, repeat with warm/rain gear in the locker zone and confirm the worker does not look absurdly under-equipped after a short wait.

Closure criteria: one screenshot of the reopened locker zone row, one screenshot before service showing locker-zone vs excluded item placement, and one screenshot after service showing the worker/ground outcome is enough to mark this v0 product-facing proof green. If the worker ignores good locker-zone gear, takes excluded/off-zone gear, or produces confusing cleanup, reopen as a bug with those three screenshots.
