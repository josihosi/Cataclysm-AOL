# CAOL-WRITHING-STALKER-PATTERN-TESTS-v0 — closure proof

## Verdict

**CLOSED / CHECKPOINTED GREEN V0**

Schani promoted the queued writhing-stalker follow-up into the next active lane and refined it from primitive minimap tests into fun-scenario pass/fail benchmarks: fair dread, no cheating, meaningful counterplay, repeated attacks with breathing room, and injured retreat.

This closure lands that as a deterministic behavior-pattern helper in `tests/writhing_stalker_test.cpp`. No gameplay tuning/code behavior changed.

## Evidence class

- **Primary evidence:** deterministic evaluator/pattern tests.
- **Live seam mapping:** existing code path `monster::plan()` -> `apply_writhing_stalker_plan()` -> `writhing_stalker::evaluate_live_response()` in `src/monmove.cpp`; this run changed tests only, so no fresh live harness probe was needed.
- **Prior live footing preserved:** `writhing_stalker.live_plan_seam_mcw`, `writhing_stalker.live_shadow_strike_mcw`, `writhing_stalker.live_exposed_retreat_mcw`, and `writhing_stalker.live_no_omniscient_beeline_mcw` remain the live-path receipts for the unchanged v0 seam.

## What changed

`tests/writhing_stalker_test.cpp` now has a small behavior-pattern harness:

- `stalker_pattern_row`
- `trace_rows`
- `decision_trace_name`
- `route_trace_name`
- `pattern_base_context`
- `run_vulnerable_stalker_pattern`
- `count_decisions`

New tests:

- `writhing_stalker_pattern_helper_covers_fair_dread_baselines`
- `writhing_stalker_pattern_helper_traces_repeated_strikes_then_injured_retreat`

## Scenario / benchmark coverage

Green deterministic coverage now includes:

- no-evidence / no-magic baseline: no target/strike/cooldown without believable local evidence;
- weak/fading evidence decay: latch times out instead of becoming permanent omniscience;
- shadow route preference: cover route wins over direct open route when available;
- light/focus counterplay: exposed/focused stalker withdraws instead of suicide-charging;
- zombie-distraction guardrail: zombie pressure alone does not grant strike permission;
- vulnerable-player attack rhythm: hurt/bleeding/tired/distracted/noisy player near clutter can be struck;
- cooldown anti-spam: turns after a strike are `cooling_off`, not immediate repeat strikes;
- repeated-attack cadence: healthy stalker can shadow -> strike -> cool down/reposition -> shadow -> strike;
- badly-injured retreat: at `hp_percent=50` the stalker withdraws even while the player remains vulnerable;
- stuck/jitter smell: no withdraw -> strike snapback in the repeated trace.

## Repeated-attack / retreat trace excerpt

From `./tests/cata_test "writhing_stalker_pattern_helper_traces_repeated_strikes_then_injured_retreat" -s`:

```text
writhing stalker pattern trace:
  t0 dist=5 hp=100 cd=0 decision=shadow route=cover_shadow strikes=0 reason=live_shadowing_before_strike_window
  t1 dist=3 hp=100 cd=0 decision=strike route=cover_shadow strikes=0 reason=live_vulnerability_window_strike
  t2 dist=4 hp=100 cd=2 decision=cooling_off route=none strikes=1 reason=live_latch_cooldown_blocks_relatched_pressure
  t3 dist=5 hp=100 cd=1 decision=cooling_off route=none strikes=1 reason=live_latch_cooldown_blocks_relatched_pressure
  t4 dist=5 hp=100 cd=0 decision=shadow route=cover_shadow strikes=1 reason=live_shadowing_before_strike_window
  t5 dist=3 hp=100 cd=0 decision=strike route=cover_shadow strikes=1 reason=live_vulnerability_window_strike
  t6 dist=4 hp=50 cd=0 decision=withdraw route=cover_shadow strikes=2 reason=live_stalker_hurt_withdraw
  t7 dist=7 hp=50 cd=0 decision=withdraw route=cover_shadow strikes=2 reason=live_stalker_hurt_withdraw
```

Key facts:

- strike count: `2`;
- turns between strikes: first strike at `t1`, second strike at `t5`;
- cooldown/reposition turns: `t2` and `t3` block immediate repeat strikes;
- retreat trigger: `hp_percent=50`, reason `live_stalker_hurt_withdraw`;
- jitter count: `0` for withdraw -> strike snapback.

## Gates run

```text
make -j4 tests/writhing_stalker_test.o tests LINTJSON=0 ASTYLE=0 && ./tests/cata_test "[writhing_stalker][ai]"
```

Result:

```text
All tests passed (97 assertions in 8 test cases)
```

Additional full focused filter:

```text
./tests/cata_test "[writhing_stalker]"
```

Result:

```text
All tests passed (129 assertions in 10 test cases)
```

Trace extraction gate:

```text
./tests/cata_test "writhing_stalker_pattern_helper_traces_repeated_strikes_then_injured_retreat" -s
```

Result:

```text
All tests passed (16 assertions in 1 test case)
```

## Caveats / non-claims

- This is a deterministic behavior-pattern closure, not a new live GUI/harness scene.
- No source behavior changed; no tuning change was required.
- `eval_us` remains visible in the existing live-plan debug line from `src/monmove.cpp`, but this test-only closure does not add a new performance harness run.
- The helper is intentionally small and textual rather than a rich minimap renderer; it meets the packet’s “primitive minimap/ASCII/equivalent” bar through compact trace rows.
