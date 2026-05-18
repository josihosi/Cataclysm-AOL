# Andi handoff — CAOL-CAMP-LOCKER-ZONE-PLAYTESTS-v0

Classification: CLOSED / CHECKPOINTED GREEN V0.

Closure readout: `doc/camp-locker-zone-playtest-proof-v0-2026-05-02.md`.

Imagination source: `doc/camp-locker-zone-playtests-imagination-source-2026-05-02.md`.

Contract: `doc/camp-locker-zone-playtest-packet-v0-2026-05-02.md`.

## Task

Josef explicitly wanted locker zone playtests. The bounded current-build playtest/proof pass is complete and closed green after the scenario evidence contract was repaired. This was the newly promoted concrete locker seam after `CAOL-CAMP-LOCKER-EQUIPMENT-API-REDUCTION-v0` closed.

## Used first

The pass inspected the existing scenario surface before inventing new machinery:

- `locker.create_zone_probe`
- `locker.display_toggle_probe`
- `locker.zone_manager_save_probe_mcw`
- `locker.weather_wait`
- `locker.package5_robbie_e2e_verified_mcw`

## Success bar

The final packet provides, for each credited row:

- claim
- scenario/command and run/artifact path
- screen/OCR visible fact, or why screen is not the evidence class
- saved-state/artifact/log excerpt
- test/build gate if relevant
- verdict: passed / failed / inconclusive

Must answer at least these questions or name the blocker:

1. Does current-build proof show real `CAMP_LOCKER` zone footing?
2. Does camp locker service use zone stock on the real service path?
3. Are boundaries/exclusions such as `NO_NPC_PICKUP`, non-locker stock, or saved zone persistence respected?
4. Is weather/wait or practical gear behavior live-playtested, or explicitly out of scope with reason?

## Non-goals

- No broad locker ontology archaeology.
- No reopening the closed API-reduction lane.
- No Smart Zone/basecamp preset redesign.
- No bandit/rider/stalker/raptor drift.
- No deterministic-only claim if the contract says live/player-facing behavior.

## Closure result

The repaired proof rows are green: `locker.zone_manager_save_probe_mcw` at `.userdata/dev-harness/harness_runs/20260502_041828/` has `feature_proof=true`, matched `Basecamp: Locker` and `Probe Locker` `CAMP_LOCKER` UI-trace rows, and saved metadata for the persistent `Basecamp: Locker` zone; `locker.weather_wait` at `.userdata/dev-harness/harness_runs/20260502_041300/` has `feature_proof=true` and same-run `camp locker:` queued/plan/after/serviced artifacts from `locker_tiles=1` stock. `[camp][locker]` is green for `NO_NPC_PICKUP`, off-zone, and policy-disabled boundary guards. `locker.package5_robbie_e2e_verified_mcw` remains blocked/no-credit but is not needed for this bounded v0 closure.
