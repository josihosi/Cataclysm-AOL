# Andi handoff: C-AOL actual playtest verification stack v0

## Active target

`C-AOL actual playtest verification stack v0` remains active, but it is currently at a **post-performance decision boundary** rather than an execution slice.

Current canon says the `C-AOL live AI performance audit packet v0` is complete as **green enough for current playtest scale**. Do not treat performance matrix setup, extra performance rows, or additional timing instrumentation as the next work unless Schani/Josef explicitly promotes a new row such as natural multi-site player-pressure behavior or a true zero-site idle baseline.

Canonical anchors:

- `Plan.md`
- `TODO.md`
- `TESTING.md`
- `SUCCESS.md`
- `doc/c-aol-actual-playtest-verification-stack-v0-2026-04-27.md`
- completed performance matrix: `doc/c-aol-live-ai-performance-matrix-v0-2026-04-29.md`
- performance contract: `doc/c-aol-live-ai-performance-audit-packet-v0-2026-04-28.md`
- performance imagination source: `doc/c-aol-live-ai-performance-imagination-source-of-truth-2026-04-28.md`
- completed cannibal confidence support: `doc/cannibal-camp-confidence-push-live-playtest-matrix-v0-2026-04-29.md`

## Boundary just reached

The performance packet is closed for the current scope. Credited rows cover one-site, two-site, pre-staged three-site, and pre-staged four-site live-path performance measurement. Latest credited four-site evidence is `.userdata/dev-harness/harness_runs/20260429_041936/`, scenario `performance.four_site_prestaged_active_wait_30m`: feature-path classification, 7/7 green step ledger, green 30m wait ledger, preflight-proven distinct active non-player-target jobs, and same-run `sites=4 active_sites=4 active_job_mix=camp_style:stalk=1,cannibal_camp:stalk=3 signals=0` counters. Four-site timing stayed compact: six perf cadence rows, `total_us` min/median/max `540/560.0/5572`, sum `8360`, one dispatch-due row `dispatch_us=4654`, harness wall-clock `/tmp/caol_perf_four_prestaged_20260429_041936.log` `real 39.27s`.

Evidence boundary: the pre-staged three/four-site rows are performance-load proof, not natural multi-site player-pressure dispatch proof. The direct-range natural three-site row remains cap/watchlist evidence (`sites=3 active_sites=2`), not a failed performance-load row.

Prior completed/blocked stack state remains in force:

- Cannibal confidence-push is complete as confidence uplift green; do not reopen it unless Josef/Schani explicitly reopens the packet.
- Smart Zone Manager remains implemented-but-unproven / Josef playtest package. Final agent-side evidence remains `.userdata/smart-zone-ui-entry-current-runtime-20260429c/harness_runs/20260429_005345/`: delivered default `Y` dispatched as `raw_action="action_menu" action_id="action_menu"`, no `invoke_zone_manager`, OCR missing `Zones manager`, and no add-zone/filter/generation/coordinate-label proof credited.
- Fuel continuation remains red/non-green at `blocked_untrusted_drop_filter_or_inventory_visibility`; no post-fuel save/writeback, lighter, or `fd_fire` proof is credited.
- Player-lit fire and roof-fire horde proof remain blocked behind that fuel/writeback gate and real player-lit fire respectively.

## Next work

There is currently **no sensible unblocked Andi execution target** in this stack.

Recommended next action is a Schani/Josef planning decision:

1. Promote a fuel/writeback repair slice so player-lit fire can reopen.
2. Explicitly greenlight a natural multi-site player-pressure behavior/cap review or a true zero-site idle baseline if that product question matters.
3. Name the next non-fire C-AOL target outside this completed stack.

Until one of those is promoted in canon, this handoff is a decision-boundary note, not a work queue.

## Non-goals/cautions

- Do not rerun the completed performance rows as ritual.
- Do not turn the performance audit into behavior redesign or broad refactor without a newly promoted measured cause.
- Do not reopen Smart Zone Manager live proof unless Josef/Schani explicitly provides or approves a materially repaired UI-entry/key-delivery primitive or Josef manual evidence.
- Do not treat deterministic `clzones` geometry or stale-window harness artifacts as live feature closure.
- Do not jump to player-lit fire or roof-horde proof while the fuel/writeback gate remains blocked.
