# SUCCESS

Exit criteria ledger for roadmap items.

Use this file so completion is explicit instead of vibes-based.

## Rules

- Every real roadmap item in `Plan.md` should have a matching success state here (or an equally explicit inline auxiliary).
- When a success-state condition is reached, **cross it off here instead of deleting it immediately**.
- When all conditions for a roadmap item are crossed off, that roadmap item is **DONE** and can be removed from `Plan.md`.
- Josef self-testing is **never** a blocker in this file.
- If Josef-specific tests or checks are useful, write them down as non-blocking notes so Josef can do them later from his own testing list.
- This file is a completion ledger, not a changelog.
- Older closed sections may be compacted: retain the success-state bullets and canonical aux-doc reference, but move closure narrative to `doc/*.md`, `TESTING.md` snapshots, and git history.

---

## GitHub Actions CI recovery + checkpoint packet v0

Status: CLOSED / CHECKPOINTED

Success state:
- [x] Current `dev` GitHub Actions are no longer red for code-caused C-AOL failures, or every remaining red check is explicitly classified with a bounded non-code cause and next owner.
- [x] The C++17/warnings-as-errors failures are fixed: no designated-initializer/missing-field failures in `tests/faction_camp_test.cpp`, no missing-declaration family in `src/basecamp.cpp` / `src/bandit_playback.cpp`.
- [x] Windows build failure is either green or reduced to a named, evidence-backed workflow/runner/package blocker.
- [x] CodeQL is green or its remaining failure is classified as upload/config/external rather than silently sharing the same source compile failure.
- [x] A lightweight CI checkpoint/linking rule exists so future reviewable Andi commits name changed file class, relevant local gate, Actions link when available, and remaining red-check classification.
- [x] `Plan.md`, `TODO.md`, `SUCCESS.md`, and `TESTING.md` match the final state.


Compact reference:
- Canonical contract lives at `doc/github-actions-ci-recovery-checkpoint-packet-v0-2026-04-25.md`.


---

## GitHub normal-download release packet v0

Status: GREENLIT / HELD BEHIND DEBUG-CORRECTION STACK

Success state:
- [ ] A new public GitHub release exists on `josihosi/Cataclysm-AOL` with a deliberate tag/version and clear release notes.
- [ ] The release assets match the stated platform support instead of implying broken platforms work.
- [ ] The release source commit and relevant Actions state are linked from canon/testing notes.
- [ ] Josef has a normal GitHub Releases URL he can download from.
- [ ] Any withheld/broken platform is plainly marked with the evidence-backed blocker.

Notes:
- Canonical contract lives at `doc/github-normal-download-release-packet-v0-2026-04-25.md`.
- Current latest stable release observed before packaging: `v0.2.0` / `Cataclysm - Arsenic and Old Lace v0.2.0`.
- This was queued behind CI recovery and became active after `c5ff712e01` went green across General, Windows, CodeQL, IWYU, and Clang-tidy. It is now held behind Josef's 2026-04-26 debug-correction stack so Andi can remove hollow code/test-to-game gaps before release publishing.

---

## Test-vs-game implementation audit report packet v0

Status: CLOSED / CHECKPOINTED

Success state:
- [x] A concrete report exists at `doc/test-vs-game-implementation-audit-report-2026-04-26.md` or an explicitly equivalent path.
- [x] The report covers the highest-risk bandit AI / camp lanes, including `tests/bandit_mark_generation_test.cpp`, `tests/bandit_playback_test.cpp`, `tests/bandit_live_world_test.cpp`, and the live dispatch path through `src/do_turn.cpp` / `src/bandit_live_world.cpp`.
- [x] The report separately classifies smoke, light, weather, horde attraction, site bootstrap, dispatch, local handoff, and scout behavior by evidence class.
- [x] Each audited pass condition says whether the tested logic is produced/consumed by live gameplay, deterministic playback only, harness setup only, or currently hollow/missing.
- [x] Every hollow/missing bridge is assigned to one of the greenlit packages or marked as a new follow-up if it does not fit.
- [x] `TESTING.md` gets a compact update summarizing the audit result and preserving the rule that tests cannot impersonate live implementation.
- [x] The report names the first implementation package Andi should execute next after the audit.


Compact reference:
- Canonical contract lives at `doc/test-vs-game-implementation-audit-report-packet-v0-2026-04-26.md`.
- Closed report lives at `doc/test-vs-game-implementation-audit-report-2026-04-26.md`.


---

## Bandit live signal + site bootstrap correction v0

Status: JOSEF REVIEW / MOVE ON (PARTIAL, NOT CLOSED)

Success state:
- [x] Existing hostile overmap special families that should participate in live hostile-site logic can register abstract `bandit_live_world` site records without requiring the player to enter spawn/load range first.
- [ ] Abstract site records carry enough cheap roster/profile/headcount state to dispatch and later materialize concrete NPCs without save/perf blow-up. _(Partial: selected abstract overmap-special candidates now lazily create only the minimum profile-specific concrete scout roster, reconcile it through the owned-site ledger, and skip failed claims before overmap insertion; paired several-hour harness proof exercises this footing.)_
- [x] Materialized NPCs reconcile back to the same owned-site ledger, preserving exact-member writeback behavior when concrete members exist.
- [ ] Real or explicitly synthetic fire/smoke/light observations can create or refresh bounded live bandit marks/leads through the running game path, not only authored playback packets, and live signal generation respects weather/light conditions such as daylight, darkness, fog/mist, rain, wind, shelter/containment, source strength, persistence, and exposure. _(Partial: raw saved `fd_fire` / `fd_smoke` near-player fields now build live smoke packets using current weather, refresh owned-site marks, and repaired reader run `.userdata/dev-harness/harness_runs/20260427_014408/` proves initial `matched_sites=1 refreshed_sites=1` before the several-hour wait plus later no-signal decay. This is map-field reader proof only. A bounded synthetic smoke source from nothing is allowed as `synthetic smoke-source/live-signal proof` if labeled as such; it may prove smoke-source live-signal behavior but not player fire-lighting. Full player-fire proof still requires the candidate action chain — deployed brazier, wood beside it, firewood source on the wood, lighter, normal in-game lighting, visible fire/smoke, safe player placement, survival needs such as thirst/water handled, and then wait/log evidence. Live fire now also builds ordinary-light packets with current time/weather/exposure/source detail, but this run still has `matched_light_sites=0`, so a clean threshold-surviving light-branch proof remains open.)_
- [ ] The corrected range matrix is implemented or explicitly centralized: `40 OMT` overmap AI/system envelope; about `15 OMT` sustained smoke cap; ordinary bounty around `10 OMT`; confident threat around `6 OMT`; hard/searchlight threat around `8 OMT`; exceptional elevated light adapter-bounded inside the `40 OMT` envelope; movement remains `1-6 OMT/day` elapsed-time-earned travel credit. _(Partial: live dispatch now separates the `40 OMT` system envelope, `10 OMT` direct-player cap, and adapter-derived smoke cap.)_
- [x] The hard `distance <= 10` live-dispatch gate is removed or demoted so `10 OMT` ordinary visibility no longer impersonates the whole system range.
- [ ] Signal observation/decay cadence is separate from dispatch decision cadence, with event-driven creation and reviewer-readable maintenance. _(Partial: signal observation/mark refresh now runs on a `5_minutes` cadence while dispatch stays `30_minutes`; the current wait proof observes seeded smoke/fire fields decaying to no-signal, but decay policy still needs reviewer-readable design/coverage.)_
- [ ] Instrumentation distinguishes empty ownership, no signal packet, below-threshold signal, rejected-by-range, cadence skip, and hold/chill decisions. _(Partial: empty ownership, no signal packet, below-threshold smoke/light, smoke/light packet counts, matched smoke/light site counts, rejected-by-range, signal packet id, candidate distance, cap used, cadence skip, missing concrete member, and route failure are now logged; full hold/chill signal reasons remain open.)_
- [ ] Deterministic tests cover the range matrix, site bootstrap serialization, signal-specific caps, and candidate filtering/scoring split. _(Partial: site bootstrap serialization/reconciliation and live-signal mark ledger refresh/capping are covered; candidate scoring split and full range matrix remain open.)_
- [ ] At least one bounded smoke-source/live-signal proof shows a smoke source under live wait/time passage producing or refreshing a live bandit candidate/mark on a real owned-site path during/around a several-hour `|` wait, plus one no-signal control for the same setup. This may be either the full player-action fire chain or an explicitly labeled synthetic smoke-source shortcut. If the shortcut is used, park the full player-fire proof separately; the full proof remains: deploy a brazier, place wood next to it, put the firewood source on top of the wood, have/place a lighter, light the fire through normal in-game mechanics, visibly produce fire/smoke while the player stays safe and basic needs such as thirst/water are handled. _(Raw saved-field run `.userdata/dev-harness/harness_runs/20260427_014408/` is retained as map-field reader proof only.)_

Notes:
- Canonical contract lives at `doc/bandit-live-signal-site-bootstrap-correction-v0-2026-04-26.md`.
- This package supersedes the older 48/60 OMT starter lean with Josef's corrected `40 OMT` overmap AI/system envelope while preserving the anti-tripwire product law.
- First implementation slice registers abstract overmap-special hostile sites from existing loaded overmaps during the 30-minute overmap NPC cadence, serializes the abstract site footprint/headcount/profile, reconciles later concrete spawn claims into the same ledger, expands live dispatch candidate eligibility to the `40 OMT` system envelope, and adds reviewer-readable dispatch/bootstrap skip/reject logging.
- Current implementation also has a minimal raw-field `fd_fire` / `fd_smoke` reader hook, accepted live-signal mark memory refresh, candidate-local lazy materialization for abstract overmap-special sites, and repaired several-hour `|`-wait reader proof for initial smoke-site refresh followed by decay/no-signal. Schani/Josef moved the smoke/fire site-refresh proof to Josef review after attempt 5; do not keep rerunning this phase. Preserve the specific failed reviewed run `.userdata/dev-harness/harness_runs/20260427_013136/`: `signal_packet=yes`, but `matched_sites=0 refreshed_sites=0 rejected_by_signal_range=1`, then no-signal decay. Full player-fire proof, clean threshold-surviving light proof, signal decay design, and remaining hold/chill evidence remain open unless Josef explicitly reopens this packet.
- Keep `Basecamp medical consumable readiness v0` separate unless Josef explicitly bundles it.

---

## Bandit live-wiring audit + visible-light horde bridge correction v0

Status: CLOSED / MOVED DOWNSTREAM

Success state:
- [x] Docs/canon clearly distinguish deterministic proof/playback behavior from live game behavior for bandit light, smoke, horde-pressure, and handoff claims.
- [x] The live visible-light-to-horde bridge is implemented in source and deterministically tested.
- [x] If implemented, the bridge calls the real horde signal path through bounded thresholds and reviewer-readable reports.
- [x] At least one deterministic test proves bridge thresholds and one live/harness proof shows a real light/fire source can affect a real horde signal path.
- [x] Existing bandit test claims are audited enough that no closed packet says “game does X” when only an authored proof packet does X.


Compact reference:
- Canonical contract lives at `doc/bandit-live-wiring-audit-and-light-horde-bridge-correction-v0-2026-04-26.md`.


---

## Bandit local sight-avoid + scout return cadence packet v0

Status: CLOSED / MOVED DOWNSTREAM

Success state:
- [x] Stalking / hold-off bandits in the reality bubble can detect current or recent exposure to the player or nearby camp NPCs and attempt a bounded reposition toward cover or broken line of sight. _(Deterministic coverage plus live/harness proof `.userdata/dev-harness/harness_runs/20260427_061344/`: `bandit_live_world sight_avoid: exposed -> repositioned npc=4 ... reason=repositioning because exposed`.)
- [x] The sight-avoid behavior is local and heuristic: deterministic tests prove it does not use magical future-cone omniscience and does not teleport.
- [x] A scout outing has a finite live sortie window and can return home after watching long enough, instead of lingering indefinitely in local contact. _(Deterministic/live-wired source path plus live/harness return-home decision proof `.userdata/dev-harness/harness_runs/20260427_051117/`.)_
- [x] Returned scout state writes back through the owned-site memory path and can drive later re-evaluation without automatically conjuring a larger group. _(Deterministic writeback coverage plus live/harness follow-through `.userdata/dev-harness/harness_runs/20260427_054353/`: returned scout pressure refresh logged, active group/target/member ids and sortie clocks cleared, member `4` saved back on home footprint.)_
- [x] The single-scout current behavior remains explainable: `scout` is still one member unless a later job or escalated decision explicitly requires more.
- [x] Reviewer-readable output distinguishes `still stalking`, `repositioning because exposed`, `returning home`, and `re-dispatch/escalation decision`. _(Source/report strings cover the branches; live proof now covers returning-home/writeback and `sight_avoid: exposed -> repositioned`. Later redispatch tuning is not claimed beyond explicit owned-site re-evaluation.)_
- [x] At least one live/harness proof uses `bandit.live_world_nearby_camp_mcw` or an equivalent real owned-site scenario and confirms the same code path would apply to a normal discovered bandit camp, while separately naming the harness bias that places the camp nearby on purpose.


Compact reference:
- Canonical contract lives at `doc/bandit-local-sight-avoid-and-scout-return-cadence-packet-v0-2026-04-26.md`.


---

## Smart Zone Manager v1 Josef playtest corrections

Status: CLOSED / MOVED DOWNSTREAM

Success state:
- [x] Smart Zone Manager adds `LOOT_MANUALS` coverage on/near the Basecamp books cluster without removing ordinary `LOOT_BOOKS` coverage.
- [x] Gun-magazine coverage remains `LOOT_MAGAZINES`, preferably with an unambiguous user-facing label such as "Basecamp weapon magazines" if label text is touched.
- [x] Auto-eat and auto-drink coverage spans the full Basecamp storage zone, with `ignore_contents` explicitly false so Basecamp sorting still sees the covered items.
- [x] Deterministic tests assert the actual zone ids/types and the `ignore_contents == false` option, not just label text.
- [x] Save inspection confirms generated zones remain saved/reopenable and do not crash or corrupt the camp layout through zone-manager serialize/deserialize proof.


Compact reference:
- Canonical contract lives at `doc/smart-zone-manager-v1-josef-playtest-followup-2026-04-26.md`.


---

## Basecamp medical consumable readiness v0

Status: CLOSED / CHECKPOINTED

Success state:
- [x] Camp locker/service logic recognizes the practical bandage family, including at least `bandages`, `adhesive_bandages`, and medical gauze / obvious bandage-like wound-care stock when item definitions expose it cleanly, as bounded medical readiness supplies when stocking NPCs.
- [x] NPCs can pick up roughly 10 pieces/charges of bandage-family stock from the relevant Basecamp/locker storage path without hoarding all medical items.
- [x] Existing carried bandage-family stock is preserved through locker refresh and counts toward the reserve cap instead of being discarded as clutter.
- [x] Non-medical loadout behavior, ammo/magazine readiness, and clothing/armor selection remain unchanged.
- [x] Deterministic tests cover fresh pickup, carried-item preservation, cap/anti-hoarding behavior, and a negative case for unrelated drugs/items.
- [x] Live/harness proof is not required for this first slice; deterministic camp/locker tests exercise the actual service path and the focused `[camp][locker]` regression pass covers readiness behavior.


Compact reference:
- Canonical contract lives at `doc/basecamp-medical-consumable-readiness-v0-2026-04-26.md`.


---

## Basecamp locker armor ranking + blocker removal packet v0

Status: CLOSED / CHECKPOINTED

Success state:
- [x] A generic helper scores candidate protective/full-body gear against currently worn blockers using body-part priority, protection/coverage, encumbrance, condition, and active locker policy.
- [x] The helper is not item-id-specific and does not special-case `RM13 combat armor`.
- [x] When a candidate is clearly superior, the locker path removes/drops the blocking worn items needed to equip it.
- [x] When a candidate is not clearly superior or cannot be equipped, the locker path stops retrying the same failed swap and does not produce visible repeated wear spam.
- [x] Existing superior-full-body and ballistic-maintenance tests are reused/extended, including positive and negative cases.
- [x] Tests prove a clearly superior full-body/protective suit can displace worse blockers, while stronger current ballistic armor is preserved against worse candidates.
- [x] At least one targeted regression covers the original symptom shape without depending on the exact RM13 item ID as the only proof.

Notes:
- Canonical contract lives at `doc/basecamp-locker-armor-ranking-blocker-removal-packet-v0-2026-04-26.md`.
- Closure evidence: `make -j4 obj/basecamp.o tests/faction_camp_test.o tests LINTJSON=0 ASTYLE=0`, focused armor tests, `./tests/cata_test "[camp][locker]"` (2050 assertions in 70 test cases), and `git diff --check` passed.
- Josef explicitly said this must not be RM13-specific. Use a metric, not a charm against one cursed item.

---

## Basecamp job spam debounce + locker/patrol exceptions packet v0

Status: GREENLIT / QUEUED FOLLOW-UP

Success state:
- [ ] Repeated Basecamp completion/missing-tool/no-progress messages are debounced by stable cause so they do not flood the visible log.
- [ ] First occurrence and changed state still produce a visible/reportable message.
- [ ] Locker-zone work has a typed exception path: real locker gear/readiness failures remain visible once with reason, while repeats are compressed.
- [ ] Patrol-zone work has a typed exception path: real assignment/interruption/reserve/backfill changes remain visible once with reason, while repeats are compressed.
- [ ] The debounce state does not survive in a way that hides messages forever after save/load or unrelated job changes.
- [ ] Deterministic tests cover ordinary repeated spam, changed-state reset, locker exception, patrol exception, and a negative case showing unrelated important messages are not swallowed.
- [ ] If practical, a harness/log proof shows the old spam shape is reduced without losing one meaningful locker/patrol message.

Notes:
- Canonical contract lives at `doc/basecamp-job-spam-debounce-exceptions-packet-v0-2026-04-26.md`.

---

## Bandit overmap/local handoff interaction packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded overmap/local handoff interaction packet exists on the current bandit playback / proof seam.
- [x] Deterministic proof shows overmap intent entering local play as posture rather than exact-square puppetry or stale route puppetry.
- [x] Deterministic proof shows local engagement, obvious danger, or contact loss can honestly rewrite, cool, or drop the prior posture instead of preserving immortal certainty.
- [x] At least one player-present context beyond a static toy camp is covered, for example basecamp pressure, follower travel, or vehicle / convoy interception.
- [x] Reviewer-readable output exposes the entry posture, any local rewrite, and the returned abstract-state change clearly enough to debug the seam without guessing from side effects.
- [x] The slice stays bounded: no full local combat AI rewrite, no coalition strategy layer, no broad visibility rewrite, and no magical omniscience by the back door.


Compact reference:
- Canonical contract lives at `doc/bandit-overmap-local-handoff-interaction-packet-v0-2026-04-21.md`.


---

## Bandit elevated-light and z-level visibility packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded elevated-light and z-level visibility packet exists on the current bandit playback / proof seam.
- [x] Deterministic proof shows nearby cross-z visibility behaves sensibly instead of collapsing into floor-bound amnesia.
- [x] Deterministic proof shows elevated exposed light can stay legible or actionable farther than ordinary hidden light under the right conditions without turning into magical global sight.
- [x] A flagship exposed-high-fire scenario, for example a radio-tower fire in a dead dark world, proves genuinely long-range visibility instead of timid toy-local range.
- [x] Matching deterministic scenarios prove the same meaningful elevated light can carry abstract zombie-horde pressure too instead of living in private bandit-only theater; this is **not** live horde attraction until the live light-to-horde bridge exists.
- [x] Deterministic proof shows smoke does **not** gain magical extra general reach merely from floor changes.
- [x] Reviewer-readable output exposes the visibility read and benchmark outcomes clearly enough that later playtesting can argue about tuning instead of first principles.
- [x] The slice stays bounded: no broad world visibility rewrite, no handoff redesign smuggled into the same packet, and no full zombie tactical sim.


Compact reference:
- Canonical contract lives at `doc/bandit-elevated-light-and-z-level-visibility-packet-v0-2026-04-21.md`.


---

## Bandit repeated-site revisit behavior packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded repeated-site follow-through exists on the current playback / evaluator footing, producing one more deliberate revisit / cautious-watch posture than plain early scout bookkeeping.
- [x] Deterministic coverage proves repeated same-site corroboration still does **not** unlock free `scavenge`, `steal`, or `raid` truth, and does not become immortal pressure.
- [x] Scenario `repeated_site_interest_stays_bounded` exposes the benchmark-facing long-horizon metrics reviewer-cleanly: `site_visit_count_500`, `site_revisit_count_500`, `cooldown_turn`, and `endless_pressure_flag`.
- [x] The honest `500`-turn proof shows the strengthened site cooling back out instead of regrowing forever.
- [x] The slice stays narrow: no site-type-sensitive branching, no settlement taxonomy pass, no broad visibility rewrite, and no z-level smuggling.


Compact reference:
- Canonical contract lives at `doc/bandit-repeated-site-revisit-behavior-packet-v0-2026-04-21.md`.


---

## Bandit overmap benchmark suite packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One complete named overmap benchmark-suite packet exists on the current bandit playback / proof seam.
- [x] Every required scenario carries a clear `100`-turn benchmark packet that is easy to read and easy to fail.
- [x] The scenarios that honestly need the longer horizon also carry `500`-turn carry-through checks instead of pretending `100` turns proves everything.
- [x] Reviewer-readable output explains why each scenario passed or failed, including timing / cadence / revisit counters where they matter, such as first non-idle turn, first actionable turn, first scout departure, first arrival, revisit count, and route-flip / back-and-forth count by turn `500`.
- [x] The explicit empty-frontier scenario proves that a camp with nothing useful nearby ventures out and increases frontier visibility through bounded scout/explore behavior instead of sitting forever, with the packet plainly reporting first scout, first arrival, visit/revisit totals, and obvious reversals.
- [x] The packet is reviewer-readable enough that Schani or Josef can answer plainly whether it is leiwand, actually fun, alive on the map, and showing real emergent activity rather than inert legal-but-boring behavior.
- [x] The slice stays bounded: no z-level implementation, no broad architecture rewrite, no vague benchmark theater, and no hand-waved passes when routing logic is still wrong.


Compact reference:
- Canonical contract lives at `doc/bandit-overmap-benchmark-suite-packet-v0-2026-04-21.md`.


---

## Bandit long-range directional light proof packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded long-range directional-light proof packet exists on the current bandit scenario / playback seam.
- [x] Deterministic multi-turn proof up to `500` turns shows the hidden-side leakage case stays non-actionable while the visible-side leakage case becomes actionable under the same broader footing.
- [x] The matching deterministic zombie-horde corridor variant proves the same light can carry abstract horde pressure too instead of existing in isolated bandit-only theater; this is **not** live horde attraction until the live light-to-horde bridge exists.
- [x] Each scenario carries explicit goals and tuning metrics, and reviewer-readable output shows whether those benchmarks were met.
- [x] The slice stays bounded: no z-level expansion, no broad light-system rewrite, no handoff redesign, and no fresh world-sim jump.


Compact reference:
- Canonical contract lives at `doc/bandit-long-range-directional-light-proof-packet-v0-2026-04-21.md`.


---

## Bandit overmap/local pressure rewrite packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded overmap/local pressure-rewrite proof packet exists on the current bandit scenario / playback footing.
- [x] Deterministic multi-turn proof shows a stalking or intercept posture can honestly cool, retreat, or reroute after local reality makes the original posture too dangerous.
- [x] Reviewer-readable output shows the pressure rewrite clearly enough to explain why the stale pursuit no longer holds.
- [x] Each scenario carries explicit goals plus scenario-specific benchmark hooks, and the later locked benchmark outcomes stay visible on the same report path.
- [x] The slice stays bounded: no broad handoff redesign, no tactical local combat AI expansion, and no fresh world-sim jump.


Compact reference:
- Canonical contract lives at `doc/bandit-overmap-local-pressure-rewrite-packet-v0-2026-04-21.md`.


---

## Bandit weather concealment refinement packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded weather-refinement packet exists on the current smoke/light mark-generation footing.
- [x] Deterministic coverage proves wind meaningfully fuzzes or de-precises smoke output instead of acting only as a token penalty.
- [x] Deterministic coverage proves portal-storm weather is handled explicitly for smoke and light instead of falling through as an unmodeled special case.
- [x] Reviewer-readable output explains how weather changed clue quality, for example reduced range, fuzzier origin, displaced/corridor-ish smoke read, or preserved bright-light legibility under dark storm conditions.
- [x] The slice stays bounded: no full plume physics, no global smoke sim, no sound-law rewrite, no z-level packet, and no broad visibility architecture rework.


Compact reference:
- Canonical contract lives at `doc/bandit-weather-concealment-refinement-packet-v0-2026-04-21.md`.


---

## Bandit bounded scout/explore seam v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One explicit bounded scout/explore option exists on the current dry-run evaluator seam, for map uncovering only.
- [x] Unreachable jobs still fail closed and do not auto-mint explore as a consolation prize.
- [x] Deterministic coverage proves the key bounded distinctions honestly: explicit explore can beat `hold / chill`, blocked routes do not create explore without explicit greenlight, and strong real reachable leads still outrank exploratory wandering.
- [x] Reviewer-readable dry-run output says plainly that the outing is explicit map uncovering and not accidental random wandering.
- [x] One small playback/reference scenario packet proves the same rule on the current scenario seam, with explicit goals and tuning metrics.
- [x] The slice stays bounded: no coalition logic, no fresh visibility family, no broad pathfinding rewrite, and no handoff expansion.


Compact reference:
- Canonical contract lives at `doc/bandit-bounded-scout-explore-seam-v0-2026-04-21.md`.


---

## Bandit concealment seam v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded concealment adapter exists on the current light signal seam, weakening outward light legibility when exposure is poor instead of inventing a second fake visibility machine.
- [x] Deterministic coverage proves the key bounded distinctions honestly: daylight suppression, weather penalty, containment, and side-dependent leakage/suppression.
- [x] Reviewer-readable output shows why a light-born mark was reduced, blocked, or allowed instead of hiding the answer in debugger soup.
- [x] The slice stays bounded: no broad all-signals concealment rewrite, no new fog-sound law, no global smoke/world simulation, no tactical stealth doctrine, and no pursuit/handoff expansion.
- [x] If the concealment adapter starts looking computationally suspicious, the packet carries one small readable cost/probe angle instead of deferring perf truth to later folklore.


Compact reference:
- Canonical contract lives at `doc/bandit-concealment-seam-v0-2026-04-21.md`.


---

## Bandit scoring refinement seam v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded scoring-refinement adapter exists on the current bandit dry-run/evaluator seam, refining how existing camp ledger state plus existing marks become job choice.
- [x] The threat side first inspects and collapses usable existing threat/danger footing instead of inventing a fresh bespoke threat ontology.
- [x] Deterministic coverage proves the key opportunism split honestly: bandits avoid strong opponents, but pounce when zombie pressure or other distraction lowers effective target coherence.
- [x] Reviewer-readable output shows the refined choice breakdown clearly enough to explain why a target was avoided, deferred, or exploited.
- [x] The slice stays bounded: no new visibility signal family, no broad heatmap/memory rewrite, no tactical zombie simulation, no coalition strategy layer, and no fresh world-sim expansion.


Compact reference:
- Canonical contract lives at `doc/bandit-scoring-refinement-seam-v0-2026-04-21.md`.


---

## Bandit moving-bounty memory seam v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded moving-bounty memory object exists for live `actor` or `corridor` style bounty, while structural bounty stays on site state instead of gaining chase memory.
- [x] Structural sites keep only cheap state such as harvested / recently-checked / false-lead / sticky-threat footing, with no stalking logic glued onto them.
- [x] Deterministic coverage proves the key bounded pursuit split honestly: live moving prey can be stalked briefly, but the memory collapses cleanly on lost contact, split, bad recheck, rising threat, or leash expiry instead of retrying forever.
- [x] Reviewer-readable output shows whether a moving lead was refreshed, narrowed, or dropped instead of hiding the memory state in debugger soup.
- [x] The slice stays computationally cheap: no per-turn tracking, no path-history scrapbook, no per-NPC biography graph, no endless retry loop, and no broad memory-palace world model.


Compact reference:
- Canonical contract lives at `doc/bandit-moving-bounty-memory-seam-v0-2026-04-21.md`.


---

## Bandit first 500-turn playback proof v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] At least one honest deterministic 500-turn bandit playback packet exists on the current abstract seams.
- [x] Reviewer-readable report output exposes long-horizon checkpoints and winner drift cleanly enough to inspect the 500-turn answers without debugger soup.
- [x] Deterministic coverage proves the chosen long-horizon scenarios stay bounded, including cooldown, peel-off, and repeated-site reinforcement behavior that does not magically harden into free raid truth.
- [x] The proof reuses the current mark-generation / playback / evaluator seams instead of smuggling in a broader overmap simulator or persistence rewrite.
- [x] The slice stays bounded: no new visibility adapter family, no live-harness-first theater, and no broad AI architecture jump.


Compact reference:
- Canonical contract lives at `doc/bandit-first-500-turn-playback-proof-v0-2026-04-20.md`.


---

## Bandit repeated site activity reinforcement seam v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] A bounded repeated-site reinforcement adapter exists from mixed repeated smoke/light/traffic footing into modest site-mark confidence and bounty amplification.
- [x] Reinforced site marks feed the current bandit mark-generation / playback / evaluator seams reviewer-cleanly instead of staying hand-authored lore.
- [x] Deterministic coverage proves the key bounded distinctions: mixed repeated signals reinforce one site mark cleanly, weak repetition does not fake durable settlement truth, self-corroboration stays bounded, and strengthened site interest still does not unlock free extraction jobs.
- [x] Reviewer-readable report output exposes the reinforcement packet and resulting mark/lead path instead of hiding the bridge in debugger soup.
- [x] The slice stays bounded: no smoke/light/human-route rewrite, no broad concealment implementation, no settlement-signature mythology, and no first 500-turn proof smuggled in.


Compact reference:
- Canonical contract lives at `doc/bandit-repeated-site-activity-reinforcement-seam-v0-2026-04-20.md`.


---

## Bandit human / route visibility mark seam v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] A bounded human / route adapter exists from direct sightings and repeated route-shaped activity, or equivalent deterministic route-visibility packets, into coarse overmap-readable moving-carrier, corridor, or bounded site signal state.
- [x] Generated human / route marks or leads feed the current bandit mark-generation / playback / evaluator seams reviewer-cleanly instead of staying hand-authored lore.
- [x] Deterministic coverage proves the key bounded distinctions: direct sighting versus repeated corridor activity, moving-carrier attachment versus site inflation, self-camp routine suppression, and at least one corroborated shared-route refresh case.
- [x] Reviewer-readable report output exposes the route packet and resulting mark/lead path instead of hiding the bridge in debugger soup.
- [x] The slice stays bounded: no light/smoke rewrite, no broad concealment implementation, no settlement-signature mythology, and no first 500-turn proof smuggled in.


Compact reference:
- Canonical contract lives at `doc/bandit-human-route-visibility-mark-seam-v0-2026-04-20.md`.


---

## Bandit light visibility mark seam v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] A bounded light-specific adapter exists from current local light and directional-exposure footing, or equivalent deterministic light packets, into coarse overmap-readable light signal state.
- [x] Generated light marks or light-born leads feed the current bandit mark-generation / playback / evaluator seams reviewer-cleanly instead of staying hand-authored lore.
- [x] Deterministic coverage proves the key bounded distinctions: daylight suppression, contained versus exposed night light, ordinary occupancy light versus searchlight-like threat light, and at least one side-dependent leakage case.
- [x] Reviewer-readable report output exposes the light packet and resulting mark/lead path instead of hiding the bridge in debugger soup.
- [x] The slice stays bounded: no smoke rewrite, no broad concealment implementation, no sound/horde expansion, and no first 500-turn proof smuggled in.


Compact reference:
- Canonical contract lives at `doc/bandit-light-visibility-mark-seam-v0-2026-04-20.md`.


---

## Bandit mark-generation + heatmap seam v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] A bounded overmap-side mark ledger and broad bounty/threat heat-pressure seam exist for deterministic bandit inputs.
- [x] Deterministic coverage proves mark creation, refresh, selective cooling, and sticky confirmed threat on named reference cases.
- [x] The existing evaluator / playback footing can consume generated mark output reviewer-cleanly instead of relying only on hand-authored leads.
- [x] The slice stays bounded: no bubble handoff, no broad visibility adapter, and no full hostile-world simulation are smuggled in.


Compact reference:
- Canonical contract lives at `doc/bandit-mark-generation-heatmap-seam-v0-2026-04-20.md`.


---

## Bandit overmap-to-bubble pursuit handoff seam v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded pursuit / investigation handoff exists from abstract overmap group state into local play.
- [x] The return path preserves meaningful abstract consequences such as updated mark/threat knowledge, losses, panic, cargo, or retreat state instead of dropping them on the floor.
- [x] Entry payload and return packet stay explicit, small, and reviewer-readable.
- [x] The slice stays bounded: no full raid / ambush suite, no broad tactical AI rewrite, and no full per-bandit biography persistence are smuggled in.


Compact reference:
- Canonical contract lives at `doc/bandit-overmap-to-bubble-pursuit-handoff-seam-v0-2026-04-20.md`.


---

## Locker lag-threshold probe v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One honest threshold packet exists for the real `CAMP_LOCKER` service path.
- [x] The packet distinguishes top-level item pressure from worker-count pressure instead of flattening them together.
- [x] The result can name an approximate fine / suspicious / bad range, or honestly report that no clear threshold was found within the tested bound.
- [x] If the threshold looks bad, the packet ends with a small cheap-first guardrail recommendation order instead of architecture opera, and if it does not, the packet says so plainly.


Compact reference:
- Canonical contract lives at `doc/locker-lag-threshold-probe-v0-2026-04-20.md`.


---

## Bandit evaluator dry-run seam v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] A deterministic dry-run evaluator exists for controlled bandit camp inputs.
- [x] The evaluator always includes `hold / chill` and only emits outward candidates from real compatible leads or current hard state.
- [x] The first explanation surface shows leads considered, the full candidate board, per-candidate score inputs/final score, veto/soft-veto reasons, and winner versus `hold / chill`.
- [x] Narrow deterministic coverage exists for the first pure reasoning reference cases.
- [x] The slice stays bounded: no full autonomous bandit world behavior, no broad scenario playback suite, and no broad persistence architecture are smuggled in.


Compact reference:
- Canonical contract lives at `doc/bandit-evaluator-dry-run-seam-v0-2026-04-20.md`.


---

## Bandit scenario fixture + playback suite v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] Named deterministic bandit scenarios exist for the first reference cases.
- [x] The playback contract can inspect behavior at multiple checkpoints such as `tick 0`, `tick 5`, `tick 20`, and one longer horizon.
- [x] The scenario packet can answer whether camps stay idle, investigate smoke, stalk edges, peel off under pressure, or mis-upgrade whole regions from moving clues.
- [x] The suite stays bounded and does not turn into broad worldgen mutation or live-harness-first theater.


Compact reference:
- Canonical contract lives at `doc/bandit-scenario-fixture-playback-suite-v0-2026-04-20.md`.


---

## Bandit perf + persistence budget probe v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] Repeatable cost measurements exist for the named bandit scenarios.
- [x] Obvious evaluator churn signals such as candidate-count growth, repeated scoring/path checks, or similar waste are visible instead of hidden.
- [x] Save-size growth has an honest first estimate tied to the actually persisted bandit state shape.
- [x] The packet can say whether the current design looks cheap enough, suspicious, or clearly too bloated before broader rollout.


Compact reference:
- Canonical contract lives at `doc/bandit-perf-persistence-budget-probe-v0-2026-04-20.md`.


---

## Locker clutter / perf guardrail probe v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One honest measurement packet exists for the real `CAMP_LOCKER` service path across a first bounded clutter sweep such as `50 / 100 / 200 / 500 / 1000` top-level items.
- [x] The packet distinguishes realistic worker-count pressure from item-hoard pressure instead of pretending twenty-to-fifty camp workers are the main threat.
- [x] The packet answers whether loaded magazines and ordinary container shapes mostly behave like one top-level locker item or create meaningful nested-content pain.
- [x] The packet can end with a usable verdict: fine for now, watch this, or land a guardrail now, plus the first cheap mitigation order if needed.


Compact reference:
- Canonical contract lives at `doc/locker-clutter-perf-guardrail-probe-v0-2026-04-20.md`.


---

## Controlled locker / basecamp follow-through packet

Status: CHECKPOINTED / PACKAGE 5 DONE FOR NOW

Success state:
- [x] **Package 1, harness zone-manager save-path polish** is landed with screenshots/artifacts from the current McWilliams harness path.
- [x] **Package 2, basecamp toolcall routing fix** is landed or honestly blocked, and the right discriminator is separated from the bad location-only heuristic.
- [x] **Package 3, locker outfit engine hardening** is now landed for the current required closeout slice: the weird board/log leak stays re-proved live on the rebuilt current tiles binary, the in-game-log plus `llm_intent.log` observability helper is exercised on that same live probe path, and the required deterministic/service-parity canon is closed through the outer one-piece civilian-garment seam (`abaya`) without reopening open-ended clothing-case collection.
- [x] **Package 5, basecamp carried-item dump lane** is landed: ordinary carried junk gets dumped during locker dressing, the kept carried lane is intentionally limited to `bandages`, `ammo`, and `magazines`, and curated locker stock is not polluted by the dump behavior.
- [x] The queue stayed controlled while Package 5 ran, and the next slice can now move forward cleanly as Package 4 instead of broadening into unrelated lanes.



---

## Patrol Zone v1

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] A Zone Manager patrol zone exists and patrol squares are grouped by 4-way connected clusters.
- [x] Patrol exists as a real camp job with its own priority surface.
- [x] Deterministic planner coverage exists for the reference staffing/topology cases.
- [x] The active patrol roster is chosen at shift boundaries from NPCs with patrol priority > 0.
- [x] On-shift patrol is sticky against routine chores.
- [x] Urgent disruption can break patrol, and reserve backfill works without full-roster reshuffle.
- [x] On-map behavior distinguishes hold-positions vs fixed-loop patrol in the intended simple v1 way.
- [x] Proportional live proof is recorded with separate screen/tests/artifacts reporting.
- [x] The player-legibility bar is met: guard behavior, uncovered posts, connected-vs-disconnected behavior, and reserve/off-shift state are understandable enough to read in play.
- [x] The result stays explainable as simple v1 patrol rather than quietly turning into smart-zone-manager soup.



---

## Smart Zone Manager v1

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One explicit one-off smart-zoning action exists for Basecamp.
- [x] The v1 creates exactly one crafting niche, one food/drink niche, and one equipment niche.
- [x] Support placement exists for clothing, dirty, rotten, unsorted, and blanket/quilt-on-beds.
- [x] The corrected fire layout is implemented: fire tile = `SOURCE_FIREWOOD`, adjacent `splintered`, nearby wood.
- [x] Anchor selection is honest about the current shape: flag-first where the map exposes real signals, small explicit id allowlists where those signals are thin, and floor fallback instead of clever failure.
- [x] Existing sorting/subcategory machinery is reused by default, with only the deliberate v1 custom filters (`splintered`, `dirty`, `rotten`, `blanket`, `quilt`) kept as explicit exceptions.
- [x] Placement is deterministic and non-destructive by default.
- [x] Deterministic tests exist for anchor choice / zone choice / no-destructive-overwrite behavior.
- [x] Proportional live proof is recorded on the rebuilt current tiles binary.


Compact reference:
- Canonical contract lives at `doc/smart-zone-manager-v1-aux-plan-2026-04-06.md`.


---

## Locker-capable harness restaging

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] A fixture or restaging path exists that contains a real `CAMP_LOCKER` zone and suitable locker-state/NPC-state for `locker.weather_wait`.
- [x] `locker.weather_wait` is no longer blocked on missing fixture shape.
- [x] A fresh packaged `locker.weather_wait` run reports **screen** / **tests** / **artifacts** separately on the repaired fixture path.
- [x] The result is described reviewer-cleanly as harness/fixture work on existing locker behavior, not as premature delivery of the later chat/ambient feature lanes.



---

## Stabilization + harness runway

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] The currently claimed locker baseline is honest enough to build on: V1/V2 stay trusted, and the active V3 slice is either properly evidenced or explicitly demoted back to the honest current state.
- [x] At least one reliable harness-driven live probe path exists on the current binary/profile/save path without stale-binary ambiguity.
- [x] That harness path reports screen/tests/artifacts as separate evidence classes instead of flattening them into one vague verdict.
- [x] At least one high-value reusable playtest scenario is documented/packaged for Schani-assisted probing instead of re-invented manually each time.
- [x] The packaged harness path no longer treats the first `lastworld.json` flip as sufficient proof of post-load gameplay readiness.
- [x] `chat.nearby_npc_basic` records recipient / artifact proof instead of only reaching dialogue and freeform-input UI.
- [x] A second named scenario contract (`ambient.weird_item_reaction`) exists and is honest about its current footing: runnable on the shipped fixture, still lacking real reaction/artifact proof.
- [x] At least one reusable scenario-setup helper exists so repeated probes stop depending on debug-menu folklore.
- [x] A compact Josef-facing testing packet exists for the pre-holiday active-testing window.



---

## Locker Zone V2

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] Locker service can select up to two compatible magazines from locker supply when a managed ranged loadout needs them.
- [x] Locker service can reload supported ranged weapons from locker-zone ammo supply.
- [x] V2 ranged-readiness logic stays basic and deterministic instead of ballooning into V3 fashion/season nuance.
- [x] Deterministic coverage exists for the V2 ranged readiness / reload behavior.
- [x] Proportional runtime validation for V2 is recorded in `TESTING.md`.
- [x] Any Josef-specific follow-up checks are written down as non-blocking notes rather than treated as plan blockers.



---

## Locker Zone V1

Status: CHECKPOINTED / DONE FOR NOW



---

## Locker Zone V3

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] Seasonal dressing / winter-vs-summer wardrobe logic exists.
- [x] Weather-sensitive wardrobe choices (coats / blankets / shorts / similar) are handled deliberately rather than by V1/V2 shortcuts.
- [x] Per-NPC overrides / nuance exist without undoing the simpler V1/V2 deterministic spine.
- [x] Deterministic coverage exists for the V3 behavior that is actually implemented.
- [x] Proportional runtime validation for the currently implemented V3 behavior is recorded in `TESTING.md`.
- [x] Any Josef-specific follow-up checks are written down as non-blocking notes rather than treated as plan blockers.


Compact reference:
- Canonical contract lives at `doc/locker-zone-v3-reopen-packet-v0-2026-04-21.md`.


---

## Basecamp AI capability audit/readout packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded capability audit/readout packet exists for the current Basecamp AI surface.
- [x] The packet distinguishes player-facing spoken behaviors from internal structured actions/tokens instead of mushing them together.
- [x] The packet says plainly what board/job actions are actually supported now.
- [x] The packet says plainly whether any prompt-shaped interpretation layer still matters, or whether the current behavior is already mostly deterministic plumbing.
- [x] The packet is grounded in current code/tests/evidence strongly enough that later cleanup decisions do not rely on stale folklore.
- [x] The slice stays bounded and does not mutate into implementation work by accident.



---

## Live bandit + Basecamp playtesting feasibility probe v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded live playtesting feasibility packet exists for current bandits plus current Basecamp footing.
- [x] The packet says plainly whether live playtesting is already practical on the current tree.
- [x] The packet says plainly whether overmap-bandit setup/spawn control is currently available, narrowly restageable, or still missing.
- [x] The packet separates screen/tests/artifacts cleanly instead of flattening them into one soup verdict.
- [x] Any blocker is stated as a concrete missing setup/control path rather than vague playtesting hand-wringing.
- [x] The slice stays bounded and does not turn into open-ended live playtesting theater.


Compact reference:
- Canonical contract lives at `doc/live-bandit-basecamp-playtesting-feasibility-probe-v0-2026-04-21.md`.


---

## Live bandit + Basecamp playtest packaging/helper packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded helper or first-class harness scenario exists for current-build live bandit + Basecamp restage on real existing Basecamp footing.
- [x] The helper packages the already-proved named-bandit restage seam instead of relying on remembered debug-menu choreography alone.
- [x] One fresh live run proves the helper can reach the intended setup repeatably and leaves reviewer-readable artifacts.
- [x] The packet says plainly what remains manual or ugly, if anything, instead of laundering it into magic.
- [x] The slice stays bounded and does not widen into fresh mechanics, zoning-mechanics reopen, encounter/readability judgment, or another generic feasibility lap.


Compact reference:
- Canonical contract lives at `doc/live-bandit-basecamp-playtest-packaging-helper-packet-v0-2026-04-22.md`.


---

## Bandit + Basecamp first-pass encounter/readability packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded first-pass live encounter/readability packet exists for current bandit + Basecamp footing on top of the packaged helper path.
- [x] The packet says plainly what the first hostile encounter actually looks and feels like on screen, including whether the player can read what is happening without debug folklore.
- [x] The packet distinguishes readable gameplay pressure from leftover helper/debug awkwardness instead of mushing them together.
- [x] Any blocker is stated concretely, for example spawn weirdness, posture confusion, unreadable hostility, dead pacing, or Basecamp-context ambiguity.
- [x] The packet leaves reviewer-readable artifacts strong enough that Schani can make the next higher-level product read without reinventing the probe from memory.
- [x] The slice stays bounded and does not widen into fresh mechanics, broad tuning, or speculative feature expansion.


Compact reference:
- Canonical contract lives at `doc/bandit-basecamp-first-pass-encounter-readability-packet-v0-2026-04-22.md`.


---

## Bandit + Basecamp playtest kit packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] The current named-bandit helper path has honest repeatability evidence instead of one lucky run.
- [x] The artifact/report surface for that helper path is screen-first and reviewer-readable enough that later playtest discussion does not depend on folklore.
- [x] The helper path cleans up after itself well enough that repeated probes do not leave stale game windows or obvious session clutter behind.
- [x] One purpose-built playtest fixture/save pack exists with a fast reinstall/load path for current bandit + Basecamp probing.
- [x] That fixture/save pack exposes a small named scenario family suitable for Josef playtesting and debugging rather than one brittle operator-only restage ritual.
- [x] The packet says plainly what still remains ugly or manual instead of laundering it into magic.
- [x] The slice stays cohesive and bounded rather than mutating into a general harness/world-authoring rewrite.


Compact reference:
- Canonical contract lives at `doc/bandit-basecamp-playtest-kit-packet-v0-2026-04-22.md`.


---

## Bandit + Basecamp playtest kit packet v1

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] A small named family of rich prepared-base fixtures exists for current Basecamp-management and bandit-interplay probing.
- [x] The fixtures are meaningfully management-ready rather than empty camp shells.
- [x] The player-side debugging/readability footing includes both clairvoyance mutations.
- [x] The fixture method is honest and reusable: captured save for structure, live restage for variants.
- [x] The scenario family is reviewer-readable enough that Schani, Andi, and Josef can tell which fixture is for which kind of playtest without folklore.
- [x] The packet says plainly what still remains manual, brittle, or not yet fixture-backed.
- [x] The slice stays about rich prepared fixtures rather than sprawling into a generic world-authoring platform.


Compact reference:
- Canonical contract lives at `doc/bandit-basecamp-playtest-kit-packet-v1-2026-04-22.md`.


---

## Bandit live-world control + playtest restage packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One live saveable owner exists for the relevant current bandit spawn families tied to this lane instead of leaving them as disconnected static-content / mapgen islands.
- [x] The owner keeps explicit per-site and per-spawn-tile bandit headcounts plus membership strongly enough that later control and writeback are honest rather than guessed.
- [x] The live system can actually control or dispatch those spawned bandits through the real world path instead of leaving them as disconnected `place_npcs` islands with post-hoc bookkeeping or whole-camp outings.
- [x] Dispatch size is derived from the site's live remaining population strongly enough that site-backed camps keep a home presence and later losses/shrinkage reduce future outing size instead of snapping back to folklore counts.
- [x] A bounded nearby restage path can seed a controlled bandit camp about `10 OMT` away on current build.
- [x] That restage path supports two honest modes: reviewer probe/capture may clean up, while manual playtest handoff leaves the game/session running instead of auto-terminating it.
- [x] The setup exercises the real overmap/bubble handoff plus local writeback path rather than a fake private theater.
- [x] Local outcomes change later world behavior instead of leaving the live owner stale, including later site size / dispatch capacity after kills, losses, returns, or partial contact.
- [x] A reviewer-clean perf report exists on that nearby live setup, including baseline turn time, bandit-cadence turn time, spike ratio, and max turn cost.
- [x] At least one dirtier later-world disturbance proof exists on that same nearby owned setup beyond the calm return->re-dispatch path, such as loss/missing shrinkage, save/load disturbance, or player-disruption without stale-roster reset.
- [x] The slice stays bounded: no giant generic map-authoring empire, no full hostile-human rewrite, no fake harness-only integration, and no faction-grand-strategy detour.


Compact reference:
- Canonical contract lives at `doc/bandit-live-world-control-playtest-restage-packet-v0-2026-04-22.md`.


---

## Multi-site hostile owner scheduler packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] The live hostile-owner seam can run `2-3` simultaneous hostile sites on one world without collapsing them into one shared fake camp brain.
- [x] Each hostile site keeps its own anchor/home site, roster/headcount, active outing or contact state, remembered pressure/marks, and persisted writeback state.
- [x] Site-backed camps still keep a home presence while losses continuously shrink later outings instead of snapping back to folklore counts.
- [x] One site's losses, return state, or contact pressure do not silently rewrite another site's state.
- [x] Repeated same-target pressure can overlap occasionally without turning into routine multi-site dogpile coalition behavior.
- [x] Save/load stays honest for multiple hostile owners at once instead of only for the single easiest happy-path site.
- [x] The slice stays bounded: no faction grand strategy, no dozens-of-families explosion, and no magical shared omniscience.


Compact reference:
- Canonical contract lives at `doc/multi-site-hostile-owner-scheduler-packet-v0-2026-04-22.md`.


---

## Hostile site profile layer packet v0

Status: CLOSED / CHECKPOINTED

Success state:
- [x] One shared hostile owner/cadence/persistence substrate exists with explicit profile rules instead of hardcoding everything to current bandit-camp assumptions.
- [x] At minimum one camp-style profile and one smaller hostile-site profile can coexist on that substrate reviewer-cleanly.
- [x] Dispatch, threat posture, return-clock, and writeback differences are profile-driven and readable rather than site-kind spaghetti.
- [x] The multi-site scheduler can consume those profiles without regressing the already-honest bandit live-owner footing.
- [x] The packet stays bounded: no giant faction-AI framework, no singleton stalker implementation, and no broad diplomacy/social-horror widening.


Compact reference:
- Canonical contract lives at `doc/hostile-site-profile-layer-packet-v0-2026-04-22.md`.


---

## Cannibal camp attack-not-extort correction v0

Status: CHECKPOINTED / DONE FOR NOW.


Compact reference:
- Canonical contract lives at `doc/cannibal-camp-attack-not-extort-correction-v0-2026-04-24.md`.
- Canonical contract lives at `doc/cannibal-camp-first-hostile-profile-adopter-packet-v0-2026-04-22.md`.


---

## Bandit approach / stand-off / attack-gate packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One honest local approach / stand-off / attack-gate layer exists on top of the live bandit control seam.
- [x] Dispatched bandit groups can reviewer-readably choose among stalking, holding off, probing, opening a shakedown, attacking directly, or aborting.
- [x] The gate law is explicit enough that later packet work can answer `why did this become a shakedown instead of a fight` without folklore reconstruction.
- [x] Ordinary Basecamp/player pressure does not feel like instant psychic tile collapse because bandits can keep bounded stand-off behavior.
- [x] Convoy / vehicle / rolling-travel contexts are allowed to skip the polite shakedown posture when they honestly read as moving ambush opportunities on a real or harnessed travel seam.
- [x] The slice stays bounded: no pay-or-fight UI yet, no giant stealth doctrine, no radio/stalker widening, and no broad combat-AI rewrite.


Compact reference:
- Canonical contract lives at `doc/bandit-approach-stand-off-attack-gate-packet-v0-2026-04-22.md`.


---

## Bandit shakedown pay-or-fight surface packet v0

Status: CLOSED / CHECKPOINTED

Success state:
- [x] One honest player-present shakedown scene can bootstrap from the prior approach/gate packet instead of appearing as disconnected chat magic.
- [x] The initial interaction is clearly a robbery demand with a readable `pay` versus `fight` fork.
- [x] Whenever this shakedown surface is invoked, including any later reopened demand, fighting remains an explicit option rather than disappearing into one-way surrender theater.
- [x] The surrender surface uses an explicit bounded goods pool that matches scene context rather than magical remote inventory reach.
- [x] The demanded toll is painful enough to read like a real bandit shakedown instead of a decorative nuisance fee.
- [x] Paying can resolve the immediate scene without requiring perfect long-tail cargo simulation, because surrendered goods can collapse into bandit bounty/writeback honestly.
- [x] The slice stays bounded: no branching diplomacy opera, no fake debt economy, no magical remote inventory, and no unrelated convoy-combat rewrite.


Compact reference:
- Canonical contract lives at `doc/bandit-shakedown-pay-or-fight-surface-packet-v0-2026-04-22.md`.


---

## Bandit aftermath / renegotiation writeback packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One honest aftermath/writeback layer exists for player-present bandit shakedown outcomes.
- [x] Later bandit behavior can reflect scene results such as losses, wounds, loot haul, failed extraction, anger, or caution instead of resetting to folklore.
- [x] A materially weakened Basecamp defense, including a killed defender, can reopen the pressure once from a stronger bandit position with a higher demand when that is the honest read.
- [x] That harsher reopened demand still gives the player a fresh explicit `pay` versus `fight` reconsideration fork instead of hard-forcing only surrender or only combat.
- [x] The harsher reopened demand is explicit and reviewer-readable rather than hidden in vague score drift.
- [x] Bandit losses or panic can also cool or shrink later pressure, so the packet does not only ratchet cruelty upward.
- [x] The slice stays bounded: no infinite haggling loops, no giant diplomacy/reputation machinery, and no multi-camp retaliation grand strategy.


Compact reference:
- Canonical contract lives at `doc/bandit-aftermath-renegotiation-writeback-packet-v0-2026-04-22.md`.


---

## Bandit extortion-at-camp restage + handoff packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One named restage path can attract a real controlled bandit group toward Basecamp through the actual live owner/dispatch seam.
- [x] One reviewer probe/capture command and one manual handoff command exist for that same path.
- [x] The handoff path leaves the session alive at a genuinely useful extortion-at-camp moment instead of cleaning up before play starts.
- [x] The setup does not rely on fake debug-spawn theater or on moved-player/basecamp hacks that break current camp validity.
- [x] Reviewer-readable reports make it obvious which setup mode ran, which real controlled group was used, and whether the scene reached approach, stand-off, or shakedown state.
- [x] The slice stays bounded: no generic harness empire and no helper polish masquerading as the robbery chain itself.


Compact reference:
- Canonical contract lives at `doc/bandit-extortion-at-camp-restage-handoff-packet-v0-2026-04-22.md`.


---

## Bandit extortion playthrough audit + harness-skill packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One named audit/playthrough surface exists for the full Basecamp extortion chain rather than only for the first setup moment.
- [x] The packaged flow can cover first demand, fight branch, defender-loss reopen, raised-price second demand, and second `pay` versus `fight` reconsideration clearly enough for review.
- [x] Reports separate screen, tests, and artifacts/logs cleanly enough that later debugging does not devolve into folklore.
- [x] The relevant harness skill/docs are updated so another agent can discover the named paths and run them without archaeological guessing.
- [x] The slice stays bounded: no fake total-automation empire and no feature-redesign side quest hiding inside audit packaging.


Compact reference:
- Canonical contract lives at `doc/bandit-extortion-playthrough-audit-harness-skill-packet-v0-2026-04-22.md`.


---

## Bandit + Basecamp playtest kit packet v2

Status: FOLDED INTO LATER ACTIVE LANE / SUPPORTING ONLY

Notes:
- Canonical helper contract still lives at `doc/bandit-basecamp-playtest-kit-packet-v2-2026-04-22.md`.
- The useful open scenario-surgery / observability work from `v2` was carried forward into `Bandit live-world control + playtest restage packet v0` instead of being killed.
- `v2` is therefore no longer a standalone roadmap item; it survives only as supporting tooling in service of the newer active lane.

---

## Post-Locker-V1 Basecamp follow-through

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] The live `DEBUG_LLM_INTENT_LOG` board/job artifact packet is made legible enough to stand beside the deterministic router proof.
- [x] The deterministic Basecamp board/job work is pruned/packaged into a cleaner upstream-ready shape.
- [x] The richer structured board/prompt treatment is extended beyond `show_board` in a deliberate next slice.
- [x] Proportional validation for each finished sub-slice is recorded in `TESTING.md`.
- [x] Any Josef-specific follow-up checks are written down as non-blocking notes rather than treated as plan blockers.



---

## LLM-side board snapshot path

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] Routing proof exists on the real `handle_heard_camp_request` path.
- [x] Structured/internal `show_board` emits the richer handoff snapshot.
- [x] Spoken `show me the board` stays on the concise spoken bark path.
- [x] Deterministic evidence for that split is recorded.



---

## Organic bulletin-board speech polish

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] Bulletin-board / camp-job requests can be triggered through natural player-facing phrasing instead of exposed machine wording.
- [x] Ordinary spoken answers no longer expose `job=<id>` / `show_board` / `show_job` style routing tokens.
- [x] Internal routing/debug structure can still exist where needed without leaking into normal in-world speech.
- [x] The visible answer tone sounds rough, practical, and in-world, like poor survivors making it work for another day while the dead and worse roam outside.


Compact reference:
- Canonical contract lives at `doc/organic-bulletin-board-speech-2026-04-09.md`.


---

## Combat-oriented locker policy

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] Future locker behavior strongly supports sensible common guard/combat gear: gloves, belts, masks, holsters, and the usual practical clothing/loadout pieces.
- [x] A bulletin-board / locker-surface bulletproof toggle exists and meaningfully shifts outfit preference toward ballistic gear.
- [x] Ballistic vest and plate handling becomes explicit enough to replace damaged (`XX`) ballistic components sensibly.
- [x] Clearly superior full-body battle/protective suits are preferred when appropriate instead of being split into worse piecemeal junk.
- [x] Future deterministic tests lean more toward combat/guard outfit behavior and less toward endlessly widening exotic garment edge-case law.


Compact reference:
- Canonical contract lives at `doc/locker-combat-oriented-policy-2026-04-09.md`.


---

## Bandit concept formalization follow-through

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] A follow-through packet exists that turns the loose remaining bandit logic into three explicit doc slices: bounty source/harvesting/stockpile rules, cadence/distance/fallback rules, and cross-layer interactions/worked scenarios.
- [x] Those three slices are further decomposed into narrow single-question micro-items so Andi can freeze one law at a time instead of hiding several assumptions inside one paragraph.
- [x] The packet makes the no-passive-decay footing explicit: structural bounty changes via harvesting/exploitation, moving bounty via current activity and collection, threat via real recheck, and camp stockpile via explicit consumption instead of background decay math.
- [x] Each micro-item includes a clear question plus a concrete answer shape, and the packet as a whole includes starter numbers/tables, clear scope/non-goals, and enough worked examples that later implementation planning does not have to rediscover the control law from scratch.
- [x] The result remains conceptualization/backlog work only and does not silently greenlight bandit implementation.


Compact reference:
- Canonical contract lives at `doc/bandit-concept-formalization-followthrough-2026-04-19.md`.


---

## Plan status summary command

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] A user can request a plan readout and see compact numbered `active`, `greenlit`, and `parked` summaries derived from current `Plan.md` canon.
- [x] The greenlit readout preserves execution order, with active first and any bottom-of-stack entries simply appearing last in that numbered list.
- [x] If canon headings are contradictory or missing enough structure to classify cleanly, the command warns instead of inventing certainty.
- [x] The output stays short and Discord-friendly rather than dumping whole roadmap prose.


Compact reference:
- Canonical contract lives at `doc/plan-status-summary-command-2026-04-20.md`.


---

## Bandit smoke visibility mark seam v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] A bounded smoke-specific adapter exists from current fire/smoke/wind footing, or equivalent deterministic smoke packets, into coarse overmap-readable smoke signal state for bandit logic.
- [x] Generated smoke marks or smoke-born leads feed the current bandit mark-generation / playback / evaluator seams instead of relying on hand-authored smoke lore.
- [x] Deterministic coverage proves the bounded long-range rule honestly: sustained clear-weather smoke stays meaningfully long-range, while weak/brief or degraded-condition smoke stays shorter and fuzzier.
- [x] Reviewer-readable report output exposes the smoke packet and resulting mark/lead path instead of hiding the bridge in debugger soup.
- [x] The slice stays bounded: no light/searchlight adapter, no broad visibility/concealment implementation, no global offscreen smoke sim, and no first 500-turn proof smuggled in.


Compact reference:
- Canonical contract lives at `doc/bandit-smoke-visibility-mark-seam-v0-2026-04-20.md`.


---

## Bandit overmap AI concept chain

Status: CHECKPOINTED / COHERENT PARKED SUBSTRATE

Success state:
- [x] The parked concept packet defines the broad bandit overmap actor model, signal/memory role, threat shape, and overmap-to-bubble intent cleanly enough that later planning no longer has to rediscover the premise from scratch.
- [x] The broad packet explicitly states that bounty and threat do not passively decay: structural bounty changes through harvesting/exploitation, moving bounty through fresh activity and collection, threat through real recheck, and camp stockpile through explicit consumption.
- [x] Deterministic bounty/threat scoring guidance exists with explicit camp-ledger inputs, map-mark fields, harvest/collection/recheck rules, and job-scoring logic.
- [x] Overmap mark-generation and heatmap guidance exists with explicit overmap-only mark creation, recheck/harvest rules, and threat/bounty field update logic on the shared cadence family without passive decay.
- [x] Bidirectional overmap-to-bubble handoff guidance exists with explicit entry modes, return-path state, collapse-back rules, and reuse of existing pursuit/noise-routing footing where possible.
- [x] Supporting physical-systems recon exists so the visibility/concealment slice is grounded in existing smoke, light, sound, and weather hooks instead of made-up parallel physics.
- [x] Player/basecamp visibility and concealment guidance exists with explicit signal sources, environmental filters, bounty/threat interpretation outputs, repetition/reinforcement rules, and player/basecamp exposure-reduction levers.
- [x] The visibility item explicitly allows fog/mist to suppress sight/light legibility without requiring new fog-based sound dampening rules unless later evidence says otherwise.
- [x] The broad packet explicitly prevents suspicion spirals by making camp knowledge sparse and camp-owned, ground bounty coarse/finite/non-regenerating, moving bounty actor-driven, and checked/false-lead/harvested memory able to damp repeated interest.
- [x] The broad packet explicitly resolves basecamp fairness asymmetry by allowing sustained offscreen pressure and stalking, keeping decisive full camp assault player-present only for current scope, and not requiring attack presignaling as the fairness mechanism.
- [x] The packet explicitly names the current overmap-NPC persistence/travel/companion substrate as reusable footing for stalking and intercept pressure, without pretending the existing need-driven random-NPC policy is already the finished hostile model.
- [x] The broad packet explicitly resolves handoff identity continuity by making the overmap group itself persistent, carrying only a small anchored-individual slice directly across handoffs, and treating the rest of the group as fungible mission strength.
- [x] The broad packet explicitly treats smoke/mark destinations as provisional mission leads whose goals can be continued, diverted, shadowed, or aborted by local observations instead of sacred tile commitments.
- [x] The broad packet explicitly resolves city opportunism under zombie pressure by allowing occasional risky opportunism without requiring direct bandit-versus-zombie tactical simulation, while keeping repeat attractiveness bounded by depleting bounty and sticky threat memory.
- [x] The broad packet explicitly keeps threat marks sticky enough that bands do not cheaply remote-rewrite a scary area as safe again until they return close enough to genuinely reassess it.
- [x] The broad packet explicitly resolves multi-camp dogpile behavior by keeping camps mostly independent in v1, allowing occasional overlap without turning it into routine coalition swarming.
- [x] The broad packet explicitly uses territoriality, distance burden, depletion, sticky threat, and fresh active-pressure penalties to damp repeated multi-camp convergence on the same target region.
- [x] The whole bandit concept packet is now coherent enough that it can support bounded promotion into greenlit implementation slices without hidden open seams.



---

## Arsenic-and-Old-Lace social threat and agentic-world concept bank

Status: CHECKPOINTED / FAR-BACK PARKED SUBSTRATE

Success state:
- [x] One auxiliary concept-bank doc preserves the current speculative C-AOL feature families cleanly enough that later planning no longer has to rediscover them from scattered chat.
- [x] The preserved bank explicitly separates nearer playable threat/control seeds from broader social-horror and weird-world systemic ideas instead of flattening everything into one soup.
- [x] The bank explicitly preserves alarm states with natural-language yelling control, radio information warfare, writhing-stalker pressure, bounded sight avoidance, feral-camp pressure, social camouflage, hidden-psychopath survivor play, faction mythmaking, living camp mood/fracture, agentic NPC goals, and interdimensional-traveler motive hooks.
- [x] `Plan.md` points to the bank as a far-back parked concept substrate rather than as an active or greenlit implementation lane.
- [x] The preserved bank explicitly states that current bandit and Basecamp zoning footing still needs honest playtesting before these concepts earn bounded promotion.
- [x] The bank explicitly requires future revisit to happen one bounded promotion at a time instead of reopening fifty speculative threads at once.



---

## Plan/Aux pipeline helper

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] A small helper can take a proposed item/greenlight and print the contract back for verification before canon files are changed.
- [x] The helper can collect corrections and then classify the item cleanly as active, parked, or bottom-of-stack.
- [x] The helper can update the relevant canon files consistently (`Plan.md`, `TODO.md`, `SUCCESS.md`, `TESTING.md` when needed, plus the auxiliary doc).
- [x] The helper reduces manual file carpentry for already-understood greenlights without bypassing the frozen workflow.
- [x] The helper can optionally generate the Andi handoff packet from the same classified contract.


Compact reference:
- Canonical contract lives at `doc/plan-aux-pipeline-helper-2026-04-09.md`.
