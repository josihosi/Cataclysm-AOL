# C-AOL live AI performance matrix v0 - 2026-04-29

Status: IN PROGRESS / ONE-SITE ROW GREEN, MULTI-SITE ROWS OPEN

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
| Two hostile overmap AI sites | Not staged yet | missing | TBD | TBD | TBD | open |
| Three hostile overmap AI sites | Not staged yet | missing | TBD | TBD | TBD | open |
| Four hostile overmap AI sites | Not staged yet | missing | TBD | TBD | TBD | open |

Dedicated first-row scenario: `tools/openclaw_harness/scenarios/performance.bandit_one_site_remembered_lead_wait_30m.json`.

Earlier support-only instrumentation smokes: `bandit.camp_map_vanished_signal_redispatch`, run `.userdata/dev-harness/harness_runs/20260429_024952/`, emitted the same perf line shape but kept a yellow wait ledger and stale title/version metadata; first dedicated run `.userdata/dev-harness/harness_runs/20260429_025436/` was green but used the dirty pre-commit runtime. Both are retained only as probe support, not row credit.

## Code audit notes

Current live-path risk points to watch as site count rises:

1. `overmap_npc_move()` is the live turn-loop entry point. The bandit/cannibal signal scan is cadence-gated (`5_minutes` / `30_minutes`), but the top-level NPC travel loop still touches nearby overmap NPCs every turn.
2. `note_live_bandit_aftermath()` and `retire_empty_hostile_sites()` sit before the cadence gates. The instrumentation separates their cost from signal/dispatch cost so a per-turn cost can be spotted instead of hidden in total wait time.
3. Signal scan and dispatch planning should scale mostly with active/known hostile sites and nearby signal candidates, not with broad map state. The `signals`, `sites`, `active_sites`, and `active_job_mix` fields are the first guardrail against unexplained jumps.
4. Report/log construction can become its own cost if every site emits large text every cadence. The perf line is compact; existing behavior lines remain useful but should be watched for spam in 2/3/4-site runs.
5. Save serialization is outside the measured cadence slices unless a scenario explicitly saves; keep save/writeback timing separate from live wait timing.

## Next row work

1. Inspect/stage the smallest honest two-site fixture that mixes at least one bandit and one cannibal hostile-overmap-AI site, without reopening blocked player-lit fire work.
2. Run the same 30m measured live window and compare `bandit_live_world perf:` cost against the one-site row.
3. Extend only after that to three- and four-site fixtures, or name the staging blocker explicitly.
