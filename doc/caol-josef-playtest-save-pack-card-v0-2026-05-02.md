# C-AOL Josef playtest save pack v0 card (2026-05-02)

Status: CURRENT-BUILD JOSEF HANDOFF CARD / READY ENTRIES + LABELLED CAVEATS

Repo/branch/build: `/Users/josefhorvath/Schanigarten/Cataclysm-AOL` on `dev`; gameplay build footing from `1113e68275` passed `./just_build_macos.sh > /tmp/caol-savepack-build-20260502.log 2>&1` with exit `0`. Later save-pack changes are harness-scenario/card classification only.

Run commands from the repo root. Use `handoff` when Josef wants a live session left open; use `probe` only to re-check the same setup. Do **not** rerun blocked rows by ritual.

## Use these first

These six entries are the Josef-ready card. They are current-build, portal-clear, and playable enough for taste reads. Everything after this section is intentionally caveated, not a hidden blocker. Na bravo, a card with borders.

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

### 2. `/horde/camp/bandit` — fast shakedown/contact

Handoff command:

```sh
python3 tools/openclaw_harness/startup_harness.py handoff bandit.extortion_first_demand_fight_mcw
```

What to try for 2-5 minutes:
- Let the first demand surface, then read the shakedown/contact beat before choosing a response.
- Try the fight branch and judge whether the contact feels readable, fast, and pressured rather than like a mystery menu.
- Judge only the first-demand/contact surface here, not the whole camp-threat ecology.

Expected read: the bandit first-demand contact should present a clear shakedown fork with immediate pressure and understandable player choices.

Taste questions:
- Does the demand arrive with enough context to feel authored?
- Are `pay` / `fight` / refusal-style choices legible and tense?
- Does the fast contact feel like a bandit shakedown, not a random NPC bump?

Current evidence: `bandit.extortion_first_demand_fight_mcw` -> `.userdata/dev-harness/harness_runs/20260502_124854/`; `probe.report.json`: `feature_proof=true`, `verdict=artifacts_matched`, step ledger `green_step_local_proof`, portal clear.

Caveat: this is first-demand shakedown/contact proof. It does **not** fold the later auxiliary no-signal / active-outside / high-threat contrast rows into the first six-entry Josef card; smoke-fire signal still remains caveated below.

### 3. `/horde/camp/cannibal` — cannibal night local-contact pressure

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

### 4. `/rider/stalker/raptor` — flesh raptor crowded-arc skirmisher

Handoff command:

```sh
python3 tools/openclaw_harness/startup_harness.py handoff flesh_raptor.live_crowded_arc_skirmisher_mcw
```

What to try for 2-5 minutes:
- Watch the raptor as zombies crowd one side and judge whether it circles/flanks instead of stacking into the blob.
- Let it commit to a swoop and decide whether the cadence feels readable or spammy.
- Pay attention to whether hit/miss/armor-absorb outcomes feel like annoying equipment nibbling or lively skirmisher pressure.

Expected read: the raptor should choose an uncrowded five-tile orbit arc, periodically swoop, and feel like a visible skirmisher rather than generic hit-and-run retreat.

Taste questions:
- Does the raptor's circling read as intentional from play?
- Is the swoop cadence tense without becoming noise?
- Does the zombie-crowded-side contrast make the behavior feel smarter, or just staged?

Current evidence: `flesh_raptor.live_crowded_arc_skirmisher_mcw` -> `.userdata/dev-harness/harness_runs/20260502_141246/`; `feature_proof=true`, `verdict=artifacts_matched`, step ledger `green_step_local_proof` (`6` green / `0` red), `portal_storm_warning.classification=clear`. The repaired scenario contract accepts any uncrowded five-tile open arc rather than the stale exact `chosen_rel=0,5` tile; the run logged `chosen_rel=-1,5`, `chosen_distance=5`, `chosen_crowding=0`, cadence swoops, and melee events.

Caveat: staged-but-live crowded-arc row, not natural random discovery. Older open-field/equipment comparison rows remain footing only; this current entry is the playable save-pack raptor card.

### 5. `/rider/stalker/raptor` — zombie rider cover escape / wounded disengagement

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

### 6. `/rider/stalker/raptor` — writhing stalker hit-fade / light / zombie-side pressure

Primary handoff command:

```sh
python3 tools/openclaw_harness/startup_harness.py handoff writhing_stalker.live_hit_fade_retreat_mcw
```

Light/counterplay contrast command:

```sh
python3 tools/openclaw_harness/startup_harness.py handoff writhing_stalker.live_campfire_counterplay_mcw
```

Zombie/escape-side contrast command:

```sh
python3 tools/openclaw_harness/startup_harness.py handoff writhing_stalker.live_escape_side_zombie_retreat_mcw
```

What to try for 2-5 minutes:
- In hit-fade, watch for strike, withdrawal, cooldown, and breathing room rather than attack spam.
- In campfire counterplay, stand in/near light and judge whether it hesitates/stalks instead of suiciding into the obvious bright danger.
- In escape-side zombie pressure, watch whether the stalker uses pressure and retreat around the zombie side without feeling like a wallhack goblin.

Expected read: the stalker should feel like fair dread: pressure, hesitation around light, and retreat/spacing when the situation stops favoring a strike.

Taste questions:
- Is the stalker scary in a way you can read and answer?
- Does fire/light feel like real counterplay, or just a debug flag with graphics?
- Does the zombie-side pressure feel emergent or contrived?

Current evidence:
- `writhing_stalker.live_hit_fade_retreat_mcw` -> `.userdata/dev-harness/harness_runs/20260502_125231/`; `feature_proof=true`, `verdict=artifacts_matched`, step ledger `green_step_local_proof`, portal clear.
- `writhing_stalker.live_campfire_counterplay_mcw` -> `.userdata/dev-harness/harness_runs/20260502_131246/`; `feature_proof=true`, `verdict=artifacts_matched`, step ledger `green_step_local_proof` (`7` green / `0` red), portal clear.
- `writhing_stalker.live_escape_side_zombie_retreat_mcw` -> `.userdata/dev-harness/harness_runs/20260502_131315/`; `feature_proof=true`, `verdict=artifacts_matched`, step ledger `green_step_local_proof` (`7` green / `0` red), portal clear.

Caveat: the later no-fire/low-threat rerun at `20260502_131232/` is blocked on time setup. Use the green `20260502_125231/` hit-fade row plus light/zombie contrasts above; do not stir the same blocked rerun again.

## Omitted / caveated rows Josef should not judge as final product

### Flesh raptor superseded caveat

The earlier caveat is superseded for the first Josef card by the current green crowded-arc row `flesh_raptor.live_crowded_arc_skirmisher_mcw` -> `.userdata/dev-harness/harness_runs/20260502_141246/`. Older open-field/crowded/equipment rows remain footing only, and the old no-credit current attempts remain useful failure receipts, but Josef now has one current raptor skirmisher entry above.

### Bandit contrast matrix / camp-threat proof

The primary six-row Josef card still uses the fast first-demand contact above. Separately, the optional bandit contrast card is now honest enough as staged current-build footing: quiet/no-signal stays quiet, smoke/fire signal produces a camp scout while a separate structural camp can dispatch, and high-threat/low-reward holds instead of replaying the same demand branch. Tiny miracle, after only the usual ceremony with the garden rake.

Optional contrast handoff commands:

```sh
python3 tools/openclaw_harness/startup_harness.py handoff bandit.live_world_nearby_camp_no_signal_control_mcw
python3 tools/openclaw_harness/startup_harness.py handoff bandit.mixed_signal_coexistence_mcw
python3 tools/openclaw_harness/startup_harness.py handoff bandit.high_threat_low_reward_holds
```

Current evidence:
- Low-threat/no-loose-NPC no-signal auxiliary proof: `bandit.live_world_nearby_camp_no_signal_control_mcw` -> `.userdata/dev-harness/harness_runs/20260502_134959/`; `feature_proof=true`, `verdict=artifacts_matched`, saved-overmap NPC preflight `observed_npc_count=0`, green wait ledger, matched no-signal cadence artifacts, portal clear. This repairs the quiet no-signal control and keeps loose NPCs out of the proof.
- Fire/smoke signal + separate camp activity proof: `bandit.mixed_signal_coexistence_mcw` -> `.userdata/dev-harness/harness_runs/20260502_155058/`; `feature_proof=true`, `verdict=artifacts_matched`, green wait/step ledgers, portal clear. The live signal camp `overmap_special:bandit_camp@151,39,0` dispatches a `scout` toward `player@140,39,0` from `live_smoke@140,39,0`; the structural camp `overmap_special:bandit_camp@160,39,0` separately dispatches `scavenge` against `fixture_mixed_signal_structural_east_forest`. Manual post-save cross-reference artifacts in the run dir require `active_members_all_found_in_saved_overmap=true` for active member `4` on the signal scout and active member `9` on the structural outing.
- Active-outside camp-pressure/dogpile-block auxiliary proof: `bandit.active_outside_dogpile_block_live` -> `.userdata/dev-harness/harness_runs/20260502_144842/`; `feature_proof=true`, `verdict=artifacts_matched`, green wait/step ledgers, same-run `hold: unresolved active outside group/contact blocks dogpile`, portal clear, `active_member_ids=[4,5]`, and `active_members_all_found_in_saved_overmap=true`. This repairs camp-pressure assignment footing; it is not automatically added to the first six-row Josef card.
- High-threat/low-reward hold auxiliary proof: `bandit.high_threat_low_reward_holds` -> `.userdata/dev-harness/harness_runs/20260502_145429/`; `feature_proof=true`, `verdict=artifacts_matched`, green wait/step ledgers, same-run `hold: high threat or poor reward does not escalate by itself`, matching `camp-map dispatch ... selected=hold / chill`, and portal clear. This repairs the old missing-artifact caveat; it is still a staged contrast proof, not a natural-discovery save-pack row.
- Superseded no-signal attempt: `.userdata/dev-harness/harness_runs/20260502_125743/`; `feature_proof=false`, `verdict=yellow_step_local_proof_incomplete`, portal clear.
- `bandit.extortion_at_camp_standoff_mcw` -> `.userdata/dev-harness/harness_runs/20260502_131536/`; `feature_proof=false`, `verdict=yellow_wait_step_unverified`, portal clear. The wait path reached a caveated standoff-shaped row but lacks green wait proof.
- Superseded high-threat attempt: `bandit.high_threat_low_reward_holds` -> `.userdata/dev-harness/harness_runs/20260502_131616/`; saved bandit-live-world preflight rows are `required_state_present`, but final artifact proof is missing: `verdict=blocked_high_threat_hold_artifact_missing`, portal clear.
- Superseded direct player-lit fire attempt: `bandit.player_lit_fire_signal_wait_mcw` -> `.userdata/dev-harness/harness_runs/20260502_154828/`; blocked before credit because startup did not land on normal map UI before the bounded signal wait. The credited high-signal proof is the materially different guarded `mixed_signal_coexistence` path above.

Remaining caveat: this is staged current-build contrast evidence, not natural-discovery/full-raid proof and not the older broken `extortion_at_camp_standoff` handoff. Use it to judge contrast feel, not to declare the whole bandit/Basecamp product finished, because apparently we are still pretending precision is optional until the logs bite us.

### Camp/NPC assignment preflight

No camp-threat credit from unassigned current-save NPCs.

Current evidence:
- `basecamp.package2_assign_camp_state_probe_mcw` -> `.userdata/dev-harness/harness_runs/20260502_131907/`; `feature_proof=false`, `verdict=blocked_step_local_proof`, portal clear.
- Camp/NPC audit on saved world `.userdata/dev-harness/harness_runs/20260502_131616/saved_world/McWilliams`: bandit live-world site `overmap_special:bandit_camp@140,51,0` has `member_count=5`, `ready_at_home_count=5`, `active_outside_count=0`, and `active_member_ids=[]`; full saved-overmap NPC scan sees `observed_npc_count=16`, including two `your_followers` and multiple `hells_raiders` NPC records with no active-member cross-reference. This stays no-credit for camp pressure because there are no active members to cross-reference.
- Repaired assignment proof `.userdata/dev-harness/harness_runs/20260502_144842/`: `bandit.active_outside_dogpile_block_live` preflight and post-save audits require `active_member_ids=[4,5]`, `active_members_all_found_in_saved_overmap=true`, and saved-overmap rows for Giuseppe Bachman and Vance Gunderson in `overmaps/o.0.0.zzip`.
- Repaired mixed-signal assignment proof `.userdata/dev-harness/harness_runs/20260502_155058/`: live-signal camp `@151,39,0` and structural camp `@160,39,0` both preflight as real `overmap_special:bandit_camp` sites with `member_count=5`/`ready_at_home_count=5`; after the wait, manual cross-reference artifacts require active member `4` and active member `9` to exist in the saved overmap. The full saved-overmap scan also sees two `your_followers`; they are explicitly not credited as bandit pressure actors.

Remaining follow-up before broader bandit-contrast credit:
- No-signal, smoke/fire signal, active-outside dogpile block, and high-threat risk/reward hold are now green auxiliary contrast proof (`20260502_134959/`, `20260502_155058/`, `20260502_144842/`, `20260502_145429/`); keep staged-vs-natural caveats explicit.
- Natural-discovery/full-raid bandit/Basecamp proof remains future-only unless Josef explicitly promotes it. Do not rerun old blocked rows by ritual.

## Thematic contrast status

- Camp locker: ready (`locker.weather_wait`, `20260502_131015/`).
- Bandit first-demand/contact: ready (`bandit.extortion_first_demand_fight_mcw`, `20260502_124854/`).
- Cannibal night contact: ready (`cannibal.live_world_night_local_contact_pack_mcw`, `20260502_131103/`).
- Zombie rider cover/wounded contrast: ready (`20260502_131914/`, `20260502_132136/`).
- Writhing stalker hit-fade/light/zombie contrast: ready (`20260502_125231/`, `20260502_131246/`, `20260502_131315/`).
- Flesh raptor crowded-arc skirmisher: ready (`20260502_141246/`); older open-field/equipment rows remain footing only.
- Bandit no-signal / smoke-fire signal / active-outside camp-pressure / high-threat hold: auxiliary green (`20260502_134959/`, `20260502_155058/`, `20260502_144842/`, `20260502_145429/`); staged contrast is ready if Schani wants a second bandit card, while natural-discovery/full-raid proof remains future-only.

## Validation gates run

- `git fetch origin dev`; local `dev` and `origin/dev` were synced before the save-pack documentation pass.
- `./just_build_macos.sh > /tmp/caol-savepack-build-20260502.log 2>&1` -> exit `0` on the gameplay build footing used for these probes.
- Current-build probe summaries:
  - `/tmp/caol-savepack-assign-probe-summary-20260502_1306_assign.txt`
  - `/tmp/caol-savepack-bandit-assign-summary-20260502_1320_bandit_assign.txt`
  - `/tmp/caol-savepack-alt-probe-summary-20260502_1324_alt.txt`
- Flesh raptor repair/probe: `python3 tools/openclaw_harness/startup_harness.py probe flesh_raptor.live_crowded_arc_skirmisher_mcw` -> `.userdata/dev-harness/harness_runs/20260502_141246/`, `feature_proof=true`, `verdict=artifacts_matched`, green step ledger, portal clear.
- Bandit mixed-signal contrast repair: `python3 tools/openclaw_harness/startup_harness.py probe bandit.mixed_signal_coexistence_mcw` -> `.userdata/dev-harness/harness_runs/20260502_155058/`, `feature_proof=true`, `verdict=artifacts_matched`, green wait/step ledgers, portal clear; manual saved-world cross-reference artifacts in the same run require active members `4` and `9` found in saved overmap records.
- Final docs gate for this trim: `git diff --check` before commit.
