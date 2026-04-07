# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Current delivery target: controlled locker / basecamp follow-through packet.
Primary auxiliary:
- `doc/locker-basecamp-followthrough-work-packages-2026-04-07.md`

Current slice: **Package 2 — basecamp toolcall routing fix**
1. keep the new discriminator narrow:
  - camp-request routing now requires active `FACTION_CAMP` duty instead of bare `assigned_camp`
  - do **not** widen this into locker or follower command redesign while validating it
2. recheck the McWilliams repro on the current harness footing:
  - confirm the wrong-snapshot/toolcall lane is gone for ordinary followers who merely still have an assigned camp
  - confirm true camp-duty NPCs still hit the intended basecamp-aware path
3. if the repro still fails after the duty-gate fix:
  - trace the next remaining discriminator or payload seam
  - keep the change isolated to Package 2, without leaking into Package 3+

Still true:
- Package 3 (`locker outfit engine hardening`) stays next once Package 2 is landed or honestly blocked
- ordinary harness footing should stay on `McWilliams` / `Zoraida Vick`, not drift back to the older default save
- the debug pass is now packetized; use the auxiliary doc instead of rebuilding the note pile from chat memory
- hackathon-reserved — do not touch before the event:
  - chat interface over dialogue branches
  - ambient-trigger reaction lane / tiny ambient-trigger NPC model
- Smart Zone Manager v1 stays parked while the controlled locker/basecamp packet is active
