# C-AOL live AI performance matrix v0 - 2026-04-29

Status: IN PROGRESS / ONE- AND TWO-SITE ROWS GREEN, THREE-SITE DIRECT-RANGE ATTEMPT BLOCKED, FOUR-SITE ROW OPEN

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
| Three hostile overmap AI sites | `performance.three_site_bandit_cannibal_wait_30m`, run `.userdata/dev-harness/harness_runs/20260429_035001/` | non-credit blocked staging attempt; startup/load and fixture seed transforms were present, same-run abstract-bootstrap proved `created_sites=2 total_sites=3`, but the measured feature row stayed red/yellow | wait-action row yellow: OCR did not classify the wait finish/interruption and the after screen was heat/"getting warm" noise, so elapsed-time proof is not green | direct-range recipe activated the two cannibal sites only: `active_job_mix=cannibal_camp:stalk=2`; no same-run remembered-lead bandit dispatch line; `signals=0` | 6 perf cadence rows in `measure_30_minutes_three_site_bandit_cannibal.after_wait.artifacts.log`; all show `sites=3 active_sites=2`, not the required `active_sites=3`; first row `total_us=4031`, later rows roughly `525-556us`; harness wall-clock `/tmp/caol_perf_three_site_probe2_20260429_035000.log` `real 39.46s` | blocked: useful cap/interference evidence only, not a green three-active-site row |
| Four hostile overmap AI sites | Not staged yet | missing | TBD | TBD | TBD | open |

Dedicated first-row scenario: `tools/openclaw_harness/scenarios/performance.bandit_one_site_remembered_lead_wait_30m.json`.
Dedicated second-row scenario/fixture: `tools/openclaw_harness/scenarios/performance.two_site_bandit_cannibal_wait_30m.json` and `tools/openclaw_harness/fixtures/saves/live-debug/performance_two_site_bandit_cannibal_v0_2026-04-29/manifest.json`.
Blocked direct-range three-site scenario/fixture: `tools/openclaw_harness/scenarios/performance.three_site_bandit_cannibal_wait_30m.json` and `tools/openclaw_harness/fixtures/saves/live-debug/performance_three_site_bandit_cannibal_v0_2026-04-29/manifest.json`.

Earlier support-only instrumentation smokes: `bandit.camp_map_vanished_signal_redispatch`, run `.userdata/dev-harness/harness_runs/20260429_024952/`, emitted the same perf line shape but kept a yellow wait ledger and stale title/version metadata; first dedicated run `.userdata/dev-harness/harness_runs/20260429_025436/` was green but used the dirty pre-commit runtime. Both are retained only as probe support, not row credit.

Earlier blocked two-site staging attempts are also support-only: `.userdata/dev-harness/harness_runs/20260429_031940/` proved a noisy smoke/fire footing could produce `sites=2 active_sites=2`, but it measured signal behavior plus a visible-hostile interruption and did not preserve the intended remembered-lead bandit mix; `/tmp/caol_perf_two_site_probe2_20260429_032213.log` failed at startup after moving the player into an unstaged map pack. Frau review approved the third changed retry as the cleaner `signals=0` mixed-profile performance row.

Evidence-boundary caveat: the two-site saved-state preflight did **not** prove both hostile sites. It proved the persisted remembered-lead bandit site. The cannibal site/profile entered the credited row through same-run live-path evidence: `bandit_live_world abstract_bootstrap created_sites=1 ... total_sites=2`, cannibal dispatch/local-gate lines, and `sites=2 active_sites=2` perf rows. Keep that split visible in later rows instead of letting fixture support impersonate measured live-path proof. Boundary-rerun `.userdata/dev-harness/harness_runs/20260429_033704/` confirmed the revised scenario contract with explicit abstract-bootstrap artifact patterns still reaches green feature-path classification.

Three-site direct-range caveat: `.userdata/dev-harness/harness_runs/20260429_035001/` shows why the "two direct-range cannibals plus remembered-lead bandit" recipe is not a green three-active-site row. Same-run artifacts prove both added cannibal sites bootstrapped and dispatched, but the live dispatch cap in `src/do_turn.cpp` (`max_simultaneous_player_pressure = 2`) means the two nearer/direct-range cannibal sites consume the available player-pressure slots before the remembered-lead bandit site dispatches. The result is `sites=3 active_sites=2 active_job_mix=cannibal_camp:stalk=2`, plus a yellow wait ledger. Do not credit this as a three-site performance row; the next attempt must materially change the footing, e.g. a separate remembered-lead/persisted-active hostile job that is not just a second direct-range cannibal, or the row contract must be revised explicitly.

## Code audit notes

Current live-path risk points to watch as site count rises:

1. `overmap_npc_move()` is the live turn-loop entry point. The bandit/cannibal signal scan is cadence-gated (`5_minutes` / `30_minutes`), but the top-level NPC travel loop still touches nearby overmap NPCs every turn.
2. `note_live_bandit_aftermath()` and `retire_empty_hostile_sites()` sit before the cadence gates. The instrumentation separates their cost from signal/dispatch cost so a per-turn cost can be spotted instead of hidden in total wait time.
3. Signal scan and dispatch planning should scale mostly with active/known hostile sites and nearby signal candidates, not with broad map state. The `signals`, `sites`, `active_sites`, and `active_job_mix` fields are the first guardrail against unexplained jumps.
4. Report/log construction can become its own cost if every site emits large text every cadence. The perf line is compact; existing behavior lines remain useful but should be watched for spam in 2/3/4-site runs.
5. Save serialization is outside the measured cadence slices unless a scenario explicitly saves; keep save/writeback timing separate from live wait timing.

## Next row work

1. Preserve the green two-site `signals=0` mixed-profile row as the last credited scale step.
2. Do **not** keep rerunning the direct-range two-cannibal three-site recipe; it currently proves `sites=3 active_sites=2` plus the player-pressure cap, not a three-active-site row.
3. For the next three-site attempt, materially change the footing: stage a separate remembered-lead or persisted-active hostile job with explicit setup proof, or get Schani/Josef to revise the row contract to a cap-aware `sites=3 active_sites=2` measurement.
4. Add one setup audit that explicitly proves all intended sites/profiles before measurement, or label any abstract-bootstrap-only site proof separately.
5. Run the same 30m measured live window for three sites only after the new footing is honest and require same-run `sites=3 active_sites=3` perf/job-mix lines unless the row contract is explicitly revised; extend to four only after that boundary is settled.
