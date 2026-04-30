# Andi handoff: CAOL-WRITHING-STALKER-PATTERN-TESTS-v0

## Current canon state

`CAOL-WRITHING-STALKER-PATTERN-TESTS-v0` is **CLOSED / CHECKPOINTED GREEN V0**.

Schani promoted the queued writhing-stalker pattern lane into active and refined it into fun-scenario pass/fail benchmarks. The packet is now closed as deterministic behavior-pattern proof. No gameplay tuning/source behavior changed.

Authoritative canon is `Plan.md`, with `TODO.md`, `SUCCESS.md`, `TESTING.md`, and `doc/work-ledger.md` aligned downstream. If this file disagrees with canon, repair this file from canon.

`CAOL-ROOF-HORDE-NICE-FIRE-v0`, old `CAOL-WRITHING-STALKER-v0`, Smart Zone, old fire proof lanes, `CAOL-MULTI-CAMP-SIGNAL-GAUNTLET-v0`, and this pattern packet must not be reopened without explicit Schani/Josef promotion.

## Canonical packet

- Contract: `doc/writhing-stalker-behavior-pattern-minimap-packet-v0-2026-04-30.md`
- Imagination source: `doc/writhing-stalker-behavior-pattern-imagination-source-of-truth-2026-04-30.md`
- Proof: `doc/writhing-stalker-behavior-pattern-proof-v0-2026-04-30.md`
- Prior live seam footing: `doc/writhing-stalker-playtest-ladder-v0-2026-04-30.md`

## Closed result

`tests/writhing_stalker_test.cpp` now has a compact behavior-pattern helper:

- `stalker_pattern_row`
- `trace_rows`
- `decision_trace_name`
- `route_trace_name`
- `pattern_base_context`
- `run_vulnerable_stalker_pattern`
- `count_decisions`

Credited tests:

- `writhing_stalker_pattern_helper_covers_fair_dread_baselines`
- `writhing_stalker_pattern_helper_traces_repeated_strikes_then_injured_retreat`

Coverage: no-magic/no-evidence, weak evidence decay, cover route preference, bright/focused counterplay, zombie-pressure guardrail, vulnerability strike window, cooldown anti-spam, healthy repeated attacks, badly-injured retreat, and jitter smell.

Trace headline: `shadow -> strike -> cooling_off -> cooling_off -> shadow -> strike -> withdraw`; strike count `2`; retreat trigger `hp=50`; jitter count `0`.

## Evidence

- `make -j4 tests/writhing_stalker_test.o tests LINTJSON=0 ASTYLE=0 && ./tests/cata_test "[writhing_stalker][ai]"` — passed, `97 assertions in 8 test cases`.
- `./tests/cata_test "[writhing_stalker]"` — passed, `129 assertions in 10 test cases`.
- `./tests/cata_test "writhing_stalker_pattern_helper_traces_repeated_strikes_then_injured_retreat" -s` — passed, `16 assertions in 1 test case`, trace captured in proof doc.

## Caveats

- Deterministic helper proof only; not a new live GUI/harness scene.
- No source behavior/tuning changed, so no fresh live-seam rerun was needed.
- Existing live path remains `monster::plan()` -> `apply_writhing_stalker_plan()` -> `writhing_stalker::evaluate_live_response()`.

## Next state

No active or greenlit queued C-AOL lane remains in repo canon after this closure. Wait for Schani/Josef to promote the next lane; do not freestyle from held/parked receipts.
