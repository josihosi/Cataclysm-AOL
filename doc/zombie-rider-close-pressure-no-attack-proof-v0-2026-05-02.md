# Zombie rider close-pressure no-attack proof v0 (2026-05-02)

Status: CLOSED / CHECKPOINTED GREEN V0 / BUGFIX + PRODUCT-FEEL FOLLOW-UP

Contract: `doc/zombie-rider-close-pressure-no-attack-packet-v0-2026-05-02.md`.

## Verdict

The decision-to-action bridge bug is named and fixed. The fresh current-build live row is green.

Root cause: rider live planning could choose `decision=bow_pressure reason=line_of_fire` and point the destination at the avatar, but it did not force `aggro_character`. The monster gun actor rejects non-monster targets when the shooter is not character-aggro, so a good-looking plan line could still fail to produce a visible avatar shot.

## Implemented fix

`src/monmove.cpp` now:

- records rider bow resources in the live trace: `special_ready`, `ammo`, and aggro state;
- when line of fire, bow range, special cooldown, and ammo are ready, sets `aggro_character=true` and raises anger before bow handoff;
- keeps wounded riders withdrawing under `reason=wounded_rider_disengages`;
- sends close/cooldown/no-ammo/no-LOS pressure to named irregular bunny-hop/reposition destinations instead of polite loitering or point-blank bow shots;
- records named reasons such as `too_close_bunny_hop`, `bow_cooldown_reposition`, `bow_no_ammo_reposition`, and `no_line_of_fire_reposition`.

`data/json/monsters/zed_misc.json` carries Josef's corrected zombie-rider description.

The harness active-monster fixture path now preserves scripted monster ammo and audits it, because the live rider proof needs a real staged archer, not a silently empty save payload.

## Deterministic proof

Command:

```sh
git diff --check && make -j4 tests/zombie_rider_test.o tests src/monmove.o LINTJSON=0 ASTYLE=0
./tests/cata_test "[zombie_rider]"
```

Result:

- build gate: passed;
- focused rider suite: `All tests passed (199 assertions in 16 test cases)`.

Covered rows include:

- `zombie_rider_bow_pressure_marks_avatar_hostile_before_shooting`: rider starts `aggro_character=false`, has line of fire/bow/ammo, flips aggro before the shot, and consumes one arrow.
- `zombie_rider_close_pressure_bunny_hops_without_point_blank_bow_shot`: close pressure repositions instead of spending a point-blank bow shot.
- `zombie_rider_close_indoor_pressure_repositions_instead_of_loitering`: indoor-ish pressure gets a non-loitering reposition destination.
- Existing rider rows still cover wounded withdrawal, blocked-LOS/cover counterplay, endpoint/evolution footing, mature light attraction, no-light/daylight/weak-light controls, capped rider-band convergence, and no wall-suicide/perfect direct camp breach behavior.

## Build and live proof

Current-build tiles relink is green:

```sh
./just_build_macos.sh > /tmp/caol-zombie-rider-tiles-build3.log 2>&1
```

Result: exit `0`; `cataclysm-tiles` linked successfully. The remaining linker output is macOS/Homebrew deployment-target warnings, not the previous missing `Character` thunk failure.

Fresh live row:

```sh
python3 tools/openclaw_harness/startup_harness.py probe zombie_rider.live_open_field_pressure_mcw
```

Run: `.userdata/dev-harness/harness_runs/20260502_050055/`.

Credited evidence:

- `probe.report.json`: `evidence_class=feature-path`, `feature_proof=true`, `verdict=artifacts_matched`, no abort, no runtime warnings.
- `probe.step_ledger.json`: `green_step_local_proof`, `6/6` green rows.
- `audit_saved_zombie_rider_before_open_field.metadata.json`: one hostile `mon_zombie_rider` at offset `[0,-5,0]` with `ammo={"arrow_wood":18}` and no missing required monsters.
- `audit_zombie_rider_open_field_live_plan.log_delta.txt`: live plan reaches bow pressure and then close-pressure repositioning. Decisive lines include:
  - `decision=bow_pressure reason=line_of_fire distance=4 line_of_fire=yes ... special_ready=yes ammo=18 aggro_before=no aggro_after=yes`
  - later bow-pressure rows with ammo decreasing (`17`, `16`, ...), proving the action handoff consumes arrows;
  - `decision=reposition reason=too_close_bunny_hop distance=3 line_of_fire=yes pressure_line_of_fire=yes ... aggro=yes`.

## Closure caveat

This is staged-but-live McWilliams feature proof, not natural random discovery or a full siege/navigation claim. The v0 closure covers the named close-pressure/no-attack seam only; broader zombie-rider v0 remains closed on its previous caveats.
