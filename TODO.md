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
1. keep the landed discriminator narrow:
  - camp-request routing now distinguishes bare `assigned_camp` from real camp-duty state
  - explicit `FACTION_CAMP` role-id workers and stationed camp guards/patrol guards are currently basecamp-eligible, while `GUARD_ALLY` hold/follower state stays out
  - do **not** widen this into locker or follower command redesign while closing the packet
2. keep the McWilliams live proof honest:
  - the current live save already gives us nearby hearers `Katharina Leach` and `Robbie Knox`
  - a fresh literal `show_board` probe on the rebuilt McWilliams binary still sent both hearers through the ordinary nearby-hearer LLM prompt path instead of emitting a camp board reply
  - current evidence packet: `.userdata/dev-harness/harness_runs/20260408_033437/`
3. next smallest honest step for Package 2:
  - inspect why the current nearby-hearer/runtime-state path still leaves `Robbie Knox` on the ordinary LLM side of the split
  - only add a new staging helper if that runtime/grouping audit proves the live save truly lacks a qualifying camp hearer
  - keep the work isolated to Package 2, without leaking into Package 3+

Still true:
- Package 3 (`locker outfit engine hardening`) stays next once Package 2 is landed or honestly blocked
- ordinary harness footing should stay on `McWilliams` / `Zoraida Vick`, not drift back to the older default save
- the debug pass is now packetized; use the auxiliary doc instead of rebuilding the note pile from chat memory
- hackathon-reserved — do not touch before the event:
  - chat interface over dialogue branches
  - ambient-trigger reaction lane / tiny ambient-trigger NPC model
- Smart Zone Manager v1 stays parked while the controlled locker/basecamp packet is active
