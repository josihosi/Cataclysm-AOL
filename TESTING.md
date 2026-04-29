# TESTING

Current validation policy and current evidence only.

This file is not a trophy wall.
Remove stale or completed fluff instead of stacking crossed-off test history forever.

## Validation policy

Use the **smallest evidence that honestly matches the change**.

- **Docs-only change** -> no compile
- **Small local code change** -> narrow build and the narrowest relevant test target
- **Broad or risky code change** -> broader rebuild and relevant filtered tests
- **Before a Josef handoff** or after suspicious stale-binary behavior -> rebuild the real targets and rerun the real smoke path

### Agent vs Josef

Schani should do agent-side playtesting first whenever the harness or direct in-game probing can answer the question.
Josef should be asked only for:
- product judgment
- tone/feel
- human-priority choices
- genuinely human-only interaction

Josef being unavailable is **not** a blocker by itself.
Josef self-testing is **not** a plan blocker and does not belong in the active success state as a gate.
If Josef-specific checks are useful, write them down as non-blocking notes so he can run them later from his own list.
If another good agent-side lane exists, keep moving there.
If several human-only judgments are likely soon, batch them instead of sending tiny separate asks.

### Anti-churn rule

Do not keep rerunning the same startup or test packet when it is no longer the missing evidence class.
If startup/load is already green, and the missing proof is live behavior, then the next probe must target live behavior.
If a target is merely waiting on Josef, do not keep revalidating it unless the code changed.

Current debug-stack attempt rule for the same item/blocker:
- attempts 1-2 may be Andi solo retries, including multiple focused harness attempts in one cron run when each attempt changes setup, instrumentation, or evidence class
- before attempt 3, consult Frau Knackal
- attempts 3-4 are the final changed retries after consultation
- after attempt 4, if code is implemented but proof still fails, add a concise implemented-but-unproven packet to Josef's playtest package and move to the next greenlit debug note; do not close it and do not park it as dead

### Test-to-game wiring rule

A test is not allowed to impersonate implementation. Before claiming gameplay behavior, identify the live code path that consumes the tested seam and name the evidence class that proves it: unit/evaluator, playback/proof packet, live source hook, harness/startup, screen, save inspection, or artifact/log. Deterministic-only packets may close only as deterministic-only packets; if the contract says the game does something, the proof must reach the game path or the claim stays open.

### Bandit overmap-proof rule

For the remaining bandit AI proof packets, single-turn deterministic checks are **not** enough by themselves.
The honest bar now includes real overmap-side multi-turn scenario proof, up to `500` turns where needed, with explicit per-scenario goals and tuning metrics.


## Current validation targets

### Recently completed validation target - Fuel normal-map entry primitive packet v0

`Fuel normal-map entry primitive packet v0` is **COMPLETE / GREEN NORMAL-MAP ENTRY GATE** under `C-AOL actual playtest verification stack v0`. Contracts: `doc/c-aol-actual-playtest-verification-stack-v0-2026-04-27.md`, `doc/fuel-normal-map-entry-primitive-packet-v0-2026-04-29.md`, and imagination source `doc/fuel-normal-map-entry-primitive-imagination-source-of-truth-2026-04-29.md`.

Evidence: `.userdata/dev-harness/harness_runs/20260429_140645/`, scenario `bandit.live_world_nearby_camp_source_zone_normal_map_entry_mcw`, command `python3 tools/openclaw_harness/startup_harness.py probe bandit.live_world_nearby_camp_source_zone_normal_map_entry_mcw`. The run is feature-path green with `verdict=artifacts_matched`, 5/5 green step-local ledger, matched `game::load: Finalizing end`, saved charged wielded `lighter`, saved `f_brazier` + real `log` items, saved `SOURCE_FIREWOOD`, and screenshot `normal_map_entry_gate_before_activation.png` with OCR fallback including `Wield:` and standalone `YOU`.

Evidence boundary: direct model image inspection was unavailable, so screen proof is screenshot artifact path plus OCR fallback rather than independent image-model inspection. This primitive sends no activation/targeting/fire/lighter action keys and does **not** close normal ignition, save mtime, `fd_fire`/`fd_smoke`, smoke/light, or bandit signal proof.

### Recently completed validation target - Fuel writeback repair via wood source zone packet v0

`Fuel writeback repair via wood source zone packet v0` is **COMPLETE / GREEN NORMAL PLAYER IGNITION + SAVED `fd_fire`** under `C-AOL actual playtest verification stack v0`. Contracts: `doc/c-aol-actual-playtest-verification-stack-v0-2026-04-27.md`, `doc/fuel-writeback-repair-via-wood-source-zone-packet-v0-2026-04-29.md`, `doc/fuel-visible-brazier-source-zone-firestarter-action-v0-2026-04-29.md`, and imagination source `doc/fuel-writeback-repair-via-wood-source-zone-imagination-source-of-truth-2026-04-29.md`.

Retired/non-proof surfaces remain retired: the old `fuel_writeback_source_zone_v0_2026-04-29` fixture is broken and must not be used for fire/lighter proof; `.userdata/dev-harness/harness_runs/20260429_142257/` remains a prior failed diagnostic because apply-wielded `A` did not show `Light where?`.

Green evidence: `.userdata/dev-harness/harness_runs/20260429_153253/`, scenario `bandit.live_world_nearby_camp_visible_brazier_source_zone_firestarter_action_mcw`, command `python3 tools/openclaw_harness/startup_harness.py probe bandit.live_world_nearby_camp_visible_brazier_source_zone_firestarter_action_mcw`. The run is feature-path green with `verdict=artifacts_matched`, `feature_proof=true`, 31/31 green step-local ledger, and claim-scoped UI trace artifacts proving normal Apply inventory returned `brazier`, `Deploy where?` consumed east/right, normal Apply inventory returned `lighter`, exact `Light where?` opened before targeting, and `Light where?` consumed east/right.

Action/writeback evidence: OCR matched `burn your firewood source` at `target_direction_east_to_visible_brazier_logs.after.screen_text.json`, then matched ignition guard `a fir` from `Dau successfillu linht a fire` at `confirm_burn_source_firewood_prompt_after_visible_brazier.after.screen_text.json`, then matched `Save and quit` before uppercase save confirmation. Same-run writeback is proven by `audit_player_save_mtime_after_visible_brazier_ignition_save.metadata.json` (`mtime_changed_since_required_label=true`, `save_mtime_iso=2026-04-29T15:34:04.648315`) and `audit_saved_target_tile_for_visible_brazier_fd_fire.metadata.json` (`observed_furniture=[f_brazier]`, `observed_field_ids=[fd_fire]`, no missing required field/furniture).

Evidence boundary: this closes real player-action source-zone ignition and same-run saved `fd_fire` on the visibly deployed brazier/log tile. It does **not** prove bandit signal response, roof/elevated fire, horde response, long-duration smoke/light behavior, or `fd_smoke`; those remain separate stack items. Direct image-model inspection was not used, so screenshot/OCR evidence remains named-artifact plus OCR fallback.

### Recently completed validation target - Player-lit fire and bandit signal verification packet v0

`Player-lit fire and bandit signal verification packet v0` is **COMPLETE / GREEN REAL-FIRE BANDIT SIGNAL RESPONSE** under `C-AOL actual playtest verification stack v0`. Canonical proof: `doc/player-lit-fire-bandit-signal-verification-v0-2026-04-29.md`.

Evidence: `.userdata/dev-harness/harness_runs/20260429_162100/`, scenario `bandit.player_lit_fire_signal_wait_mcw`, command `python3 tools/openclaw_harness/startup_harness.py probe bandit.player_lit_fire_signal_wait_mcw`. The run is feature-path green with `verdict=artifacts_matched`, `feature_proof=true`, 14/14 green step-local ledger rows, and one green bounded wait ledger row.

Footing/writeback evidence: fixture `player_lit_fire_bandit_signal_wait_v0_2026-04-29` was captured from the green real-fire run `.userdata/dev-harness/harness_runs/20260429_153253/`, then installed with 12 adjacent `water_mineral` items plus an `AUTO_DRINK` / `your_followers` zone named `OpenClaw auto drink wait support zone`. Same-run metadata proves the saved east tile still has `f_brazier` + `fd_fire`, the water and auto-drink zone are present, save mtime advanced after the post-wait save, and saved turns advanced from `5266942` to `5268742` (`observed_delta_turns=1800`, required minimum `1500`).

Signal evidence: `audit_player_lit_fire_signal_dispatch_artifact.log_delta.txt` includes live `bandit_live_world signal scan: signal_packet=yes` with `smoke_packets=1`, `signal maintenance` with `matched_sites=1`, and `dispatch plan` for `overmap_special:bandit_camp@140,52,0` targeting `player@140,41,0` with `candidate_reason=live_signal`, `job=scout`, `selected_members=1`, and `signal_packet=live_smoke@140,41,0`. `audit_saved_bandit_live_world_after_player_lit_signal_save.metadata.json` persists matching `camp_style` active scout state, active member `[4]`, and `known_recent_marks=["live_smoke@140,41,0", "player@140,41,0"]`.

Evidence boundary: this closes source-zone player-lit fire -> bandit signal response only. It does **not** prove roof/elevated fire, `fd_smoke`, horde response, or natural three/four-site pressure. Direct image-model inspection was unavailable, so screen proof remains named screenshot/OCR/ledger fallback rather than independent image-model confirmation.

### Active validation target - Roof-fire horde detection proof packet v0

`Roof-fire horde detection proof packet v0` is the next separate downstream target. The proof must not reuse source-zone fire/signal evidence as roof/horde closure. Debug may stage the horde/distance, but not the fire. The target needs roof/elevated player footing, real player-created roof fire/light/smoke, bounded time passage or exact blocker prompt screenshots/key handling, and claim-scoped horde before/after detection or response metadata. A proven no-detect/no-response outcome is acceptable if the roof/fire/time footing is green and the horde metadata is decisive.

Completed performance context remains preserved but inactive: `doc/c-aol-live-ai-performance-matrix-v0-2026-04-29.md` keeps the green one-site, two-site, pre-staged three-site, and pre-staged four-site performance-load rows, latest `.userdata/dev-harness/harness_runs/20260429_041936/`. Do not rerun those rows as ritual; only rerun performance if Schani/Josef promotes a new performance row.

### Recently completed validation target - Cannibal camp confidence-push live playtest packet v0

`Cannibal camp confidence-push live playtest packet v0` is complete as **confidence uplift green**. Matrix: `doc/cannibal-camp-confidence-push-live-playtest-matrix-v0-2026-04-29.md`.

Checkpoint evidence boundary:

- bandit contrast control: `.userdata/dev-harness/harness_runs/20260429_012915/`, scenario `bandit.extortion_first_demand_fight_mcw`, current runtime `7ca870f6be`, 6/6 green step ledger, feature proof, shakedown/pay/fight surface preserved;
- cannibal day smoke/pressure: `.userdata/dev-harness/harness_runs/20260429_013310/`, scenario `cannibal.live_world_day_smoke_pressure_mcw`, current runtime `782d8edabd`, 2/2 green step ledger, feature proof, live smoke/signal path with `profile=cannibal_camp`, `darkness_or_concealment=no`, `shakedown=no`, `combat_forward=no`;
- cannibal night/contact pack-forward: `.userdata/dev-harness/harness_runs/20260429_014900/`, scenario `cannibal.live_world_night_local_contact_pack_mcw`, current runtime `acfe6fd0ce`, 2/2 green step ledger, feature proof, local-contact `attack_now` pack with `pack_size=2`, `darkness_or_concealment=yes`, `shakedown=no`, `combat_forward=yes`;
- reload brain create/save: `.userdata/dev-harness/harness_runs/20260429_021849/`, scenario `cannibal.live_world_day_smoke_persistence_mcw`, current runtime `e778902cac`, 7/7 green step ledger, feature proof, guarded save mtime writeback, saved active `cannibal_camp` stalk group with active members `[4,5]`, target `player@140,41,0`, and `known_recent_marks` including `live_smoke@140,41,0`;
- paired no-fixture reload support: `.userdata/dev-harness/harness_runs/20260429_021929/`, scenario `cannibal.live_world_day_smoke_persistence_reload_audit_mcw`, current runtime `e778902cac`, 2/2 green step ledger and saved-state metadata still present after fresh startup; top-level classifier stays yellow/no-new-artifacts, so this is saved-file/startup support paired with the create/save feature proof, not independent new behavior proof;
- different-footing exposed-sight repeat: `.userdata/dev-harness/harness_runs/20260429_022021/`, scenario `cannibal.live_world_exposed_sight_avoid_mcw`, current runtime `e778902cac`, 5/5 green step ledger, feature proof on fixture `cannibal_live_world_exposed_sight_avoid_v0_2026-04-28` from source footing `bandit_local_sight_avoid_exposed_v0_2026-04-27`, showing `profile=cannibal_camp`, `active_job=stalk`, `posture=hold_off`, `pack_size=2`, `recent_exposure=yes`, `sight_exposure=recent`, `shakedown=no`, `combat_forward=no`.

Caveats retained: this is confidence uplift for already-finished cannibal behavior, not behavior redesign; reload support reads saved state rather than proving fresh post-load local-gate behavior; the different-footing repeat reduces but does not eliminate fixture-bias risk.

### Recently completed validation target - Bandit camp-map risk/reward dispatch planning packet v0

`Bandit camp-map risk/reward dispatch planning packet v0` is closed as **scoped live/product checkpoint green**. Frau product review accepted the current matrix as sufficient for this checkpoint and did not require a second-fixture bias variant before closure.

Checkpoint evidence boundary:

- deterministic/code support covers the camp-owned `camp_intelligence_map`, serialization, active target OMT persistence, scout-return writeback, live signal camp-map lead writes, remembered-lead selection through live dispatch cadence/reporting, two-OMT ordinary scout stand-off, 720-minute scout return clock, 2/4/5/7/10 roster/reserve sizing, active-outside dogpile blocking, wounded/unready and killed-member shrinkage, bounded stockpile pressure, high-threat non-escalation, prior defender-loss pressure, prior bandit-loss cooling, larger-than-scout stalk sizing, and no-opening hold/return;
- feature-path proof covers sight-avoid (`20260428_173626`), vanished-signal redispatch (`20260428_185947`), 2/5/10 variable-roster sizing (`20260428_202044`, `20260428_192059`, `20260428_193621`), no-opening stalk-pressure (`20260428_195617`), high-threat/low-reward hold (`20260428_200600`), active-outside dogpile-block (`20260428_200434`), shakedown/toll-control (`20260428_204454`, `20260428_204630`, `20260428_204813`), same-fixture high-threat repeatability (`20260428_211105`, `20260428_211153`), and optional empty-camp retirement sanity (`20260428_214542`).

Recorded gates: `python3 -m py_compile tools/openclaw_harness/startup_harness.py`; relevant scenario/fixture `python3 -m json.tool` checks; `python3 tools/openclaw_harness/proof_classification_unit_test.py`; `git diff --check`; `make -j4 TILES=1 cataclysm-tiles LINTJSON=0 ASTYLE=0`; `./tests/cata_test "[bandit_live_world]"`; `./tests/cata_test "[bandit][live_world][camp_map]" --success`; and the named harness probes. `make astyle-diff` remains locally blocked by missing `astyle`.

Caveats retained: the old Multidrop fuel/writeback route for player-created fire/smoke/light scenario 1 remains non-credit evidence, and the later source-zone `fd_fire` proof does not by itself prove bandit signal response; same-fixture repeatability is not second-fixture anti-bias proof; the no-opening proof is not opening-present escalation proof.

### Recently completed validation target - Cannibal camp night-raid behavior packet v0

The required cannibal live matrix scenarios are checkpointed green: day smoke/stalk pressure, night local-contact pack attack, exposed/recent-sight hold-off, daylight/high-threat negative, saved-state persistence, and day-smoke repeatability. The optional labeled bandit-control contrast is non-blocking unless product review explicitly reopens it.

### Recently completed validation target - C-AOL debug-proof finish stack

`C-AOL debug-proof finish stack v0` is complete and ready for Schani review. Retain the old implemented-but-unproven boundary for Smart Zone Manager live layout; real source-zone player-lit fire has since been proven in the actual-playtest stack, while bandit signal response remains a separate active target.

---

## Pending probes

Active pending probe: `Roof-fire horde detection proof packet v0`.

1. Start from the current actual-playtest stack proof boundary: source-zone real fire and bandit signal response are green, but they are not roof/horde evidence.
2. Shape a roof/elevated fixture or scenario that proves the player is on a roof/elevated position before fire creation.
3. Create the roof fire through a real player action path; debug may stage horde/distance but must not inject the credited fire/light/smoke.
4. Require bounded time passage with before/after turns or exact blocker prompt screenshots/key handling. Do not blind-spam through warnings.
5. Require claim-scoped horde before/after detection or response metadata/artifacts. A proven no-detect/no-response outcome is acceptable if the roof/fire/time footing is green and the horde evidence is decisive.
6. Preserve `.userdata/dev-harness/harness_runs/20260429_162100/` as green player-lit source-zone fire -> bandit signal proof; do not rerun it as ritual.
7. Preserve `.userdata/dev-harness/harness_runs/20260429_040926/` and `.userdata/dev-harness/harness_runs/20260429_041936/` as green pre-staged performance-load rows, not natural three/four-site player-pressure dispatch proof.

Still blocked/later in the actual playtest stack:

- Natural three/four-site player-pressure behavior and true zero-site idle baseline remain decision/watchlist items, not current requirements.

Stale runs `.userdata/dev-harness/harness_runs/20260429_093118/`, `.userdata/dev-harness/harness_runs/20260429_093509/`, `.userdata/dev-harness/harness_runs/20260429_095021/`, `.userdata/dev-harness/harness_runs/20260429_122807/`, and `.userdata/dev-harness/harness_runs/20260429_122955/` are retained as postmortem evidence only, not fire proof surfaces.

If a later live probe is needed:
- build the current runtime first when binary freshness matters;
- use one named scenario/command path;
- extract only decisive report/log fields into context;
- after two same-blocker attempts, consult Frau Knackal before attempt 3; after four unresolved attempts, package implemented-but-unproven state for Josef and move to the next greenlit unblocked target.

---

## Reusable commands

Use these when they are actually the missing evidence, not as ritual.

### Narrow camp deterministic pass after a code slice lands
- `git diff --check`
- `make -j4 obj/basecamp.o tests/faction_camp_test.o tests LINTJSON=0 ASTYLE=0`
- focused `./tests/cata_test` filters for the touched camp/locker/patrol reporting path

### Plan status summary command
- `python3 tools/plan_status_summary.py --self-test`
- `python3 tools/plan_status_summary.py /plan`
- `python3 tools/plan_status_summary.py /plan active`
- `python3 tools/plan_status_summary.py /plan greenlit`
- `python3 tools/plan_status_summary.py /plan parked`

### Fresh tiles rebuild only if a later handoff really needs live proof
- `make -j4 TILES=1 cataclysm-tiles`

## Local build caveat

On this Mac, treat top-level `make -j4 tests` as the reliable path for a fresh `cata_test`.
Avoid treating `make -C tests cata_test` as authoritative here; it has been a repeated source of toolchain/stale-build nonsense.
Also: if you actually need a fresh tiles binary, use `make -j4 TILES=1 cataclysm-tiles`; plain `make cataclysm-tiles` is not an honest rebuild path here.
