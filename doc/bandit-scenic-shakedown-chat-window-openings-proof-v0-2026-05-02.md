# CAOL-BANDIT-SCENIC-SHAKEDOWN-CHAT-OPENINGS-v0 proof (2026-05-02)

Status: CLOSED AS SCENIC-UI PROOF / SUPERSEDED BY `CAOL-JOSEF-LIVE-DEBUG-BATCH-v0` FOR FINAL RESPONSE + PAYMENT CONTRACT

Contract: `doc/bandit-scenic-shakedown-chat-window-openings-packet-v0-2026-05-01.md`.

## Result

The live bandit shakedown surface now uses the normal `dialogue_window` presentation instead of the previous bare `uilist` toll menu. It opens with contextual scenic beats. This proof remains credited only for the scenic dialogue-window/opening path and cannibal/no-shakedown regression. Its recorded `Pay / Fight / Refuse` response contract and hidden pay-surrender behavior are now superseded by `CAOL-JOSEF-LIVE-DEBUG-BATCH-v0`: Josef's stranded 2026-05-02/03 notes require visible Pay/Fight only, with backout/refuse entering the hostile branch, and Pay opening a trade/debt-style payment surface over the honest basecamp/faction-side pool.

Implemented opening IDs:

- `basecamp_pressure`
- `warning_from_cover`
- `weakness_read`
- `roadblock_toll`
- `reopened_demand`

## Credited gates

- `git diff --check` -> clean.
- `make -j4 tests src/do_turn.o src/bandit_live_world.o tests/bandit_live_world_test.o LINTJSON=0 ASTYLE=0` -> green.
- `./tests/cata_test "[bandit][live_world][shakedown]"` -> `All tests passed (136 assertions in 4 test cases)`.
- Captured gate log: `/tmp/caol-bandit-scenic-narrow-gate-20260502.log`.

## Credited live/harness evidence

### First-demand dialogue-window row

Scenario: `bandit.extortion_first_demand_pay_mcw`.

Run: `.userdata/dev-harness/harness_runs/20260502_065253/`.

Evidence class: `feature-path`; status `green`; verdict `artifacts_matched`; feature proof `true`; runtime warnings `[]`.

Key matched artifacts:

- `shakedown_surface`
- `profile=camp_style`
- `posture=open_shakedown`
- `demanded_toll=15797`
- `shakedown_surface_dialogue_window opening=basecamp_pressure responses=pay/fight/refuse`
- `pay_option=yes fight_option=yes`
- `shakedown_surface paid toll=`

Optical artifacts:

- `.userdata/dev-harness/harness_runs/20260502_065253/advance_final_turn_to_first_shakedown.after.png` (`2560x1440`)
- Review copy: `/Users/josefhorvath/.openclaw/workspace/runtime/caol-bandit-scenic-review-20260502/first-demand-dialogue-065253.png`

Visible fact recorded by scenario: bandit shakedown dialogue window visible with the basecamp-pressure opening after the final local-contact turn. The recorded player-facing `pay/fight/refuse` choices are superseded and must not be cited as final response-contract proof.

### Reopened higher-demand dialogue-window row

Scenario: `bandit.extortion_reopened_demand_mcw`.

Run: `.userdata/dev-harness/harness_runs/20260502_065445/`.

Evidence class: `feature-path`; status `green`; verdict `artifacts_matched`; feature proof `true`; runtime warnings `[]`.

Key matched artifacts:

- `shakedown_surface`
- `profile=camp_style`
- `posture=open_shakedown`
- `demanded_toll=22116`
- `shakedown_surface_dialogue_window opening=reopened_demand responses=pay/fight/refuse`
- `renegotiation reopen: previous defender loss raises this one bounded demand`
- `pay_option=yes fight_option=yes`

Optical artifacts:

- `.userdata/dev-harness/harness_runs/20260502_065445/advance_final_turn_to_reopened_shakedown.after.png` (`2560x1440`)
- Review copy: `/Users/josefhorvath/.openclaw/workspace/runtime/caol-bandit-scenic-review-20260502/reopened-demand-dialogue-065445.png`

Visible fact recorded by scenario: reopened bandit shakedown dialogue window visible with the reopened-demand opening after the final local-contact turn. The recorded player-facing `pay/fight/refuse` choices are superseded and must not be cited as final response-contract proof.

### No-shakedown regression row

Scenario: `cannibal.live_world_exposed_sight_avoid_mcw`.

Run: `.userdata/dev-harness/harness_runs/20260502_065927/`.

Evidence class: `feature-path`; status `green`; verdict `artifacts_matched`; feature proof `true`; runtime warnings `[]`.

Key matched artifacts:

- `local_gate site=overmap_special:cannibal_camp@`
- `active_job=stalk`
- `profile=cannibal_camp`
- `posture=hold_off`
- `current_exposure=no`
- `recent_exposure=yes`
- `sight_exposure=recent`
- `visible beeline`
- `shakedown=no`
- `combat_forward=no`

## Scope and caveats

- This is staged-but-live McWilliams harness feature proof, not a natural random-discovery claim.
- The change is product-UX presentation plus contextual openings only; it does not redesign bandit economy, toll math, camp risk/reward, cannibal behavior, or release packaging.
- Superseded caveat: Pay must no longer be considered correct if it silently surrenders demanded reachable goods; the active contract requires a trade/debt-style payment surface. Fight/refuse equivalence remains correct, but `Refuse` should not be shown as a third visible response.
