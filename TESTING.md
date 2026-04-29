# TESTING

Current validation policy and current evidence only.

This file is not a trophy wall.
Remove stale or completed fluff instead of stacking crossed-off test history forever.

## Validation policy

Use the **smallest evidence that honestly matches the change**.

- **Docs-only change** -> no compile
- **Small local code change** -> narrow build and the narrowest relevant test target
- **Broad or risky code change** -> broader rebuild and relevant filtered tests
- **Before a Josef handoff** or after suspicious stale-binary behavior -> rebuild the real targets and rerun the real smoke path

### Agent vs Josef

Schani should do agent-side playtesting first whenever the harness or direct in-game probing can answer the question.
Josef should be asked only for:
- product judgment
- tone/feel
- human-priority choices
- genuinely human-only interaction

Josef being unavailable is **not** a blocker by itself.
Josef self-testing is **not** a plan blocker and does not belong in the active success state as a gate.
If Josef-specific checks are useful, write them down as non-blocking notes so he can run them later from his own list.
If another good agent-side lane exists, keep moving there.
If several human-only judgments are likely soon, batch them instead of sending tiny separate asks.

### Anti-churn rule

Do not keep rerunning the same startup or test packet when it is no longer the missing evidence class.
If startup/load is already green, and the missing proof is live behavior, then the next probe must target live behavior.
If a target is merely waiting on Josef, do not keep revalidating it unless the code changed.

Current debug-stack attempt rule for the same item/blocker:
- attempts 1-2 may be Andi solo retries, including multiple focused harness attempts in one cron run when each attempt changes setup, instrumentation, or evidence class
- before attempt 3, consult Frau Knackal
- attempts 3-4 are the final changed retries after consultation
- after attempt 4, if code is implemented but proof still fails, add a concise implemented-but-unproven packet to Josef's playtest package and move to the next greenlit debug note; do not close it and do not park it as dead

### Test-to-game wiring rule

A test is not allowed to impersonate implementation. Before claiming gameplay behavior, identify the live code path that consumes the tested seam and name the evidence class that proves it: unit/evaluator, playback/proof packet, live source hook, harness/startup, screen, save inspection, or artifact/log. Deterministic-only packets may close only as deterministic-only packets; if the contract says the game does something, the proof must reach the game path or the claim stays open.

### Bandit overmap-proof rule

For the remaining bandit AI proof packets, single-turn deterministic checks are **not** enough by themselves.
The honest bar now includes real overmap-side multi-turn scenario proof, up to `500` turns where needed, with explicit per-scenario goals and tuning metrics.


## Current validation targets

### Active validation target - C-AOL actual playtest verification stack v0 / C-AOL live AI performance audit packet v0

`C-AOL actual playtest verification stack v0` remains active. The next unblocked slice is `C-AOL live AI performance audit packet v0`: a measured performance/playability audit for multiple active hostile overmap AI sites. Contracts: `doc/c-aol-actual-playtest-verification-stack-v0-2026-04-27.md` and `doc/c-aol-live-ai-performance-audit-packet-v0-2026-04-28.md`; imagination source: `doc/c-aol-live-ai-performance-imagination-source-of-truth-2026-04-28.md`.

Evidence burden: a compact live/harness performance matrix for baseline/one, two, three, and four active hostile-overmap-AI cases where staging is honest. Each credited row needs current-runtime freshness, a green startup gate, a real live game-path measured window, in-game elapsed time, wall-clock timing, active site/job/profile counts, and relevant signal/dispatch/local-gate/sight-avoid counters or logs. Startup/load and deterministic-only results are support only; evaluator speed does not close live playability.

Current state: matrix/instrumentation setup is landed and the one-site, two-site, and pre-staged three-site rows are green. `doc/c-aol-live-ai-performance-matrix-v0-2026-04-29.md` defines the row/evidence shape, `src/do_turn.cpp` emits compact `bandit_live_world perf:` live-path timing/counter lines, and dedicated run `.userdata/dev-harness/harness_runs/20260429_025639/` (`performance.bandit_one_site_remembered_lead_wait_30m`) reached feature-path classification with 4/4 green step ledger, green wait ledger, current-runtime freshness (`ae0c974d47`, clean repo-head/runtime-path match), and one-site timing counters. Dedicated run `.userdata/dev-harness/harness_runs/20260429_032427/` (`performance.two_site_bandit_cannibal_wait_30m`) reached feature-path classification with 4/4 green step ledger, green wait ledger, current-runtime freshness (`5325f8fb4d`, clean repo-head/runtime-path match), and two-site mixed-profile counters for `camp_style:stalk=1,cannibal_camp:stalk=1` with `signals=0`; its saved-state preflight proved only the remembered-lead bandit site, while same-run `abstract_bootstrap`/dispatch/perf lines proved the cannibal site/profile. Direct-range three-site attempt `.userdata/dev-harness/harness_runs/20260429_035001/` is blocked/non-credit: same-run artifacts show `abstract_bootstrap created_sites=2 total_sites=3` and both direct-range cannibal dispatch/local-gate lines, but the perf rows remain `sites=3 active_sites=2 active_job_mix=cannibal_camp:stalk=2`, the remembered-lead bandit dispatch line is absent, and the wait ledger is yellow/unclassified. Frau consult allowed a persisted-active attempt only after cap inspection; code inspection shows saved active player-target groups are included in `active_player_pressure`, so one pre-persisted player-target site plus two new direct-range dispatches would still collide with `max_simultaneous_player_pressure = 2`. Schani/Josef greenlit attempt 3 as a labeled pre-staged performance-load row; run `.userdata/dev-harness/harness_runs/20260429_040926/` (`performance.three_site_prestaged_active_wait_30m`) reached feature-path classification with 6/6 green step ledger, green 30m wait ledger, preflight-proven distinct active non-player-target jobs, same-run `sites=3 active_sites=3 active_job_mix=camp_style:stalk=1,cannibal_camp:stalk=2 signals=0` counters, and current-runtime-path freshness (`5325f8fb4d`, `captured_dirty=false`, `version_matches_runtime_paths=true`). Four-site credited row remains open.

Validation policy for this target: docs-only matrix setup uses `git diff --check`. If harness/runtime instrumentation or source code changes, run the narrowest compile/test for touched files plus any focused deterministic test/benchmark added. Before credited live timing, rebuild or verify current `cataclysm-tiles` freshness and run the named measured scenario/report path.

### Recently completed validation target - Cannibal camp confidence-push live playtest packet v0

`Cannibal camp confidence-push live playtest packet v0` is complete as **confidence uplift green**. Matrix: `doc/cannibal-camp-confidence-push-live-playtest-matrix-v0-2026-04-29.md`.

Checkpoint evidence boundary:

- bandit contrast control: `.userdata/dev-harness/harness_runs/20260429_012915/`, scenario `bandit.extortion_first_demand_fight_mcw`, current runtime `7ca870f6be`, 6/6 green step ledger, feature proof, shakedown/pay/fight surface preserved;
- cannibal day smoke/pressure: `.userdata/dev-harness/harness_runs/20260429_013310/`, scenario `cannibal.live_world_day_smoke_pressure_mcw`, current runtime `782d8edabd`, 2/2 green step ledger, feature proof, live smoke/signal path with `profile=cannibal_camp`, `darkness_or_concealment=no`, `shakedown=no`, `combat_forward=no`;
- cannibal night/contact pack-forward: `.userdata/dev-harness/harness_runs/20260429_014900/`, scenario `cannibal.live_world_night_local_contact_pack_mcw`, current runtime `acfe6fd0ce`, 2/2 green step ledger, feature proof, local-contact `attack_now` pack with `pack_size=2`, `darkness_or_concealment=yes`, `shakedown=no`, `combat_forward=yes`;
- reload brain create/save: `.userdata/dev-harness/harness_runs/20260429_021849/`, scenario `cannibal.live_world_day_smoke_persistence_mcw`, current runtime `e778902cac`, 7/7 green step ledger, feature proof, guarded save mtime writeback, saved active `cannibal_camp` stalk group with active members `[4,5]`, target `player@140,41,0`, and `known_recent_marks` including `live_smoke@140,41,0`;
- paired no-fixture reload support: `.userdata/dev-harness/harness_runs/20260429_021929/`, scenario `cannibal.live_world_day_smoke_persistence_reload_audit_mcw`, current runtime `e778902cac`, 2/2 green step ledger and saved-state metadata still present after fresh startup; top-level classifier stays yellow/no-new-artifacts, so this is saved-file/startup support paired with the create/save feature proof, not independent new behavior proof;
- different-footing exposed-sight repeat: `.userdata/dev-harness/harness_runs/20260429_022021/`, scenario `cannibal.live_world_exposed_sight_avoid_mcw`, current runtime `e778902cac`, 5/5 green step ledger, feature proof on fixture `cannibal_live_world_exposed_sight_avoid_v0_2026-04-28` from source footing `bandit_local_sight_avoid_exposed_v0_2026-04-27`, showing `profile=cannibal_camp`, `active_job=stalk`, `posture=hold_off`, `pack_size=2`, `recent_exposure=yes`, `sight_exposure=recent`, `shakedown=no`, `combat_forward=no`.

Caveats retained: this is confidence uplift for already-finished cannibal behavior, not behavior redesign; reload support reads saved state rather than proving fresh post-load local-gate behavior; the different-footing repeat reduces but does not eliminate fixture-bias risk.

### Recently completed validation target - Bandit camp-map risk/reward dispatch planning packet v0

`Bandit camp-map risk/reward dispatch planning packet v0` is closed as **scoped live/product checkpoint green**. Frau product review accepted the current matrix as sufficient for this checkpoint and did not require a second-fixture bias variant before closure.

Checkpoint evidence boundary:

- deterministic/code support covers the camp-owned `camp_intelligence_map`, serialization, active target OMT persistence, scout-return writeback, live signal camp-map lead writes, remembered-lead selection through live dispatch cadence/reporting, two-OMT ordinary scout stand-off, 720-minute scout return clock, 2/4/5/7/10 roster/reserve sizing, active-outside dogpile blocking, wounded/unready and killed-member shrinkage, bounded stockpile pressure, high-threat non-escalation, prior defender-loss pressure, prior bandit-loss cooling, larger-than-scout stalk sizing, and no-opening hold/return;
- feature-path proof covers sight-avoid (`20260428_173626`), vanished-signal redispatch (`20260428_185947`), 2/5/10 variable-roster sizing (`20260428_202044`, `20260428_192059`, `20260428_193621`), no-opening stalk-pressure (`20260428_195617`), high-threat/low-reward hold (`20260428_200600`), active-outside dogpile-block (`20260428_200434`), shakedown/toll-control (`20260428_204454`, `20260428_204630`, `20260428_204813`), same-fixture high-threat repeatability (`20260428_211105`, `20260428_211153`), and optional empty-camp retirement sanity (`20260428_214542`).

Recorded gates: `python3 -m py_compile tools/openclaw_harness/startup_harness.py`; relevant scenario/fixture `python3 -m json.tool` checks; `python3 tools/openclaw_harness/proof_classification_unit_test.py`; `git diff --check`; `make -j4 TILES=1 cataclysm-tiles LINTJSON=0 ASTYLE=0`; `./tests/cata_test "[bandit_live_world]"`; `./tests/cata_test "[bandit][live_world][camp_map]" --success`; and the named harness probes. `make astyle-diff` remains locally blocked by missing `astyle`.

Caveats retained: player-created fire/smoke/light scenario 1 remains blocked behind the fuel/writeback gate and is not credited; same-fixture repeatability is not second-fixture anti-bias proof; the no-opening proof is not opening-present escalation proof.

### Recently completed validation target - Cannibal camp night-raid behavior packet v0

The required cannibal live matrix scenarios are checkpointed green: day smoke/stalk pressure, night local-contact pack attack, exposed/recent-sight hold-off, daylight/high-threat negative, saved-state persistence, and day-smoke repeatability. The optional labeled bandit-control contrast is non-blocking unless product review explicitly reopens it.

### Recently completed validation target - C-AOL debug-proof finish stack

`C-AOL debug-proof finish stack v0` is complete and ready for Schani review. Retain the old implemented-but-unproven boundaries for real player-lit fire and Smart Zone Manager live layout unless current canon explicitly promotes their retry packets.

---

## Pending probes

Active validation target: **C-AOL actual playtest verification stack v0 / C-AOL live AI performance audit packet v0**.

1. Keep `.userdata/dev-harness/harness_runs/20260429_035001/` as blocked direct-range evidence only; do not credit it and do not rerun the same two-direct-cannibal recipe.
2. Preserve `.userdata/dev-harness/harness_runs/20260429_040926/` as the green pre-staged three-active-site performance-load row, not natural dispatch behavior proof.
3. Stage a four-site row only under an explicit contract. Recommended comparable footing: pre-staged active performance-load jobs with separated site ids/anchors/target ids, no smoke/fire/map-field signals, preflight proving all active jobs and live/findable members, green 30m wait ledger, and same-run `sites=4 active_sites=4` perf/job-mix lines.
4. If natural four-site player-pressure behavior is desired instead, stop and ask Schani/Josef; the current cap makes that a behavior/contract question.
5. Add deeper instrumentation or optimization only if measured rows expose an actual bottleneck.

Still blocked/later in the actual playtest stack:

- `Player-lit fire and bandit signal verification packet v0`: blocked behind the fuel/writeback gate.
- `Roof-fire horde detection proof packet v0`: blocked behind real player-lit fire.

If a later live probe is needed:
- build the current runtime first when binary freshness matters;
- use one named scenario/command path;
- extract only decisive report/log fields into context;
- after two same-blocker attempts, consult Frau Knackal before attempt 3; after four unresolved attempts, package implemented-but-unproven state for Josef and move to the next greenlit unblocked target.

---

## Reusable commands

Use these when they are actually the missing evidence, not as ritual.

### Narrow camp deterministic pass after a code slice lands
- `git diff --check`
- `make -j4 obj/basecamp.o tests/faction_camp_test.o tests LINTJSON=0 ASTYLE=0`
- focused `./tests/cata_test` filters for the touched camp/locker/patrol reporting path

### Plan status summary command
- `python3 tools/plan_status_summary.py --self-test`
- `python3 tools/plan_status_summary.py /plan`
- `python3 tools/plan_status_summary.py /plan active`
- `python3 tools/plan_status_summary.py /plan greenlit`
- `python3 tools/plan_status_summary.py /plan parked`

### Fresh tiles rebuild only if a later handoff really needs live proof
- `make -j4 TILES=1 cataclysm-tiles`

## Local build caveat

On this Mac, treat top-level `make -j4 tests` as the reliable path for a fresh `cata_test`.
Avoid treating `make -C tests cata_test` as authoritative here; it has been a repeated source of toolchain/stale-build nonsense.
Also: if you actually need a fresh tiles binary, use `make -j4 TILES=1 cataclysm-tiles`; plain `make cataclysm-tiles` is not an honest rebuild path here.
