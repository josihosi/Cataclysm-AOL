# CAOL-CAMP-LOCKER-ZONE-PLAYTESTS-v0 proof readout (2026-05-02)

Status: CLOSED / CHECKPOINTED GREEN V0

This is the bounded locker-zone playtest pass Josef asked for after `CAOL-CAMP-LOCKER-EQUIPMENT-API-REDUCTION-v0`. It does not reopen locker ontology or the closed API-reduction lane.

## Gates

- Scenario evidence contracts were repaired so screenshot/UI steps name their expected visible facts and artifact patterns match the harness' substring matcher.
- `git diff --check` passed; log: `/tmp/caol-locker-zone/git_diff_check_final.log`.
- `./tests/cata_test "[camp][locker]"` passed: `All tests passed (2147 assertions in 78 test cases)`; log: `/tmp/caol-locker-zone/camp_locker_tests_rerun.log`.

## Row verdicts

| Row | Claim | Scenario / run | Screen or OCR fact | Artifact / saved-state / log fact | Verdict |
| --- | --- | --- | --- | --- | --- |
| Zone footing / creation / persistence | A real `CAMP_LOCKER` zone exists on disk, and the current-build Zone Manager path can create and reopen a `Probe Locker` `CAMP_LOCKER` row. | `python3 tools/openclaw_harness/startup_harness.py probe locker.zone_manager_save_probe_mcw` -> `.userdata/dev-harness/harness_runs/20260502_041828/` | Step ledger is green with named screen checkpoints for gameplay load, Zone Manager open, add/type/name/place/save/reopen flow. | `probe.artifacts.log` matches `name="Basecamp: Locker" type="CAMP_LOCKER"` and `name="Probe Locker" type="CAMP_LOCKER"`; `audit_saved_existing_locker_zone_after_save.metadata.json` reports `status=required_state_present` for persistent `Basecamp: Locker` in `#Wm9yYWlkYSBWaWNr.zones.json` and temp mirror. | GREEN / `feature_proof=true` / `verdict=artifacts_matched`. |
| Service from zone stock | Camp locker service consumes real `CAMP_LOCKER` zone stock on the real service path. | `python3 tools/openclaw_harness/startup_harness.py probe locker.weather_wait` -> `.userdata/dev-harness/harness_runs/20260502_041300/` | Screen rows are named footing/turn-window checkpoints; service behavior is proven by same-run artifacts, not by pretending the UI visibly lists the branch. | `probe.artifacts.log` matches `camp locker: queued`, `plan for`, `after`, and `serviced`; decisive line includes `locker_tiles=1`, `candidates=1`, `changed_slots=1`, `applied=true`, worker pants changed to `cargo pants`, and `antarvasa` returned to locker stock. | GREEN / `feature_proof=true` / `verdict=artifacts_matched`. |
| Boundary / exclusion | Locker candidate gathering respects `NO_NPC_PICKUP`, non-locker stock, and policy-disabled slots. | Deterministic gate `./tests/cata_test "[camp][locker]"`; source row `camp_locker_zone_candidate_gathering` in `tests/faction_camp_test.cpp`. | Not a screen row. | Test creates `Locker` and `Blocked locker` `CAMP_LOCKER` zones plus a `No NPC pickup` zone on the blocked tile; it places an off-zone helmet and a blocked-zone helmet, then asserts locker candidates include locker-zone gear and `helmet == 0`, with disabled `belt`/`bag` also excluded. | PASS deterministic boundary guard, supporting the live rows rather than replacing them. |
| Practical gear / weather wait | Practical gear service can be triggered on current build after forced cold and bounded turns. | `locker.weather_wait` -> `.userdata/dev-harness/harness_runs/20260502_041300/` | Named screen checkpoints show loaded fixture, forced-temperature return, and bounded turn window completion. | The same service artifacts prove practical clothing cleanup/swap from locker stock. | GREEN for v0 practical gear service. Full human weather-feel taste remains optional manual playtest, not a blocker. |
| Robbie package/e2e | Seed ground items, ask Robbie to pick them up, assign worker, then observe first locker service. | `locker.package5_robbie_e2e_verified_mcw` -> `.userdata/dev-harness/harness_runs/20260502_034951/` | Intermediate screenshots exist, but final `probe_after` capture failed. | `probe.report.json` ended `blocked_step_local_proof`; no new `camp locker:` artifacts matched. | BLOCKED / NO CREDIT. Not needed for the bounded green v0 closure. |

## Decision

Closed green for the bounded v0 evidence question: the current build has live harness feature proof for Zone Manager `CAMP_LOCKER` footing/creation/reopen, saved persistent locker-zone metadata, real service use from locker-zone stock, and deterministic boundary/exclusion guards. The evidence classes stay separated: UI trace and screen checkpoints prove the Zone Manager path; saved-zone metadata proves persistent existing locker footing; service logs prove the worker/loadout branch; deterministic tests prove `NO_NPC_PICKUP`/off-zone/policy-disabled boundaries.

## Optional Josef product-feel card

This is no longer a closure blocker, but it remains a useful human taste pass if Josef wants to judge feel rather than proof:

1. Launch current `dev` build (`8b0fb096e4` or newer) and load a basecamp save with followers and Zone Manager access.
2. Open Zone Manager (`Y`). Confirm an existing `Basecamp: Locker` row, or create one named `Josef Locker Probe`, place it on a clear tile, exit and confirm save; reopen Zone Manager and confirm the row is still present.
3. Put one obviously useful gear item inside the locker zone and one tempting comparison item outside the locker zone or on a `No NPC Pickup` tile.
4. Let the camp worker service run, then inspect the worker and ground.
5. Expected pass: worker uses only locker-zone stock, excluded/off-zone stock stays untouched, and replaced gear returns/cleans up sensibly.
