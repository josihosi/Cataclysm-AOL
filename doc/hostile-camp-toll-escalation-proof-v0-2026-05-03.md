# Hostile-camp toll escalation proof v0 — 2026-05-03

Active lane: `CAOL-JOSEF-LIVE-DEBUG-BATCH-v0`, Slice 4.

## Claim boundary

Credited here:
- a scout-confirmed remembered basecamp lead is not trapped in the old lone-scout loop;
- an ordinary bandit camp with seven living/ready members preserves a two-member home reserve and escalates the next 30-minute overmap cadence to a three-member `toll` party;
- the live/staged path writes back the active outside toll group and reports it as shakedown-capable once local contact is established;
- deterministic coverage also keeps the shared sizing/risk gates bounded: small bandit rosters stay at stalk pressure, wounded/dead/active-outside members shrink or block dispatch, high-threat openings stay cautious, and cannibal scout confirmation promotes to attack/raid rather than toll/shakedown.

Still outside this proof:
- natural discovery of the camp or lead without the staged fixture;
- the full local shakedown contact/payment resolution after the toll party reaches the player;
- live cannibal raid contact/combat resolution or broad hostile-camp ecology;
- every-camp suicide dogpile or full faction diplomacy.

## Code checkpoint

Commit under test: `7e5a506c76` (`Promote scout-confirmed camps to toll pressure`).

Follow-up harness checkpoint: `c639b5e87c` (`Harden toll escalation live scenario`) tightened the scenario so feature credit requires green step-local proof, a preflight saved-state scout-confirmed lead, strict same-run cadence artifacts, saved active toll writeback, and `shakedown_capable=yes` rather than the pre-contact `shakedown=yes` surface.

Touched behavior/observability:
- `src/bandit_live_world.cpp` / `src/bandit_live_world.h`
  - scout-confirmed basecamp leads now run through roster-scaled camp-map dispatch sizing;
  - ordinary bandit camps with ample roster can promote from stalk/scout to `toll` while preserving reserve;
  - cannibal camps use the same scout-confirmed sizing gate for attack/raid pressure, but never shakedown;
  - `local_gate_decision::shakedown_capable` separates “this toll party can open shakedown once contact is established” from the immediate `opens_shakedown_surface` boolean.
- `tests/bandit_live_world_test.cpp`
  - adds the scout-confirmed bandit toll escalation, small-roster caution, cannibal attack-pack promotion, active-outside blocker, wounded/dead shrinkage, and risk/cooling coverage.
- `tools/openclaw_harness/scenarios/bandit.hostile_camp_toll_escalation_live.json`
  - proves the staged live cadence with strict artifact and saved-state requirements.

## Deterministic evidence

Commands:

```sh
make -j4 tests LINTJSON=0 ASTYLE=0
./tests/cata_test "[bandit][live_world][camp_map]" --reporter compact
./tests/cata_test "[bandit][live_world]" --reporter compact
./tests/cata_test "[bandit][handoff]" --reporter compact
```

Results:

```text
/tmp/caol_slice4_camp_map_tests_20260503.log: Passed all 15 test cases with 288 assertions.
/tmp/caol_slice4_bandit_live_world_tests_20260503.log: Passed all 66 test cases with 1366 assertions.
/tmp/caol_slice4_handoff_tests_20260503.log: Passed all 3 test cases with 41 assertions.
```

Fresh-runtime guard after stale-binary suspicion:

```text
./cataclysm-tiles --version
Cataclysm Dark Days Ahead: 7e5a506c76
```

Evidence class: deterministic planner/local-gate coverage plus fresh build/runtime-version guard.

## Live/harness artifact evidence

Scenario:

```sh
python3 tools/openclaw_harness/startup_harness.py probe bandit.hostile_camp_toll_escalation_live
```

Credited run: `.userdata/dev-harness/harness_runs/20260503_214648/`

Run summary from `probe.report.json` and `probe.step_ledger.json`:

```text
scenario=bandit.hostile_camp_toll_escalation_live
feature_proof=true
verdict=artifacts_matched
evidence_class=feature-path
proof_classification.status=green
wait_step_status=green_wait_steps_proven
step_ledger_status=green_step_local_proof
step_ledger_summary=7 green / 0 yellow / 0 red_or_blocked
runtime_warnings=[]
abort=null
cleanup.status=terminated
```

Decisive same-run artifact excerpts:

```text
21:47:33.521 INFO : bandit_live_world dispatch plan: site=overmap_special:bandit_camp@140,51,0 target=player@140,39,0 candidate_reason=remembered_camp_map_lead job=toll selected_members=3 signal_packet=none remembered_lead=overmap_special:bandit_camp@140,51,0#lead:basecamp_activity:player@140,39,0@140,39,0 opening_state=opening_present_or_not_required opening_available=yes notes=camp-map decision uses ready roster, wounded/unready, reserve, and lead pressure | camp-map roster living=7 ready_at_home=7 wounded_or_unready=0 active_outside=0 reserve=2 dispatchable=5 | camp-map opening_state=opening_present_or_not_required opening_available=yes | toll: scout-confirmed basecamp lead promotes to a shakedown-capable party | camp-map remembered lead overmap_special:bandit_camp@140,51,0#lead:basecamp_activity:player@140,39,0@140,39,0 selected toll reward=12 risk=1 margin=11 | hostile handoff v0 stays on scout/probe/shadow/withdrawal entry modes | camp-map dispatch ready: toll toward player@140,39,0 members=3 reserve=2 dispatchable=5 | profile camp_style: checks the shared 30-minute cadence, keeps a home reserve, and writes back as persistent camp pressure
21:47:33.521 INFO : local_gate site=overmap_special:bandit_camp@140,51,0 active_group=overmap_special:bandit_camp@140,51,0#dispatch target=player@140,39,0 live_dispatch_goal=140,39,0 active_job=toll profile=camp_style posture=hold_off strength=3 pack_size=3 threat=3 opportunity=2 margin=2 darkness_or_concealment=no standoff_distance=5 basecamp_or_camp=yes current_exposure=no recent_exposure=yes sight_exposure=recent local_contact=no rolling_travel=no shakedown_capable=yes shakedown=no combat_forward=no
- live_candidate reason=remembered_camp_map_lead distance=12 cap=2 signal_packet=none remembered_lead=overmap_special:bandit_camp@140,51,0#lead:basecamp_activity:player@140,39,0@140,39,0 signal_distance=0
```

Saved-state guard from `audit_saved_toll_escalation_party.metadata.json`:

```text
status=required_state_present
observed_matching_site_count=1
site_id=overmap_special:bandit_camp@140,51,0
active_group_id=overmap_special:bandit_camp@140,51,0#dispatch
active_target_id=player@140,39,0
active_target_omt=140,39,0
active_job_type=toll
member_count=7
ready_at_home_count=4
wounded_or_unready_count=0
active_outside_count=3
active_member_ids=4,5,6
remembered_target_or_mark=player@140,39,0
lead_count=1
```

Evidence class: current feature-path/live cadence artifact + saved-state proof. Startup/load and the preflight fixture audit are footing only; the credited behavior is the green 30-minute wait step, same-run dispatch/local-gate artifacts, and saved active toll-party writeback.

## Verdict

The Slice 4 bandit hostile-camp escalation checkpoint is green: after remembered scout confirmation, an ample ordinary bandit camp escalates from lone scout/stalk pressure to a three-member toll-capable party while keeping reserve. This unblocks the ordered debug stack to move to Slice 5 all-light-source live adapter work. Keep the broader cannibal live raid/contact and full local shakedown contact resolution outside this checkpoint unless canon promotes them later.
