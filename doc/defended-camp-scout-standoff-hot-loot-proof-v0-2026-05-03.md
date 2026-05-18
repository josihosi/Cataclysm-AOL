# Defended camp scout/standoff/hot-loot proof v0 — 2026-05-03

Active lane: `CAOL-JOSEF-LIVE-DEBUG-BATCH-v0`, Slice 2.

## Claim boundary

This is a partial Slice 2 checkpoint, not closure of the whole defended-camp/stalking-mode slice.

Credited here:
- ordinary defended-camp scout/hold-off watch distance is restored to about `5` OMT where the planner/local gate can choose it;
- local-gate hold-off keeps the dispatched scout away from the defended doorstep instead of collapsing to the 1-2 OMT loiter shape;
- hot defended doorstep pickup is blocked for active hostile dispatch members when the camp scene is exposed/recently exposed and the posture is stalk/hold-off.

Still open after this proof:
- full live sight-avoidance/pathing proof that sighted or smoke-revealed scouts/stalkers visibly break LoS, reroute, wait, or escalate;
- cannibal/compatible-hostile live parity beyond deterministic/shared local-gate seams;
- a green harness `feature_proof` row for the standoff scenario, because the current scenario's startup/safe-mode screen checkpoints are OCR-caveated even though the wait-action artifact is decisive for the `5` OMT local-gate claim.

## Code checkpoint

Commit under test: `3d4abc4cf0` (`Fix shakedown dialogue visible options`) with Slice 2 behavior in parent `540b131791` (`Bandit scouts hold wider defended standoff`).

Touched behavior:
- `src/bandit_live_world.cpp` / `.h`
  - `ordinary_scout_watch_standoff_omt()` now returns `5`.
  - `minimum_hold_off_standoff_omt()` inherits that distance.
  - `choose_hold_off_standoff_goal(...)` therefore keeps hold-off dispatch goals at least five OMT out.
  - `hot_defended_doorstep_blocks_pickup(...)` is the deterministic guard for active hostile dispatch members in exposed/recently exposed defended camp scenes.
- `src/npcmove.cpp`
  - live NPC item pickup now calls the hot-doorstep guard for non-ally active hostile dispatch members near a player camp before planning or completing casual pickup.

## Deterministic/local gate evidence

Command:

```sh
git diff --check && make -j4 tests/bandit_live_world_test.o tests LINTJSON=0 ASTYLE=0 && ./tests/cata_test "[bandit][live_world]" --reporter compact
```

Result at `18:21` local:

```text
Cataclysm DDA version 3d4abc4cf0
Passed all 61 test cases with 1274 assertions.
```

Relevant assertions include:
- defended camp hold-off report still renders `posture=hold_off`;
- `ordinary_scout_watch_standoff_omt() == 5`;
- `minimum_hold_off_standoff_omt() == 5`;
- requested shorter hold-off goals clamp to five OMT;
- exposed/recently exposed active hostile dispatch member on defended camp footing trips `hot_defended_doorstep_blocks_pickup(...)`;
- non-member/non-hot/attack-forward/shakedown surfaces are not incorrectly blocked by that guard.

Evidence class: deterministic planner/local-gate + live-source hook coverage.

## Live/harness artifact evidence

Scenario:

```sh
python3 tools/openclaw_harness/startup_harness.py probe bandit.extortion_at_camp_standoff_mcw
```

Run: `.userdata/dev-harness/harness_runs/20260503_181426/`

Decisive artifact lines from `probe.report.json` / `probe.artifacts.log`:

```text
18:14:52.520 INFO : local_gate site=overmap_special:bandit_camp@140,51,0 active_group=overmap_special:bandit_camp@140,51,0#dispatch target=player@140,41,0 active_job=scout profile=camp_style posture=hold_off strength=1 pack_size=1 threat=3 opportunity=2 margin=0 darkness_or_concealment=no standoff_distance=5 basecamp_or_camp=yes current_exposure=no recent_exposure=yes sight_exposure=recent local_contact=no rolling_travel=no shakedown=no combat_forward=no
- live_dispatch_goal=140,46,0
```

Run summary:
- startup gate: green;
- wait-action ledger: `green_wait_steps_proven`, `1/1` green;
- artifact patterns all matched for `local_gate`, active dispatch group, `posture=hold_off`, `basecamp_or_camp=yes`, and `live_dispatch_goal=140,46,0`;
- overall harness classification: `yellow_step_local_proof_incomplete` / `feature_proof=false` because the scenario's `post_load_settle` and `disable_safe_mode` screen checkpoints requested OCR without text guards.

Evidence class: current-build live source/harness artifact proof for the five-OMT local-gate standoff goal, with an explicit yellow caveat for scenario-local OCR checkpoint hygiene.

## Josef current-pass playtest read

After this checkpoint, Josef reported his own tests looked good overall: the bandits did their thing. He explicitly said Andi can continue with the debug notes and worry about this deeper playtest later.

Interpretation: the `5` OMT standoff / hot-doorstep pickup checkpoint is good enough to unblock the ordered debug stack. The deeper sight/smoke visible behavior remains real validation debt, but it is deferred hardening, not the next Andi blocker.

## Deferred proof

Do not treat this packet as full closure of every Slice 2 edge case. Deferred useful work is a changed live proof route or harness scenario that directly demonstrates the visible sight/smoke/hot-doorstep behavior: a sighted or smoke-revealed bandit/cannibal/compatible scout breaks LoS, backs off, reroutes, waits, or escalates instead of loitering or casually looting the defended doorstep.
