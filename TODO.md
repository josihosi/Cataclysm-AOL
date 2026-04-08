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
  - camp-request routing now distinguishes bare `assigned_camp` from actual stationed-camp state
  - idle assigned-camp hearers in `NPC_MISSION_NULL`, explicit `FACTION_CAMP` role-id workers, and stationed camp guards/patrol guards are currently basecamp-eligible
  - walking-with-player companion states and `GUARD_ALLY` still stay out
  - do **not** widen this into locker or follower command redesign while closing the packet
2. keep the McWilliams live proof honest:
  - the old literal `show_board` packet at `.userdata/dev-harness/harness_runs/20260408_033437/` is now demoted beyond mere follower contamination
  - fresh hearer-routing instrumentation on `.userdata/dev-harness/harness_runs/20260408_053336/` shows nearby hearers `Katharina Leach` / `Robbie Knox` currently have `assigned_camp=none`, so those ordinary-hearer replies never exercised the real Package 2 state
3. next smallest honest step for Package 2:
  - add the smallest McWilliams restaging helper that creates one nearby non-following assigned-camp hearer
  - rerun the live board/craft probe on that restaged hearer state
  - keep the work isolated to Package 2, without leaking into Package 3+

Still true:
- Package 3 (`locker outfit engine hardening`) stays next once Package 2 is landed or honestly blocked
- ordinary harness footing should stay on `McWilliams` / `Zoraida Vick`, not drift back to the older default save
- the debug pass is now packetized; use the auxiliary doc instead of rebuilding the note pile from chat memory
- hackathon-reserved — do not touch before the event:
  - chat interface over dialogue branches
  - ambient-trigger reaction lane / tiny ambient-trigger NPC model
- Smart Zone Manager v1 stays parked while the controlled locker/basecamp packet is active
