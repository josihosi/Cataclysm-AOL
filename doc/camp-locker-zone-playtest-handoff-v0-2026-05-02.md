# Andi handoff — CAOL-CAMP-LOCKER-ZONE-PLAYTESTS-v0

Classification: ACTIVE / GREENLIT / LOCKER-ZONE PLAYTEST PACKET.

Imagination source: `doc/camp-locker-zone-playtests-imagination-source-2026-05-02.md`.

Contract: `doc/camp-locker-zone-playtest-packet-v0-2026-05-02.md`.

## Task

Josef explicitly wants locker zone playtests. Run a bounded current-build playtest/proof pass for actual camp locker zone behavior. This is the newly promoted concrete locker seam after `CAOL-CAMP-LOCKER-EQUIPMENT-API-REDUCTION-v0` closed.

## Use first

Inspect and use the existing scenario surface before inventing new machinery:

- `locker.create_zone_probe`
- `locker.display_toggle_probe`
- `locker.zone_manager_save_probe_mcw`
- `locker.weather_wait`
- `locker.package5_robbie_e2e_verified_mcw`

## Success bar

Provide a final packet that says, for each credited row:

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

## If blocked

If the harness cannot drive or observe the zone path honestly, write a concise Josef playtest card with exact manual steps, expected outcomes, and what result closes the row. Preserve the blocker in canon; do not close green from helper tests alone.
