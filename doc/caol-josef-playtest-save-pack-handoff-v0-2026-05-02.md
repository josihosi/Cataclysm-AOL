# CAOL-JOSEF-PLAYTEST-SAVE-PACK-v0 handoff (2026-05-02)

Status: ACTIVE / GREENLIT / JOSEF PLAYTEST SAVE-PACK PREP

Contract: `doc/caol-josef-playtest-save-pack-packet-v0-2026-05-02.md`.

Imagination source: `doc/caol-josef-playtest-save-pack-imagination-source-2026-05-02.md`.

## Task

Prepare a labelled set of current-build saves or handoff sessions Josef can actually load and playtest.

Required entries preserve the existing playtest family labels `/visions/camp-locker`, `/rider/stalker/raptor`, and `/horde/camp`.

Required entries:

1. Basecamp AI / camp locker usefulness.
2. Bandit pressure / shakedown / basecamp contact.
3. Cannibal camp pressure.
4. Flesh raptor skirmisher behavior.
5. Zombie rider predator/counterplay.
6. Writhing stalker hit-fade / zombie-shadow behavior.

## Added thematic contrast pass

Before the card is final, try paired thematic playtests for stalkers and bandits:

- Stalker no-fire / low-threat: should close, strike, then fade out.
- Stalker with fire/light: should keep stalking or hesitate rather than committing like an idiot into light.
- Stalker high-threat player/support: should stalk/pressure rather than rush suicidally.
- Bandit no obvious fire/smoke/camp signal: should not instantly produce theatrical pressure.
- Bandit fire/smoke/basecamp signal: should produce visible scout, shakedown, hold-off, or camp pressure quickly enough to judge.
- Bandit high-threat/resistant setup: should stalk, hold off, back out, or escalate differently rather than replay the same demand branch.

Deliver these as paired save entries, paired handoff reports, or explicit caveated/blocked rows. Do not flatten them into one generic “feature works” claim.

## Output

Produce a Josef-facing save-pack card under `doc/` with, for each entry:

- label;
- save path, handoff command, or exact load path;
- 2-5 minute “what to try” instructions;
- expected visible/mechanical read;
- 2-4 taste questions;
- proof footing or artifact dir;
- current-build load/start-state result;
- caveats, including staged-vs-natural and portal-storm warning status.

## Starting footing

Use existing green/caveated rows instead of rebuilding from folklore:

- Camp locker / basecamp: `locker.zone_manager_save_probe_mcw`, `locker.weather_wait`, basecamp follower/board probes, `doc/camp-locker-zone-playtest-proof-v0-2026-05-02.md`.
- Bandit: scenic shakedown, `bandit.extortion_at_camp_standoff_mcw`, `bandit.extortion_first_demand_fight_mcw`, `bandit.extortion_first_demand_pay_mcw`, `bandit.extortion_reopened_demand_mcw`, prepared-base contact rows.
- Cannibal: `cannibal.live_world_night_local_contact_pack_mcw`, `cannibal.live_world_day_smoke_pressure_mcw`, and related cannibal camp rows.
- Flesh raptor: `flesh_raptor.live_open_field_skirmisher_mcw`, `flesh_raptor.live_crowded_arc_skirmisher_mcw`, `flesh_raptor.live_equipment_frustration_comparison_mcw`.
- Zombie rider: `zombie_rider.live_open_field_pressure_mcw`, `zombie_rider.live_cover_escape_mcw`, `zombie_rider.live_camp_light_band_mcw`, close-pressure run `.userdata/dev-harness/harness_runs/20260502_050055/`.
- Writhing stalker: `writhing_stalker.live_hit_fade_retreat_mcw` -> `.userdata/dev-harness/harness_runs/20260502_113738/`; avoid old dirty quiet-side optical footing unless rerun clean.

## Validation bar

- `git diff --check` for docs/harness edits.
- Any code/harness change gets the narrow relevant compile/test gate.
- Each ready entry needs current-build load/start-state evidence, step ledger/report inspection, and portal-storm warning classification.
- Do not call an entry ready if it only has old proof but no usable current handoff/save path.
- If one entry blocks, mark it blocked with the missing primitive and finish the rest of the pack.

## Non-goals

- No release work.
- No broad gameplay implementation or tuning unless needed to make a save actually load/play.
- No reopening closed lanes just because they appear in the save pack.
- No hidden log archaeology for Josef.

## First move

Inventory the six best candidate scenarios/save footings and decide which need fresh `handoff` versus which can be delivered as existing loadable save/card entries. Then build the smallest usable pack.