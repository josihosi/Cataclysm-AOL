# Plan

Canonical roadmap for Cataclysm-AOL.

This file answers one question: **what should the agent advance next?**
It is not a changelog, not a graveyard of crossed-off substeps, and not a place to preserve every historical detour.
Rewrite it as reality changes.

## File roles

- **Plan.md** - canonical roadmap and current delivery target
- **SUCCESS.md** - success-state ledger / crossed-off exit criteria for roadmap items
- **TODO.md** - short execution queue for the current target only
- **TESTING.md** - current validation policy, latest relevant evidence, and pending probes
- **TechnicalTome.md** - durable mechanic notes, not daily state tracking
- **COMMIT_POLICY.md** - checkpoint rules to prevent repo soup

If these files disagree, **Plan.md wins** and the other files should be repaired.

## Working rules for agents

- Do **not** mechanically grab the first unchecked-looking thing from some list.
- Follow the current delivery target below and move it to its **next real state**.
- Josef being unavailable for playtesting is **not** a blocker by itself.
- When a target is waiting on Josef, move to the next best unblocked target.
- If no good unblocked target remains, send Josef a short parked-options note so he can greenlight the next lane; do not just keep revalidating the old packet.
- During the current debug-proof finish stack, failed agent-side proof does **not** close or park implemented code. After the attempt budget, move implemented-but-unproven items to Josef's playtest package and continue the next greenlit debug note.
- Prefer batching human-only asks where practical. One useful packet with two real product questions beats two tiny pings.
- Keep these files lean. Remove finished fluff from `TODO.md` and `TESTING.md` instead of piling up crossed-off archaeology.
- Each real roadmap item needs an explicit success state in `SUCCESS.md` (or an equally explicit inline auxiliary) so completion is visible instead of guessed.
- Cross off reached success-state items; only remove the whole roadmap item from `Plan.md` once its success state is fully crossed off / done.
- Prefer agent-side playtesting first. Josef should be used for product judgment, feel, priority calls, or genuinely human-only checks.
- Validation should match risk:
  - docs-only change -> no compile
  - small local code change -> narrow compile/test
  - broad or risky code change, or a Josef handoff -> broader rebuild / startup harness as needed
- Follow `COMMIT_POLICY.md`. Do not let the repo turn into one giant dirty blob.

---

## Current status

Repo policy remains unchanged: `/Users/josefhorvath/Schanigarten/Cataclysm-AOL` on `dev` is the normal worktree and `josihosi/Cataclysm-AOL` is the real project/release repo. `josihosi/C-AOL-mirror` is green-dot-only.

Detailed contracts, closure evidence, and older checkpoint history belong in `doc/*.md`, `SUCCESS.md`, and git history. Keep this file short enough that the active stack is visible without archaeology.

---

## Closed lane - Basecamp locker armor ranking + blocker removal packet v0

**Status:** CLOSED / CHECKPOINTED

Generic protective/full-body armor ranking and blocker clearing are landed for the camp locker path without special-casing `RM13 combat armor`. Deterministic proof covers superior full-body candidates displacing worse blockers, damaged candidates being rejected without repeated requeue/equip churn, stronger ballistic armor being preserved while a compatible full-body suit is added, and `[camp][locker]` readiness regression coverage.

Canonical contract lives at `doc/basecamp-locker-armor-ranking-blocker-removal-packet-v0-2026-04-26.md`.

---

## Closed lane - Basecamp job spam debounce + locker/patrol exceptions packet v0

**Status:** CLOSED / CHECKPOINTED

Stable-cause camp job chatter debounce landed for camp activity completion, camp request blocked/missing-tool barks, and no-progress request barks. Locker and patrol paths now use typed `[camp][locker]` / `[camp][patrol]` reports with repeated-state compression, preserving first occurrence and changed state visibility without a global message-log rewrite.

Canonical contract lives at `doc/basecamp-job-spam-debounce-exceptions-packet-v0-2026-04-26.md`.

---

## Completed lane - C-AOL debug-proof finish stack v0

**Status:** CLOSED / CHECKPOINTED

Josef explicitly reopened the current C-AOL debug-proof notes on 2026-04-27. The stack has now reached an honest agent-side boundary without leaving failed proof in parked/review-only posture.

- Bandit live signal/site-bootstrap product-proof work reached its honest agent-side boundary: synthetic loaded-map fire/light reaches the running live path and the real player-lit fire bridge is listed in Josef's playtest package as implemented-but-unproven; do not loop on it again without a fresh reopen/material blocker change.
- Smart Zone Manager live layout separation correction reached its honest agent-side boundary: the planner now keeps intended-separate generated zones on separate reserved tiles with deterministic geometry/separation coverage, the failed clean live/UI macro is packaged for Josef as implemented-but-unproven, and the 2026-04-28 current-runtime rerun only added startup/load red evidence (`Dunn has no characters to load`) rather than a fresh product-proof reopen.
- Bandit local standoff / scout return live correction is now fixed/proven on the current live product path: deterministic coverage asserts the pre-local-contact scout timeout, rebuilt current-runtime run `.userdata/dev-harness/harness_runs/20260427_154309/` used the real `wait_action` path, logged `local_gate ... posture=hold_off ... standoff_distance=5 ... live_dispatch_goal=140,46,0`, then logged `scout_sortie: linger limit reached -> return_home`, `scout_sortie: home footprint observed ... pos=(140,51,0)`, and `scout_report: returned -> pressure refreshed`.
- Attempt rule for this stack was observed: attempt 3 happened only after Frau Knackal consultation and a material code-path/instrumentation change.
- **GitHub normal-download release packet v0** should stay held until Schani/Josef decide whether the greenlit harness trust audit/proof-freeze package or release posture is next.

Canonical contract lives at `doc/caol-debug-proof-finish-stack-v0-2026-04-27.md`.

---

## Active lane - C-AOL harness trust audit + proof-freeze v0

**Status:** ACTIVE / GREENLIT PROCESS AUDIT

Josef requested this on 2026-04-27 after watching harness runs load the game, close it, and still risk being treated as feature proof. This is a large audit of the C-AOL harness skill/procedures, preferably Andi execution with Frau Knackal review: inventory every harness primitive, prove every keystroke/setup step with screenshots or exact game metadata, enforce same-save/provenance discipline where possible, and freeze the workflow so false passes become red states instead of polite nonsense.

Josef has now explicitly promoted actual product playtests under this proof standard: load-and-close is startup/load proof only, and harness-based feature proof must show step-by-step evidence for the actual feature path.

Canonical contract lives at `doc/c-aol-harness-trust-audit-and-proof-freeze-packet-v0-2026-04-27.md`.

Current checkpoint:

- Harness surface inventory and provisional same-save policy live at `doc/c-aol-harness-trust-audit-inventory-v0-2026-04-27.md`.
- Compact proof-freeze matrix lives at `doc/c-aol-harness-proof-freeze-matrix-v0-2026-04-28.md`; Frau reviewed the draft at process altitude and the matrix now includes her tighter locks for claim-scoped artifacts, metadata-only limits, deferred guards, anti-fixture bias, green debug-spawn item/monster/follower-NPC/weather plus debug map-editor field/furniture and terrain/trap/radiation target-state proof patterns, scoped green talker-selector plus follower-rules dialogue recipes, and a scoped follower-rule toggle/writeback metadata proof.
- `startup_harness.py start` now emits a startup step ledger and explicit proof classification: load/readiness remains `startup/load`, `feature_proof=false`.
- Probe classification requires a clean startup gate before `artifacts_matched` can become feature proof, so stale runtime/profile/load-readiness problems cannot be hidden by later log matches.
- `audit_saved_map_tile_near_player` reports explicit empty target-tile metadata for requested offsets and now audits required terrain/fields/items/furniture/traps/radiation, including the flat saved-radiation strength/count RLE; `audit_saved_player_items` now distinguishes live-selector-accessible carried/worn/contained items from legacy top-level `player.inv` rows before GUI macros run; `audit_saved_active_monsters` reads same-save `active_monsters` type/location metadata for debug-spawn monster proofs; `audit_saved_overmap_npcs` reads same-save overmap `npcs` metadata and can require baseline-to-post-save NPC count/new-NPC deltas plus exact saved follower-rule values for debug-spawn follower/NPC and rule-toggle proofs; `audit_saved_weather_state` reads same-save `dimension_data.gsav` weather metadata including forced/effective temperature for weather proofs; `abort_on_metadata_failure` prevents later steps from being credited after missing required terrain/fields/items/furniture/traps/radiation/monsters/NPCs/weather.

Current false-pass evidence:

- `.userdata/dev-harness/harness_runs/20260427_175051/` proves source-backed `t` opens `Talk to whom`, then blocks/red-classifies the alpha talker-selection step.
- `.userdata/dev-harness/harness_runs/20260427_184319/` keeps the real-fire chain red because the saved target tile lacks `fd_fire` while the preceding GUI steps remain yellow/untrusted.
- `.userdata/dev-harness/harness_runs/20260427_191725/` stops at the first deploy gate because the saved tile east of the player lacks `f_brazier`.
- `.userdata/dev-harness/harness_runs/20260427_200100/` proves exact fixture inventory first (`brazier=1`, `2x4=20`, `lighter=1`), then still stops red at the missing east-tile `f_brazier` deploy state.
- `.userdata/dev-harness/harness_runs/20260427_200919/` tried checked GUI text for Josef's GUI-as-text idea and aborted at `open_apply_inventory_for_brazier_text_guard` because source-backed `Use item` menu text was not proven.
- `.userdata/dev-harness/harness_runs/20260427_202434/` uses harness-gated inventory/direction trace plus richer saved-item metadata. It proves `Use item` opens and the saved brazier definition is `deploy_furn -> f_brazier`, but after filtering `brazier` the selector still does **not** prove a highlighted `brazier` row (`highlight_after_redraw selected_item=no`). The abort fires before `CONFIRM`, so `Deploy where?`, `RIGHT`, save, and east-tile `f_brazier` remain unproven.
- `.userdata/dev-harness/harness_runs/20260427_203847/` plus a direct updated-audit check sharpened the gap: the exact fire items existed only in legacy top-level `player.inv` (`legacy_top_level_inv_counts={"2x4":20,"brazier":1,"lighter":1}`), while live-selector-accessible carried/worn contents contained no `brazier`, `2x4`, or `lighter`; this matched the live `Use item` selector showing only `smart_phone` before filtering and zero visible entries after filter `brazier`.
- `.userdata/dev-harness/harness_runs/20260427_214207/` first proved the guarded deploy gate but still had non-decisive yellow OCR/baseline/confirmation rows. Follow-up run `.userdata/dev-harness/harness_runs/20260427_222635/` cleans those step-local mechanics: 16/16 green, `step_ledger_summary.status=green_step_local_proof`, `evidence_class=feature-path`, `feature_proof=true` for the scoped normal player-action deploy primitive only. It proves selected `brazier`, `Deploy where?`, right/east direction consumption, case-sensitive save prompt before uppercase `Y`, saved-player mtime advance from `1777321610298746508` to `1777321628903849060`, and saved east tile `[3368,994,0]` with `furniture=["f_brazier"]`.
- `.userdata/dev-harness/harness_runs/20260428_011205/` is the first green debug-spawn item target-state primitive proof. The scenario `harness.debug_spawn_item_inventory_target_state_audit_mcw` rebuilds/runs against current runtime `32c36dd9bc`, spawns `electric toothbrush` through the debug item path, proves the case-sensitive save/writeback gate, and then reads same-run saved-player metadata with `live_accessible_counts.toothbrush_electric=1`; all six step-ledger rows are green. `.userdata/dev-harness/harness_runs/20260428_021800/` is the first green debug-spawn monster target-state primitive proof: `harness.debug_spawn_monster_target_state_audit_mcw` spawns `mon_squirrel` one tile east, proves the same save/writeback gate, and reads saved-player `active_monsters` with `mon_squirrel` at player-relative offset `[1,0,0]`; all six step-ledger rows are green. `.userdata/dev-harness/harness_runs/20260428_032724/` is the tightened green debug-spawn follower NPC target-state primitive proof: `harness.debug_spawn_follower_npc_target_state_audit_mcw` records a saved-overmap NPC baseline, spawns one random follower, proves the same save/writeback gate, and reads saved overmap `npcs` with count delta `+1` plus a new nearby `your_followers` NPC at attitude `3`; all seven step-ledger rows are green. `.userdata/dev-harness/harness_runs/20260428_024606/` is the first green debug weather/temperature target-state primitive proof: `harness.debug_force_temperature_target_state_audit_mcw` forces 123F, proves the same save/writeback gate, and reads saved dimension weather metadata with `forced_temperature=123.000031` and effective `123.000031F`; all six step-ledger rows are green. `.userdata/dev-harness/harness_runs/20260428_034421/` is the first green debug map-editor field/furniture target-state primitive proof: `harness.debug_map_editor_field_furniture_target_state_audit_mcw` paints `f_chair` and `fd_smoke` one tile east via the debug map editor, proves the same save/writeback gate, and reads same-run saved-map metadata with `observed_furniture=["f_chair"]` and `observed_field_ids=["fd_smoke"]` at offset `[1,0,0]`; all seven step-ledger rows are green. `.userdata/dev-harness/harness_runs/20260428_041845/` is the first green debug map-editor terrain/trap/radiation target-state primitive proof: `harness.debug_map_editor_terrain_trap_radiation_target_state_audit_mcw` paints `t_grass_dead`, `tr_bubblewrap`, and radiation `37` one tile east via the debug map editor, proves the same save/writeback gate, and reads same-run saved-map metadata with `observed_terrain=["t_grass_dead"]`, `observed_traps=["tr_bubblewrap"]`, and `observed_radiation=[37]` at offset `[1,0,0]`; all eight step-ledger rows are green. These are scoped harness primitive/process proof only (`feature_proof=false`, metadata/no claim-scoped product artifact match), not product-feature closure.
- `.userdata/dev-harness/harness_runs/20260428_014348/` is the first green scoped talker-selector primitive proof. The focused scenario `basecamp.talker_selector_metadata_probe_mcw` runs on current runtime `ef22838b71`, proves `C` opens `What do you want to do`, `t` opens `Talk to whom`, and `enter` confirms the highlighted talker into Katharina Leach's dialogue response surface (`Your response`) with 4/4 green step-local rows. This is scoped navigation/process proof only (`feature_proof=false`, no claim-scoped artifact match).
- `.userdata/dev-harness/harness_runs/20260428_014825/` is the first green scoped follower-rules dialogue proof. Updated `basecamp.package2_dialog_rules_probe_mcw` now uses the proven talker-selector recipe, then requires the visible `[rules] ... work together` response list before pressing the source-backed first dialogue alpha hotkey again; all 6 step-local rows are green and the final screenshot/OCR proves `Rules for your follower`. Top-level classification remains process/navigation evidence (`feature_proof=false`, no claim-scoped artifact match), not a product-feature closure.
- `.userdata/dev-harness/harness_runs/20260428_044855/` is the first green scoped follower-rule toggle/writeback proof. New `basecamp.follower_rules_toggle_probe_mcw` starts from saved-overmap `Katharina Leach` follower metadata with `rule_follow_distance_2=false`, proves the current McWilliams selector/dialogue/rules UI path, presses the visible rules-UI hotkey `b`, OCR-proves the row changed from about four paces to about two paces, exits back to gameplay, proves the case-sensitive save prompt before uppercase `Y`, advances saved-player mtime, and reads same-run saved-overmap NPC metadata with `rule_follow_distance_2=true` plus `override_enable_follow_distance_2=true` / `override_follow_distance_2=true`; all 14 step-local rows are green. Top-level classification remains process/metadata evidence (`feature_proof=false`, no claim-scoped product artifact match), not downstream package behavior closure.
- Fuel continuation remains non-green after bounded post-deploy retries: `.userdata/dev-harness/harness_runs/20260427_215006/` proves the post-deploy saved tile still has `f_brazier` but live-accessible inventory is missing `2x4`; exact-items reruns `.userdata/dev-harness/harness_runs/20260427_215154/`, `.userdata/dev-harness/harness_runs/20260427_215445/`, and `.userdata/dev-harness/harness_runs/20260427_215757/` prove live-accessible `brazier`/`2x4`/`lighter` before the chain, but stop before fuel proof because the post-fuel save/writeback gate never advances saved-player mtime. Follow-up artifact inspection of `20260427_215757/request_save_after_fuel_drop.after.screen_text.json` shows the post-fuel save request was not a proven `Save and quit?` prompt. Changed actual-playtest attempt `.userdata/dev-harness/harness_runs/20260427_224113/` uses source-backed multidrop `MARK_WITH_COUNT` and still aborts at `blocked_untrusted_post_fuel_save_prompt`. Post-drop guard runs `.userdata/dev-harness/harness_runs/20260427_225552/`, `20260427_225730/`, and `20260427_225909/` add/use a harness UI-trace gate before any save request and stop before exact-20 Multidrop return proof. Instrumented diagnostic run `.userdata/dev-harness/harness_runs/20260427_231210/` adds Multidrop entry/location tracing and shows the sharper boundary: after filter `plank`, the redraw has `visible_item_count=0` in every column, `CONFIRM` blocks with `selected_stacks=0 total_selected_qty=0`, and no save/mtime/current-tile `2x4`/lighter/`fd_fire` credit is allowed. Follow-up row-specific diagnostic `.userdata/dev-harness/harness_runs/20260427_232220/` stops earlier and more honestly at `blocked_untrusted_drop_filter_or_inventory_visibility` because the filtered Multidrop trace has no `typeid="2x4"` row for fuel selection. No fuel/lighter/`fd_fire` feature proof is credited from these runs.

Current boundary: the normal player-action brazier deploy gate is green only as a scoped deploy primitive; debug-spawn item, monster, follower-NPC, weather/temperature, and debug map-editor field/furniture plus terrain/trap/radiation target-state proofs are green as scoped harness primitive/process evidence (`20260428_011205`, saved-player `toothbrush_electric=1`; `20260428_021800`, saved-player `active_monsters` has `mon_squirrel` at offset `[1,0,0]`; `20260428_032724`, saved overmap `npcs` has count delta `+1` and a new nearby `your_followers` NPC at attitude `3`; `20260428_024606`, saved dimension weather effective/forced temperature `123F`; `20260428_034421`, saved-map offset `[1,0,0]` has `f_chair` plus `fd_smoke`; `20260428_041845`, saved-map offset `[1,0,0]` has `t_grass_dead`, `tr_bubblewrap`, and radiation `37`); the focused talker-selector recipe is green as scoped navigation/process evidence (`20260428_014348`, `C` -> `t` -> highlighted-row `enter` -> `Your response`); the follower-rules response/menu recipe is green as scoped dialogue navigation/process evidence (`20260428_014825`, visible `[rules] ... work together` -> `Rules for your follower`); and the follower-rule toggle/writeback recipe is green as scoped UI+metadata/process evidence (`20260428_044855`, `rule_follow_distance_2=false` -> visible two-pace toggle -> guarded save/writeback -> saved-overmap `rule_follow_distance_2=true`). The player-action fuel continuation reached an honest non-green boundary at `blocked_untrusted_drop_filter_or_inventory_visibility`: filtered Multidrop has no visible/selectable `2x4`/plank row, so count selection, confirm-return, save/writeback, lighter, and `fd_fire` proof remain uncredited. Smart Zone Manager live layout verification is also implemented-but-unproven/Josef-package now: after the earlier four-attempt budget, the current-runtime rerun `.userdata/smart-zone-safe-clean-20260427/harness_runs/20260428_001347/` only proved the named clean profile cannot load a character (`Dunn has no characters to load`), so no Smart Zone feature steps ran and no layout proof is credited. The next unblocked active work is another explicitly named harness proof-freeze primitive or Schani/Josef seam; rule-toggle proof is now frozen, but downstream package behavior remains a separate burden.

Known source/control footing: `brazier` uses `deploy_furn` -> `f_brazier`; activation should enter `Deploy where?`; `right` is valid only in that direction prompt. Source inspection confirms `game_menus::inv::use()` walks wielded/worn/contained/nearby selector sources rather than legacy top-level `player.inv`; `query_yn` with `FORCE_CAPITAL_YN` requires uppercase `Y` for the case-sensitive save prompt.

Active sub-order:

1. Preserve run `20260427_222635` as the all-green scoped deploy primitive gate evidence; do not inflate it into bandit real-fire/product proof.
2. Keep the fuel-continuation scenario (`bandit.live_world_nearby_camp_real_fire_exact_items_fuel_tile_audit_mcw`) as a red audit/probe artifact for `blocked_untrusted_drop_filter_or_inventory_visibility` before any count/confirm/post-fuel save/writeback credit, not as closure proof.
3. Preserve Smart Zone Manager live layout verification as implemented-but-unproven in Josef's playtest package. Do not attempt another clean live/UI macro unless Schani/Josef explicitly reopen it after a materially repaired loadable-profile/UI primitive; current-runtime run `20260428_001347` is startup/load red only (`blocked_clean_profile_no_loadable_character`).
4. Preserve debug-spawn item target-state proof `20260428_011205`, debug-spawn monster target-state proof `20260428_021800`, debug-spawn follower-NPC target-state proof `20260428_032724`, debug force-temperature target-state proof `20260428_024606`, debug map-editor field/furniture target-state proof `20260428_034421`, and debug map-editor terrain/trap/radiation target-state proof `20260428_041845` as green primitive patterns: they prove only debug item/monster/follower-NPC/weather/map-editor helper -> save/writeback -> same-run saved-player, saved-overmap, saved-dimension, or saved-map metadata, not product feature behavior.
5. Preserve talker-selector proof `20260428_014348` as the scoped current McWilliams selector recipe: it proves only nearby interaction menu -> Talk to whom -> highlighted talker Enter -> NPC dialogue response surface.
6. Preserve follower-rules proof `20260428_014825` as the scoped current McWilliams response/menu recipe: it proves the visible rules/work-together response list and resulting `Rules for your follower` UI. Preserve follower-rule toggle/writeback proof `20260428_044855` as the scoped current McWilliams rules-UI metadata recipe: it proves `rule_follow_distance_2=false` -> visible two-pace toggle with hotkey `b` -> guarded save/writeback -> saved-overmap `rule_follow_distance_2=true`, but still not downstream package product behavior. The next named audit primitive is another Schani/Josef-named harness seam. Do not continue to lighter/final `fd_fire` until a post-fuel save/writeback gate advances mtime and saved-map metadata proves current-tile `f_brazier` plus `2x4` in the same run.


---

## Greenlit lane - C-AOL actual playtest verification stack v0

**Status:** GREENLIT / ACTUAL PLAYTEST STACK

Josef explicitly greenlit actual playtests now, using the same step-by-step metadata/screenshot validation that the harness trust audit is establishing. This is not a release lane and not a license to wander around a save; it is a small ordered verification stack for live product claims that deterministic tests or load-only harness runs could not honestly close.

Canonical contract lives at `doc/c-aol-actual-playtest-verification-stack-v0-2026-04-27.md`.

Greenlit order:

1. **Fuel continuation behind the green brazier deploy gate — honest boundary reached.** Keep this inside the active harness audit as red/non-green evidence: `blocked_untrusted_drop_filter_or_inventory_visibility` means no count selection, confirm-return, save/writeback, lighter, or `fd_fire` proof is credited.
2. **Smart Zone Manager live layout verification packet v0 — honest boundary reached.** Keep this implemented-but-unproven in Josef's playtest package. Deterministic geometry proof remains support only; the bounded live/UI budget failed to capture generated layout, and current-runtime run `20260428_001347` is startup/load red because the clean profile has no loadable character.
3. **Player-lit fire and bandit signal verification packet v0 — still blocked behind fuel.** Only after fuel placement/writeback is green, prove the real lighter/action path, actual `fd_fire`/smoke state, bounded wait/time passage, and bandit signal response. Do not jump here while the fuel gate is red.
4. **Roof-fire horde detection proof packet v0 — greenlit but blocked behind real player-lit fire.** Josef/Schani greenlit this later item: debug may stage the horde/distance, but not the fire. Only after real player-lit fire is green, prove roof/elevated player position, real player-created roof fire/light/smoke, bounded time passage, and horde before/after detection or response metadata; a proven no-detect/no-response outcome is acceptable.

Non-goals: no Lacapult, no release publishing, no broad debug-note reopening, no further Smart Zone live-probe loop without explicit fresh reopen/material primitive repair, no contaminated old McWilliams Smart Zone macro as closure proof, and no feature closure from load-and-close or deterministic tests alone.

---

## Recently closed correction checkpoints

**Status:** CHECKPOINTED / CLOSED

Do not reopen these by drift:
- **Bandit live-wiring audit + visible-light horde bridge correction v0** — loaded-map visible fire/light -> horde signal bridge proof, not player-lit fire proof.
- **Bandit local sight-avoid + scout return cadence packet v0** — reclosed for the 2026-04-27 product gap: current-runtime live proof now covers five-OMT hold-off standoff plus scout timeout, home-footprint observation, and returned pressure-refresh writeback on the McWilliams/Basecamp product path.
- **Smart Zone Manager v1 Josef playtest corrections** — implemented-but-unproven live: deterministic geometry/separation proof is green and the clean live/UI macro failure is in Josef's playtest package; do not loop on it again without a fresh reopen or materially repaired harness primitive.
- **Basecamp medical consumable readiness v0** — deterministic camp/locker proof for bounded bandage-family readiness, including carried-stock cap behavior.
- **Basecamp locker armor ranking + blocker removal packet v0** — generic protective/full-body armor comparison and blocker clearing proof; no RM13 special case.

---

## Checkpointed packet index

**Status:** CHECKPOINTED / INDEX

Use the auxiliary docs below when a later discussion needs the canonical contract or the honest closed verdict, not as permission to reopen the lane automatically.

### Camp / Basecamp packets

- `doc/bandit-extortion-playthrough-audit-harness-skill-packet-v0-2026-04-22.md` (checkpointed / done for now)
- `doc/bandit-extortion-at-camp-restage-handoff-packet-v0-2026-04-22.md` (checkpointed / done for now)
- `doc/bandit-aftermath-renegotiation-writeback-packet-v0-2026-04-22.md` (checkpointed / done for now)
- `doc/bandit-shakedown-pay-or-fight-surface-packet-v0-2026-04-22.md` (checkpointed / done for now)
- `doc/bandit-approach-stand-off-attack-gate-packet-v0-2026-04-22.md` (checkpointed / done for now)
- `doc/bandit-live-world-control-playtest-restage-packet-v0-2026-04-22.md` (checkpointed / done for now)
- `doc/bandit-basecamp-playtest-kit-packet-v1-2026-04-22.md` (checkpointed / done for now)
- `doc/bandit-basecamp-playtest-kit-packet-v2-2026-04-22.md` (folded into later active lane / supporting only)
- `doc/bandit-basecamp-playtest-kit-packet-v0-2026-04-22.md`
- `doc/bandit-basecamp-first-pass-encounter-readability-packet-v0-2026-04-22.md`
- `doc/live-bandit-basecamp-playtest-packaging-helper-packet-v0-2026-04-22.md`
- `doc/locker-zone-v3-reopen-packet-v0-2026-04-21.md`
- `doc/basecamp-ai-capability-audit-readout-packet-v0-2026-04-21.md`
- `doc/live-bandit-basecamp-playtesting-feasibility-probe-v0-2026-04-21.md`

### Bandit readiness and follow-through packets

- `doc/bandit-overmap-local-handoff-interaction-packet-v0-2026-04-21.md`
- `doc/bandit-elevated-light-and-z-level-visibility-packet-v0-2026-04-21.md`
- `doc/bandit-overmap-benchmark-suite-packet-v0-2026-04-21.md`
- `doc/bandit-weather-concealment-refinement-packet-v0-2026-04-21.md`
- `doc/bandit-overmap-local-pressure-rewrite-packet-v0-2026-04-21.md`
- `doc/bandit-long-range-directional-light-proof-packet-v0-2026-04-21.md`
- `doc/bandit-bounded-scout-explore-seam-v0-2026-04-21.md`
- `doc/bandit-repeated-site-revisit-behavior-packet-v0-2026-04-21.md`
- `doc/bandit-scoring-refinement-seam-v0-2026-04-21.md`
- `doc/bandit-moving-bounty-memory-seam-v0-2026-04-21.md`

### Earlier bandit substrate docs still worth keeping straight

- `doc/bandit-concept-formalization-followthrough-2026-04-19.md`
- `doc/bandit-player-basecamp-visibility-and-concealment-2026-04-19.md`
- `doc/bandit-overmap-ai-concept-2026-04-19.md`

---

## Parked concept chain - Bandit overmap AI

**Status:** PARKED / COHERENT SUBSTRATE

This larger concept stays parked as a planning substrate.
Several bounded `v0` slices were promoted, implemented, and checkpointed, but that does **not** silently greenlight the remaining broader concept work.

Current parked-chain anchor:
- `doc/bandit-overmap-ai-concept-2026-04-19.md`

Still-parked concept docs:
- `doc/bandit-mark-generation-and-heatmap-model-2026-04-19.md`
- `doc/bandit-overmap-to-bubble-handoff-seam-2026-04-19.md`
- `doc/bandit-player-basecamp-visibility-and-concealment-2026-04-19.md`

Promoted slices that are now checkpointed closed live in the packet index above.
If this chain is revisited later, the next discussion should be about one new bounded promotion or one real contradiction in current canon, not about spawning another fog bank of feeder docs.

---

## Future feature lanes - parked far back

**Status:** PARKED / FAR BACK

These lanes are **not part of the current camp-handling or bandit queue**.
Keep them visibly separate so adjacent tooling or observability work does not get mistaken for partial feature delivery.

- Chat interface over in-game dialogue branches
- Tiny ambient-trigger NPC model
- Arsenic-and-Old-Lace social threat and agentic-world concept bank
  - anchor doc: `doc/arsenic-old-lace-social-threat-and-agentic-world-concept-bank-2026-04-22.md`
  - preserve this as a far-back parked concept bank, not a disguised implementation queue
  - the strongest near-promotable seeds currently include alarm states via natural-language yelling, bandits exploiting readable camp routines, radio information warfare, writhing-stalker pressure, and social camouflage / hidden-psychopath survivor play
  - broader far-out families worth keeping include agentic NPC goals, interdimensional-traveler motives, conspiracy pressure, and other weird-world social-horror systems
  - do **not** let this bank outrun honest playtesting of the current bandit and Basecamp zoning footing; unpack one bounded item at a time only after the present substrate is reviewer-clean enough to deserve more load

Do not reopen them by drift.
They stay buried until Josef explicitly promotes them.

---

## Documentation discipline

If the structure starts bloating again, apply this rule:
- `Plan.md` should be readable in a minute
- `TODO.md` should show only the current execution queue
- `TESTING.md` should show only current policy, latest relevant evidence, and pending probes
- `COMMIT_POLICY.md` should stop the dirty tree from becoming a lifestyle

If a sentence exists only to remember that something used to be true, it probably belongs in git history, not here.
