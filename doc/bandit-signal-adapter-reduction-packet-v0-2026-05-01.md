# CAOL-BANDIT-SIGNAL-ADAPTER-REDUCTION-v0

Status: CLOSED / CHECKPOINTED GREEN V0 / ANTI-REDUNDANCY PACKAGE

Imagination source: `doc/anti-redundancy-packaging-imagination-source-of-truth-2026-05-01.md`.

Closure proof: `doc/bandit-signal-adapter-reduction-proof-v0-2026-05-02.md`.

## Summary

Reduce the bandit smoke/light/fire signal layer so it reads as an adapter over existing local field, light/weather, and horde-signal seams rather than a private bandit physics universe. Bandit interpretation remains custom; basic environmental observation and horde signaling should be visibly grounded in existing game systems.

## Scope

- Inventory the current runtime path from local `fd_fire` / `fd_smoke` observation through `bandit_mark_generation` and `overmap_buffer.signal_hordes`.
- Keep bandit mark scoring, bounty/threat interpretation, and site-memory refresh custom where they are product-specific.
- Extract or rename any basic smoke/light/fire observation helper so it is clearly an adapter over local fields/time/weather rather than bandit-owned physics.
- Preserve current roof-fire/horde and live-signal proof behavior unless a separate promoted tuning packet changes it.
- Add tests that prove existing horde signaling remains the downstream seam for horde reaction.

## Non-goals

- Do not redesign bandit live-world dispatch, roster state, structural bounty, or camp-map memory in this package.
- Do not change signal tuning/ranges unless required to preserve behavior after seam cleanup.
- Do not reopen roof-fire horde, real-fire signal, multi-camp gauntlet, or structural-bounty closed claims.
- Do not convert this into a generic overmap event bus project.

## Success state

- [x] The source path from local fire/smoke/light observation to bandit mark input is named and tested as an adapter.
- [x] Bandit mark-generation tests remain green for smoke/weather, light/time/weather, human route, repeated-site, and moving-memory cases touched by the package.
- [x] A focused test or harness/log assertion proves horde reactions still go through `overmap_buffer.signal_hordes` rather than a parallel private horde path.
- [x] Existing green roof-fire/live-signal expectations remain true or any changed expectation is explicitly classified as tuning, not cleanup.
- [x] No new bandit behavior claim is made from adapter refactoring alone.

## Targeted tests

- Focused `bandit_mark_generation` unit tests for smoke and light packet projection after adapter cleanup.
- Live-field adapter test with local `fd_fire` / `fd_smoke` inputs mapped into smoke/light packet inputs.
- Horde seam guard: signal output calls or reaches the existing `overmap_buffer.signal_hordes` / `overmap::signal_hordes` path.
- Regression check against current roof-fire/live-signal proof assumptions if touched.

## Cautions

The redundancy problem is not that bandits interpret signals. That part is the feature. The smell is basic fire/smoke/light truth becoming bandit-private vocabulary. Keep the bandit brain; reduce the duplicate senses.
