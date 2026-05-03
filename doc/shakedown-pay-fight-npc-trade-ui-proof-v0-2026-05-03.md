# Shakedown Pay/Fight + NPC trade UI proof — 2026-05-03

Status: GREEN FEATURE-PATH CHECKPOINT for the shakedown surface/open-payment contract inside `CAOL-JOSEF-LIVE-DEBUG-BATCH-v0`.

## Claim proven

- First-demand bandit shakedown shows the player-visible Pay/Fight fork, with no visible `Refuse` third option.
- Selecting Pay opens the real NPC trade UI path (`npc_trading::trade` / `trade_ui`) with `Pay:` debt before any hidden surrender.
- The Pay window is wired to the broader honest camp-side pool: avatar carried goods, nearby ally/NPC carried goods, and nearby scene/basecamp goods.
- Fight remains a distinct branch.
- Reopened demand still shows Pay/Fight with the higher bounded reopened toll.

## Evidence

Gates:

- `git diff --check` -> green after final doc/scenario edits; log `/tmp/caol_shakedown_gates_quick_20260503_r2.log`.
- `python3 -m py_compile tools/openclaw_harness/startup_harness.py` -> green; log `/tmp/caol_shakedown_gates_quick_20260503.log`.
- `make -j4 obj/do_turn.o src/npctrade.o src/trade_ui.o src/bandit_live_world.o tests/bandit_live_world_test.o LINTJSON=0 ASTYLE=0` -> green/up to date; log `/tmp/caol_shakedown_gates_tests_20260503.log`.
- `make -j4 tests LINTJSON=0 ASTYLE=0 && ./tests/cata_test "[bandit][live_world][shakedown]"` -> green, 139 assertions / 4 cases; log `/tmp/caol_shakedown_gates_tests_20260503.log`.
- `make -j4 TILES=1 cataclysm-tiles LINTJSON=0 ASTYLE=0` -> green/up to date; log `/tmp/caol_shakedown_tiles_20260503.log`.
- Scenario JSON parse for `bandit.extortion_*_mcw.json` -> green; log `/tmp/caol_shakedown_gates_quick_20260503_r2.log`.

Live/staged harness rows:

- `bandit.extortion_first_demand_pay_mcw` -> `.userdata/dev-harness/harness_runs/20260503_171632/`
  - `probe.report.json`: `evidence_class=feature-path`, `feature_proof=true`, `verdict=artifacts_matched`.
  - `probe.step_ledger.json`: `green_step_local_proof`, 6/6 green rows.
  - Debug artifact: `shakedown_surface_dialogue_window opening=basecamp_pressure responses=pay/fight payment_surface=npc_trade_ui`.
  - Debug artifact: `shakedown_trade_ui opened demanded=15797 reachable=45134 player_pool=3211 nearby_npc_pool=5222 scene_pool=36701 trader=4 trade_api=npc_trading::trade title=Pay:`.
  - Screenshot: `choose_pay.after.png` opens the trade UI; manual OCR receipt `choose_pay.after.screen_text.manual.txt` includes `Pay`, `F1 to auto balance with highlighted item`, `You`, `Selected`, and visible item rows.

- `bandit.extortion_first_demand_fight_mcw` -> `.userdata/dev-harness/harness_runs/20260503_171825/`
  - `probe.report.json`: `evidence_class=feature-path`, `feature_proof=true`, `verdict=artifacts_matched`.
  - Proves first-demand Pay/Fight surface plus distinct `shakedown_surface fight demanded=15797 reachable=45134` branch.

- `bandit.extortion_reopened_demand_mcw` -> `.userdata/dev-harness/harness_runs/20260503_172007/`
  - `probe.report.json`: `evidence_class=feature-path`, `feature_proof=true`, `verdict=artifacts_matched`.
  - Proves reopened `responses=pay/fight payment_surface=npc_trade_ui`, `demanded_toll=22116`, and `renegotiation reopen: previous defender loss raises this one bounded demand`.

## Caveats / next slice boundary

This checkpoint proves the visible fork and real trade-window opening path. It does not claim full natural-discovery coverage or broad diplomacy/payment redesign. Future rows may still add saved successful-payment/writeback proof after item selection, but the previous fake selector / silent-confiscation Pay surface is no longer the active product path.
