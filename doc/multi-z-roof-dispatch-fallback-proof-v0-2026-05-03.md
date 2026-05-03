# Multi-z roof/tower dispatch fallback proof v0 — 2026-05-03

Active lane: `CAOL-JOSEF-LIVE-DEBUG-BATCH-v0`, Slice 3.

## Claim boundary

Credited here:
- multi-z bandit-camp site identity/z-footprint remains one site across ground/upper/tower z-levels;
- remembered elevated/tower-z leads still match when the current dispatch target is routed through reachable ground;
- dispatch preserves the elevated target identity (`player@140,39,5`) while routing the live dispatch goal to reachable ground (`140,39,0`);
- the live 30-minute cadence produces an active stalk dispatch instead of `route_missing` rejection or the old empty-site/empty-travel `30_minute_throttle` silence.

Still outside this proof:
- local/reality-bubble stair, roof, or tower pathfinding after the overmap dispatch reaches the ground approach;
- broad vertical assault AI or tile-perfect roof entry behavior;
- unrelated light-source signaling, hostile-camp escalation, patrol, stalker, sorting, debug-spawn, locker, or Monsterbone spear slices.

## Code checkpoint

Commit under test: `c06590e143` (`Route roof-z dispatch through ground fallback`).

Touched behavior/observability:
- `src/bandit_live_world.cpp`
  - `find_camp_map_dispatch_lead_for_target(...)` now compares routed lead OMT and routed target OMT, so a remembered roof/tower-z lead can match the reachable ground dispatch target instead of being stranded by a z mismatch.
  - `render_local_gate_report(...)` now prints `live_dispatch_goal=x,y,z`, making the ground fallback visible in live harness artifacts.
- `tests/bandit_live_world_test.cpp`
  - the vertical dispatch test now checks both direct elevated lookup and ground-routed lookup against the same elevated remembered lead.

## Deterministic evidence

Commands:

```sh
git diff --check && make -j4 obj/bandit_live_world.o tests/bandit_live_world_test.o tests LINTJSON=0 ASTYLE=0 && ./tests/cata_test "bandit_live_world_vertical_targets_route_via_reachable_ground_dispatch_target" --reporter compact
./tests/cata_test "[multi_z]" --reporter compact
```

Results:

```text
Passed 1 test case with 19 assertions.
Passed both 2 test cases with 35 assertions.
```

Evidence class: deterministic planner/site-identity coverage. The `[multi_z]` row includes the existing one-site/z-footprint assertion, and the targeted vertical row covers elevated target identity plus reachable-ground routing.

## Live/harness artifact evidence

Scenario:

```sh
python3 tools/openclaw_harness/startup_harness.py probe bandit.roof_z_dispatch_fallback_mcw
```

Run: `.userdata/dev-harness/harness_runs/20260503_201355/`

Run summary from `probe.report.json`:

```text
scenario=bandit.roof_z_dispatch_fallback_mcw
feature_proof=true
verdict=artifacts_matched
evidence_class=feature-path
proof_classification.status=green
wait_step_status=green_wait_steps_proven
step_ledger_status=green_step_local_proof
step_ledger_summary=8 green / 0 yellow / 0 red_or_blocked
cleanup.status=terminated
```

Decisive same-run artifact excerpts:

```text
20:14:18.367 INFO : bandit_live_world dispatch plan: site=overmap_special:bandit_camp@140,51,0 target=player@140,39,5 candidate_reason=remembered_camp_map_lead job=stalk selected_members=4 signal_packet=none remembered_lead=overmap_special:bandit_camp@140,51,0#lead:basecamp_activity:player@140,39,5@140,39,5
20:14:18.368 INFO : local_gate site=overmap_special:bandit_camp@140,51,0 active_group=overmap_special:bandit_camp@140,51,0#dispatch target=player@140,39,5 live_dispatch_goal=140,39,0 active_job=stalk profile=camp_style posture=hold_off
- live_candidate reason=remembered_camp_map_lead distance=13 cap=2 signal_packet=none remembered_lead=overmap_special:bandit_camp@140,51,0#lead:basecamp_activity:player@140,39,5@140,39,5 signal_distance=0
```

Negative guard: `audit_no_route_missing_for_roof_z_dispatch.metadata.json` records both forbidden same-run patterns absent:

```text
absent: bandit_live_world dispatch rejected: && site=overmap_special:bandit_camp@140,51,0 && reason=route_missing
absent: cadence_skip: && reason=30_minute_throttle && active_sites=0 && travelling_npcs=0
```

Saved-state guard: `audit_saved_roof_z_dispatch_state.metadata.json` finds the same camp site with:

```text
site_id=overmap_special:bandit_camp@140,51,0
active_group_id=overmap_special:bandit_camp@140,51,0#dispatch
active_target_id=player@140,39,5
active_target_omt=140,39,0
active_job_type=stalk
active_member_ids=5,6,7,8
```

Evidence class: current feature-path/live source artifact + saved-state proof. Startup/load is only the gate; the credited behavior is the 30-minute live dispatch cadence, same-run artifact lines, negative route/throttle guard, and saved active dispatch writeback.

## Verdict

Slice 3 roof/tower-z dispatch fallback is green enough to unblock the ordered debug stack. The next active execution target should move to Slice 4 hostile-camp escalation after scout confirmation.
