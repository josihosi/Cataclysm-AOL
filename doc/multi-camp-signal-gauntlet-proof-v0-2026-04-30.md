# Multi-camp signal gauntlet proof v0 — 2026-04-30

## Verdict

`CAOL-MULTI-CAMP-SIGNAL-GAUNTLET-v0` is **CLOSED / CHECKPOINTED GREEN V0**.

This is live harness proof, not setup-only proof: each challenge row reached a loaded game, bounded wait/state passage, saved metadata checks, artifact checks, and log/perf stability checks.

## Artifacts

- Challenge A scenario: `bandit.multi_camp_structural_stress_mcw`
  - Fixture: `tools/openclaw_harness/fixtures/saves/live-debug/bandit_multi_camp_structural_stress_v0_2026-04-30/manifest.json`
  - Scenario: `tools/openclaw_harness/scenarios/bandit.multi_camp_structural_stress_mcw.json`
  - Green run: `.userdata/dev-harness/harness_runs/20260430_204416/`
- Challenge B scenario: `bandit.mixed_signal_coexistence_mcw`
  - Fixture: `tools/openclaw_harness/fixtures/saves/live-debug/bandit_mixed_signal_coexistence_v0_2026-04-30/manifest.json`
  - Scenario: `tools/openclaw_harness/scenarios/bandit.mixed_signal_coexistence_mcw.json`
  - Green run: `.userdata/dev-harness/harness_runs/20260430_203757/`
- Challenge C scenario: `bandit.mixed_signal_reload_resume_mcw`
  - Scenario: `tools/openclaw_harness/scenarios/bandit.mixed_signal_reload_resume_mcw.json`
  - Green run: `.userdata/dev-harness/harness_runs/20260430_203944/`

Commands:

```sh
python3 tools/openclaw_harness/startup_harness.py probe bandit.multi_camp_structural_stress_mcw
python3 tools/openclaw_harness/startup_harness.py probe bandit.mixed_signal_coexistence_mcw
python3 tools/openclaw_harness/startup_harness.py probe bandit.mixed_signal_reload_resume_mcw
```

All three final runs report `verdict=artifacts_matched`, `feature_proof=True`, green wait ledgers, and green step-local proof.

## Challenge A — multi-camp structural stress

Green run: `.userdata/dev-harness/harness_runs/20260430_204416/`.

Proof shape:

- Two real bandit camp sites were staged with clean non-overlapping rosters:
  - west/original camp: `overmap_special:bandit_camp@140,51,0`
  - east/cloned camp: `overmap_special:bandit_camp@160,39,0`
- Preflight state: both camps had `member_count=5`, `ready_at_home_count=5`, no active outside group, and one fixture `structural_bounty` forest lead.
- Bounded wait: two green `3h` wait rows. The earlier `6h` menu route was intentionally dropped because it stuck on the wait menu; the `3h` rows proved actual time passage without pretending the menu issue was product proof.
- Structural maintenance artifacts include:
  - `sites_scanned=2`
  - `dispatch_cap=2`
  - `dispatches_planned=2`
  - `dispatches_applied=2`
  - `active_job_mix=camp_style:scavenge=2`
  - later `arrivals=1`, `members_returned=1`
- Dogpile/spread readout: the two fixture leads targeted separate forests, `forest@138,52,0` and `forest@162,40,0`; no same-target dogpile was observed. Dispatch cap was not exhausted at initial dispatch.
- Saved after-state:
  - west fixture lead persisted as `status=harvested`, `times_harvested=1`, `last_outcome=harvested_structural_bounty`, with `ready_at_home_count=5` and no active group.
  - east fixture lead persisted as `status=harvested`, `times_harvested=1`, `last_outcome=harvested_structural_bounty`; the same camp then immediately continued on a **new** structural target `overmap_special:bandit_camp@160,39,0:structural_bounty:155,38,0:forest` with `active_job_type=scavenge`, `ready_at_home_count=4`, `active_outside_count=1`.

Caveat: the strict first guard expected both camps to be idle after harvesting. The live result was better described as “harvested fixture lead plus east-camp followthrough on a fresh structural lead.” That is accepted for v0 because it proves no repeat of the harvested fixture target; it is **not** claimed as an all-camps-idle proof.

## Challenge B — mixed signal coexistence

Green run: `.userdata/dev-harness/harness_runs/20260430_203757/`.

Proof shape:

- Fixture used three bandit sites:
  - retired/empty original site: `overmap_special:bandit_camp@140,51,0`
  - live-signal camp: `overmap_special:bandit_camp@151,39,0`
  - structural camp: `overmap_special:bandit_camp@160,39,0`
- Saved map preflight proved live `fd_fire` / `fd_smoke` source near the player.
- Bounded wait: green `30m` wait row, saved turn advanced from `5241593` to `5243393` (`+1800` turns; required `+1500`).
- Live-signal artifacts include:
  - `bandit_live_world signal scan: signal_packet=yes`
  - `smoke_packets=1`
  - `bandit_live_world signal maintenance: signal_packet=yes`
  - `matched_smoke_sites=1`
  - dispatch plan for `site=overmap_special:bandit_camp@151,39,0`
  - `candidate_reason=live_signal`
  - `signal_packet=live_smoke@140,39,0`
- Structural artifacts in the same run include:
  - `bandit_live_world structural maintenance:`
  - `sites_scanned=3`
  - `dispatches_planned=1`
  - `dispatches_applied=1`
  - structural dispatch for `site=overmap_special:bandit_camp@160,39,0`
  - lead `overmap_special:bandit_camp@160,39,0#lead:structural_bounty:forest@162,40,0`
- Saved after-state preserved both classes:
  - live camp: `active_job_type=scout`, target `player@140,39,0`, known marks `live_smoke@140,39,0` and `player@140,39,0`, plus a `smoke_signal` lead.
  - structural camp: `active_job_type=scavenge`, target structural-bounty lead, `status=active`, with structural suspected leads still present.

Result: live smoke/fire signal handling did not erase structural-bounty state, and structural scanning did not drown the urgent live signal. Candidate priority chose `live_signal` for the smoke camp while a separate structural camp dispatched its bounty in the same bounded run.

## Challenge C — reload/resume continuity

Green run: `.userdata/dev-harness/harness_runs/20260430_203944/`.

Proof shape:

- The run deliberately used `fixture=""` and `replace_existing_worlds=false`, starting from the saved world produced by Challenge B instead of reinstalling a fixture.
- Reload preflight proved both meaningful active outings survived load:
  - live camp `@151,39,0`: `active_job_type=scout`, `active_group_id` containing `#dispatch`, target `player@140,39,0`, known mark `live_smoke@140,39,0`, `active_outside_count=1`.
  - structural camp `@160,39,0`: `active_job_type=scavenge`, `active_group_id` containing `#structural`, target `overmap_special:bandit_camp@160,39,0#lead:structural_bounty:forest@162,40,0`, `active_outside_count=1`.
- Bounded resume wait: green `5m` wait row, saved turn advanced from `5243393` to `5243693` (`+300` turns; required `+300`).
- After save/writeback, both active groups remained present with the same job/target class and no disappearance, duplication, or stale-target loss.
- Perf artifact after reload: `sites=3`, `active_sites=2`, `active_job_mix=camp_style:scavenge=1,camp_style:scout=1`, `total_us=742`.

## Metrics and stability

- Challenge A: 72 `bandit_live_world perf:` samples; final line kept `sites=2`, `active_sites=1`, `active_job_mix=camp_style:scavenge=1`, `total_us=488`. No debug `ERROR` or `WARNING` lines found in the run artifact.
- Challenge B: 6 `bandit_live_world perf:` samples; observed `sites=3`, `active_sites=2`, `active_job_mix=camp_style:scavenge=1,camp_style:scout=1`, `total_us` around `593-1095`. No debug `ERROR` or `WARNING` lines found in the run artifact.
- Challenge C: reload/resume perf sample `total_us=742`. No debug `ERROR` or `WARNING` lines found in the run artifact.

## Final classification

Green v0, with scoped caveats:

- Two camps, not four. Four was preferred only if clean; two satisfied the explicit minimum and avoided duplicate roster/member-id soup.
- Challenge A proves spread/followthrough/no-repeat, not all-camps-idle after harvest.
- Challenge B uses a staged-but-live smoke/fire source and cloned camps; it is not natural unseeded discovery proof.
- Challenge C proves reload/resume for the active outing state created by Challenge B, not every possible structural/live signal state.

No dogpile, stale-state, reload-loss, CPU churn, log spam, or crash failure was observed in the final green runs.
