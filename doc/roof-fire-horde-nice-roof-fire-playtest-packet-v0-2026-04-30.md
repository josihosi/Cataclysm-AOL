# Roof-fire horde nice-fire playtest packet v0 — 2026-04-30

## Classification

Status: **COMPLETE / GREEN FEATURE-PATH PROOF**.

Ledger ID: `CAOL-ROOF-HORDE-NICE-FIRE-v0`.

Imagination source: `doc/roof-fire-horde-nice-roof-fire-imagination-source-of-truth-2026-04-30.md`.

Prior proof this extends: `CAOL-ROOF-HORDE-SPLIT-v0`, documented in `doc/roof-fire-horde-detection-proof-v0-2026-04-29.md`.

## Goal

Completed in green run `.userdata/dev-harness/harness_runs/20260430_191556/` with scenario `bandit.roof_fire_horde_nice_roof_fire_mcw`; closure proof: `doc/roof-fire-horde-nice-roof-fire-proof-v0-2026-04-30.md`.

Run a focused roof-fire horde playtest with a **nice roof fire** and direct horde-response evidence. This promotes the earlier future-only stricter roof-horde attribution caveat into the current active lane.

The playtest should prove more than “a horde existed near a mixed-hostile fixture”. It should show an elevated roof fire, bounded time passage, horde before/after state, attribution to the roof-fire signal, and performance/stability metrics.

## Scope

- Create or reuse an honest elevated roof-fire footing:
  - preferred source: the previous player-created roof-fire proof chain from `.userdata/dev-harness/harness_runs/20260429_172847/` / split proof `.userdata/dev-harness/harness_runs/20260429_180239/`;
  - acceptable alternative: rebuild a new normal player-action roof/elevated fire path with equivalent audits.
- Make the fire “nice” enough to be credible: roof/elevated terrain, brazier/fire metadata, clear target location, and artifact/screenshot/OCR or metadata proof that the fire exists before waiting.
- Place or preserve a horde at a plausible distance so a roof-fire response is possible without reducing the test to adjacent-monster noise.
- Run bounded in-game time passage, preferably a wait long enough to process horde response without turning the test into an unattended soup loop.
- Capture direct horde before/after evidence:
  - position/offset;
  - destination/retarget;
  - `last_processed` or equivalent processed-turn field;
  - movement budget / moves;
  - tracking or interest metadata if the game exposes it;
  - decisive log/artifact lines tying the response to roof-fire light/signal.
- Report metrics:
  - sampled turn count / waited time;
  - wall-clock duration;
  - per-turn or available timing summary;
  - horde-specific timing if instrumented, otherwise explicit `not instrumented`;
  - debug/log spam and crash/stderr status.

## Non-goals

- Do not reopen `CAOL-WRITHING-STALKER-v0` or rerun `performance.mixed_hostile_stalker_horde_mcw` as a substitute.
- Do not claim mixed-hostile horde setup proof as this packet’s direct horde response proof.
- Do not implement a broad horde AI rewrite unless the current roof-fire proof path exposes a concrete missing bridge.
- Do not require natural random horde discovery, multi-day simulation, or full combat/pathfinding proof for v0.
- Do not overclaim positive `tracking_intensity`; if it remains zero or unavailable, state the exact credited response fields instead.

## Success state

- [x] A named scenario/fixture exists for this packet, proposed name: `bandit.roof_fire_horde_nice_roof_fire_mcw`.
- [x] The credited fire is roof/elevated, inspectable before the wait, and tied to the previous player-created roof-fire chain or an equivalent newly proven player-action path.
- [x] A horde is present before the wait at a plausible distance with saved/metadata footing.
- [x] Bounded in-game time passes through the normal wait path or an equally honest harness time-passage path.
- [x] Same-run artifacts show the roof-fire signal path firing for the elevated fire.
- [x] Saved/log artifacts show horde response after the wait: retarget/destination and processed/move-budget evidence at minimum.
- [x] Metrics report cost/stability and labels any unavailable horde-specific timing as `not instrumented` instead of hiding it.
- [x] Canon files (`Plan.md`, `TODO.md`, `SUCCESS.md`, `TESTING.md`, `doc/work-ledger.md`, `andi.handoff.md`) match the final state.

## Evidence boundary

Green closure may be feature-path/harness proof if it reaches the live roof-fire signal and saved horde response path. Screenshot/OCR/metadata is acceptable for the roof-fire footing when image-model inspection is unavailable, but the horde response itself needs saved/log before/after fields, not vibe.

If direct horde response cannot be proven after two materially different attempts, consult Frau before attempt 3. After four total unresolved attempts, preserve the implemented-but-unproven caveat and stop rather than rerunning roof-fire soup.
