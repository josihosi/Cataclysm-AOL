# C-AOL Josef playtest save pack v0 card (2026-05-02)

Status: CURRENT-BUILD HANDOFF CARD / MIXED GREEN + BLOCKED ROWS

Repo/branch/build: `/Users/josefhorvath/Schanigarten/Cataclysm-AOL` on `dev` at `1113e68275`; `./just_build_macos.sh > /tmp/caol-savepack-build-20260502.log 2>&1` exited `0`.

Run commands from the repo root. Use `probe` for verification and `handoff` when Josef wants a live session left open.

## Use these first

### 1. `/visions/camp-locker` — camp locker weather/service

Handoff command:

```sh
python3 tools/openclaw_harness/startup_harness.py handoff locker.weather_wait
```

What to try for 2-5 minutes:
- Open the camp/base screen and look for whether the locker-stock service feels legible.
- Wait a short chunk and check whether the equipment service reads as useful rather than invisible bookkeeping.
- Look for whether the camp feels like it has a practical supply rhythm.

Expected read: camp locker service should visibly use the locker stock and leave the worker better equipped without magic inventory nonsense.

Taste questions:
- Does this feel like a useful camp service, or like a spreadsheet hiding in a game?
- Is the locker interaction discoverable enough?
- Does the weather/wait cadence make the service feel alive or merely delayed?

Current evidence: `locker.weather_wait` -> `.userdata/dev-harness/harness_runs/20260502_131015/`; `probe.report.json`: `feature_proof=true`, `verdict=artifacts_matched`, step ledger `green_step_local_proof` (`3` green / `0` red), `portal_storm_warning.classification=clear`.

Caveat: staged harness footing, not a natural long-form camp career.

### 2. `/horde/camp/cannibal` — cannibal night local-contact pressure

Handoff command:

```sh
python3 tools/openclaw_harness/startup_harness.py handoff cannibal.live_world_night_local_contact_pack_mcw
```

What to try for 2-5 minutes:
- Let the scene settle, then wait/move just enough to feel the local-contact pressure.
- Watch whether the cannibal camp pressure reads as night threat rather than random monster soup.
- If contact happens, judge the first 30-60 seconds of pressure: readable, scary, fair?

Expected read: current build reaches the cannibal night local-contact footing cleanly and produces the intended pressure packet.

Taste questions:
- Does the pressure arrive with enough context to feel authored?
- Is night doing useful mood work, or just reducing visibility?
- Does the camp pressure feel distinct from ordinary wandering enemies?

Current evidence: `cannibal.live_world_night_local_contact_pack_mcw` -> `.userdata/dev-harness/harness_runs/20260502_131103/`; `feature_proof=true`, `verdict=artifacts_matched`, step ledger `green_step_local_proof` (`2` green / `0` red), portal clear.

Caveat: staged-but-live McWilliams row, not natural map discovery proof.

### 3. `/rider/stalker/raptor` — flesh raptor blocked-corridor skirmisher

Handoff command:

```sh
python3 tools/openclaw_harness/startup_harness.py handoff flesh_raptor.live_blocked_corridor_skirmisher_mcw
```

What to try for 2-5 minutes:
- Use the corridor/blocked-space setup to see whether the raptor feels like a skirmisher rather than a straight-line idiot.
- Step, back up, and change angles; watch whether it pressures around the blocked geometry.
- Do not judge the open-field version from this row; that footing is blocked below.

Expected read: the raptor should feel opportunistic in constrained terrain, with skirmisher pressure visible through movement rather than pure stat-checking.

Taste questions:
- Is the raptor readable as clever, or merely twitchy?
- Does the blocked corridor make it more fun or more annoying?
- Would you want this behavior in a broader open-field encounter once that footing is repaired?

Current evidence: `flesh_raptor.live_blocked_corridor_skirmisher_mcw` -> `.userdata/dev-harness/harness_runs/20260502_132217/`; `feature_proof=true`, `verdict=artifacts_matched`, step ledger `green_step_local_proof` (`8` green / `0` red), portal clear.

Caveat: this is the usable raptor row. Current open-field/crowded/equipment rows are blocked or caveated, so do not use this as proof that every raptor setup is healthy.

### 4. `/rider/stalker/raptor` — zombie rider cover escape / wounded disengagement

Primary handoff command:

```sh
python3 tools/openclaw_harness/startup_harness.py handoff zombie_rider.live_cover_escape_mcw
```

Optional contrast command:

```sh
python3 tools/openclaw_harness/startup_harness.py handoff zombie_rider.live_wounded_disengagement_mcw
```

What to try for 2-5 minutes:
- In cover escape, move through cover and watch whether the rider pressures without feeling omniscient.
- In wounded disengagement, push it enough to see whether retreat/disengagement reads as survival, not cowardly pathing failure.
- Pay attention to whether bow pressure is visibly threatening after the close-pressure fix.

Expected read: the rider should create predator/counterplay pressure, then reposition or disengage under the shaped conditions instead of standing around politely.

Taste questions:
- Does the bow pressure now feel present and fair?
- Is the rider’s repositioning legible, or does it look like random hopping?
- Is wounded disengagement satisfying, or does it drain the encounter?

Current evidence:
- `zombie_rider.live_cover_escape_mcw` -> `.userdata/dev-harness/harness_runs/20260502_131914/`; `feature_proof=true`, `verdict=artifacts_matched`, step ledger `green_step_local_proof` (`9` green / `0` red), portal clear.
- `zombie_rider.live_wounded_disengagement_mcw` -> `.userdata/dev-harness/harness_runs/20260502_132136/`; `feature_proof=true`, `verdict=artifacts_matched`, step ledger `green_step_local_proof` (`6` green / `0` red), portal clear.

Caveat: staged-but-live rows. Open-field pressure rerun at `20260502_131217/` is blocked on time setup in this save-pack pass, so use the two green rows above for playtesting.

### 5. `/rider/stalker/raptor` — writhing stalker light + zombie-side pressure

Primary handoff command:

```sh
python3 tools/openclaw_harness/startup_harness.py handoff writhing_stalker.live_escape_side_zombie_retreat_mcw
```

Light/counterplay contrast command:

```sh
python3 tools/openclaw_harness/startup_harness.py handoff writhing_stalker.live_campfire_counterplay_mcw
```

What to try for 2-5 minutes:
- In escape-side zombie retreat, watch whether the stalker uses pressure and retreat around the zombie side without feeling like a wallhack goblin.
- In campfire counterplay, stand in/near light and judge whether it hesitates/stalks instead of suiciding into the obvious bright danger.
- If it closes, watch for strike/fade rhythm and breathing room rather than attack spam.

Expected read: the stalker should feel like fair dread: pressure, hesitation around light, and retreat/spacing when the situation stops favoring a strike.

Taste questions:
- Is the stalker scary in a way you can read and answer?
- Does fire/light feel like real counterplay, or just a debug flag with graphics?
- Does the zombie-side pressure feel emergent or contrived?

Current evidence:
- `writhing_stalker.live_escape_side_zombie_retreat_mcw` -> `.userdata/dev-harness/harness_runs/20260502_131315/`; `feature_proof=true`, `verdict=artifacts_matched`, step ledger `green_step_local_proof` (`7` green / `0` red), portal clear.
- `writhing_stalker.live_campfire_counterplay_mcw` -> `.userdata/dev-harness/harness_runs/20260502_131246/`; `feature_proof=true`, `verdict=artifacts_matched`, step ledger `green_step_local_proof` (`7` green / `0` red), portal clear.

Caveat: the pure no-fire/low-threat hit-fade row is blocked in the current save-pack pass (`writhing_stalker.live_hit_fade_retreat_mcw` -> `.userdata/dev-harness/harness_runs/20260502_131232/`, `blocked_writhing_stalker_time_setup_missing`). So the card has light/zombie contrast ready, but not a clean current no-fire close-strike-fade handoff.

## Blocked / no-credit rows Josef should not judge as final product

### 6. `/horde/camp/bandit` — bandit pressure / shakedown / basecamp contact

Do **not** use current camp-threat rows as credited pressure evidence yet. This is the one with the loud little warning bell, because otherwise loose NPCs dress up as a camp and everyone pretends that is science. Na bravo.

Caveated handoff command, only if Josef wants to inspect the current broken footing:

```sh
python3 tools/openclaw_harness/startup_harness.py handoff bandit.extortion_at_camp_standoff_mcw
```

Current evidence:
- `bandit.extortion_at_camp_standoff_mcw` -> `.userdata/dev-harness/harness_runs/20260502_131536/`; `feature_proof=false`, `verdict=yellow_wait_step_unverified`, portal clear. The wait path reached a caveated standoff-shaped row but lacks green wait proof.
- `bandit.high_threat_low_reward_holds` -> `.userdata/dev-harness/harness_runs/20260502_131616/`; saved bandit-live-world preflight rows are `required_state_present`, but final artifact proof is missing: `verdict=blocked_high_threat_hold_artifact_missing`, portal clear.
- Camp/NPC audit on saved world `.userdata/dev-harness/harness_runs/20260502_131616/saved_world/McWilliams`: bandit live-world site `overmap_special:bandit_camp@140,51,0` has `member_count=5`, `ready_at_home_count=5`, `active_outside_count=0`, and `active_member_ids=[]`; full saved-overmap NPC scan sees `observed_npc_count=16`, including two `your_followers` and multiple `hells_raiders` NPC records with no active-member cross-reference. That is not clean camp-assigned threat proof.

Blocked follow-up required before credit:
- Low-threat/no-signal row: remove/kill/despawn/neutralize loose extra NPCs, then audit the saved overmap count and absence of unassigned pressure actors before the no-signal row is credited.
- High-threat/resistant row: spawn or repair active pressure as camp-assigned bandit-live-world members, then require `active_member_ids` plus `active_members_all_found_in_saved_overmap=true` before threat pressure is credited.
- Fire/smoke/basecamp signal row: rerun with the same assignment audit rather than borrowing older green smoke proof.

## Thematic contrast status

- Stalker fire/light: ready (`writhing_stalker.live_campfire_counterplay_mcw`, `20260502_131246/`).
- Stalker zombie/escape-side pressure: ready (`writhing_stalker.live_escape_side_zombie_retreat_mcw`, `20260502_131315/`).
- Stalker no-fire/low-threat close-strike-fade: blocked on current time setup (`20260502_131232/`).
- Bandit no-signal / signal / high-threat: blocked or caveated pending camp/NPC assignment repair; do not flatten loose NPCs into camp pressure.

## Validation gates run

- `git fetch origin dev`; local `dev` and `origin/dev` both at `1113e68275`.
- `./just_build_macos.sh > /tmp/caol-savepack-build-20260502.log 2>&1` -> exit `0`.
- Current-build probe summaries:
  - `/tmp/caol-savepack-assign-probe-summary-20260502_1306_assign.txt`
  - `/tmp/caol-savepack-bandit-assign-summary-20260502_1320_bandit_assign.txt`
  - `/tmp/caol-savepack-alt-probe-summary-20260502_1324_alt.txt`
- Final docs gate: `git diff --check` before commit.
