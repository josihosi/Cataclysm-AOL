# Cannibal camp confidence-push live playtest matrix v0 - 2026-04-29

Status: ACTIVE / PARTIAL CONFIDENCE UPLIFT

Contract: `doc/cannibal-camp-confidence-push-live-playtest-packet-v0-2026-04-28.md`
Imagination source: `doc/cannibal-camp-confidence-push-live-playtest-imagination-source-of-truth-2026-04-28.md`

## Evidence rules

- Credited feature-path rows require a current runtime/window, green startup gate, all-green step ledger, and claim-scoped artifact/log match.
- Startup/load, stale-window, deterministic, and fixture-setup evidence stay support only.
- This matrix is confidence uplift for closed cannibal behavior, not a redesign lane.

## Matrix

| Shape | Status | Run / scenario | Decisive evidence | Verdict |
| --- | --- | --- | --- | --- |
| Wandering day pressure near cannibal camp | green | `.userdata/dev-harness/harness_runs/20260429_013310/`, scenario `cannibal.live_world_day_smoke_pressure_mcw` | Fresh startup/window captured `Cataclysm: Dark Days Ahead - 782d8edabd`, `captured_dirty=false`, `version_matches_repo_head=true`; probe ledger `green_step_local_proof` 2/2; proof classification `feature-path`, `green`, `feature_proof=true`; artifacts matched live smoke/signal path, `profile=cannibal_camp`, `darkness_or_concealment=no`, `shakedown=no`, `combat_forward=no`, and `signal_packet=live_smoke@`. | Day smoke/pressure reaches real live dispatch/signal path and holds/stalks without bandit shakedown or instant combat-forward behavior. |
| Night mistake / contact | green | `.userdata/dev-harness/harness_runs/20260429_014900/`, scenario `cannibal.live_world_night_local_contact_pack_mcw` | Fresh startup/window captured `Cataclysm: Dark Days Ahead - acfe6fd0ce`, `captured_dirty=false`, `version_matches_repo_head=true`; probe ledger `green_step_local_proof` 2/2; proof classification `feature-path`, `green`, `feature_proof=true`; artifacts matched real local-contact pack-forward path, `profile=cannibal_camp`, `active_job=stalk`, `posture=attack_now`, `pack_size=2`, `darkness_or_concealment=yes`, `local_contact=yes`, `shakedown=no`, `combat_forward=yes`, and `live_candidate reason=direct_player_range`. | Night darkness/contact reaches real live local-gate behavior and pushes a multi-member cannibal pack into attack-now without bandit shakedown. |
| Reload brain | queued | Existing candidates: `cannibal.live_world_day_smoke_persistence_mcw` + `cannibal.live_world_day_smoke_persistence_reload_audit_mcw` | Needs active cannibal group/profile/member/target/job state persisted through save/reload without target/profile lobotomy. | Not yet attempted in this confidence-push pass. |
| Different-seed / different-footing repeat | queued | No packaged second footing selected yet | Needs explicit fixture provenance and same broad behavior class outside the original blessed McWilliams geometry. | Open confidence-bias gap. |
| Bandit contrast control | green | `.userdata/dev-harness/harness_runs/20260429_012915/`, scenario `bandit.extortion_first_demand_fight_mcw` | Fresh startup/window captured `Cataclysm: Dark Days Ahead - 7ca870f6be`, `captured_dirty=false`, `version_matches_repo_head=true`; probe ledger `green_step_local_proof` 6/6; proof classification `feature-path`, `green`, `feature_proof=true`; artifacts matched `shakedown_surface`, `profile=camp_style`, `posture=open_shakedown`, `demanded_toll=15797`, `pay_option=yes fight_option=yes`, and `shakedown_surface fight demanded=15797 reachable=45134`. | Bandit control preserves shakedown/pay/fight separation on current runtime. |

## Current verdict

Partial confidence uplift: the stale-window blocker from the first contrast attempt is cleared; the bandit contrast control is green on runtime `7ca870f6be`; cannibal wandering day pressure is green on runtime `782d8edabd`; and cannibal night/contact pack-forward behavior is green on runtime `acfe6fd0ce`. Reload brain and different-footing rows remain open.
