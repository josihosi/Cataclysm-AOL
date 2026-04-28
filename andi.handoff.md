# Andi handoff: Cannibal camp night-raid behavior packet v0

## Active target

`Cannibal camp night-raid behavior packet v0` is active. Josef promoted the live/harness product-proof slice, and the live matrix is now green for the named cannibal scenarios, with persistence covered as saved-state/writeback proof plus no-fixture reload saved-file support.

Canonical anchors:

- `Plan.md`
- `TODO.md`
- `TESTING.md`
- `SUCCESS.md`
- `doc/cannibal-camp-night-raid-behavior-packet-v0-2026-04-28.md`
- `doc/cannibal-camp-night-raid-code-audit-2026-04-28.md`
- `doc/cannibal-camp-night-raid-live-playtest-matrix-v0-2026-04-28.md`

## Current implementation substrate

Deterministic/code substrate for `Cannibal camp pack-size + darkness/sight-avoid local-gate substrate v0` is present in the dirty tree:

- `src/bandit_live_world.h` adds `local_gate_input::darkness_or_concealment` and `local_gate_input::current_exposure`; `src/do_turn.cpp` derives current player/basecamp-ally exposure from live active members before the local gate runs.
- Cannibal local-gate decisions use darkness/concealment as a bounded attack-window modifier, require pack contact for `attack_now`, degrade lone contact to `probe`, and hold off after current/recent exposure instead of continuing a visible beeline.
- Reports include lead source/notes where available, `pack_size`, `darkness_or_concealment`, `current_exposure`, `recent_exposure`, and `sight_exposure` alongside profile, posture, shakedown, and note text.
- Tests cover multi-member cannibal dispatch/save-load, lone cannibal pack-pressure block, daylight versus darkness local-gate split, high-threat abort, cannibal no-extort, bandit approach-gate regression, and bounded sight-avoid reposition helper behavior.

## Latest deterministic validation for substrate

- `git diff --check`
- `make -j4 obj/do_turn.o obj/bandit_live_world.o tests/bandit_live_world_test.o LINTJSON=0 ASTYLE=0`
- `make -j4 tests LINTJSON=0 ASTYLE=0`
- `./tests/cata_test "[bandit][live_world][cannibal]"`
- `./tests/cata_test "[bandit][live_world][sight_avoid]"`
- `./tests/cata_test "[bandit][live_world][approach_gate]"`
- `./tests/cata_test "[bandit][live_world]"`

## Live playtest evidence

Green feature-path runs:

1. Day smoke/light pressure: `.userdata/dev-harness/harness_runs/20260428_124902/` — `feature_proof=true`, 2/2 green steps, smoke lead, cannibal `active_job=stalk`, `posture=hold_off`, `pack_size=2`, no shakedown/combat-forward.
2. Night local-contact attack window: `.userdata/dev-harness/harness_runs/20260428_124947/` — `feature_proof=true`, 2/2 green steps, direct/local contact, `darkness_or_concealment=yes`, `pack_size=2`, `posture=attack_now`, `combat_forward=yes`, `shakedown=no`.
3. Exposed/recent-sight hold-off: `.userdata/dev-harness/harness_runs/20260428_125138/` — `feature_proof=true`, 5/5 green steps, bounded 20-turn window, cannibal `sight_exposure=recent`, `posture=hold_off`, no shakedown/combat-forward. The inherited `bandit_camp` reposition artifact in that fixture is control/source-fixture noise, not cannibal credit.
4. Daylight/high-threat negative: `.userdata/dev-harness/harness_runs/20260428_135323/` — current runtime `48abd82de9`, `feature_proof=true`, 2/2 green steps, daylight signal scan, cannibal `threat=3`, `opportunity=2`, `darkness_or_concealment=no`, `posture=hold_off`, no shakedown/combat-forward.
5. Repeatability smoke: `repeatability --count 2 cannibal.live_world_day_smoke_pressure_mcw` passed stable, with runs `.userdata/dev-harness/harness_runs/20260428_125319/` and `.userdata/dev-harness/harness_runs/20260428_125342/`.
6. Save/writeback persistence: `.userdata/dev-harness/harness_runs/20260428_130948/` — `feature_proof=true`, 7/7 green steps, active day-smoke cannibal `stalk` group saved with player-save mtime advance and saved `dimension_data.gsav` metadata for `profile=cannibal_camp`, active group/target, `active_member_ids=[4,5]`, and `known_recent_marks` containing `live_smoke@...`; `intelligence_map.leads=[]` is out of scope.
7. No-fixture reload support: `.userdata/dev-harness/harness_runs/20260428_131031/` — fresh startup without fixture reinstall, 2/2 green step rows, saved metadata still shows the active cannibal group/profile/target/member state plus smoke mark. Top-level classifier is yellow/no-new-artifacts, so this is reload support, not a separate behavior artifact.

## Current boundary / next review target

Do not overclaim in-memory reload behavior: persistence is green as saved-state/writeback proof; the no-fixture reload audit is saved-file support only, not fresh in-memory local-gate proof. The separated high-threat/daylight negative is now green. Remaining open seam is optional/product-review only:

- optional labeled bandit-control contrast for shakedown/pay/fight, if product review wants it beyond the cannibal `shakedown=no` contact proof.

Recommended next action: Schani plans-aux review whether to checkpoint the live slice now or request the optional labeled bandit-control contrast before checkpointing.
