# CAOL-CI-RED-TRIAGE-v0

Date: 2026-05-06
Owner: Andi
Status: ACTIVE / GREENLIT

## Why this exists

GitHub Actions is red on `dev`. This blocks honest branch/release-health claims and must be repaired as its own bounded lane instead of being mixed into the completed defended-camp smoke proof.

Latest observed red run:
- Run: `25371458600`
- Workflow: `General build matrix`
- Head: `5043f2c32c`
- Title: `Retitle Andi handoff for checkpointed camp smoke proof`
- URL: https://github.com/josihosi/Cataclysm-AOL/actions/runs/25371458600

Failed jobs in that run:
- `GCC 9, Curses, LTO`
- `Clang 18, Ubuntu, Tiles, ASan`
- `macOS 15, Apple Clang 17, Tiles, Sound, x64 and arm64 Universal Binary`
- `Basic Build and Test (Clang oldest supported, Ubuntu, Curses)`
- `GCC 14, Ubuntu, Curses`
- `GCC 9, Ubuntu, Tiles, Sound, CMake`

Local downloaded failed log for Schani's first triage snapshot:
- `/tmp/caol-ci-25371458600/failed.log`

## First suspicious failure clusters

Do not assume these are exhaustive; use them as triage seed rows.

From the latest failed log:
- `tests/faction_camp_test.cpp` around lines `933`, `934`, `947`, `1013`, `1019`, `1056`, `1057`, `1061`:
  - `guard.current_target()` unexpectedly non-null / wrong target.
  - `test_camp->is_patrol_alarm_active()` unexpectedly true.
- `tests/debug_menu_test.cpp:31`:
  - required debug menu entry not found.
- `tests/flesh_raptor_test.cpp:199`:
  - `eigenspectre.sees( here, you )` false.
- `tests/item_test.cpp:842`:
  - `zombie_rider_bone_bow` density `1.5f` exceeds max `1.25f` (`9000000mg`, `6000ml`).
- `tests/uncraft_test.cpp:174`:
  - uncraft yield expected approx `500`, got `444`.
- `tests/zombie_rider_test.cpp:456`, `461`:
  - rider spawn entry starts at `0` instead of mature gate; direct entries count `2` instead of `1`.

Earlier same-head neighborhood also had red `Cataclysm Windows build` and `General build matrix` on `ab5f88b67d`; after the latest dev repair, re-check whether Windows is still red or was superseded.

## Scope

Fix branch CI health for `josihosi/Cataclysm-AOL` `dev`.

In scope:
- reproduce or inspect the current CI failures;
- classify failures as code/data/test/CI-environment;
- make the smallest honest repair(s);
- update tests/data/docs only when they encode the intended product contract;
- push the fix to `origin/dev`;
- confirm the new GitHub Actions run reaches green, or park with a precise remaining red job/log if it cannot be finished in this pass.

Out of scope:
- reopening `CAOL-DEFENDED-CAMP-SIGHT-SMOKE-HARDENING-v0` proof rows by ritual;
- broad product redesign;
- release packaging or release-health claims before CI is green;
- hiding failures by weakening tests without preserving/clarifying the intended contract;
- touching unrelated old closed lanes except where their data/tests are directly causing CI red.

## Success state

- [ ] Latest relevant `dev` CI failure set is classified with exact failing jobs/tests.
- [ ] Local/narrow reproduction or source-level cause is recorded for each fixed cluster.
- [ ] Concrete fixes are committed and pushed to `origin/dev`.
- [ ] At minimum, local gates cover every touched cluster; broader gate is run when practical.
- [ ] GitHub Actions on the pushed fix is green for the relevant branch-health workflows, or the remaining red state is parked with exact run/job/log evidence and the next bounded fix target.

## Evidence bar

Preferred local gates, adjusted to actual touched files:
- `git diff --check`
- focused `./tests/cata_test` filters for touched clusters (`[camp]`, `[zombie_rider]`, `[flesh_raptor]`, relevant item/data/debug menu tests)
- broader `make -j4 tests LINTJSON=0 ASTYLE=0` when practical before claiming branch-health repair
- GitHub Actions run status after push

If full local reproduction is impractical because CI differs from this Mac, do not pretend. Use source inspection plus focused local gates, push a narrow fix, and verify in Actions.
