# Smart Zone Manager harness-audit retry packet v0 - 2026-04-28

Status: GREENLIT / QUEUED BOUNDED HARNESS-AUDIT RETRY

Imagination source: `doc/smart-zone-manager-harness-audit-retry-imagination-source-of-truth-2026-04-28.md`

## Intent

Josef reopened the Smart Zone Manager live proof only as a better-shaped harness-audit retry. This is not permission to repeat the old blind UI macro. The retry exists because the previous agent-side boundary was honest but unsatisfying: deterministic geometry/separation proof was green, yet the clean live/UI path failed at startup/load (`Dunn has no characters to load`) and did not prove the player-visible generated layout.

The desired proof is clever and token-efficient: prove basically every step with the strongest cheap evidence available, avoiding screenshots/OCR where structured metadata or exact game-state artifacts prove more.

## Preserved non-green boundary

The latest known retry evidence remains non-green and must not be laundered into closure:

- Scenario/profile: `smart_zone.live_probe_safe_clean` on `smart-zone-audit-live-20260428a`.
- Run: `.userdata/smart-zone-audit-live-20260428a/harness_runs/20260428_151053/`.
- Classification: `feature_proof=false`, `verdict=inconclusive_version_mismatch`, with runtime window `Cataclysm: Dark Days Ahead - 48abd82de9` rather than the current relinked `76605e3d77-dirty` runtime.
- Step ledger: `yellow_step_local_proof_incomplete`, 25/25 scenario rows yellow, 0 green, mostly `runtime_version_mismatch` plus unguarded OCR/checkpoint caveats.
- Saved-zone evidence: `.userdata/smart-zone-audit-live-20260428a/save/Delta/#SGVucmlldHRlIEZseW5u.zoneszmgr-temp.json` contains exactly one `ZONE_START_POINT` row at `[995, 1086, 0]` -> `[997, 1096, 0]`; `.userdata/smart-zone-audit-live-20260428a/save/Delta/zoneszmgr-temp.json` contains `[]`.
- Verdict: this proves neither Smart Zone generation nor separated generated layout metadata. It is at most a wrong-runtime/yellow UI attempt plus a temporary Zone Manager start-point artifact.

Next evidence target: relink/rebuild to the current runtime and capture exact generated-zone metadata after live Zone Manager generation plus save/reopen, or leave the packet explicitly blocked/non-green. Do not close Smart Zone from screenshots, OCR, deterministic `clzones` tests, or the temp `ZONE_START_POINT` row.

## Product picture

A human player opens Zone Manager, invokes Smart Zone generation, then reopens or inspects the generated layout and sees the intended separated/reserved zones actually exist. The harness proof should demonstrate that same product picture, not merely prove that a fixture contains some zone-like metadata or that the game loaded.

## Scope

1. Repair or replace the loadable clean Smart Zone fixture honestly if the prior `smart-zone-safe-clean-20260427` path still has no loadable character.
2. Use proof-freeze step ledgers for the live path:
   - precondition;
   - exact action;
   - expected state;
   - failure rule;
   - artifact path;
   - green/yellow/red/blocked verdict.
3. Prefer structured proof over screenshots when it is stronger:
   - saved zone metadata after generation and after reopen;
   - exact generated zone types, names, options, and coordinates;
   - reserved/separated tile positions for zones that must not collapse onto one tile;
   - save/writeback or reopened-state evidence where persistence is part of the claim;
   - widget/menu trace proving Zone Manager opened and the generation command was invoked;
   - deterministic geometry/separation checks as support, not closure alone.
4. Use screenshots/OCR sparingly as UI checkpoints only when no stronger artifact exists.
5. Include a Knackal-style imagination review of the proof shape before closure: does the packet prove what a human means by “Smart Zone Manager generated the right layout,” or is it laundering startup/load, deterministic geometry, or metadata-only setup into live closure?

## Non-goals

- No redesign of Smart Zone Manager behavior.
- No contaminated old McWilliams Smart Zone macro as closure proof.
- No blind rerun of `smart-zone-safe-clean-20260427` without a materially repaired loadable fixture/UI primitive.
- No feature closure from load-and-close, deterministic tests alone, or pre-seeded zone metadata that was not produced by the live generation action.
- No giant screenshot/OCR dump when a small structured artifact proves the same fact better.

## Green evidence bar

A green retry must prove all of these in the same shaped packet, or explicitly explain the blocker:

- a current-runtime loadable character reaches gameplay;
- Zone Manager is opened through the live UI path;
- the Smart Zone generation action is invoked through the live UI path;
- generated zones are inspected after generation or after reopen;
- exact zone metadata shows the intended zone types/options/coordinates;
- zones that must be separated/reserved are not collapsed onto one fake mega-tile;
- the artifact packet is small and reviewable, with paths plus decisive fields rather than walls of OCR;
- deterministic geometry/separation tests are run only as support for the live claim.

## Yellow/red smells

- The run only proves startup/load.
- The fixture already contains the target zones before the generation action.
- Metadata proves zones exist but not that the live UI action generated/reopened them.
- Screenshots show a menu but not the resulting layout/coordinates.
- OCR is used where saved zone metadata would be exact.
- Old contaminated McWilliams state is used as closure.
- A non-loadable clean profile is retried without repair.
- Deterministic `clzones` tests are presented as player-visible layout proof.

## Validation gates

Minimum expected gates when implemented:

- `git diff --check`;
- narrow compile/test if harness/runtime code changes;
- relevant deterministic `clzones` geometry/separation tests if Smart Zone code changes;
- one named harness probe/handoff with step ledger and structured zone metadata;
- compact final packet separating screen/UI proof, metadata proof, deterministic support, and verdict.
