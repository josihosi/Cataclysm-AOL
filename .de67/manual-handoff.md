# Manual handoff — 2026-09-20

Josef stopped DE67 3 and requested checkpoint commits, push to `origin/dev`, and worktree closure.
Autonomous dispatch, automatic reviews and restarts stay stopped until a new explicit owner start.
This is a development checkpoint, not a claim that the combined native regression campaign passed.

## Repository and retained evidence

Product: `josihosi/Cataclysm-AOL`, branch `dev`. Source, tests, reusable scenario/charter assets and
portable fixture snapshots are committed in separate code/harness/scenario/documentation slices.
Generated binaries, `.userdata` saves/registries, `.openclaw/tmp`, build logs and `.de67` runtime
state remain local. The complete worktree is preserved as an ordinary archive at:
`/Volumes/CodexBulk/Schanigarten/archives/Cataclysm-AOL-hostile-ecology-dev-20260920`.
Original absolute evidence paths under the closed worktree map to the same relative paths there.
No evidence was bulk-published merely to make the Git checkout clean.

The separate `de67-lab` repository's staged optional Pit Crew/funding work and installed DE67 skill
are outside this product push. Their existing staged/source state is preserved, not promoted here.

## Validation and unfinished work

- A current test build completed with `make -j6 TILES=1 SOUND=1 RELEASE=1 LOCALIZE=1 LANGUAGES=all LINTJSON=0 ASTYLE=0 TESTS=1 tests`.
- The selected 15 changed/new C++ cases ran on the rebuilt binary with seed 20260920:
  13 passed, 2 failed; 442 of 452 assertions passed. The two failures are
  `writhing_stalker_pressure_history_is_visible_absolute_and_lifecycle_bounded` and
  `writhing_stalker_pressure_history_survives_a_real_bubble_shift`. The runner also reported
  error logging. Preserve these failures; do not claim current combined history parity.
- The selected Python modules ran 197 tests: 196 passed. The existing
  `certification_round_registry_unit_test.test_successful_recheck_is_current_lifecycle_evidence_for_final_gate`
  still fails with `certification_round_lifecycle_not_active`, previously documented during registry
  adoption. No failure was suppressed.
- `git diff --check` passed for the checkpoint. No new cross-platform/native completion claim.
- Structured closeout review used `autoreview --mode branch --base 82eba6863c06f73f7326a02ed2d5e0de12947075`
  with the local manual-closeout prompt. It found one P2 regression in
  `src/writhing_stalker_ai.cpp::is_meaningful_pressure`: removing the attacking/closing qualification
  also removed the proximity/reachability boundary. Visible same-target distant pursuers can count
  as local pressure. Source inspection confirms the broadened predicate. This is an accepted manual
  follow-up, not a clean-review result; the stopped checkpoint preserves it without beginning another
  autonomous behavior-repair campaign.
- Relevant local results: `build_logs/manual-closeout-20260920/{build.log,cpp-tests-rebuilt.log,python-tests.log,review.json,review.txt}`.
  The current branch's GitHub Actions history is at
  https://github.com/josihosi/Cataclysm-AOL/actions?query=branch%3Adev . No new remote pass is claimed
  before the pushed checkpoint runs. Changed classes: C++ source/tests, Python harness/tests,
  JSON scenario/fixture assets, and documentation; no workflow changes.

The earlier accepted stalking-to-attack account and scoped cleanup proofs remain at their original
source/evidence ceilings. Fresh post-cleanup light, ownership/history and setup/diagnostics work
remains incomplete; see `R-CAOL-NATIVE-REGRESSION` in the ledger. Current stop does not settle those
obligations or erase task/claim history.

## Immediate manual diagnosis leads

1. Game waiting performance: PID77649 was near one full core while the ten-core Mac was roughly
   84–86% idle, with no interval swap traffic. In a short 2,214-sample main-thread profile, 1,368
   stacks were in the advancing-turn light pipeline, much of it loaded-map emitter discovery.
   Preserve actual sampling/expiry behavior while investigating the work per advancing turn.
   This is a profile lead, not a measured optimization. Profiles are in
   `.de67/task-logs/wait-performance-20260920/`.
2. Reconcile the pressure-locality regression and the two current failing history tests against the
   intended first-observation pursuit behavior; do not merely weaken their expectations to turn green.
3. Harness ergonomics: a scenario must not progress to a dependent step just because a real-time
   sleep elapsed. Shared action handling should distinguish pending, completed, interrupted and
   failed native work, own routine polling without repeated LLM turns, retain request identity on
   retries, and return the current input owner and valid next actions. Saving/reentry must be based
   on actual native save/quit/readiness. Existing bandit long-wait scripts exposed this completion
   boundary defect; use that route as a regression, not another guessed sleep duration.

No game or DE67 worker is intentionally retained as a running playtest. The supervisor's supported
stop command was used and orphaned old bridges received supported cleanup requests. The archive
retains saves and partial run evidence for manual inspection rather than claiming graceful native
save completion for an interrupted test.
