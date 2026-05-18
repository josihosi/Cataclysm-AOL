# Smart Zone Manager harness-audit retry packet v0 - 2026-04-28

Status: CLOSED 2026-04-29 / GREEN LIVE COORDINATE-LABEL PROOF AFTER REOPEN / PRIOR NON-GREEN BOUNDARY PRESERVED

Imagination source: `doc/smart-zone-manager-harness-audit-retry-imagination-source-of-truth-2026-04-28.md`

## Intent

Josef reopened the Smart Zone Manager live proof only as a better-shaped harness-audit retry. This is not permission to repeat the old blind UI macro. The retry exists because the previous agent-side boundary was honest but unsatisfying: deterministic geometry/separation proof was green, yet the clean live/UI path failed at startup/load (`Dunn has no characters to load`) and did not prove the player-visible generated layout.

The desired proof is clever and token-efficient: prove basically every step with the strongest cheap evidence available, avoiding screenshots/OCR where structured metadata or exact game-state artifacts prove more.

New Josef-discovered proof primitive: the Zone Manager itself shows relative coordinate labels beside zones, e.g. `2E` for two tiles east of the player. Andi can run the real Zone Manager generation path and read/capture those coordinates. If generated zones are lumped onto the same tile, their labels will match; if the layout is separated correctly, the labels should show distinct expected offsets.

Josef's 2026-04-29 reopen sharpened the practical route: set up or load a usable Basecamp zone, open Zone Manager, invoke Smart Zone generation, press **Yes**, then screenshot/OCR the Zone Manager coordinate labels for each generated zone. That route is now the green proof shape; the old clean-profile/key-delivery failure remains a prior boundary, not closure evidence.

## Final 2026-04-29 green boundary

The reopened coordinate-label route is green agent-side. Final run: `.userdata/smart-zone-audit-live-20260429e/harness_runs/20260429_225644/`, scenario `smart_zone.live_coordinate_label_proof_v0_tmp9` (stabilized as `tools/openclaw_harness/scenarios/smart_zone.live_coordinate_label_proof_v0.json`), command `python3 tools/openclaw_harness/startup_harness.py probe smart_zone.live_coordinate_label_proof_v0_tmp9`.

Classification: `feature_proof=true`, `evidence_class=feature-path`, `verdict=artifacts_matched`, with 18/18 green step-local rows. The run loads gameplay, opens the real `Zones manager` UI through the profile-local binding, opens the add-zone/type picker, selects `Basecamp: Storage`, accepts the default name, places the zone corners, accepts the Smart Zone Manager prompt with **Yes**, closes/saves changes, reopens Zone Manager, and audits generated row traces.

Decisive live evidence:

- UI-entry trace: `component=default_action_dispatch event=invoke_zone_manager raw_action="zones" action_id="zones"`.
- Generation trace: `[harness][smart_zone] prompt accepted` followed by `[harness][smart_zone] result success=1 placed_zones=23`.
- Reopened layout trace: `zone_manager_row` redraw entries include `name`, `type`, `visible_label`, `compact_label`, `start_abs`, `end_abs`, and `center_abs` for generated rows.
- Example generated labels/positions include storage/support area `1 S`, wood/tools/spares around `1 SW` / `1 W` / `1 E`, books/manuals `2 SE`, containers `2 N`, chemicals/drugs `2 E` / `3 E`, food/drink `0 CE` / `1 N`, clothing `3 W`, rotten dump `10 NW`, and unsorted `10 SE`.

Verdict: green for the live coordinate-label claim and not the one-tile lumping bug. Same-label paired rows remain allowed where the generated layout intentionally overlays related zones; the key proof is that the generated layout spans multiple expected relative labels and absolute coordinates rather than collapsing everything onto one tile.

Evidence boundary: screenshots/OCR are archived for UI checkpoints (`open_zones_manager.after.png`, `accept_smart_zone_prompt.after.png`, `reopen_zones_manager_for_layout_inspection.after.png`) but OCR is fallback-quality. The strongest proof is the same-run live Zone Manager row trace. This closes create -> inspect -> close/save-changes -> reopen in the live UI; it does not claim a separate full process-reload disk-persistence audit.

## Preserved non-green boundary

Older pre-green retry evidence remains non-green and must not be laundered into closure:

- Scenario/profile: `smart_zone.live_probe_safe_clean` on `smart-zone-audit-live-20260428a`.
- Run: `.userdata/smart-zone-audit-live-20260428a/harness_runs/20260428_151053/`.
- Classification: `feature_proof=false`, `verdict=inconclusive_version_mismatch`, with runtime window `Cataclysm: Dark Days Ahead - 48abd82de9` rather than the current relinked `76605e3d77-dirty` runtime.
- Step ledger: `yellow_step_local_proof_incomplete`, 25/25 scenario rows yellow, 0 green, mostly `runtime_version_mismatch` plus unguarded OCR/checkpoint caveats.
- Saved-zone evidence: `.userdata/smart-zone-audit-live-20260428a/save/Delta/#SGVucmlldHRlIEZseW5u.zoneszmgr-temp.json` contains exactly one `ZONE_START_POINT` row at `[995, 1086, 0]` -> `[997, 1096, 0]`; `.userdata/smart-zone-audit-live-20260428a/save/Delta/zoneszmgr-temp.json` contains `[]`.
- Verdict: this proves neither Smart Zone generation nor separated generated layout metadata. It is at most a wrong-runtime/yellow UI attempt plus a temporary Zone Manager start-point artifact.

Do not close Smart Zone from load-only screenshots, deterministic `clzones` tests, or the temp `ZONE_START_POINT` row. The green closure is the later live coordinate-label proof above.

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
