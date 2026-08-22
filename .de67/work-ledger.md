# Active Phase-3 projection

## R-004 — durable actor identity, ownership transfer, and crossing receipts

- DFS slice: `.de67/DFS.md` §4 “Durable actor identity, ownership transfer, and crossing receipts”; acceptance row `R-004`.
- Claim status: red and unaccepted.
- Active gap: actor-level ownership continuity is not completely receipted or reconciled across both crossing directions and load.
- Required mechanism: retain the existing candidate/rollback transfers; add symmetric persistence-confirmed crossing receipts through the structured event stream; populate and serialize simulation owner; reconcile or reject durable owner state against loaded local NPC projections before either layer advances.
- Next executable route: inspect `bandit_live_world.h/.cpp`, `bandit_live_world_probe.*`, production callers in `do_turn.cpp`, save/load persistence, and crossing tests; return a finite implementation/proof decomposition grounded in those owners.
- Required proof: production-path success, repeat, stale, partial, callback-failed, persistence-failed, duplicate, and crash-window crossings in both directions and across save/relaunch preserve one actor identity, one owner, monotone generation/epoch/cursor, and one compact correlated receipt.
- Clock: exact lineage `CAOL-proof-preserving-playtesting`; deadline state supplied by `DE67_DEADLINE_STATE`; original claim deadline generation remains authoritative.
- Accepted frontier: `R-003` accepted by `R003-M007` after independent mutation and DFS review; its live-seam finding was repaired before acceptance, and the claim deadline was not missed.
