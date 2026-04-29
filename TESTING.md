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

### Recently completed validation target - Fuel normal-map entry primitive packet v0

`Fuel normal-map entry primitive packet v0` is **COMPLETE / GREEN NORMAL-MAP ENTRY GATE** under `C-AOL actual playtest verification stack v0`. Contracts: `doc/c-aol-actual-playtest-verification-stack-v0-2026-04-27.md`, `doc/fuel-normal-map-entry-primitive-packet-v0-2026-04-29.md`, and imagination source `doc/fuel-normal-map-entry-primitive-imagination-source-of-truth-2026-04-29.md`.

Evidence: `.userdata/dev-harness/harness_runs/20260429_140645/`, scenario `bandit.live_world_nearby_camp_source_zone_normal_map_entry_mcw`, command `python3 tools/openclaw_harness/startup_harness.py probe bandit.live_world_nearby_camp_source_zone_normal_map_entry_mcw`. The run is feature-path green with `verdict=artifacts_matched`, 5/5 green step-local ledger, matched `game::load: Finalizing end`, saved charged wielded `lighter`, saved `f_brazier` + real `log` items, saved `SOURCE_FIREWOOD`, and screenshot `normal_map_entry_gate_before_activation.png` with OCR fallback including `Wield:` and standalone `YOU`.

Evidence boundary: direct model image inspection was unavailable, so screen proof is screenshot artifact path plus OCR fallback rather than independent image-model inspection. This primitive sends no activation/targeting/fire/lighter action keys and does **not** close normal ignition, save mtime, `fd_fire`/`fd_smoke`, smoke/light, or bandit signal proof.

### Active validation target - Fuel writeback repair via wood source zone packet v0

`Fuel writeback repair via wood source zone packet v0` is **NEXT / UNBLOCKED FOR REOPENED FIRE PROOF, STILL UNPROVEN** under `C-AOL actual playtest verification stack v0`. The first missing evidence class is now the real normal-map fire path after the green entry gate: activation to `Light where?`, source-firewood confirmation, ignition/no-ignition player message/OCR, save mtime, and saved `fd_fire`/`fd_smoke`.

### Packaged validation boundary - Fuel writeback repair via wood source zone packet v0

`Fuel writeback repair via wood source zone packet v0` is now **implemented-but-unproven / Josef playtest package** under `C-AOL actual playtest verification stack v0`. Contracts: `doc/c-aol-actual-playtest-verification-stack-v0-2026-04-27.md`, `doc/fuel-writeback-repair-via-wood-source-zone-packet-v0-2026-04-29.md`, and imagination source `doc/fuel-writeback-repair-via-wood-source-zone-imagination-source-of-truth-2026-04-29.md`. Manual package entry: `/Users/josefhorvath/.openclaw/workspace/runtime/josef-playtest-package.md#2026-04-29--fuel-writeback-source-zone-repair-v0`.

Current state: the harness support/fixture can stage the intended footing — saved `f_brazier`, real saved-world logs, saved `SOURCE_FIREWOOD` zone, and a charged lighter already wielded. Frau's 09:13 correction classifies `.userdata/dev-harness/harness_runs/20260429_090634/` as blocked, not proof: it proved log/source-zone footing but spent wait/save after a failed or depleted-lighter action, so save mtime/`fd_fire` writeback could not close. Attempt 3/B `.userdata/dev-harness/harness_runs/20260429_093118/` proves setup rows green (`required_weapon=lighter`, nested butane `100`, saved `f_brazier`, saved `log`, saved `SOURCE_FIREWOOD`) but the live UI was already a stale item-info/raw-JSON screen (`pocket_type`, `contents`, `specific energy` OCR). Because activation/target/save keys were typed through that wrong screen, the post-action save mtime did not advance and no saved `fd_fire`/`fd_smoke` was credited. Attempt 4 `.userdata/dev-harness/harness_runs/20260429_093509/` added a guarded `escape` and aborted when the stale JSON/info screen remained instead of normal map UI. Josef's 09:30 action-audit reopen ran `.userdata/dev-harness/harness_runs/20260429_095021/` with per-boundary guards staged for activation, targeting, source-firewood confirmation, save prompt/confirmation, mtime, and saved `fd_fire`/`fd_smoke`; it stopped at the first boundary again: after `escape`, OCR still showed stale raw item JSON (`pocket_type`, `contents`, `specific energy`) and missed `YOU`, so no activation key was sent.

Evidence boundary: setup helpers may create the wood pile, source-zone footing, saved `f_brazier`, and pre-wielded fuelled lighter, but they do not close player-lit fire. No inventory-selector/wield UI coverage is claimed from the pre-wielded setup. The parked scenario now includes a `dev-harness` profile with no automatic post-load Return, a direct normal-map capture gate that red-blocks stale raw JSON before action keys, and action-boundary guards before every unsafe key: activation must reveal `Light where?`, east targeting must reveal `Do you really want to burn your firewood source?`, unhandled confirmation prompts abort, and the post-target/advance player-message/OCR guard must red-block depleted-lighter/no-ignition text such as `lighter has 0 charges, but needs 1` before any save/`fd_fire` audit. Josef's 10:18 clarification makes screenshot evidence mandatory for any reopened fuel/fire path: every relevant boundary needs a screenshot artifact path plus the named visible fact it supports; if the agent/model cannot inspect the image directly, say so and use OCR/metadata only as fallback. If the screenshot still shows stale raw JSON/item-info, report that visible failure and stop before action keys. Closure still requires normal player ignition from normal map UI, a visible normal-map/fire-or-no-fire screenshot fact, player message/OCR, guarded same-run save mtime advance, and saved-state proof of actual `fd_fire` plus `fd_smoke`/smoke-light-relevant state. Debug map-editor `fd_fire`, synthetic loaded-map fields, stale item-info screens, roof-horde proof, OCR without screenshot path, and performance reruns are not substitutes.

Completed performance context remains preserved but inactive: `doc/c-aol-live-ai-performance-matrix-v0-2026-04-29.md` keeps the green one-site, two-site, pre-staged three-site, and pre-staged four-site performance-load rows, latest `.userdata/dev-harness/harness_runs/20260429_041936/`. Do not rerun those rows as ritual; only rerun performance if Schani/Josef promotes a new performance row.

### Recently completed validation target - Cannibal camp confidence-push live playtest packet v0

`Cannibal camp confidence-push live playtest packet v0` is complete as **confidence uplift green**. Matrix: `doc/cannibal-camp-confidence-push-live-playtest-matrix-v0-2026-04-29.md`.

Checkpoint evidence boundary:

- bandit contrast control: `.userdata/dev-harness/harness_runs/20260429_012915/`, scenario `bandit.extortion_first_demand_fight_mcw`, current runtime `7ca870f6be`, 6/6 green step ledger, feature proof, shakedown/pay/fight surface preserved;
- cannibal day smoke/pressure: `.userdata/dev-harness/harness_runs/20260429_013310/`, scenario `cannibal.live_world_day_smoke_pressure_mcw`, current runtime `782d8edabd`, 2/2 green step ledger, feature proof, live smoke/signal path with `profile=cannibal_camp`, `darkness_or_concealment=no`, `shakedown=no`, `combat_forward=no`;
- cannibal night/contact pack-forward: `.userdata/dev-harness/harness_runs/20260429_014900/`, scenario `cannibal.live_world_night_local_contact_pack_mcw`, current runtime `acfe6fd0ce`, 2/2 green step ledger, feature proof, local-contact `attack_now` pack with `pack_size=2`, `darkness_or_concealment=yes`, `shakedown=no`, `combat_forward=yes`;
- reload brain create/save: `.userdata/dev-harness/harness_runs/20260429_021849/`, scenario `cannibal.live_world_day_smoke_persistence_mcw`, current runtime `e778902cac`, 7/7 green step ledger, feature proof, guarded save mtime writeback, saved active `cannibal_camp` stalk group with active members `[4,5]`, target `player@140,41,0`, and `known_recent_marks` including `live_smoke@140,41,0`;
- paired no-fixture reload support: `.userdata/dev-harness/harness_runs/20260429_021929/`, scenario `cannibal.live_world_day_smoke_persistence_reload_audit_mcw`, current runtime `e778902cac`, 2/2 green step ledger and saved-state metadata still present after fresh startup; top-level classifier stays yellow/no-new-artifacts, so this is saved-file/startup support paired with the create/save feature proof, not independent new behavior proof;
- different-footing exposed-sight repeat: `.userdata/dev-harness/harness_runs/20260429_022021/`, scenario `cannibal.live_world_exposed_sight_avoid_mcw`, current runtime `e778902cac`, 5/5 green step ledger, feature proof on fixture `cannibal_live_world_exposed_sight_avoid_v0_2026-04-28` from source footing `bandit_local_sight_avoid_exposed_v0_2026-04-27`, showing `profile=cannibal_camp`, `active_job=stalk`, `posture=hold_off`, `pack_size=2`, `recent_exposure=yes`, `sight_exposure=recent`, `shakedown=no`, `combat_forward=no`.

Caveats retained: this is confidence uplift for already-finished cannibal behavior, not behavior redesign; reload support reads saved state rather than proving fresh post-load local-gate behavior; the different-footing repeat reduces but does not eliminate fixture-bias risk.

### Recently completed validation target - Bandit camp-map risk/reward dispatch planning packet v0

`Bandit camp-map risk/reward dispatch planning packet v0` is closed as **scoped live/product checkpoint green**. Frau product review accepted the current matrix as sufficient for this checkpoint and did not require a second-fixture bias variant before closure.

Checkpoint evidence boundary:

- deterministic/code support covers the camp-owned `camp_intelligence_map`, serialization, active target OMT persistence, scout-return writeback, live signal camp-map lead writes, remembered-lead selection through live dispatch cadence/reporting, two-OMT ordinary scout stand-off, 720-minute scout return clock, 2/4/5/7/10 roster/reserve sizing, active-outside dogpile blocking, wounded/unready and killed-member shrinkage, bounded stockpile pressure, high-threat non-escalation, prior defender-loss pressure, prior bandit-loss cooling, larger-than-scout stalk sizing, and no-opening hold/return;
- feature-path proof covers sight-avoid (`20260428_173626`), vanished-signal redispatch (`20260428_185947`), 2/5/10 variable-roster sizing (`20260428_202044`, `20260428_192059`, `20260428_193621`), no-opening stalk-pressure (`20260428_195617`), high-threat/low-reward hold (`20260428_200600`), active-outside dogpile-block (`20260428_200434`), shakedown/toll-control (`20260428_204454`, `20260428_204630`, `20260428_204813`), same-fixture high-threat repeatability (`20260428_211105`, `20260428_211153`), and optional empty-camp retirement sanity (`20260428_214542`).

Recorded gates: `python3 -m py_compile tools/openclaw_harness/startup_harness.py`; relevant scenario/fixture `python3 -m json.tool` checks; `python3 tools/openclaw_harness/proof_classification_unit_test.py`; `git diff --check`; `make -j4 TILES=1 cataclysm-tiles LINTJSON=0 ASTYLE=0`; `./tests/cata_test "[bandit_live_world]"`; `./tests/cata_test "[bandit][live_world][camp_map]" --success`; and the named harness probes. `make astyle-diff` remains locally blocked by missing `astyle`.

Caveats retained: the old Multidrop fuel/writeback route for player-created fire/smoke/light scenario 1 remains non-credit evidence, but the active wood-source-zone repair reopens the player-lit fire bridge; same-fixture repeatability is not second-fixture anti-bias proof; the no-opening proof is not opening-present escalation proof.

### Recently completed validation target - Cannibal camp night-raid behavior packet v0

The required cannibal live matrix scenarios are checkpointed green: day smoke/stalk pressure, night local-contact pack attack, exposed/recent-sight hold-off, daylight/high-threat negative, saved-state persistence, and day-smoke repeatability. The optional labeled bandit-control contrast is non-blocking unless product review explicitly reopens it.

### Recently completed validation target - C-AOL debug-proof finish stack

`C-AOL debug-proof finish stack v0` is complete and ready for Schani review. Retain the old implemented-but-unproven boundaries for real player-lit fire and Smart Zone Manager live layout unless current canon explicitly promotes their retry packets.

---

## Pending probes

Active pending probe: `Fuel writeback repair via wood source zone packet v0`.

1. Before fire proof continues, perform a bounded broken-save postmortem: compare the broken `fuel_writeback_source_zone_v0_2026-04-29` fixture/save manipulation against the green normal-map fixture path and identify the backend edit, transform, or harness resume step that produced raw item-info/JSON startup, or state that the exact culprit is no longer recoverable from retained artifacts.
2. Start from the green normal-map gate established by `.userdata/dev-harness/harness_runs/20260429_140645/`; do not send blind cleanup keys through raw JSON/item-info/stale/debug screens.
3. Guard activation with screenshot/OCR proof of `Light where?`; abort if the normal map gate is not green first.
4. Guard east targeting/source-firewood confirmation before any unsafe confirmation key.
5. Require post-target ignition/no-ignition player-message/OCR before save/`fd_fire` audit; red-block depleted-lighter/no-ignition text.
6. Only after a real ignition line, prove save mtime plus saved `fd_fire`/`fd_smoke`, then bounded bandit signal/no-response.
7. Preserve `.userdata/dev-harness/harness_runs/20260429_090634/` as blocked/not proof and `.userdata/dev-harness/harness_runs/20260429_093118/` / `.userdata/dev-harness/harness_runs/20260429_093509/` / `.userdata/dev-harness/harness_runs/20260429_095021/` / `.userdata/dev-harness/harness_runs/20260429_122807/` / `.userdata/dev-harness/harness_runs/20260429_122955/` as prior stale-screen/normal-map-gate blocker evidence and postmortem input only.
8. Preserve `.userdata/dev-harness/harness_runs/20260429_040926/` and `.userdata/dev-harness/harness_runs/20260429_041936/` as green pre-staged performance-load rows, not natural three/four-site player-pressure dispatch proof.

Still blocked/later in the actual playtest stack:

- `Player-lit fire and bandit signal verification packet v0`: blocked behind normal player-lit fire proof/manual evidence.
- `Roof-fire horde detection proof packet v0`: blocked behind real player-lit fire.
- Natural three/four-site player-pressure behavior and true zero-site idle baseline remain decision/watchlist items, not current requirements.

If a later live probe is needed:
- build the current runtime first when binary freshness matters;
- use one named scenario/command path;
- extract only decisive report/log fields into context;
- after two same-blocker attempts, consult Frau Knackal before attempt 3; after four unresolved attempts, package implemented-but-unproven state for Josef and move to the next greenlit unblocked target.

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
