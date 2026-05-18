# Roof-fire horde nice-fire proof v0 — 2026-04-30

## Status

**COMPLETE / GREEN FEATURE-PATH PROOF**

This closes `CAOL-ROOF-HORDE-NICE-FIRE-v0`: Josef's promoted “get horde playtested with a nice roof fire” lane.

The run uses the existing honest player-created roof-fire chain as footing, not the old mixed-hostile soup:

1. source run `.userdata/dev-harness/harness_runs/20260429_172847/` created the elevated/roof fire through the normal player path and saved `t_tile_flat_roof` + `f_brazier` + `fd_fire`;
2. split fixture `roof_fire_horde_split_wait_from_player_fire_v0_2026-04-29` preserved that saved world with no `fd_fire` transform;
3. new named scenario `bandit.roof_fire_horde_nice_roof_fire_mcw` loaded that footing, re-audited roof fire and horde state, waited a bounded 5 minutes through the harness wait path, captured live roof-fire horde signal artifacts, saved, and audited the horde response metadata.

## Credited run

- Run: `.userdata/dev-harness/harness_runs/20260430_191556/`
- Scenario: `bandit.roof_fire_horde_nice_roof_fire_mcw`
- Fixture: `roof_fire_horde_split_wait_from_player_fire_v0_2026-04-29`
- Source roof-fire run: `.userdata/dev-harness/harness_runs/20260429_172847/`
- Prior split proof footing: `.userdata/dev-harness/harness_runs/20260429_180239/`
- Report: `.userdata/dev-harness/harness_runs/20260430_191556/probe.report.json`
- Verdict: `artifacts_matched`
- Evidence class: `feature-path`
- Feature proof: `true`
- Step ledger: `14/14` green, no yellow/red/blocked rows
- Wait ledger: `green_wait_steps_proven`

## Evidence

### Nice roof fire before the wait

`audit_saved_roof_fire_target_before_horde_wait.metadata.json` proved:

- player location ms `[3367,994,1]`, OMT `[140,41,1]`;
- scanned roof/elevated tiles are `t_tile_flat_roof`;
- required furniture `f_brazier` is present;
- required field `fd_fire` is present;
- fixture manifest has `save_transforms: []`, so the credited fire remains tied to the source normal player-action roof-fire path rather than a fresh fixture fire injection.

`audit_saved_roof_fire_target_after_wait.metadata.json` kept the same required `t_tile_flat_roof` + `f_brazier` + `fd_fire` state after the bounded wait/save.

### Horde before the wait

`audit_saved_horde_before_split_roof_fire_horde_wait.metadata.json` proved a plausible pre-wait horde footing:

- `mon_zombie` horde at `[3367,874,1]`, offset `[0,-120,0]` from player;
- destination `[3367,874,1]` before signaling;
- `tracking_intensity=0`, `last_processed=0`, `moves=0`.

This is setup/footing only; it is not credited as behavior proof until the after-wait response changes.

### Bounded time passage and roof-fire signal

`audit_saved_turn_before_roof_fire_horde_wait.metadata.json` recorded turn `5266942`.

`audit_saved_turn_after_roof_fire_horde_wait.metadata.json` recorded turn `5267242`, with `observed_delta_turns=300` and `required_min_delta_turns=250`.

Same-run artifact line from `audit_roof_fire_horde_light_signal_artifact.metadata.json`:

```text
19:16:20.182 INFO : bandit_live_world horde light signal: packet=live_light@140,41,1 kind=light source_omt=(140,41,1) horde_signal_power=20 range_cap_omt=8 weather=light concealment: verdict=allowed, weather=clear, time_penalty=0, weather_penalty=0, exposure=exposed, side_leakage=1, terrain=open, terrain_penalty=0, vertical_offset=0, vertical_sightline=yes, vertical_penalty=0, nearby_cross_z_visible=no, nearby_cross_z_bonus=0, elevation_bonus=2, elevated_exposure_bonus=2, elevated_exposure_extended=yes, storm_bright_light_preserved=no, projected_range_omt=8, visibility_score=6
```

### Horde response after the wait

`audit_saved_horde_after_roof_fire_signal_wait.metadata.json` proved direct response metadata after the live roof-fire signal and save:

- same `mon_zombie` horde remains at `[3367,874,1]`, offset `[0,-120,0]`;
- destination retargeted to `[3360,984,1]`, the roof-fire source submap vicinity;
- `last_processed=5267242`;
- `moves=8400`;
- `status=required_state_present`.

Boundary: `tracking_intensity` remained `0`, so this proof does **not** claim positive tracking-intensity behavior. The credited response is retarget/destination plus processed-turn and positive movement-budget evidence after a same-run roof-fire horde signal.

## Cost / stability metrics

- Bounded wait: `5m`, observed `300` turns.
- End-to-end harness wall-clock: `2:34.72` for `python3 tools/openclaw_harness/startup_harness.py probe bandit.roof_fire_horde_nice_roof_fire_mcw`.
- Step proof: `14/14` green.
- Wait proof: `1/1` green.
- Runtime warnings: none in `probe.report.json`.
- Abort: `null`.
- Cleanup: launched `cataclysm-tiles` process was terminated by harness cleanup with status `terminated`.
- Debug artifact matched required roof-fire signal lines; captured debug artifact line count was `14`.
- Horde-specific timing: `not instrumented` separately.
- Binary freshness note: the captured window title reports an older source commit string, but `probe.report.json` records `version_matches_runtime_paths=true` and `runtime_relevant_diff_since_capture=[]`; later commits were docs/harness-scenario canon, not runtime source changes.

## Gates run

- `python3 tools/openclaw_harness/startup_harness.py probe bandit.roof_fire_horde_nice_roof_fire_mcw` — green feature-path run; log `build_logs/roof_fire_horde_nice_probe_20260430_1916.log`.
- `./tests/cata_test "bandit_mark_generation_visible_light_horde_bridge_stays_bounded"` — passed `7 assertions in 1 test case`; log `build_logs/roof_fire_horde_nice_mark_generation_test_20260430_1922.log`.
- `python3 tools/openclaw_harness/proof_classification_unit_test.py` — passed `8` tests; log `build_logs/roof_fire_horde_nice_proof_classification_unit_20260430_1922.log`.
- `python3 -m json.tool tools/openclaw_harness/scenarios/bandit.roof_fire_horde_nice_roof_fire_mcw.json` — passed; log `build_logs/roof_fire_horde_nice_json_check_20260430_1922.log`.
- `git diff --check` — passed; log `build_logs/roof_fire_horde_nice_git_diff_check_20260430_1928.log`.

## Evidence boundary

Closed claim: a normal player-created roof/elevated brazier fire from the previous green source chain can be reused as a credible nice roof-fire footing, survive same-run metadata audit before and after bounded waiting, emit the live roof-fire horde light-signal path, and produce saved horde response metadata: retargeted destination, processed turn, and positive movement budget.

Not claimed:

- rerun mixed-hostile proof as horde behavior proof;
- synthetic fire injection as the credited fire;
- positive `tracking_intensity`;
- horde-specific micro-timing;
- broad combat/pathfinding or natural multi-day horde discovery.
