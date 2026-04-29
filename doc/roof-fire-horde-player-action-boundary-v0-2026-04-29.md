# Roof-fire horde player-action boundary v0 — 2026-04-29

## Status

**PARTIAL GREEN / HORDE RESPONSE UNPROVEN / AGENT-SIDE ATTEMPT BUDGET EXHAUSTED**

This packet advances `Roof-fire horde detection proof packet v0` to a cleaner evidence boundary, but it does **not** close roof-fire horde detection.

Green scoped result:

- fixture/scenario can place the player on roof/elevated footing;
- horde setup metadata can be staged and audited;
- the credited roof fire is created through the normal player action path, not a fixture `fd_fire` transform;
- same-run guarded save/writeback can show roof/elevated `f_brazier` + `fd_fire`.

Missing result:

- no green bounded wait from the post-fire state;
- no same-run roof-fire horde light-signal artifact;
- no saved horde detection/response metadata after elapsed time.

## Files

- Fixture: `tools/openclaw_harness/fixtures/saves/live-debug/roof_fire_horde_player_action_v0_2026-04-29/manifest.json`
- Scenario: `tools/openclaw_harness/scenarios/bandit.roof_fire_horde_player_action_mcw.json`
- Harness support: `tools/openclaw_harness/startup_harness.py`
  - new setup-only `player_location_offset_ms` fixture transform;
  - `audit_saved_hordes_near_player` supports `min_tracking_intensity`.

## Evidence runs

### Attempt 1 — `.userdata/dev-harness/harness_runs/20260429_171831/`

Reached green setup footing:

- saved player location `[3367,994,1]`;
- player and east target terrain `t_tile_flat_roof`;
- staged `mon_zombie` horde present before action;
- normal Apply → `brazier` → `Deploy where?` → east/right UI trace green.

Blocked at the old visible-brazier look/OCR gate: `open_look_after_prompted_brazier_deploy.after.screen_text.json` had `line_count=0`, so the `Survivor` guard failed. This is not roof-fire proof.

### Attempt 2 — `.userdata/dev-harness/harness_runs/20260429_171949/`

Removed the brittle roof look/OCR gate. Reached:

- roof footing green;
- horde-before audit green;
- Apply → `lighter` selector green;
- `Light where?` opened and east/right was consumed;
- source-firewood prompt OCR appeared.

Blocked at post-confirm ignition OCR: missing `a fir`. This is not roof-fire proof.

### Attempt 3 — `.userdata/dev-harness/harness_runs/20260429_172338/`

After Frau Knackal approved replacing the OCR-only ignition gate with result-state evidence, this attempt tried a saved-player message-log guard. It blocked as `blocked_post_confirm_no_roof_fire_result` because no new player-message lines appeared.

Follow-up inspection showed the saved `#Wm9yYWlkYSBWaWNr.log` is an overmap event log and does not carry transient action messages such as `You successfully light a fire.` even in the already-green source-zone proof run. This message-log gate was therefore the wrong evidence surface.

### Attempt 4 — `.userdata/dev-harness/harness_runs/20260429_172847/`

Changed to guarded save/writeback plus saved roof-map metadata before attempting horde wait.

Green scoped roof-fire evidence:

- `audit_saved_roof_fire_target_before_horde_wait.metadata.json`
- `status=required_state_present`
- target/east roof tile at `[3368,994,1]` / OMT `[140,41,1]`
- terrain `t_tile_flat_roof`
- furniture `f_brazier`
- field `fd_fire`, intensity `1`
- no fixture `fd_fire` transform exists in the manifest

Non-green horde/wait evidence:

- `wait_5_minutes_for_roof_fire_horde_signal` was yellow: OCR/ledger saw the main menu after the guarded save, not a proven in-game wait path.
- `audit_roof_fire_horde_light_signal_artifact.metadata.json` was red: missing both `bandit_live_world signal scan: signal_packet=yes && light_packets=1` and `bandit_live_world horde light signal: && horde_signal_power=`.
- `probe.report.json`: `verdict=blocked_roof_fire_horde_light_signal_artifact_missing`, `feature_proof=false`.

Interpretation: the direct save/writeback gate proves the roof-fire result state, but `S` is a save-and-quit path, so this single scenario cannot both audit saved roof `fd_fire` before the wait and then continue into a live wait. Do not classify the horde part as tested.

## Evidence boundary

This packet proves **player-created roof/elevated fire writeback** on the shaped fixture. It does not prove horde detection, horde response, or elapsed-time roof-fire signaling.

A future agent-side path would need a materially different shape, for example:

1. split the proof into two stages, using the saved world from the player-created roof-fire run as the source for a second wait/horde scenario while keeping the provenance explicit; or
2. add/use a non-quitting save/writeback primitive so the same live session can save/audit roof `fd_fire` and then continue to bounded wait; or
3. have Josef manually play from the prepared fixture: create the roof fire, wait in-game, save, and provide the resulting save/log bundle.

No further same-item agent retry should run without one of those concrete changes.
