# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Locker Zone V3 is the active lane.

Execution queue:
1. rebuild a reliable proportional runtime probe for the landed V3 legwear slice
   - the intended pants-slot short-vs-full-length probe is still `shorts_cargo` / `pants_cargo`
   - the current staged-save repro is not honest yet: restoring the current `dev` save and staging that pair did not emit a fresh legwear locker packet after restart, and the harness fixture plus Peekaboo-driven turn advance/save also left the staged swap untouched
   - temporary `process_camp_locker_downtime` queue/skip tracing is now in place, but a plain fresh harness load on the restored current-save path still does not enter the locker downtime logs at all
   - next honest options: recover the old dirty-worker trigger on the current save, stage a guaranteed post-load downtime/dirty NPC state that actually reaches the instrumented service path, or add the smallest caller-side trace above `process_camp_locker_downtime` if the runtime still never enters it
2. keep V1/V2 closed unless this new V3 lane shows the older locker spine regressed
3. only after the live legwear probe path is trustworthy, capture the packet and choose the next narrow V3 follow-up lane

Still true:
- Locker Zone V1 and V2 stay closed only because their bundled task sets are checked in `SUCCESS.md`
- if V3 work disproves any bundled V1/V2 claim, reopen the affected slice first
- keep V3 from turning into giant NPC-personality soup
- keep the finished Basecamp follow-through closed unless Josef explicitly reopens it
