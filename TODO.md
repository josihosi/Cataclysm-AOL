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
   - the current staged-save repro is still not honest yet, but the blocker is narrower now: a plain fresh harness load is **not** the missing evidence class on the restored current save; the locker loop only woke once the already-loaded game got live post-load turn advance (`Tab`) through Peekaboo
   - the smallest caller-side trace above `process_camp_locker_downtime` is now in place, and the packed-save audit showed the current dev save is reading the `./zzip` archives rather than loose extracted staging files
   - locker-tile staging through the packed map archive now reaches runtime again, but Bruna's staged overmap wardrobe still does not hydrate into the live worker state, so the remaining honest job is finding/staging the **actual active NPC worn-state source** before claiming a legwear packet
   - next honest options: recover the real active-state source for Bruna on the restored current save, or build the smallest packed-save/current-save path that stages a clean `shorts_cargo` / `pants_cargo` swap onto a worker who definitely reaches the post-load live service loop
2. keep V1/V2 closed unless this new V3 lane shows the older locker spine regressed
3. only after the live legwear probe path is trustworthy, capture the packet and choose the next narrow V3 follow-up lane

Still true:
- Locker Zone V1 and V2 stay closed only because their bundled task sets are checked in `SUCCESS.md`
- if V3 work disproves any bundled V1/V2 claim, reopen the affected slice first
- keep V3 from turning into giant NPC-personality soup
- keep the finished Basecamp follow-through closed unless Josef explicitly reopens it
