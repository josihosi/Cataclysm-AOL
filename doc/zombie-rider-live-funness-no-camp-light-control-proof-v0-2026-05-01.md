# Zombie rider live funness no-camp-light control proof v0 — 2026-05-01

## Claim

With the same mature-world/two-rider footing as the positive camp-light row, absence of camp-light fields produces no live horde-light signal and no zombie-rider camp-light convergence/band posture.

## Evidence

- Scenario: `zombie_rider.live_no_camp_light_control_mcw`
- Command: `python3 tools/openclaw_harness/startup_harness.py probe zombie_rider.live_no_camp_light_control_mcw`
- Run: `.userdata/dev-harness/harness_runs/20260501_032016/`
- Runtime: `cataclysm-tiles` rebuilt after removing `obj/tiles/version.o`; captured title `b821579e9c-dirty`, `version_matches_runtime_paths=true`
- Verdict: `feature-path`, `green`, `artifacts_matched`
- Step ledger: `8/8` green, no yellow/red/blocked rows
- Runtime warnings/abort: none

## Start conditions

- Mature-world midnight fixture: saved time `00:00:00` at the same mature-world turn family used by the positive camp-light row.
- Human NPC targets removed: saved-overmap NPC count `0`.
- Rider footing: two hostile `mon_zombie_rider` records staged at offsets `[20,20,0]` and `[-20,20,0]`, matching the positive camp-light convergence row's rider footing.
- No-light footing: saved-map audit at the four positive-row camp-light source offsets forbids `fd_fire`; `observed_forbidden_fields=[]` for `forbidden_fields=["fd_fire"]`.

## Decisive artifact

The negative log guard scanned the same-run artifact delta after the bounded 310-turn live window and found none of the forbidden camp-light/band traces:

```text
ABSENT after artifact baseline: zombie_rider camp_light:; bandit_live_world horde light signal:; selected_riders= && band_formed=yes; posture=circle_harass (scanned 14 line(s))
```

Source artifact: `.userdata/dev-harness/harness_runs/20260501_032016/audit_zombie_rider_no_camp_light_no_band_trace.log_delta.txt`.

## Harness support added

- `audit_saved_map_tile_near_player` now accepts `forbidden_terrain`, `forbidden_fields`, `forbidden_items`, `forbidden_furniture`, `forbidden_traps`, and `forbidden_radiation`, allowing absence footing such as “no `fd_fire` at these source offsets” to be a green metadata precondition instead of a yellow scan.
- Packaged probe reports now allow a successful `audit_log_not_contains` row to become an explicit negative artifact guard when no positive artifact pattern is appropriate.

## Caveat

This is a staged-but-live no-light source-hook control. It proves absence of the camp-light signal/convergence trace in a bounded live window on matched rider footing; it does not claim natural stealth, persistent overmap migration, or full camp siege/counter-siege navigation.
