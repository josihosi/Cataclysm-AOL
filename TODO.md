# TODO

Short execution queue only.

Remove finished items when they are done.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active lane: `CAOL-JOSEF-PLAYTEST-SAVE-PACK-v0`.

Current execution item: prepare a labelled current-build save/handoff pack for Josef to playtest.

Required entries:
- Basecamp AI / camp locker usefulness.
- Bandit pressure / shakedown / basecamp contact.
- Cannibal camp pressure.
- Flesh raptor skirmisher behavior.
- Zombie rider predator/counterplay.
- Writhing stalker hit-fade / zombie-shadow behavior.

Current checkpoint:
- Josef-facing save-pack card exists at `doc/caol-josef-playtest-save-pack-card-v0-2026-05-02.md`. It inventories all six requested entries and labels ready rows versus no-credit blockers.
- Current ready rows with portal-storm clear: camp locker `locker.weather_wait` -> `.userdata/dev-harness/harness_runs/20260502_131015/`; cannibal local contact `cannibal.live_world_night_local_contact_pack_mcw` -> `.userdata/dev-harness/harness_runs/20260502_131103/`; flesh raptor blocked-corridor skirmisher `flesh_raptor.live_blocked_corridor_skirmisher_mcw` -> `.userdata/dev-harness/harness_runs/20260502_132217/`; zombie rider cover/wounded contrast -> `.userdata/dev-harness/harness_runs/20260502_131914/` and `20260502_132136/`; writhing stalker light/zombie-escape contrast -> `.userdata/dev-harness/harness_runs/20260502_131246/` and `20260502_131315/`.
- Current no-credit blockers: bandit camp-threat/contrast rows remain blocked by camp/NPC assignment audit; `bandit.extortion_at_camp_standoff_mcw` -> `20260502_131536/` is yellow wait proof, and `bandit.high_threat_low_reward_holds` -> `20260502_131616/` has saved site footing but missing hold artifact plus unassigned/loose overmap NPC noise. Stalker pure no-fire/low-threat hit-fade is blocked on current time setup (`20260502_131232/`).
- `CAOL-VISIONS-PLAYTEST-SAMPLER-v0` is folded into this active save-pack lane; use its card/footing as source material, not as the final deliverable.
- Portal-storm warning-light lane is Frau-accepted green v0 from commits `74ef657057` / `8ea5546107`; proof in `doc/harness-portal-storm-warning-light-proof-v0-2026-05-02.md`. Use the warning light while preparing entries; do not rerun portal-storm proof by ritual.

Next execution target:
- Run final docs gate, commit, and push this save-pack card. Future repair work is the camp/NPC assignment + bandit contrast preflight and the stalker no-fire time-setup row, not rerunning the five ready entries by ritual.

Non-goals: no release packaging, no broad gameplay implementation/tuning, no closed-lane reopen by drift, no log archaeology as Josef's primary playtest activity.
