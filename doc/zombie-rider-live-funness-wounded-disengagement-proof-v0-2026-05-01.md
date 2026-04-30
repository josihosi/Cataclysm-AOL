# Zombie rider live funness proof — wounded disengagement row v0

Date: 2026-05-01

Item: `CAOL-ZOMBIE-RIDER-0.3-v0`

Scenario: `zombie_rider.live_wounded_disengagement_mcw`

Run: `.userdata/dev-harness/harness_runs/20260501_014613/`

Verdict: GREEN for the staged-but-live wounded-rider disengagement row only.

## Start conditions

- Fixture: `mcwilliams_live_debug_zombie_rider_wounded_2026-05-01` from McWilliams live-debug snapshot.
- Saved time: local noon, audited by `audit_saved_noon_before_wounded_rider`.
- Active monsters: exactly one staged hostile `mon_zombie_rider` required at offset `[0, -8, 0]` from the player before the live window.
- Rider health: fixture writes `hp: 80`, observed by live-plan rows as `hp=36` percent.
- Safe mode: toggled off before advancing the bounded live turn window.
- Runtime freshness: first two attempts found the right artifact line but were yellow because the GUI binary still identified as `95ec285a7c-dirty`; after forcing a real executable relink (`rm -f obj/version.o cataclysm-tiles && make -j4 TILES=1 LINTJSON=0 ASTYLE=0`), the credited run captured `Cataclysm: Dark Days Ahead - 7ecdd41f03-dirty` with `version_matches_runtime_paths=true`.

## Credited evidence

Command:

```sh
python3 tools/openclaw_harness/startup_harness.py probe zombie_rider.live_wounded_disengagement_mcw
```

Green run:

- Report: `.userdata/dev-harness/harness_runs/20260501_014613/probe.report.json`
- Artifact log: `.userdata/dev-harness/harness_runs/20260501_014613/probe.artifacts.log`
- Step ledger: `6/6` green, no abort.
- Runtime warnings: `0`; no crash or harness abort observed.
- Feature classification: `feature-path`, `feature_proof=true`, `verdict=artifacts_matched`.

Same-run artifact excerpts:

```text
zombie_rider live_plan: decision=withdraw reason=wounded_or_close distance=7 line_of_fire=yes hp=36 run=no eval_us=1
zombie_rider live_plan: decision=withdraw reason=wounded_or_close distance=24 line_of_fire=no hp=36 run=no eval_us=1
```

Decision counts from `probe.report.json` / `probe.artifacts.log`:

- `withdraw`: 14
- `bow_pressure`: 0
- total `zombie_rider live_plan:` rows: 14
- observed distance range: `7` -> `24`
- max observed `eval_us`: 3

## Claim this row proves

A staged live wounded zombie rider at visible bow range reaches the real `monster::plan()` path and chooses wounded/close withdrawal instead of greedy final-shot pressure. The rider keeps widening distance across the bounded turn window, with no crash, no runtime mismatch, and no log-spam/perf smell in the credited artifact rows.

## Caveats / not yet proven

- This is one staged-but-live McWilliams row, not natural discovery.
- It does not prove camp-light attraction into rider convergence/bands.
- It does not prove cover/indoor escape.
- It does not by itself close the full zombie rider live funness packet.
