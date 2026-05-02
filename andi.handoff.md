# Andi handoff

Active lane: `CAOL-JOSEF-PLAYTEST-SAVE-PACK-v0`.

Contract: `doc/caol-josef-playtest-save-pack-packet-v0-2026-05-02.md`.

Imagination source: `doc/caol-josef-playtest-save-pack-imagination-source-2026-05-02.md`.

Handoff packet: `doc/caol-josef-playtest-save-pack-handoff-v0-2026-05-02.md`.

Working Josef card: `doc/caol-josef-playtest-save-pack-card-v0-2026-05-02.md`.

Current repo canon now says this lane is at a handoff/review boundary, not at first-prep start:
- `Plan.md` active lane: `ACTIVE / GREENLIT / JOSEF HANDOFF CARD READY / OPTIONAL BANDIT CONTRAST READY`.
- `TODO.md` current execution item: the six-entry Josef card is ready, and the optional second bandit contrast card now has staged green footing if Schani wants to include it.
- `TESTING.md` active validation target: the save-pack handoff card is ready, with optional staged bandit contrast footing if Schani wants to include it.

Ready first-card entries:
1. Basecamp AI / camp locker usefulness — `locker.weather_wait` -> `.userdata/dev-harness/harness_runs/20260502_131015/`.
2. Bandit pressure / shakedown / basecamp contact — `bandit.extortion_first_demand_fight_mcw` -> `.userdata/dev-harness/harness_runs/20260502_124854/`.
3. Cannibal camp pressure — `cannibal.live_world_night_local_contact_pack_mcw` -> `.userdata/dev-harness/harness_runs/20260502_131103/`.
4. Flesh raptor skirmisher behavior — `flesh_raptor.live_crowded_arc_skirmisher_mcw` -> `.userdata/dev-harness/harness_runs/20260502_141246/`.
5. Zombie rider predator/counterplay — `zombie_rider.live_cover_escape_mcw` -> `.userdata/dev-harness/harness_runs/20260502_131914/` and `zombie_rider.live_wounded_disengagement_mcw` -> `.userdata/dev-harness/harness_runs/20260502_132136/`.
6. Writhing stalker hit-fade / zombie-shadow behavior — `writhing_stalker.live_hit_fade_retreat_mcw` -> `.userdata/dev-harness/harness_runs/20260502_125231/`, `writhing_stalker.live_campfire_counterplay_mcw` -> `.userdata/dev-harness/harness_runs/20260502_131246/`, and `writhing_stalker.live_escape_side_zombie_retreat_mcw` -> `.userdata/dev-harness/harness_runs/20260502_131315/`.

Optional second bandit contrast footing, if Schani wants to include it:
- Low-threat/no-signal control: `bandit.live_world_nearby_camp_no_signal_control_mcw` -> `.userdata/dev-harness/harness_runs/20260502_134959/`.
- Smoke/fire signal through guarded mixed-signal path: `bandit.mixed_signal_coexistence_mcw` -> `.userdata/dev-harness/harness_runs/20260502_155058/`.
- Active-outside camp-pressure/dogpile block: `bandit.active_outside_dogpile_block_live` -> `.userdata/dev-harness/harness_runs/20260502_144842/`.
- High-threat/low-reward hold: `bandit.high_threat_low_reward_holds` -> `.userdata/dev-harness/harness_runs/20260502_145429/`.

Next honest move:
- Schani decides whether to relay the six-entry Josef card as-is or include the optional staged bandit contrast card.
- Do **not** restart inventory/prep of the six ready entries by ritual.
- Do **not** rerun old blocked bandit rows by ritual; use only a materially changed path if Schani explicitly asks for more bandit contrast.

Boundary:
- Save-pack prep and product-taste handoff only.
- Do not implement new gameplay unless a hard blocker prevents a save from loading or being playable.
- Do not reopen closed v0 lanes by drift.
- Do not create release packaging.
- Do not make Josef inspect logs as the primary playtest activity.
