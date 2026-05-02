# CAOL-BANDIT-SIGNAL-ADAPTER-REDUCTION-v0 proof

Status: CLOSED / CHECKPOINTED GREEN V0 / ANTI-REDUNDANCY PACKAGE

Contract: `doc/bandit-signal-adapter-reduction-packet-v0-2026-05-01.md`.
Imagination source: `doc/anti-redundancy-packaging-imagination-source-of-truth-2026-05-01.md`.

## What changed

The live bandit field-signal path now routes local `fd_fire` / `fd_smoke` observations through a named `bandit_mark_generation::adapt_local_field_signal_reading()` adapter before producing smoke/light projections.

The custom bandit interpretation stayed in the existing mark-generation/live-world layer. The cleanup is only the basic environmental seam: local field intensity + time/weather/exposure is converted into existing smoke/light packet inputs in one shared adapter instead of open-coded in `src/do_turn.cpp`.

Touched source:
- `src/bandit_mark_generation.h`
- `src/bandit_mark_generation.cpp`
- `src/do_turn.cpp`
- `tests/bandit_mark_generation_test.cpp`

## Evidence

Gates run:
- `git diff --check`
- `make -j4 tests/bandit_mark_generation_test.o src/bandit_mark_generation.o obj/do_turn.o LINTJSON=0 ASTYLE=0`
- standalone adapter probe linked against `src/bandit_mark_generation.o` and `obj/bandit_dry_run.o`
- `make -j1 tests LINTJSON=0 ASTYLE=0 && ./tests/cata_test "[bandit][marks]"` -> `All tests passed (236 assertions in 18 test cases)`

The focused test additions prove:
- local fire+smoke readings produce both smoke and light projections;
- smoke-only readings do not create light projections or horde signal power;
- adapter output matches the direct `adapt_smoke_packet()` / `adapt_light_packet()` projections for the same mapped inputs;
- projected light still feeds `horde_signal_power_from_light_projection()`.

The live path still consumes the horde seam through `signal_live_hordes_from_light_observations()`, which calls `overmap_buffer.signal_hordes(...)` for light observations with positive `horde_signal_power`.

## Caveats / boundary

This is cleanup/refactor only. It does not redesign bandit dispatch, roster state, camp-map memory, structural bounty, signal ranges, horde tuning, or a generic overmap event bus.

No new bandit behavior claim is made from the refactor. Existing roof-fire/live-signal behavior is preserved by keeping the prior packet mapping and horde signal seam intact.
