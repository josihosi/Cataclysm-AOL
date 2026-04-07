# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Locker Zone V1 surface/control is reopened.
Work the smallest real repair first, not the whole bug pile at once.

1. compare the clean harness locker-creation recheck against Josef's reported `McWilliams` live-session failure:
  - current code now survives a real Zone Manager `CAMP_LOCKER` creation on `basecamp_dev_manual_2026-04-02`
  - next reduce whether the remaining mismatch is save-specific, fixture-specific, or tied to the still-open live `McWilliams` session
2. triage the reported visible `S` overlay:
  - the clean harness recheck still did **not** leave a stuck `S` after closing the zone manager
  - a synthetic harness probe now shows that explicitly toggling zone display on a `CAMP_LOCKER` zone leaves the expected storage-style `S` overlay visible after the manager closes
  - the remaining question is whether Josef saw that same latched zone-display state on `McWilliams`, or a real stuck-overlay bug on that save/path
3. keep the direct-talk wrong-snapshot bug queued, but do not let it interrupt the narrower locker surface slice unless the locker trail stalls

Still true:
- the live `McWilliams` / `Zoraida Vick` save is the preferred future manual/harness baseline once it is safely snapshotted after Josef closes the session
- Hackathon-reserved — do not touch before the event:
  - chat interface over dialogue branches
  - ambient-trigger reaction lane / tiny ambient-trigger NPC model
- `sustain_npc` is parked as a helper idea until a future live-probe lane actually needs it
- Smart Zone Manager v1 stays parked while the locker surface reopen is active
- keep V3 from turning into giant NPC-personality soup
- keep the finished Basecamp follow-through closed unless Josef explicitly reopens it
