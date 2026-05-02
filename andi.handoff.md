# Andi handoff

Last closed lane: `CAOL-ZOMBIE-RIDER-CLOSE-PRESSURE-NO-ATTACK-v0`.

Closure proof: `doc/zombie-rider-close-pressure-no-attack-proof-v0-2026-05-02.md`.

State: closed green v0 after deterministic bridge proof, successful `cataclysm-tiles` relink, and fresh live feature proof.

Evidence:
- `git diff --check && make -j4 tests/zombie_rider_test.o tests src/monmove.o LINTJSON=0 ASTYLE=0` passed.
- `./tests/cata_test "[zombie_rider]"` passed: `199 assertions in 16 test cases`.
- `./just_build_macos.sh > /tmp/caol-zombie-rider-tiles-build3.log 2>&1` exited `0`.
- `zombie_rider.live_open_field_pressure_mcw` -> `.userdata/dev-harness/harness_runs/20260502_050055/`: `feature_proof=true`, `verdict=artifacts_matched`, `green_step_local_proof`, no abort/runtime warnings; saved rider ammo audited as `arrow_wood=18`; live log shows bow-pressure aggro bridge, ammo decrement, and close `too_close_bunny_hop` reposition.

Next active candidate from repo canon: `CAOL-VISIONS-PLAYTEST-SAMPLER-v0` unless Schani/Josef promote a different greenlit item.
