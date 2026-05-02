# Writhing stalker hit-fade retreat-distance proof v0 — 2026-05-02

Lane: `CAOL-WRITHING-STALKER-HIT-FADE-RETREAT-DISTANCE-v0`.
Contract: `doc/writhing-stalker-hit-fade-retreat-distance-packet-v0-2026-05-02.md`.

## Verdict

Agent-side v0 is green/checkpointed with staged-but-live caveats.

The stalker now owns its custom burst/fade cadence instead of generic `HIT_AND_RUN`: favorable vulnerable pressure can produce a bounded short burst, while counterpressure/light/allies/injury shorten persistence and increase retreat caution. After the burst, the live plan aims for about `8+` tiles of retreat when pathing allows.

## Behavior change

- Removed `HIT_AND_RUN` from `mon_writhing_stalker`; the generic melee hook forced `effect_run` after every hit and prevented a custom bounded burst.
- Added stalker burst state (`caol_writhing_stalker_burst_count`) consumed by the live planner.
- Added stress/counterpressure shaping:
  - vulnerability/zombie pressure can raise the burst limit up to `4`;
  - bright exposure, player focus, allied NPC support, and injury lower the burst limit;
  - counterpressure also increases requested retreat distance.
- Added retreat destination selection around the target, preferring reachable tiles roughly at the requested retreat range instead of a raw one-vector flee destination.
- Kept cooling-off non-refresh behavior so the stalker can breathe out and later re-engage instead of freezing in permanent cooldown.

## Evidence classes

### Original raptor reference

Source inspection: current flesh raptor still carries generic `HIT_AND_RUN` (`data/json/monsters/zed_misc.json` around the flesh raptor entry), and `monster::melee_attack` adds `effect_run` for `mon_flag_HIT_AND_RUN`. That is the original reference feel for a short hit/fade, but the stalker cannot keep that generic flag because it forces one-hit fleeing before the desired 2-4 hit bounded burst.

### Deterministic/unit and live-path tests

Gate: `git diff --check && make -j8 tests LINTJSON=0 ASTYLE=0 && ./tests/cata_test "[writhing_stalker]"`.

Result: green; `All tests passed (206 assertions in 17 test cases)`.

Coverage added/kept:
- stalker no longer has `HIT_AND_RUN` and instead uses the custom live planner;
- burst limit is shaped by stress/counterpressure (`1` under light/focus/allied support, up to `4` under high vulnerable pressure);
- post-burst live plan retreats to an about-8-tile destination;
- existing writhing-stalker guarantees still pass.

### Current-build tiles build

Gate: `./just_build_macos.sh > /tmp/caol-writhing-stalker-hit-fade-tiles-build-current.log 2>&1`.

Result: exited `0`; `cataclysm-tiles` linked. Linker emitted existing macOS deployment-version duplicate/SDK warnings only.

### Fresh staged-but-live feature proof

Scenario: `writhing_stalker.live_hit_fade_retreat_mcw`.

Run: `.userdata/dev-harness/harness_runs/20260502_113738/`.

Result: `feature_proof=true`, `evidence_class=feature-path`, `verdict=artifacts_matched`, `step_ledger_status=green_step_local_proof`, no runtime warnings, weather clear.

Matched artifact patterns:
- `writhing_stalker live_plan:`
- `decision=strike`
- `decision=withdraw`
- `decision=cooling_off`
- `burst=0/2`, `burst=1/2`, `burst=2/2`
- `retreat_distance=8`
- `cooldown=yes`
- `eval_us=`

Decisive live-plan excerpts from `audit_writhing_stalker_hit_fade_retreat_live_plan.log_delta.txt`:

```text
11:37:59.220 INFO : writhing_stalker live_plan: decision=strike route=direct_forced reason=live_vulnerability_window_strike opportunity=74 confidence=21 evidence=yes overmap_interest=no distance=3 zombie_pressure=0 allied_support=0 burst=0/2 retreat_distance=8 target_bright=no stalker_bright=no target_focus=no cooldown=no eval_us=8
11:37:59.645 INFO : writhing_stalker live_plan: decision=withdraw route=direct_forced reason=live_burst_limit_reached_withdraw opportunity=74 confidence=21 evidence=yes overmap_interest=no distance=3 zombie_pressure=0 allied_support=0 burst=2/2 retreat_distance=8 target_bright=no stalker_bright=no target_focus=no cooldown=no eval_us=26
11:37:59.645 INFO : writhing_stalker live_plan: decision=cooling_off route=none reason=live_latch_cooldown_blocks_relatched_pressure opportunity=0 confidence=0 evidence=yes overmap_interest=no distance=4 zombie_pressure=0 allied_support=0 burst=2/1 retreat_distance=8 target_bright=no stalker_bright=no target_focus=no cooldown=yes eval_us=7
```

## Caveats

- This is staged-but-live McWilliams proof, not natural random discovery.
- The earlier watched seed `.userdata/dev-harness/harness_runs/20260502_015032/` remains yellow/debug footing only because of runtime-version mismatch.
- The live row proves planner decisions/artifacts and screenshots from the harness path; final human taste for “too scary / too forgiving” remains optional Josef play-feel, not an agent-side blocker.
