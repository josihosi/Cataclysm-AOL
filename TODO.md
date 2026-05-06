# TODO

Short execution queue only.

Remove finished items when they are done.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active lane: `CAOL-CI-RED-TRIAGE-v0`.

Status: ACTIVE / GREENLIT / ANDI NEXT.

Contract: `doc/ci-red-triage-packet-v0-2026-05-06.md`.

Current problem: `dev` CI is red. Latest observed run `25371458600` on `5043f2c32c` failed `General build matrix` across multiple jobs. First suspicious clusters from `/tmp/caol-ci-25371458600/failed.log` are `faction_camp_test` current-target/patrol-alarm failures, `debug_menu_test` missing entry, `flesh_raptor_test` sight setup, `item_test` density for `zombie_rider_bone_bow`, `uncraft_test` yield drift, and `zombie_rider_test` mature-gate/direct-entry failures.

Next concrete steps:
- classify every current red CI cluster with exact job/test/log evidence;
- reproduce locally where practical or identify source/data/test cause where CI-only;
- make the smallest honest repair(s), preserving product contracts;
- run focused local gates plus broader gate when practical;
- push to `origin/dev` and verify the follow-up GitHub Actions run;
- if still red, park with exact remaining run/job/log evidence and the next bounded fix target.

Do not rerun completed defended-camp sight/smoke rows as ritual. CI repair owns the active slot until branch health is green or precisely parked.
