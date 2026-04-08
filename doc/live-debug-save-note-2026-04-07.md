# Live debug save note (2026-04-07)

Status: **temporary transition note**.
Keep this until the McWilliams harness footing is fully normalized in the long-lived harness docs/fixtures, then archive or fold it into the harness docs instead of letting it masquerade as permanent canon.

Current live `./cataclysm-tiles` session is using world/save:
- **World:** `McWilliams`
- **Character:** `Zoraida Vick` (per Josef)

Intended harness direction:
- treat this live world as the new preferred debug/harness baseline
- do **not** keep using the older stale dev/dev-harness save for manual/live verification
- after Josef finishes the current manual session and the save is safely written, promote/snapshot this world cleanly into the harness fixture/profile instead of copying a moving target mid-session

Reason:
- the currently open game is the source of truth
- previous harness launches were proven to hit the wrong save
- copying while the game is still open risks snapshotting a moving save imperfectly
