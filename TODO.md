# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Current delivery target: controlled locker / basecamp follow-through packet.
Primary auxiliary:
- `doc/locker-basecamp-followthrough-work-packages-2026-04-07.md`

Current slice: **Package 3 — locker outfit engine hardening**
1. keep Package 2 closed unless new code breaks it:
  - the honest assigned-camp restaging source is the ally dialogue path `C -> t -> 1 -> b -> d -> n -> a -> q -> c`, now also representable as the harness step `assign_nearby_npc_to_camp_dialog`, plus enough settle time for the interim `GUARD_ALLY` state to clear
  - the current routed live proof is `tools/openclaw_harness/scenarios/basecamp.package2_assign_camp_toolcall_probe_mcw.json` on `.userdata/dev-harness/harness_runs/20260408_083415/`; the debug-menu folklore path `} -> p -> p -> b -> A` is not that seam on current McWilliams
  - `show_board` -> `job=1` is the honest structured follow-up on that state; do **not** drift back into treating raw `craft 1 bandage` as equivalent proof of Package 2
2. keep the landed Package 3 slices narrow and true:
  - same-type bag upgrades now prefer the better-condition equivalent instead of leaving a damaged current bag in place just because the score delta is tiny
  - footed/full-body jumpsuits now stay in the pants lane instead of getting misbucketed as shoes by the feet-first classifier
  - baseball cap -> army helmet replacement now has deterministic planning + service proof on the current path instead of just a debug-pass note
  - the hot-weather `antarvasa` + cargo pants -> cargo shorts cleanup path now also has deterministic planning + service proof instead of living only as debug-pass folklore
  - the hot-weather duplicate `cargo shorts + jeans` conflict now also has deterministic planning + service proof: the locker path keeps the shorts, strips the duplicate jeans, and returns the jeans to locker stock without needing a replacement item from the zone
  - full-leg skintight underlayers like `leggings` now also have deterministic planning + service proof: the locker path treats them as pants-lane duplicates instead of sheltering them in underwear, so hot-weather cleanup can strip them alongside cargo pants before landing cargo shorts
  - jumpsuit-like outer one-piece suits now stay in the pants lane instead of falling into vest logic just because they are marked `OUTER`, so the planner no longer pretends those full-body suits leave the lower-body slot empty
  - lower-body-only upgrades no longer strip torso coverage from a current one-piece suit unless the same locker pass also supplies a torso replacement, so the locker path stops "upgrading" suits into half-dressed nonsense
  - skintight full-body one-piece suits like union suits and wetsuits now also stay in the pants lane instead of hiding in underwear, so the locker path no longer tries to layer cargo shorts over them without a torso replacement
  - indirect suit-alias full-body items like `tux` now also stay in the pants lane instead of falling back into vest logic, so `looks_like: suit` variants keep the same torso-coverage guard instead of reopening the half-dressed split bug through an alias path
  - short dresses now also keep the same torso-coverage guard as the suit-like cases, so a shorts-only locker candidate leaves the dress in place while a shorts + t-shirt locker packet can still split it cleanly in one service pass
  - deterministic coverage now includes planning + service checks for the bag-condition slice, the cap -> helmet slice, the new lower-body cleanup slice, the duplicate-shorts-vs-jeans cleanup slice, the leggings-underlayer cleanup slice, the new one-piece torso-coverage guard slice, the new skintight one-piece guard slice, the new indirect suit-alias guard slice, and the new short-dress guard slice, plus classification/planning checks for the jumpsuit-not-shoes slice and the outer-suit classification slice
3. pick the next isolated ugly locker conflict, not a whole-barn rewrite:
  - the next honest Package 3 question is the next current-path lower-body oddity beyond the now-proven hot-weather cleanup, duplicate-shorts-vs-jeans, leggings-underlayer, outer-suit-classification, indirect suit-alias one-piece guard, one-piece torso-strip-guard, skintight one-piece no-shorts-overlayer, and short-dress torso-coverage paths
  - do **not** leak into locker policy/control-surface or carried-item work while continuing Package 3

Still true:
- Package 3 (`locker outfit engine hardening`) stays next once Package 2 is landed or honestly blocked
- ordinary harness footing should stay on `McWilliams` / `Zoraida Vick`, not drift back to the older default save
- the debug pass is now packetized; use the auxiliary doc instead of rebuilding the note pile from chat memory
- hackathon-reserved — do not touch before the event:
  - chat interface over dialogue branches
  - ambient-trigger reaction lane / tiny ambient-trigger NPC model
- Smart Zone Manager v1 is now greenlit but stays queued at the bottom of the stack until the current locker/basecamp packet reaches its honest handoff point
