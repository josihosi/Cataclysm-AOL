# Shakedown Pay/Fight + NPC trade UI proof — 2026-05-03

Status: GREEN FEATURE-PATH CHECKPOINT for the shakedown visible-fork/open-payment/cancel/successful-writeback contract inside `CAOL-JOSEF-LIVE-DEBUG-BATCH-v0`.

## Claim proven

- First-demand bandit shakedown now shows exactly the player-visible `Pay` / `Fight` fork on the fresh current build.
- Backout/refuse still maps to the fight/refusal branch, but no third visible response or conversation utility action is shown on this shakedown surface.
- Selecting `Pay` opens the real NPC trade UI path (`npc_trading::trade` / `trade_ui`) with `Pay:` debt before any hidden surrender.
- The Pay window is wired to the broader honest camp-side pool: avatar carried goods, nearby ally/NPC carried goods, basecamp storage goods, basecamp assigned-NPC carried goods, and nearby honest scene/basecamp goods.
- The demanded debt/toll is derived from the reachable camp-side pool and pressure modifier, not a fixed/stub amount or player-carried-only value.
- Fight remains a distinct branch.
- Reopened demand still shows Pay/Fight with the higher bounded reopened toll, and reopened `Pay` opens the same real NPC trade UI path.
- Cancelling/backing out of the Pay trade UI maps to the refusal/fight outcome; it does not silently surrender goods or expose a third visible dialogue answer.
- Successful payment through the real trade UI records a paid shakedown outcome, advances the saved-player mtime on save, and persists the bandit site back home with no active outside group/target.

## Red evidence that triggered the repair

Josef's live report was correct. A fresh-current pre-repair run still exposed extra dialogue actions, so the earlier green proof was not enough. Root cause: the custom shakedown `dialogue_window` was still behaving like a normal conversation and appended utility actions.

- Red run: `.userdata/dev-harness/harness_runs/20260503_173812/`
  - Window/build receipt: `Cataclysm: Dark Days Ahead - 6bf5ebfc64`, `repo_head=6bf5ebfc64`, `captured_head=6bf5ebfc64`, `captured_dirty=false`, `version_matches_runtime_paths=true`.
  - Red screenshot: `hold_visible_shakedown_for_external_inspection.after.png`; inspected crop copy `/Users/josefhorvath/.openclaw/workspace/caol_red_extra_actions_20260503_173812_crop_upscale.png`.
  - OCR excerpt `/tmp/caol_red_extra_actions_173812_ocr.txt`: `Your response:`, `p: Pay.`, `f: Fight.`, plus `_ook at`, `Size up stats`, `Assess personality`, `Yell`, `Check opinion`.
  - Classification: RED live-path UI proof; the visible surface was not Pay/Fight-only.

Repair: `query_live_bandit_shakedown_dialogue()` now marks the window as non-conversation with `d_win.is_not_conversation = true;`, preventing the generic talk utility actions from being drawn.

## Fresh-current evidence

Additional live nudge: after the visible response-count repair, Josef reported that live `Pay` still did not open the trade/payment window. A later loose playtest/probe packet also did not cut the cheese: it is treated as no-credit unless it proves the UI-first bridge directly. The credited UI closure row was therefore first established by the tight cropped-OCR proof on rebuilt `d1a4f076c8`; saved/writeback was credited only from a separate current-HEAD row that first reached the same `npc_trading::trade` UI path. After the later hard-stop warning and a forced rebuild to remove a runtime mismatch, the latest current-version receipt is the rebuilt `7e5a506c76` master-profile proof pair below; those rows supersede the `d1a4f076c8` rows as the newest proof receipt while preserving the older rows as supporting history.

Build/current-binary gates:

- `make version && make -j4 TILES=1 cataclysm-tiles LINTJSON=0 ASTYLE=0` after committing the repair -> green; log `/tmp/caol_tiles_build_post_commit_d047_20260503_andi.log`.
- `git diff --check`, harness `py_compile`, and edited shakedown scenario JSON parse -> green; logs `/tmp/caol_shakedown_no_extra_actions_quick_gates_20260503.log` and `/tmp/caol_shakedown_post_commit_doc_diff_check_20260503.log`.
- Final proof windows after the original visible-option repair reported `Cataclysm: Dark Days Ahead - d0476b4407`, `repo_head=d0476b4407`, `captured_head=d0476b4407`, `captured_dirty=false`, `version_matches_runtime_paths=true`.
- Tight Pay/Fight + Pay-window proof after Josef's later no-credit nudge reports `Cataclysm: Dark Days Ahead - d1a4f076c8` on the rebuilt current binary, with no version mismatch; build log `/tmp/caol_slice1_tight_clean_build_d1a_20260503.log`.
- Earlier Pay-bridge/cancel reruns after the first later live Pay nudge reported `Cataclysm: Dark Days Ahead - 04de6e0f94` on a rebuilt current binary; these remain supporting evidence for reopened/cancel behavior, not the final UI-first closure row.
- Deterministic shakedown contract gate after the real trade-window/basecamp-pool code pass: `./tests/cata_test "[bandit][live_world][shakedown]" --reporter compact` -> passed 5 test cases / 162 assertions; log `/tmp/caol_shakedown_contract_tests_20260503.log`.
- Full bandit live-world focused gate after the same code pass: `./tests/cata_test "[bandit][live_world]" --reporter compact` -> passed 66 test cases / 1365 assertions; log `/tmp/caol_bandit_live_world_tests_20260503.log`.
- Source-path audit for the bridge: `Pay` calls `npc_trading::trade( *trader, surface.demanded_value, _( "Pay:" ), ... )`; that constructs `trade_ui` with the `Pay:` debt already active. For basecamp/camp scenes, the player pane now receives the expanded honest camp-side pool through normal trade inventory population: carried avatar goods, nearby items/vehicles, nearby allies, basecamp storage (`inventory_selector::add_basecamp_items`) and basecamp-assigned allied NPC carried items. The demanded value is still computed from `surface.reachable_goods_value`, now including basecamp storage and assigned-NPC goods where present rather than using a fixed/stub or avatar-carried-only figure.

Live/staged harness rows credited after the repair:

- Latest rebuilt-current cancel/refusal row, `bandit.extortion_first_demand_pay_cancel_fight_mcw` with `--profile master` -> `.userdata/master/harness_runs/20260503_213831/`
  - Command: `python3 tools/openclaw_harness/startup_harness.py probe --profile master bandit.extortion_first_demand_pay_cancel_fight_mcw` from the active repo after forced rebuild.
  - Window/build receipt: `Cataclysm: Dark Days Ahead - 7e5a506c76`; `repo_head=7e5a506c76`, `captured_head=7e5a506c76`, `captured_dirty=false`, `version_matches_repo_head=true`, `version_matches_runtime_paths=true`, and no runtime-relevant diff since capture.
  - `probe.report.json`: `evidence_class=feature-path`, `feature_proof=true`, `verdict=artifacts_matched`, `step_ledger_status=green_step_local_proof`.
  - Pre-selection artifact: `shakedown_surface ... visible_responses=pay/fight payment_surface=npc_trade_ui reachable_goods=45134 demanded_toll=15797` and `shakedown_surface_dialogue_window opening=basecamp_pressure responses=pay/fight payment_surface=npc_trade_ui`.
  - Immediate post-Pay artifact: `shakedown_trade_ui opened demanded=15797 reachable=45134 player_pool=3211 nearby_npc_pool=5222 basecamp_npc_pool=0 scene_pool=36701 basecamp_storage_pool=0 trader=4 trade_api=npc_trading::trade title=Pay:`.
  - Cancel/shortfall branch: `shakedown_trade_ui result=cancel_or_short demanded=15797 reachable=45134 trader=4` followed by `shakedown_surface fight demanded=15797 reachable=45134`.

- Latest rebuilt-current successful Pay/save/writeback row, `bandit.extortion_first_demand_pay_success_save_mcw` with `--profile master` -> `.userdata/master/harness_runs/20260503_214015/`
  - Command: `python3 tools/openclaw_harness/startup_harness.py probe --profile master bandit.extortion_first_demand_pay_success_save_mcw` from the active repo after forced rebuild.
  - Window/build receipt: `Cataclysm: Dark Days Ahead - 7e5a506c76`; `repo_head=7e5a506c76`, `captured_head=7e5a506c76`, `captured_dirty=false`, `version_matches_repo_head=true`, `version_matches_runtime_paths=true`, and no runtime-relevant diff since capture.
  - `probe.report.json`: `evidence_class=feature-path`, `feature_proof=true`, `verdict=artifacts_matched`, `step_ledger_status=green_step_local_proof`.
  - Pre-selection artifact: `shakedown_surface ... visible_responses=pay/fight payment_surface=npc_trade_ui reachable_goods=45134 demanded_toll=15797`.
  - Immediate post-Pay artifact: `shakedown_trade_ui opened demanded=15797 reachable=45134 player_pool=3211 nearby_npc_pool=5222 basecamp_npc_pool=0 scene_pool=36701 basecamp_storage_pool=0 trader=4 trade_api=npc_trading::trade title=Pay:`.
  - Item-offer proof: screenshot/OCR guards for `filter_hub_items`, `autobalance_hub_helmets`, and `confirm_trade_offer` show the real Pay trade UI with filtered Hub items, a selected helmet stack in the offer, and `Accept this trade? (Case Sensitive)` before acceptance.
  - Paid writeback: `shakedown_trade_ui result=paid demanded=15797 reachable=45134 trader=4` and `shakedown_surface paid toll=15797 demanded=15797 reachable=45134`, followed by save-mtime and saved bandit-state gates.

- Latest three-source payment-pool + offer/accept row, `bandit.extortion_first_demand_pay_three_source_trade_window_mcw` -> `.userdata/dev-harness/harness_runs/20260503_233823/`
  - Command: `python3 tools/openclaw_harness/startup_harness.py probe bandit.extortion_first_demand_pay_three_source_trade_window_mcw` from the active repo after rebuilding `cataclysm-tiles` to `eddfca4f10-dirty`; startup gate is green for feature steps with no runtime-relevant version mismatch.
  - `probe.report.json`: `evidence_class=feature-path`, `feature_proof=true`, `verdict=artifacts_matched`, `step_ledger_status=green_step_local_proof` 19/19.
  - Fixture/marker footing is explicit in `startup.result.json`: player marker `binoculars`, basecamp-assigned NPC marker `saxophone` on NPC id `2`, and `200` owned `gold_watch` items on a `CAMP_STORAGE` tile.
  - Immediate post-Pay artifact: `shakedown_trade_ui opened demanded=33542 reachable=95834 player_pool=3711 nearby_npc_pool=2268 basecamp_npc_pool=3154 scene_pool=86701 basecamp_storage_pool=50000 trader=4 trade_api=npc_trading::trade title=Pay:`.
  - Same-open-window UI traces prove all three source classes in the real trade selector: `typeid="binoculars" recursive_location_type="character"`, `typeid="saxophone" location_type="character" recursive_location_type="character"`, and `typeid="gold_watch" location_type="map" recursive_location_type="map" stack_count=200 available_count=200`.
  - Screenshot/OCR artifacts: `choose_pay_open_trade_window.after.png` / `.screen_text.txt` show the Pay trade window and filter surface; `confirm_trade_offer.after.png` / `.screen_text.txt` show `F1 to auto balance`, `Pay:`, selected `180 of 200 ++ gold`, and `Accept this trade? (Case Sensitive)`.
  - Paid artifacts after the same trade path: `shakedown_trade_ui result=paid demanded=33542 reachable=95834 trader=4` and `shakedown_surface paid toll=33542 demanded=33542 reachable=95834`.

- Final tight current-HEAD Pay/Fight + immediate Pay-window row, `bandit.extortion_first_demand_pay_tight_crop_ocr_mcw` -> `/Users/josefhorvath/Schanigarten/Cataclysm-AOL-slice1-proof/.userdata/dev-harness/harness_runs/20260503_202326/`
  - Command: `python3 tools/openclaw_harness/startup_harness.py probe bandit.extortion_first_demand_pay_tight_crop_ocr_mcw` from the clean proof worktree.
  - Current HEAD/binary: proof worktree `d1a4f076c8`; rebuilt `cataclysm-tiles`; build log `/tmp/caol_slice1_tight_clean_build_d1a_20260503.log`.
  - `probe.report.json`: `evidence_class=feature-path`, `feature_proof=true`, `verdict=artifacts_matched`; step ledger `green_step_local_proof` with no non-green rows; cleanup terminated the launched game.
  - Pre-selection cropped OCR: `delayed_prompt_visible_pay_fight_before_selection.after.screen_text.txt` shows the demand text plus `: Pay.` and `: Fight.` before the Pay key is sent; forbidden `Refuse`, `Look at`, `Size up stats`, `Assess personality`, `Yell`, and `Check opinion` guards did not fire.
  - Immediate post-Pay cropped OCR: `choose_pay.after.screen_text.txt` shows `Pay:`, `Debt $157.97`, and `F1 to auto balance with highlighted item`, proving the real trade/payment window is visibly open.
  - Same-run debug artifact: `shakedown_surface_dialogue_window opening=basecamp_pressure responses=pay/fight payment_surface=npc_trade_ui`.
  - Same-run trade artifact: `shakedown_trade_ui opened demanded=15797 reachable=45134 player_pool=3211 nearby_npc_pool=5222 scene_pool=36701 trader=4 trade_api=npc_trading::trade title=Pay:`.
  - Pool/basis read: `3211 + 5222 + 36701 = 45134` reachable camp-side value; first demand `15797` is derived from that reachable pool, not from a fixed/stub or player-carried-only amount.

- Supporting first-demand Pay/open-window row, `bandit.extortion_first_demand_pay_mcw` -> `.userdata/dev-harness/harness_runs/20260503_191704/`
  - `probe.report.json`: `evidence_class=feature-path`, `feature_proof=true`, `verdict=artifacts_matched`; step ledger `green_step_local_proof` 6/6.
  - Window/build receipt: `Cataclysm: Dark Days Ahead - 04de6e0f94`; no version mismatch.
  - OCR guard after choosing `Pay`: `F1 to auto balance with highlighted item` from `choose_pay.after.screen_text.json`, proving the real trade UI is visibly open.
  - Debug artifact: `shakedown_trade_ui opened demanded=15797 reachable=45134 player_pool=3211 nearby_npc_pool=5222 scene_pool=36701 trader=4 trade_api=npc_trading::trade title=Pay:`.
  - Pool/basis read: `3211 + 5222 + 36701 = 45134` reachable camp-side value; first demand `15797` is the derived first-demand toll, not a player-carried-only or fixed/stub value.

- Fresh current reopened-demand Pay/open-window row, `bandit.extortion_reopened_demand_mcw` -> `.userdata/dev-harness/harness_runs/20260503_192442/`
  - `probe.report.json`: `evidence_class=feature-path`, `feature_proof=true`, `verdict=artifacts_matched`; step ledger `green_step_local_proof` 6/6.
  - Window/build receipt: `Cataclysm: Dark Days Ahead - 04de6e0f94`; no version mismatch.
  - OCR guard after choosing reopened `Pay`: `F1 to auto balance with highlighted item`.
  - Debug artifact: `shakedown_trade_ui opened demanded=22116 reachable=45134 player_pool=3211 nearby_npc_pool=5222 scene_pool=36701 trader=4 trade_api=npc_trading::trade title=Pay:`.
  - Pool/basis read: same reachable camp-side pool, with reopened-demand pressure modifier raising the derived debt to `22116`.

- Fresh current Pay-cancel/refusal row, `tmp.bandit_pay_cancel_fight_probe_mcw` -> `.userdata/dev-harness/harness_runs/20260503_192911/`
  - `probe.report.json`: `evidence_class=feature-path`, `feature_proof=true`, `verdict=artifacts_matched`; step ledger `green_step_local_proof` 8/8.
  - Promoted stable scenario: `tools/openclaw_harness/scenarios/bandit.extortion_first_demand_pay_cancel_fight_mcw.json`.
  - OCR guard first proves the Pay trade UI opened (`F1 to auto balance with highlighted item`).
  - Cancel/backout artifacts: `shakedown_trade_ui result=cancel_or_short demanded=15797 reachable=45134 trader=4` and `shakedown_surface fight demanded=15797 reachable=45134`.

- Current-HEAD successful Pay/save/writeback row, `bandit.extortion_first_demand_pay_success_save_mcw` -> `/Users/josefhorvath/Schanigarten/Cataclysm-AOL-slice1-proof/.userdata/dev-harness/harness_runs/20260503_203141/`
  - Command: `python3 tools/openclaw_harness/startup_harness.py probe bandit.extortion_first_demand_pay_success_save_mcw` from the clean proof worktree on the same rebuilt `d1a4f076c8` binary.
  - `probe.report.json`: `evidence_class=feature-path`, `feature_proof=true`, `verdict=artifacts_matched`; step ledger `green_step_local_proof` 17/17.
  - This row reaches the real trade UI before any writeback credit: `shakedown_trade_ui opened demanded=15797 reachable=45134 player_pool=3211 nearby_npc_pool=5222 scene_pool=36701 trader=4 trade_api=npc_trading::trade title=Pay:`.
  - Paid artifacts after the UI path: `shakedown_trade_ui result=paid demanded=15797 reachable=45134 trader=4` and `shakedown_surface paid toll=15797 demanded=15797 reachable=45134`.
  - Save/writeback gates: saved-player mtime changed after the save request (`2026-05-03T20:33:29.986098`); saved bandit metadata has site `overmap_special:bandit_camp@140,51,0`, `member_count=14`, `ready_at_home_count=14`, `active_outside_count=0`, empty active group, empty active target, and empty active job.

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

Superseded/no-credit rows: the pre-nudge rows `20260503_171632`, `20260503_171825`, and `20260503_172007` are not credited as final Slice 1 proof because they were stale/insufficient for Josef's live-red visible-options report. The loose later rows and historical `20260503_180929`/`175512` rows remain useful forensic/supporting evidence. The tight cropped-OCR row `20260503_202326` and successful writeback row `20260503_203141` on rebuilt `d1a4f076c8` remain the first UI-first closure after Josef's no-credit nudge, but the latest current-version receipt after the forced rebuild is the `7e5a506c76` master-profile pair: cancel/refusal `20260503_213831` and successful Pay/save/writeback `20260503_214015`. The `20260503_191704`, `20260503_192442`, and `20260503_192911` rows remain supporting branch evidence after the trade UI path is established.

## Caveats / next slice boundary

This checkpoint proves the visible Pay/Fight-only fork, real trade-window opening path, whole camp-side pool footing, pool-derived demand basis, cancel/refusal mapping, item-offer/autobalance, and successful-payment writeback. The latest three-source row `20260503_233823` additionally proves player inventory, basecamp-assigned NPC inventory, and basecamp storage-zone inventory inside the same open Pay trade window before paid acceptance; the rebuilt `7e5a506c76` rows remain the clean master cancel/success receipts. It does not claim full natural-discovery coverage or broad diplomacy/payment redesign, but the previous fake selector / silent-confiscation Pay surface is no longer the active product path.
