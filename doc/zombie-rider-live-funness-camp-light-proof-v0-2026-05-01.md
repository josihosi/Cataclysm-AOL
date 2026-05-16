# Zombie rider live funness proof — camp-light convergence/band row v0

Date: 2026-05-01

Item: `CAOL-ZOMBIE-RIDER-0.3-v0`

Scenario: `zombie_rider.live_camp_light_band_mcw`

Run: `.userdata/dev-harness/harness_runs/20260501_030432/`

Verdict: GREEN for the staged-but-live camp-light convergence/band row only.

## Start conditions

- Fixture: `mcwilliams_live_debug_zombie_rider_camp_light_2026-05-01` from the McWilliams live-debug snapshot.
- Saved time: local midnight with mature-world turn `68256000`, audited as `00:00:00` and `world_age_days=735` in the live artifact row.
- Human targets: overmap NPCs removed and audited as zero by `audit_saved_no_human_npcs_before_rider_camp_light`.
- Active monsters: two hostile `mon_zombie_rider` monsters staged at offsets `[20, 20, 0]` and `[-20, 20, 0]`, each at least 12 tiles from the player.
- Camp lights: four saved `fd_fire` fields at offsets `[0, -20, 0]`, `[20, 0, 0]`, `[-20, 0, 0]`, and `[0, 20, 0]`, each intensity `3` and age `0`.
- Safe mode: toggled off before advancing the bounded live turn window.

## Credited evidence

Command after the fresh `cataclysm-tiles` relink:

```sh
python3 tools/openclaw_harness/startup_harness.py probe zombie_rider.live_camp_light_band_mcw
```

Green run:

- Report: `.userdata/dev-harness/harness_runs/20260501_030432/probe.report.json`
- Artifact log: `.userdata/dev-harness/harness_runs/20260501_030432/probe.artifacts.log`
- Step ledger: `8/8` green, no yellow/red/blocked rows.
- Feature classification: `feature-path`, `feature_proof=true`, `verdict=artifacts_matched`.
- Matched contract fields: `zombie_rider camp_light:`, `signal=yes`, `interest=yes`, `aggregate_sources=3`, `selected_riders=2`, `band_formed=yes`, and `posture=circle_harass`.
- Cleanup: harness terminated the launched `cataclysm-tiles` process normally after artifact capture.

Same-run artifact excerpts:

```text
bandit_live_world horde light signal: packet=live_light@139,41,0 kind=light source_omt=(139,41,0) horde_signal_power=22 range_cap_omt=11 ... projected_range_omt=11, visibility_score=7
bandit_live_world horde light signal: packet=live_light@140,40,0 kind=light source_omt=(140,40,0) horde_signal_power=22 range_cap_omt=11 ... projected_range_omt=11, visibility_score=7
bandit_live_world horde light signal: packet=live_light@140,42,0 kind=light source_omt=(140,42,0) horde_signal_power=22 range_cap_omt=11 ... projected_range_omt=11, visibility_score=7
zombie_rider camp_light: signal=yes source_omt=(139,41,0) world_age_days=735 horde_signal_power=30 aggregate_sources=3 aggregate_horde_signal_power=66 interest=yes interest_reason=exposed_bright_light interest_score=5 memory_active=yes riders_observed=2 selected_riders=2 cap=2 band_formed=yes band_size=2 convergence_reason=riders_converged_to_light_band posture=circle_harass posture_reason=band_without_breach_circles_and_harasses wounded_riders=0
```

The same run also logs a separate leftover single-source light row with `selected_riders=1`, `band_formed=no`, and `posture=investigate`; that row is not the credited band proof. The credited proof is the same-run clustered row above with `aggregate_sources=3`, capped `selected_riders=2`, `band_formed=yes`, and `posture=circle_harass`.

## Claim this row proves

A staged mature-world live camp-light cluster reaches the real live light observation path, reuses the existing horde light signal projection, aggregates nearby fire observations into a bounded rider signal, selects the capped two-rider band from active zombie riders, and chooses `circle_harass` rather than direct camp deletion.

This is live-shaped convergence/band footing: it proves the game path now bridges local camp-light observations into rider overmap interest and bounded band pressure instrumentation.

## Caveats / not yet proven

- This is one staged-but-live McWilliams row, not natural random discovery.
- The row proves the band decision/instrumented posture, not a full multi-minute tactical siege around camp geometry.
- It does not prove every camp-light configuration or indoor navigation case.
- It does not by itself close the full zombie rider live funness packet.
