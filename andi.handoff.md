# Andi handoff: fuel writeback repair via wood source zone packet v0

## Active target

`Fuel writeback repair via wood source zone packet v0` is the active execution slice under `C-AOL actual playtest verification stack v0`.

Josef promoted the repair on 2026-04-29: brazier deployment is okay/green; stop trying to force the old Multidrop filtered-`2x4` selector. Instead, spawn an oversized in-world firewood source, preferably logs first, place/verify a broad source-firewood zone, and prove normal player-action ignition plus saved `fd_fire`/smoke/light writeback.

Canonical anchors:

- `Plan.md`
- `TODO.md`
- `TESTING.md`
- `SUCCESS.md`
- `doc/c-aol-actual-playtest-verification-stack-v0-2026-04-27.md`
- active fuel repair packet: `doc/fuel-writeback-repair-via-wood-source-zone-packet-v0-2026-04-29.md`
- fuel repair imagination source: `doc/fuel-writeback-repair-via-wood-source-zone-imagination-source-of-truth-2026-04-29.md`
- completed performance matrix: `doc/c-aol-live-ai-performance-matrix-v0-2026-04-29.md`
- performance contract: `doc/c-aol-live-ai-performance-audit-packet-v0-2026-04-28.md`
- performance imagination source: `doc/c-aol-live-ai-performance-imagination-source-of-truth-2026-04-28.md`
- completed cannibal confidence support: `doc/cannibal-camp-confidence-push-live-playtest-matrix-v0-2026-04-29.md`

## Current work

Build one named probe for the fuel repair slice:

1. Reuse the already-green brazier deployment footing, or rerun only if fresh current-runtime evidence is needed; saved-state inspection must show the expected `f_brazier`.
2. Create an oversized real wood source near the brazier: prefer logs first; fallback to planks/`2x4` only if logs fail the intended firewood path. Prove the items in saved-world metadata.
3. Place/verify a broad source-firewood zone over/around the pile by visible UI or zone metadata. This is a practical fuel footing, not Smart Zone Manager closure.
4. Run the normal player fire-start/lighter action. Do not use debug map-editor `fd_fire` or synthetic loaded-map fields as closure. Capture any narrow in-game lighting message/log line as bridge evidence if it exists.
5. Optional if cheap/clean: winter or cold exposure can add character-warmth log lines after bounded time passage as liveliness evidence, but this is adjunct only and must not replace saved-state proof.
6. Require guarded post-ignition save/writeback with actual `fd_fire` plus smoke/light-relevant state. If green, continue into bounded wait/time passage and bandit signal response or a clearly classified no-response outcome.

Prior completed/downstream stack state remains in force:

- Performance audit is complete as green enough for current playtest scale; do not rerun rows as ritual.
- Cannibal confidence-push is complete as confidence uplift green; do not reopen it unless Josef/Schani explicitly reopens the packet.
- Smart Zone Manager remains implemented-but-unproven / Josef playtest package. Final agent-side evidence remains `.userdata/smart-zone-ui-entry-current-runtime-20260429c/harness_runs/20260429_005345/`: delivered default `Y` dispatched as `raw_action="action_menu" action_id="action_menu"`, no `invoke_zone_manager`, OCR missing `Zones manager`, and no add-zone/filter/generation/coordinate-label proof credited.
- The old Multidrop fuel continuation remains red/non-green at `blocked_untrusted_drop_filter_or_inventory_visibility`; do not credit that old path, but do not let it latch or block the new wood-source-zone repair route.
- Roof-fire horde proof remains downstream behind real player-lit fire; do not treat it as a separate parked/decision latch while this repair slice is active.

## Non-goals/cautions

- Do not rerun the completed performance rows as ritual.
- Do not turn the performance audit into behavior redesign or broad refactor.
- Do not reopen Smart Zone Manager live proof unless Josef/Schani explicitly provides or approves a materially repaired UI-entry/key-delivery primitive or Josef manual evidence.
- Do not treat deterministic `clzones` geometry or stale-window harness artifacts as live feature closure.
- Do not jump to roof-horde proof while real player-lit fire remains unproven.
