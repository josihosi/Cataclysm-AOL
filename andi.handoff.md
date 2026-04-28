# Andi handoff: Bandit camp-map risk/reward dispatch planning packet v0

## Active target

`Bandit camp-map risk/reward dispatch planning packet v0` is active. The cannibal night-raid live slice is checkpointed green for the required scenarios; its optional bandit-control contrast is non-blocking unless product review explicitly reopens it.

Canonical anchors:

- `Plan.md`
- `TODO.md`
- `TESTING.md`
- `SUCCESS.md`
- `doc/bandit-camp-map-ecology-source-of-truth-2026-04-28.md`
- `doc/bandit-camp-map-ecology-implementation-map-2026-04-28.md`
- `doc/bandit-camp-map-risk-reward-dispatch-planning-packet-v0-2026-04-28.md`
- `doc/bandit-camp-map-risk-reward-dispatch-andi-lane-v0-2026-04-28.md`
- `doc/bandit-live-product-playtest-matrix-v0-2026-04-28.md`

## Current evidence boundary

Initial deterministic/code support already covers a camp-owned `camp_intelligence_map`, normal serialize/deserialize support, active target OMT persistence, scout-return writeback into the source camp map, and legacy scalar remembered-memory migration only as fallback.

Prior validation:

- `git diff --check`
- `make -j4 obj/bandit_live_world.o tests/bandit_live_world_test.o tests LINTJSON=0 ASTYLE=0`
- `./tests/cata_test "[bandit][live_world][camp_map]" --success`
- `./tests/cata_test "[bandit][live_world]"`

This is deterministic/code evidence only, not live product proof.

## Next implementation/proof target

Continue the camp-map ecology contract through deterministic/code proof before attempting live closure:

- two-OMT ordinary scout stand-off instead of old five-OMT default;
- half-day scout-watch timeout/return with memory writeback;
- roster/reserve sizing for 2/4/5/7/10 member camps;
- wounded/killed-member shrinkage;
- active-outside / same-target dogpile blocking;
- stockpile/need pressure without breaking reserve/risk gates;
- high-threat non-escalation;
- prior defender-loss pressure and prior bandit-loss cooling;
- larger stalk/pressure follow-up when justified;
- no-opening return/hold behavior;
- sight-avoid reposition/abort within at most two visible local turns without teleporting.

## Queued after this lane

`C-AOL live AI performance audit packet v0` is greenlit as a bottom-of-stack performance audit, not an interruption to the active bandit camp-map lane unless Schani/Josef explicitly promotes it. Canonical contract: `doc/c-aol-live-ai-performance-audit-packet-v0-2026-04-28.md`; imagination source: `doc/c-aol-live-ai-performance-imagination-source-of-truth-2026-04-28.md`.
