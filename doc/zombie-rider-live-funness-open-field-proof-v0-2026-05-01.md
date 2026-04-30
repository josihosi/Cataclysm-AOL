# Zombie rider live funness proof — open-field local-combat row v0

Date: 2026-05-01

Item: `CAOL-ZOMBIE-RIDER-0.3-v0`

Scenario: `zombie_rider.live_open_field_pressure_mcw`

Run: `.userdata/dev-harness/harness_runs/20260501_013055/`

Verdict: GREEN for the first live-shaped local-combat row only.

## Start conditions

- Fixture: `mcwilliams_live_debug_zombie_rider_open_field_2026-05-01` from McWilliams live-debug snapshot.
- Saved time: local noon, audited by `audit_saved_noon_before_rider_open_field`.
- Active monsters: exactly one staged hostile `mon_zombie_rider` required at offset `[0, -5, 0]` from the player before the live window.
- Safe mode: toggled off before advancing the bounded live turn window.
- Runtime was freshly rebuilt with `make -j4 TILES=1 LINTJSON=0 ASTYLE=0` before this probe.

## Credited evidence

Command:

```sh
python3 tools/openclaw_harness/startup_harness.py probe zombie_rider.live_open_field_pressure_mcw
```

Green run:

- Report: `.userdata/dev-harness/harness_runs/20260501_013055/probe.report.json`
- Artifact log: `.userdata/dev-harness/harness_runs/20260501_013055/probe.artifacts.log`
- Step ledger: `6/6` green, no abort.
- Runtime warnings: `0`; no crash or harness abort observed.

Same-run artifact excerpts:

```text
zombie_rider live_plan: decision=bow_pressure reason=line_of_fire distance=5 line_of_fire=yes hp=100 run=no eval_us=2
zombie_rider live_plan: decision=withdraw reason=post_shot_or_close distance=3 line_of_fire=yes hp=100 run=no eval_us=1
```

Decision counts from `probe.artifacts.log`:

- `bow_pressure`: 1
- `withdraw`: 2
- total `zombie_rider live_plan:` rows: 3
- max observed `eval_us`: 2

## Claim this row proves

A staged live zombie rider at visible bow/pressure range reaches the real `monster::plan()` path, records ranged `bow_pressure`, then records close-pressure `withdraw`/repositioning instead of collapsing into a static melee dogpile. This is live-shaped local-combat evidence, not just JSON presence or startup/load proof.

## Caveats / not yet proven

- This is one staged-but-live McWilliams row, not natural discovery.
- It does not prove camp-light attraction into rider convergence/bands.
- It does not prove cover/indoor escape or wounded-rider disengagement.
- It does not by itself close the full zombie rider live funness packet.
