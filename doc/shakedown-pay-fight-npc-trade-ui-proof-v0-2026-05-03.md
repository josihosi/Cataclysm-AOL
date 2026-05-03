# Shakedown Pay/Fight + NPC trade UI proof — 2026-05-03

Status: GREEN FEATURE-PATH CHECKPOINT for the shakedown visible-fork/open-payment contract inside `CAOL-JOSEF-LIVE-DEBUG-BATCH-v0`.

## Claim proven

- First-demand bandit shakedown now shows exactly the player-visible `Pay` / `Fight` fork on the fresh current build.
- Backout/refuse still maps to the fight/refusal branch, but no third visible response or conversation utility action is shown on this shakedown surface.
- Selecting `Pay` opens the real NPC trade UI path (`npc_trading::trade` / `trade_ui`) with `Pay:` debt before any hidden surrender.
- The Pay window is wired to the broader honest camp-side pool: avatar carried goods, nearby ally/NPC carried goods, and nearby scene/basecamp goods.
- Fight remains a distinct branch.
- Reopened demand still shows Pay/Fight with the higher bounded reopened toll.

## Red evidence that triggered the repair

Josef's live report was correct. A fresh-current pre-repair run still exposed extra dialogue actions, so the earlier green proof was not enough. Root cause: the custom shakedown `dialogue_window` was still behaving like a normal conversation and appended utility actions.

- Red run: `.userdata/dev-harness/harness_runs/20260503_173812/`
  - Window/build receipt: `Cataclysm: Dark Days Ahead - 6bf5ebfc64`, `repo_head=6bf5ebfc64`, `captured_head=6bf5ebfc64`, `captured_dirty=false`, `version_matches_runtime_paths=true`.
  - Red screenshot: `hold_visible_shakedown_for_external_inspection.after.png`; inspected crop copy `/Users/josefhorvath/.openclaw/workspace/caol_red_extra_actions_20260503_173812_crop_upscale.png`.
  - OCR excerpt `/tmp/caol_red_extra_actions_173812_ocr.txt`: `Your response:`, `p: Pay.`, `f: Fight.`, plus `_ook at`, `Size up stats`, `Assess personality`, `Yell`, `Check opinion`.
  - Classification: RED live-path UI proof; the visible surface was not Pay/Fight-only.

Repair: `query_live_bandit_shakedown_dialogue()` now marks the window as non-conversation with `d_win.is_not_conversation = true;`, preventing the generic talk utility actions from being drawn.

## Fresh-current evidence

Build/current-binary gates:

- `make version && make -j4 TILES=1 cataclysm-tiles LINTJSON=0 ASTYLE=0` after committing the repair -> green; log `/tmp/caol_tiles_build_post_commit_d047_20260503_andi.log`.
- `git diff --check`, harness `py_compile`, and edited shakedown scenario JSON parse -> green; logs `/tmp/caol_shakedown_no_extra_actions_quick_gates_20260503.log` and `/tmp/caol_shakedown_post_commit_doc_diff_check_20260503.log`.
- Final proof windows after the repair commit report `Cataclysm: Dark Days Ahead - d0476b4407`, `repo_head=d0476b4407`, `captured_head=d0476b4407`, `captured_dirty=false`, `version_matches_runtime_paths=true`.

Live/staged harness rows credited after the repair:

- Final current-HEAD Pay/Fight + trade UI row, `bandit.extortion_first_demand_pay_mcw` -> `.userdata/dev-harness/harness_runs/20260503_180929/`
  - `probe.report.json`: `evidence_class=feature-path`, `feature_proof=true`, `verdict=artifacts_matched`.
  - Window/build receipt: `Cataclysm: Dark Days Ahead - d0476b4407`, `repo_head=d0476b4407`, `captured_head=d0476b4407`, `captured_dirty=false`, `version_matches_runtime_paths=true`.
  - Screenshot: `advance_final_turn_to_first_shakedown.after.png`; inspected crop copy `/Users/josefhorvath/.openclaw/workspace/caol_pay_fight_only_post_commit_20260503_180929_crop_upscale.png`.
  - OCR excerpt `/tmp/caol_pay_fight_only_post_commit_180929_ocr.txt`: `What do you do?`, `p: Pay.`, `f: Fight.`.
  - Debug artifact: `shakedown_surface_dialogue_window opening=basecamp_pressure responses=pay/fight payment_surface=npc_trade_ui`.
  - Debug artifact: `shakedown_trade_ui opened demanded=15797 reachable=45134 player_pool=3211 nearby_npc_pool=5222 scene_pool=36701 trader=4 trade_api=npc_trading::trade title=Pay:`.

- Supporting visible fork inspection -> `.userdata/dev-harness/harness_runs/20260503_174815/`
  - Scenario: temporary inspection row for the first-demand shakedown surface.
  - `probe.report.json`: `evidence_class=feature-path`, `feature_proof=true`, `verdict=artifacts_matched`.
  - Screenshot: `hold_visible_shakedown_for_external_inspection.after.png`.
  - Inspected crop copy: `/Users/josefhorvath/.openclaw/workspace/caol_green_pay_fight_only_20260503_174815_crop_upscale.png`.
  - OCR excerpt `/tmp/caol_green_pay_fight_only_174815_ocr.txt`: `What do you do?`, `p: Pay.`, `f: Fight.`; no `Refuse`, `Look at`, `Size up stats`, `Assess personality`, `Yell`, or `Check opinion` appears in the inspected prompt crop.


- `bandit.extortion_first_demand_fight_mcw` -> `.userdata/dev-harness/harness_runs/20260503_175254/`
  - `probe.report.json`: `evidence_class=feature-path`, `feature_proof=true`, `verdict=artifacts_matched`.
  - Proves first-demand Pay/Fight surface plus distinct `shakedown_surface fight demanded=15797 reachable=45134` branch.

- `bandit.extortion_reopened_demand_mcw` -> `.userdata/dev-harness/harness_runs/20260503_175512/`
  - `probe.report.json`: `evidence_class=feature-path`, `feature_proof=true`, `verdict=artifacts_matched`.
  - Proves reopened `responses=pay/fight payment_surface=npc_trade_ui`, `demanded_toll=22116`, and `renegotiation reopen: previous defender loss raises this one bounded demand`.

Superseded rows: the pre-nudge rows `20260503_171632`, `20260503_171825`, and `20260503_172007` are not credited as final Slice 1 proof because they were stale/insufficient for Josef's live-red visible-options report.

## Caveats / next slice boundary

This checkpoint proves the visible Pay/Fight-only fork and real trade-window opening path on a freshly rebuilt current binary. It does not claim full natural-discovery coverage, broad diplomacy/payment redesign, or a saved successful-payment/writeback row after the player manually balances and accepts the trade. Future rows may extend that, but the previous fake selector / silent-confiscation Pay surface is no longer the active product path.
