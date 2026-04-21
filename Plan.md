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

There is currently one active greenlit bandit lane: **Bandit overmap benchmark suite packet v0**.
The earlier **Bandit weather concealment refinement packet v0**, **Bandit overmap/local pressure rewrite packet v0**, **Bandit long-range directional light proof packet v0**, **Bandit scoring refinement seam v0**, **Bandit moving-bounty memory seam v0**, and **Bandit bounded scout/explore seam v0** are checkpointed closed.
Do not reopen them just because they are nearby.

Current active contract:
- the authoritative contract lives at `doc/bandit-overmap-benchmark-suite-packet-v0-2026-04-21.md`
- build one complete named overmap scenario family with explicit `100`-turn benchmarks plus `500`-turn carry-through checks where the longer horizon is the honest proof burden
- include the empty-frontier / increase-visibility case explicitly, so a camp with nothing useful nearby ventures out through bounded scout/explore behavior instead of sitting forever
- keep `Bandit z-level visibility proof packet v0` not greenlit until this suite exists and has already surfaced the next real routing problems
- treat benchmark misses as routing-logic failures to fix, not as excuses to water the benchmarks down

Fresh checkpoint that now stays closed:
- **Bandit weather concealment refinement packet v0** is now honestly checkpointed too:
  - the authoritative contract lives at `doc/bandit-weather-concealment-refinement-packet-v0-2026-04-21.md`
  - `src/bandit_mark_generation.{h,cpp}` now carries one bounded weather-refinement packet on the current smoke/light seam, with windy smoke losing source precision and confidence instead of taking only a token range haircut, plus explicit `portal_storm` handling for smoke and light
  - reviewer-readable smoke/light summaries now expose the weather read directly, including fuzzed, reduced, blocked, displaced/corridor-ish, and preserved bright-light cases instead of hiding the interpretation in debugger soup
  - `src/bandit_playback.cpp` now carries explicit windy-smoke and portal-storm exposed-light scenarios on the current generated-mark seam, and `tests/bandit_playback_test.cpp` keeps the broader reference suite count aligned with those additions instead of pretending the suite froze in amber
  - deterministic coverage in `tests/bandit_mark_generation_test.cpp` and `tests/bandit_playback_test.cpp` now proves the bounded weather distinctions honestly: windy smoke stays scoutable but fuzzier, portal-storm smoke is harder to localize, exposed bright portal-storm light can stay legible while sheltered ordinary light stays bounded, and rain remains an explicit reducer
  - narrow deterministic validation passed on the current tree via `make -j4 tests`, `./tests/cata_test "[bandit][marks]"`, `./tests/cata_test "[bandit][playback]"`, and `./tests/cata_test "[bandit]"`

Fresh checkpoints that stay closed:
- **Bandit overmap/local pressure rewrite packet v0** is now honestly checkpointed too:
  - the authoritative contract lives at `doc/bandit-overmap-local-pressure-rewrite-packet-v0-2026-04-21.md`
  - `src/bandit_playback.{h,cpp}` now carries the named `generated_local_loss_rewrites_corridor_to_withdrawal` and `generated_local_loss_reroutes_to_safer_detour` multi-turn scenarios plus `run_overmap_local_pressure_rewrite_proof_packet()` and `render_overmap_local_pressure_rewrite_proof_packet( const proof_packet_result &result )` on the current playback seam
  - deterministic coverage in `tests/bandit_playback_test.cpp` now proves the key bounded distinctions honestly: one stale corridor posture collapses to `hold / chill` once local loss makes the tile too hot, one parallel case reroutes onto a safer detour instead of homing forever on the burned route, and the long horizon stays off the stale pressure seam
  - narrow deterministic validation passed on the current tree via `make -j4 tests`, `./tests/cata_test "[bandit][playback]"`, and `./tests/cata_test "[bandit]"`
- **Bandit long-range directional light proof packet v0** is now honestly checkpointed too:
  - the authoritative contract lives at `doc/bandit-long-range-directional-light-proof-packet-v0-2026-04-21.md`
  - `src/bandit_playback.{h,cpp}` now carries three named directional-light multi-turn scenarios plus `run_long_range_directional_light_proof_packet()` and `render_long_range_directional_light_proof_packet( const proof_packet_result &result )` on the current playback seam
  - deterministic coverage in `tests/bandit_playback_test.cpp` now proves the key bounded distinctions honestly: hidden-side leakage stays non-actionable, visible-side leakage becomes actionable under the same surrounding footing, and the matching corridor packet still carries shared zombie-pressure consequences instead of private bandit-only truth
  - narrow deterministic validation passed on the current tree via `make -j4 tests`, `./tests/cata_test "[bandit][playback]"`, and `./tests/cata_test "[bandit]"`
- **Bandit moving-bounty memory seam v0** is now honestly checkpointed too:
  - `src/bandit_mark_generation.{h,cpp}` now keeps a bounded moving-memory packet on moving-carrier and corridor marks only, with leash, opportunity/threat bands, reviewer-readable transition state, and prune behavior that preserves active moving memory instead of dropping it the moment raw signal values cool
  - structural/site marks stay on cheap site footing, so harvested or revisited places do not quietly grow stalking logic
  - deterministic coverage in `tests/bandit_mark_generation_test.cpp` now proves the key bounded distinctions honestly: moving prey can be stalked briefly after raw signal cooling, structural/site marks do not gain chase memory, and stale moving contact drops reviewer-cleanly on leash expiry
  - `tests/bandit_playback_test.cpp` now keeps the corridor playback expectation aligned with the bounded memory window instead of pretending the mark cools straight to quiet
  - the slice stayed bounded: no per-turn tracking, no path-history scrapbook, no per-NPC biography graph, no endless retry loop, and no broad memory-palace world model
  - narrow deterministic validation passed via `make -j4 tests` and `./tests/cata_test "[bandit]"`
- **Bandit first 500-turn playback proof v0** is now honestly checkpointed too:
  - `src/bandit_playback.{h,cpp}` now carries `proof_packet_result`, `run_first_500_turn_playback_proof()`, and `render_first_500_turn_playback_proof( const proof_packet_result &result )` so reviewer-readable long-horizon proof output exists on the current playback seam instead of living as debugger folklore
  - the first bounded proof packet reuses three named scenario cases, `smoke_only_distant_clue`, `city_edge_moving_hordes`, and `generated_repeated_site_reinforcement_stays_bounded`, with explicit `tick 500` checkpoint framing so cooldown, peel-off, and repeated-site boundedness stay inspectable without hand-waving
  - `generated_repeated_site_reinforcement_stays_bounded` now includes additional idle-horizon frames on ordinary inactive cadence, which keeps repeated site interest bounded over five hundred turns instead of letting one reinforced clue become immortal scout pressure
  - deterministic coverage in `tests/bandit_playback_test.cpp` now proves the packet honestly: smoke cools back to `hold_chill`, the city-edge peel-off stays bounded, and the repeated-site case eventually cools back out instead of unlocking magical settlement truth or free extraction
  - the slice stayed bounded: no new visibility adapter family, no broader overmap simulator, no persistence rewrite, and no live-harness-first theater
  - narrow deterministic validation passed via `make -j4 tests` and `./tests/cata_test "[bandit]"`
- **Bandit repeated site activity reinforcement seam v0** is now honestly checkpointed too:
  - `src/bandit_mark_generation.{h,cpp}` now carries a bounded `repeated_site_reinforcement_packet` on site marks, requiring mixed repeated smoke/light/route activity before modest confidence and bounty amplification appears
  - `src/bandit_playback.cpp` now adds `generated_repeated_site_reinforcement_stays_bounded`, while reviewer-readable mark/lead reports expose the reinforcement packet instead of hiding it in debugger soup
  - deterministic coverage in `tests/bandit_mark_generation_test.cpp` and `tests/bandit_playback_test.cpp` now proves the key bounded distinctions honestly: mixed repeated signals reinforce one site mark cleanly, weak repetition stays weak, self-corroboration stays bounded, and strengthened site interest still keeps extraction jobs blocked
  - the product rules stay frozen here: repeated ordinary site signals raise revisit interest first, not free settlement truth, free loot truth, or a magical raid warrant
  - the slice stayed bounded: no smoke/light/human-route rewrite, no broad concealment implementation, no settlement-signature mythology, and no 500-turn proof smuggling
  - narrow deterministic validation passed via `make -j4 tests` and `./tests/cata_test "[bandit]"`
- **Bandit human / route visibility mark seam v0** is now honestly checkpointed too:
  - `src/bandit_mark_generation.{h,cpp}` now adds deterministic human/route packets plus a bounded adapter, translating direct sightings and repeated route-shaped activity into moving-carrier, corridor, or bounded site marks instead of leaving traffic clues as parked prose
  - `src/bandit_playback.{h,cpp}` now feeds those packets through the existing generated-mark seam, with one direct human-sighting case and one shared-route refresh case proving the bridge into evaluator/playback output
  - deterministic coverage in `tests/bandit_mark_generation_test.cpp` and `tests/bandit_playback_test.cpp` now proves the key bounded distinctions honestly: direct sightings stay mobile, same-camp routine traffic is suppressed, shared/external route activity can reinforce corridors, and only site-correlated traffic yields bounded site clues with extraction still blocked
  - the product rules stay frozen here: direct human sightings are strong bounty clues, route activity only hardens when it plausibly belongs to somebody else or a shared corridor, and the camp's own routine traffic does not self-poison into hostile-contact truth
  - the slice stayed bounded: no smoke/light rewrite, no broad concealment implementation, no settlement-signature mythology, no full traffic simulator, and no 500-turn proof theater
  - narrow deterministic validation passed via `make -j4 tests` and `./tests/cata_test "[bandit]"`
- **Bandit light visibility mark seam v0** is now honestly checkpointed too:
  - `src/bandit_mark_generation.{h,cpp}` now adds deterministic light packets plus a bounded light adapter, translating exposed night light and searchlight-style packets into coarse overmap-readable marks instead of leaving light clues as parked prose
  - `src/bandit_playback.{h,cpp}` now feeds those packets through the existing generated-mark seam, with one ordinary exposed night-light case and one searchlight corridor case proving the bridge into evaluator/playback output
  - deterministic coverage in `tests/bandit_mark_generation_test.cpp` and `tests/bandit_playback_test.cpp` now proves the key bounded distinctions honestly: daylight suppression, contained versus exposed night light, side leakage, ordinary occupancy light versus searchlight-like threat light, and reviewer-readable packet reporting
  - the product rule stays frozen here: meaningful exposed night light can be legible from quite far away, but daylight and containment suppress the magical house-glow fantasy
  - the slice stayed bounded: no smoke rewrite, no broad concealment implementation, no sound/horde expansion, no global offscreen light sim, and no 500-turn proof theater
  - narrow deterministic validation passed via `make -j4 tests` and `./tests/cata_test "[bandit]"`
- **Bandit smoke visibility mark seam v0** is now honestly checkpointed too:
  - `src/bandit_mark_generation.{h,cpp}` now adds deterministic smoke packets plus a bounded smoke adapter, while `src/bandit_playback.{h,cpp}` feeds those packets through the existing generated-mark seam instead of leaving smoke as hand-authored scenario lore
  - deterministic coverage in `tests/bandit_mark_generation_test.cpp` and `tests/bandit_playback_test.cpp` now proves the coarse long-range rule honestly: sustained clear-weather smoke stays several OMT legible with a hard cap, while weak fogged smoke does not fake long-range truth
  - reviewer-readable playback / mark reports now expose the smoke packet projection and resulting mark/lead path instead of hiding the bridge in debugger soup
  - the slice stayed bounded: no light/searchlight adapter, no broad visibility/concealment implementation, no global offscreen smoke sim, and no 500-turn proof theater
  - narrow deterministic validation passed via `make -j4 tests` and `./tests/cata_test "[bandit]"`
- **Locker lag-threshold probe v0** is now honestly checkpointed too:
  - `src/basecamp.{h,cpp}` now keeps the real `camp_locker_service_probe` seam quiet during timing runs, while `tests/faction_camp_test.cpp` drives the threshold packet through real multi-tile `CAMP_LOCKER` zones so counts stay honest past the single-tile `MAX_ITEM_IN_SQUARE` ceiling
  - the threshold packet now covers top-level clutter at `1000 / 2000 / 5000 / 10000 / 20000` plus worker-count sweeps at `1 / 5 / 10` on `5000` clutter, staying on the real service path instead of synthetic guesswork
  - the current verdict is `not found within tested bound`: median service cost stayed roughly linear from about `210 us` at `1000` clutter to about `4152 us` at `20000`, and the `5000`-clutter worker sweep stayed around `1.0 ms` per worker across `1 / 5 / 10`
  - this packet does **not** justify a fresh guardrail yet; if the lane reopens later, keep the cheap-first guardrail order instead of jumping straight to architecture opera
  - narrow deterministic validation passed via `make -j4 tests`, `./tests/cata_test "[camp][locker]~[threshold]"`, and `./tests/cata_test "[camp][locker][threshold]"`
- **Locker clutter / perf guardrail probe v0** is now honestly checkpointed too:
  - `src/basecamp.{h,cpp}` now contains a real locker-service probe seam through `camp_locker_service_probe`, `basecamp::measure_camp_locker_service( npc & )`, and `render_camp_locker_service_probe()` instead of fake-path guesswork
  - the first bounded direct packet now covers top-level clutter sweeps at `50 / 100 / 200 / 500 / 1000`, worker-count sweeps at `1 / 5 / 10`, the first junk-heavy / locker-candidate-heavy / ammo-magazine-container-heavy stock-shape comparison, and the nested-content question for loaded magazines and ordinary filled bags on the real `CAMP_LOCKER` service path
  - the current verdict is `fine for now`: observed service cost grows with top-level locker items and worker passes, while loaded magazines and ordinary filled bags still behave like one top-level locker item on this path instead of producing an obvious nested-content cliff
  - narrow deterministic validation passed via `make -j4 tests` and `./tests/cata_test "[camp][locker]"`
- **Plan status summary command** is now honestly checkpointed too:
  - `tools/plan_status_summary.py` now provides a deliberately small read-only seam for `/plan`, `/plan active`, `/plan greenlit`, and `/plan parked`
  - it reads `Plan.md` only, preserves greenlit ordering, and warns when canon is thin or contradictory instead of inventing certainty
  - narrow validation passed via `python3 tools/plan_status_summary.py --self-test` plus direct current-canon samples for `/plan`, `/plan active`, `/plan greenlit`, and `/plan parked`
- **Bandit scenario fixture + playback suite v0** is now honestly checkpointed too:
  - `src/bandit_playback.{h,cpp}` now defines eighteen stable named reference scenarios on top of the dry-run evaluator seam, including the generated smoke, ordinary night-light, directional hidden/visible split, shared horde-pressure corridor, direct human-sighting, shared-route, repeated-site, and sticky-threat cases added by the visibility/mark packets, instead of pretending a full overmap simulator already exists
  - `run_scenario()` now replays those cases at stable checkpoints (`tick 0`, `tick 5`, `tick 20`, `tick 100`), and `render_report()` exposes winner drift plus generated mark/heat state so idle, smoke-investigation, corridor stalking, moving-carrier attachment, peel-off, repeated-site boundedness, and sticky-threat behavior can be inspected reviewer-cleanly
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

Meaning:
- the locker lag-threshold probe is checkpointed closed too, with no clear knee found through `20000` top-level locker items
- the smoke-specific bridge is checkpointed closed too
- the light-specific bridge is checkpointed closed too
- the human / route bridge is checkpointed closed too
- the repeated site-centered reinforcement bridge is checkpointed closed too, with mixed repeated smoke/light/traffic now yielding bounded confidence and bounty amplification on one site mark
- repeated site activity raises revisit interest first and still does not mint free settlement truth or free extraction permission
- the first honest 500-turn playback proof is checkpointed closed too on the current abstract bandit seams
- the concealment seam is checkpointed closed too on the current light-mark footing
- there is currently no active greenlit bandit lane in repo canon
- the bandit overmap-to-bubble pursuit handoff seam is checkpointed closed too
- the bandit mark-generation + heatmap seam is checkpointed closed too
- the plan status summary command is checkpointed closed
- the locker clutter / perf packet is checkpointed closed too
- the bandit scenario playback suite is checkpointed closed too
- the bandit follow-through lane is checkpointed closed too
- the first bandit evaluator seam is checkpointed closed too
- the bandit perf/persistence probe is checkpointed closed too
- the broad bandit concept chain stays parked outside those explicit promoted slices

---

## Checkpointed — Bandit first 500-turn playback proof v0

**Status:** CHECKPOINTED / DONE FOR NOW

The first honest deterministic 500-turn playback proof now exists and is honest enough to stay closed for now.
The canonical contract lives at `doc/bandit-first-500-turn-playback-proof-v0-2026-04-20.md`.

Current honest state:
- `src/bandit_playback.{h,cpp}` now provides `proof_packet_result`, `run_first_500_turn_playback_proof()`, and `render_first_500_turn_playback_proof( const proof_packet_result &result )` so the long-horizon packet is a first-class report path on the existing playback seam
- the first bounded proof packet reuses the named `smoke_only_distant_clue`, `city_edge_moving_hordes`, and `generated_repeated_site_reinforcement_stays_bounded` scenarios, with explicit `tick 500` framing so reviewer-readable winner drift and generated-mark state stay visible on the longer horizon
- the repeated-site case now cools honestly on the idle horizon instead of becoming immortal scout pressure, while smoke and city-edge peel-off also stay bounded at `tick 500`
- the slice stayed bounded: no new visibility adapter family, no broader overmap simulator, no persistence rewrite, and no live-harness-first theater
- narrow deterministic validation passed via `make -j4 tests` and `./tests/cata_test "[bandit]"`

Keep this lane closed unless later evidence shows the long-horizon packet is lying, too optimistic about cooling, or too noisy to trust as reviewer-readable proof.

## Checkpointed — Bandit repeated site activity reinforcement seam v0

**Status:** CHECKPOINTED / DONE FOR NOW

The fourth promoted visibility bridge now exists and is honest enough to stay closed for now.

Current honest state:
- the canonical contract lives at `doc/bandit-repeated-site-activity-reinforcement-seam-v0-2026-04-20.md`
- the bounded repeated-site reinforcement packet now exists on the current bandit seam, turning mixed repeated smoke/light/traffic activity into modest confidence and bounty amplification on one existing site mark instead of a magical settlement-signature class
- repeated-site reinforcement now feeds the current bandit mark-generation / playback seam as reviewer-readable strengthening of ordinary signals instead of staying parked prose
- the product rules stay preserved: weak repetition stays weak, mixed repeated signals raise revisit interest first, and extraction jobs stay blocked on that packet
- narrow deterministic validation passed via `make -j4 tests` and `./tests/cata_test "[bandit]"`

Keep this lane closed unless later evidence shows the packet is lying, too generous, too timid, or needs a truly different reinforcement law.

## Checkpointed — Bandit human / route visibility mark seam v0

**Status:** CHECKPOINTED / DONE FOR NOW

The third promoted visibility bridge now exists and is honest enough to stay closed for now.

Current honest state:
- the canonical contract lives at `doc/bandit-human-route-visibility-mark-seam-v0-2026-04-20.md`
- the bounded human / route adapter now exists on the current bandit seam, turning deterministic human/route packets into moving-carrier, corridor, or bounded site signal state
- human / route now feeds the current bandit mark-generation / playback seam as reviewer-readable traffic clues instead of staying parked prose
- the product rules stay preserved: direct human sightings stay mobile bounty clues, same-camp routine traffic stays suppressed, and only external/shared/corroborated route activity hardens into corridor or bounded site pressure
- narrow deterministic validation passed via `make -j4 tests` and `./tests/cata_test "[bandit]"`

Keep this lane closed unless later evidence shows the packet is lying, too generous, too timid, or needs a truly different adapter class.

## Checkpointed — Bandit light visibility mark seam v0

**Status:** CHECKPOINTED / DONE FOR NOW

The second promoted visibility bridge now exists and is honest enough to stay closed for now.

Current honest state:
- the canonical contract lives at `doc/bandit-light-visibility-mark-seam-v0-2026-04-20.md`
- the bounded light adapter now exists on the current bandit seam, turning deterministic light packets into coarse overmap-readable light signal state for ordinary night light and searchlight-style threat reads
- light now feeds the current bandit mark-generation / playback seam as reviewer-readable occupancy/bounty or searchlight/threat clues instead of staying parked prose
- the product rules stay preserved: meaningful exposed night light can stay several OMT legible with a hard cap, while daylight and contained light do not fake distant truth
- narrow deterministic validation passed via `make -j4 tests` and `./tests/cata_test "[bandit]"`

Keep this lane closed unless later evidence shows the light packet is lying, too generous, too timid, or needs a truly different adapter class.

## Checkpointed — Bandit smoke visibility mark seam v0

**Status:** CHECKPOINTED / DONE FOR NOW

The first promoted visibility bridge now exists and is honest enough to stay closed for now.

Current honest state:
- the canonical contract lives at `doc/bandit-smoke-visibility-mark-seam-v0-2026-04-20.md`
- the bounded smoke adapter now exists on the current bandit seam, turning deterministic smoke packets into coarse overmap-readable smoke signal state
- smoke now feeds the current bandit mark-generation / playback seam as bounty-first, reviewer-readable `worth scoping out` leads instead of staying hand-authored scenario lore
- the product rule stays preserved: sustained clear-weather smoke remains several OMT legible with a hard cap instead of shrinking into comically short-range OMT intuition
- narrow deterministic validation passed via `make -j4 tests` and `./tests/cata_test "[bandit]"`

Keep this lane closed unless later evidence shows the smoke packet is lying, too thin, or needs a truly different adapter class.

## Checkpointed — Locker lag-threshold probe v0

**Status:** CHECKPOINTED / DONE FOR NOW

The sharper locker threshold packet is now honest enough to stay closed for now.

Current contract:
- the canonical contract lives at `doc/locker-lag-threshold-probe-v0-2026-04-20.md`
- `src/basecamp.{h,cpp}` now suppresses locker `DebugLog` noise for probe timing runs, and `tests/faction_camp_test.cpp` now drives the threshold packet through real multi-tile `CAMP_LOCKER` zones so `5000 / 10000 / 20000` top-level clutter counts stay honest instead of colliding with the single-tile `MAX_ITEM_IN_SQUARE` cap
- the top-level clutter sweep now reaches `1000 / 2000 / 5000 / 10000 / 20000`, while the worker-count sweep stays realistic at `1 / 5 / 10` on `5000` clutter
- the current result is an honest `not found within tested bound`: median service time stayed roughly linear from about `210 us` at `1000` clutter to about `4152 us` at `20000`, and the `5000`-clutter worker packet stayed around `1.0 ms` per worker across `1 / 5 / 10`
- no fresh guardrail is justified from this packet alone; if later evidence reopens the lane, keep the cheap-first guardrail order instead of architecture opera
- narrow deterministic validation passed via `make -j4 tests`, `./tests/cata_test "[camp][locker]~[threshold]"`, and `./tests/cata_test "[camp][locker][threshold]"`

Keep this lane closed unless later evidence shows the threshold packet was dishonest, player-facing hitching appears below this tested bound, or a genuinely justified cheap guardrail answer appears.

## Checkpointed — Bandit overmap-to-bubble pursuit handoff seam v0

**Status:** CHECKPOINTED / DONE FOR NOW

The first bounded bandit handoff now exists and is honest enough to stay closed for now.

Current honest state:
- the canonical contract lives at `doc/bandit-overmap-to-bubble-pursuit-handoff-seam-v0-2026-04-20.md`
- `src/bandit_pursuit_handoff.{h,cpp}` now provides the bounded overmap-to-bubble seam, translating a winning pursuit candidate plus small abstract group state into an explicit `entry_payload` and explicit `return_packet`
- the current chooser stays intentionally narrow on `scout` / `probe` / `shadow` / `withdrawal`, preserving one pursuit / investigation slice instead of widening into raid or ambush theater
- `apply_return_packet()` now carries back meaningful abstract consequences such as losses, wound / morale burden, carried-vs-delivered cargo, threat / bounty writeback, learned marks, and retreat pressure instead of dropping them on the floor
- `render_report()` plus deterministic coverage in `tests/bandit_pursuit_handoff_test.cpp` keep the seam reviewer-readable and prove the bounded scout entry case, explicit return consequences, and moving-carrier shadow path
- narrow deterministic validation passed via `make -j4 tests`, `./tests/cata_test "[bandit][handoff]"`, and `./tests/cata_test "[bandit]"`

Keep this lane closed unless later evidence shows the packet is lying, no longer bounded enough, or needs a truly different local handoff class.

---

## Checkpointed — Bandit mark-generation + heatmap seam v0

**Status:** CHECKPOINTED / DONE FOR NOW

The writer-side bandit seam now exists and is honest enough to stay closed for now.

Current honest state:
- the canonical contract lives at `doc/bandit-mark-generation-heatmap-seam-v0-2026-04-20.md`
- `src/bandit_mark_generation.{h,cpp}` now provides a bounded overmap-side mark ledger with deterministic signal ingestion, refresh, selective cooling, sticky confirmed threat, generated lead emission, and a reviewer-readable mark/heat report instead of hand-authored lead theater
- `src/bandit_playback.{h,cpp}` now bridges generated marks/leads into the existing playback/evaluator footing, carries generated mark state through checkpoints, and exposes that state in playback reports
- deterministic coverage in `tests/bandit_mark_generation_test.cpp` and `tests/bandit_playback_test.cpp` now proves mark creation, refresh, selective cooling, sticky confirmed threat, and the clean bridge into the evaluator / playback seam
- narrow deterministic validation passed via `make -j4 tests` and `./tests/cata_test "[bandit]"`

Keep this lane closed unless later evidence shows the writer-side ledger/report bridge is lying, too thin, or no longer bounded enough for reviewer trust.

## Checkpointed — Locker clutter / perf guardrail probe v0

**Status:** CHECKPOINTED / DONE FOR NOW

This follow-up existed because player hoarding is ordinary play, while camp populations above about ten NPCs are comparatively rare.
The believable failure mode was a curated locker zone turning into a universal junk carpet and making service cost spike in ways the player could not read.

Current honest state:
- the canonical contract lives at `doc/locker-clutter-perf-guardrail-probe-v0-2026-04-20.md`
- `src/basecamp.{h,cpp}` now contains a real locker-service probe seam through `camp_locker_service_probe`, `basecamp::measure_camp_locker_service( npc & )`, and `render_camp_locker_service_probe()` instead of fake-path guesswork
- the first bounded direct packet now covers top-level clutter sweeps at `50 / 100 / 200 / 500 / 1000`, worker-count sweeps at `1 / 5 / 10`, the first junk-heavy / locker-candidate-heavy / ammo-magazine-container-heavy stock-shape comparison, and the nested-content question for loaded magazines and ordinary filled bags on the real `CAMP_LOCKER` service path
- the current verdict is `fine for now`: observed service cost grows with top-level locker items and worker passes, while loaded magazines and ordinary filled bags still behave like one top-level locker item on this path instead of producing an obvious nested-content cliff
- if later timing evidence says the still-linear top-level scan is too expensive, prefer cheap guardrails such as early junk-ignore, bounded candidate consideration, or a simple curated-stock warning/cap before inventing broad architecture

Keep this lane closed unless later timing-specific evidence says the current direct packet is lying or too thin.

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
- `src/bandit_playback.{h,cpp}` now defines eighteen stable deterministic reference scenarios on top of the bounded dry-run evaluator, including the generated directional-light split and shared horde-pressure corridor cases layered onto the earlier writer-side suite, instead of inventing a broader world simulator
- the playback seam now reruns those scenarios at `tick 0`, `tick 5`, `tick 20`, and `tick 100`, which is enough to inspect idle, smoke, corridor, moving-carrier, split-route, city-edge peel-off, repeated-site boundedness, generated cooling, refresh, and sticky-threat drift without smuggling in a bigger system
- `render_report()` now provides a reviewer-readable checkpoint summary plus generated mark/heat state when present, and deterministic coverage in `tests/bandit_playback_test.cpp` proves the named scenario set, the stable checkpoints, and the expected drift answers
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

## 14. Checkpointed — Bandit scoring refinement seam v0

**Status:** CHECKPOINTED / DONE FOR NOW

The bounded scoring packet now exists on the current bandit dry-run/evaluator seam and is honest enough to stay closed for now.
Do not widen it into vague parked lore or fresh world-sim theater.

Current honest state:
- the authoritative contract lives at `doc/bandit-scoring-refinement-seam-v0-2026-04-21.md`
- `src/bandit_dry_run.{h,cpp}` now carries the bounded danger-collapse packet on the current evaluator seam, forwarding existing monster-pressure and target-coherence footing into `effective_threat_penalty` plus a small opportunism bonus instead of inventing a fresh threat astrology chart
- `src/bandit_mark_generation.cpp` now forwards `monster_pressure_add` and `target_coherence_subtract` into emitted leads so the current marks actually drive the refined score choice instead of burying that footing inside one undifferentiated threat number
- deterministic coverage in `tests/bandit_dry_run_test.cpp` and `tests/bandit_mark_generation_test.cpp` now proves the key bounded distinctions honestly: one clearly strong target still gets deferred, one distracted target becomes materially more attractive, one neutral case stays sane, and reviewer-readable output exposes raw threat, collapsed threat, and the bounded opportunism window
- the slice stayed bounded: no new visibility signals, no broad heatmap/memory rewrite, no tactical zombie simulation, no coalition strategy layer, and no fresh world-sim expansion
- narrow deterministic validation passed on the current tree via `make -j4 tests` and `./tests/cata_test "[bandit]"`

Keep this lane closed unless later canon or later evidence says the scoring packet is dishonest, too weak, or too generous.

---

## 15. Checkpointed — Bandit moving-bounty memory seam v0

**Status:** CHECKPOINTED / DONE FOR NOW

The bounded moving-memory packet now exists on the current bandit seam and is honest enough to stay closed for now.
Keep it small, cheap, and stupid in the good way.
Do not quietly widen it into a general memory architecture pass.

Current honest state:
- the authoritative contract lives at `doc/bandit-moving-bounty-memory-seam-v0-2026-04-21.md`
- `src/bandit_mark_generation.{h,cpp}` now keeps a bounded `moving_bounty_memory` packet on moving-carrier and corridor marks only, with leash, opportunity/threat bands, and reviewer-readable transition state
- structural/site marks stay on cheap site footing such as harvested / recently-checked / false-lead / sticky threat and do not gain chase memory
- deterministic coverage in `tests/bandit_mark_generation_test.cpp` and `tests/bandit_playback_test.cpp` now proves the key bounded distinctions honestly: moving prey can be stalked briefly after raw signal cooling, structural sites do not gain chase memory, and stale moving contact drops reviewer-cleanly instead of retrying forever
- reviewer-readable output now shows whether a moving lead was refreshed, narrowed, or dropped instead of hiding the memory state in debugger soup
- the slice stayed computationally cheap: no per-turn tracking, no path-history scrapbook, no per-NPC biography graph, no endless retry loop, and no broad memory-palace world model
- narrow deterministic validation passed on the current tree via `make -j4 tests` and `./tests/cata_test "[bandit]"`

Keep this lane closed unless later canon or later evidence says the memory packet is dishonest, too generous, or too sticky.

---

## 16. Checkpointed — Bandit long-range directional light proof packet v0

**Status:** CHECKPOINTED / DONE FOR NOW

Josef explicitly greenlit this as a narrow bandit promotion, and it is now honestly landed on the current tree.
The proof stayed on the current playback/generated-mark footing.
It used real overmap-side multi-turn scenarios instead of one-tick theater.

Current honest state:
- the authoritative contract lives at `doc/bandit-long-range-directional-light-proof-packet-v0-2026-04-21.md`
- `src/bandit_playback.{h,cpp}` now carries the named `generated_directional_light_hidden_side_stays_inert`, `generated_directional_light_visible_side_becomes_actionable`, and `generated_directional_light_corridor_shares_horde_pressure` scenarios on the current playback seam
- the same file now also exposes `run_long_range_directional_light_proof_packet()` plus `render_long_range_directional_light_proof_packet( const proof_packet_result &result )`, so the reviewer-readable packet is first-class instead of debugger folklore
- deterministic coverage in `tests/bandit_playback_test.cpp` now proves the key bounded distinctions honestly: hidden-side leakage stays non-actionable, visible-side leakage becomes actionable under the same broader footing, and the matching corridor variant still carries shared zombie-pressure consequences instead of becoming private bandit-only truth
- narrow deterministic validation passed on the current tree via `make -j4 tests`, `./tests/cata_test "[bandit][playback]"`, and `./tests/cata_test "[bandit]"`

Keep this lane checkpointed closed unless later evidence shows the packet was dishonest, too generous, or missing an honest reviewer-readable benchmark.

---

## 17. Checkpointed — Bandit overmap/local pressure rewrite packet v0

**Status:** CHECKPOINTED / DONE FOR NOW

Josef explicitly greenlit this as the bounded follow-up after directional light, and it is now honestly landed on the current tree.
The packet stayed on the current playback seam.
It proved pressure rewrite by scenario drift instead of slipping back into one-turn legality theater.

Current honest state:
- the authoritative contract lives at `doc/bandit-overmap-local-pressure-rewrite-packet-v0-2026-04-21.md`
- `src/bandit_playback.{h,cpp}` now carries the named `generated_local_loss_rewrites_corridor_to_withdrawal` and `generated_local_loss_reroutes_to_safer_detour` scenarios on the current playback seam
- the same file now also exposes `run_overmap_local_pressure_rewrite_proof_packet()` plus `render_overmap_local_pressure_rewrite_proof_packet( const proof_packet_result &result )`, so the reviewer-readable packet is first-class output instead of debugger folklore
- deterministic coverage in `tests/bandit_playback_test.cpp` now proves the key bounded distinctions honestly: a plausible corridor stalk can collapse to `hold / chill` once a confirmed local loss makes the tile too hot, a parallel intercept case can reroute onto a safer detour instead of homing forever on stale pressure, and the long horizon stays off the burned route instead of regrowing it by habit
- narrow deterministic validation passed on the current tree via `make -j4 tests`, `./tests/cata_test "[bandit][playback]"`, and `./tests/cata_test "[bandit]"`

Keep this lane checkpointed closed unless later evidence shows the packet was dishonest, too sticky, or not reviewer-readable enough to trust.

---

## 18. Checkpointed — Bandit weather concealment refinement packet v0

**Status:** CHECKPOINTED / DONE FOR NOW

Josef explicitly greenlit this as the bounded weather follow-up, and it now lands honestly on the current tree without reopening the whole visibility opera.
It stayed small.
It did not turn weather into a full simulation vanity project.

Current honest state:
- the authoritative contract lives at `doc/bandit-weather-concealment-refinement-packet-v0-2026-04-21.md`
- `src/bandit_mark_generation.{h,cpp}` now carries one bounded weather-refinement packet on the current smoke/light seam, with windy smoke losing source precision and confidence instead of taking only a token range haircut, plus explicit `portal_storm` handling for smoke and light
- reviewer-readable smoke/light summaries now expose the weather read directly, including fuzzed, reduced, blocked, displaced/corridor-ish, and preserved bright-light cases instead of hiding the interpretation in debugger soup
- `src/bandit_playback.cpp` now carries explicit windy-smoke and portal-storm exposed-light scenarios on the current generated-mark seam, and `tests/bandit_playback_test.cpp` keeps the broader reference suite count aligned with those additions instead of pretending the suite froze in amber
- deterministic coverage in `tests/bandit_mark_generation_test.cpp` and `tests/bandit_playback_test.cpp` now proves the bounded weather distinctions honestly: windy smoke stays scoutable but fuzzier, portal-storm smoke is harder to localize, exposed bright portal-storm light can stay legible while sheltered ordinary light stays bounded, and rain remains an explicit reducer
- narrow deterministic validation passed on the current tree via `make -j4 tests`, `./tests/cata_test "[bandit][marks]"`, `./tests/cata_test "[bandit][playback]"`, and `./tests/cata_test "[bandit]"`

Keep this lane checkpointed closed unless later evidence shows the weather packet was dishonest, too generous, too timid, or not reviewer-readable enough to trust.

---

## 19. Active greenlit — Bandit overmap benchmark suite packet v0

**Status:** ACTIVE / GREENLIT

Josef explicitly redirected the next lane away from z-level and toward the test burden itself.
This packet should turn the current bandit proof staircase into one coherent scenario family with easy-to-read benchmark hooks.

Current contract:
- the authoritative contract lives at `doc/bandit-overmap-benchmark-suite-packet-v0-2026-04-21.md`
- give every required scenario a clear `100`-turn benchmark packet that is easy to read and easy to fail
- keep or add `500`-turn carry-through checks where the long horizon is the honest proof burden
- expose richer timing and cadence metrics where they matter, for example first non-idle turn, first scout departure, first arrival, revisit count, and route-flip / back-and-forth count by turn `500`
- include the explicit empty-frontier scenario, where a camp with nothing useful nearby should venture out and increase frontier visibility through bounded explore instead of sitting idle forever
- keep z-level work ungreenlit for now; this lane is the proof-suite packet first
- if a scenario misses its benchmark, treat that as a routing-logic problem to fix rather than a reason to mush the benchmark into passing prose

Keep this item active greenlit unless later canon finds a contradiction in the suite contract or the implementation surfaces a real blocker.

---

## 20. Checkpointed — Bandit bounded scout/explore seam v0

**Status:** CHECKPOINTED / DONE FOR NOW

Josef explicitly re-enabled narrow bandit throughput, and this bounded slice is now honestly landed.
It stayed small.
It did not turn blocked routes into accidental random wandering.

Current honest state:
- the authoritative contract lives at `doc/bandit-bounded-scout-explore-seam-v0-2026-04-21.md`
- `src/bandit_dry_run.{h,cpp}` now carries one explicit bounded scout/explore option on the current evaluator seam via `camp_input`, instead of minting fake wandering from failed routes
- unreachable leads still fail closed as `no_path`, and deterministic coverage in `tests/bandit_dry_run_test.cpp` proves the key bounded distinctions honestly: explicit explore can beat `hold / chill`, blocked routes do not auto-create explore without the explicit greenlight, and strong real reachable leads still stay ahead when they score better
- `src/bandit_playback.cpp` now carries the named `bounded_explore_frontier_tripwire` scenario packet with explicit goals and tuning metrics, and `tests/bandit_playback_test.cpp` proves the same rule on the current scenario seam
- reviewer-readable dry-run output says plainly that the explore packet is explicit map uncovering and not accidental random wandering

This lane is checkpointed closed.
The next throughput pass should choose the next bounded parked-chain promotion instead of widening scope by accident.

---

## 21. Parked concept chain — Bandit overmap AI

**Status:** PARKED / COHERENT SUBSTRATE

Josef wanted the larger bandit / overmap-threat idea developed as a parked concept chain first, then re-evaluated for greenlight only after the concept packet was coherent enough as a whole.
That coherence audit now passes for four narrow promotions so far: the smoke-specific visibility -> mark bridge is checkpointed above, the light-specific bridge is checkpointed above, the human / route bridge is checkpointed above, and the repeated site-centered reinforcement bridge is checkpointed above too.
Do not quietly treat the rest of the parked notes as broad implementation approval just because several bounded slices were promoted.
The now-checkpointed formalization follow-through above was greenlit only for doc/spec cleanup inside this chain, and the broader concept still stays parked outside the explicit promoted v0 slices above plus the now-checkpointed first-500-turn proof packet.

Current parked-chain anchor:
- the broad synthesis paper lives at `doc/bandit-overmap-ai-concept-2026-04-19.md`

Current parked sub-items:
- overmap mark-generation and heatmap model v1 at `doc/bandit-mark-generation-and-heatmap-model-2026-04-19.md`
- bidirectional overmap-to-bubble handoff seam v1 at `doc/bandit-overmap-to-bubble-handoff-seam-2026-04-19.md`
- player/basecamp visibility and concealment v1 at `doc/bandit-player-basecamp-visibility-and-concealment-2026-04-19.md`

Promoted out of the parked chain into explicit canon lanes:
- active now: overmap benchmark suite packet v0 at `doc/bandit-overmap-benchmark-suite-packet-v0-2026-04-21.md`
- checkpointed now: weather concealment refinement packet v0 at `doc/bandit-weather-concealment-refinement-packet-v0-2026-04-21.md`
- checkpointed now: overmap/local pressure rewrite packet v0 at `doc/bandit-overmap-local-pressure-rewrite-packet-v0-2026-04-21.md`
- checkpointed now: long-range directional light proof packet v0 at `doc/bandit-long-range-directional-light-proof-packet-v0-2026-04-21.md`
- checkpointed now: bounded scout/explore seam v0 at `doc/bandit-bounded-scout-explore-seam-v0-2026-04-21.md`
- checkpointed now: concealment seam v0 at `doc/bandit-concealment-seam-v0-2026-04-21.md`
- checkpointed now: scoring refinement seam v0 at `doc/bandit-scoring-refinement-seam-v0-2026-04-21.md`
- checkpointed now: moving-bounty memory seam v0 at `doc/bandit-moving-bounty-memory-seam-v0-2026-04-21.md`
- checkpointed earlier: smoke, light, human / route, repeated-site reinforcement, and the first honest 500-turn proof

Supporting recon note for the visibility item:
- physical-systems recon at `doc/bandit-visibility-physical-systems-recon-2026-04-19.md`

What the current parked sub-items should do:
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
- resolved note-2 extortion footing: when bandits directly intercept player/basecamp actors in a player-present scene and the context still reads like a shakedown rather than a hot convoy hit, the first interaction may open in an explicit chat window with a hard choice to pay or fight instead of forcing pure bark-only negotiation or instant murder
- resolved note-2 payment-pool footing: the toll should draw from the honest local inventory pool for that scene, for example basecamp-side interception may expose player inventory plus nearby NPC and basecamp stock, while off-base pounces should only expose what the player, companions, and current vehicle actually carry
- resolved note-2 repo footing: existing overmap-NPC persistence, travel, and companion plumbing is valid substrate for following outbound groups and route interception, but the current need-driven wandering policy is not the finished hostile model
- resolved no-path exploration footing: unreachable jobs should still fail closed for that dispatch pass, but if a camp honestly chooses to uncover more map it may do so only through a bounded scout/explore outing with leash and return discipline instead of inheriting engine random-goal nonsense by accident
- resolved note-3 identity footing: persistent overmap group continuity is mandatory, with only a small anchored-individual slice surviving handoffs directly while the remaining membership stays fungible
- resolved note-3 goal-sustainability footing: smoke/mark destinations are provisional mission leads, not sacred tile commitments, and local observations may continue, divert, shadow, or abort the current goal
- resolved note-3 scope footing: v1 should cap anchored continuity at roughly 1-3 individuals rather than forcing full per-bandit persistence everywhere
- resolved note-4 city-opportunism footing: cities may support one-off or occasional opportunistic bandit action under zombie chaos, but not persistent farming, because structural bounty depletes while threat remains high
- resolved note-4 zombie-model footing: zombies currently matter as threat and target-coherence pressure only, not as a separately simulated bandit-versus-zombie tactical system
- resolved note-4 vehicle-ambush footing: if the player is driving a car or the scene reads like a convoy/travel-group hit, bandits should usually skip polite toll talk and lean straight into violent interception because that context reads as a hot attack window, not a calm shakedown
- resolved note-4 sticky-threat footing: if a group leaves due to threat, the area should keep a sticky scary mark that is only meaningfully reevaluated on close revisit or when some other attraction pulls the group back nearby
- resolved note-5 independence footing: v1 camps should behave as mostly independent actors with their own maps, bounty reads, and local pressures rather than a coordinated coalition strategy layer, so overlap may happen by accident but routine dogpile convergence should not be the norm
- resolved note-5 other-camps footing: other bandit camps should usually read as threat-bearing spots on a camp's own map, broadly more like hostile pressure sources than allies; occasional same-target opportunism may still happen indirectly when two camps both read the same opening, but not through shared planning
- resolved note-5 renegotiation footing: if bandits kill a basecamp NPC or otherwise materially reduce local defender threat in a player-present pressure scene, a later toll pass may reopen at a higher demanded percentage rather than pretending the old bargaining position still holds
- resolved note-5 anti-dogpile footing: territoriality, distance burden, depletion, sticky threat, and fresh active-pressure penalties should suppress repeated multi-camp pile-ons against the same region
- resolved note-5 scope footing: deliberate alliances, campaign planning, and explicit multi-camp coordination are later-layer strategy material, not part of the current parked v1 shape
- resolved testing-priority footing: before reopening hazy handoff elaboration as a planning focus, prefer a complete multi-turn overmap AI scenario packet with per-scenario goals and tuning metrics so later balancing can ask whether behavior is actually rad, cool, and fun instead of merely mathematically legal
- resolved testing-bar footing: for the remaining parked-chain proof packets, single-turn unit checks are not enough by themselves; the honest bar now includes real overmap-side multi-turn benchmark scenarios, up to `500` turns where needed

The intended parked-chain order for now is:
1. broad concept vessel
2. deterministic bounty/threat scoring
3. overmap mark-generation and heatmap model
4. bidirectional overmap-to-bubble handoff seam
5. player/basecamp visibility and concealment, informed by the physical-systems recon note
6. promotion audit, now passed narrowly for smoke first, light second, human / route third, repeated-site fourth, then concealment, scoring refinement, moving-bounty memory, bounded scout/explore, long-range directional light, overmap/local pressure rewrite, weather concealment refinement, and now the active overmap benchmark suite packet, with any later promotions requiring the same bounded review instead of more disconnected feeder docs

The broad anchor doc has now been rewritten into the synthesis paper for the parked chain.
If the packet is revisited later, the next planning discussion should be about the next bounded promotion or a real contradiction in the current packet, not about spawning more disconnected feeder docs by default.

---

## 22. Documentation discipline

If the structure starts bloating again, apply this rule:
- `Plan.md` should be readable in a minute
- `TODO.md` should show only the current execution queue
- `TESTING.md` should show only current policy, latest relevant evidence, and pending probes
- `COMMIT_POLICY.md` should stop the dirty tree from becoming a lifestyle

If a sentence exists only to remember that something used to be true, it probably belongs in git history, not here.
