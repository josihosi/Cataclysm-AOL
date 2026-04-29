# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active target: `Fuel writeback repair via wood source zone packet v0` under `C-AOL actual playtest verification stack v0`.

Current state: Josef promoted the fuel/writeback repair path on 2026-04-29. Brazier deployment remains the already-green footing; the new slice bypasses the brittle Multidrop filtered-`2x4` selector by creating an oversized in-world firewood source, preferably logs first, placing/verifying a broad source-firewood zone, and then proving normal player-action ignition plus saved `fd_fire`/smoke/light writeback. The live AI performance audit remains complete as **green enough for current playtest scale** and should not be rerun as ritual.

Canonical anchors for the active target:

- Stack contract: `doc/c-aol-actual-playtest-verification-stack-v0-2026-04-27.md`.
- Active fuel repair packet: `doc/fuel-writeback-repair-via-wood-source-zone-packet-v0-2026-04-29.md`.
- Fuel repair imagination source: `doc/fuel-writeback-repair-via-wood-source-zone-imagination-source-of-truth-2026-04-29.md`.
- Completed performance matrix: `doc/c-aol-live-ai-performance-matrix-v0-2026-04-29.md`.
- Performance audit contract: `doc/c-aol-live-ai-performance-audit-packet-v0-2026-04-28.md`.
- Performance imagination source: `doc/c-aol-live-ai-performance-imagination-source-of-truth-2026-04-28.md`.
- Completed cannibal confidence matrix: `doc/cannibal-camp-confidence-push-live-playtest-matrix-v0-2026-04-29.md`.

Next narrow work queue:

1. Build/verify a named fuel-repair probe using the already-green brazier deployment as footing.
2. Spawn an oversized saved-world firewood source near the brazier, preferably logs first; use planks/`2x4` only if logs do not satisfy the intended firewood path.
3. Place/verify a broad source-firewood zone over/around that pile, then prove normal player fire-start/lighter action, guarded save/writeback, actual `fd_fire`/smoke/light state, and only then bandit signal response.
4. Do not rerun the performance rows unless Schani/Josef asks for a natural player-pressure behavior test or a true zero-site idle baseline.

Proof discipline:

- This fuel repair is a bridge to real player-lit fire proof, not a debug `fd_fire` shortcut.
- Setup helpers may create the wood pile/zone footing, but closure requires normal player ignition and saved fire/smoke/light writeback.
- Do not promote roof-horde work while real player-lit fire remains unproven.
