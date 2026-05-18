# Writhing stalker zombie-shadow live escape-side proof v0 - 2026-05-01

## Scope

This is the second staged-but-live row for `CAOL-WRITHING-STALKER-ZOMBIE-SHADOW-PREDATOR-v0`.

Claim proved: with honest local evidence already present, the player retreats south from ordinary zombie pressure on the north/front side, and the live writhing-stalker shadow-destination path chooses a south/escape-side cutoff tile.

Scope caveat: this row deliberately does **not** claim overmap-interest footing. `writhing_stalker_live_context` still reports `overmap_interest=no` here, so the row is local-evidence-only and preserves the no-magic target-acquisition guardrail.

## Run

- Scenario: `writhing_stalker.live_escape_side_zombie_retreat_mcw`
- Fixture: `mcwilliams_live_debug_stalker_escape_side_zombie_retreat_2026-05-01`
- Command: `python3 tools/openclaw_harness/startup_harness.py probe writhing_stalker.live_escape_side_zombie_retreat_mcw`
- Run: `.userdata/dev-harness/harness_runs/20260501_071940/`
- Report: `.userdata/dev-harness/harness_runs/20260501_071940/probe.report.json`
- Step ledger: `.userdata/dev-harness/harness_runs/20260501_071940/probe.step_ledger.json`
- Same-run audit: `.userdata/dev-harness/harness_runs/20260501_071940/audit_writhing_stalker_escape_side_retreat_live_plan.metadata.json`

## Setup footing

The fixture starts from the McWilliams live-debug save, sets midnight vulnerability/local evidence, clears prior active monsters, and places:

- `mon_writhing_stalker` at offset `[0, 8, 0]`
- `mon_zombie` at `[0, -3, 0]`
- `mon_zombie` at `[-1, -4, 0]`
- `mon_zombie` at `[1, -4, 0]`

The scenario audits the saved midnight state and active-monster offsets, toggles safe mode off, sends two southward retreat inputs (`j`, `j`), then advances a bounded live window.

## Decisive live-path evidence

`probe.report.json` classifies the run as `feature-path`, `status=green`, `feature_proof=true`, with `7/7` step-local proof rows green, no abort, no runtime warnings, and cleanup terminated the launched `cataclysm-tiles` process.

Same-run `debug.log` audit matched the live plan and the actual shadow-destination cutoff path:

```text
07:19:57.464 INFO : writhing_stalker live_plan: decision=shadow route=cover_shadow reason=live_shadowing_before_strike_window opportunity=95 confidence=73 evidence=yes overmap_interest=no distance=7 zombie_pressure=3 target_bright=no stalker_bright=no target_focus=no cooldown=no eval_us=10
07:19:57.464 INFO : writhing_stalker quiet_cutoff: pressure_x=0 pressure_y=-3 pressure_count=3 dominant=yes ambiguous=no quiet_x=0 quiet_y=1 has_candidate=yes chosen_rel_x=-2 chosen_rel_y=4 score=69 quiet_alignment=1 crowding_penalty=0 reason=quiet_side_cutoff_preferred
```

This is the second bridge: north/front zombie pressure (`pressure_y=-3`) maps to the south/escape side (`quiet_y=1`) and a chosen cutoff tile behind the retreat line (`chosen_rel_y=4`) from the live shadow-destination path, not from confidence-total logs alone.

The same run also showed strike/cooldown rhythm after the cutoff window, e.g. `decision=strike ... confidence=73 ... zombie_pressure=3 ... cooldown=no`, followed by `decision=cooling_off ... cooldown=yes`, so the row did not prove a glued forever-chase.

## Metrics

- Confidence: `73` on the sampled live-plan row.
- Zombie pressure: `zombie_pressure=3`, `pressure_count=3`.
- Pressure side: north/front, `pressure_y=-3`.
- Chosen escape side: south, `quiet_y=1`.
- Chosen cutoff tile: `chosen_rel_x=-2`, `chosen_rel_y=4` on the first matched cutoff row; later sampled rows remain south/escape-side with positive `chosen_rel_y`.
- Turn-time cost in the matched shadow live-plan rows: max observed `eval_us=16`.
- Warnings/errors: no runtime warnings; no abort.
- Fun/fairness read: bounded and readable rather than magical — local evidence is required, overmap interest is explicitly not claimed, the pressure side and cutoff side are visible in the artifact, and cooldown breathes after strike pressure.

## Verdict

Passed for the second live row: local-evidence-only north/front zombie pressure during a player retreat reaches the live shadow-destination path and selects a south/escape-side cutoff tile.

Remaining caveat for v0 closure: the package still does not claim live overmap-interest footing. That is honest and intentional for this scoped close unless a later packet wires/logs overmap interest into `writhing_stalker_live_context`.
