# CAOL Manual Playtest Scenarios - 2026-05-19

These scenarios are for Josef handoff play, not proof automation. Each command loads a current staged save, performs only a short settle/screenshot plus saved-metadata footing audits, and then leaves the game running. No harness gameplay keys are sent after load.

Run one at a time from the repo root:

```sh
python3 tools/openclaw_harness/startup_harness.py handoff manual.intact_camp_shakedown_mcw --compact-stdout
python3 tools/openclaw_harness/startup_harness.py handoff manual.mixed_hostile_siege_mcw --compact-stdout
python3 tools/openclaw_harness/startup_harness.py handoff manual.writhing_stalker_hit_fade_mcw --compact-stdout
python3 tools/openclaw_harness/startup_harness.py handoff manual.zombie_rider_open_field_mcw --compact-stdout
python3 tools/openclaw_harness/startup_harness.py handoff manual.cannibal_night_pack_mcw --compact-stdout
```

## 1. `manual.intact_camp_shakedown_mcw`

Question: does an intact armed camp make the bandit shakedown and Fight branch feel like a believable camp-defense scene?

Start state: armed camp followers are staged near the player; one hostile raider is already active outside. Advance time or move naturally until the demand path shows up, then try Pay or Fight.

Post-play metadata to inspect: `debug.log` shakedown/local-gate lines, saved NPC survivor/duplicate state, saved `bandit_live_world` active group state.

## 2. `manual.mixed_hostile_siege_mcw`

Question: does stacked hostile pressure create readable layered tension or incoherent noise?

Start state: midnight camp-adjacent scene with bandits, cannibals, writhing stalker, zombie rider, flesh raptor, horde, and fire/smoke cues staged.

Post-play metadata to inspect: active job mix, local gates, hostile live-plan rows, saved active monsters, horde state, and `bandit_live_world`.

## 3. `manual.writhing_stalker_hit_fade_mcw`

Question: does the stalker feel like fair dread with pressure, hit/fade, cooldown, and retreat?

Start state: vulnerable midnight scene with one hostile writhing stalker south of the player.

Post-play metadata to inspect: `writhing_stalker live_plan` rows and saved active monster location/hp.

## 4. `manual.zombie_rider_open_field_mcw`

Question: does the rider create scary ranged pressure with readable cover/counterplay?

Start state: noon open field, one hostile zombie rider at bow range with tainted bone arrows.

Post-play metadata to inspect: `zombie_rider live_plan` / `target_probe` rows and saved rider location/hp/ammo.

## 5. `manual.cannibal_night_pack_mcw`

Question: does night cannibal pressure read as a distinct pack attack instead of bandit shakedown behavior?

Start state: 23:00 local-contact cannibal camp pressure, intentionally one player action away from the dispatch/local-gate path.

Post-play metadata to inspect: `cannibal_camp local_gate` rows, absence of shakedown UI, saved site/active group state, and spawned pack NPC positions if contact happens.

## Notes

- Each handoff overwrites the `dev-harness` `McWilliams` world with that scenario fixture.
- If safe mode interrupts a feel pass, toggle it yourself and continue. The harness will not do it for these manual rows.
- Saving after an interesting outcome makes the post-play metadata much more useful.
