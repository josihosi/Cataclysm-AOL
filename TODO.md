# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Locker-capable harness restaging is the active lane.

Execution queue:
1. inspect the current shipped fixture/restaging path for `locker.weather_wait`
   - confirm exactly where the missing `CAMP_LOCKER` zone/state gap lives
2. choose the smallest honest repair
   - new fixture with a real `CAMP_LOCKER` zone, or
   - reproducible restaging step that creates/restores the needed locker state before the probe
3. rerun `locker.weather_wait` once the fixture path is real
   - report **screen** / **tests** / **artifacts** separately
4. keep the story reviewer-clean
   - this is harness/fixture work on existing locker behavior
   - it is **not** the hackathon chat feature
   - it is **not** the hackathon ambient-model feature

Still true:
- Hackathon-reserved — do not touch before the event:
  - chat interface over dialogue branches
  - ambient-trigger reaction lane / tiny ambient-trigger NPC model
- Later discussion topics only, not active lanes yet:
  - patrol zone for the Zone Manager
  - smart zone manager
- Locker Zone V1 and V2 stay closed only because their bundled task sets are checked in `SUCCESS.md`
- if later work disproves any bundled V1/V2 claim, reopen the affected slice first
- keep V3 from turning into giant NPC-personality soup
- keep the finished Basecamp follow-through closed unless Josef explicitly reopens it
