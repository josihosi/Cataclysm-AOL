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

No active live GUI probe is required for the completed debug-proof finish stack. Deterministic-only evidence remains insufficient for future product claims, but the reopened bandit local standoff / scout return correction now has current-runtime live proof. Do not spin another bandit player-fire loop or another Smart Zone GUI macro unless Josef/Schani explicitly reopens the item or the harness primitive materially changes.

Active process validation target: **C-AOL harness trust audit + proof-freeze packet v0**. Inventory checkpoint `doc/c-aol-harness-trust-audit-inventory-v0-2026-04-27.md` identifies the harness primitive/scenario/fixture surface and selects `dev-harness` / `McWilliams` with `mcwilliams_live_debug_2026-04-07` as the provisional canonical audit anchor. This is inventory/provenance evidence only, not a feature-proof freeze.

Current process-audit evidence: `startup_harness.py start` writes `startup.step_ledger.json`, embeds `startup_step_ledger` in `startup.result.json`, and marks successful load/readiness as `evidence_class=startup/load`, `feature_proof=false`. Clean startup/load may be green as startup evidence, but still never feature proof by itself. Probe classification now requires `startup_clean_for_feature_steps=true` before `artifacts_matched` can become `feature_proof=true`; stale runtime/profile/load-readiness problems force non-green `startup/load-or-inconclusive`. Dry-run start reports `dry_run_contract.evidence_class=plan-only` and does not install fixtures/profile snapshots. Canonical startup audit run `.userdata/dev-harness/harness_runs/20260427_164639/` reached load/readiness with green plan/focus/load rows, but the screenshot row is yellow because the window title captured stale runtime `84b1e3a604-dirty` while repo HEAD is `821cbf01c6` and runtime-relevant diff includes `src/do_turn.cpp`; this is exactly a non-green startup/load proof, not feature proof.

Step-local proof-freeze slice landed in `startup_harness.py`: live probe/handoff runs now write `<mode>.step_ledger.json`, embed `step_ledger` and `step_ledger_summary` in the report, and block `feature_proof=true` unless every scenario step has a green step-local ledger row in addition to clean startup, artifact matches, and no yellow/blocked wait ledger. `capture` and `capture_after` steps can declare `expected_visible_fact` plus `expected_screen_text_contains` / `expected_screen_text_after_contains`; missing expected facts, unguarded OCR, failed screen capture, stale runtime metadata, abort guards, and non-green wait ledgers become explicit yellow/red/blocked step verdicts instead of being hidden under `artifacts_matched`.

Current step-ledger audit evidence: after forcing the tiles binary to report current runtime `85d6f37f00`, `basecamp.package2_dialog_rules_probe_mcw` run `.userdata/dev-harness/harness_runs/20260427_174653/` caught the old guessed `1` talk key as wrong-screen evidence; Frau-approved attempt 3 `.userdata/dev-harness/harness_runs/20260427_175051/` proves source-backed `t` opens `Talk to whom` as a green row, then blocks/red-classifies `choose_first_talker` because the alpha talker selector remains visible after `a`. Top-level classification remains `feature_proof=false`, `evidence_class=startup/load-or-inconclusive`, `proof_classification.status=red`, `verdict=blocked_still_on_talk_selector`. This is a harness-audit false-pass catch, not a basecamp feature regression.

Current target-tile audit evidence: after relinking `cataclysm-tiles` to repo HEAD `373511bc36`, `python3 tools/openclaw_harness/startup_harness.py probe bandit.live_world_nearby_camp_real_fire_exact_items_tile_audit_mcw` run `.userdata/dev-harness/harness_runs/20260427_184319/` reached clean startup/runtime (`version_matches_repo_head=true`) but stayed red: 18 GUI/item/firestarter steps are yellow because they still lack expected visible facts or OCR guards, and `audit_saved_target_tile_for_fd_fire` is red because required `fd_fire` is missing. A follow-up direct metadata validation against the same saved world is preserved at `.userdata/dev-harness/harness_runs/20260427_184319/audit_saved_target_tile_for_fd_fire.post_patch_direct_validation.metadata.json`; it reports the requested offset explicitly as an empty tile (`target_location_ms=[3368,994,0]`, `fields=[]`, `items=[]`, `furniture=[]`) instead of leaving the pre-patch probe artifact's `tiles=[]` ambiguous. Follow-up run `.userdata/dev-harness/harness_runs/20260427_191725/` adds same-run `abort_on_metadata_failure` gates after brazier deploy, fuel drop, and final fire; it reached clean startup for feature steps (`startup_clean_for_feature_steps=true`, runtime title `373511bc36` while repo HEAD `747e5d7fce` has no runtime-relevant diff) and then aborted at `audit_saved_target_tile_for_brazier` because the saved tile east of the player had `fields=[]`, `items=[]`, `furniture=[]`, missing required `f_brazier`. After source/static control lookup and a bounded post-Frau selector check, canonical run `.userdata/dev-harness/harness_runs/20260427_200100/` adds `audit_saved_player_items` before the GUI macro and proves the fixture inventory is present (`inventory_counts={"2x4":20,"brazier":1,"lighter":1}`), but selector/activation/place remain non-green and the same east tile is still empty/missing `f_brazier`. This is the desired false-pass shape, not product fire proof.

Next evidence target: do not try more blind live key variants. Add/review a menu-entry or hotkey metadata capture for the still-untrusted inventory selector/deploy UI, then run at most one source-backed live confirmation that can prove `Deploy where?`/selection state and saved east-tile `f_brazier`. Every primitive/keystroke/setup step needs a precondition, action, expected state, screenshot or exact metadata proof, failure rule, artifact path, and pass/yellow/red/blocked verdict. Any non-canonical fixture must be justified with provenance before it can support closure. A load-and-close run remains startup/load proof only. Failed spawns, wrong screens, missing target fields, stale binaries/profiles, and missing save metadata must produce explicit non-green results rather than feature-proof claims. For `wait_action`, `artifacts_matched` must not override a yellow/blocked wait ledger: every long-wait step needs `<label>.wait_menu`, before/after clock-or-turn evidence plus expected elapsed duration, and either a finish signal or classified interruption.

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
