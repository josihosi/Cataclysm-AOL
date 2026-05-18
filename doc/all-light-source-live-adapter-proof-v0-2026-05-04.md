# All-light-source live adapter proof v0 — 2026-05-04

Lane: `CAOL-JOSEF-LIVE-DEBUG-BATCH-v0`, Slice 5.

## Claim boundary

Credited as live/staged proof for the all-light-source adapter source/weather/log checkpoint and saved horde retarget-persistence metadata:

- fire/smoke plus exposed terrain/furniture/item/vehicle light sources feed the existing live signal packet adapter;
- raw weather/light-weather/wind/sight inputs are logged from the live adapter path;
- horde light-signal rows preserve current-source metadata, including `destination=`, `current_signal=yes`, and `horde_signal_power=`;
- after a guarded save/writeback, the staged ground-z `mon_zombie` horde remains in saved overmap metadata with the all-light destination and post-baseline processing timestamp.

Not claimed here: full horde response/devirtualization, local/reality-bubble zombie spawn-in, positive `tracking_intensity`, or tile-perfect overmap lighting. Local `mon_zombie` devirtualization is explicitly unproven for Slice 5; the green checkpoint is source/weather/log plus saved overmap horde retarget-persistence. Ja eh: the soup is edible, not a three-course miracle.

## Evidence

Build gate: `make -j4 TILES=1 cataclysm-tiles` completed and linked the runtime used by the live probes (`Cataclysm: Dark Days Ahead - 59db5f171e-dirty`). Earlier deterministic gate remains the Slice 5 code checkpoint: `git diff --check`; `make -j4 tests LINTJSON=0 ASTYLE=0`; `./tests/cata_test "[bandit][marks]" --reporter compact` -> 19 cases / 250 assertions.

Credited live probe: `.userdata/dev-harness/harness_runs/20260504_092840/` via `python3 tools/openclaw_harness/startup_harness.py probe --compact-stdout bandit.all_light_source_live_adapter_mcw`.

Run verdict:

- `verdict=artifacts_matched`
- `evidence_class=feature-path`
- `feature_proof=true`
- `step_ledger_summary.status=green_step_local_proof`
- `green_count=14`, `red_or_blocked_count=0`

Credited live artifact lines include:

- `bandit_live_world signal scan: signal_packet=yes kind=smoke/fire/light packets=5 smoke_packets=1 light_packets=4 ... weather=clear light_time=night light_weather=clear raw_weather=clear ...`
- four `bandit_live_world horde light signal:` rows with `destination=`, `current_signal=yes`, and `horde_signal_power=22` for exposed light packets at `140,41,0`, `140,42,0`, `141,42,0`, and `142,41,0`.

Saved-state/source footing and post-wait persistence:

- pre-wait fixture footing: saved `fd_fire`, `fd_smoke`, `f_exodii_lamp`, `flashlight_on`, `torch_lit`, and `smart_lamp_on` are present;
- fixture horde footing is ground-z: `player_location_ms=[3367,994,0]`; staged `mon_zombie` transform uses `offset_ms=[-228,0,0]` / `destination_offset_ms=[-228,0,0]`;
- the post-wait save prompt was guarded by OCR text containing `quit` and `Case Sensitive`, then uppercase `Y` was confirmed;
- `audit_player_save_mtime_after_all_light_save.metadata.json`: `status=required_state_present`, mtime changed after `audit_player_save_mtime_before_all_light_save`;
- `audit_saved_turn_after_all_light_wait.metadata.json`: `status=required_state_present`, saved turn advanced from `5266800` to `5267100`;
- `audit_saved_horde_after_all_light_signal_wait.metadata.json`: `status=required_state_present`, saved horde row retains `monster_id=mon_zombie`, `location_ms=[3165,994,0]`, `destination_ms=[3360,984,0]`, `tracking_intensity=0`, `last_processed=5266836`, `moves=-80`, `offset_ms=[-202,0,0]`.

## Result

Slice 5 is green for the intended all-light-source adapter checkpoint: source/weather/log handling, exposed-light horde signal metadata, guarded save/writeback, and saved overmap horde retarget-persistence are all green in `.userdata/dev-harness/harness_runs/20260504_092840/`.

Boundary remains explicit: this does not prove local active-zombie devirtualization or positive tracking intensity. Continue down the debug stack at Slice 6 unless Josef explicitly promotes a stricter horde response/devirtualization follow-up.
