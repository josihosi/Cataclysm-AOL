# CAOL-CAMP-LOCKER-ZONE-PLAYTESTS-v0 (2026-05-02)

Status: CLOSED / CHECKPOINTED YELLOW V0 / JOSEF MANUAL CARD WRITTEN

Closure readout: `doc/camp-locker-zone-playtest-proof-v0-2026-05-02.md`.

Imagination source: `doc/camp-locker-zone-playtests-imagination-source-2026-05-02.md`.

## Intent

Run a bounded locker-zone playtest pass after `CAOL-CAMP-LOCKER-EQUIPMENT-API-REDUCTION-v0` closed. The goal is to prove or honestly package the player-facing zone behavior, not to reopen the closed API-reduction refactor.

Josef asked explicitly for locker zone playtests. Treat this as the newly promoted concrete seam that the closed locker refactor said future locker work requires.

## Scope v0

Use the current harness and fixture surface first, especially:

- `locker.create_zone_probe`
- `locker.display_toggle_probe`
- `locker.zone_manager_save_probe_mcw`
- `locker.weather_wait`
- `locker.package5_robbie_e2e_verified_mcw`

Pick the smallest set that honestly answers the live/product question. Prefer current-build live/handoff proof over repeating old deterministic gates.

The packet should cover, if feasible:

1. A real `CAMP_LOCKER` zone exists or can be created/saved through the intended path.
2. Camp locker service draws from zone stock and produces an observable worker/loadout or carried-cleanup result.
3. Zone boundaries are respected, including `NO_NPC_PICKUP` / excluded stock where relevant.
4. Weather/wait or practical gear pressure remains believable on the live path when zone stock is present.
5. Evidence is split into screen/UI fact, saved-state fact, deterministic gate, artifact/log excerpt, and verdict.

## Success state

- [x] The playtest pass chooses a bounded v0 set instead of reopening all locker-zone history.
- [x] At least one current-build harness/live row proves real `CAMP_LOCKER` zone footing with a scenario/run id and artifact path, or records the exact harness/UI blocker.
- [x] At least one row proves camp locker service behavior from zone stock on the real service path, not only helper classification.
- [x] At least one row checks boundary/exclusion behavior such as `NO_NPC_PICKUP`, non-locker stock, or saved zone persistence.
- [x] Weather/wait or practical gear behavior is either playtested on a live/harness row or explicitly scoped out with a reason.
- [x] Screenshots/OCR/save audits name the visible or persisted fact they prove; raw JSON and startup/load are not credited as gameplay proof.
- [x] If the harness cannot honestly prove a required row, `runtime/josef-playtest-package.md` or a repo doc gets a concise Josef manual playtest recipe with expected outcomes and closure criteria.
- [x] `Plan.md`, `TODO.md`, `SUCCESS.md`, `TESTING.md`, `doc/work-ledger.md`, and `andi.handoff.md` agree on the final state before closure.

## Non-goals

- Do not reopen `CAOL-CAMP-LOCKER-EQUIPMENT-API-REDUCTION-v0`; it is closed green at its scoped refactor boundary.
- Do not redesign basecamp missions, Smart Zones, general NPC equipment selection, fashion scoring, or basecamp zone presets.
- Do not drift into bandits, zombie riders, writhing stalkers, flesh raptors, or release packaging.
- Do not claim live product success from deterministic tests alone.
- Do not ask Josef to inspect logs as the main playtest activity.

## Validation expectations

Follow the C-AOL harness skill. Choose the evidence class before each row.

Minimum final packet for each credited row:

- claim
- scenario/command and artifact directory
- screen or OCR fact, or `not the relevant evidence class`
- deterministic test/build gate, if code changed or if needed as footing
- saved-state/artifact/log excerpt
- verdict: passed / failed / inconclusive

If two materially changed harness attempts hit the same blocker, stop and either consult Frau Knackal before retry 3 or write the Josef manual playtest packet.

## First implementation step

Inventory the existing locker scenarios above, then run the smallest current-build proof set that can answer zone creation/persistence, service use from zone stock, and boundary behavior. Update canon only after the evidence says whether this is green, blocked, or needs a manual Josef playtest card.
