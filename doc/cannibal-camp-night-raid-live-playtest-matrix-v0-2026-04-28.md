# Cannibal camp night-raid live playtest matrix v0 (2026-04-28)

Status: ACTIVE / PRODUCT-PROOF SLICE GREENLIT

Imagination source: `doc/cannibal-camp-night-raid-imagination-source-of-truth-2026-04-28.md`
Canonical contract: `doc/cannibal-camp-night-raid-behavior-packet-v0-2026-04-28.md`
Proof-freeze matrix: `doc/c-aol-harness-proof-freeze-matrix-v0-2026-04-28.md`

## Purpose

The live/playtest pass is greenlit in canon after the deterministic cannibal substrate landed. The goal is not “run around until something exciting happens.” The goal is to turn the imagined cannibal scenario into audited product evidence: every input, setup action, wait, dispatch, contact, and conclusion must be proven by a step ledger row, screenshot/OCR, exact metadata, or log field.

This matrix is intentionally several small playtests rather than one soup macro. More coverage is welcome, but each run must name its claim and evidence class before it starts.

## Global proof rules

Every scenario must record:

1. scenario name, run id, repo/runtime version, profile/save/fixture provenance;
2. preconditions: cannibal camp id/location, roster, at-home members, active outside members, target/basecamp location, time/weather/light state, smoke/light/routine source, and player/camp visibility setup;
3. action ledger: exact harness primitive, keypress, wait duration, debug/setup transform, or in-game action;
4. immediate proof: screenshot/OCR or exact metadata/log field for each meaningful step;
5. failure rule: what makes the step yellow/red/blocked and whether later steps are allowed to continue;
6. final artifacts: report JSON, step ledger, screenshots, debug log excerpts, saved-map/overmap/NPC metadata, and any local-gate/dispatch reports;
7. verdict: passed / failed / inconclusive, with no feature-proof credit after a non-green prerequisite.

Load-and-close remains startup/load proof only. Deterministic evaluator output remains support only. Live product proof requires the real dispatch/local-contact path or an explicitly labeled setup primitive whose limits are visible in the report.

## Current run ledger

Green feature-path runs so far:

- Scenario 1 day smoke/light lead: `.userdata/dev-harness/harness_runs/20260428_124902/`, `feature_proof=true`, `2/2` green step rows, smoke signal matched, cannibal `active_job=stalk`, `posture=hold_off`, `darkness_or_concealment=no`, `shakedown=no`, `combat_forward=no`.
- Scenario 2 night/local-contact pack attack: `.userdata/dev-harness/harness_runs/20260428_124947/`, `feature_proof=true`, `2/2` green step rows, direct player range/local contact matched, `profile=cannibal_camp`, `posture=attack_now`, `pack_size=2`, `darkness_or_concealment=yes`, `local_contact=yes`, `combat_forward=yes`, `shakedown=no`.
- Scenario 3 exposed/recent-sight hold-off: `.userdata/dev-harness/harness_runs/20260428_125138/`, `feature_proof=true`, `5/5` green step rows, bounded 20-turn exposed window, cannibal `posture=hold_off`, `current_exposure=no`, `recent_exposure=yes`, `sight_exposure=recent`, `shakedown=no`, `combat_forward=no`. The inherited `bandit_camp` reposition artifact in this fixture is control/source-fixture noise and must not be credited as cannibal behavior.
- Scenario 4 daylight/high-threat negative: `.userdata/dev-harness/harness_runs/20260428_135323/`, `feature_proof=true`, `2/2` green step rows on current runtime `48abd82de9`, daylight smoke/light scan, cannibal `posture=hold_off`, `threat=3`, `opportunity=2`, `darkness_or_concealment=no`, `recent_exposure=yes`, `sight_exposure=recent`, `shakedown=no`, `combat_forward=no`.
- Scenario 7 repeatability smoke: `repeatability --count 2 cannibal.live_world_day_smoke_pressure_mcw`, stable pass, runs `.userdata/dev-harness/harness_runs/20260428_125319/` and `.userdata/dev-harness/harness_runs/20260428_125342/`.
- Scenario 6 saved-state persistence: `.userdata/dev-harness/harness_runs/20260428_130948/`, `feature_proof=true`, `7/7` green step rows, day-smoke active cannibal `stalk` group created and saved through guarded mtime writeback; saved `dimension_data.gsav` reports `profile=cannibal_camp`, active group/target, `active_member_ids=[4,5]`, and `known_recent_marks` containing `live_smoke@...`; `intelligence_map.leads=[]` is out of scope, not camp-map proof. No-fixture reload support: `.userdata/dev-harness/harness_runs/20260428_131031/`, `2/2` green step rows after fresh startup without fixture reinstall, same saved active cannibal state still present; top-level classifier yellow/no-new-artifacts, so this is reload support, not a separate behavior artifact.

Still open only if product review wants extra separation: optional labeled bandit-control contrast for shakedown/pay/fight beyond the green cannibal `shakedown=no` local-contact proof.

## Scenario matrix

### 1. Day smoke/light lead becomes scout/stalk pressure, not instant combat

Claim: daytime smoke/light or repeated human/basecamp routine near a cannibal-relevant target creates a lead that may scout/stalk/prepare dispatch, but does not teleport into combat or immediate `attack_now`.

Required proof:
- setup proves smoke/light/routine source and target/basecamp location;
- cannibal camp has enough roster for a later pack but does not instantly consume it as combat;
- artifact/log/report names lead source (`smoke`, `light`, or `routine`) and cannibal profile;
- active group, if created, is scout/stalk/probe/dispatch-prep rather than direct attack;
- player/basecamp screen does not show immediate visible beeline combat as the credited outcome;
- no shakedown/pay/fight surface appears.

Current verdict: GREEN for smoke/light pressure via `.userdata/dev-harness/harness_runs/20260428_124902/`. Matched artifacts include `bandit_live_world signal scan: signal_packet=yes`, `smoke_packets=1`, `matched_sites=1 refreshed_sites=1`, cannibal local gate `active_job=stalk profile=cannibal_camp posture=hold_off pack_size=2`, and `shakedown=no combat_forward=no`.

### 2. Night/darkness or concealment turns a confirmed pack contact into attack window

Claim: with equivalent target pressure but a dark/concealed favorable window, a cannibal camp with enough available members can dispatch or commit a multi-member attack through the real path.

Required proof:
- time/light/concealment state is proven, not assumed;
- available cannibal roster after reserve is at least two;
- dispatch/local-contact metadata shows `profile=cannibal_camp`, pack size/active member ids >= 2, and `darkness_or_concealment=yes` or equivalent live-derived input;
- final posture reaches attack/combat-forward only after the lead/scout/dispatch/contact chain is established;
- no singleton ordinary attack is credited;
- save/load or same-run metadata can preserve active group/target/profile if the run crosses a save boundary.

Current verdict: GREEN for same-run local-contact attack via `.userdata/dev-harness/harness_runs/20260428_124947/`. Matched artifacts include `live_candidate reason=direct_player_range`, cannibal local gate `active_job=stalk profile=cannibal_camp posture=attack_now pack_size=2 darkness_or_concealment=yes local_contact=yes`, and `shakedown=no combat_forward=yes`. Save/load persistence remains scenario 6, not credited here.

### 3. Seen stalker holds, repositions, or aborts within a bounded window

Claim: a stalking/scouting cannibal exposed to the player or basecamp NPC vision does not keep visibly beelining; it holds off, repositions through normal movement/pathing, or aborts within at most two local turns when such proof is in scope.

Required proof:
- before screenshot/metadata proves the cannibal's relevant exposure state;
- action/wait ledger proves the one- or two-turn observation window;
- after screenshot/metadata/log proves hold/reposition/abort, or names blocked/no-cover/path-fail honestly;
- no teleport is credited as sight avoidance;
- if the cannibal remains visible and closing, verdict is red/inconclusive, not green.

Current verdict: GREEN for bounded hold-off via `.userdata/dev-harness/harness_runs/20260428_125138/`. This scenario uses a bounded 20-turn live window rather than a two-turn movement proof; it credits the cannibal local-gate hold-off path (`profile=cannibal_camp posture=hold_off current_exposure=no recent_exposure=yes sight_exposure=recent shakedown=no combat_forward=no`) and the visible-beeline hold-off note. The inherited `bandit_camp` reposition debug line is a source/control artifact only and is explicitly non-crediting for cannibal behavior.

### 4. Daylight/no-cover and high-threat negatives stay non-suicidal

Claim: daylight/no-cover or high-threat conditions still hold/probe/abort instead of making cannibals blindly attack just because they are cannibals.

Required proof:
- same or comparable target/camp setup as the attack-window scenario;
- daylight/no-cover and/or high threat is proven;
- local gate/report posture is hold/probe/abort, not attack;
- no visible combat-forward behavior is credited;
- the result is contrasted against the night/concealment run where possible.

Current verdict: GREEN for separated daylight/high-threat negative via `.userdata/dev-harness/harness_runs/20260428_135323/`. The scenario `cannibal.live_world_daylight_high_threat_negative_mcw` ran after rebuilding the tiles binary to current runtime `48abd82de9`; startup was clean, `step_ledger_summary=2/2` green, and matched artifacts include daylight `signal_packet=yes`, cannibal local gate `active_job=stalk profile=cannibal_camp posture=hold_off pack_size=2 threat=3 opportunity=2 darkness_or_concealment=no recent_exposure=yes sight_exposure=recent shakedown=no combat_forward=no`. This is separated negative proof, not a new smoke-source breadth claim.

### 5. Cannibals never open bandit shakedown/extortion UI

Claim: cannibal contact does not expose pay/fight/shakedown negotiation surfaces, while ordinary bandit control behavior remains separate.

Required proof:
- cannibal contact reaches the relevant local surface or report path;
- report/screenshot/log shows `shakedown=no` or absence of pay/fight UI;
- if a control bandit run is used, it is labeled regression/control and not merged into cannibal proof;
- no cannibal run is allowed to close from merely not reaching contact.

### 6. Saved-state persistence for active cannibal proof state

Claim: once a cannibal scout/pack/contact state is created, the save/writeback path preserves the reviewer-critical saved state: profile, active member ids, target id/location, camp roster state, live-signal `known_recent_marks`, and serialized posture/intent fields. A no-fixture reload audit may support this by proving the saved world still loads and the same saved `dimension_data.gsav` fields remain present, but it is not fresh in-memory local-gate proof unless a post-load live report is captured.

Required proof:
- pre-save metadata captures active state;
- guarded save prompt and uppercase `Y` / writeback mtime proof are green if saving through GUI;
- post-save metadata matches the relevant active group/profile/target and live-signal `known_recent_marks` fields;
- any reload audit is labeled as clean reload/saved-file support when it reads saved `dimension_data.gsav` and reports `inconclusive_no_new_artifacts`;
- `intelligence_map.leads=[]` is called out as out-of-scope downstream camp-map evidence for this packet, not hidden as green proof;
- missing non-serialized fields are called out as product gaps instead of hidden.

### 7. Repeatability / fixture-bias smoke check

Claim: the proof does not depend on one magical fixture accident.

Required proof:
- after one scenario passes, rerun the smallest equivalent repeatability check or a second fixture variant if available;
- record what changed and what stayed equivalent;
- if the second run fails due to fixture/setup, classify the fixture bias and do not erase the first run, but do not claim broad reliability.

Current verdict: GREEN smoke repeatability for scenario 1. `python3 tools/openclaw_harness/startup_harness.py repeatability --count 2 cannibal.live_world_day_smoke_pressure_mcw` returned `overall_verdict=stable_repeatability_pass`; individual probe runs were `.userdata/dev-harness/harness_runs/20260428_125319/` and `.userdata/dev-harness/harness_runs/20260428_125342/`, both `artifacts_matched` with current runtime metadata.

## Preferred execution order

1. Build/relink current runtime if binary freshness matters.
2. Run a dry preflight that prints intended scenario steps and expected evidence fields without mutating the save.
3. Run scenario 1 day smoke/light lead.
4. Run scenario 2 night/darkness attack window.
5. Run scenario 3 exposure hold/reposition/abort.
6. Scenario 4 daylight/high-threat negative is now covered by `20260428_135323`; rerun only if the negative-proof contract changes.
7. Scenario 5 cannibal no-shakedown surface is supported by the green local-contact attack report (`20260428_124947`, `shakedown=no`); run an optional bandit control only if product review wants a labeled pay/fight contrast.
8. Scenario 6 saved-state persistence is now covered by `20260428_130948`; no-fixture reload/saved-file support is `20260428_131031`; rerun only if the persistence proof contract changes.
9. Run scenario 7 repeatability only after a passing scenario exists.

## Stop conditions

- Stop immediately when a prerequisite step is red and later steps would launder it.
- After two same-blocker attempts, consult Frau Knackal before attempt three.
- After four unresolved attempts, write the implemented-but-unproven packet for Josef and move to the next explicit greenlit target.
- Do not “improve” the product by widening cannibal lore, capture mechanics, or unrelated bandit AI during this proof slice.
