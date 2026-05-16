# Zombie rider live funness proof — cover/LOS escape row v0

Date: 2026-05-01

Item: `CAOL-ZOMBIE-RIDER-0.3-v0`

Scenario: `zombie_rider.live_cover_escape_mcw`

Run: `.userdata/dev-harness/harness_runs/20260501_021656/`

Verdict: GREEN for the staged-but-live cover/LOS escape row only.

## Start conditions

- Fixture: `mcwilliams_live_debug_zombie_rider_cover_escape_2026-05-01` from the McWilliams live-debug snapshot.
- Saved time: local noon, audited by `audit_saved_noon_before_rider_cover_escape`.
- Human targets: overmap NPCs removed and audited as zero by `audit_saved_no_human_npcs_before_rider_cover_escape`.
- Active monsters: one hostile `mon_zombie_rider` staged at offset `[10, 0, 0]` from the player.
- Cover: an 85-tile `f_locker` opaque wall, with sampled saved-map guard offsets at x `4`, `6`, and `8`, blocks line of sight between player and rider.
- Safe mode: toggled off before advancing the bounded live turn window.

## Credited evidence

Command:

```sh
python3 tools/openclaw_harness/startup_harness.py probe zombie_rider.live_cover_escape_mcw
```

Green run:

- Report: `.userdata/dev-harness/harness_runs/20260501_021656/probe.report.json`
- Artifact log: `.userdata/dev-harness/harness_runs/20260501_021656/probe.artifacts.log`
- Step ledger: `9/9` green, no yellow/red/blocked rows.
- Feature classification: `feature-path`, `feature_proof=true`, `verdict=artifacts_matched`.
- Runtime warnings: `0`; no abort.
- Cleanup: harness terminated the launched `cataclysm-tiles` process normally after artifact capture.

Same-run artifact excerpts:

```text
zombie_rider target_probe: target=no sees_player=no distance=10 line_of_fire=no hp=100 run=no eval_us=1
zombie_rider target_probe: target=no sees_player=no distance=9 line_of_fire=no hp=100 run=no eval_us=1
```

Counts from `probe.artifacts.log`:

- `zombie_rider target_probe:` rows: 104
- `decision=bow_pressure`: 0
- `line_of_fire=yes`: 0

## Claim this row proves

A staged live zombie rider behind opaque cover reaches the real `monster::plan()` no-target observation path and does not acquire or shoot the player through the wall during the bounded live window. This is a cover/LOS counterplay row: broken sight interrupts rider bow pressure instead of turning the endpoint monster into unavoidable wall-piercing damage.

## Caveats / not yet proven

- This is one staged-but-live McWilliams row, not natural discovery.
- The wall is harness-built opaque locker cover; this proves the line-of-sight break shape, not every indoor navigation case.
- It does not prove camp-light attraction into rider convergence/bands.
- It does not by itself close the full zombie rider live funness packet.
