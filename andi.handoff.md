# Andi handoff

Active lane: `CAOL-CI-RED-TRIAGE-v0`.

Status: ACTIVE / GREENLIT / ANDI NEXT.

Contract: `doc/ci-red-triage-packet-v0-2026-05-06.md`.

Standing build cadence: `doc/andi-build-cadence-note.md`.

## Current ask

Repair red `dev` GitHub Actions as its own bounded CI-health lane. This temporarily owns the active execution slot because branch/release-health claims are dishonest while CI is red.

Do not reopen `CAOL-DEFENDED-CAMP-SIGHT-SMOKE-HARDENING-v0` or rerun its sight/smoke rows by ritual. That packet remains checkpointed green pending Frau/Augerl review.

## Initial CI evidence

Latest observed red run:
- Run: `25371458600`
- Workflow: `General build matrix`
- Head: `5043f2c32c`
- Title: `Retitle Andi handoff for checkpointed camp smoke proof`
- URL: https://github.com/josihosi/Cataclysm-AOL/actions/runs/25371458600

Failed jobs included GCC 9/LTO, Clang 18 ASan, macOS 15 universal, oldest-supported Clang, GCC 14, and GCC 9 CMake.

First log clusters from `/tmp/caol-ci-25371458600/failed.log`:
- `faction_camp_test.cpp` current-target/patrol-alarm failures;
- `debug_menu_test.cpp` missing expected entry;
- `flesh_raptor_test.cpp` sight assertion false;
- `item_test.cpp` density failure for `zombie_rider_bone_bow`;
- `uncraft_test.cpp` yield drift;
- `zombie_rider_test.cpp` mature-gate/direct-entry failures.

## Success bar

Classify the current red CI set, fix the concrete branch-caused failures with the smallest honest changes, run focused local gates plus broader tests when practical, push to `origin/dev`, and verify the follow-up Actions run.

If CI remains red, park with exact run/job/log evidence and the next bounded target rather than declaring branch health from local tests.
