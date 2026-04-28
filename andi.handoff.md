# Andi handoff: Cannibal camp night-raid behavior packet v0

## Canon anchors

- active plan: `Plan.md`
- active queue: `TODO.md`
- validation ledger: `TESTING.md`
- success ledger: `SUCCESS.md`
- imagination source: `doc/cannibal-camp-night-raid-imagination-source-of-truth-2026-04-28.md`
- code audit / gap map: `doc/cannibal-camp-night-raid-code-audit-2026-04-28.md`
- canonical contract: `doc/cannibal-camp-night-raid-behavior-packet-v0-2026-04-28.md`

## Current state

`Cannibal camp pack-size + smoke-light/darkness/sight-avoid substrate v0` is landed as deterministic/code substrate only. It is not live/harness night-raid product proof.

The prior `C-AOL harness trust audit + proof-freeze v0` lane is checkpointed/held: its proof-freeze matrix and primitive evidence are preserved, and remaining product-playtest blockers live in the actual-playtest stack rather than keeping that process lane active.

## What changed

- `src/bandit_live_world.cpp` now applies profile-specific cannibal pack pressure: stalk/attack-style pressure requires a multi-member cannibal group after reserve, one-dispatchable-member pressure stays scout/probe instead of full attack, smoke/light nearby leads classify as scout/stalk/dispatch pressure rather than instant combat, and reports `pack_size` / `available_after_reserve`.
- `src/bandit_live_world.h` adds `local_gate_input::darkness_or_concealment`.
- Cannibal local-gate decisions now use darkness/concealment as a bounded attack-window modifier, require pack contact for `attack_now`, degrade lone contact to `probe`, hold off after recent camp-edge exposure, preserve daylight/no-cover hold-off, and still abort under overwhelming threat.
- Reports now include lead source/notes where available, `pack_size`, and `darkness_or_concealment` alongside existing profile, exposure, posture, shakedown, and note text.
- `tests/bandit_live_world_test.cpp` now covers multi-member cannibal dispatch/save-load, lone cannibal pack-pressure block, manual lone scout/probe non-attack, daylight versus darkness local-gate split, high-threat abort, cannibal no-extort, and bandit approach-gate regression.

## Evidence

- `git diff --check`
- `make -j4 obj/bandit_live_world.o tests/bandit_live_world_test.o LINTJSON=0 ASTYLE=0`
- `make -j4 tests LINTJSON=0 ASTYLE=0`
- `./tests/cata_test "[bandit][live_world][cannibal]"` — 110 assertions / 4 test cases
- `./tests/cata_test "[bandit][live_world][approach_gate]"` — 36 assertions / 1 test case
- `./tests/cata_test "[bandit][live_world]"` — 672 assertions / 29 test cases

## Remaining boundary

Do not claim live night-raid product behavior yet. A later promoted slice still needs real time/light/visibility wiring and a proof-freeze harness/product scenario that reaches actual dispatch/local-contact with a multi-member cannibal night attack and no shakedown surface.

## Recommended next action

Schani plans-aux / review should decide whether the next lane is the later cannibal live-wiring/product proof, the held harness trust-audit/proof-freeze lane, or the greenlit bandit camp-map stack. Frau no-nudge needed unless review finds a proof/scope contradiction.
