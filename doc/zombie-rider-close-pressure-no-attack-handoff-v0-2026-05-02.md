# Andi handoff — CAOL-ZOMBIE-RIDER-CLOSE-PRESSURE-NO-ATTACK-v0

Classification: CLOSED / CHECKPOINTED GREEN V0 / BUGFIX + PRODUCT-FEEL FOLLOW-UP.

Read first:
- Closure proof: `doc/zombie-rider-close-pressure-no-attack-proof-v0-2026-05-02.md`
- Contract: `doc/zombie-rider-close-pressure-no-attack-packet-v0-2026-05-02.md`
- Imagination source: `doc/zombie-rider-close-pressure-no-attack-imagination-source-2026-05-02.md`

Root cause fixed: `decision=bow_pressure reason=line_of_fire` did not force `aggro_character`, while the monster gun actor refuses avatar shots when the shooter is not character-aggro.

What landed:
- bow pressure now marks the rider character-aggro and raises anger before bow handoff when line of fire, bow range, cooldown, and ammo are ready;
- close/cooldown/no-ammo/no-LOS pressure now chooses named irregular bunny-hop/reposition destinations instead of loitering or point-blank bow shots;
- rider live traces include `special_ready`, `ammo`, and aggro state;
- harness active-monster save transforms preserve/audit scripted ammo for staged archer proof;
- corrected zombie-rider description text is in JSON.

Evidence:
- `git diff --check && make -j4 tests/zombie_rider_test.o tests src/monmove.o LINTJSON=0 ASTYLE=0` passed.
- `./tests/cata_test "[zombie_rider]"` passed: `199 assertions in 16 test cases`.
- `./just_build_macos.sh > /tmp/caol-zombie-rider-tiles-build3.log 2>&1` exited `0`; `cataclysm-tiles` linked.
- Fresh live row `zombie_rider.live_open_field_pressure_mcw` -> `.userdata/dev-harness/harness_runs/20260502_050055/`: `feature_proof=true`, `verdict=artifacts_matched`, `green_step_local_proof`, no abort/runtime warnings; saved rider starts with `ammo={"arrow_wood":18}`; live log shows `aggro_before=no aggro_after=yes`, arrow ammo decrement after bow pressure, and `decision=reposition reason=too_close_bunny_hop` at close pressure.

Caveat: staged-but-live McWilliams proof only; not natural random discovery or full siege/navigation proof. Do not reopen broader zombie-rider v0 without a fresh promoted follow-up.
