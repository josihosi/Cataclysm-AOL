# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active target: `Bandit structural bounty overmap completion packet v0`.

Canonical contract: `doc/bandit-structural-bounty-overmap-completion-packet-v0-2026-04-30.md`.
Imagination source: `doc/bandit-structural-bounty-overmap-completion-imagination-source-of-truth-2026-04-30.md`.

Current state boundary: **Phase 2 scan/seed cadence**. Phase 1 deterministic substrate is locally green; do not start live harness work yet.

1. Add a bounded per-camp structural scan pass that samples ordinary OMT terrain near camp without waking/scanning the whole map.
2. Write sparse `structural_bounty` camp-map leads through the Phase 1 helper, preserving harvested/dangerous suppression and mobile actor separation.
3. Add deterministic tests for sparse seeding, scan budget/cap behavior, retired-empty-site skip, and no harvested/dangerous refresh.
4. Run a short Ralph Wiggum pass before and after the slice: verify the scan still preserves bounty-before-threat, no omniscient threat reveal, no repeated dumb revisits, and no player-sighting-to-terrain smear.

Do **not** start live/harness proof yet. Do **not** implement writhing stalker, local bubble materialization, exact loot harvest, random casualty economy, or release packaging in this packet.
