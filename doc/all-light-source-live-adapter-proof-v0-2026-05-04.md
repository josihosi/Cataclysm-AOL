# All-light-source live adapter proof v0 — 2026-05-04

Lane: `CAOL-JOSEF-LIVE-DEBUG-BATCH-v0`, Slice 5.

## Claim boundary

Credited as live/staged proof for the all-light-source adapter source/weather/log checkpoint:

- fire/smoke plus exposed terrain/furniture/item/vehicle light sources feed the existing live signal packet adapter;
- raw weather/light-weather/wind/sight inputs are logged from the live adapter path;
- horde light-signal rows preserve current-source metadata, including `destination=`, `current_signal=yes`, and `horde_signal_power=`.

Not claimed here: full horde response/devirtualization, local/reality-bubble zombie spawn-in, or tile-perfect overmap lighting. The latest guarded retry proves save completion and then fails the active-zombie audit, so local `mon_zombie` devirtualization is explicitly unproven for Slice 5. Kleine Überraschung: the spoon stayed a spoon.

## Evidence

Build gate: `make -j4 TILES=1 cataclysm-tiles` completed and linked the runtime used by the live probes (`Cataclysm: Dark Days Ahead - 59db5f171e-dirty`). Earlier deterministic gate remains the Slice 5 code checkpoint: `git diff --check`; `make -j4 tests LINTJSON=0 ASTYLE=0`; `./tests/cata_test "[bandit][marks]" --reporter compact` -> 19 cases / 250 assertions.

Credited source/weather/log live probe: `.userdata/dev-harness/harness_runs/20260504_085531/` via `python3 tools/openclaw_harness/startup_harness.py probe bandit.all_light_source_live_adapter_mcw --compact-stdout`.

Credited live artifact lines include:

- `bandit_live_world signal scan: signal_packet=yes kind=smoke/fire/light packets=5 smoke_packets=1 light_packets=4 ... weather=clear light_time=night light_weather=clear raw_weather=clear ...`
- four `bandit_live_world horde light signal:` rows with `destination=`, `current_signal=yes`, and `horde_signal_power=22` for exposed light packets at `140,41,0`, `140,42,0`, `141,42,0`, and `142,41,0`.

Saved-state/source footing:

- pre-wait fixture footing: saved `fd_fire`, `fd_smoke`, `f_exodii_lamp`, `flashlight_on`, `torch_lit`, and `smart_lamp_on` are present;
- current fixture manifest stages the pre-wait `mon_zombie` horde at `offset_ms=[-228,0,0]` / `destination_offset_ms=[-228,0,0]`; this supersedes the earlier draft proof text that still named `[-120,0,0]`.

Guarded post-consult retry: `.userdata/dev-harness/harness_runs/20260504_090911/`.

- `audit_all_light_source_signal_artifacts.metadata.json`: `status=required_state_present`, `smoke_packets=1`, `light_packets=4`, `raw_weather=clear`, and exposed-light horde signal rows present.
- `audit_player_save_mtime_after_all_light_save_completion_guard.metadata.json`: `status=required_state_present`, mtime advanced after the save prompt and remained stable for about `2.0s`.
- `audit_saved_active_zombie_after_completed_all_light_save.metadata.json`: `status=required_state_missing`, `active_monsters=[]`, missing required nearby `mon_zombie`.

## Result

Slice 5 is green only for the intended source/weather/log adapter checkpoint. Horde-response/devirtualization is not green: the one guarded retry with a real completed-save gate still produced no active nearby `mon_zombie`. Do not rerun this lane just to polish the proof; move down the debug stack and treat local devirtualization as a future explicit follow-up if Josef asks for it.
