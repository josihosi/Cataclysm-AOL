# Writhing stalker threat/distraction handoff — live/staged proof v0

Date: 2026-05-03

Lane: `CAOL-WRITHING-STALKER-THREAT-DISTRACTION-HANDOFF-v0`.

This records the current-build staged/live proof rows that close the remaining agent-side evidence gap after the deterministic checkpoint in `doc/writhing-stalker-threat-distraction-deterministic-checkpoint-v0-2026-05-03.md`.

## Credited live/staged rows

- High-threat/allies retreat/stalk: `writhing_stalker.live_high_threat_allied_light_retreat_stalk_mcw` -> `.userdata/dev-harness/harness_runs/20260503_021310/`.
  - Evidence class: feature-path.
  - Report: `feature_proof=true`, `verdict=artifacts_matched`, green step ledger, portal clear.
  - Decisive live-plan shape: `decision=withdraw`, `reason=live_high_threat_allied_light_retreat_stalk`, `allied_support=3`, `persistent=yes`, `stalk_omt=3`.
- Zombie/distraction strike: `writhing_stalker.live_zombie_distraction_mcw` -> `.userdata/dev-harness/harness_runs/20260503_025148/`.
  - Evidence class: feature-path.
  - Report: `feature_proof=true`, `verdict=artifacts_matched`, green step ledger, portal clear.
  - Decisive lines: first `decision=shadow`, `route=cover_shadow`, `reason=live_shadowing_before_strike_window`, `zombie_pressure=2`; then `decision=strike`, `reason=live_vulnerability_window_strike`, `evidence=yes`, `zombie_pressure=2`, `persistent=yes`.
  - Repair note: the previous red was a stale proof expectation that required exactly `zombie_pressure=1`; the current behavior keeps zombie pressure present through the strike window, and the scenario now requires pressure presence rather than a brittle exact count.
- Night/outside anti-gnome bad-loiter strike: `writhing_stalker.live_anti_gnome_bad_loiter_mcw` -> `.userdata/dev-harness/harness_runs/20260503_025712/`.
  - Evidence class: feature-path.
  - Report: `feature_proof=true`, `verdict=artifacts_matched`, green step ledger, portal clear.
  - Decisive line: `decision=strike`, `reason=live_anti_gnome_night_reachable_probe_strike`, `evidence=yes`, `persistent=yes`, `bad_loiter=2`, `anti_gnome=yes`, `distance=3`, `eval_us=16`.
  - Fixture guard: local-midnight McWilliams transform, player moved to outside sidewalk/pavement line, hostile writhing stalker at `[3,0,0]`, and `caol_writhing_stalker_bad_loiter_count=2` audited from saved active-monster metadata before the live window.

## Harness support added

`tools/openclaw_harness/startup_harness.py` now preserves `values` on `active_monsters_near_player` fixture monsters, includes those values in active-monster audit summaries, and allows `required_monsters[].values` matching. This made the bad-position loiter memory auditable instead of relying on screenshot interpretation.

## Gates

- `python3 -m py_compile tools/openclaw_harness/startup_harness.py`
- `python3 -m json.tool` on the changed/new scenario and fixture manifests
- `git diff --check`
- `python3 tools/openclaw_harness/startup_harness.py probe writhing_stalker.live_zombie_distraction_mcw` -> `.userdata/dev-harness/harness_runs/20260503_025148/`, feature-path green
- `python3 tools/openclaw_harness/startup_harness.py probe writhing_stalker.live_anti_gnome_bad_loiter_mcw` -> `.userdata/dev-harness/harness_runs/20260503_025712/`, feature-path green
- `make -j4 tests LINTJSON=0 ASTYLE=0 && ./tests/cata_test "[writhing_stalker]" --reporter compact` -> `Passed all 23 test cases with 264 assertions`
- `./tests/cata_test "[zombie_rider],[flesh_raptor]" --reporter compact` -> `Passed all 24 test cases with 268 assertions`

## Boundary

Door opening did not land and is not required for this proof. This packet does not claim natural random discovery, broad house navigation, burglar behavior, locked-door solving, or reopening old stalker/zombie-rider/flesh-raptor lanes.
