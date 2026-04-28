# TESTING

Current validation policy and current evidence only.

This file is not a trophy wall.
Remove stale or completed fluff instead of stacking crossed-off test history forever.

## Validation policy

Use the **smallest evidence that honestly matches the change**.

- **Docs-only change** -> no compile
- **Small local code change** -> narrow build and the narrowest relevant test target
- **Broad or risky code change** -> broader rebuild and relevant filtered tests
- **Before a Josef handoff** or after suspicious stale-binary behavior -> rebuild the real targets and rerun the real smoke path

### Agent vs Josef

Schani should do agent-side playtesting first whenever the harness or direct in-game probing can answer the question.
Josef should be asked only for:
- product judgment
- tone/feel
- human-priority choices
- genuinely human-only interaction

Josef being unavailable is **not** a blocker by itself.
Josef self-testing is **not** a plan blocker and does not belong in the active success state as a gate.
If Josef-specific checks are useful, write them down as non-blocking notes so he can run them later from his own list.
If another good agent-side lane exists, keep moving there.
If several human-only judgments are likely soon, batch them instead of sending tiny separate asks.

### Anti-churn rule

Do not keep rerunning the same startup or test packet when it is no longer the missing evidence class.
If startup/load is already green, and the missing proof is live behavior, then the next probe must target live behavior.
If a target is merely waiting on Josef, do not keep revalidating it unless the code changed.

Current debug-stack attempt rule for the same item/blocker:
- attempts 1-2 may be Andi solo retries, including multiple focused harness attempts in one cron run when each attempt changes setup, instrumentation, or evidence class
- before attempt 3, consult Frau Knackal
- attempts 3-4 are the final changed retries after consultation
- after attempt 4, if code is implemented but proof still fails, add a concise implemented-but-unproven packet to Josef's playtest package and move to the next greenlit debug note; do not close it and do not park it as dead

### Test-to-game wiring rule

A test is not allowed to impersonate implementation. Before claiming gameplay behavior, identify the live code path that consumes the tested seam and name the evidence class that proves it: unit/evaluator, playback/proof packet, live source hook, harness/startup, screen, save inspection, or artifact/log. Deterministic-only packets may close only as deterministic-only packets; if the contract says the game does something, the proof must reach the game path or the claim stays open.

### Bandit overmap-proof rule

For the remaining bandit AI proof packets, single-turn deterministic checks are **not** enough by themselves.
The honest bar now includes real overmap-side multi-turn scenario proof, up to `500` turns where needed, with explicit per-scenario goals and tuning metrics.

## Current validation targets

### Recently completed validation target - C-AOL debug-proof finish stack

`C-AOL debug-proof finish stack v0` is complete and ready for Schani review.

Smart Zone Manager live layout separation correction is **implemented-but-unproven live** at its honest agent-side boundary: deterministic geometry/separation assertions and the explicit overlap allowlist are green, but the clean GUI macro still could not honestly inspect the generated player-visible layout. The manual close recipe is packaged as `smart-zone-live-layout-separation-correction` in `/Users/josefhorvath/.openclaw/workspace/runtime/josef-playtest-package.md`; do not rerun the contaminated old McWilliams macro as closure proof.

Bandit local standoff / scout return live correction is reclosed on current runtime. Evidence:
- deterministic standoff-distance correction is green (`choose_hold_off_standoff_goal()` minimum five-OMT goal);
- copied-save inspection answered the stale fixture state: the inspected McWilliams site had `active_sortie_started_minutes=-1` / `active_sortie_local_contact_minutes=-1` and no active group before the live proof setup;
- live standoff proof `.userdata/dev-harness/harness_runs/20260427_152117/` used the real `|` wait action, handled the alarm/watch pre-menu with explicit `w`, chose bounded 30m (`4`), captured before/menu/after evidence, and logged `local_gate ... posture=hold_off ... standoff_distance=5 ... live_dispatch_goal=140,46,0`;
- final current-runtime proof `.userdata/dev-harness/harness_runs/20260427_154309/` used the same real `wait_action` path and required artifacts, then logged `bandit_live_world scout_sortie: linger limit reached -> return_home`, `bandit_live_world scout_sortie: home footprint observed ... pos=(140,51,0)`, and `bandit_live_world scout_report: returned -> pressure refreshed`.

Attempt 3 for this item happened only after Frau Knackal consultation and a material code-path/instrumentation change, satisfying the repeated-test escalation rule.

Retained evidence classification for `Bandit live signal + site bootstrap correction v0`:
- raw saved `fd_fire` / `fd_smoke` fixtures prove map-field reader / consumer behavior only
- synthetic smoke proof proves only synthetic smoke-source/live-signal behavior and must be labeled as such
- if Josef reopens player-fire product proof, it still requires the real brazier/wood/lighter/player-action chain and fresh matched-site evidence; current agent-side status is implemented-but-unproven in the playtest package
- threshold-surviving light proof for the synthetic loaded-map `fd_fire` source path is now covered by run `.userdata/dev-harness/harness_runs/20260427_114034/`: current-runtime probe at commit `daa2f1694c`, night `light_packets=1`, horde light signal, and `matched_light_sites=1` / `refreshed_sites=1`; this is not full player-lit-fire proof
- full player-fire product proof is now in `runtime/josef-playtest-package.md` as `bandit-live-signal-real-fire-product-proof` implemented-but-unproven: post-Frau attempt 3 (`bandit.live_world_nearby_camp_real_fire_exact_items_tile_audit_mcw`, run `.userdata/dev-harness/harness_runs/20260427_124445/`) failed the target-tile `fd_fire` midpoint audit, deterministic non-GUI primitive `fire_start_activity_exact_brazier_wood_lighter_creates_fd_fire` proves only the activity seam, and final activity-bridge experiment `.userdata/dev-harness/harness_runs/20260427_134253/` did not leave audited target-tile `fd_fire`/`fd_smoke` before live bandit product closure

### Recently closed validation references

Use the auxiliary docs / `SUCCESS.md` for details. Current short closure map:
- `Basecamp job spam debounce + locker/patrol exceptions packet v0`: stable-cause debounce for repeated completion/missing-tool/no-progress camp chatter; typed `[camp][locker]` / `[camp][patrol]` reports preserve first and changed states while compressing repeats; deterministic message tests, focused `[camp][patrol]` / `[camp][locker]`, and touched-object compile passed.
- `Bandit live-wiring audit + visible-light horde bridge correction v0`: loaded-map visible fire/light -> horde signal bridge proof; not player-lit fire proof.
- `Bandit local sight-avoid + scout return cadence packet v0`: reclosed for the 2026-04-27 live product gap; current-runtime live proof now covers five-OMT hold-off standoff plus scout timeout, home-footprint observation, and returned pressure-refresh writeback.
- `Smart Zone Manager v1 Josef playtest corrections`: implemented-but-unproven live; deterministic geometry/separation proof is green, while clean live/UI layout inspection is in Josef's playtest package.
- `Basecamp medical consumable readiness v0`: deterministic camp/locker proof for bounded bandage-family readiness and cap behavior.
- `Basecamp locker armor ranking + blocker removal packet v0`: deterministic camp/locker proof for generic full-body/protective ranking, blocker clearing, damaged-candidate rejection without repeated requeue/equip churn, ballistic armor preservation, and `[camp][locker]` regression coverage.

---

## Pending probes

Josef has explicitly greenlit actual playtests now, using the same step-by-step metadata/screenshot validation discipline as the harness trust audit. Deterministic-only evidence remains insufficient for product-facing live claims, and load-and-close remains startup/load proof only.

Active process validation target: **C-AOL harness trust audit + proof-freeze packet v0**. Inventory checkpoint `doc/c-aol-harness-trust-audit-inventory-v0-2026-04-27.md` identifies the harness primitive/scenario/fixture surface and selects `dev-harness` / `McWilliams` with `mcwilliams_live_debug_2026-04-07` as the provisional canonical audit anchor. Compact matrix `doc/c-aol-harness-proof-freeze-matrix-v0-2026-04-28.md` now freezes the current primitive verdicts, evidence artifacts, claim rules, and untrusted seams, with Frau-reviewed tightenings for claim-scoped artifacts, metadata-only limits, deferred guards, anti-fixture bias, and green debug-spawn/weather/NPC target-state primitive patterns. This is process-audit/matrix evidence only, not a product-feature closure.

Current process-audit evidence: `startup_harness.py start` writes `startup.step_ledger.json`, embeds `startup_step_ledger` in `startup.result.json`, and marks successful load/readiness as `evidence_class=startup/load`, `feature_proof=false`. Clean startup/load may be green as startup evidence, but still never feature proof by itself. Probe classification now requires `startup_clean_for_feature_steps=true` before `artifacts_matched` can become `feature_proof=true`; stale runtime/profile/load-readiness problems force non-green `startup/load-or-inconclusive`. Dry-run start reports `dry_run_contract.evidence_class=plan-only` and does not install fixtures/profile snapshots. Canonical startup audit run `.userdata/dev-harness/harness_runs/20260427_164639/` reached load/readiness with green plan/focus/load rows, but the screenshot row is yellow because the window title captured stale runtime `84b1e3a604-dirty` while repo HEAD is `821cbf01c6` and runtime-relevant diff includes `src/do_turn.cpp`; this is exactly a non-green startup/load proof, not feature proof.

Step-local proof-freeze slice landed in `startup_harness.py`: live probe/handoff runs now write `<mode>.step_ledger.json`, embed `step_ledger` and `step_ledger_summary` in the report, and block `feature_proof=true` unless every scenario step has a green step-local ledger row in addition to clean startup, artifact matches, and no yellow/blocked wait ledger. `capture` and `capture_after` steps can declare `expected_visible_fact` plus `expected_screen_text_contains` / `expected_screen_text_after_contains`; missing expected facts, unguarded OCR, failed screen capture, stale runtime metadata, abort guards, and non-green wait ledgers become explicit yellow/red/blocked step verdicts instead of being hidden under `artifacts_matched`.

Current step-ledger audit evidence: after forcing the tiles binary to report current runtime `85d6f37f00`, `basecamp.package2_dialog_rules_probe_mcw` run `.userdata/dev-harness/harness_runs/20260427_174653/` caught the old guessed `1` talk key as wrong-screen evidence; Frau-approved attempt 3 `.userdata/dev-harness/harness_runs/20260427_175051/` proves source-backed `t` opens `Talk to whom` as a green row, then blocks/red-classifies `choose_first_talker` because the alpha talker selector remains visible after `a`. The red run remains the preserved false-pass catch, not a basecamp feature regression. Current replacement evidence is green only for scoped navigation/process proof: `.userdata/dev-harness/harness_runs/20260428_014348/` proves `C` -> `What do you want to do`, `t` -> `Talk to whom`, and highlighted-row `enter` -> `Your response`; `.userdata/dev-harness/harness_runs/20260428_014825/` proves the updated `basecamp.package2_dialog_rules_probe_mcw` path through visible `[rules] ... work together` to `Rules for your follower` with 6/6 green step-local rows. Top-level classification remains `feature_proof=false`, `evidence_class=startup/load-or-inconclusive`, `verdict=inconclusive_no_artifact_match`, because these are process/navigation primitives without claim-scoped product artifacts.

Current target-tile audit evidence: after relinking `cataclysm-tiles` to repo HEAD `373511bc36`, `python3 tools/openclaw_harness/startup_harness.py probe bandit.live_world_nearby_camp_real_fire_exact_items_tile_audit_mcw` run `.userdata/dev-harness/harness_runs/20260427_184319/` reached clean startup/runtime (`version_matches_repo_head=true`) but stayed red: 18 GUI/item/firestarter steps are yellow because they still lack expected visible facts or OCR guards, and `audit_saved_target_tile_for_fd_fire` is red because required `fd_fire` is missing. A follow-up direct metadata validation against the same saved world is preserved at `.userdata/dev-harness/harness_runs/20260427_184319/audit_saved_target_tile_for_fd_fire.post_patch_direct_validation.metadata.json`; it reports the requested offset explicitly as an empty tile (`target_location_ms=[3368,994,0]`, `fields=[]`, `items=[]`, `furniture=[]`) instead of leaving the pre-patch probe artifact's `tiles=[]` ambiguous. Follow-up run `.userdata/dev-harness/harness_runs/20260427_191725/` adds same-run `abort_on_metadata_failure` gates after brazier deploy, fuel drop, and final fire; it reached clean startup for feature steps (`startup_clean_for_feature_steps=true`, runtime title `373511bc36` while repo HEAD `747e5d7fce` has no runtime-relevant diff) and then aborted at `audit_saved_target_tile_for_brazier` because the saved tile east of the player had `fields=[]`, `items=[]`, `furniture=[]`, missing required `f_brazier`. After source/static control lookup and a bounded post-Frau selector check, canonical run `.userdata/dev-harness/harness_runs/20260427_200100/` added the original `audit_saved_player_items` before the GUI macro and proved only legacy save inventory presence (`inventory_counts={"2x4":20,"brazier":1,"lighter":1}` at that time), but selector/activation/place remained non-green and the same east tile was still empty/missing `f_brazier`. Updated audit logic now distinguishes that legacy top-level `player.inv` bucket from live-selector-accessible worn/held/contained items, which is the desired false-pass shape, not product fire proof.

Current debug-spawn/weather/map-editor target-state evidence: after forcing a real tiles relink so `./cataclysm-tiles --version` reports repo HEAD `32c36dd9bc`, `python3 tools/openclaw_harness/startup_harness.py probe harness.debug_spawn_item_inventory_target_state_audit_mcw` produced run `.userdata/dev-harness/harness_runs/20260428_011205/`. Step-local proof is green (`green_count=6`, `yellow_count=0`, `red_or_blocked_count=0`): the debug item path spawns `electric toothbrush`, the case-sensitive save prompt is proven before uppercase `Y`, saved-player mtime/writeback advances, and same-run saved-player metadata reports `live_accessible_counts.toothbrush_electric=1` with no missing required items. The first monster-family target-state expansion is also green: `python3 tools/openclaw_harness/startup_harness.py probe harness.debug_spawn_monster_target_state_audit_mcw` produced `.userdata/dev-harness/harness_runs/20260428_021800/` with six green step-local rows, a proven `quit`/`Case Sensitive` save prompt before uppercase `Y`, saved-player mtime/writeback advance, and same-run saved-player `active_monsters` metadata reporting `mon_squirrel=1` at player-relative offset `[1,0,0]`. The first weather/temperature target-state expansion is green: `python3 tools/openclaw_harness/startup_harness.py probe harness.debug_force_temperature_target_state_audit_mcw` produced `.userdata/dev-harness/harness_runs/20260428_024606/` with six green step-local rows, the same guarded save/writeback gate, and same-run saved dimension weather metadata reporting `forced_temperature=123.000031` and effective `123.000031F`. The tightened follower-NPC target-state expansion is green: `python3 tools/openclaw_harness/startup_harness.py probe harness.debug_spawn_follower_npc_target_state_audit_mcw` produced `.userdata/dev-harness/harness_runs/20260428_032724/` with seven green step-local rows, a pre-spawn saved-overmap NPC baseline (`observed_npc_count=2`), the same guarded save/writeback gate, and same-run saved overmap `npcs` delta metadata reporting `observed_npc_count_delta=1` plus one new nearby `your_followers` NPC at attitude `3` (`Josefine Ruff`, id `4`, offset `[-3,-3,0]`) with no missing required new NPCs. The debug map-editor field/furniture target-state expansion is green: `python3 tools/openclaw_harness/startup_harness.py probe harness.debug_map_editor_field_furniture_target_state_audit_mcw` produced `.userdata/dev-harness/harness_runs/20260428_034421/` with seven green step-local rows, the same guarded save/writeback gate, and same-run saved-map metadata at offset `[1,0,0]` reporting `observed_field_ids=["fd_smoke"]` and `observed_furniture=["f_chair"]`. The debug map-editor terrain/trap/radiation target-state expansion is green: `python3 tools/openclaw_harness/startup_harness.py probe harness.debug_map_editor_terrain_trap_radiation_target_state_audit_mcw` produced `.userdata/dev-harness/harness_runs/20260428_041845/` with eight green step-local rows, the same guarded save/writeback gate, and same-run saved-map metadata at offset `[1,0,0]` reporting `observed_terrain=["t_grass_dead"]`, `observed_traps=["tr_bubblewrap"]`, and `observed_radiation=[37]` with no missing required terrain/traps/radiation. Top-level classification intentionally remains metadata/process evidence (`feature_proof=false`, no claim-scoped artifact match), not product-feature proof.

Current follower-rule toggle evidence: `python3 tools/openclaw_harness/startup_harness.py probe basecamp.follower_rules_toggle_probe_mcw` produced `.userdata/dev-harness/harness_runs/20260428_044855/` with 14/14 green step-local rows. It starts from saved-overmap `Katharina Leach` follower metadata with `rule_follow_distance_2=false`, proves the current McWilliams path into `Rules for your follower`, presses rules-UI hotkey `b`, OCR-proves the visible row changed to about two paces, proves the case-sensitive save prompt before uppercase `Y`, advances saved-player mtime from `1775515638000000000` to `1777344587255429879`, and reads same-run saved-overmap metadata with `rule_follow_distance_2=true`, `override_enable_follow_distance_2=true`, and `override_follow_distance_2=true`. Top-level classification intentionally remains process/metadata evidence (`feature_proof=false`, no claim-scoped product artifact match), not downstream package behavior closure.

Next evidence target: another Schani/Josef-named harness seam. Smart Zone Manager live layout verification is not the current target anymore: the prior four-attempt package remains implemented-but-unproven, and current-runtime run `.userdata/smart-zone-safe-clean-20260427/harness_runs/20260428_001347/` adds only startup/load red evidence (`blocked_clean_profile_no_loadable_character`, OCR `Dunn has no characters to load`).

Do not try more blind live key variants. Static/source/control lookup says `brazier` activation should run `deploy_furn` -> `Deploy where?`, and `right` is valid only once that direction prompt is active. GUI-as-text observation run `.userdata/dev-harness/harness_runs/20260427_200919/` aborts at missing checked `Use item` menu text. Harness-gated UI trace run `.userdata/dev-harness/harness_runs/20260427_202434/` improves the primitive: `Use item` opens, saved-player metadata proves `brazier` exists with `deploy_furn -> f_brazier`, but after filter `brazier` the post-redraw selector trace does not prove a highlighted brazier row, so the guard aborts before confirm/right. Follow-up run `.userdata/dev-harness/harness_runs/20260427_203847/` uses stronger selector-entry tracing and proves the live `Use item` selector has only `smart_phone` visible before filtering and zero visible entries after filter `brazier`; direct updated-audit validation against that saved world reports the exact items only under `legacy_top_level_inv_counts`, with `missing_required_items=["brazier","2x4","lighter"]` for live-selector-accessible inventory. Fixture storage is now repaired for the focused deploy primitive: the exact-items manifest wields the `brazier` and keeps fuel/lighter in a worn pocket, and runs `.userdata/dev-harness/harness_runs/20260427_212403/` plus `.userdata/dev-harness/harness_runs/20260427_212839/` prove `brazier` selection, `Deploy where?`, and right/east consumption by UI trace. They do **not** prove `f_brazier`, because saved-file mtimes show the map audit read pre-action state. Post-Frau run `.userdata/dev-harness/harness_runs/20260427_213435/` adds `audit_player_save_mtime` and blocks at `blocked_untrusted_post_action_save_writeback`: OCR captured `and quit? (Case Sensitive)` but the prompt guard pattern was too strict, so no confirmation/save/writeback was credited. Follow-up run `.userdata/dev-harness/harness_runs/20260427_213850/` loosens the prompt guard to the observed/source-backed tail and greens the prompt proof, but still blocks at the mtime gate: lower-case `y` did not satisfy the case-sensitive `query_yn` prompt, so the saved-player mtime remained `1777318744398092837`. Source check in `src/output.cpp` shows `FORCE_CAPITAL_YN` makes `query_yn` disallow lower-case/non-modified letters. Frau cleared one source-backed uppercase-`Y` retry; run `.userdata/dev-harness/harness_runs/20260427_214207/` proves the post-deploy writeback primitive and deploy tile gate but still has non-decisive yellow ledger rows. Follow-up `.userdata/dev-harness/harness_runs/20260427_222635/` cleans those proof mechanics: `step_ledger_summary.status=green_step_local_proof`, `green_count=16`, `yellow_count=0`, `red_or_blocked_count=0`, `evidence_class=feature-path`, `feature_proof=true` for the scoped deploy primitive only; save prompt proof matched `and quit?` / `Case Sensitive`, uppercase `Y` advanced saved-player mtime from `1777321610298746508` to `1777321628903849060`, and `audit_saved_target_tile_for_brazier.metadata.json` reports target `[3368,994,0]` with `furniture=["f_brazier"]` / `status=required_state_present`.

Fuel continuation audit evidence: `.userdata/dev-harness/harness_runs/20260427_215006/` starts from the captured post-deploy save and re-proves the saved east tile has `f_brazier`, but aborts before fuel because live-accessible saved inventory is missing `2x4` (`lighter` remains present as charged metadata). Frau cleared a changed same-run retry from the original exact-items fixture. `.userdata/dev-harness/harness_runs/20260427_215154/` proves live-accessible `brazier`/`2x4`/`lighter` before the chain but aborts at the post-fuel save-prompt text guard; `.userdata/dev-harness/harness_runs/20260427_215445/` removes the prompt-text dependency but, without confirmation, does not advance saved-player mtime; `.userdata/dev-harness/harness_runs/20260427_215757/` restores uppercase-`Y` confirmation and still does not advance saved-player mtime (`mtime_changed_since_required_label=false`). Follow-up artifact inspection of `20260427_215757/request_save_after_fuel_drop.after.screen_text.json` found no proven `Save and quit?` / `Case Sensitive` prompt before confirmation. Changed actual-playtest attempt `.userdata/dev-harness/harness_runs/20260427_224113/` uses source-backed multidrop `MARK_WITH_COUNT` but still aborts at `blocked_untrusted_post_fuel_save_prompt`. The scenario then adds a post-drop UI-trace guard before any save request; `.userdata/dev-harness/harness_runs/20260427_225552/`, `20260427_225730/`, and `20260427_225909/` all classify `startup/load-or-inconclusive`, `feature_proof=false`, and abort at `blocked_untrusted_drop_menu_exit_primitive`. The single allowed instrumented diagnostic run `.userdata/dev-harness/harness_runs/20260427_232220/` was rebuilt from the current tiles binary and adds per-entry `chosen_count`, `location_type`, and `recursive_location_type`: it proves Multidrop opens with container/worn rows but, after filter `plank`, the redraw has `visible_item_count=0` in every column and no `typeid="2x4"` row before `TOGGLE_ENTRY` / `MARK_WITH_COUNT`; the run aborts at `blocked_untrusted_drop_filter_or_inventory_visibility`. OCR around the deploy/step checkpoints also reports `plank falls to the ground. x 20`, matching the conclusion that the saved preflight over-credited 20 `2x4` injected into the jeans pocket: runtime does not keep them as a live droppable inventory row. No count selection, confirm-return, save request, uppercase-`Y`, mtime, current-tile fuel, lighter, or `fd_fire` credit is allowed after this red filtered-row gate.

Active validation order:

1. Preserve run `.userdata/dev-harness/harness_runs/20260427_222635/` as all-green scoped deploy primitive evidence, not real-fire proof.
2. Keep `bandit.live_world_nearby_camp_real_fire_exact_items_fuel_tile_audit_mcw` as a red audit scenario for `blocked_untrusted_drop_filter_or_inventory_visibility` / unstable fixture-live inventory, not as closure proof.
3. Preserve Smart Zone Manager live layout verification as implemented-but-unproven/Josef-package. Do not run a fifth/next live probe from `smart-zone-safe-clean-20260427`; current-runtime run `20260428_001347` proved that path has no loadable character and no feature steps ran.
4. Preserve debug-spawn item target-state proof `.userdata/dev-harness/harness_runs/20260428_011205/`, debug-spawn monster target-state proof `.userdata/dev-harness/harness_runs/20260428_021800/`, debug-spawn follower-NPC target-state proof `.userdata/dev-harness/harness_runs/20260428_032724/`, debug force-temperature target-state proof `.userdata/dev-harness/harness_runs/20260428_024606/`, debug map-editor field/furniture target-state proof `.userdata/dev-harness/harness_runs/20260428_034421/`, and debug map-editor terrain/trap/radiation target-state proof `.userdata/dev-harness/harness_runs/20260428_041845/` as scoped harness primitive/process evidence only; remaining debug/map-editor families without same-run target-state metadata remain untrusted.
5. Preserve talker-selector proof `.userdata/dev-harness/harness_runs/20260428_014348/`, follower-rules response/menu proof `.userdata/dev-harness/harness_runs/20260428_014825/`, and follower-rule toggle/writeback proof `.userdata/dev-harness/harness_runs/20260428_044855/` as scoped navigation/UI+metadata/process evidence only. They prove the current McWilliams path into `Rules for your follower` plus one saved follow-distance rule toggle/writeback, not downstream package product behavior. Next named audit primitive is another explicit Schani/Josef harness seam. Do not run lighter/final `fd_fire` steps until a post-fuel save/writeback gate advances mtime and saved-map metadata proves current-tile `f_brazier` plus exact `2x4` in the same run.

Greenlit follow-up validation stack after the actual-playtest non-green boundaries:

1. `Player-lit fire and bandit signal verification packet v0`: still blocked; only after the fuel writeback/current-tile gate is green, prove real player-action ignition, actual `fd_fire`/smoke, bounded wait/time passage, and bandit signal response.
2. `Roof-fire horde detection proof packet v0`: greenlit but blocked behind real player-lit fire; debug may stage horde/distance, but fire/elevated-position proof must be real player-action evidence.
3. Otherwise remain on harness trust-audit/proof-freeze consolidation or ask Schani/Josef for a fresh target rather than reopening packaged Smart Zone proof.

Every primitive/keystroke/setup step needs a precondition, action, expected state, screenshot or exact metadata proof, failure rule, artifact path, and pass/yellow/red/blocked verdict. Any non-canonical fixture must be justified with provenance before it can support closure. A load-and-close run remains startup/load proof only. Failed spawns, wrong screens, missing target fields, stale binaries/profiles, and missing save metadata must produce explicit non-green results rather than feature-proof claims. For `wait_action`, `artifacts_matched` must not override a yellow/blocked wait ledger: every long-wait step needs `<label>.wait_menu`, before/after clock-or-turn evidence plus expected elapsed duration, and either a finish signal or classified interruption.

If a later live probe is needed:
- build the current runtime first when binary freshness matters
- use one named scenario/command path
- extract only decisive report/log fields into context
- multiple focused attempts in one run are allowed when each changes setup/instrumentation/evidence and output stays small
- after two same-item failures, consult Frau Knackal before attempt 3; after four total failures, write the implemented-but-unproven item to Josef's playtest package instead of looping

---

## Reusable commands

Use these when they are actually the missing evidence, not as ritual.

### Narrow camp deterministic pass after a code slice lands
- `git diff --check`
- `make -j4 obj/basecamp.o tests/faction_camp_test.o tests LINTJSON=0 ASTYLE=0`
- focused `./tests/cata_test` filters for the touched camp/locker/patrol reporting path

### Plan status summary command
- `python3 tools/plan_status_summary.py --self-test`
- `python3 tools/plan_status_summary.py /plan`
- `python3 tools/plan_status_summary.py /plan active`
- `python3 tools/plan_status_summary.py /plan greenlit`
- `python3 tools/plan_status_summary.py /plan parked`

### Fresh tiles rebuild only if a later handoff really needs live proof
- `make -j4 TILES=1 cataclysm-tiles`

## Local build caveat

On this Mac, treat top-level `make -j4 tests` as the reliable path for a fresh `cata_test`.
Avoid treating `make -C tests cata_test` as authoritative here; it has been a repeated source of toolchain/stale-build nonsense.
Also: if you actually need a fresh tiles binary, use `make -j4 TILES=1 cataclysm-tiles`; plain `make cataclysm-tiles` is not an honest rebuild path here.
