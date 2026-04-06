# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Patrol Zone v1 is the active lane.

Execution queue:
1. follow `doc/patrol-zone-v1-patch-plan-2026-04-06.md`
2. land sticky-roster / interrupt-whitelist behavior
   - routine work does not steal on-shift guards
   - urgent disruption can break patrol
   - reserve backfill without full roster reshuffle
3. then land on-map hold-vs-loop behavior
   - fully staffed connected cluster => hold distinct squares
   - understaffed connected cluster => fixed loop
4. keep reports honest
   - separate implemented deterministic contract from live proof
   - do not count zone plumbing, prose, or plausible-looking motion as success by themselves
   - if something smells too smooth, audit it

Still true:
- Hackathon-reserved — do not touch before the event:
  - chat interface over dialogue branches
  - ambient-trigger reaction lane / tiny ambient-trigger NPC model
- `sustain_npc` is parked as a helper idea until a future live-probe lane actually needs it
- Locker Zone V1 and V2 stay closed only because their bundled task sets are checked in `SUCCESS.md`
- if later work disproves any bundled V1/V2 claim, reopen the affected slice first
- keep V3 from turning into giant NPC-personality soup
- keep the finished Basecamp follow-through closed unless Josef explicitly reopens it
