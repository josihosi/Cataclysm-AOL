# C-AOL live AI performance matrix v0 - 2026-04-29

Status: IN PROGRESS / ONE-, TWO-, AND PRE-STAGED THREE-SITE ROWS GREEN; NATURAL THREE-SITE PLAYER-PRESSURE ROW BLOCKED BY CAP; FOUR-SITE ROW OPEN

Contract: `doc/c-aol-live-ai-performance-audit-packet-v0-2026-04-28.md`
Imagination source: `doc/c-aol-live-ai-performance-imagination-source-of-truth-2026-04-28.md`

## Measurement rule

Keep the evidence classes separate:

- **Fixture/setup support:** save transforms, preflight saved-state audits, startup/load screenshots.
- **Live game path:** the real `overmap_npc_move()` path during bounded wait/time passage.
- **Timing/counters:** compact `bandit_live_world perf:` debug lines emitted from the live path.
- **Harness wall-clock:** whole probe runtime, useful for smoke but not the same as game-path cost.

A row is not green just because the game loaded or an artifact exists. A credited row needs a clean current-runtime launch, bounded live time passage, the perf counter line, and enough job/profile context to explain what was measured.

## Instrumentation added

`src/do_turn.cpp` now emits one compact `bandit_live_world perf:` line whenever the live hostile-overmap cadence does meaningful work: signal cadence, dispatch cadence, or empty-site retirement reporting.

Fields currently emitted:

- `sites`, `active_sites`, `active_job_mix`
- `signals`, `retired_reports`, `travelling_npcs`, `npcs_need_reload`
- `signal_cadence_due`, `dispatch_cadence_due`
- microsecond slices: `aftermath_us`, `retirement_us`, `signal_us`, `dispatch_us`, `travel_us`, `total_us`

This is intentionally top-level instrumentation, not a behavior redesign.

## Current matrix

| Case | Scenario / run | Evidence class | In-game window | Profile/job mix | Counters/timing | Verdict |
| --- | --- | --- | --- | --- | --- | --- |
| Baseline / zero hostile active sites | Not staged yet | missing | TBD | none | TBD | open |
| One hostile overmap AI site | `performance.bandit_one_site_remembered_lead_wait_30m`, run `.userdata/dev-harness/harness_runs/20260429_025639/` | green feature-path performance row for one-site case; startup/fixture support remains separate | 30m bounded wait choice; 4/4 green step ledger; green wait ledger via artifact delta after bounded wait | `camp_style:stalk=1`; one remembered-lead bandit camp; `signals=0`; 4 travelling NPCs | 6 perf cadence rows in `measure_30_minutes_one_site_remembered_lead.after_wait.artifacts.log`; `total_us` min/median/max `506/528.0/1134`, sum `3751`; one dispatch-due row `dispatch_us=297`; harness wall-clock `/tmp/caol_perf_clean_20260429_025638.log` `real 38.58s`; runtime `ae0c974d47`, `captured_dirty=false`, `version_matches_repo_head=true`, `version_matches_runtime_paths=true` | green one-site row |
| Two hostile overmap AI sites | `performance.two_site_bandit_cannibal_wait_30m`, run `.userdata/dev-harness/harness_runs/20260429_032427/` | green feature-path performance row for two-site mixed-profile case; startup/fixture support remains separate; saved-state preflight proves the remembered-lead bandit site only, while the added cannibal site is credited from same-run `abstract_bootstrap`/dispatch/perf artifact lines | 30m bounded wait choice; 4/4 green step ledger; green wait ledger via artifact delta after bounded wait | `camp_style:stalk=1,cannibal_camp:stalk=1`; one remembered-lead bandit camp plus one direct-range seeded cannibal camp; `signals=0`; 6 travelling NPCs | `abstract_bootstrap created_sites=1 ... total_sites=2`; 6 perf cadence rows in `measure_30_minutes_two_site_bandit_cannibal.after_wait.artifacts.log`; `total_us` min/median/max `515/546.0/2889`, sum `5637`; one dispatch-due row `dispatch_us=1935`; harness wall-clock `/tmp/caol_perf_two_site_probe3_20260429_032426.log` `38.522s total`; runtime `5325f8fb4d`, `captured_dirty=false`, `version_matches_repo_head=true`, `version_matches_runtime_paths=true` | green two-site row |
| Three hostile overmap AI sites | `performance.three_site_prestaged_active_wait_30m`, run `.userdata/dev-harness/harness_runs/20260429_040926/` | green feature-path performance-load row for three pre-staged active hostile jobs; startup/fixture support remains separate; this is explicitly not natural three-way player-pressure dispatch proof | 30m bounded wait choice; 6/6 green step ledger; green wait ledger via artifact delta after bounded wait | preflight-proven non-player-target active jobs: `performance_load:bandit@140,51,0`, `performance_load:cannibal_east@150,39,0`, `performance_load:cannibal_west@130,39,0`; same-run perf mix `camp_style:stalk=1,cannibal_camp:stalk=2`; `signals=0`; `travelling_npcs=0` | 6 perf cadence rows in `measure_30_minutes_three_site_prestaged_active.after_wait.artifacts.log`; `total_us` min/median/max `521/543.5/3901`, sum `6610`; one dispatch-due row `dispatch_us=3048`; harness wall-clock `/tmp/caol_perf_three_prestaged_20260429_040925.log` `real 39.04s`; runtime `5325f8fb4d`, `captured_dirty=false`, `version_matches_runtime_paths=true` | green pre-staged three-active-site performance-load row; natural player-pressure row remains blocked by cap/watchlist |
| Four hostile overmap AI sites | Not staged yet | missing | TBD | TBD | TBD | open |

Dedicated first-row scenario: `tools/openclaw_harness/scenarios/performance.bandit_one_site_remembered_lead_wait_30m.json`.
Dedicated second-row scenario/fixture: `tools/openclaw_harness/scenarios/performance.two_site_bandit_cannibal_wait_30m.json` and `tools/openclaw_harness/fixtures/saves/live-debug/performance_two_site_bandit_cannibal_v0_2026-04-29/manifest.json`.
Blocked direct-range three-site scenario/fixture: `tools/openclaw_harness/scenarios/performance.three_site_bandit_cannibal_wait_30m.json` and `tools/openclaw_harness/fixtures/saves/live-debug/performance_three_site_bandit_cannibal_v0_2026-04-29/manifest.json`.
Green pre-staged three-site performance-load scenario/fixture: `tools/openclaw_harness/scenarios/performance.three_site_prestaged_active_wait_30m.json` and `tools/openclaw_harness/fixtures/saves/live-debug/performance_three_site_prestaged_active_v0_2026-04-29/manifest.json`.

Earlier support-only instrumentation smokes: `bandit.camp_map_vanished_signal_redispatch`, run `.userdata/dev-harness/harness_runs/20260429_024952/`, emitted the same perf line shape but kept a yellow wait ledger and stale title/version metadata; first dedicated run `.userdata/dev-harness/harness_runs/20260429_025436/` was green but used the dirty pre-commit runtime. Both are retained only as probe support, not row credit.

Earlier blocked two-site staging attempts are also support-only: `.userdata/dev-harness/harness_runs/20260429_031940/` proved a noisy smoke/fire footing could produce `sites=2 active_sites=2`, but it measured signal behavior plus a visible-hostile interruption and did not preserve the intended remembered-lead bandit mix; `/tmp/caol_perf_two_site_probe2_20260429_032213.log` failed at startup after moving the player into an unstaged map pack. Frau review approved the third changed retry as the cleaner `signals=0` mixed-profile performance row.

Evidence-boundary caveat: the two-site saved-state preflight did **not** prove both hostile sites. It proved the persisted remembered-lead bandit site. The cannibal site/profile entered the credited row through same-run live-path evidence: `bandit_live_world abstract_bootstrap created_sites=1 ... total_sites=2`, cannibal dispatch/local-gate lines, and `sites=2 active_sites=2` perf rows. Keep that split visible in later rows instead of letting fixture support impersonate measured live-path proof. Boundary-rerun `.userdata/dev-harness/harness_runs/20260429_033704/` confirmed the revised scenario contract with explicit abstract-bootstrap artifact patterns still reaches green feature-path classification.

Three-site direct-range caveat: `.userdata/dev-harness/harness_runs/20260429_035001/` shows why the "two direct-range cannibals plus remembered-lead bandit" recipe is not a green three-active-site row. Same-run artifacts prove both added cannibal sites bootstrapped and dispatched, but the live dispatch cap in `src/do_turn.cpp` (`max_simultaneous_player_pressure = 2`) means the two nearer/direct-range cannibal sites consume the available player-pressure slots before the remembered-lead bandit site dispatches. The result is `sites=3 active_sites=2 active_job_mix=cannibal_camp:stalk=2`, plus a yellow wait ledger. Frau consult allowed a changed attempt only if cap inspection supported it; inspection of `has_active_player_pressure()` shows a saved active group with `active_target_id` starting `player@` counts against the same cap, so a persisted player-target site plus two new direct-range dispatches is expected to remain capped at two. Schani/Josef then greenlit an honest contract change for attempt 3: pre-staged active persisted jobs are acceptable for a **performance** row if the matrix labels them as load/cost measurement rather than natural dispatch proof. Run `.userdata/dev-harness/harness_runs/20260429_040926/` closes that pre-staged three-active-site row green. The natural player-pressure dispatch ceiling remains a watchlist item, not a failed performance-load row.

## Code audit notes

Current live-path risk points to watch as site count rises:

1. `overmap_npc_move()` is the live turn-loop entry point. The bandit/cannibal signal scan is cadence-gated (`5_minutes` / `30_minutes`), but the top-level NPC travel loop still touches nearby overmap NPCs every turn.
2. `note_live_bandit_aftermath()` and `retire_empty_hostile_sites()` sit before the cadence gates. The instrumentation separates their cost from signal/dispatch cost so a per-turn cost can be spotted instead of hidden in total wait time.
3. Signal scan and dispatch planning should scale mostly with active/known hostile sites and nearby signal candidates, not with broad map state. The `signals`, `sites`, `active_sites`, and `active_job_mix` fields are the first guardrail against unexplained jumps.
4. Report/log construction can become its own cost if every site emits large text every cadence. The perf line is compact; existing behavior lines remain useful but should be watched for spam in 2/3/4-site runs.
5. Save serialization is outside the measured cadence slices unless a scenario explicitly saves; keep save/writeback timing separate from live wait timing.

## Next row work

1. Preserve the green two-site `signals=0` mixed-profile row and the green pre-staged three-site performance-load row as distinct evidence classes.
2. Keep the direct-range two-cannibal three-site recipe as a cap/watchlist artifact only; it proves `sites=3 active_sites=2`, not natural three-active player pressure.
3. Stage the four-site row only with an explicit contract. Recommended comparable footing: pre-staged active performance-load jobs with separated site ids/anchors/target ids, no smoke/fire/map-field signals, green preflight for all active jobs, a green 30m wait ledger, and same-run `sites=4 active_sites=4` perf lines.
4. If a natural four-site player-pressure row is desired instead, get a fresh Schani/Josef decision first; the current cap makes that a behavior/contract question, not a performance-matrix chore.
