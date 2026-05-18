# Writhing stalker zombie-shadow live quiet-side proof v0 - 2026-05-01

## Scope

This is the first staged-but-live row for `CAOL-WRITHING-STALKER-ZOMBIE-SHADOW-PREDATOR-v0`.

Claim proved: with honest local evidence already present, ordinary zombies pressing the east/front side of the player feed the live writhing-stalker shadow-destination path and choose a west/quiet-side cutoff tile.

Scope caveat: this row deliberately does **not** claim overmap-interest footing. `writhing_stalker_live_context` still reports `overmap_interest=no` here, so the row is local-evidence-only and preserves the no-magic target-acquisition guardrail.

## Run

- Scenario: `writhing_stalker.live_quiet_side_zombie_pressure_mcw`
- Fixture: `mcwilliams_live_debug_stalker_quiet_side_zombie_pressure_2026-05-01`
- Command: `python3 tools/openclaw_harness/startup_harness.py probe writhing_stalker.live_quiet_side_zombie_pressure_mcw`
- Run: `.userdata/dev-harness/harness_runs/20260501_071548/`
- Report: `.userdata/dev-harness/harness_runs/20260501_071548/probe.report.json`
- Step ledger: `.userdata/dev-harness/harness_runs/20260501_071548/probe.step_ledger.json`
- Same-run audit: `.userdata/dev-harness/harness_runs/20260501_071548/audit_writhing_stalker_quiet_side_pressure_live_plan.metadata.json`

## Setup footing

The fixture starts from the McWilliams live-debug save, sets midnight vulnerability/local evidence, clears prior active monsters, and places:

- `mon_writhing_stalker` at offset `[8, 0, 0]`
- `mon_zombie` at `[3, 0, 0]`
- `mon_zombie` at `[4, 1, 0]`
- `mon_zombie` at `[4, -1, 0]`

The scenario audits the saved midnight state and active-monster offsets before advancing the live window.

## Decisive live-path evidence

`probe.report.json` classifies the run as `feature-path`, `status=green`, `feature_proof=true`, with `6/6` step-local proof rows green, no abort, no runtime warnings, and cleanup terminated the launched `cataclysm-tiles` process.

Same-run `debug.log` audit matched the live plan and the actual shadow-destination cutoff path:

```text
07:16:06.727 INFO : writhing_stalker live_plan: decision=shadow route=cover_shadow reason=live_shadowing_before_strike_window opportunity=95 confidence=73 evidence=yes overmap_interest=no distance=5 zombie_pressure=3 target_bright=no stalker_bright=no target_focus=no cooldown=no eval_us=28
07:16:06.728 INFO : writhing_stalker quiet_cutoff: pressure_x=3 pressure_y=0 pressure_count=3 dominant=yes ambiguous=no quiet_x=-1 quiet_y=0 has_candidate=yes chosen_rel_x=-1 chosen_rel_y=-4 score=66 quiet_alignment=1 crowding_penalty=0 reason=quiet_side_cutoff_preferred
```

This is the important bridge Frau asked for: not just total confidence, but the live zombie-pressure side (`pressure_x=3`), the quiet side (`quiet_x=-1`), and the chosen cutoff tile (`chosen_rel_x=-1`, `chosen_rel_y=-4`) from the live shadow-destination path.

## Metrics

- Confidence: `73` on the sampled live-plan row.
- Zombie pressure: `zombie_pressure=3`, `pressure_count=3`.
- Pressure side: east/front, `pressure_x=3`.
- Chosen quiet side: west, `quiet_x=-1`.
- Chosen cutoff tile: `chosen_rel_x=-1`, `chosen_rel_y=-4` on the first matched cutoff row; later rows remain west/quiet-side (`chosen_rel_x=-1` or `-2`).
- Turn-time cost in the matched live-plan rows: max observed `eval_us=38`.
- Warnings/errors: no runtime warnings; no abort.
- Strike timing/counterplay/fun notes: not closed by this row. This row proves the scoped quiet-side/back-route shadow selection; the second live row still needs retreat/escape-side cutoff behavior and the final packet still needs the broader fun/fairness read.

## Verdict

Passed for the scoped first live row: local-evidence-only zombie pressure from one side reaches the live shadow-destination path and selects the opposite quiet-side cutoff tile.

Still pending for the package: a staged-but-live retreat/running row where the player escapes zombie pressure and the stalker occupies a readable escape-side/cutoff tile, plus final strike/counterplay/fun-fairness notes before v0 closure.
