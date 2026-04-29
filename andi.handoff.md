# Andi handoff: roof-fire horde detection proof

## Active target

`Roof-fire horde detection proof packet v0` is the active slice under `C-AOL actual playtest verification stack v0`.

The previous `Player-lit fire and bandit signal verification packet v0` is complete/green. Do **not** rerun it as ritual and do **not** treat its source-zone fire/signal evidence as roof/horde proof.

Green player-lit source-zone fire -> bandit signal proof to preserve:

- source real-fire run: `.userdata/dev-harness/harness_runs/20260429_153253/`
- signal fixture: `player_lit_fire_bandit_signal_wait_v0_2026-04-29`
- signal scenario: `bandit.player_lit_fire_signal_wait_mcw`
- signal run: `.userdata/dev-harness/harness_runs/20260429_162100/`
- result: `verdict=artifacts_matched`, `evidence_class=feature-path`, `feature_proof=true`, step ledger 14/14 green, wait ledger green
- proof: saved real player-created `f_brazier` + `fd_fire`, mineral water plus `AUTO_DRINK` support, real wait UI 30-minute choice, saved turn delta `1800`, same-run live smoke/fire signal scan/maintenance, matched `bandit_camp`, dispatch plan `candidate_reason=live_signal`, and saved active `bandit_camp` scout response with `known_recent_marks` including `live_smoke@140,41,0`
- canonical proof doc: `doc/player-lit-fire-bandit-signal-verification-v0-2026-04-29.md`

Canonical anchors:

- `Plan.md`
- `TODO.md`
- `TESTING.md`
- `SUCCESS.md`
- stack contract: `doc/c-aol-actual-playtest-verification-stack-v0-2026-04-27.md`
- green signal proof: `doc/player-lit-fire-bandit-signal-verification-v0-2026-04-29.md`
- green fire proof: `doc/fuel-visible-brazier-source-zone-firestarter-action-v0-2026-04-29.md`

## Current work

Shape and run the roof/horde packet. The missing proof is not source-zone fire and not bandit signal response; it is roof/elevated footing plus real player-created roof fire/light/smoke plus horde before/after detection or response metadata.

1. Prove or stage the horde/distance setup. Debug may be used for the horde/distance side.
2. Prove the player is actually on a roof/elevated position before fire creation.
3. Create the credited roof fire/light/smoke through a real player action path. Do not debug-inject the credited fire.
4. Run bounded time passage. Require before/after turns or exact blocker prompt screenshots/key handling.
5. Inspect horde before/after detection or response metadata/artifacts.
6. A proven no-detect/no-response outcome is acceptable if roof footing, real fire/light/smoke, elapsed time, and horde metadata are all decisive.

## Evidence expectations

Minimum packet:

- command/scenario and run directory;
- horde/distance setup evidence;
- roof/elevated player-position evidence;
- real player-created roof fire/light/smoke evidence;
- bounded wait/time-passage evidence or exact blocker prompt screenshot/key handling;
- horde before/after detection or response metadata/artifacts;
- clear verdict: horde response, no-response, or blocked/inconclusive, with evidence class boundaries intact.

## Non-goals/cautions

- Do not rerun solved player-lit source-zone fire -> bandit signal proof.
- Do not use `fuel_writeback_source_zone_v0_2026-04-29`.
- Do not claim roof/horde proof from saved source-zone `fd_fire` or bandit signal metadata.
- Do not debug-inject the credited roof fire/light/smoke.
- Do not call a blocked wait a no-response result until the warning/prompt screenshot is captured and classified.
- Keep natural three/four-site player-pressure behavior and true zero-site idle baseline as decision/watchlist items unless Schani/Josef explicitly promotes them.
