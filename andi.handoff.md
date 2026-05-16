# Andi handoff

Active lane: none currently promoted for Andi code work.

Most recent lane: `CAOL-CI-RED-TRIAGE-v0`.

Status: CLOSED / CHECKPOINTED GREEN / ACTIONS VERIFIED at code head `cb21294168`.

Contract: `doc/ci-red-triage-packet-v0-2026-05-06.md`.

Standing build cadence: `doc/andi-build-cadence-note.md`.

## Current ask

The temporary CI-repair workloop should stand down once any docs-only closure/checkpoint push is verified. Do not reopen `CAOL-DEFENDED-CAMP-SIGHT-SMOKE-HARDENING-v0` or rerun its sight/smoke rows by ritual. That packet remains checkpointed green pending Frau/Augerl review.

## CI closure evidence

Initial red run:
- Run: `25371458600`
- Workflow: `General build matrix`
- Head: `5043f2c32c`
- Title: `Retitle Andi handoff for checkpointed camp smoke proof`
- URL: https://github.com/josihosi/Cataclysm-AOL/actions/runs/25371458600

Repair stack from `29cb5bbb97` through `cb21294168` handled the branch-caused red clusters: zombie rider data/tests, camp patrol/current-target setup, debug-menu expectation, flesh-raptor sight setup, NPC zone-sort ASan completion, and layered bedroom terrain item allowance.

Final green Actions at code head `cb21294168`:
- `General build matrix` run `25462728843`: completed success.
- `Cataclysm Windows build` run `25462728845`: completed success.

## Next

If a docs-only closure alignment commit requeues GitHub Actions, verify it only; do not make churn commits while queued/running. If it completes green, send the concise completion note and disable temporary cron `6b58c068-2a6c-4160-9da0-c0bd1eefa877`.
