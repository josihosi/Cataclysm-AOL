# Plan

Canonical roadmap for Cataclysm-AOL.

This file answers one question: **what should the agent advance next?**
It is not a changelog, not a graveyard of crossed-off substeps, and not a place to preserve every historical detour.
Rewrite it as reality changes.

## File roles

- **Plan.md** — canonical roadmap and current delivery target
- **SUCCESS.md** — success-state ledger / crossed-off exit criteria for roadmap items
- **TODO.md** — short execution queue for the current target only
- **TESTING.md** — current validation policy, latest relevant evidence, and pending probes
- **TechnicalTome.md** — durable mechanic notes, not daily state tracking
- **COMMIT_POLICY.md** — checkpoint rules to prevent repo soup

If these files disagree, **Plan.md wins** and the other files should be repaired.

## Working rules for agents

- Do **not** mechanically grab the first unchecked-looking thing from some list.
- Follow the current delivery target below and move it to its **next real state**.
- Josef being unavailable for playtesting is **not** a blocker by itself.
- When a target is waiting on Josef, move to the next best unblocked target.
- If no good unblocked target remains, send Josef a short parked-options note so he can greenlight the next lane; do not just keep revalidating the old packet.
- Prefer batching human-only asks where practical. One useful packet with two real product questions beats two tiny pings.
- Keep these files lean. Remove finished fluff from `TODO.md` and `TESTING.md` instead of piling up crossed-off archaeology.
- Each real roadmap item needs an explicit success state in `SUCCESS.md` (or an equally explicit inline auxiliary) so completion is visible instead of guessed.
- Cross off reached success-state items; only remove the whole roadmap item from `Plan.md` once its success state is fully crossed off / done.
- Prefer agent-side playtesting first. Josef should be used for product judgment, feel, priority calls, or genuinely human-only checks.
- Validation should match risk:
  - docs-only change -> no compile
  - small local code change -> narrow compile/test
  - broad or risky code change, or a Josef handoff -> broader rebuild / startup harness as needed
- Follow `COMMIT_POLICY.md`. Do not let the repo turn into one giant dirty blob.

---

## 1. Current delivery target — controlled locker / basecamp follow-through packet

**Status:** ACTIVE / PACKAGE 3 NOW

The repo is not honestly parked anymore, but it also should not be reopened as one giant locker/basecamp soup lane.
Josef's McWilliams debug pass has now been reduced into a controlled packet: one package at a time, preserve the working loop, and do not let Andi broaden scope by vibes.

Primary auxiliary:
- `doc/locker-basecamp-followthrough-work-packages-2026-04-07.md`

### Current truth
- Ordinary chat / ambient harness footing now points at the captured `McWilliams` / `Zoraida Vick` save instead of the older Sandy Creek default.
- The McWilliams debug pass produced a coherent follow-through queue rather than one monolithic rewrite:
  1. harness zone-manager save-path polish
  2. basecamp toolcall routing fix
  3. locker outfit engine hardening
  4. locker zone policy + control-surface cleanup
  5. basecamp carried-item support + dump lane
- **Package 1** is landed on the current McWilliams harness path: the zone name must be entered at creation time, a single `Esc` opens the save prompt, uppercase `Y` returns cleanly to gameplay, and reopening Zone Manager shows the custom `Probe Locker` name still present.
- **Package 2** is now landed on the real McWilliams path instead of on the fake nearby-activity detour:
  - the deterministic reduction stays the same in code: camp-request routing no longer keys only off bare `assigned_camp`; idle stationed camp assignees in `NPC_MISSION_NULL`, explicit `FACTION_CAMP` role-id workers, and stationed camp guards/patrol guards are basecamp-eligible, while walking-with-player companion states and `GUARD_ALLY` still stay out
  - the nearby activity-menu probe at `tools/openclaw_harness/scenarios/basecamp.package2_activity_menu_probe_mcw.json` remains a useful negative result, not the restaging source: after `Taking it easy`, Katharina/Robbie still kept `assigned_camp=none`
  - the honest restaging source is the ally dialogue path on McWilliams: `C -> t -> 1 -> b -> d -> n -> a -> q -> c`, now also representable as the harness step `assign_nearby_npc_to_camp_dialog`, then let the camp state settle
  - the first recheck on `.userdata/dev-harness/harness_runs/20260408_081903/` proved the missing intermediate truth: `assign_camp` writes `assigned_camp=140,41,0` immediately, but one-turn evidence still leaves Katharina in interim `mission=6` / `GUARD_ALLY`, so the route honestly stays ordinary at that point
  - after roughly 100 turns of settling, `.userdata/dev-harness/harness_runs/20260408_082344/` shows the intended stationed state on the same real save: Katharina logs `uses_basecamp=yes`, `camp_found=yes`, `assigned_camp=140,41,0`, `mission=8`, and `reason=camp_grouped`
  - the latest exact-token live packet at `tools/openclaw_harness/scenarios/basecamp.package2_assign_camp_toolcall_probe_mcw.json`, run `.userdata/dev-harness/harness_runs/20260408_083415/`, proves the actual routed path: `show_board` now logs `camp heard Katharina Leach`, `heard=show_board`, `board=show_board`, and emits the board follow-through with `job=1 ... next=job=1`; the follow-up `job=1` token also rides the same camp-aware lane
  - the earlier freeform `craft 1 bandage` phrasing is now demoted as the wrong live-proof shape for this packet. On the true assigned-camp state the honest routed follow-up is the structured `job=1` token coming back from `show_board`, not another raw craft phrase.
- **Package 3** is now the active slice on purpose. The locker outfit hardening queue should not bury the now-settled Package 2 truth under more routing churn.
- The first narrow Package 3 hardening slice is now landed in deterministic code/tests: same-type locker bags prefer the better-condition equivalent instead of shrugging at a damaged current bag just because the score delta is small.
- A second narrow Package 3 hardening slice is now landed in deterministic code/tests: footed/full-body jumpsuits no longer get bucketed as shoes just because the classifier sees feet first, so the planner now keeps them in the pants lane instead of excluding them through the footwear bucket.
- A third narrow Package 3 acceptance-bar slice is now closed in deterministic planning/service tests: baseball cap -> army helmet replacement already works on the current path, and the repo now has explicit proof instead of only debug-pass folklore.
- A fourth narrow Package 3 lower-body slice is now closed in deterministic planning/service tests: the hot-weather `antarvasa` + cargo pants conflict now has explicit proof that the locker path cleans up the duplicate lower-body wear and lands the cargo-shorts swap in one service pass.
- A fifth narrow Package 3 lower-body acceptance-bar slice is now closed in deterministic planning/service tests: when an NPC is already wearing cargo shorts plus duplicate jeans in hot weather, the locker path keeps the shorts, strips the duplicate jeans, and returns the jeans to locker stock without requiring a fresh replacement item.
- A sixth narrow Package 3 lower-body slice is now closed in deterministic planning/service tests: full-leg skintight underlayers like leggings now ride the pants lane instead of being sheltered in underwear, so hot-weather locker cleanup can strip them alongside cargo pants before landing the cargo-shorts swap.
- A seventh narrow Package 3 lower-body classification slice is now closed in deterministic code/tests: jumpsuit-like outer one-piece suits no longer fall into vest logic just because they are marked `OUTER`, so the planner now keeps them in the pants lane instead of pretending the lower-body slot is empty.
- An eighth narrow Package 3 one-piece acceptance-bar slice is now closed in deterministic planning/service tests: lower-body-only upgrades no longer strip torso coverage from a current one-piece suit unless the same locker pass also supplies a torso replacement, so the planner stops "upgrading" into half-dressed nonsense.
- A ninth narrow Package 3 one-piece classification slice is now closed in deterministic planning/service tests: skintight full-body one-piece suits like union suits and wetsuits no longer hide in underwear, so the locker path now keeps them in the pants lane and refuses to layer cargo shorts over them unless some separate torso replacement exists.
- A tenth narrow Package 3 one-piece alias slice is now closed in deterministic code/tests: indirect suit-alias full-body items like tuxedos now stay in the pants lane instead of falling back into vest logic, so the locker path keeps their torso-coverage guard instead of reintroducing half-dressed nonsense through `looks_like: suit` variants.
- Patrol sanity on the current McWilliams save already checks out: the visible patrol tiles currently resolve to **2 clusters** under 4-way connectivity, so that note is no longer an open mystery.
- The active repo rule for this packet is still simple:
  - one package at a time
  - revalidate before widening
  - no broad Andi reactivation
  - no opportunistic side quests while the packet is active

### Next real state
1. keep **Package 3** narrow after the landed bag-condition, jumpsuit-classification, cap -> helmet, lower-body cleanup, shorts-vs-jeans duplicate-cleanup, leggings-underlayer cleanup, outer-suit classification, indirect suit-alias one-piece, and skintight one-piece classification proof slices: the next honest hardening target is the next current-path lower-body oddity, not a grab-bag rewrite
2. preserve the landed better-condition bag swap behavior, the new jumpsuit-not-shoes behavior, the cap -> helmet swap path, the lower-body cleanup path, the shorts-vs-jeans duplicate cleanup, the leggings-underlayer cleanup, the outer-suit classification fix, the indirect suit-alias one-piece fix, and the skintight one-piece no-shorts-over-wetsuits guard while extending the acceptance bar one visible failure mode at a time
3. preserve the new Package 2 assigned-camp probe as the routing baseline and only reopen routing if Package 3 work actually breaks it
4. once the current locker / basecamp stack reaches its honest bottom-of-stack handoff point, continue into the already-greenlit **Smart Zone Manager v1** one-off zone-stamping lane from `doc/smart-zone-manager-v1-aux-plan-2026-04-06.md` without interrupting the active packet early

---

## 2. Checkpointed — Patrol Zone v1

**Status:** CHECKPOINTED / DONE FOR NOW

This lane is now considered done for now because the bundled success state in `SUCCESS.md` is checked:
- patrol zone surface + planner + sticky-shift contract exist
- deterministic hold-vs-loop runtime behavior exists
- current-binary live proof exists for disconnected-loop and connected-hold cases
- the packaged patrol packet is now legible enough to explain gaps / off-shift state without leaning on raw trace logs alone

If later code work or runtime evidence shows any one of those claims is false or incomplete, reopen Patrol Zone v1 immediately.

---

## 3. Checkpointed — Locker-capable harness restaging

**Status:** CHECKPOINTED / DONE FOR NOW

This lane is now considered done for now because the bundled success state in `SUCCESS.md` is checked:
- a real locker-capable fixture/restaging path exists
- `locker.weather_wait` is no longer blocked on missing fixture shape
- a fresh packaged run reports **screen** / **tests** / **artifacts** separately
- the result is documented reviewer-cleanly as harness/fixture work on existing locker behavior

If later fixture drift, harness drift, or locker runtime evidence breaks any one of those bundled claims, reopen this lane immediately.

---

## 4. Checkpointed — Locker Zone V2

**Status:** CHECKPOINTED / DONE FOR NOW

V2 is now considered done for now because the bundled V2 task set in `SUCCESS.md` is checked:
- managed ranged loadouts can pull up to two compatible magazines from locker supply
- selected compatible magazines can be topped off from locker-zone ammo and the supported weapon reloaded from that supply
- deterministic coverage exists for the V2 contract
- proportional runtime proof is recorded on the current binary

If later code work or runtime evidence shows any one of those bundled claims is false or incomplete, reopen V2 immediately.

---

## 5. Reopened context — Locker Zone V1

**Status:** REOPENED UNDER SECTION 1

Do not treat Locker Zone V1 as closed right now.
Fresh-save manual testing disproved the old surface/control close-out, so the active V1 reopen now lives in section 1.

Still believed true unless new evidence breaks it:
- locker outfitting core exists as real planner/service behavior
- locker maintenance rhythm exists as real dirty/queue/reservation behavior
- earlier deterministic + proportional runtime proof for those non-surface slices still exists

What is no longer safe to claim as closed until revalidated:
- that the locker surface/control is currently solid on the real fresh-save path
- that ordinary sorting cannot siphon gear out of locker tiles
- that the current locker zone interaction surface is free of the reported type-mismatch / overlay problems

---

## 6. Checkpointed — post-Locker-V1 Basecamp follow-through

**Status:** CHECKPOINTED / DONE FOR NOW

This queue reached its exit criteria for now:
- the board/job log packet is legible enough to compare against the deterministic router proof
- the deterministic board packaging is cleaner/upstream-friendlier
- the richer structured treatment now follows the board-emitted `next=` tokens instead of dropping straight back to spoken bark
- the testing/docs packet describes the closed state instead of an open queue

Keep this closed unless Josef explicitly reopens Basecamp prompt follow-through or a later change breaks the structured board/job lane again.

---

## 7. Checkpointed — LLM-side board snapshot path

**Status:** CHECKPOINTED

This slice reached its exit criteria for now:
- routing proof exists on the actual camp request router, not only on helper builders
- the richer structured/internal `show_board` lane is covered with deterministic evidence
- the short spoken board bark stayed separate
- the testing/docs packet can now describe current truth instead of an open routing question

Keep this out of the active queue unless later code changes break the route again or a new greenlit slice explicitly extends it.

---

## 8. Hackathon-reserved feature lanes — do not touch before the event

These are intentionally **reserved for the hackathon itself**.
They should stay visibly separate from the current repo-footing/harness work so reviewers do not mistake scaffolding for early feature implementation.

1. **Chat interface over in-game dialogue branches**
   - the future feature lane
   - current harness work may exercise nearby-NPC/freeform chat controls, but that is only test scaffolding and **not** this feature
2. **Tiny ambient-trigger NPC model**
   - the future feature lane
   - current harness work may stage weird-item scenarios and artifact checks, but that is only test scaffolding/observability and **not** this feature

Do not start them early, do not half-land them, and do not describe scaffolding as partial completion.

---

## 9. Documentation discipline

If the structure starts bloating again, apply this rule:
- `Plan.md` should be readable in a minute
- `TODO.md` should show only the current execution queue
- `TESTING.md` should show only current policy, latest relevant evidence, and pending probes
- `COMMIT_POLICY.md` should stop the dirty tree from becoming a lifestyle

If a sentence exists only to remember that something used to be true, it probably belongs in git history, not here.
