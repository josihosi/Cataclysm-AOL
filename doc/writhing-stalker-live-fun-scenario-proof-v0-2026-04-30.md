# Writhing stalker live fun-scenario proof v0 — 2026-04-30

Status: **GREEN V0 live-shaped scenario packet** for `CAOL-WRITHING-STALKER-LIVE-FUN-SCENARIOS-v0`.

Contract: `doc/writhing-stalker-live-fun-scenarios-packet-v0-2026-04-30.md`.
Imagination source: `doc/writhing-stalker-live-fun-scenarios-imagination-source-of-truth-2026-04-30.md`.
Prior deterministic proof: `doc/writhing-stalker-behavior-pattern-proof-v0-2026-04-30.md`.

## Gates

- Code/tuning repair: `src/monmove.cpp` no longer refreshes `effect_run` on every `cooling_off` live-plan turn. This was the smallest live-path fix needed to let cooldown breathe out instead of becoming permanent cooling-off after a strike.
- Deterministic support: `./tests/cata_test "[writhing_stalker]"` -> `All tests passed (129 assertions in 10 test cases)`.
- Live binary: rebuilt with `make -j4 TILES=1 LINTJSON=0 ASTYLE=0`; final harness runs used fresh `cataclysm-tiles` from this tree.
- All credited runs below are `feature-path`, `feature_proof: true`, `verdict: artifacts_matched`, with green step-local ledgers and no runtime warnings/aborts in `probe.report.json`.

Caveat shared by all rows: these are staged-but-live McWilliams harness scenarios. They prove the live `monster::plan()` -> `apply_writhing_stalker_plan()` -> `writhing_stalker::evaluate_live_response()` game path under named setup, not fully natural random discovery.

## Credited rows

### Scenario A — campfire/light counterplay

- Scenario/run: `writhing_stalker.live_campfire_counterplay_mcw`, `.userdata/dev-harness/harness_runs/20260430_233129/`.
- Artifact path: `.userdata/dev-harness/harness_runs/20260430_233129/probe.artifacts.log`.
- Start-state facts: fixture `mcwilliams_live_debug_noon_stalker_2026-04-30`; saved turn transformed to noon (`5227200`); one hostile `mon_writhing_stalker` at player offset `[7,0,0]`, hp `90`, faction `zombie`; no zombies; bright/focused counterplay footing.
- Trace:
  - `target_probe ... target=yes ... mon_plan_dist=7 sees_player=yes`
  - `decision=withdraw route=hold_exposed reason=live_exposed_and_focused_withdraw ... distance=7 zombie_pressure=0 stalker_bright=yes target_focus=yes cooldown=no eval_us=12`
  - `decision=cooling_off route=none reason=live_latch_cooldown_blocks_relatched_pressure ... distance=6 cooldown=yes eval_us=11`
- Verdict: **PASS / GREEN**. Light/focus counterplay causes withdrawal/cooldown and the scenario forbids same-run strike lines.
- Stability/perf: `eval_us` in cited lines `11–12`; no runtime warnings/abort.
- Caveat: campfire is represented by bright/focused daylight footing rather than a hand-lit fire actor.

### Scenario B — alley predator / shadow route rhythm

- Scenario/run: `writhing_stalker.live_alley_predator_mcw`, `.userdata/dev-harness/harness_runs/20260430_233156/`.
- Artifact path: `.userdata/dev-harness/harness_runs/20260430_233156/probe.artifacts.log`.
- Start-state facts: fixture `mcwilliams_live_debug_vulnerable_stalker_repeat_2026-04-30`; midnight turn (`5270400`); player at `60%` hp, stamina `1000`, `bleed:torso`; one hostile `mon_writhing_stalker` at offset `[8,0,0]`, hp `90`.
- Trace:
  - `target_probe ... target=yes ... mon_plan_dist=5 sees_player=yes`
  - `decision=shadow route=cover_shadow reason=live_shadowing_before_strike_window ... distance=5 cooldown=no eval_us=16`
  - `decision=strike route=cover_shadow reason=live_vulnerability_window_strike ... distance=2 cooldown=no eval_us=18`
  - `decision=cooling_off route=none reason=live_latch_cooldown_blocks_relatched_pressure ... distance=1 cooldown=yes eval_us=7`
  - later re-engagement: `decision=shadow route=cover_shadow reason=live_shadowing_believable_evidence ... distance=7 cooldown=no eval_us=9`
- Verdict: **PASS / GREEN**. Cover-shadow approach, plausible close strike, cooldown breath, and later re-engagement are present without instant open beeline/spam.
- Stability/perf: cited live-plan `eval_us` `7–18`; no runtime warnings/abort.
- Caveat: second strike is proven more strongly in the zombie-pressure row; this row proves post-cooldown re-engagement in the alley predator shape.

### Scenario C — zombie distraction without magic

Positive row:

- Scenario/run: `writhing_stalker.live_zombie_distraction_mcw`, `.userdata/dev-harness/harness_runs/20260430_233521/`.
- Artifact path: `.userdata/dev-harness/harness_runs/20260430_233521/probe.artifacts.log`.
- Start-state facts: fixture `mcwilliams_live_debug_vulnerable_zombie_stalker_2026-04-30`; midnight turn; player at `60%` hp with `bleed:torso`; one hostile stalker at `[8,0,0]` hp `90`; two hostile `mon_zombie` at `[0,8,0]` and `[0,-8,0]`, hp `80` each.
- Trace:
  - `target_probe ... target=yes ... mon_plan_dist=5 sees_player=yes`
  - `decision=shadow route=cover_shadow reason=live_shadowing_before_strike_window opportunity=88 evidence=yes distance=5 zombie_pressure=2 ... eval_us=12`
  - `decision=strike route=cover_shadow reason=live_vulnerability_window_strike opportunity=88 evidence=yes distance=2 zombie_pressure=2 ... eval_us=10`
  - `decision=cooling_off ... distance=1 zombie_pressure=1 cooldown=yes eval_us=10`
  - second rhythm: `decision=shadow ... distance=7 zombie_pressure=1 ... eval_us=13`; `decision=strike ... distance=3 zombie_pressure=1 ... eval_us=15`
- Verdict: **PASS / GREEN**. Zombie pressure amplifies an already evidenced/vulnerable window and supports repeated strike rhythm with cooldown between strikes.
- Stability/perf: cited live-plan `eval_us` `10–15`; no runtime warnings/abort.
- Caveat: pressure count can drop from `2` to `1` as zombies move during the live approach; the first shadow/strike still proves two-zombie pressure at the decision point.

Negative no-magic guard:

- Scenario/run: `writhing_stalker.live_zombie_distraction_no_magic_guard_mcw`, `.userdata/dev-harness/harness_runs/20260430_233335/`.
- Artifact path: `.userdata/dev-harness/harness_runs/20260430_233335/probe.artifacts.log`.
- Start-state facts: fixture `mcwilliams_live_debug_zombie_no_magic_2026-04-30`; clean noon/no-human-target footing; opaque locker wall; stalker at `[10,0,0]`; zombies at `[0,8,0]` and `[0,-8,0]`.
- Trace: `target_probe ... target=no target_type=none target_name=none target_pos=none mon_plan_dist=35 sees_player=no ... turns_since_target=1`.
- Verdict: **PASS / GREEN**. Zombies near the player do not grant magic target acquisition through the wall; the scenario forbids `target=yes`, `sees_player=yes`, and live-plan lines.
- Stability/perf: no runtime warnings/abort.
- Caveat: negative guard is an intentionally synthetic opaque-wall setup.

### Scenario D — door/light escape

- Scenario/run: `writhing_stalker.live_door_light_escape_mcw`, `.userdata/dev-harness/harness_runs/20260430_233405/`.
- Artifact path: `.userdata/dev-harness/harness_runs/20260430_233405/probe.artifacts.log`.
- Start-state facts: fixture `mcwilliams_live_debug_noon_stalker_2026-04-30`; saved turn noon; one hostile stalker at `[7,0,0]`, hp `90`; light/focus escape-equivalent footing.
- Trace:
  - `target_probe ... target=yes ... mon_plan_dist=7 sees_player=yes`
  - `decision=withdraw route=hold_exposed reason=live_exposed_and_focused_withdraw ... distance=7 stalker_bright=yes target_focus=yes cooldown=no eval_us=15`
  - `decision=cooling_off route=none reason=live_latch_cooldown_blocks_relatched_pressure ... distance=7 stalker_bright=yes target_focus=yes cooldown=yes eval_us=9`
- Verdict: **PASS / GREEN**. Readable light/focus escape breaks pressure into withdraw/cooling and forbids same-run strike.
- Stability/perf: cited `eval_us` `9–15`; no runtime warnings/abort.
- Caveat: door/light is represented by the same bright/focused escape lever rather than a manually operated door macro.

### Scenario E — wounded predator retreat

- Scenario/run: `writhing_stalker.live_wounded_predator_mcw`, `.userdata/dev-harness/harness_runs/20260430_233434/`.
- Artifact path: `.userdata/dev-harness/harness_runs/20260430_233434/probe.artifacts.log`.
- Start-state facts: fixture `mcwilliams_live_debug_wounded_stalker_2026-04-30`; midnight turn; vulnerable player (`60%` hp, `bleed:torso`); one wounded hostile stalker at `[4,0,0]`, hp `45`.
- Trace:
  - `target_probe ... target=yes ... mon_plan_dist=4 sees_player=yes`
  - `decision=withdraw route=cover_shadow reason=live_stalker_hurt_withdraw opportunity=29 evidence=yes distance=4 zombie_pressure=0 cooldown=no eval_us=17`
  - `decision=cooling_off route=none reason=live_latch_cooldown_blocks_relatched_pressure ... distance=5 cooldown=yes eval_us=6`
- Verdict: **PASS / GREEN**. Injured retreat overrides greed despite vulnerability/evidence, and the scenario forbids strike snapback during the wounded-retreat window.
- Stability/perf: cited `eval_us` `6–17`; no runtime warnings/abort.
- Caveat: injured HP is staged directly in the fixture transform rather than produced by a manual attack macro.

## Overall verdict

`CAOL-WRITHING-STALKER-LIVE-FUN-SCENARIOS-v0` is **green for v0**. The live-shaped rows prove fair dread instead of omniscient unfairness: counterplay can break pressure, shadow routes can produce plausible close strikes, cooldown prevents immediate spam, zombie pressure helps only with evidence/vulnerability, repeated strike rhythm exists, and wounded retreat overrides greed.

The remaining future-only stricter bar would be a fully natural discovery/manual playtest packet, not a blocker for this staged-but-live v0 proof.
