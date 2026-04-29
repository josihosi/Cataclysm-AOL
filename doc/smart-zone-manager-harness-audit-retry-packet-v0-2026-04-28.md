# Smart Zone Manager harness-audit retry packet v0 - 2026-04-28

Status: REOPENED 2026-04-29 AS ACTIVE LIVE COORDINATE-LABEL PROBE / PRIOR NON-GREEN BOUNDARY PRESERVED

Imagination source: `doc/smart-zone-manager-harness-audit-retry-imagination-source-of-truth-2026-04-28.md`

## Intent

Josef reopened the Smart Zone Manager live proof only as a better-shaped harness-audit retry. This is not permission to repeat the old blind UI macro. The retry exists because the previous agent-side boundary was honest but unsatisfying: deterministic geometry/separation proof was green, yet the clean live/UI path failed at startup/load (`Dunn has no characters to load`) and did not prove the player-visible generated layout.

The desired proof is clever and token-efficient: prove basically every step with the strongest cheap evidence available, avoiding screenshots/OCR where structured metadata or exact game-state artifacts prove more.

New Josef-discovered proof primitive: the Zone Manager itself shows relative coordinate labels beside zones, e.g. `2E` for two tiles east of the player. Andi can run the real Zone Manager generation path and read/capture those coordinates. If generated zones are lumped onto the same tile, their labels will match; if the layout is separated correctly, the labels should show distinct expected offsets.

Josef's 2026-04-29 reopen sharpened the practical route: set up or load a usable Basecamp zone, open Zone Manager, invoke Smart Zone generation, press **Yes**, then screenshot/OCR the Zone Manager coordinate labels for each generated zone. This is the active proof shape now; the old clean-profile/key-delivery failure is a prior boundary, not a reason to avoid a working UI route.

## Final 2026-04-29 boundary

The bounded retry did not produce live Smart Zone feature proof. Current-runtime rebuild and guarded probes reached clean startup/load on `5f17cc7901-dirty`, but every UI-entry attempt stopped before credited add-zone/filter/generation input because `Zones manager` was not observed.

Decisive final run: `.userdata/smart-zone-ui-entry-current-runtime-20260429c/harness_runs/20260429_005345/`. Startup/runtime hygiene is green (`version_matches_repo_head=true`, `version_matches_runtime_paths=true`), but the action-dispatch trace records `raw_action="action_menu" action_id="action_menu"` after the default `Y` delivery, no `invoke_zone_manager` trace appears, and OCR still shows ordinary gameplay/actions UI rather than `Zones manager`. Earlier guarded attempts `.userdata/smart-zone-audit-live-20260429e/harness_runs/20260429_002148/`, `.userdata/smart-zone-audit-live-20260429f/harness_runs/20260429_004203/`, and `.userdata/smart-zone-ui-entry-current-runtime-20260429b/harness_runs/20260429_005059/` remain the same UI-entry/key-delivery blocker class.

Prior verdict: implemented-but-unproven / Josef playtest package. Deterministic Smart Zone geometry remains support only; no live Zone Manager entry, generation, generated coordinate-label, or saved generated-zone layout proof is credited from those runs. This packet is now explicitly reopened with Josef's simpler UI-coordinate-label route; do not rerun the old blind macro, but do use a materially working UI path.

## Preserved non-green boundary

The latest known retry evidence remains non-green and must not be laundered into closure:

- Scenario/profile: `smart_zone.live_probe_safe_clean` on `smart-zone-audit-live-20260428a`.
- Run: `.userdata/smart-zone-audit-live-20260428a/harness_runs/20260428_151053/`.
- Classification: `feature_proof=false`, `verdict=inconclusive_version_mismatch`, with runtime window `Cataclysm: Dark Days Ahead - 48abd82de9` rather than the current relinked `76605e3d77-dirty` runtime.
- Step ledger: `yellow_step_local_proof_incomplete`, 25/25 scenario rows yellow, 0 green, mostly `runtime_version_mismatch` plus unguarded OCR/checkpoint caveats.
- Saved-zone evidence: `.userdata/smart-zone-audit-live-20260428a/save/Delta/#SGVucmlldHRlIEZseW5u.zoneszmgr-temp.json` contains exactly one `ZONE_START_POINT` row at `[995, 1086, 0]` -> `[997, 1096, 0]`; `.userdata/smart-zone-audit-live-20260428a/save/Delta/zoneszmgr-temp.json` contains `[]`.
- Verdict: this proves neither Smart Zone generation nor separated generated layout metadata. It is at most a wrong-runtime/yellow UI attempt plus a temporary Zone Manager start-point artifact.

Next evidence target: relink/rebuild to the current runtime, run live Zone Manager generation, and capture/read the visible relative coordinate labels beside generated zones. Pair this with exact generated-zone metadata after save/reopen where available, or leave the packet explicitly blocked/non-green. Do not close Smart Zone from load-only screenshots, deterministic `clzones` tests, or the temp `ZONE_START_POINT` row.

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
   - visible Zone Manager coordinate labels beside each generated zone (`2E`-style relative offsets);
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
- the visible Zone Manager list coordinate labels are captured/read for the relevant generated zones;
- exact zone metadata shows the intended zone types/options/coordinates where available;
- zones that must be separated/reserved are not collapsed onto one fake mega-tile; same-coordinate labels are treated as direct lumping evidence and distinct expected labels as direct separation evidence;
- the artifact packet is small and reviewable, with paths plus decisive fields rather than walls of OCR;
- deterministic geometry/separation tests are run only as support for the live claim.

## Yellow/red smells

- The run only proves startup/load.
- The fixture already contains the target zones before the generation action.
- Metadata proves zones exist but not that the live UI action generated/reopened them.
- Screenshots show a menu but not the resulting layout/coordinate labels.
- OCR is too blurry or incomplete to prove the zone-list coordinate labels, and no stronger UI text/widget trace or saved metadata fills the gap.
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
