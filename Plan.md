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

## Current status

There is currently **one active greenlit lane**.

Fresh checkpoints that stay closed:
- **Plan status summary command** is now honestly checkpointed too:
  - `tools/plan_status_summary.py` now provides a deliberately small read-only seam for `/plan`, `/plan active`, `/plan greenlit`, and `/plan parked`
  - it reads `Plan.md` only, preserves greenlit ordering, and warns when canon is thin or contradictory instead of inventing certainty
  - narrow validation passed via `python3 tools/plan_status_summary.py --self-test` plus direct current-canon samples for `/plan`, `/plan active`, `/plan greenlit`, and `/plan parked`
- **Bandit scenario fixture + playback suite v0** is now honestly checkpointed too:
  - `src/bandit_playback.{h,cpp}` now defines seven stable named reference scenarios (`empty_region`, `smoke_only_distant_clue`, `smoke_searchlight_corridor`, `forest_plus_town`, `single_traveler_forest`, `strong_camp_split_route`, `city_edge_moving_hordes`) on top of the dry-run evaluator seam instead of pretending a full overmap simulator already exists
  - `run_scenario()` now replays those cases at stable checkpoints (`tick 0`, `tick 5`, `tick 20`, `tick 100`), and `render_report()` exposes winner drift so idle, smoke-investigation, corridor stalking, moving-carrier attachment, and peel-off behavior can be inspected reviewer-cleanly
  - narrow deterministic validation passed via `make -j4 tests` and `./tests/cata_test "[bandit]"`
- **Bandit concept formalization follow-through** is now honestly checkpointed too:
  - Package 3, micro-item 31, `Invariants and non-goals packet`, is now landed, so the follow-through finally carries the explicit must-never-happen sheet that the earlier starter numbers and worked scenarios were waiting on
  - the full 3-package / 31-micro-item packet now has explicit law slices, starter numbers, worked scenarios, and red-line invariants/non-goals, which makes the parked bandit chain materially easier to pick up later without rediscovering the whole control law from scratch
- **Bandit evaluator dry-run seam v0** is now honestly checkpointed too:
  - `src/bandit_dry_run.{h,cpp}` now provides a bounded abstract evaluator that always seeds `hold / chill`, generates outward candidates only from compatible valid lead envelopes, and applies visible pre-veto scoring, need-pressure rescue, threat gating, and downstream `no_path` invalidation without pretending full autonomous bandit world behavior exists
  - `render_report()` exposes leads considered, the full candidate board, per-candidate score terms, veto / soft-veto reasons, the winner versus `hold / chill`, and explicitly says no return-packet fields are touched in v0
  - narrow deterministic validation passed via `make -j4 tests` and `./tests/cata_test "[bandit][dry_run]"`
- **Bandit perf + persistence budget probe v0** is now honestly checkpointed too:
  - `src/bandit_playback.{h,cpp}` now provides `measure_scenario_budget()`, `measure_reference_suite_budget()`, `estimate_v0_persistence_budget()`, and `render_budget_report()` on top of the named playback suite, so reviewer-readable runtime/churn/save-budget packets exist without pretending a full profiler or persistence architecture landed
  - `bandit_dry_run::evaluation_metrics` now exposes lead filtering, candidate generation, score/path checks, veto/no-path invalidations, and winner-comparison churn, while the first bounded persistence sample stays around `512` payload bytes before serializer overhead and still reads cheap enough for the abstract v0 shape
  - narrow deterministic validation passed via `make -j4 tests` and `./tests/cata_test "[bandit]"`
- **Plan/Aux pipeline helper** is honestly checkpointed:
  - `tools/plan_aux_pipeline_helper.py` now lets `emit` produce reviewer-visible aux/canon snippets plus optional downstream `andi.handoff.md` output from the same validated classified contract
  - the bounded patch path still stays on known existing headings instead of pretending broad freeform canon mutation is solved
  - narrow validation passed on a sample spec via `python3 -m py_compile tools/plan_aux_pipeline_helper.py`, `schema`, `show`, `emit`, emitted handoff review, and `apply` on a temp repo copy
- **Combat-oriented locker policy** is honestly checkpointed:
  - the explicit `gloves`, `mask`, `belt`, and `holster` locker slots are real, the `Prefer bulletproof gear` toggle is real, ballistic vest scoring/replacement now notices loaded vs damaged ablative plates, and the full-body combat-suit packet now reaches weaker current shirts, vests, and body armor while stronger ballistic armor still stays put
  - the final suspicion-first audit found one real remaining proof gap, namely that the new common combat-support slots had classification/candidate coverage but no direct service proof yet
  - that gap is now closed by deterministic test `camp_locker_service_equips_common_combat_support_slots`, which drives real `CAMP_LOCKER` service and shows gloves, dust mask, tool belt, and holster actually being equipped from locker stock
  - focused deterministic validation passed on the current tree: `make -j4 tests`, `./tests/cata_test "camp_locker_service_equips_common_combat_support_slots"`, and `./tests/cata_test "[camp][locker]"`
- **Package 5, basecamp carried-item dump lane** is honestly closed on the documented `bandages` acceptance item.
- **Package 4, locker zone policy + control-surface cleanup** is now honestly reclosed too:
  - `.userdata/dev-harness/harness_runs/20260419_141422/` shows the real McWilliams `CAMP_LOCKER` Zone Manager seam creating `Basecamp: Locker`, renaming it to `Probe Locker`, closing through the single-`Esc` save prompt, and reopening Zone Manager with `Probe Locker` still present
  - that same live packet did **not** surface the reported zone-creation type-mismatch on screen, in `debug.final.log`, or in stderr
- **Organic bulletin-board speech polish** is now honestly reclosed too:
  - deterministic coverage still passes for the cleaned spoken board/job bark and widened organic status parsing
  - live run `.userdata/dev-harness/harness_runs/20260419_154244/` used the real camp-assignment seam, asked `what needs making`, and showed Katharina answering `Board's got 1 live and 1 old - 1 x bandages.` with no visible request-id glue
  - that same live packet still had Robbie chime in as ordinary follower crosstalk on the McWilliams fixture, but no fresh machine-speech seam appeared

Current target:
- `Locker clutter / perf guardrail probe v0`
  - authoritative active contract: `doc/locker-clutter-perf-guardrail-probe-v0-2026-04-20.md`
  - now that the bandit perf lane is honestly checkpointed, the next job is to measure the real `CAMP_LOCKER` service path under clutter-heavy stock and keep the answer reviewer-readable
  - keep it bounded to a first honest clutter matrix and cheap mitigation order, not broad locker architecture theater

Meaning:
- the plan status summary command is checkpointed closed
- the bandit scenario playback suite is checkpointed closed too
- the bandit follow-through lane is checkpointed closed too
- the first bandit evaluator seam is checkpointed closed too
- the bandit perf/persistence probe is checkpointed closed too
- Andi now has one active locker clutter / perf guardrail lane, because item hoarding is ordinary play and `CAMP_LOCKER` invites players to dump a lot of stock onto it
- the broad bandit concept chain stays parked outside those explicit greenlit slices

---

## Active lane — Locker clutter / perf guardrail probe v0

**Status:** ACTIVE / GREENLIT

This follow-up exists because player hoarding is ordinary play, while camp populations above about ten NPCs are comparatively rare.
The more believable locker failure mode is a curated zone getting used like a universal junk carpet and then making locker service cost spike in ways the player cannot read.

Current honest state:
- the authoritative active contract lives at `doc/locker-clutter-perf-guardrail-probe-v0-2026-04-20.md`
- this slice is only meant to measure the real `CAMP_LOCKER` service path under clutter and recommend a small mitigation order if the curve looks bad
- the first matrix should bias toward top-level item count and realistic worker counts, roughly `50 / 100 / 200 / 500 / 1000` items and `1 / 5 / 10` workers
- the nested-content question is explicit scope: loaded magazines and common container shapes should be measured honestly enough to say whether they mostly behave like one top-level scan unit or create meaningful extra cost
- if the curve looks bad, prefer cheap guardrails such as early junk-ignore, bounded candidate consideration, or a simple curated-stock warning/cap before inventing broad architecture

## Checkpointed — Bandit perf + persistence budget probe v0

**Status:** CHECKPOINTED / DONE FOR NOW

The first bandit budget packet now exists and is honest enough to stay closed for now.

Current honest state:
- the authoritative contract lives at `doc/bandit-perf-persistence-budget-probe-v0-2026-04-20.md`
- `src/bandit_playback.{h,cpp}` now provides `measure_scenario_budget()`, `measure_reference_suite_budget()`, `estimate_v0_persistence_budget()`, and `render_budget_report()` so the named scenarios can report runtime, churn, and save-budget answers reviewer-cleanly
- `bandit_dry_run::evaluation_metrics` now exposes lead filtering, candidate generation, score/path checks, invalidations, and winner-comparison churn instead of hiding that work inside the evaluator
- the first bounded persistence sample stays around `512` payload bytes before serializer overhead and still reads cheap enough for the abstract v0 state shape, with the main bloat warning remaining duplicated tactical truth or historical-delta sludge
- deterministic coverage in `tests/bandit_dry_run_test.cpp` and `tests/bandit_playback_test.cpp` now proves the new measurement counters, short-vs-long horizon drift, persistence estimate, and report rendering
- narrow deterministic validation passed via `make -j4 tests` and `./tests/cata_test "[bandit]"`

## Checkpointed — Bandit scenario fixture + playback suite v0

**Status:** CHECKPOINTED / DONE FOR NOW

The first named playback packet now exists and is honest enough to stay closed for now.

Current honest state:
- the authoritative contract lives at `doc/bandit-scenario-fixture-playback-suite-v0-2026-04-20.md`
- `src/bandit_playback.{h,cpp}` now defines seven stable deterministic reference scenarios on top of the bounded dry-run evaluator instead of inventing a broader world simulator
- the playback seam now reruns those scenarios at `tick 0`, `tick 5`, `tick 20`, and `tick 100`, which is enough to inspect idle, smoke, corridor, moving-carrier, split-route, and city-edge peel-off drift without smuggling in a bigger system
- `render_report()` now provides a reviewer-readable checkpoint summary, and deterministic coverage in `tests/bandit_playback_test.cpp` proves the named scenario set, the stable checkpoints, and the expected drift answers
- narrow deterministic validation passed via `make -j4 tests` and `./tests/cata_test "[bandit]"`

Keep this lane closed unless later evidence shows the playback cases are lying, too thin to answer the intended product questions, or no longer stable enough to rerun honestly.

---

## 1. Checkpointed — Smart Zone Manager v1

**Status:** CHECKPOINTED / DONE FOR NOW

Smart Zone Manager v1 is now good enough for the current pre-freeze goal.
The lane should stay closed unless later code/runtime evidence disproves the claimed state.

Current honest state:
- the one-off Smart Zone Manager v1 surface is real on both Basecamp inventory-zone creation and later position/stretch edits
- the deterministic packet still proves the main stamped layout, the corrected firewood/splintered/wood knot, too-small-zone failure, outdoor rotten placement beyond a simple wall ring, non-destructive refusal on pre-zoned bed tiles, and repeatability on the same layout
- the contract seam audit is now closed honestly: fire/food/equipment anchors are flag-first where the map exposes real signals, clothing/bed support still use small explicit id allowlists where the map does not expose a clean category signal, and ordinary floor fallback stays in place instead of clever failure
- existing built-in loot/custom zone machinery is still the default shape; the only current deliberate custom filters in the v1 packet are `splintered`, `dirty`, `rotten`, `blanket`, and `quilt`
- a proportional live McWilliams proof now exists on the rebuilt current tiles binary via `tools/openclaw_harness/scenarios/smart_zone.live_probe_mcw_prepped.json`, using a narrow prepared-save restage (clear existing zones, spawn/deploy a brazier, place `Basecamp: Storage`, accept the smart-zoning prompt, save, reopen Zone Manager)
- the live packet stayed narrow: it proved the real prompt path plus the saved stamped layout without reopening zone-doctrine archaeology

Keep this lane out of the active queue unless later work breaks one of those claims.

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

## 5. Checkpointed — Locker zone policy + control-surface cleanup

**Status:** CHECKPOINTED / DONE FOR NOW

This slice is now considered done for now because the bundled Locker Zone V1 surface/control row in `SUCCESS.md` is checked.

Current honest state:
- the suspicion-first audit and fresh deterministic packet already reclosed the non-live claims:
  - `CAMP_LOCKER` remains the real `Basecamp: Locker` Zone Manager zone definition
  - locker policy state still exists and the player-facing `Camp_Locker_Policy` / `basecamp::locker_policy_ui()` surface still exists
  - ordinary sorting still leaves `CAMP_LOCKER` tiles alone
- fresh live proof now exists at `.userdata/dev-harness/harness_runs/20260419_141422/`:
  - the real McWilliams Zone Manager path creates `Basecamp: Locker`
  - the creation-time rename to `Probe Locker` works on screen
  - single-`Esc` closeout opens the save prompt, `Y` returns to gameplay, and reopening Zone Manager still shows `Probe Locker`
  - no type-mismatch popup or related stderr/debug failure surfaced on that packet

Keep this slice closed unless later code or runtime evidence breaks one of those claims.

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

## 8. Hackathon feature lanes — parked far back / do not touch anytime soon

These lanes are **not part of the current basecamp work and not part of the near-term queue**.
Keep them visibly separate so scaffolding/support work is not mistaken for partial feature delivery.

1. **Chat interface over in-game dialogue branches**
   - future feature lane, parked far back
2. **Tiny ambient-trigger NPC model**
   - future feature lane, parked far back

Do not reopen them during the current locker/basecamp slice.
Do not treat them as the next natural follow-up after Package 5.
Current ordering intent is that they stay buried until much later threat/worldwork exists, with Josef explicitly saying they are not for anytime soon and not before the later Mongol riders lane.
Do not describe adjacent harness or UI work as partial completion of those features.

---

## 9. Checkpointed, organic bulletin-board speech

**Status:** CHECKPOINTED / DONE FOR NOW

This slice is now considered done for now because the bundled success state in `SUCCESS.md` is checked.

Current honest state:
- ordinary player-facing board status asks now accept organic wording like `what needs making`, `what needs doing`, `got any craft work`, and `show me what needs doing`
- ordinary spoken board/job replies stay free of visible request-id glue and keep `show_board` / `show_job` / `job=` structure on the internal/log side only
- deterministic coverage still passes on the current tree for the widened organic parsing and cleaned spoken bark
- live run `.userdata/dev-harness/harness_runs/20260419_154244/` used the real camp-assignment seam, asked `what needs making`, and showed Katharina answering `Board's got 1 live and 1 old - 1 x bandages.` with no visible request-id glue
- the same live packet still had Robbie chime in as ordinary follower crosstalk on the McWilliams fixture; treat that as separate routing noise unless a later change makes it the active seam again

Keep this lane closed unless a fresh probe surfaces a real visible machine-speech seam again.

Canonical contract lives at `doc/organic-bulletin-board-speech-2026-04-09.md`.

---

## 10. Checkpointed — Plan/Aux pipeline helper

**Status:** CHECKPOINTED / DONE FOR NOW

Josef explicitly wanted a small helper for the `Plan.md` / auxiliary-doc pipeline so already-understood lanes stop requiring slow manual canon carpentry every time.

Current honest state:
- `tools/plan_aux_pipeline_helper.py` now provides a spec-driven preview/merge/emit/apply path for this repo's Plan/Aux workflow
- `emit` can now also write a terse downstream `andi.handoff.md` packet from the same validated classified contract when `handoff_needed` is `yes`
- the `apply` path still stays bounded to known existing canon headings and current active-lane anchors, so the workflow law is real without pretending broad freeform mutation is solved
- when a heading does not already exist, the honest fallback is still `emit` output for reviewer-visible paste/review instead of magical guessing
- narrow validation passed on a sample spec via `python3 -m py_compile tools/plan_aux_pipeline_helper.py`, `schema`, `show`, `emit`, emitted handoff review, and `apply` on a temp repo copy

Keep this lane closed unless later evidence shows the handoff output or bounded patch path lying about what the canon actually says.

Canonical contract lives at `doc/plan-aux-pipeline-helper-2026-04-09.md`.

---

## 11. Checkpointed — combat-oriented locker policy

**Status:** CHECKPOINTED / DONE FOR NOW

Josef wants locker development to lean harder toward sensible guard/combat outfits without throwing away the already-earned weird-garment safety wins.

The current narrow combat-policy slices are now landed on the current tree:
- `camp_locker_slot`, `all_camp_locker_slots()`, and the persisted locker-policy surface now include explicit `gloves`, `mask`, `belt`, and `holster` slots instead of the older 12-slot-only footing
- `classify_camp_locker_item()` now gives common combat support gear an explicit locker-policy home instead of dropping holsters on the floor or letting belt-like waist gear get lost in generic clothing logic
- the locker policy surface now also has a persisted `Prefer bulletproof gear` toggle, and body-armor / helmet scoring now leans harder toward higher bullet protection when that control is enabled
- body-armor scoring now also counts loaded ablative plates as real armor value, and same-type ballistic vests can be upgraded over damaged-plate variants instead of treating identical item ids as an automatic tie
- focused deterministic coverage now exercises loaded-vs-empty ballistic vest scoring, damaged insert scoring, same-type healthy-plate replacement behavior, and the positive/negative full-body-suit-vs-current-body-armor tradeoffs on the current tree
- protective full-body suits in the pants lane now also suppress missing-current shirt filler when the suit itself is already the better combat/protective packet, so survivor-style suits stop inviting junk T-shirts underneath them just because the shirt lane is empty
- that same protective-suit seam now also handles the direct current-outfit comparisons all the way through weaker body armor: when the suit wins the pants-lane upgrade honestly, weaker current shirts, vests, and body armor can be demoted into duplicate cleanup instead of being layered under the new suit, while stronger current ballistic armor remains equipped
- the current locker safety spine stayed intact while doing that: weird-garment preservation, weather-sensitive outerwear/legwear handling, full-body suit protection, and the earlier great-helm / holster safety cases still survive the filtered locker suite
- the final closure audit found one real remaining proof gap, namely end-to-end service evidence for the newly explicit combat-support slots
- that gap is now closed by deterministic test `camp_locker_service_equips_common_combat_support_slots`, which proves real `CAMP_LOCKER` service equips gloves, dust mask, tool belt, and holster from locker stock
- focused deterministic validation passed on the current tree via `make -j4 tests`, `./tests/cata_test "camp_locker_service_equips_common_combat_support_slots"`, and `./tests/cata_test "[camp][locker]"`

Keep this lane closed unless later code or runtime evidence breaks one of those claims.

Canonical contract lives at `doc/locker-combat-oriented-policy-2026-04-09.md`.

---

## 12. Checkpointed — Bandit concept formalization follow-through

**Status:** CHECKPOINTED / DONE FOR NOW

Josef wanted the remaining loose bandit control-law gaps tightened after the current basecamp fixes were honestly closed, but still as conceptualization/doc work rather than implementation.

This follow-through has now done that: the packet turns the remaining bandit economy, cadence, and cross-layer interaction rules into explicit doc slices without reopening code or pretending the whole bandit system is now active.

Frozen footing for this follow-through:
- no passive decay for bounty
- no passive decay for threat
- structural bounty is set by terrain / tile class and reduced by harvesting or exploitation
- moving bounty changes when real actors or visible activity change, not because an idle timer shaved numbers off in the dark
- bandit camp stockpile may go down by consumption, and any future spoilage rule must stay explicit and separate from bounty/threat logic

Default package order:
1. bounty source / harvesting / camp-stockpile rules
2. cadence / distance burden / route-fallback rules
3. cross-layer interaction packet with starter numbers and worked scenarios

The auxiliary contract intentionally decomposes those three packages into **31 narrow single-question micro-items** so Andi can freeze one law at a time instead of hiding several assumptions inside one friendly paragraph.

Current bounded slice:
- landed: Package 1, micro-item 1, `Terrain bounty bucket table`, now freezes a 0-5 starter site-class prior with the intended ordering `open street/field < forest < cabin < house < farm/city structure < camp footprint`
- landed: Package 1, micro-item 2, `Structural bounty harvest trigger rule`, now freezes that structural bounty only depletes once bandits actually extract, strip, or meaningfully deny site-bound value; scouting, visitation, and failed approaches do not count
- landed: Package 1, micro-item 3, `Structural bounty reappearance rule`, now freezes that depleted structural bounty stays down until the world adds new site-bound value back through real resupply, rebuilding, or reoccupation; fresh traffic or signals only restore moving interest
- landed: Package 1, micro-item 4, `Moving bounty source table`, now freezes the starter live source families for moving bounty instead of leaving humans, traffic, caravans, basecamp routine, smoke, light, and sound cues as one blurry blob
- landed: Package 1, micro-item 5, `Moving bounty attachment rule`, now freezes that moving bounty stays source-shaped, with actor/group, route/corridor, and site-centered activity attachments instead of being dumped back into structural terrain value
- landed: Package 1, micro-item 6, `Moving bounty clear / rewrite rule`, now freezes that moving bounty is cleaned up by evidence-driven refresh, overwrite, clear, replacement, or coexistence on the current carrier rather than by passive timer shave
- landed: Package 1, micro-item 7, `Threat source table`, now freezes that threat comes from direct humans, organized defenders, fortification signs, searchlights/watch routines, combat-contact cues, failed probes, recent losses, and zombie/monster pressure rather than from ordinary bounty-first activity clues
- landed: Package 1, micro-item 8, `Threat rewrite rule`, now freezes that threat only changes through source-specific recheck, with explicit raise / confirm / lower cases and no passive decay shortcut
- landed: Package 1, micro-item 9, `Threat-and-bounty coexistence rule`, now freezes that threat and bounty remain separate concurrent reads on the same region/carrier when the evidence supports both, with only source-specific contradiction clearing one side
- landed: Package 1, micro-item 10, `Destination-only vs along-route collection rule`, now freezes that outings may collect opportunistically on the current corridor or immediate intercept envelope, but not by broad side-sweep harvesting or free mission rewrites
- landed: Package 1, micro-item 11, `Collection yield rule`, now freezes a 0-3 per-outing haul cap so ingress stays bounded, route-side skims stay inside the same packet, and rich regions still need repeat exploitation
- landed: Package 1, micro-item 12, `Camp stockpile consumption rule`, now freezes one daily housekeeping drain plus explicit dispatch/return mission costs, with food/ammo/med pressure driven by those named costs rather than by hidden stockpile decay
- landed: Package 1, micro-item 13, `Forest yield rule`, now freezes forest as thin food/fuel/basic-material value, usually 0-1 haul, unless the outing actually touches a richer embedded site that should score by its own site class
- landed: Package 2, micro-item 14, `Daily movement budget rule`, now freezes a job-shaped 1-6 OMT/day starter envelope so local forage skims, probes, convoy hits, scavenge runs, raids, and rare redeploys stop sharing one mushy "few tiles" assumption
- landed: Package 2, micro-item 15, `Cadence budget-spend rule`, now freezes travel as elapsed-time-earned credit so cadence wakes can spend only what real world time has accrued instead of minting fresh movement every pass
- landed: Package 2, micro-item 16, `Distance burden rule`, now freezes target desirability as a round-trip-share discount against the outing's daily travel budget instead of a flat map-distance tax
- landed: Package 2, micro-item 17, `Return-clock rule`, now freezes a calm-condition outing leash with starter job clocks and a plain-return threshold, so groups prefer home before one chase turns into endless free pressure
- landed: Package 2, micro-item 18, `Cargo / wounds / panic burden rule`, now freezes a bounded additive burden table so cargo, wounds, and panic only shrink useful travel and remaining leash instead of secretly reopening distance scoring or fallback law
- landed: Package 2, micro-item 19, `No-target fallback rule`, now freezes `hold / chill` as the score-`0` stay-home baseline, so camps keep manpower home unless a real outward job beats that bar
- landed: Package 2, micro-item 20, `No-path fallback rule`, now freezes unreachable targets as dispatch-time `no_path` rejects that fall back to other reachable jobs or `hold / chill`, with a soft route-blocked memory instead of implicit wander behavior
- landed: Package 2, micro-item 21, `Mid-route abort / divert / shadow rule`, now freezes one bounded mid-route rewrite ladder, where an outing may only continue, probe, shadow, divert to a materially better same-envelope opportunity, or abort/reroute while spending the same remaining movement and leash
- landed: Package 3, micro-item 22, `Job candidate generation rule`, now freezes a separate board-building pass where `hold / chill` is always present and outward jobs only enter from deduped compatible lead envelopes before later scoring/veto/no-path filtering
- landed: Package 3, micro-item 23, `Job scoring formula shape`, now freezes a pre-veto comparison pass where positive pull (lead bounty, confidence, job-lead fit, mild need alignment, soft temperament bias, job-type bias) is shaped by the landed distance multiplier, then softened by threat and active-pressure penalties before later override/veto passes
- landed: Package 3, micro-item 24, `Need-pressure override rule`, now freezes one bounded post-score rescue where only `low`/`critical` shortage bands may add a capped reward-profile-matched bonus to mediocre real leads near `hold / chill`
- landed: Package 3, micro-item 25, `Threat veto vs soft-veto rule`, now freezes a three-step post-score danger ladder, where ordinary threat remains a subtraction, confirmed serious threat only preserves capped marginal info/pressure jobs, and catastrophic recent-loss / failed-probe style reads hard-veto the dispatch
- landed: Package 3, micro-item 26, `Overmap-to-bubble entry-mode chooser`, now freezes local handoff as a deterministic chooser from winning job, lead carrier, contact certainty, danger posture, and return pressure into bounded `scout` / `probe` / `harvest` / `ambush` / `raid` / `shadow` / `withdrawal` modes
- landed: Package 3, micro-item 27, `Bubble-to-overmap return-state packet`, now freezes a compact writeback field list covering survivors, anchored identities, wound/panic burden, carried cargo vs delivered stockpile delta, mission result, lead resolution, mark rewrites, and return posture
- landed: Package 3, micro-item 28, `Save/load persistence boundary`, now freezes the cheap durable boundary: persist camp ledgers, source-shaped marks, active abstract group state, bounded anchored identities, carried cargo/burden/leash, and minimal bubble-owned join keys, while exact loaded tactical NPC truth and one-wake score math stay outside the bandit save schema
- landed: Package 3, micro-item 29, `Starter numbers table`, now freezes one shared starter-number sheet for the whole packet, so structural bounty, mark bands, haul scale, movement budgets, distance burden, return clocks, burden multipliers, score-factor ranges, need rescue, threat caps, and anchored-identity count stop competing across leaf prose
- landed: Package 3, micro-item 30, `Worked scenarios packet`, now pushes the frozen law through concrete situation reads for forest-edge activity, depleted houses, scary city edges, long-distance temptation, unreachable paths, moving trails, shortage rescue, burdened withdrawal, and save/load continuity
- landed: Package 3, micro-item 31, `Invariants and non-goals packet`, now freezes the explicit must-never-happen sheet plus the deliberate v1 omissions, so later review can reject nonsense fast without reopening the already-landed law
- the 3-package / 31-micro-item follow-through is now complete as doc/spec cleanup inside the parked chain

Canonical contract lives at `doc/bandit-concept-formalization-followthrough-2026-04-19.md`.

Keep this lane closed unless later canon or review finds a real contradiction in the packet.
Do not treat it as permission to start coding bandit AI.
It is now a cleaner parked-chain substrate, not implementation approval.

---

## 13. Checkpointed — Plan status summary command

**Status:** CHECKPOINTED / DONE FOR NOW

Josef wanted a small read-only command surface that prints current plan categories from `Plan.md` so active, greenlit, and parked work stop feeling like a black box when Andi or Schani reshuffle canon.

Current honest state:
- `tools/plan_status_summary.py` now provides a deliberately small read-only seam for `/plan`, `/plan active`, `/plan greenlit`, and `/plan parked`
- the script reads `Plan.md` only, classifies sections from explicit status lines first, and falls back to heading wording only when canon is thin enough that a warning is warranted
- greenlit ordering stays meaningful: active first, queued greenlit next, and bottom-of-stack entries last instead of as a separate printed class
- the current `Hackathon feature lanes` heading already exercises the thin-canon warning path because it is parked by heading text without an explicit `**Status:**` line
- narrow validation passed via `python3 tools/plan_status_summary.py --self-test` plus direct sample readouts from current canon for `/plan`, `/plan active`, `/plan greenlit`, and `/plan parked`

Keep this lane closed unless later evidence shows the parser is lying about canon, the output shape is too noisy to trust, or Josef explicitly wants a second-stage integration slice.

Canonical contract lives at `doc/plan-status-summary-command-2026-04-20.md`.

---

## 14. Parked concept chain — Bandit overmap AI

**Status:** PARKED / CONCEPT CHAIN

Josef wants the larger bandit / overmap-threat idea developed as a parked concept chain first, then re-evaluated for greenlight only after the concept packet is coherent enough as a whole.
Do not quietly treat partial bandit notes as an active lane or as already-greenlit implementation.
The now-checkpointed formalization follow-through above was greenlit only for doc/spec cleanup inside this chain; broader bandit implementation still stays parked outside the explicit promoted v0 slices above.

Current parked-chain anchor:
- the broad synthesis paper lives at `doc/bandit-overmap-ai-concept-2026-04-19.md`

Current parked sub-items:
- deterministic bounty/threat scoring guidance v1 at `doc/bandit-bounty-threat-scoring-guidance-2026-04-19.md`
- overmap mark-generation and heatmap model v1 at `doc/bandit-mark-generation-and-heatmap-model-2026-04-19.md`
- bidirectional overmap-to-bubble handoff seam v1 at `doc/bandit-overmap-to-bubble-handoff-seam-2026-04-19.md`
- player/basecamp visibility and concealment v1 at `doc/bandit-player-basecamp-visibility-and-concealment-2026-04-19.md`

Supporting recon note for the visibility item:
- physical-systems recon at `doc/bandit-visibility-physical-systems-recon-2026-04-19.md`

What the current parked sub-items should do:
- scoring guidance: define deterministic camp-ledger and map-mark inputs for bandit decision-making, then score bounty/threat/job desirability from those inputs
- mark/heatmap guidance: define how overmap-only marks get created, rechecked, harvested, and folded into broad threat/bounty heatmaps on the same cadence family without passive decay math
- handoff guidance: define how abstract overmap groups enter local play and how cargo, wounds, panic, losses, and updated threat/bounty knowledge return back into overmap state
- visibility/concealment guidance: define signal sources, environmental filters, bounty/threat interpretation outputs, and player/basecamp exposure-reduction levers on recon-backed footing
- current acceptable footing: fog/mist can suppress sight/light legibility without forcing new fog-based sound dampening rules
- resolved note-1 anti-spiral footing: each camp should own its own sparse operational map rather than shared omniscience
- resolved note-1 bounty footing: ground bounty should be coarse, structural, finite, and non-regenerating by region class, while player/NPC/camp activity supplies the moving bounty layer
- resolved no-passive-decay footing: do not model passive decay for bounty or threat; structural bounty changes through harvesting/exploitation, moving bounty changes through fresh activity and collection, threat changes through real recheck, and camp stockpile changes through explicit consumption
- resolved note-1 dampers: false-lead / recently-checked / harvested memory and self-signal filtering should stop camps from spiraling into pseudo-psychic fixation
- resolved note-2 fairness footing: sustained pressure and stalking against legible camps is desired, but decisive full camp assault stays player-present only for current scope
- resolved note-2 attack-signaling footing: fairness should come from bounded offscreen consequence scope, not from requiring bandits to presignal attacks
- resolved note-2 repo footing: existing overmap-NPC persistence, travel, and companion plumbing is valid substrate for following outbound groups and route interception, but the current need-driven wandering policy is not the finished hostile model
- resolved note-3 identity footing: persistent overmap group continuity is mandatory, with only a small anchored-individual slice surviving handoffs directly while the remaining membership stays fungible
- resolved note-3 goal-sustainability footing: smoke/mark destinations are provisional mission leads, not sacred tile commitments, and local observations may continue, divert, shadow, or abort the current goal
- resolved note-3 scope footing: v1 should cap anchored continuity at roughly 1-3 individuals rather than forcing full per-bandit persistence everywhere
- resolved note-4 city-opportunism footing: cities may support one-off or occasional opportunistic bandit action under zombie chaos, but not persistent farming, because structural bounty depletes while threat remains high
- resolved note-4 zombie-model footing: zombies currently matter as threat and target-coherence pressure only, not as a separately simulated bandit-versus-zombie tactical system
- resolved note-4 sticky-threat footing: if a group leaves due to threat, the area should keep a sticky scary mark that is only meaningfully reevaluated on close revisit or when some other attraction pulls the group back nearby
- resolved note-5 independence footing: v1 camps should behave as mostly independent actors rather than a coordinated coalition strategy layer, so overlap may happen by accident but routine dogpile convergence should not be the norm
- resolved note-5 anti-dogpile footing: territoriality, distance burden, depletion, sticky threat, and fresh active-pressure penalties should suppress repeated multi-camp pile-ons against the same region
- resolved note-5 scope footing: deliberate alliances, campaign planning, and explicit multi-camp coordination are later-layer strategy material, not part of the current parked v1 shape

The intended parked-chain order for now is:
1. broad concept vessel
2. deterministic bounty/threat scoring
3. overmap mark-generation and heatmap model
4. bidirectional overmap-to-bubble handoff seam
5. player/basecamp visibility and concealment, informed by the physical-systems recon note
6. only then decide whether the whole bandit concept is ready to be promoted into greenlit backlog

The broad anchor doc has now been rewritten into the synthesis paper for the parked chain.
If the packet is revisited later, the next planning discussion should be about whether the packet is coherent enough for future greenlight consideration, not about spawning more disconnected feeder docs by default.

---

## 15. Documentation discipline

If the structure starts bloating again, apply this rule:
- `Plan.md` should be readable in a minute
- `TODO.md` should show only the current execution queue
- `TESTING.md` should show only current policy, latest relevant evidence, and pending probes
- `COMMIT_POLICY.md` should stop the dirty tree from becoming a lifestyle

If a sentence exists only to remember that something used to be true, it probably belongs in git history, not here.
