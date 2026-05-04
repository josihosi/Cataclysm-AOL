# Writhing stalker distance / sight / threat-drop live proof v0 — 2026-05-04

Active lane: `CAOL-JOSEF-LIVE-DEBUG-BATCH-v0`, Slice 7.

## Claim

Current-build staged/live proof now covers the player-facing Slice 7 timing rhythm on top of the deterministic checkpoint:

- the stalker begins as a distant/hidden shadow rather than immediate melee spam;
- it approaches into a short strike window only when the vulnerable/dark-scene opportunity is present;
- after the burst budget is spent, it withdraws and then cools off while writing back far-watch `stalk_omt=5` memory;
- the proof stays on the real live `monster::plan` seam, not just the isolated evaluator tests.

## Source footing

Source/test checkpoint: `doc/writhing-stalker-distance-sight-threat-drop-deterministic-checkpoint-v0-2026-05-04.md`.

That checkpoint covers the code-side changes for:

- `5` OMT cautious stalk distance writeback;
- sighted stalker LoS-break/withdraw decisions;
- low zombie-pressure threat-drop pounce with evidence;
- post-burst boot-out / spent-disengage memory.

## Deterministic gate

```text
git diff --check
make -j4 tests LINTJSON=0 ASTYLE=0
./tests/cata_test "[writhing_stalker]" --reporter compact
```

Result:

```text
Passed all 25 test cases with 283 assertions.
```

## Live/staged gate

Fresh tiles build before the live probe:

```text
make -j4 TILES=1 cataclysm-tiles
python3 tools/openclaw_harness/startup_harness.py probe writhing_stalker.live_hit_fade_retreat_mcw --compact-stdout
```

Run:

```text
.userdata/dev-harness/harness_runs/20260504_113828/
probe.report.json
probe.artifacts.log
```

Report verdict:

```text
verdict=artifacts_matched
evidence_class=feature-path
feature_proof=true
step_ledger_summary.status=green_step_local_proof
green_count=6 red_or_blocked_count=0
window_title="Cataclysm: Dark Days Ahead - d5e31b2ef5"
version_matches_runtime_paths=true
```

Decisive live-plan sequence from `probe.artifacts.log`:

```text
writhing_stalker live_plan: decision=shadow route=cover_shadow reason=live_shadowing_before_strike_window ... distance=8 ... burst=0/2 retreat_distance=8 ... cooldown=no
writhing_stalker live_plan: decision=strike route=direct_forced reason=live_vulnerability_window_strike ... distance=3 ... burst=0/2 retreat_distance=8 ... cooldown=no
writhing_stalker live_plan: decision=strike route=direct_forced reason=live_vulnerability_window_strike ... distance=3 ... burst=1/2 retreat_distance=8 ... cooldown=no
writhing_stalker live_plan: decision=withdraw route=direct_forced reason=live_burst_limit_reached_withdraw ... writeback_intent=spent_disengage persistent=yes stalk_omt=5 ... burst=2/2 retreat_distance=8 ... cooldown=no
writhing_stalker live_plan: decision=cooling_off route=cover_shadow reason=live_cooldown_blocks_repeat_strike ... handoff_intent=spent_disengage persistent=yes stalk_omt=5 ... burst=2/1 retreat_distance=8 ... cooldown=yes
writhing_stalker live_plan: decision=cooling_off route=direct_forced reason=live_cooldown_blocks_repeat_strike ... persistent=yes stalk_omt=5 ... distance=8 ... cooldown=yes
```

## Boundary

This closes Slice 7 as a staged/live timing checkpoint for the named rhythm. It does not claim natural random discovery, broad house navigation, smoke-out playfeel, player-hunts-it combat feel, or every high-z/inside-house variant from the larger future playtest matrix. Those remain future/human-playtest/hardening surfaces unless canon promotes them again.
