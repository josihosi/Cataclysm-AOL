# Roof-fire horde detection proof v0 — 2026-04-29

## Status

**COMPLETE / GREEN SPLIT-RUN FEATURE PROOF**

This packet closes `Roof-fire horde detection proof packet v0` by using the materially different route Josef requested: split-run from the saved player-created roof-fire world produced by `.userdata/dev-harness/harness_runs/20260429_172847/`.

The proof chain is deliberately narrow:

1. source run `20260429_172847` created the roof/elevated fire through the normal player path and saved `t_tile_flat_roof` + `f_brazier` + `fd_fire`;
2. split fixture `roof_fire_horde_split_wait_from_player_fire_v0_2026-04-29` copied that saved world without adding an `fd_fire` transform;
3. scenario `bandit.roof_fire_horde_split_wait_from_player_fire_mcw` loaded the saved gameplay state, re-audited the roof fire, ran a bounded in-game wait, captured same-run live horde light-signal artifacts, saved, and audited turn/map/horde response metadata.

## Product fix included

The first split-run attempt showed the saved roof fire could be scanned but did not produce a horde light signal. Inspection found the live light packet path treated `here.is_outside( p )` as the only exposure gate, which did not classify elevated roof terrain as exposed.

`src/do_turn.cpp` now treats elevated `t_flat_roof` / `t_tile_flat_roof` fire sources as exposed roof light sources:

- helper: `live_bandit_fire_source_is_elevated_roof_exposed( const map &here, const tripoint_bub_ms &p )`;
- light packet exposure/open-terrain side leakage enabled for elevated roof fire;
- `vertical_sightline=true`, `elevation_bonus=2` when elevated roof exposure applies;
- debug notes include `elevated_roof_exposed=yes`.

This is the product bridge that made the live roof-fire signal honest instead of laundering source-zone or synthetic-fire proof into the roof case.

## Credited run

- Run: `.userdata/dev-harness/harness_runs/20260429_180239/`
- Scenario: `bandit.roof_fire_horde_split_wait_from_player_fire_mcw`
- Fixture: `roof_fire_horde_split_wait_from_player_fire_v0_2026-04-29`
- Source run: `.userdata/dev-harness/harness_runs/20260429_172847/`
- Report: `.userdata/dev-harness/harness_runs/20260429_180239/probe.report.json`
- Verdict: `artifacts_matched`
- Evidence class: `feature-path`
- Feature proof: `true`
- Step ledger: `14/14` green, no yellow/red/blocked rows
- Wait ledger: `green_wait_steps_proven`

## Evidence

### Source roof-fire state survived split-run load

`audit_saved_roof_fire_target_before_horde_wait.metadata.json`:

- player location ms `[3367,994,1]`, OMT `[140,41,1]`;
- target/east tile `[3368,994,1]`, OMT `[140,41,1]`;
- terrain `t_tile_flat_roof`;
- furniture `f_brazier`;
- field `fd_fire`, intensity `1`.

The fixture manifest has `"save_transforms": []`; it does not inject `fd_fire`.

### Bounded in-game wait was proven

`audit_saved_turn_before_roof_fire_horde_wait.metadata.json` recorded turn `5266942`.

`audit_saved_turn_after_roof_fire_horde_wait.metadata.json` recorded turn `5267242`, with:

- `observed_delta_turns=300`;
- `required_min_delta_turns=250`;
- `status=required_state_present`.

The wait step `wait_5_minutes_for_roof_fire_horde_signal` was green; no yellow/blocked wait ledger remained.

### Same-run live roof-fire horde light signal was captured

`audit_roof_fire_horde_light_signal_artifact.metadata.json` matched both required line groups:

- `bandit_live_world signal scan: signal_packet=yes ... light_packets=1`;
- `bandit_live_world horde light signal: ... horde_signal_power=20`.

Key captured line:

```text
18:03:02.770 INFO : bandit_live_world horde light signal: packet=live_light@140,41,1 kind=light source_omt=(140,41,1) horde_signal_power=20 range_cap_omt=8 weather=light concealment: verdict=allowed, weather=clear, time_penalty=0, weather_penalty=0, exposure=exposed, side_leakage=1, terrain=open, terrain_penalty=0, vertical_offset=0, vertical_sightline=yes, vertical_penalty=0, nearby_cross_z_visible=no, nearby_cross_z_bonus=0, elevation_bonus=2, elevated_exposure_bonus=2, elevated_exposure_extended=yes, storm_bright_light_preserved=no, projected_range_omt=8, visibility_score=6
```

### Horde before/after response metadata changed

Before wait — `audit_saved_horde_before_split_roof_fire_horde_wait.metadata.json`:

- `mon_zombie` at `[3367,874,1]`, offset `[0,-120,0]`;
- destination `[3367,874,1]`;
- `tracking_intensity=0`;
- `last_processed=0`;
- `moves=0`.

After wait/save — `audit_saved_horde_after_roof_fire_signal_wait.metadata.json`:

- same `mon_zombie` at `[3367,874,1]`, offset `[0,-120,0]`;
- destination retargeted to `[3360,984,1]`, offset `[-7,-10,0]`, the roof-fire source submap;
- `last_processed=5267242`;
- `moves=8400`;
- `status=required_state_present`.

Important boundary: `tracking_intensity` remained `0`, so this packet does **not** claim positive tracking-intensity proof. The credited horde response metadata is the saved horde retarget plus positive movement budget after the live horde light signal.

## Gates run

- `make TILES=1 -j4` — rebuilt `src/do_turn.cpp` into `cataclysm-tiles`; log `build_logs/roof_horde_rebuild_tiles_20260429_175652.log`.
- `./tests/cata_test "bandit_mark_generation_visible_light_horde_bridge_stays_bounded"` — passed `7 assertions in 1 test case`; log `build_logs/roof_horde_mark_generation_test_20260429_180650.log`.
- `python3 -m py_compile tools/openclaw_harness/startup_harness.py` — passed.
- `python3 -m json.tool tools/openclaw_harness/scenarios/bandit.roof_fire_horde_split_wait_from_player_fire_mcw.json` — passed.
- `python3 -m json.tool tools/openclaw_harness/fixtures/saves/live-debug/roof_fire_horde_split_wait_from_player_fire_v0_2026-04-29/manifest.json` — passed.
- `python3 tools/openclaw_harness/proof_classification_unit_test.py` — passed `8` tests; log `build_logs/roof_horde_proof_classification_unit_20260429_180650.log`.
- `git diff --check` — passed; log `build_logs/roof_horde_git_diff_check_20260429_180650.log`.

Direct image-model inspection was unavailable in this session, so visual proof is screenshot/OCR/ledger fallback plus saved metadata, not independent image-model confirmation.

## Evidence boundary

Closed claim: a saved, normal player-created roof/elevated fire can be loaded back into gameplay, survive metadata audit as `t_tile_flat_roof` + `f_brazier` + `fd_fire`, run through bounded elapsed in-game time, emit live roof-fire horde light-signal artifacts, and produce saved horde response metadata.

Not claimed:

- source-zone fire proof as roof proof;
- synthetic/fixture `fd_fire` as the credited fire;
- positive `tracking_intensity` after the wait;
- broader horde combat/pathfinding behavior beyond the saved horde retarget/move-budget response.
