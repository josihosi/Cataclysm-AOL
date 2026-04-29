# Andi handoff: player-lit fire -> bandit signal proof

## Active target

`Player-lit fire and bandit signal verification packet v0` is the active slice under `C-AOL actual playtest verification stack v0`.

The source-zone fire prerequisite is now green. Preserve run `.userdata/dev-harness/harness_runs/20260429_153253/` as the current green real-fire source: normal map UI -> normal Apply inventory `brazier` -> `Deploy where?` -> east/right -> normal Apply inventory `lighter` -> exact UI trace `Light where?` -> east/right target -> `SOURCE_FIREWOOD` prompt -> uppercase `Y` -> recognizable ignition OCR -> save prompt -> mtime advance -> saved target tile `f_brazier` + `fd_fire`.

Do not reopen the old broken proof surface `fuel_writeback_source_zone_v0_2026-04-29`.

Canonical anchors:

- `Plan.md`
- `TODO.md`
- `TESTING.md`
- `SUCCESS.md`
- stack contract: `doc/c-aol-actual-playtest-verification-stack-v0-2026-04-27.md`
- green fire proof: `doc/fuel-visible-brazier-source-zone-firestarter-action-v0-2026-04-29.md`
- visible deployed-brazier gate: `doc/fuel-visible-brazier-source-zone-gate-v0-2026-04-29.md`
- fuel repair packet: `doc/fuel-writeback-repair-via-wood-source-zone-packet-v0-2026-04-29.md`

## Current work

Prove bandit signal response from real player-created fire. The next missing proof is not fire creation; it is bounded wait/time passage plus claim-scoped bandit response/metadata.

1. Start from `.userdata/dev-harness/harness_runs/20260429_153253/` or a fresher equivalent green real-fire source.
2. Stage survivable long-wait footing: spawn/provide mineral water and set up a Smart Zone / auto-drink zone around the player so thirst does not become the whole proof.
3. Record before time/turn and relevant bandit/signal baseline metadata.
4. Start the long-wait path.
5. If time does not pass, screenshot the blocker/warning prompt and classify that exact prompt. Do not report merely “time did not pass” without the warning screenshot.
6. If a wait-interruption prompt offers `I` or `N`, prefer `I` over `N`. The same prompt class may recur multiple times during one long wait; screenshot each distinct prompt class before responding, and do not blind-spam through unknown warnings.
7. After waiting, record after time/turn and claim-scoped bandit signal response/metadata.
8. If no bandit response occurs despite elapsed time, classify that as a no-response outcome with bounded-time evidence, not as fire proof failure.

## Evidence expectations

Minimum packet:

- command/scenario and run directory;
- green real-fire source reference or fresh equivalent;
- mineral-water / auto-drink Smart Zone setup evidence;
- before/after game time or turn;
- screenshot artifact paths for every distinct warning/prompt that blocks or interrupts waiting;
- exact key chosen for wait prompts (`I` over `N` where both are offered);
- bandit signal response/metadata, or a bounded no-response classification with elapsed-time proof.

## Non-goals/cautions

- Do not rerun solved fire/lighter proof as ritual.
- Do not use `fuel_writeback_source_zone_v0_2026-04-29`.
- Do not claim bandit signal from saved `fd_fire` alone.
- Do not call a blocked wait a no-response result until the warning/prompt screenshot is captured and classified.
- Keep roof-fire horde detection separate; it still needs roof/elevated-position plus real player-created roof fire/light/smoke and horde response metadata.
