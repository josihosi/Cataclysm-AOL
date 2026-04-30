# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active target: `Bandit structural bounty overmap completion packet v0`.

Canonical contract: `doc/bandit-structural-bounty-overmap-completion-packet-v0-2026-04-30.md`.
Imagination source: `doc/bandit-structural-bounty-overmap-completion-imagination-source-of-truth-2026-04-30.md`.

Current state boundary: **Phase 1 deterministic substrate**. Start with the lived-rule support before live harness work.

1. Add a coarse structural OMT classifier for forest/woods, town/building, and open/no-bounty terrain.
2. Add/reuse structural lead id and upsert/debounce helpers using `camp_intelligence_map` / `camp_map_lead`.
3. Add focused deterministic tests for classifier output, harvested/dangerous suppression, and mobile actor leads not upgrading ground bounty.
4. Run a short Ralph pass before and after the slice: verify the diff preserves bounty-before-threat, no omniscient threat reveal, no repeated dumb revisits, and no player-sighting-to-terrain smear.

Do **not** start live/harness proof yet. Do **not** implement writhing stalker, local bubble materialization, exact loot harvest, random casualty economy, or release packaging in this packet.
