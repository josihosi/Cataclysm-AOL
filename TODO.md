# TODO

Short execution queue only.

Remove finished items when they are done.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active lane: `CAOL-JOSEF-PLAYTEST-SAVE-PACK-v0`.

Current execution item: v0 save-pack prep is at a handoff boundary; the six-entry Josef card is ready, and the optional second bandit contrast card now has staged green footing if Schani wants to include it.

Required entries:
- Basecamp AI / camp locker usefulness.
- Bandit pressure / shakedown / basecamp contact.
- Cannibal camp pressure.
- Flesh raptor skirmisher behavior.
- Zombie rider predator/counterplay.
- Writhing stalker hit-fade / zombie-shadow behavior.

Current checkpoint:
- Josef-facing save-pack card exists at `doc/caol-josef-playtest-save-pack-card-v0-2026-05-02.md`. It ships a six-entry Josef-ready card and labels remaining caveated rows honestly instead of treating them as hidden blockers.
- Current ready rows with portal-storm clear: camp locker `locker.weather_wait` -> `.userdata/dev-harness/harness_runs/20260502_131015/`; bandit first-demand contact `bandit.extortion_first_demand_fight_mcw` -> `.userdata/dev-harness/harness_runs/20260502_124854/`; cannibal local contact `cannibal.live_world_night_local_contact_pack_mcw` -> `.userdata/dev-harness/harness_runs/20260502_131103/`; flesh raptor crowded-arc skirmisher -> `.userdata/dev-harness/harness_runs/20260502_141246/`; zombie rider cover/wounded contrast -> `.userdata/dev-harness/harness_runs/20260502_131914/` and `20260502_132136/`; writhing stalker hit-fade/light/zombie contrast -> `.userdata/dev-harness/harness_runs/20260502_125231/`, `20260502_131246/`, and `20260502_131315/`.
- Current omitted/caveated rows: bandit low-threat/no-signal control now has cleaned auxiliary proof `bandit.live_world_nearby_camp_no_signal_control_mcw` -> `.userdata/dev-harness/harness_runs/20260502_134959/` with zero saved overmap NPCs preflight, green wait ledger, matched no-signal cadence artifacts, and portal clear, but it is not camp-threat membership proof and was not added to the first Josef card as a pressure entry. Fire/smoke signal is repaired through the materially different guarded `bandit.mixed_signal_coexistence_mcw` path -> `.userdata/dev-harness/harness_runs/20260502_155058/`, with `feature_proof=true`, green wait/step ledgers, portal clear, live-signal scout dispatch from camp `@151,39,0`, separate structural scavenge from camp `@160,39,0`, and manual saved-overmap active-member cross-references for members `4` and `9`; direct `bandit.player_lit_fire_signal_wait_mcw` attempt `20260502_154828/` remains blocked at startup UI and is not credited. Active-outside camp-pressure assignment is repaired as auxiliary proof: `bandit.active_outside_dogpile_block_live` -> `.userdata/dev-harness/harness_runs/20260502_144842/` has `feature_proof=true`, green wait/step ledgers, portal clear, `active_member_ids=[4,5]`, and `active_members_all_found_in_saved_overmap=true`. Pure high-threat hold is now repaired as auxiliary proof: `bandit.high_threat_low_reward_holds` -> `.userdata/dev-harness/harness_runs/20260502_145429/` has `feature_proof=true`, green wait/step ledgers, portal clear, and the same-run risk/reward hold artifact; old `20260502_131616/` remains the missing-artifact failure receipt. `bandit.extortion_at_camp_standoff_mcw` -> `20260502_131536/` remains yellow wait proof. Stalker later no-fire/low-threat rerun is blocked on current time setup (`20260502_131232/`), but the earlier current green hit-fade row `20260502_125231/` is included in the card.
- `CAOL-VISIONS-PLAYTEST-SAMPLER-v0` is folded into this active save-pack lane; use its card/footing as source material, not as the final deliverable.
- Portal-storm warning-light lane is Frau-accepted green v0 from commits `74ef657057` / `8ea5546107`; proof in `doc/harness-portal-storm-warning-light-proof-v0-2026-05-02.md`. Use the warning light while preparing entries; do not rerun portal-storm proof by ritual.

Next execution target:
- No further unblocked Andi proof target remains for the first v0 save-pack card or the optional staged bandit contrast card. Schani can package/relay the six-entry card, and may include the auxiliary bandit contrast if useful; do not rerun the six ready save-pack entries or old blocked bandit rows by ritual.

Non-goals: no release packaging, no broad gameplay implementation/tuning, no closed-lane reopen by drift, no log archaeology as Josef's primary playtest activity.
