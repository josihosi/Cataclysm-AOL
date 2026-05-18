# Player-lit fire and bandit signal verification v0 (2026-04-29)

## Verdict

Green feature-path proof for a real player-created source-zone fire feeding the live bandit signal path during bounded time passage.

This proof starts from the green normal player ignition/writeback run `.userdata/dev-harness/harness_runs/20260429_153253/`, captures that saved world as a reviewable fixture, adds survivability support for waiting, then proves:

1. saved east target tile still contains the real player-created `f_brazier` + `fd_fire`;
2. mineral water and an `AUTO_DRINK` support zone are present before waiting;
3. the real wait UI opens and the 30-minute option is selected;
4. saved player `turn` advances by 1800 after guarded save/writeback;
5. same-run artifacts show live smoke/fire signal scan, matched bandit site maintenance, and a bandit dispatch plan with `candidate_reason=live_signal`;
6. saved `dimension_data.gsav` contains an active `bandit_camp` scout response targeting the player plus persisted `known_recent_marks` including `live_smoke@`.

## Fixture and scenario

Fixture:

- `player_lit_fire_bandit_signal_wait_v0_2026-04-29`
- path: `tools/openclaw_harness/fixtures/saves/live-debug/player_lit_fire_bandit_signal_wait_v0_2026-04-29/manifest.json`
- source run: `.userdata/dev-harness/harness_runs/20260429_153253/`
- source scenario: `bandit.live_world_nearby_camp_visible_brazier_source_zone_firestarter_action_mcw`
- source proof: green real player-action visible-brazier/source-zone ignition proof with saved `f_brazier` + `fd_fire`
- save transforms: 12 `water_mineral` items at player offset `[0,1,0]`, plus `OpenClaw auto drink wait support zone` (`AUTO_DRINK`, `your_followers`) covering the player and adjacent support tile.

Scenario:

- `bandit.player_lit_fire_signal_wait_mcw`
- path: `tools/openclaw_harness/scenarios/bandit.player_lit_fire_signal_wait_mcw.json`
- command: `python3 tools/openclaw_harness/startup_harness.py probe bandit.player_lit_fire_signal_wait_mcw`

## Green evidence

Run:

- `.userdata/dev-harness/harness_runs/20260429_162100/`
- verdict: `artifacts_matched`
- evidence class: `feature-path`
- feature proof: `true`
- step ledger: 14/14 green, `green_step_local_proof`
- wait ledger: one bounded 30-minute wait, `green_wait_step_proven`
- cleanup: launched `cataclysm-tiles --userdir .userdata/dev-harness/ --world McWilliams`, then auto-terminated with `SIGTERM` after artifact capture.

Pre-wait footing metadata:

- `audit_saved_player_lit_fire_target_before_signal_wait.metadata.json`
  - `status=required_state_present`;
  - player overmap tile `[140,41,0]`;
  - east target tile has required `f_brazier` and `fd_fire`;
  - no missing required field/furniture.
- `audit_saved_mineral_water_support_before_signal_wait.metadata.json`
  - `status=required_state_present`;
  - adjacent support tile has required `water_mineral`;
  - no missing required item.
- `audit_saved_auto_drink_zone_before_signal_wait.metadata.json`
  - `status=required_state_present`;
  - matching `AUTO_DRINK` / `your_followers` zone named `OpenClaw auto drink wait support zone` covers the player `[3367,994,0]` and support point `[3367,995,0]` in both zone files.

Wait/time evidence:

- `wait_30_minutes_for_player_lit_fire_bandit_signal.wait_menu.screen_text.json` captured the wait-duration menu and matched the `30 minutes` option.
- `wait_30_minutes_for_player_lit_fire_bandit_signal.wait_step_ledger` in `probe.report.json` recorded `choice_key=4`, `expected_duration=30m`, and `finish_or_interrupt_status=completed_by_artifact_delta`.
- `audit_saved_turn_after_player_lit_fire_signal_wait.metadata.json`
  - `status=required_state_present`;
  - baseline turn `5266942`;
  - post-save turn `5268742`;
  - `observed_delta_turns=1800`;
  - `required_min_delta_turns=1500`;
  - `missing_required_min_delta_turns=false`.

Same-run live signal/dispatch artifact:

`audit_player_lit_fire_signal_dispatch_artifact.log_delta.txt` includes the decisive path:

```text
16:21:23.708 INFO : bandit_live_world signal scan: signal_packet=yes kind=smoke/fire/light packets=1 smoke_packets=1 light_packets=0 scan_radius_ms=60 weather=clear light_time=night light_weather=clear
16:21:23.708 INFO : bandit_live_world signal maintenance: signal_packet=yes packets=1 matched_sites=1 refreshed_sites=1 matched_smoke_sites=1 matched_light_sites=0 rejected_by_system_range=0 rejected_by_signal_range=0 rejected_retired_empty_site=0 scan_radius_omt=40
16:21:24.100 INFO : bandit_live_world dispatch plan: site=overmap_special:bandit_camp@140,52,0 target=player@140,41,0 candidate_reason=live_signal job=scout selected_members=1 signal_packet=live_smoke@140,41,0 ... dispatch ready: scout toward player@140,41,0
- live_candidate reason=live_signal distance=11 cap=12 signal_packet=live_smoke@140,41,0 remembered_lead=none signal_distance=11
```

Same-run saved live-world metadata:

- `audit_saved_bandit_live_world_after_player_lit_signal_save.metadata.json`
  - `status=required_state_present`;
  - `observed_site_count=1`, `observed_matching_site_count=1`;
  - matching site `overmap_special:bandit_camp@140,52,0`, profile `camp_style`;
  - active group `overmap_special:bandit_camp@140,52,0#dispatch`;
  - active target `player@140,41,0`;
  - active job `scout`;
  - active member IDs `[4]`;
  - `known_recent_marks=["live_smoke@140,41,0", "player@140,41,0"]`;
  - a `smoke_signal` lead is present for `live_smoke@140,41,0`.

## Boundary

This closes `Player-lit fire and bandit signal verification packet v0` for a real player-created source-zone fire, bounded wait/time passage, live bandit signal scan/maintenance/dispatch, save/writeback, and persisted bandit live-world signal/active-state metadata.

It does **not** prove roof/elevated fire, horde detection/response, `fd_smoke`, or natural three/four-site player-pressure behavior. Direct image-model inspection was unavailable in this run, so screen proof is named screenshot artifact plus OCR/ledger fallback rather than independent image-model confirmation.
