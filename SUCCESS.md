# SUCCESS

Exit criteria ledger for roadmap items.

Use this file so completion is explicit instead of vibes-based.

## Rules

- Every real roadmap item in `Plan.md` should have a matching success state here (or an equally explicit inline auxiliary).
- When a success-state condition is reached, **cross it off here instead of deleting it immediately**.
- When all conditions for a roadmap item are crossed off, that roadmap item is **DONE** and can be removed from `Plan.md`.
- Josef self-testing is **never** a blocker in this file.
- If Josef-specific tests or checks are useful, write them down as non-blocking notes so Josef can do them later from his own testing list.
- This file is a completion ledger, not a changelog.
- Older closed sections may be compacted: retain the success-state bullets and canonical aux-doc reference, but move closure narrative to `doc/*.md`, `TESTING.md` snapshots, and git history.

---

## Bandit structural bounty overmap completion packet v0

Status: ACTIVE / GREENLIT IMPLEMENTATION PACKET

Success state:
- [ ] Structural OMT classifier exists and deterministic tests cover forest/town/open classes.
- [ ] Per-camp bounded structural scan seeds sparse camp-map leads without global scanning.
- [ ] Harvested/dangerous/recently-checked debounce prevents immediate repeat interest.
- [ ] Non-player structural outing planner can send a small bandit dispatch to forest/town structural bounty.
- [ ] Abstract outing resolver reveals threat at stalking distance, subtracts it from effective bounty/interest, and only consumes structural bounty on arrival if interest survives.
- [ ] Player/NPC mobile bounty remains attached to actors/routes and does not permanently upgrade terrain.
- [ ] Save/load preserves structural leads, active outings, harvested/dangerous outcomes, and member state.
- [ ] Deterministic 500-turn tests prove bandits do not get stuck repeating the same harvested/dangerous tile.
- [ ] Performance tests/counters prove scan/outing work is bounded for multi-camp scenarios.
- [ ] Live/harness feature-path proof shows an idle camp dispatching to structural forest/town bounty without player smoke/light/direct-range bait.
- [ ] Live/harness proof shows stalking-distance threat/interest writeback, optional later arrival harvest, and no immediate repeat of the consumed/dangerous target.
- [ ] Existing player smoke/light signal dispatch behavior still passes its relevant tests and is not regressed.
- [ ] `Plan.md`, `TODO.md`, `SUCCESS.md`, and `TESTING.md` match the final active/closed state.

Current boundary:
- Phase 1 deterministic substrate is the next execution slice; no live game claim is credited yet.

Notes:
- Imagination source lives at `doc/bandit-structural-bounty-overmap-completion-imagination-source-of-truth-2026-04-30.md`.
- Canonical contract lives at `doc/bandit-structural-bounty-overmap-completion-packet-v0-2026-04-30.md`.


---

## GitHub Actions CI recovery + checkpoint packet v0

Status: CLOSED / CHECKPOINTED

Success state:
- [x] Current `dev` GitHub Actions are no longer red for code-caused C-AOL failures, or every remaining red check is explicitly classified with a bounded non-code cause and next owner.
- [x] The C++17/warnings-as-errors failures are fixed: no designated-initializer/missing-field failures in `tests/faction_camp_test.cpp`, no missing-declaration family in `src/basecamp.cpp` / `src/bandit_playback.cpp`.
- [x] Windows build failure is either green or reduced to a named, evidence-backed workflow/runner/package blocker.
- [x] CodeQL is green or its remaining failure is classified as upload/config/external rather than silently sharing the same source compile failure.
- [x] A lightweight CI checkpoint/linking rule exists so future reviewable Andi commits name changed file class, relevant local gate, Actions link when available, and remaining red-check classification.
- [x] `Plan.md`, `TODO.md`, `SUCCESS.md`, and `TESTING.md` match the final state.


Compact reference:
- Canonical contract lives at `doc/github-actions-ci-recovery-checkpoint-packet-v0-2026-04-25.md`.


---

## GitHub normal-download release packet v0

Status: HELD / NOT CURRENTLY PROMOTED

Success state:
- [ ] A new public GitHub release exists on `josihosi/Cataclysm-AOL` with a deliberate tag/version and clear release notes.
- [ ] The release assets match the stated platform support instead of implying broken platforms work.
- [ ] The release source commit and relevant Actions state are linked from canon/testing notes.
- [ ] Josef has a normal GitHub Releases URL he can download from.
- [ ] Any withheld/broken platform is plainly marked with the evidence-backed blocker.

Notes:
- Canonical contract lives at `doc/github-normal-download-release-packet-v0-2026-04-25.md`.
- Current latest stable release observed before packaging: `v0.2.0` / `Cataclysm - Arsenic and Old Lace v0.2.0`.
- This packet was once queued after CI recovery went green, but current `Plan.md` / `TODO.md` promote no active or greenlit execution target. Release/download work needs an explicit Schani/Josef promotion before Andi treats it as active.

---

## C-AOL debug-proof finish stack v0

Status: CLOSED / CHECKPOINTED

Success state:
- [x] The active Basecamp job-spam debounce lane is closed/checkpointed honestly before this stack became active.
- [x] The reopened bandit live signal/site-bootstrap stack has live/source-hook proof for remaining smoke/fire/light product claims, or a precise Josef playtest package for exact implemented-but-unproven behavior.
- [x] Signal range, candidate/scoring split, decay/maintenance, and hold/chill instrumentation are either completed with tests/evidence or explicitly listed as implemented-but-unproven playtest items.
- [x] Smart Zone clean-save live proof is converted into a precise Josef playtest package without reopening the contaminated McWilliams macro.
- [x] The reopened bandit local standoff / scout return live correction is inspected and fixed/proven on the current product path: current-runtime run `.userdata/dev-harness/harness_runs/20260427_154309/` logs five-OMT hold-off (`live_dispatch_goal=140,46,0`), return-home timeout, home-footprint observation, and returned pressure refresh.
- [x] Canon files stop calling current debug notes parked/review-only when Josef has explicitly greenlit another finish pass.

Notes:
- Canonical contract lives at `doc/caol-debug-proof-finish-stack-v0-2026-04-27.md`.
- Attempt rule for this stack was observed: the bandit local standoff / scout-return attempt 3 happened after Frau Knackal consultation and a material code-path/instrumentation change.

---

## Cannibal camp night-raid behavior packet v0

Status: CLOSED / CHECKPOINTED

Success state:
- [x] Deterministic coverage proves cannibal planned attacks/raids require a meaningful pack size under ordinary conditions, while scout/probe behavior may remain smaller only when explicitly classified that way.
- [x] Deterministic coverage proves a one-dispatchable-member cannibal camp does not perform an ordinary full attack/raid against a defended target.
- [x] Deterministic/code coverage proves smoke/light/human-routine nearby leads are interpreted as scout/probe or pack-dispatch pressure rather than instant combat where that substrate is available.
- [x] Deterministic coverage proves darkness/concealment can change a cannibal local-gate outcome from stalk/probe/hold to attack when the rest of the scene is favorable.
- [x] Deterministic coverage proves daylight/no-cover and high-threat cases still delay, hold, probe, stalk, or abort instead of making cannibals suicidal.
- [x] Deterministic coverage proves cannibal stalk/approach sight-avoidance reacts to exposure with bounded adjacent reposition or hold-off/abort rather than visible beeline pressure.
- [x] Bandit shakedown/pay/fight behavior still works unchanged and cannibal shakedown remains blocked.
- [x] Reports name lead source/notes where available, `profile=cannibal_camp`, pack size/strength, darkness/concealment input, current/recent sight-exposure state, posture, and no-shakedown/attack reason.
- [x] Save/load proof preserves cannibal roster, active member IDs, target state, and profile after a multi-member dispatch.
- [x] Live matrix scenario proves day smoke/light or repeated routine creates scout/stalk/dispatch pressure, not instant combat (`20260428_124902`, feature-path, `posture=hold_off`, `active_job=stalk`, smoke lead, no shakedown/combat-forward).
- [x] Live matrix scenario proves night/darkness/concealment can turn confirmed multi-member pack contact into attack through the real dispatch/local-contact path (`20260428_124947`, feature-path, `local_contact=yes`, `darkness_or_concealment=yes`, `posture=attack_now`, `pack_size=2`).
- [x] Live matrix scenario proves exposed stalking/scouting cannibals hold, reposition normally, or abort within a bounded window instead of visibly beelining or teleporting (`20260428_125138`, feature-path, bounded 20-turn window, cannibal `posture=hold_off`, `sight_exposure=recent`, no shakedown/combat-forward).
- [x] Live matrix scenario proves daylight/no-cover and high-threat negatives still hold/probe/abort (`20260428_135323`, feature-path, current runtime `48abd82de9`, `threat=3`, `opportunity=2`, `darkness_or_concealment=no`, `posture=hold_off`, no shakedown/combat-forward).
- [x] Live matrix scenario proves cannibal contact never opens bandit shakedown/pay/fight surfaces, with any bandit control run labeled separately (`20260428_124947` reaches local contact/attack report path with `shakedown=no`; no bandit control run credited).
- [x] Live matrix scenario proves saved-state persistence for active cannibal group/profile/target/live-signal state when a real active group is created (`20260428_130948`, feature-path, guarded save/writeback mtime, active members `[4,5]`, player target, `known_recent_marks` with `live_smoke@...`; no-fixture reload support `20260428_131031`; `intelligence_map.leads=[]` is out of scope, not credited).
- [x] Repeatability or fixture-bias check is run after the first passing product scenario (`repeatability --count 2 cannibal.live_world_day_smoke_pressure_mcw`, stable pass, runs `20260428_125319` and `20260428_125342`).
- [x] Any later harness/product proof reaches the real dispatch/local-contact path and does not close from evaluator-only tests for the scenarios marked green above; remaining open scenario(s) keep the same burden.

Notes:
- Deterministic/code substrate landed in `src/bandit_live_world.cpp`, `src/bandit_live_world.h`, `src/do_turn.cpp`, and `tests/bandit_live_world_test.cpp`: pack-size dispatch, smoke/light nearby-lead classification, live current-exposure feeding into the local gate, darkness/concealment local-gate input, current/recent exposure hold-off, bounded sight-avoid reposition helper proof, no-extort/no-shakedown reporting, high-threat abort, and multi-member save/load proof.
- Evidence: `git diff --check`; `make -j4 obj/do_turn.o obj/bandit_live_world.o tests/bandit_live_world_test.o LINTJSON=0 ASTYLE=0`; `make -j4 tests LINTJSON=0 ASTYLE=0`; `./tests/cata_test "[bandit][live_world][cannibal]"`; `./tests/cata_test "[bandit][live_world][sight_avoid]"`; `./tests/cata_test "[bandit][live_world][approach_gate]"`; `./tests/cata_test "[bandit][live_world]"`.
- Imagination source lives at `doc/cannibal-camp-night-raid-imagination-source-of-truth-2026-04-28.md`.
- Code audit lives at `doc/cannibal-camp-night-raid-code-audit-2026-04-28.md`.
- Canonical contract lives at `doc/cannibal-camp-night-raid-behavior-packet-v0-2026-04-28.md`.
- Live playtest matrix lives at `doc/cannibal-camp-night-raid-live-playtest-matrix-v0-2026-04-28.md`.
- Live feature-path evidence: day smoke pressure `.userdata/dev-harness/harness_runs/20260428_124902/`; night pack local-contact attack `.userdata/dev-harness/harness_runs/20260428_124947/`; exposed/recent-sight hold-off `.userdata/dev-harness/harness_runs/20260428_125138/`; daylight/high-threat negative `.userdata/dev-harness/harness_runs/20260428_135323/`; repeatability day smoke `.userdata/dev-harness/harness_runs/20260428_125319/` and `.userdata/dev-harness/harness_runs/20260428_125342/`.

---

## C-AOL harness trust audit + proof-freeze packet v0

Status: CLOSED / CHECKPOINTED PROCESS FREEZE

Success state:
- [x] A full harness-surface inventory exists and names each current primitive/scenario, its proof artifact, and its known blind spots: `doc/c-aol-harness-trust-audit-inventory-v0-2026-04-27.md`.
- [x] One canonical disposable audit save/profile is selected for the majority of checks, with explicit provenance and justified exceptions for any other fixture: provisional anchor `dev-harness` / `McWilliams` using `mcwilliams_live_debug_2026-04-07`, with transformed bandit and legacy dev fixtures called out as exceptions in the inventory doc.
- [x] Each audited primitive has a step ledger with precondition, action/keystroke, expected state, screenshot or metadata proof, failure rule, artifact path, and pass/yellow/red/blocked verdict. The proof-freeze matrix closes this for audited primitives; unaudited legacy scenarios remain explicitly untrusted rather than silently green.
- [x] The blocked inventory/deploy UI seam has better information extraction before retry: selector/menu trace and checked GUI/OCR guards now prove the selected `brazier`, `Deploy where?`, right/east key consumption, and guarded save prompt; saved-item metadata backs `deploy_furn -> f_brazier`; no terminal ASCII scrape was used as SDL GUI closure proof.
- [x] After that observation primitive landed, the current harness audit was rerun key by key; normal player-action brazier deployment only went green at `.userdata/dev-harness/harness_runs/20260427_222635/` after menu/selector state, uppercase-`Y` save/writeback, and saved east-tile `f_brazier` were all proven.
- [x] The audit covers launch/load readiness, focus/keystrokes, screenshot capture, debug spawn paths, target-tile metadata checks, save/zzip transforms, Smart Zone UI/layout inspection, fire/smoke setup, wait/time passage, NPC/bandit positioning, and report writing. Coverage means each surface has an explicit green/yellow/red/blocked verdict and claim boundary, not that every product feature is green.
- [x] False-pass behavior is guarded against: wrong screen, failed spawn, missing target field, missing save metadata, stale binary/profile, and load-only runs all produce explicit non-green verdicts. Report-classifier selftest covers load-only, stale-startup artifact-match, non-green step-ledger, yellow-wait, and blocked-wait false-pass cases; scenario-specific primitives without their own guards are explicitly non-green/untrusted, not latent closure evidence.
- [x] The frozen workflow states that load-and-close is startup proof only, and no feature package may close from it: see `doc/c-aol-harness-proof-freeze-matrix-v0-2026-04-28.md` and the C-AOL harness skill proof-freeze rules.
- [x] The relevant harness skill/docs are updated so another agent can run and audit the workflow without chat archaeology: the proof-freeze matrix is the repo reference and the C-AOL harness skill now names the step-ledger, claim-scoped artifact, metadata-only, save/writeback, debug target-state, map-editor, Smart Zone, and current boundary rules.
- [x] Frau Knackal or an equivalent review pass checks the proof matrix for contradictions, hidden fixture bias, and claims that outrun their evidence: Frau's process-altitude review found no major false-pass route after claim-scoped artifact, metadata-only, deferred-proof, and anti-fixture-bias tightenings were folded in.

Compact reference:
- Canonical contract lives at `doc/c-aol-harness-trust-audit-and-proof-freeze-packet-v0-2026-04-27.md`.
- Inventory checkpoint lives at `doc/c-aol-harness-trust-audit-inventory-v0-2026-04-27.md`.
- Frozen proof matrix lives at `doc/c-aol-harness-proof-freeze-matrix-v0-2026-04-28.md`.
- Closure review on 2026-04-28: no remaining process-audit seam blocks checkpoint closure. At that time, concrete product seams remained outside this lane: fuel Multidrop exact-`2x4` visibility was red/blocked, Smart Zone live layout was implemented-but-unproven/Josef-package, and real fire/bandit signal proof was blocked behind fuel. Later proof packets supersede those product seams where explicitly recorded below.
- Validation for closure review: `python3 tools/openclaw_harness/proof_classification_unit_test.py` (6 tests, OK).

---

## C-AOL actual playtest verification stack v0

Status: CLOSED / CHECKPOINTED READY FOR SCHANI REVIEW

Success state:
- [x] Fuel continuation behind the green brazier deploy gate has an honest old-path outcome: instrumented run `.userdata/dev-harness/harness_runs/20260427_231210/` keeps the chain non-green after the Multidrop `plank` filter redraw shows zero visible rows and `CONFIRM` blocks with `selected_stacks=0 total_selected_qty=0`; follow-up row-specific diagnostic `.userdata/dev-harness/harness_runs/20260427_232220/` names the current blocker as `blocked_untrusted_drop_filter_or_inventory_visibility` because filtered Multidrop has no selectable `typeid="2x4"` fuel row. No count selection, confirm-return, save request, post-fuel mtime/current-tile `2x4`/lighter/`fd_fire` proof is credited from that path.
- [x] Fuel writeback repair via wood source zone preserves the old-path non-proof history without reopening it: `.userdata/dev-harness/harness_runs/20260429_090634/`, `_093118/`, `_093509/`, `_095021/`, `_122807/`, `_122955/`, and final diagnostic `.userdata/dev-harness/harness_runs/20260429_142257/` remain blocker/postmortem evidence only; `fuel_writeback_source_zone_v0_2026-04-29` remains retired and must not be used for fire/lighter proof.
- [x] Fuel normal-map entry primitive is green at `.userdata/dev-harness/harness_runs/20260429_140645/`: setup footing and normal gameplay map UI are proven, with raw JSON/item-info/stale/debug screens red-blocked before action keys. Canonical contract: `doc/fuel-normal-map-entry-primitive-packet-v0-2026-04-29.md`.
- [x] Fuel writeback repair via wood source zone proves the visible/operational deployed-brazier gate at `.userdata/dev-harness/harness_runs/20260429_144805/`: normal Apply inventory -> filtered `brazier` -> `Deploy where?` -> east/right placement, look/OCR `hrazıer` + `burn things`, saved `f_brazier` + `log`, saved `SOURCE_FIREWOOD`, no lighter/fire keys.
- [x] Real player source-zone fire is green at `.userdata/dev-harness/harness_runs/20260429_153253/`: normal Apply inventory deployed `brazier`, normal Apply inventory selected charged `lighter`, exact `Light where?` UI trace opened before targeting, source-firewood prompt and recognizable ignition OCR were captured, save mtime advanced, and saved target tile contains `f_brazier` + `fd_fire`. Canonical proof: `doc/fuel-visible-brazier-source-zone-firestarter-action-v0-2026-04-29.md`.
- [x] Smart Zone Manager live layout verification first reached an honest non-green boundary: deterministic geometry/separation remained support only, the precise manual recipe was packaged in `/Users/josefhorvath/.openclaw/workspace/runtime/josef-playtest-package.md`, and current-runtime rerun `.userdata/smart-zone-safe-clean-20260427/harness_runs/20260428_001347/` was startup/load red only (`Dunn has no characters to load`) with no feature/layout proof credited. This historical boundary is superseded by the later green live coordinate-label proof recorded below.
- [x] Player-lit fire/bandit signal verification is green at `.userdata/dev-harness/harness_runs/20260429_162100/`: starting from the real player source-zone fire evidence at `.userdata/dev-harness/harness_runs/20260429_153253/`, fixture `player_lit_fire_bandit_signal_wait_v0_2026-04-29` adds mineral water plus an `AUTO_DRINK` zone, scenario `bandit.player_lit_fire_signal_wait_mcw` proves 30-minute bounded wait/time passage (`observed_delta_turns=1800`), same-run live smoke/fire signal scan/maintenance, `bandit_camp` dispatch with `candidate_reason=live_signal`, and saved active scout response metadata with `known_recent_marks` including `live_smoke@140,41,0`. Canonical proof: `doc/player-lit-fire-bandit-signal-verification-v0-2026-04-29.md`.
- [x] Roof-fire horde detection proof is green at `.userdata/dev-harness/harness_runs/20260429_180239/`: split-run fixture `roof_fire_horde_split_wait_from_player_fire_v0_2026-04-29` and scenario `bandit.roof_fire_horde_split_wait_from_player_fire_mcw` load the saved player-created roof-fire world from `.userdata/dev-harness/harness_runs/20260429_172847/`, re-audit roof/elevated `t_tile_flat_roof` + `f_brazier` + `fd_fire`, prove bounded time passage (`observed_delta_turns=300`), capture live roof-fire horde light-signal artifacts (`horde_signal_power=20`, `elevated_roof_exposed=yes`, `vertical_sightline=yes`), and save staged-horde response metadata (`destination_ms` retargeted to `[3360,984,1]`, `moves=8400`, `last_processed=5267242`). Product seam: `live_bandit_fire_source_is_elevated_roof_exposed` treats elevated `t_flat_roof` / `t_tile_flat_roof` fire sources as exposed roof signals. Boundary: `tracking_intensity` remained `0`, so the credited response is retarget/movement-budget metadata, not positive tracking-intensity proof. Canonical proof: `doc/roof-fire-horde-detection-proof-v0-2026-04-29.md`.
- [x] Bandit empty-camp retirement proof implements the conjunctive predicate: a camp/site retires from active AI calculations only when no home/inside live members or spawn-tile home headcount remain **and** no active dispatch/outbound/local-contact pressure remains. Deterministic coverage proves home-only, active-dispatch-only, unresolved returning-home/contact negative cases, plus the fully empty positive case and `retired_empty_site` report/dispatch/signal guards. Canonical contract: `doc/bandit-empty-camp-retirement-audit-mode-packet-v0-2026-04-28.md`.
- [x] Andi reports evidence class boundaries intact: the old Multidrop fuel path remains red/non-green at the visibility/writeback gate; the new fuel repair proves normal source-zone ignition + saved `fd_fire`; player-lit source-zone fire -> bandit signal response is green; roof-fire horde detection is green only through the split-run roof/elevated proof above; Smart Zone live layout is green only through the later reopened coordinate-label proof, not the old deterministic/startup attempts; performance rows are live-path performance-load evidence; the pre-staged three/four-site rows are not natural multi-site player-pressure proof; startup/load, deterministic support, live product proof, and unproven seams stay separately labeled.

Compact reference:
- Canonical contract lives at `doc/c-aol-actual-playtest-verification-stack-v0-2026-04-27.md`.

---

## Cannibal camp confidence-push live playtest packet v0

Status: COMPLETE / CONFIDENCE UPLIFT GREEN

Success state:
- [x] A compact live/harness matrix records the five proposed shapes and their current status: `doc/cannibal-camp-confidence-push-live-playtest-matrix-v0-2026-04-29.md`.
- [x] At least one broader wandering/day pressure run reaches the real live dispatch/signal path and supports the closed cannibal behavior claim without instant daytime suicide: `.userdata/dev-harness/harness_runs/20260429_013310/`, scenario `cannibal.live_world_day_smoke_pressure_mcw`, current window/runtime `782d8edabd`, 2/2 green step ledger, `feature_proof=true`, matched live smoke/signal path, `profile=cannibal_camp`, `darkness_or_concealment=no`, `shakedown=no`, `combat_forward=no`, and `signal_packet=live_smoke@`.
- [x] At least one night/contact run reaches real local-contact behavior and supports pack-forward danger under darkness/concealment: `.userdata/dev-harness/harness_runs/20260429_014900/`, scenario `cannibal.live_world_night_local_contact_pack_mcw`, current window/runtime `acfe6fd0ce`, 2/2 green step ledger, `feature_proof=true`, matched real local-contact pack-forward path, `profile=cannibal_camp`, `posture=attack_now`, `pack_size=2`, `darkness_or_concealment=yes`, `local_contact=yes`, `shakedown=no`, and `combat_forward=yes`.
- [x] Save/reload continuation proves active cannibal group/profile/target/job state stays coherent after reload: create/save feature proof `.userdata/dev-harness/harness_runs/20260429_021849/`, scenario `cannibal.live_world_day_smoke_persistence_mcw`, current runtime `e778902cac`, 7/7 green step ledger, guarded save mtime writeback, saved active `cannibal_camp` stalk group with active members `[4,5]`, target `player@140,41,0`, and `known_recent_marks` including `live_smoke@140,41,0`; paired no-fixture reload support `.userdata/dev-harness/harness_runs/20260429_021929/`, scenario `cannibal.live_world_day_smoke_persistence_reload_audit_mcw`, 2/2 green step ledger, saved-state metadata still present after fresh startup.
- [x] A different-seed/different-footing run reduces fixture-bias risk: `.userdata/dev-harness/harness_runs/20260429_022021/`, scenario `cannibal.live_world_exposed_sight_avoid_mcw`, current runtime `e778902cac`, 5/5 green step ledger, feature proof on fixture `cannibal_live_world_exposed_sight_avoid_v0_2026-04-28` from source footing `bandit_local_sight_avoid_exposed_v0_2026-04-27`, showing `profile=cannibal_camp`, `active_job=stalk`, `posture=hold_off`, `pack_size=2`, `recent_exposure=yes`, `sight_exposure=recent`, `shakedown=no`, `combat_forward=no`.
- [x] A bandit contrast control preserves the shakedown/pay/fight distinction while the credited cannibal day-pressure row remains no-shakedown/no-combat-forward: bandit run `.userdata/dev-harness/harness_runs/20260429_012915/` proves `pay_option=yes fight_option=yes` plus `shakedown_surface fight demanded=15797 reachable=45134`; cannibal day-pressure run `.userdata/dev-harness/harness_runs/20260429_013310/` proves `profile=cannibal_camp`, `darkness_or_concealment=no`, `shakedown=no`, and `combat_forward=no`. Night/contact no-shakedown is also covered by the checked night/contact row above.
- [x] The final verdict updates confidence honestly: confidence uplift green, with retained caveats that reload support is saved-file/startup support paired with create/save feature proof, and different-footing repeat reduces but does not eliminate fixture-bias risk.

Notes:
- Imagination source lives at `doc/cannibal-camp-confidence-push-live-playtest-imagination-source-of-truth-2026-04-28.md`.
- Canonical contract lives at `doc/cannibal-camp-confidence-push-live-playtest-packet-v0-2026-04-28.md`.
- This is confidence uplift for the closed cannibal behavior, not a behavior redesign or proof that the old closure failed.

---

## Smart Zone Manager harness-audit retry packet v0

Status: CLOSED / GREEN LIVE COORDINATE-LABEL PROOF AFTER REOPEN

Success state:
- [x] The current-runtime retry used clean startup/load hygiene before UI-entry checks.
- [x] Guarded screen-text rows prevented add-zone/filter/generation credit when `Zones manager` was not visible in the older blocked route.
- [x] Action-dispatch instrumentation identified the old agent-side primitive gap: delivered default `Y` reached `action_menu`, not `zones`, and no Zone Manager invocation trace fired.
- [x] The prior non-green result was classified as implemented-but-unproven / Josef playtest package instead of being laundered into green proof.
- [x] The reopened live UI path proves Zone Manager opened and Smart Zone generation was invoked: `.userdata/smart-zone-audit-live-20260429e/harness_runs/20260429_225644/` traces `invoke_zone_manager`, prompt acceptance, and `[harness][smart_zone] result success=1 placed_zones=23`.
- [x] The generated Zone Manager list is inspected after live generation and after close/save-changes/reopen; visible relative coordinate labels are captured in live `zone_manager_row` redraw traces with `visible_label` / `compact_label` fields.
- [x] Generated/reopened row metadata proves zone names/types plus absolute coordinates where available via `start_abs`, `end_abs`, and `center_abs`, showing a multi-position layout rather than one-tile collapse.

Notes:
- Imagination source lives at `doc/smart-zone-manager-harness-audit-retry-imagination-source-of-truth-2026-04-28.md`.
- Canonical contract lives at `doc/smart-zone-manager-harness-audit-retry-packet-v0-2026-04-28.md`.
- Final green run: `.userdata/smart-zone-audit-live-20260429e/harness_runs/20260429_225644/`, scenario `smart_zone.live_coordinate_label_proof_v0_tmp9` (stabilized as `smart_zone.live_coordinate_label_proof_v0`), `feature_proof=true`, `evidence_class=feature-path`, `verdict=artifacts_matched`, 18/18 green step-local rows.
- The proof verdict is green for the live coordinate-label/lumping claim. Screenshot/OCR artifacts support the UI checkpoints but OCR is fallback-quality; row trace metadata is the decisive coordinate-label evidence. Full process-reload disk persistence is not separately claimed.
- Prior non-green UI-entry runs remain postmortem evidence only and should not be rerun as ritual.

---

## Generic clean-code boundary review packet v0

Status: CLOSED / CHECKPOINTED REPORT-ONLY BOUNDARY REVIEW

Success state:
- [x] One compact boundary-review report exists and names the repo state it inspected (`doc/generic-clean-code-boundary-review-report-v0-2026-04-29.md`).
- [x] The report checks canon consistency across `Plan.md`, `TODO.md`, `SUCCESS.md`, `TESTING.md`, and `andi.handoff.md`.
- [x] The report checks build/test/lint risk at the appropriate level for the current boundary, or explains why a gate was not run.
- [x] Findings are concrete, anchored, and classified as `fix now`, `queue`, or `ignore/watch`.
- [x] No feature lane is reopened, promoted, or deprioritized without explicit Schani/Josef review.
- [x] Any proposed edits are bounded follow-up items, not silent cleanup drift.

Notes:
- Imagination source lives at `doc/generic-clean-code-boundary-review-imagination-source-of-truth-2026-04-28.md`.
- Canonical contract lives at `doc/generic-clean-code-boundary-review-packet-v0-2026-04-28.md`.
- Report-only boundary is complete. The only unpromoted queue/watch item is whether to later remove the gated `src/handle_action.cpp` dispatch trace hook; do not reopen Smart Zone live proof without a repaired UI-entry/key-delivery primitive or Josef manual evidence.

---

## C-AOL live AI performance audit packet v0

Status: COMPLETE / GREEN ENOUGH FOR CURRENT PLAYTEST SCALE

Success state:
- [x] A live/harness performance matrix exists for one, two, pre-staged three, and pre-staged four active hostile overmap AI cases in `doc/c-aol-live-ai-performance-matrix-v0-2026-04-29.md`; a true zero-site idle baseline is explicitly omitted by current scope, with the one-site row serving as the low-end live baseline unless Schani/Josef requests a separate idle comparison.
- [x] Each measured case names the profile/job mix, active site count, in-game elapsed window, wall-clock timing, and relevant counters/log fields. Credited rows: `.userdata/dev-harness/harness_runs/20260429_025639/`, `.userdata/dev-harness/harness_runs/20260429_032427/`, `.userdata/dev-harness/harness_runs/20260429_040926/`, and `.userdata/dev-harness/harness_runs/20260429_041936/`.
- [x] The measured path reaches the real live-game bounded-wait/`overmap_npc_move()` cadence and emits compact `bandit_live_world perf:` counters; evaluator-only, benchmark-only, fixture/setup, and startup/load evidence are classified as support only.
- [x] The code performance audit names hot-loop/scaling risks in `src/do_turn.cpp`, `src/bandit_live_world.cpp`, save/serialization, signal matching, sight/exposure checks, and report/log emission in the matrix code-audit notes.
- [x] No optimization is needed for the measured current envelope: the four-site row stayed compact (`total_us` min/median/max `540/560.0/5572`, sum `8360`, one dispatch-due row `dispatch_us=4654`, harness wall-clock `real 39.27s`), and natural three/four-site player-pressure remains a cap/watchlist behavior question rather than a measured performance bottleneck.
- [x] The final verdict is **green enough for current playtest scale**, with the boundary that pre-staged three/four-site rows are performance-load evidence, not natural multi-site player-pressure dispatch proof.

Notes:
- Imagination source lives at `doc/c-aol-live-ai-performance-imagination-source-of-truth-2026-04-28.md`.
- Canonical contract lives at `doc/c-aol-live-ai-performance-audit-packet-v0-2026-04-28.md`.
- Current matrix progress: one-site and two-site rows are green as natural/live dispatch footing; the three-site row is green only as a labeled pre-staged performance-load row (`.userdata/dev-harness/harness_runs/20260429_040926/`). The natural three-player-pressure recipe remains cap/watchlist evidence at `sites=3 active_sites=2`, not row closure.

---


## Bandit camp-map risk/reward dispatch planning packet v0

Status: CLOSED / SCOPED LIVE PRODUCT CHECKPOINT GREEN

Scoped checkpoint success state:
- [x] Camp-map implementation substrate landed: camp-owned `camp_intelligence_map`, save/load serialization, active target OMT persistence, scout-return writeback into the source camp map, live signal marks writing camp-map leads, and remembered camp-map lead selection through the live dispatch cadence/report path.
- [x] Deterministic/code support landed for two-OMT ordinary scout stand-off, 720-minute ordinary scout return clock, 2/4/5/7/10 roster/reserve sizing, active-outside dogpile blocking, wounded/unready and killed-member shrinkage, bounded stockpile pressure, high-threat non-escalation, prior defender-loss pressure, prior bandit-loss cooling, larger-than-scout stalk sizing, and no-opening hold/return.
- [x] Feature-path evidence covers vanished-signal remembered-lead redispatch: `bandit.camp_map_vanished_signal_redispatch` in `.userdata/dev-harness/harness_runs/20260428_185947/`.
- [x] Feature-path evidence covers 2/5/10 variable-roster sizing: `bandit.variable_roster_tiny_dispatch_sizing_live` in `.userdata/dev-harness/harness_runs/20260428_202044/`, `bandit.variable_roster_dispatch_sizing_live` in `.userdata/dev-harness/harness_runs/20260428_192059/`, and `bandit.variable_roster_large_cooled_dispatch_sizing_live` in `.userdata/dev-harness/harness_runs/20260428_193621/`.
- [x] Feature-path evidence covers no-opening stalk-pressure decay: `bandit.stalk_pressure_waits_for_opening` in `.userdata/dev-harness/harness_runs/20260428_195617/`. This is only active remembered pressure -> no-opening rejection -> stale/decayed lead, not opening-present escalation.
- [x] Feature-path evidence covers bounded sight avoidance: `bandit.scout_stalker_sight_avoid_live` in `.userdata/dev-harness/harness_runs/20260428_173626/`.
- [x] Feature-path evidence covers high-threat/low-reward hold and same-fixture repeatability: `bandit.high_threat_low_reward_holds` in `.userdata/dev-harness/harness_runs/20260428_200600/`, repeated in `.userdata/dev-harness/harness_runs/20260428_211105/` and `.userdata/dev-harness/harness_runs/20260428_211153/` with `overall_verdict=stable_repeatability_pass`.
- [x] Feature-path evidence covers active-outside dogpile blocking: `bandit.active_outside_dogpile_block_live` in `.userdata/dev-harness/harness_runs/20260428_200434/`.
- [x] Feature-path evidence preserves shakedown/toll-control guardrails: `.userdata/dev-harness/harness_runs/20260428_204454/`, `.userdata/dev-harness/harness_runs/20260428_204630/`, and `.userdata/dev-harness/harness_runs/20260428_204813/`.
- [x] Optional empty-camp live sanity is green: `bandit.empty_camp_retirement_live` in `.userdata/dev-harness/harness_runs/20260428_214542/` proves the fully empty positive retires and home-side/active-outside negatives do not.
- [x] Product review accepted the scoped live/product matrix as enough for this checkpoint; no second-fixture bias variant is required before closure.

Caveats:
- The old player-created fire/smoke/light fuel/writeback route receives no credit from this checkpoint; the active wood-source-zone repair is the only reopened bridge for that proof.
- Repeatability is same-fixture confidence only, not second-fixture anti-bias proof.
- A later second-fixture bias variant can be queued if Josef wants stronger confidence; it is not a closure blocker for this lane.

Validation recorded: `python3 -m py_compile tools/openclaw_harness/startup_harness.py`; relevant scenario/fixture `python3 -m json.tool` checks; `python3 tools/openclaw_harness/proof_classification_unit_test.py`; `git diff --check`; `make -j4 TILES=1 cataclysm-tiles LINTJSON=0 ASTYLE=0`; `./tests/cata_test "[bandit_live_world]"`; `./tests/cata_test "[bandit][live_world][camp_map]" --success`; and the named feature-path harness probes above. `make astyle-diff` remains locally blocked by missing `astyle`.

Canonical contract lives at `doc/bandit-camp-map-risk-reward-dispatch-planning-packet-v0-2026-04-28.md`.
Andi lane draft lives at `doc/bandit-camp-map-risk-reward-dispatch-andi-lane-v0-2026-04-28.md`.
Bandit live product matrix lives at `doc/bandit-live-product-playtest-matrix-v0-2026-04-28.md`.

---
## Test-vs-game implementation audit report packet v0

Status: CLOSED / CHECKPOINTED

Success state:
- [x] A concrete report exists at `doc/test-vs-game-implementation-audit-report-2026-04-26.md` or an explicitly equivalent path.
- [x] The report covers the highest-risk bandit AI / camp lanes, including `tests/bandit_mark_generation_test.cpp`, `tests/bandit_playback_test.cpp`, `tests/bandit_live_world_test.cpp`, and the live dispatch path through `src/do_turn.cpp` / `src/bandit_live_world.cpp`.
- [x] The report separately classifies smoke, light, weather, horde attraction, site bootstrap, dispatch, local handoff, and scout behavior by evidence class.
- [x] Each audited pass condition says whether the tested logic is produced/consumed by live gameplay, deterministic playback only, harness setup only, or currently hollow/missing.
- [x] Every hollow/missing bridge is assigned to one of the greenlit packages or marked as a new follow-up if it does not fit.
- [x] `TESTING.md` gets a compact update summarizing the audit result and preserving the rule that tests cannot impersonate live implementation.
- [x] The report names the first implementation package Andi should execute next after the audit.


Compact reference:
- Canonical contract lives at `doc/test-vs-game-implementation-audit-report-packet-v0-2026-04-26.md`.
- Closed report lives at `doc/test-vs-game-implementation-audit-report-2026-04-26.md`.


---

## Bandit live signal + site bootstrap correction v0

Status: GREENLIT / REOPENED FOR DEBUG-PROOF FINISH (PARTIAL, NOT CLOSED)

Success state:
- [x] Existing hostile overmap special families that should participate in live hostile-site logic can register abstract `bandit_live_world` site records without requiring the player to enter spawn/load range first.
- [ ] Abstract site records carry enough cheap roster/profile/headcount state to dispatch and later materialize concrete NPCs without save/perf blow-up. _(Partial: selected abstract overmap-special candidates now lazily create only the minimum profile-specific concrete scout roster, reconcile it through the owned-site ledger, and skip failed claims before overmap insertion; paired several-hour harness proof exercises this footing.)_
- [x] Materialized NPCs reconcile back to the same owned-site ledger, preserving exact-member writeback behavior when concrete members exist.
- [ ] Real or explicitly synthetic fire/smoke/light observations can create or refresh bounded live bandit marks/leads through the running game path, not only authored playback packets, and live signal generation respects weather/light conditions such as daylight, darkness, fog/mist, rain, wind, shelter/containment, source strength, persistence, and exposure. _(Partial: raw saved `fd_fire` / `fd_smoke` near-player fields now build live smoke packets using current weather, refresh owned-site marks, and repaired reader run `.userdata/dev-harness/harness_runs/20260427_014408/` proves initial `matched_sites=1 refreshed_sites=1` before the several-hour wait plus later no-signal decay. This is map-field reader proof only. A bounded synthetic smoke source from nothing is allowed as `synthetic smoke-source/live-signal proof` if labeled as such; it may prove smoke-source live-signal behavior but not player fire-lighting. Full player-fire proof still requires the candidate action chain — deployed brazier, wood beside it, firewood source on the wood, lighter, normal in-game lighting, visible fire/smoke, safe player placement, survival needs such as thirst/water handled, and then wait/log evidence. Live fire now also builds ordinary-light packets with current time/weather/exposure/source detail; current-runtime run `.userdata/dev-harness/harness_runs/20260427_114034/` proves the synthetic loaded-map `fd_fire` light branch can survive thresholding with `light_packets=1`, horde light signal, `matched_light_sites=1`, and `refreshed_sites=1`. This is synthetic source-hook proof, not full player-lit-fire proof.)_
- [ ] The corrected range matrix is implemented or explicitly centralized: `40 OMT` overmap AI/system envelope; about `15 OMT` sustained smoke cap; ordinary bounty around `10 OMT`; confident threat around `6 OMT`; hard/searchlight threat around `8 OMT`; exceptional elevated light adapter-bounded inside the `40 OMT` envelope; movement remains `1-6 OMT/day` elapsed-time-earned travel credit. _(Partial: live dispatch now separates the `40 OMT` system envelope, `10 OMT` direct-player cap, and adapter-derived smoke cap.)_
- [x] The hard `distance <= 10` live-dispatch gate is removed or demoted so `10 OMT` ordinary visibility no longer impersonates the whole system range.
- [ ] Signal observation/decay cadence is separate from dispatch decision cadence, with event-driven creation and reviewer-readable maintenance. _(Partial: signal observation/mark refresh now runs on a `5_minutes` cadence while dispatch stays `30_minutes`; the current wait proof observes seeded smoke/fire fields decaying to no-signal, but decay policy still needs reviewer-readable design/coverage.)_
- [ ] Instrumentation distinguishes empty ownership, no signal packet, below-threshold signal, rejected-by-range, cadence skip, and hold/chill decisions. _(Partial: empty ownership, no signal packet, below-threshold smoke/light, smoke/light packet counts, matched smoke/light site counts, rejected-by-range, signal packet id, candidate distance, cap used, cadence skip, missing concrete member, and route failure are now logged; full hold/chill signal reasons remain open.)_
- [ ] Deterministic tests cover the range matrix, site bootstrap serialization, signal-specific caps, and candidate filtering/scoring split. _(Partial: site bootstrap serialization/reconciliation and live-signal mark ledger refresh/capping are covered; candidate scoring split and full range matrix remain open.)_
- [ ] At least one bounded smoke-source/live-signal proof shows a smoke source under live wait/time passage producing or refreshing a live bandit candidate/mark on a real owned-site path during/around a several-hour `|` wait, plus one no-signal control for the same setup. This may be either the full player-action fire chain or an explicitly labeled synthetic smoke-source shortcut. If the shortcut is used, keep the full player-fire proof open or move it to Josef's playtest package under the 4-attempt rule; the full proof remains: deploy a brazier, place wood next to it, put the firewood source on top of the wood, have/place a lighter, light the fire through normal in-game mechanics, visibly produce fire/smoke while the player stays safe and basic needs such as thirst/water are handled. _(Raw saved-field run `.userdata/dev-harness/harness_runs/20260427_014408/` is retained as map-field reader proof only.)_

Notes:
- Canonical contract lives at `doc/bandit-live-signal-site-bootstrap-correction-v0-2026-04-26.md`.
- This package supersedes the older 48/60 OMT starter lean with Josef's corrected `40 OMT` overmap AI/system envelope while preserving the anti-tripwire product law.
- First implementation slice registers abstract overmap-special hostile sites from existing loaded overmaps during the 30-minute overmap NPC cadence, serializes the abstract site footprint/headcount/profile, reconciles later concrete spawn claims into the same ledger, expands live dispatch candidate eligibility to the `40 OMT` system envelope, and adds reviewer-readable dispatch/bootstrap skip/reject logging.
- Current implementation also has a minimal raw-field `fd_fire` / `fd_smoke` reader hook, accepted live-signal mark memory refresh, candidate-local lazy materialization for abstract overmap-special sites, and repaired several-hour `|`-wait reader proof for initial smoke-site refresh followed by decay/no-signal. Josef explicitly reopened the remaining debug-proof work on 2026-04-27 under `C-AOL debug-proof finish stack v0`; do not close from raw saved-field proof, and do not park failed implemented behavior as dead. Preserve the specific failed reviewed run `.userdata/dev-harness/harness_runs/20260427_013136/`: `signal_packet=yes`, but `matched_sites=0 refreshed_sites=0 rejected_by_signal_range=1`, then no-signal decay. Full player-fire proof, signal decay design, candidate/scoring/range follow-up, and remaining hold/chill evidence remain open under the 4-attempt escalation/playtest-package rule; the clean threshold-surviving synthetic loaded-map light-source proof is covered by `.userdata/dev-harness/harness_runs/20260427_114034/`.
- Keep `Basecamp medical consumable readiness v0` separate unless Josef explicitly bundles it.

---

## Bandit live-wiring audit + visible-light horde bridge correction v0

Status: CLOSED / MOVED DOWNSTREAM

Success state:
- [x] Docs/canon clearly distinguish deterministic proof/playback behavior from live game behavior for bandit light, smoke, horde-pressure, and handoff claims.
- [x] The live visible-light-to-horde bridge is implemented in source and deterministically tested.
- [x] If implemented, the bridge calls the real horde signal path through bounded thresholds and reviewer-readable reports.
- [x] At least one deterministic test proves bridge thresholds and one live/harness proof shows a real light/fire source can affect a real horde signal path.
- [x] Existing bandit test claims are audited enough that no closed packet says “game does X” when only an authored proof packet does X.


Compact reference:
- Canonical contract lives at `doc/bandit-live-wiring-audit-and-light-horde-bridge-correction-v0-2026-04-26.md`.


---

## Bandit local sight-avoid + scout return cadence packet v0

Status: RECLOSED / CURRENT-RUNTIME PRODUCT PROOF

Fresh 2026-04-27 live evidence from Josef superseded the earlier closure for product purposes: smoke attraction worked, but the local scout/hold-off behavior crowded the player/basecamp too closely and was not visibly timing out/returning home in the current save. The reopened product gap is now reclosed on current runtime: the correction handles timed-out scouts before local-contact gating, preserves five-OMT hold-off standoff, and proves returned scout writeback through the live McWilliams/Basecamp path.

Success state:
- [x] Stalking / hold-off bandits in the reality bubble can detect current or recent exposure to the player or nearby camp NPCs and attempt a bounded reposition toward cover or broken line of sight. _(Deterministic coverage plus live/harness proof `.userdata/dev-harness/harness_runs/20260427_061344/`: `bandit_live_world sight_avoid: exposed -> repositioned npc=4 ... reason=repositioning because exposed`.)
- [x] The sight-avoid behavior is local and heuristic: deterministic tests prove it does not use magical future-cone omniscience and does not teleport.
- [x] A scout outing has a finite live sortie window and can return home after watching long enough, instead of lingering indefinitely in local contact. _(Deterministic/live-wired source path plus live/harness return-home decision proof `.userdata/dev-harness/harness_runs/20260427_051117/`.)_
- [x] Returned scout state writes back through the owned-site pressure/outing cleanup path without automatically conjuring a larger group. _(Deterministic writeback coverage plus live/harness follow-through `.userdata/dev-harness/harness_runs/20260427_054353/`: returned scout pressure refresh logged, active group/target/member ids and sortie clocks cleared, member `4` saved back on home footprint.)_ **Boundary:** this is not proof of the newer persistent camp-map lead contract; it does not by itself prove bounty/threat/confidence/age/source/outcome scout memory, vanished-signal redispatch, or risk/reward sizing from remembered scout knowledge.
- [x] The single-scout current behavior remains explainable: `scout` is still one member unless a later job or escalated decision explicitly requires more.
- [x] Reviewer-readable output distinguishes `still stalking`, `repositioning because exposed`, `returning home`, and `re-dispatch/escalation decision`. _(Source/report strings cover the branches; live proof now covers returning-home/writeback and `sight_avoid: exposed -> repositioned`. Later redispatch tuning is not claimed beyond explicit owned-site re-evaluation.)_
- [x] At least one live/harness proof uses `bandit.live_world_nearby_camp_mcw` or an equivalent real owned-site scenario and confirms the same code path would apply to a normal discovered bandit camp, while separately naming the harness bias that places the camp nearby on purpose.
- [x] Recent live logs/save fields from the 2026-04-27 McWilliams/Basecamp fixture are inspected and answer whether the scout timer was actually running before proof: copied-save inspection found `active_sortie_started_minutes=-1`, `active_sortie_local_contact_minutes=-1`, and no active group/target/job ids for the stale inspected site, so pre-aged closure was rejected.
- [x] Bandit hold-off/stalking placement keeps a meaningful stand-off instead of crowding the immediately adjacent/neighboring OMT-scale area unless an explicit shakedown/fight state has started. _(Deterministic five-OMT correction plus live harness proof `.userdata/dev-harness/harness_runs/20260427_152117/`: real `wait_action` 30m path completed, `local_gate ... posture=hold_off ... standoff_distance=5`, `live_dispatch_goal=140,46,0`.)_
- [x] The scout timeout/return path is proven on the current live-product path. _(Current-runtime live run `.userdata/dev-harness/harness_runs/20260427_154309/`: real `wait_action` path logged `scout_sortie: linger limit reached -> return_home`, `scout_sortie: home footprint observed ... pos=(140,51,0)`, and `scout_report: returned -> pressure refreshed`; copied save later showed a fresh redispatch, not the stale returning-home loop.)_


Compact reference:
- Canonical contract lives at `doc/bandit-local-sight-avoid-and-scout-return-cadence-packet-v0-2026-04-26.md`.


---

## Smart Zone Manager v1 Josef playtest corrections

Status: CLOSED / LIVE COORDINATE-LABEL PROOF GREEN AFTER REOPEN

Success state:
- [x] Smart Zone Manager adds `LOOT_MANUALS` coverage on/near the Basecamp books cluster without removing ordinary `LOOT_BOOKS` coverage.
- [x] Gun-magazine coverage remains `LOOT_MAGAZINES`, preferably with an unambiguous user-facing label such as "Basecamp weapon magazines" if label text is touched.
- [x] Auto-eat and auto-drink coverage spans the full Basecamp storage zone, with `ignore_contents` explicitly false so Basecamp sorting still sees the covered items.
- [x] Deterministic tests assert the actual zone ids/types and the `ignore_contents == false` option, not just label text.
- [x] Save inspection confirms generated zones remain saved/reopenable and do not crash or corrupt the camp layout through zone-manager serialize/deserialize proof.
- [x] Live/player-visible layout no longer lumps intended-separate generated zones onto a single tile. The reopened coordinate-label proof `.userdata/smart-zone-audit-live-20260429e/harness_runs/20260429_225644/` opens the real Zone Manager, accepts Smart Zone generation, reopens the generated list, and captures row labels/absolute coordinates across the layout.
- [x] Deterministic tests assert zone geometry/separation and the intended-overlap allowlist, not only zone ids/options.
- [x] The implementation respects the smart-zone aux-plan separation rules that are deterministic-checkable in the fixture: fire-source / `splintered` / wood placement, readable crafting/food/equipment support separation, clothing/dirty support, outside rotten placement, and a larger unsorted intake area.
- [x] Clean live/UI proof or a precise Josef playtest package demonstrates the corrected layout without rerunning the contaminated old McWilliams/bandit macro. The old playtest package is superseded for the lumping/layout claim by the green coordinate-label proof; full process-reload disk persistence remains a separate optional future audit if promoted.


Compact reference:
- Canonical contract lives at `doc/smart-zone-manager-v1-josef-playtest-followup-2026-04-26.md`.


---

## Basecamp medical consumable readiness v0

Status: CLOSED / CHECKPOINTED

Success state:
- [x] Camp locker/service logic recognizes the practical bandage family, including at least `bandages`, `adhesive_bandages`, and medical gauze / obvious bandage-like wound-care stock when item definitions expose it cleanly, as bounded medical readiness supplies when stocking NPCs.
- [x] NPCs can pick up roughly 10 pieces/charges of bandage-family stock from the relevant Basecamp/locker storage path without hoarding all medical items.
- [x] Existing carried bandage-family stock is preserved through locker refresh and counts toward the reserve cap instead of being discarded as clutter.
- [x] Non-medical loadout behavior, ammo/magazine readiness, and clothing/armor selection remain unchanged.
- [x] Deterministic tests cover fresh pickup, carried-item preservation, cap/anti-hoarding behavior, and a negative case for unrelated drugs/items.
- [x] Live/harness proof is not required for this first slice; deterministic camp/locker tests exercise the actual service path and the focused `[camp][locker]` regression pass covers readiness behavior.


Compact reference:
- Canonical contract lives at `doc/basecamp-medical-consumable-readiness-v0-2026-04-26.md`.


---

## Basecamp locker armor ranking + blocker removal packet v0

Status: CLOSED / CHECKPOINTED

Success state:
- [x] A generic helper scores candidate protective/full-body gear against currently worn blockers using body-part priority, protection/coverage, encumbrance, condition, and active locker policy.
- [x] The helper is not item-id-specific and does not special-case `RM13 combat armor`.
- [x] When a candidate is clearly superior, the locker path removes/drops the blocking worn items needed to equip it.
- [x] When a candidate is not clearly superior or cannot be equipped, the locker path stops retrying the same failed swap and does not produce visible repeated wear spam.
- [x] Existing superior-full-body and ballistic-maintenance tests are reused/extended, including positive and negative cases.
- [x] Tests prove a clearly superior full-body/protective suit can displace worse blockers, while stronger current ballistic armor is preserved against worse candidates.
- [x] At least one targeted regression covers the original symptom shape without depending on the exact RM13 item ID as the only proof.

Notes:
- Canonical contract lives at `doc/basecamp-locker-armor-ranking-blocker-removal-packet-v0-2026-04-26.md`.
- Closure evidence: `make -j4 obj/basecamp.o tests/faction_camp_test.o tests LINTJSON=0 ASTYLE=0`, focused armor tests, `./tests/cata_test "[camp][locker]"` (2050 assertions in 70 test cases), and `git diff --check` passed.
- Josef explicitly said this must not be RM13-specific. Use a metric, not a charm against one cursed item.

---

## Basecamp job spam debounce + locker/patrol exceptions packet v0

Status: CLOSED / CHECKPOINTED

Success state:
- [x] Repeated Basecamp completion/missing-tool/no-progress messages are debounced by stable cause so they do not flood the visible log.
- [x] First occurrence and changed state still produce a visible/reportable message.
- [x] Locker-zone work has a typed exception path: real locker gear/readiness failures remain visible once with reason, while repeats are compressed.
- [x] Patrol-zone work has a typed exception path: real assignment/interruption/reserve/backfill changes remain visible once with reason, while repeats are compressed.
- [x] The debounce state does not survive in a way that hides messages forever after save/load or unrelated job changes.
- [x] Deterministic tests cover ordinary repeated spam, changed-state reset, locker exception, patrol exception, and a negative case showing unrelated important messages are not swallowed.
- [x] If practical, a harness/log proof shows the old spam shape is reduced without losing one meaningful locker/patrol message. Local deterministic/log-message gates were sufficient for this narrow message-debounce slice.

Notes:
- Canonical contract lives at `doc/basecamp-job-spam-debounce-exceptions-packet-v0-2026-04-26.md`.

---

## Bandit overmap/local handoff interaction packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded overmap/local handoff interaction packet exists on the current bandit playback / proof seam.
- [x] Deterministic proof shows overmap intent entering local play as posture rather than exact-square puppetry or stale route puppetry.
- [x] Deterministic proof shows local engagement, obvious danger, or contact loss can honestly rewrite, cool, or drop the prior posture instead of preserving immortal certainty.
- [x] At least one player-present context beyond a static toy camp is covered, for example basecamp pressure, follower travel, or vehicle / convoy interception.
- [x] Reviewer-readable output exposes the entry posture, any local rewrite, and the returned abstract-state change clearly enough to debug the seam without guessing from side effects.
- [x] The slice stays bounded: no full local combat AI rewrite, no coalition strategy layer, no broad visibility rewrite, and no magical omniscience by the back door.


Compact reference:
- Canonical contract lives at `doc/bandit-overmap-local-handoff-interaction-packet-v0-2026-04-21.md`.


---

## Bandit elevated-light and z-level visibility packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded elevated-light and z-level visibility packet exists on the current bandit playback / proof seam.
- [x] Deterministic proof shows nearby cross-z visibility behaves sensibly instead of collapsing into floor-bound amnesia.
- [x] Deterministic proof shows elevated exposed light can stay legible or actionable farther than ordinary hidden light under the right conditions without turning into magical global sight.
- [x] A flagship exposed-high-fire scenario, for example a radio-tower fire in a dead dark world, proves genuinely long-range visibility instead of timid toy-local range.
- [x] Matching deterministic scenarios prove the same meaningful elevated light can carry abstract zombie-horde pressure too instead of living in private bandit-only theater; this is **not** live horde attraction until the live light-to-horde bridge exists.
- [x] Deterministic proof shows smoke does **not** gain magical extra general reach merely from floor changes.
- [x] Reviewer-readable output exposes the visibility read and benchmark outcomes clearly enough that later playtesting can argue about tuning instead of first principles.
- [x] The slice stays bounded: no broad world visibility rewrite, no handoff redesign smuggled into the same packet, and no full zombie tactical sim.


Compact reference:
- Canonical contract lives at `doc/bandit-elevated-light-and-z-level-visibility-packet-v0-2026-04-21.md`.


---

## Bandit repeated-site revisit behavior packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded repeated-site follow-through exists on the current playback / evaluator footing, producing one more deliberate revisit / cautious-watch posture than plain early scout bookkeeping.
- [x] Deterministic coverage proves repeated same-site corroboration still does **not** unlock free `scavenge`, `steal`, or `raid` truth, and does not become immortal pressure.
- [x] Scenario `repeated_site_interest_stays_bounded` exposes the benchmark-facing long-horizon metrics reviewer-cleanly: `site_visit_count_500`, `site_revisit_count_500`, `cooldown_turn`, and `endless_pressure_flag`.
- [x] The honest `500`-turn proof shows the strengthened site cooling back out instead of regrowing forever.
- [x] The slice stays narrow: no site-type-sensitive branching, no settlement taxonomy pass, no broad visibility rewrite, and no z-level smuggling.


Compact reference:
- Canonical contract lives at `doc/bandit-repeated-site-revisit-behavior-packet-v0-2026-04-21.md`.


---

## Bandit overmap benchmark suite packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One complete named overmap benchmark-suite packet exists on the current bandit playback / proof seam.
- [x] Every required scenario carries a clear `100`-turn benchmark packet that is easy to read and easy to fail.
- [x] The scenarios that honestly need the longer horizon also carry `500`-turn carry-through checks instead of pretending `100` turns proves everything.
- [x] Reviewer-readable output explains why each scenario passed or failed, including timing / cadence / revisit counters where they matter, such as first non-idle turn, first actionable turn, first scout departure, first arrival, revisit count, and route-flip / back-and-forth count by turn `500`.
- [x] The explicit empty-frontier scenario proves that a camp with nothing useful nearby ventures out and increases frontier visibility through bounded scout/explore behavior instead of sitting forever, with the packet plainly reporting first scout, first arrival, visit/revisit totals, and obvious reversals.
- [x] The packet is reviewer-readable enough that Schani or Josef can answer plainly whether it is leiwand, actually fun, alive on the map, and showing real emergent activity rather than inert legal-but-boring behavior.
- [x] The slice stays bounded: no z-level implementation, no broad architecture rewrite, no vague benchmark theater, and no hand-waved passes when routing logic is still wrong.


Compact reference:
- Canonical contract lives at `doc/bandit-overmap-benchmark-suite-packet-v0-2026-04-21.md`.


---

## Bandit long-range directional light proof packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded long-range directional-light proof packet exists on the current bandit scenario / playback seam.
- [x] Deterministic multi-turn proof up to `500` turns shows the hidden-side leakage case stays non-actionable while the visible-side leakage case becomes actionable under the same broader footing.
- [x] The matching deterministic zombie-horde corridor variant proves the same light can carry abstract horde pressure too instead of existing in isolated bandit-only theater; this is **not** live horde attraction until the live light-to-horde bridge exists.
- [x] Each scenario carries explicit goals and tuning metrics, and reviewer-readable output shows whether those benchmarks were met.
- [x] The slice stays bounded: no z-level expansion, no broad light-system rewrite, no handoff redesign, and no fresh world-sim jump.


Compact reference:
- Canonical contract lives at `doc/bandit-long-range-directional-light-proof-packet-v0-2026-04-21.md`.


---

## Bandit overmap/local pressure rewrite packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded overmap/local pressure-rewrite proof packet exists on the current bandit scenario / playback footing.
- [x] Deterministic multi-turn proof shows a stalking or intercept posture can honestly cool, retreat, or reroute after local reality makes the original posture too dangerous.
- [x] Reviewer-readable output shows the pressure rewrite clearly enough to explain why the stale pursuit no longer holds.
- [x] Each scenario carries explicit goals plus scenario-specific benchmark hooks, and the later locked benchmark outcomes stay visible on the same report path.
- [x] The slice stays bounded: no broad handoff redesign, no tactical local combat AI expansion, and no fresh world-sim jump.


Compact reference:
- Canonical contract lives at `doc/bandit-overmap-local-pressure-rewrite-packet-v0-2026-04-21.md`.


---

## Bandit weather concealment refinement packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded weather-refinement packet exists on the current smoke/light mark-generation footing.
- [x] Deterministic coverage proves wind meaningfully fuzzes or de-precises smoke output instead of acting only as a token penalty.
- [x] Deterministic coverage proves portal-storm weather is handled explicitly for smoke and light instead of falling through as an unmodeled special case.
- [x] Reviewer-readable output explains how weather changed clue quality, for example reduced range, fuzzier origin, displaced/corridor-ish smoke read, or preserved bright-light legibility under dark storm conditions.
- [x] The slice stays bounded: no full plume physics, no global smoke sim, no sound-law rewrite, no z-level packet, and no broad visibility architecture rework.


Compact reference:
- Canonical contract lives at `doc/bandit-weather-concealment-refinement-packet-v0-2026-04-21.md`.


---

## Bandit bounded scout/explore seam v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One explicit bounded scout/explore option exists on the current dry-run evaluator seam, for map uncovering only.
- [x] Unreachable jobs still fail closed and do not auto-mint explore as a consolation prize.
- [x] Deterministic coverage proves the key bounded distinctions honestly: explicit explore can beat `hold / chill`, blocked routes do not create explore without explicit greenlight, and strong real reachable leads still outrank exploratory wandering.
- [x] Reviewer-readable dry-run output says plainly that the outing is explicit map uncovering and not accidental random wandering.
- [x] One small playback/reference scenario packet proves the same rule on the current scenario seam, with explicit goals and tuning metrics.
- [x] The slice stays bounded: no coalition logic, no fresh visibility family, no broad pathfinding rewrite, and no handoff expansion.


Compact reference:
- Canonical contract lives at `doc/bandit-bounded-scout-explore-seam-v0-2026-04-21.md`.


---

## Bandit concealment seam v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded concealment adapter exists on the current light signal seam, weakening outward light legibility when exposure is poor instead of inventing a second fake visibility machine.
- [x] Deterministic coverage proves the key bounded distinctions honestly: daylight suppression, weather penalty, containment, and side-dependent leakage/suppression.
- [x] Reviewer-readable output shows why a light-born mark was reduced, blocked, or allowed instead of hiding the answer in debugger soup.
- [x] The slice stays bounded: no broad all-signals concealment rewrite, no new fog-sound law, no global smoke/world simulation, no tactical stealth doctrine, and no pursuit/handoff expansion.
- [x] If the concealment adapter starts looking computationally suspicious, the packet carries one small readable cost/probe angle instead of deferring perf truth to later folklore.


Compact reference:
- Canonical contract lives at `doc/bandit-concealment-seam-v0-2026-04-21.md`.


---

## Bandit scoring refinement seam v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded scoring-refinement adapter exists on the current bandit dry-run/evaluator seam, refining how existing camp ledger state plus existing marks become job choice.
- [x] The threat side first inspects and collapses usable existing threat/danger footing instead of inventing a fresh bespoke threat ontology.
- [x] Deterministic coverage proves the key opportunism split honestly: bandits avoid strong opponents, but pounce when zombie pressure or other distraction lowers effective target coherence.
- [x] Reviewer-readable output shows the refined choice breakdown clearly enough to explain why a target was avoided, deferred, or exploited.
- [x] The slice stays bounded: no new visibility signal family, no broad heatmap/memory rewrite, no tactical zombie simulation, no coalition strategy layer, and no fresh world-sim expansion.


Compact reference:
- Canonical contract lives at `doc/bandit-scoring-refinement-seam-v0-2026-04-21.md`.


---

## Bandit moving-bounty memory seam v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded moving-bounty memory object exists for live `actor` or `corridor` style bounty, while structural bounty stays on site state instead of gaining chase memory.
- [x] Structural sites keep only cheap state such as harvested / recently-checked / false-lead / sticky-threat footing, with no stalking logic glued onto them.
- [x] Deterministic coverage proves the key bounded pursuit split honestly: live moving prey can be stalked briefly, but the memory collapses cleanly on lost contact, split, bad recheck, rising threat, or leash expiry instead of retrying forever.
- [x] Reviewer-readable output shows whether a moving lead was refreshed, narrowed, or dropped instead of hiding the memory state in debugger soup.
- [x] The slice stays computationally cheap: no per-turn tracking, no path-history scrapbook, no per-NPC biography graph, no endless retry loop, and no broad memory-palace world model.


Compact reference:
- Canonical contract lives at `doc/bandit-moving-bounty-memory-seam-v0-2026-04-21.md`.


---

## Bandit first 500-turn playback proof v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] At least one honest deterministic 500-turn bandit playback packet exists on the current abstract seams.
- [x] Reviewer-readable report output exposes long-horizon checkpoints and winner drift cleanly enough to inspect the 500-turn answers without debugger soup.
- [x] Deterministic coverage proves the chosen long-horizon scenarios stay bounded, including cooldown, peel-off, and repeated-site reinforcement behavior that does not magically harden into free raid truth.
- [x] The proof reuses the current mark-generation / playback / evaluator seams instead of smuggling in a broader overmap simulator or persistence rewrite.
- [x] The slice stays bounded: no new visibility adapter family, no live-harness-first theater, and no broad AI architecture jump.


Compact reference:
- Canonical contract lives at `doc/bandit-first-500-turn-playback-proof-v0-2026-04-20.md`.


---

## Bandit repeated site activity reinforcement seam v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] A bounded repeated-site reinforcement adapter exists from mixed repeated smoke/light/traffic footing into modest site-mark confidence and bounty amplification.
- [x] Reinforced site marks feed the current bandit mark-generation / playback / evaluator seams reviewer-cleanly instead of staying hand-authored lore.
- [x] Deterministic coverage proves the key bounded distinctions: mixed repeated signals reinforce one site mark cleanly, weak repetition does not fake durable settlement truth, self-corroboration stays bounded, and strengthened site interest still does not unlock free extraction jobs.
- [x] Reviewer-readable report output exposes the reinforcement packet and resulting mark/lead path instead of hiding the bridge in debugger soup.
- [x] The slice stays bounded: no smoke/light/human-route rewrite, no broad concealment implementation, no settlement-signature mythology, and no first 500-turn proof smuggled in.


Compact reference:
- Canonical contract lives at `doc/bandit-repeated-site-activity-reinforcement-seam-v0-2026-04-20.md`.


---

## Bandit human / route visibility mark seam v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] A bounded human / route adapter exists from direct sightings and repeated route-shaped activity, or equivalent deterministic route-visibility packets, into coarse overmap-readable moving-carrier, corridor, or bounded site signal state.
- [x] Generated human / route marks or leads feed the current bandit mark-generation / playback / evaluator seams reviewer-cleanly instead of staying hand-authored lore.
- [x] Deterministic coverage proves the key bounded distinctions: direct sighting versus repeated corridor activity, moving-carrier attachment versus site inflation, self-camp routine suppression, and at least one corroborated shared-route refresh case.
- [x] Reviewer-readable report output exposes the route packet and resulting mark/lead path instead of hiding the bridge in debugger soup.
- [x] The slice stays bounded: no light/smoke rewrite, no broad concealment implementation, no settlement-signature mythology, and no first 500-turn proof smuggled in.


Compact reference:
- Canonical contract lives at `doc/bandit-human-route-visibility-mark-seam-v0-2026-04-20.md`.


---

## Bandit light visibility mark seam v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] A bounded light-specific adapter exists from current local light and directional-exposure footing, or equivalent deterministic light packets, into coarse overmap-readable light signal state.
- [x] Generated light marks or light-born leads feed the current bandit mark-generation / playback / evaluator seams reviewer-cleanly instead of staying hand-authored lore.
- [x] Deterministic coverage proves the key bounded distinctions: daylight suppression, contained versus exposed night light, ordinary occupancy light versus searchlight-like threat light, and at least one side-dependent leakage case.
- [x] Reviewer-readable report output exposes the light packet and resulting mark/lead path instead of hiding the bridge in debugger soup.
- [x] The slice stays bounded: no smoke rewrite, no broad concealment implementation, no sound/horde expansion, and no first 500-turn proof smuggled in.


Compact reference:
- Canonical contract lives at `doc/bandit-light-visibility-mark-seam-v0-2026-04-20.md`.


---

## Bandit mark-generation + heatmap seam v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] A bounded overmap-side mark ledger and broad bounty/threat heat-pressure seam exist for deterministic bandit inputs.
- [x] Deterministic coverage proves mark creation, refresh, selective cooling, and sticky confirmed threat on named reference cases.
- [x] The existing evaluator / playback footing can consume generated mark output reviewer-cleanly instead of relying only on hand-authored leads.
- [x] The slice stays bounded: no bubble handoff, no broad visibility adapter, and no full hostile-world simulation are smuggled in.


Compact reference:
- Canonical contract lives at `doc/bandit-mark-generation-heatmap-seam-v0-2026-04-20.md`.


---

## Bandit overmap-to-bubble pursuit handoff seam v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded pursuit / investigation handoff exists from abstract overmap group state into local play.
- [x] The return path preserves meaningful abstract consequences such as updated mark/threat knowledge, losses, panic, cargo, or retreat state instead of dropping them on the floor.
- [x] Entry payload and return packet stay explicit, small, and reviewer-readable.
- [x] The slice stays bounded: no full raid / ambush suite, no broad tactical AI rewrite, and no full per-bandit biography persistence are smuggled in.


Compact reference:
- Canonical contract lives at `doc/bandit-overmap-to-bubble-pursuit-handoff-seam-v0-2026-04-20.md`.


---

## Locker lag-threshold probe v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One honest threshold packet exists for the real `CAMP_LOCKER` service path.
- [x] The packet distinguishes top-level item pressure from worker-count pressure instead of flattening them together.
- [x] The result can name an approximate fine / suspicious / bad range, or honestly report that no clear threshold was found within the tested bound.
- [x] If the threshold looks bad, the packet ends with a small cheap-first guardrail recommendation order instead of architecture opera, and if it does not, the packet says so plainly.


Compact reference:
- Canonical contract lives at `doc/locker-lag-threshold-probe-v0-2026-04-20.md`.


---

## Bandit evaluator dry-run seam v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] A deterministic dry-run evaluator exists for controlled bandit camp inputs.
- [x] The evaluator always includes `hold / chill` and only emits outward candidates from real compatible leads or current hard state.
- [x] The first explanation surface shows leads considered, the full candidate board, per-candidate score inputs/final score, veto/soft-veto reasons, and winner versus `hold / chill`.
- [x] Narrow deterministic coverage exists for the first pure reasoning reference cases.
- [x] The slice stays bounded: no full autonomous bandit world behavior, no broad scenario playback suite, and no broad persistence architecture are smuggled in.


Compact reference:
- Canonical contract lives at `doc/bandit-evaluator-dry-run-seam-v0-2026-04-20.md`.


---

## Bandit scenario fixture + playback suite v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] Named deterministic bandit scenarios exist for the first reference cases.
- [x] The playback contract can inspect behavior at multiple checkpoints such as `tick 0`, `tick 5`, `tick 20`, and one longer horizon.
- [x] The scenario packet can answer whether camps stay idle, investigate smoke, stalk edges, peel off under pressure, or mis-upgrade whole regions from moving clues.
- [x] The suite stays bounded and does not turn into broad worldgen mutation or live-harness-first theater.


Compact reference:
- Canonical contract lives at `doc/bandit-scenario-fixture-playback-suite-v0-2026-04-20.md`.


---

## Bandit perf + persistence budget probe v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] Repeatable cost measurements exist for the named bandit scenarios.
- [x] Obvious evaluator churn signals such as candidate-count growth, repeated scoring/path checks, or similar waste are visible instead of hidden.
- [x] Save-size growth has an honest first estimate tied to the actually persisted bandit state shape.
- [x] The packet can say whether the current design looks cheap enough, suspicious, or clearly too bloated before broader rollout.


Compact reference:
- Canonical contract lives at `doc/bandit-perf-persistence-budget-probe-v0-2026-04-20.md`.


---

## Locker clutter / perf guardrail probe v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One honest measurement packet exists for the real `CAMP_LOCKER` service path across a first bounded clutter sweep such as `50 / 100 / 200 / 500 / 1000` top-level items.
- [x] The packet distinguishes realistic worker-count pressure from item-hoard pressure instead of pretending twenty-to-fifty camp workers are the main threat.
- [x] The packet answers whether loaded magazines and ordinary container shapes mostly behave like one top-level locker item or create meaningful nested-content pain.
- [x] The packet can end with a usable verdict: fine for now, watch this, or land a guardrail now, plus the first cheap mitigation order if needed.


Compact reference:
- Canonical contract lives at `doc/locker-clutter-perf-guardrail-probe-v0-2026-04-20.md`.


---

## Controlled locker / basecamp follow-through packet

Status: CHECKPOINTED / PACKAGE 5 DONE FOR NOW

Success state:
- [x] **Package 1, harness zone-manager save-path polish** is landed with screenshots/artifacts from the current McWilliams harness path.
- [x] **Package 2, basecamp toolcall routing fix** is landed or honestly blocked, and the right discriminator is separated from the bad location-only heuristic.
- [x] **Package 3, locker outfit engine hardening** is now landed for the current required closeout slice: the weird board/log leak stays re-proved live on the rebuilt current tiles binary, the in-game-log plus `llm_intent.log` observability helper is exercised on that same live probe path, and the required deterministic/service-parity canon is closed through the outer one-piece civilian-garment seam (`abaya`) without reopening open-ended clothing-case collection.
- [x] **Package 5, basecamp carried-item dump lane** is landed: ordinary carried junk gets dumped during locker dressing, the kept carried lane is intentionally limited to `bandages`, `ammo`, and `magazines`, and curated locker stock is not polluted by the dump behavior.
- [x] The queue stayed controlled while Package 5 ran, and the next slice can now move forward cleanly as Package 4 instead of broadening into unrelated lanes.



---

## Patrol Zone v1

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] A Zone Manager patrol zone exists and patrol squares are grouped by 4-way connected clusters.
- [x] Patrol exists as a real camp job with its own priority surface.
- [x] Deterministic planner coverage exists for the reference staffing/topology cases.
- [x] The active patrol roster is chosen at shift boundaries from NPCs with patrol priority > 0.
- [x] On-shift patrol is sticky against routine chores.
- [x] Urgent disruption can break patrol, and reserve backfill works without full-roster reshuffle.
- [x] On-map behavior distinguishes hold-positions vs fixed-loop patrol in the intended simple v1 way.
- [x] Proportional live proof is recorded with separate screen/tests/artifacts reporting.
- [x] The player-legibility bar is met: guard behavior, uncovered posts, connected-vs-disconnected behavior, and reserve/off-shift state are understandable enough to read in play.
- [x] The result stays explainable as simple v1 patrol rather than quietly turning into smart-zone-manager soup.



---

## Smart Zone Manager v1

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One explicit one-off smart-zoning action exists for Basecamp.
- [x] The v1 creates exactly one crafting niche, one food/drink niche, and one equipment niche.
- [x] Support placement exists for clothing, dirty, rotten, unsorted, and blanket/quilt-on-beds.
- [x] The corrected fire layout is implemented: fire tile = `SOURCE_FIREWOOD`, adjacent `splintered`, nearby wood.
- [x] Anchor selection is honest about the current shape: flag-first where the map exposes real signals, small explicit id allowlists where those signals are thin, and floor fallback instead of clever failure.
- [x] Existing sorting/subcategory machinery is reused by default, with only the deliberate v1 custom filters (`splintered`, `dirty`, `rotten`, `blanket`, `quilt`) kept as explicit exceptions.
- [x] Placement is deterministic and non-destructive by default.
- [x] Deterministic tests exist for anchor choice / zone choice / no-destructive-overwrite behavior.
- [x] Proportional live proof is recorded on the rebuilt current tiles binary.


Compact reference:
- Canonical contract lives at `doc/smart-zone-manager-v1-aux-plan-2026-04-06.md`.


---

## Locker-capable harness restaging

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] A fixture or restaging path exists that contains a real `CAMP_LOCKER` zone and suitable locker-state/NPC-state for `locker.weather_wait`.
- [x] `locker.weather_wait` is no longer blocked on missing fixture shape.
- [x] A fresh packaged `locker.weather_wait` run reports **screen** / **tests** / **artifacts** separately on the repaired fixture path.
- [x] The result is described reviewer-cleanly as harness/fixture work on existing locker behavior, not as premature delivery of the later chat/ambient feature lanes.



---

## Stabilization + harness runway

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] The currently claimed locker baseline is honest enough to build on: V1/V2 stay trusted, and the active V3 slice is either properly evidenced or explicitly demoted back to the honest current state.
- [x] At least one reliable harness-driven live probe path exists on the current binary/profile/save path without stale-binary ambiguity.
- [x] That harness path reports screen/tests/artifacts as separate evidence classes instead of flattening them into one vague verdict.
- [x] At least one high-value reusable playtest scenario is documented/packaged for Schani-assisted probing instead of re-invented manually each time.
- [x] The packaged harness path no longer treats the first `lastworld.json` flip as sufficient proof of post-load gameplay readiness.
- [x] `chat.nearby_npc_basic` records recipient / artifact proof instead of only reaching dialogue and freeform-input UI.
- [x] A second named scenario contract (`ambient.weird_item_reaction`) exists and is honest about its current footing: runnable on the shipped fixture, still lacking real reaction/artifact proof.
- [x] At least one reusable scenario-setup helper exists so repeated probes stop depending on debug-menu folklore.
- [x] A compact Josef-facing testing packet exists for the pre-holiday active-testing window.



---

## Locker Zone V2

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] Locker service can select up to two compatible magazines from locker supply when a managed ranged loadout needs them.
- [x] Locker service can reload supported ranged weapons from locker-zone ammo supply.
- [x] V2 ranged-readiness logic stays basic and deterministic instead of ballooning into V3 fashion/season nuance.
- [x] Deterministic coverage exists for the V2 ranged readiness / reload behavior.
- [x] Proportional runtime validation for V2 is recorded in `TESTING.md`.
- [x] Any Josef-specific follow-up checks are written down as non-blocking notes rather than treated as plan blockers.



---

## Locker Zone V1

Status: CHECKPOINTED / DONE FOR NOW



---

## Locker Zone V3

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] Seasonal dressing / winter-vs-summer wardrobe logic exists.
- [x] Weather-sensitive wardrobe choices (coats / blankets / shorts / similar) are handled deliberately rather than by V1/V2 shortcuts.
- [x] Per-NPC overrides / nuance exist without undoing the simpler V1/V2 deterministic spine.
- [x] Deterministic coverage exists for the V3 behavior that is actually implemented.
- [x] Proportional runtime validation for the currently implemented V3 behavior is recorded in `TESTING.md`.
- [x] Any Josef-specific follow-up checks are written down as non-blocking notes rather than treated as plan blockers.


Compact reference:
- Canonical contract lives at `doc/locker-zone-v3-reopen-packet-v0-2026-04-21.md`.


---

## Basecamp AI capability audit/readout packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded capability audit/readout packet exists for the current Basecamp AI surface.
- [x] The packet distinguishes player-facing spoken behaviors from internal structured actions/tokens instead of mushing them together.
- [x] The packet says plainly what board/job actions are actually supported now.
- [x] The packet says plainly whether any prompt-shaped interpretation layer still matters, or whether the current behavior is already mostly deterministic plumbing.
- [x] The packet is grounded in current code/tests/evidence strongly enough that later cleanup decisions do not rely on stale folklore.
- [x] The slice stays bounded and does not mutate into implementation work by accident.



---

## Live bandit + Basecamp playtesting feasibility probe v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded live playtesting feasibility packet exists for current bandits plus current Basecamp footing.
- [x] The packet says plainly whether live playtesting is already practical on the current tree.
- [x] The packet says plainly whether overmap-bandit setup/spawn control is currently available, narrowly restageable, or still missing.
- [x] The packet separates screen/tests/artifacts cleanly instead of flattening them into one soup verdict.
- [x] Any blocker is stated as a concrete missing setup/control path rather than vague playtesting hand-wringing.
- [x] The slice stays bounded and does not turn into open-ended live playtesting theater.


Compact reference:
- Canonical contract lives at `doc/live-bandit-basecamp-playtesting-feasibility-probe-v0-2026-04-21.md`.


---

## Live bandit + Basecamp playtest packaging/helper packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded helper or first-class harness scenario exists for current-build live bandit + Basecamp restage on real existing Basecamp footing.
- [x] The helper packages the already-proved named-bandit restage seam instead of relying on remembered debug-menu choreography alone.
- [x] One fresh live run proves the helper can reach the intended setup repeatably and leaves reviewer-readable artifacts.
- [x] The packet says plainly what remains manual or ugly, if anything, instead of laundering it into magic.
- [x] The slice stays bounded and does not widen into fresh mechanics, zoning-mechanics reopen, encounter/readability judgment, or another generic feasibility lap.


Compact reference:
- Canonical contract lives at `doc/live-bandit-basecamp-playtest-packaging-helper-packet-v0-2026-04-22.md`.


---

## Bandit + Basecamp first-pass encounter/readability packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded first-pass live encounter/readability packet exists for current bandit + Basecamp footing on top of the packaged helper path.
- [x] The packet says plainly what the first hostile encounter actually looks and feels like on screen, including whether the player can read what is happening without debug folklore.
- [x] The packet distinguishes readable gameplay pressure from leftover helper/debug awkwardness instead of mushing them together.
- [x] Any blocker is stated concretely, for example spawn weirdness, posture confusion, unreadable hostility, dead pacing, or Basecamp-context ambiguity.
- [x] The packet leaves reviewer-readable artifacts strong enough that Schani can make the next higher-level product read without reinventing the probe from memory.
- [x] The slice stays bounded and does not widen into fresh mechanics, broad tuning, or speculative feature expansion.


Compact reference:
- Canonical contract lives at `doc/bandit-basecamp-first-pass-encounter-readability-packet-v0-2026-04-22.md`.


---

## Bandit + Basecamp playtest kit packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] The current named-bandit helper path has honest repeatability evidence instead of one lucky run.
- [x] The artifact/report surface for that helper path is screen-first and reviewer-readable enough that later playtest discussion does not depend on folklore.
- [x] The helper path cleans up after itself well enough that repeated probes do not leave stale game windows or obvious session clutter behind.
- [x] One purpose-built playtest fixture/save pack exists with a fast reinstall/load path for current bandit + Basecamp probing.
- [x] That fixture/save pack exposes a small named scenario family suitable for Josef playtesting and debugging rather than one brittle operator-only restage ritual.
- [x] The packet says plainly what still remains ugly or manual instead of laundering it into magic.
- [x] The slice stays cohesive and bounded rather than mutating into a general harness/world-authoring rewrite.


Compact reference:
- Canonical contract lives at `doc/bandit-basecamp-playtest-kit-packet-v0-2026-04-22.md`.


---

## Bandit + Basecamp playtest kit packet v1

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] A small named family of rich prepared-base fixtures exists for current Basecamp-management and bandit-interplay probing.
- [x] The fixtures are meaningfully management-ready rather than empty camp shells.
- [x] The player-side debugging/readability footing includes both clairvoyance mutations.
- [x] The fixture method is honest and reusable: captured save for structure, live restage for variants.
- [x] The scenario family is reviewer-readable enough that Schani, Andi, and Josef can tell which fixture is for which kind of playtest without folklore.
- [x] The packet says plainly what still remains manual, brittle, or not yet fixture-backed.
- [x] The slice stays about rich prepared fixtures rather than sprawling into a generic world-authoring platform.


Compact reference:
- Canonical contract lives at `doc/bandit-basecamp-playtest-kit-packet-v1-2026-04-22.md`.


---

## Bandit live-world control + playtest restage packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One live saveable owner exists for the relevant current bandit spawn families tied to this lane instead of leaving them as disconnected static-content / mapgen islands.
- [x] The owner keeps explicit per-site and per-spawn-tile bandit headcounts plus membership strongly enough that later control and writeback are honest rather than guessed.
- [x] The live system can actually control or dispatch those spawned bandits through the real world path instead of leaving them as disconnected `place_npcs` islands with post-hoc bookkeeping or whole-camp outings.
- [x] Dispatch size is derived from the site's live remaining population strongly enough that site-backed camps keep a home presence and later losses/shrinkage reduce future outing size instead of snapping back to folklore counts.
- [x] A bounded nearby restage path can seed a controlled bandit camp about `10 OMT` away on current build.
- [x] That restage path supports two honest modes: reviewer probe/capture may clean up, while manual playtest handoff leaves the game/session running instead of auto-terminating it.
- [x] The setup exercises the real overmap/bubble handoff plus local writeback path rather than a fake private theater.
- [x] Local outcomes change later world behavior instead of leaving the live owner stale, including later site size / dispatch capacity after kills, losses, returns, or partial contact.
- [x] A reviewer-clean perf report exists on that nearby live setup, including baseline turn time, bandit-cadence turn time, spike ratio, and max turn cost.
- [x] At least one dirtier later-world disturbance proof exists on that same nearby owned setup beyond the calm return->re-dispatch path, such as loss/missing shrinkage, save/load disturbance, or player-disruption without stale-roster reset.
- [x] The slice stays bounded: no giant generic map-authoring empire, no full hostile-human rewrite, no fake harness-only integration, and no faction-grand-strategy detour.


Compact reference:
- Canonical contract lives at `doc/bandit-live-world-control-playtest-restage-packet-v0-2026-04-22.md`.


---

## Multi-site hostile owner scheduler packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] The live hostile-owner seam can run `2-3` simultaneous hostile sites on one world without collapsing them into one shared fake camp brain.
- [x] Each hostile site keeps its own anchor/home site, roster/headcount, active outing or contact state, remembered pressure/marks, and persisted writeback state.
- [x] Site-backed camps still keep a home presence while losses continuously shrink later outings instead of snapping back to folklore counts.
- [x] One site's losses, return state, or contact pressure do not silently rewrite another site's state.
- [x] Repeated same-target pressure can overlap occasionally without turning into routine multi-site dogpile coalition behavior.
- [x] Save/load stays honest for multiple hostile owners at once instead of only for the single easiest happy-path site.
- [x] The slice stays bounded: no faction grand strategy, no dozens-of-families explosion, and no magical shared omniscience.


Compact reference:
- Canonical contract lives at `doc/multi-site-hostile-owner-scheduler-packet-v0-2026-04-22.md`.


---

## Hostile site profile layer packet v0

Status: CLOSED / CHECKPOINTED

Success state:
- [x] One shared hostile owner/cadence/persistence substrate exists with explicit profile rules instead of hardcoding everything to current bandit-camp assumptions.
- [x] At minimum one camp-style profile and one smaller hostile-site profile can coexist on that substrate reviewer-cleanly.
- [x] Dispatch, threat posture, return-clock, and writeback differences are profile-driven and readable rather than site-kind spaghetti.
- [x] The multi-site scheduler can consume those profiles without regressing the already-honest bandit live-owner footing.
- [x] The packet stays bounded: no giant faction-AI framework, no singleton stalker implementation, and no broad diplomacy/social-horror widening.


Compact reference:
- Canonical contract lives at `doc/hostile-site-profile-layer-packet-v0-2026-04-22.md`.


---

## Cannibal camp attack-not-extort correction v0

Status: CHECKPOINTED / DONE FOR NOW.


Compact reference:
- Canonical contract lives at `doc/cannibal-camp-attack-not-extort-correction-v0-2026-04-24.md`.
- Canonical contract lives at `doc/cannibal-camp-first-hostile-profile-adopter-packet-v0-2026-04-22.md`.


---

## Bandit approach / stand-off / attack-gate packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One honest local approach / stand-off / attack-gate layer exists on top of the live bandit control seam.
- [x] Dispatched bandit groups can reviewer-readably choose among stalking, holding off, probing, opening a shakedown, attacking directly, or aborting.
- [x] The gate law is explicit enough that later packet work can answer `why did this become a shakedown instead of a fight` without folklore reconstruction.
- [x] Ordinary Basecamp/player pressure does not feel like instant psychic tile collapse because bandits can keep bounded stand-off behavior.
- [x] Convoy / vehicle / rolling-travel contexts are allowed to skip the polite shakedown posture when they honestly read as moving ambush opportunities on a real or harnessed travel seam.
- [x] The slice stays bounded: no pay-or-fight UI yet, no giant stealth doctrine, no radio/stalker widening, and no broad combat-AI rewrite.


Compact reference:
- Canonical contract lives at `doc/bandit-approach-stand-off-attack-gate-packet-v0-2026-04-22.md`.


---

## Bandit shakedown pay-or-fight surface packet v0

Status: CLOSED / CHECKPOINTED

Success state:
- [x] One honest player-present shakedown scene can bootstrap from the prior approach/gate packet instead of appearing as disconnected chat magic.
- [x] The initial interaction is clearly a robbery demand with a readable `pay` versus `fight` fork.
- [x] Whenever this shakedown surface is invoked, including any later reopened demand, fighting remains an explicit option rather than disappearing into one-way surrender theater.
- [x] The surrender surface uses an explicit bounded goods pool that matches scene context rather than magical remote inventory reach.
- [x] The demanded toll is painful enough to read like a real bandit shakedown instead of a decorative nuisance fee.
- [x] Paying can resolve the immediate scene without requiring perfect long-tail cargo simulation, because surrendered goods can collapse into bandit bounty/writeback honestly.
- [x] The slice stays bounded: no branching diplomacy opera, no fake debt economy, no magical remote inventory, and no unrelated convoy-combat rewrite.


Compact reference:
- Canonical contract lives at `doc/bandit-shakedown-pay-or-fight-surface-packet-v0-2026-04-22.md`.


---

## Bandit aftermath / renegotiation writeback packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One honest aftermath/writeback layer exists for player-present bandit shakedown outcomes.
- [x] Later bandit behavior can reflect scene results such as losses, wounds, loot haul, failed extraction, anger, or caution instead of resetting to folklore.
- [x] A materially weakened Basecamp defense, including a killed defender, can reopen the pressure once from a stronger bandit position with a higher demand when that is the honest read.
- [x] That harsher reopened demand still gives the player a fresh explicit `pay` versus `fight` reconsideration fork instead of hard-forcing only surrender or only combat.
- [x] The harsher reopened demand is explicit and reviewer-readable rather than hidden in vague score drift.
- [x] Bandit losses or panic can also cool or shrink later pressure, so the packet does not only ratchet cruelty upward.
- [x] The slice stays bounded: no infinite haggling loops, no giant diplomacy/reputation machinery, and no multi-camp retaliation grand strategy.


Compact reference:
- Canonical contract lives at `doc/bandit-aftermath-renegotiation-writeback-packet-v0-2026-04-22.md`.


---

## Bandit extortion-at-camp restage + handoff packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One named restage path can attract a real controlled bandit group toward Basecamp through the actual live owner/dispatch seam.
- [x] One reviewer probe/capture command and one manual handoff command exist for that same path.
- [x] The handoff path leaves the session alive at a genuinely useful extortion-at-camp moment instead of cleaning up before play starts.
- [x] The setup does not rely on fake debug-spawn theater or on moved-player/basecamp hacks that break current camp validity.
- [x] Reviewer-readable reports make it obvious which setup mode ran, which real controlled group was used, and whether the scene reached approach, stand-off, or shakedown state.
- [x] The slice stays bounded: no generic harness empire and no helper polish masquerading as the robbery chain itself.


Compact reference:
- Canonical contract lives at `doc/bandit-extortion-at-camp-restage-handoff-packet-v0-2026-04-22.md`.


---

## Bandit extortion playthrough audit + harness-skill packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One named audit/playthrough surface exists for the full Basecamp extortion chain rather than only for the first setup moment.
- [x] The packaged flow can cover first demand, fight branch, defender-loss reopen, raised-price second demand, and second `pay` versus `fight` reconsideration clearly enough for review.
- [x] Reports separate screen, tests, and artifacts/logs cleanly enough that later debugging does not devolve into folklore.
- [x] The relevant harness skill/docs are updated so another agent can discover the named paths and run them without archaeological guessing.
- [x] The slice stays bounded: no fake total-automation empire and no feature-redesign side quest hiding inside audit packaging.


Compact reference:
- Canonical contract lives at `doc/bandit-extortion-playthrough-audit-harness-skill-packet-v0-2026-04-22.md`.


---

## Bandit + Basecamp playtest kit packet v2

Status: FOLDED INTO LATER ACTIVE LANE / SUPPORTING ONLY

Notes:
- Canonical helper contract still lives at `doc/bandit-basecamp-playtest-kit-packet-v2-2026-04-22.md`.
- The useful open scenario-surgery / observability work from `v2` was carried forward into `Bandit live-world control + playtest restage packet v0` instead of being killed.
- `v2` is therefore no longer a standalone roadmap item; it survives only as supporting tooling in service of the newer active lane.

---

## Post-Locker-V1 Basecamp follow-through

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] The live `DEBUG_LLM_INTENT_LOG` board/job artifact packet is made legible enough to stand beside the deterministic router proof.
- [x] The deterministic Basecamp board/job work is pruned/packaged into a cleaner upstream-ready shape.
- [x] The richer structured board/prompt treatment is extended beyond `show_board` in a deliberate next slice.
- [x] Proportional validation for each finished sub-slice is recorded in `TESTING.md`.
- [x] Any Josef-specific follow-up checks are written down as non-blocking notes rather than treated as plan blockers.



---

## LLM-side board snapshot path

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] Routing proof exists on the real `handle_heard_camp_request` path.
- [x] Structured/internal `show_board` emits the richer handoff snapshot.
- [x] Spoken `show me the board` stays on the concise spoken bark path.
- [x] Deterministic evidence for that split is recorded.



---

## Organic bulletin-board speech polish

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] Bulletin-board / camp-job requests can be triggered through natural player-facing phrasing instead of exposed machine wording.
- [x] Ordinary spoken answers no longer expose `job=<id>` / `show_board` / `show_job` style routing tokens.
- [x] Internal routing/debug structure can still exist where needed without leaking into normal in-world speech.
- [x] The visible answer tone sounds rough, practical, and in-world, like poor survivors making it work for another day while the dead and worse roam outside.


Compact reference:
- Canonical contract lives at `doc/organic-bulletin-board-speech-2026-04-09.md`.


---

## Combat-oriented locker policy

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] Future locker behavior strongly supports sensible common guard/combat gear: gloves, belts, masks, holsters, and the usual practical clothing/loadout pieces.
- [x] A bulletin-board / locker-surface bulletproof toggle exists and meaningfully shifts outfit preference toward ballistic gear.
- [x] Ballistic vest and plate handling becomes explicit enough to replace damaged (`XX`) ballistic components sensibly.
- [x] Clearly superior full-body battle/protective suits are preferred when appropriate instead of being split into worse piecemeal junk.
- [x] Future deterministic tests lean more toward combat/guard outfit behavior and less toward endlessly widening exotic garment edge-case law.


Compact reference:
- Canonical contract lives at `doc/locker-combat-oriented-policy-2026-04-09.md`.


---

## Bandit concept formalization follow-through

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] A follow-through packet exists that turns the loose remaining bandit logic into three explicit doc slices: bounty source/harvesting/stockpile rules, cadence/distance/fallback rules, and cross-layer interactions/worked scenarios.
- [x] Those three slices are further decomposed into narrow single-question micro-items so Andi can freeze one law at a time instead of hiding several assumptions inside one paragraph.
- [x] The packet makes the no-passive-decay footing explicit: structural bounty changes via harvesting/exploitation, moving bounty via current activity and collection, threat via real recheck, and camp stockpile via explicit consumption instead of background decay math.
- [x] Each micro-item includes a clear question plus a concrete answer shape, and the packet as a whole includes starter numbers/tables, clear scope/non-goals, and enough worked examples that later implementation planning does not have to rediscover the control law from scratch.
- [x] The result remains conceptualization/backlog work only and does not silently greenlight bandit implementation.


Compact reference:
- Canonical contract lives at `doc/bandit-concept-formalization-followthrough-2026-04-19.md`.


---

## Plan status summary command

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] A user can request a plan readout and see compact numbered `active`, `greenlit`, and `parked` summaries derived from current `Plan.md` canon.
- [x] The greenlit readout preserves execution order, with active first and any bottom-of-stack entries simply appearing last in that numbered list.
- [x] If canon headings are contradictory or missing enough structure to classify cleanly, the command warns instead of inventing certainty.
- [x] The output stays short and Discord-friendly rather than dumping whole roadmap prose.


Compact reference:
- Canonical contract lives at `doc/plan-status-summary-command-2026-04-20.md`.


---

## Bandit smoke visibility mark seam v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] A bounded smoke-specific adapter exists from current fire/smoke/wind footing, or equivalent deterministic smoke packets, into coarse overmap-readable smoke signal state for bandit logic.
- [x] Generated smoke marks or smoke-born leads feed the current bandit mark-generation / playback / evaluator seams instead of relying on hand-authored smoke lore.
- [x] Deterministic coverage proves the bounded long-range rule honestly: sustained clear-weather smoke stays meaningfully long-range, while weak/brief or degraded-condition smoke stays shorter and fuzzier.
- [x] Reviewer-readable report output exposes the smoke packet and resulting mark/lead path instead of hiding the bridge in debugger soup.
- [x] The slice stays bounded: no light/searchlight adapter, no broad visibility/concealment implementation, no global offscreen smoke sim, and no first 500-turn proof smuggled in.


Compact reference:
- Canonical contract lives at `doc/bandit-smoke-visibility-mark-seam-v0-2026-04-20.md`.


---

## Bandit overmap AI concept chain

Status: CHECKPOINTED / COHERENT PARKED SUBSTRATE

Success state:
- [x] The parked concept packet defines the broad bandit overmap actor model, signal/memory role, threat shape, and overmap-to-bubble intent cleanly enough that later planning no longer has to rediscover the premise from scratch.
- [x] The broad packet explicitly states that bounty and threat do not passively decay: structural bounty changes through harvesting/exploitation, moving bounty through fresh activity and collection, threat through real recheck, and camp stockpile through explicit consumption.
- [x] Deterministic bounty/threat scoring guidance exists with explicit camp-ledger inputs, map-mark fields, harvest/collection/recheck rules, and job-scoring logic.
- [x] Overmap mark-generation and heatmap guidance exists with explicit overmap-only mark creation, recheck/harvest rules, and threat/bounty field update logic on the shared cadence family without passive decay.
- [x] Bidirectional overmap-to-bubble handoff guidance exists with explicit entry modes, return-path state, collapse-back rules, and reuse of existing pursuit/noise-routing footing where possible.
- [x] Supporting physical-systems recon exists so the visibility/concealment slice is grounded in existing smoke, light, sound, and weather hooks instead of made-up parallel physics.
- [x] Player/basecamp visibility and concealment guidance exists with explicit signal sources, environmental filters, bounty/threat interpretation outputs, repetition/reinforcement rules, and player/basecamp exposure-reduction levers.
- [x] The visibility item explicitly allows fog/mist to suppress sight/light legibility without requiring new fog-based sound dampening rules unless later evidence says otherwise.
- [x] The broad packet explicitly prevents suspicion spirals by making camp knowledge sparse and camp-owned, ground bounty coarse/finite/non-regenerating, moving bounty actor-driven, and checked/false-lead/harvested memory able to damp repeated interest.
- [x] The broad packet explicitly resolves basecamp fairness asymmetry by allowing sustained offscreen pressure and stalking, keeping decisive full camp assault player-present only for current scope, and not requiring attack presignaling as the fairness mechanism.
- [x] The packet explicitly names the current overmap-NPC persistence/travel/companion substrate as reusable footing for stalking and intercept pressure, without pretending the existing need-driven random-NPC policy is already the finished hostile model.
- [x] The broad packet explicitly resolves handoff identity continuity by making the overmap group itself persistent, carrying only a small anchored-individual slice directly across handoffs, and treating the rest of the group as fungible mission strength.
- [x] The broad packet explicitly treats smoke/mark destinations as provisional mission leads whose goals can be continued, diverted, shadowed, or aborted by local observations instead of sacred tile commitments.
- [x] The broad packet explicitly resolves city opportunism under zombie pressure by allowing occasional risky opportunism without requiring direct bandit-versus-zombie tactical simulation, while keeping repeat attractiveness bounded by depleting bounty and sticky threat memory.
- [x] The broad packet explicitly keeps threat marks sticky enough that bands do not cheaply remote-rewrite a scary area as safe again until they return close enough to genuinely reassess it.
- [x] The broad packet explicitly resolves multi-camp dogpile behavior by keeping camps mostly independent in v1, allowing occasional overlap without turning it into routine coalition swarming.
- [x] The broad packet explicitly uses territoriality, distance burden, depletion, sticky threat, and fresh active-pressure penalties to damp repeated multi-camp convergence on the same target region.
- [x] The whole bandit concept packet is now coherent enough that it can support bounded promotion into greenlit implementation slices without hidden open seams.



---

## Arsenic-and-Old-Lace social threat and agentic-world concept bank

Status: CHECKPOINTED / FAR-BACK PARKED SUBSTRATE

Success state:
- [x] One auxiliary concept-bank doc preserves the current speculative C-AOL feature families cleanly enough that later planning no longer has to rediscover them from scattered chat.
- [x] The preserved bank explicitly separates nearer playable threat/control seeds from broader social-horror and weird-world systemic ideas instead of flattening everything into one soup.
- [x] The bank explicitly preserves alarm states with natural-language yelling control, radio information warfare, writhing-stalker pressure, bounded sight avoidance, feral-camp pressure, social camouflage, hidden-psychopath survivor play, faction mythmaking, living camp mood/fracture, agentic NPC goals, and interdimensional-traveler motive hooks.
- [x] `Plan.md` points to the bank as a far-back parked concept substrate rather than as an active or greenlit implementation lane.
- [x] The preserved bank explicitly states that current bandit and Basecamp zoning footing still needs honest playtesting before these concepts earn bounded promotion.
- [x] The bank explicitly requires future revisit to happen one bounded promotion at a time instead of reopening fifty speculative threads at once.



---

## Plan/Aux pipeline helper

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] A small helper can take a proposed item/greenlight and print the contract back for verification before canon files are changed.
- [x] The helper can collect corrections and then classify the item cleanly as active, parked, or bottom-of-stack.
- [x] The helper can update the relevant canon files consistently (`Plan.md`, `TODO.md`, `SUCCESS.md`, `TESTING.md` when needed, plus the auxiliary doc).
- [x] The helper reduces manual file carpentry for already-understood greenlights without bypassing the frozen workflow.
- [x] The helper can optionally generate the Andi handoff packet from the same classified contract.


Compact reference:
- Canonical contract lives at `doc/plan-aux-pipeline-helper-2026-04-09.md`.
