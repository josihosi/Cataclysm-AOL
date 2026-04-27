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

Notes:
- Canonical contract lives at `doc/github-actions-ci-recovery-checkpoint-packet-v0-2026-04-25.md`.
- Observed failing run footing before packaging: `General build matrix` runs `24931588361` / `24931574601`, `Cataclysm Windows build` run `24931574609`, and `CodeQL` run `24932236370`.
- CI checkpoint/linking rule was added to `COMMIT_POLICY.md` in `6c0e75b8ec`; current recovery checkpoints now name file class, local gate, Actions link, and remaining red-check classification in handoffs/docs.
- Current recovery checkpoints show the source/warnings-as-errors family cleared locally and past CodeQL compile extraction: `make -j4 tests LINTJSON=0 ASTYLE=0`, focused camp/bandit/overmap tests, and CodeQL run `24938191289` all reached JSON style instead of the previous C++17/missing-declaration failures.  Windows run `24939016251` on `471d4ef8e6` first closed the earlier Windows source failure by evidence.
- Final CI closure on `c5ff712e01`: `General build matrix` `24944793868`, `Cataclysm Windows build` `24944793884`, `CodeQL` `24944793877`, `IWYU` `24944793878`, and `Clang-tidy 18` `24944793865` all completed green, so the release packet may become active.

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

Notes:
- Canonical contract lives at `doc/test-vs-game-implementation-audit-report-packet-v0-2026-04-26.md`.
- Closed report lives at `doc/test-vs-game-implementation-audit-report-2026-04-26.md`.
- First implementation package after the audit is `Bandit live signal + site bootstrap correction v0`.

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

Notes:
- Canonical contract lives at `doc/bandit-live-wiring-audit-and-light-horde-bridge-correction-v0-2026-04-26.md`.
- First sweep found the highest-risk deterministic-only wording in `tests/bandit_mark_generation_test.cpp` and `tests/bandit_playback_test.cpp`: useful adapter/playback proof, not live game proof by itself.
- Implementation checkpoint: `src/do_turn.cpp` assigns `horde_signal_power` to qualifying live light observations and calls `overmap_buffer.signal_hordes(...)`; `src/bandit_mark_generation.cpp` keeps the threshold policy bounded via `horde_signal_power_from_light_projection(...)`. Live/harness closure proof now exists at `.userdata/dev-harness/harness_runs/20260427_031951/`: scenario `bandit.live_world_nearby_camp_light_horde_mcw` sets a copied save to night, injects a safe raw `fd_fire` source near the player, and logs `bandit_live_world horde light signal:` with `horde_signal_power=22`. This is loaded-map visible fire/light bridge proof, not player-lit brazier/wood/lighter product proof.

---

## Bandit local sight-avoid + scout return cadence packet v0

Status: ACTIVE / GREENLIT NOW

Success state:
- [ ] Stalking / hold-off bandits in the reality bubble can detect current or recent exposure to the player or nearby camp NPCs and attempt a bounded reposition toward cover or broken line of sight.
- [x] The sight-avoid behavior is local and heuristic: deterministic tests prove it does not use magical future-cone omniscience and does not teleport.
- [x] A scout outing has a finite live sortie window and can return home after watching long enough, instead of lingering indefinitely in local contact. _(Deterministic/live-wired source path plus live/harness return-home decision proof `.userdata/dev-harness/harness_runs/20260427_051117/`.)_
- [ ] Returned scout state writes back through the owned-site memory path and can drive later re-evaluation without automatically conjuring a larger group. _(Deterministic writeback coverage; full walked-home/writeback live proof still open.)_
- [x] The single-scout current behavior remains explainable: `scout` is still one member unless a later job or escalated decision explicitly requires more.
- [ ] Reviewer-readable output distinguishes `still stalking`, `repositioning because exposed`, `returning home`, and `re-dispatch/escalation decision`. _(Source/log/report strings are present and live returning-home output is proved; live sight-avoid/re-dispatch reviewer proof still open.)_
- [x] At least one live/harness proof uses `bandit.live_world_nearby_camp_mcw` or an equivalent real owned-site scenario and confirms the same code path would apply to a normal discovered bandit camp, while separately naming the harness bias that places the camp nearby on purpose.

Notes:
- Canonical contract lives at `doc/bandit-local-sight-avoid-and-scout-return-cadence-packet-v0-2026-04-26.md`.
- 2026-04-27 implementation checkpoint: deterministic `[bandit][live_world]` coverage now passes for bounded adjacent sight-avoid choice, finite scout sortie expiry, return-home/writeback, active job typing, and new active sortie serialization fields. Local gates: `git diff --check`; `make -j4 tests LINTJSON=0 ASTYLE=0`; `./tests/cata_test "[bandit][live_world]"` -> 524 assertions in 22 test cases passed.
- Live/harness return-home decision proof now exists: `.userdata/dev-harness/harness_runs/20260427_051117/` uses equivalent nearby-owned-site local-contact footing plus a narrow pre-aged sortie-clock transform, records `scout_sortie: linger limit reached -> return_home`, and confirms `returning_home -> local_gate skipped` with no shakedown/local-gate artifact. This is not full walked-home/writeback proof. Earlier `.userdata/dev-harness/harness_runs/20260427_040319/` and `.userdata/dev-harness/harness_runs/20260427_041554/` remain inconclusive and must not be counted.
- Josef's smoke-attraction observation still wants a no-smoke control before we claim smoke itself is the live cause.

---

## Smart Zone Manager v1 Josef playtest corrections

Status: GREENLIT / QUEUED FOLLOW-UP

Success state:
- [ ] Smart Zone Manager adds `LOOT_MANUALS` coverage on/near the Basecamp books cluster without removing ordinary `LOOT_BOOKS` coverage.
- [ ] Gun-magazine coverage remains `LOOT_MAGAZINES`, preferably with an unambiguous user-facing label such as "Basecamp weapon magazines" if label text is touched.
- [ ] Auto-eat and auto-drink coverage spans the full Basecamp storage zone, with `ignore_contents` explicitly false so Basecamp sorting still sees the covered items.
- [ ] Deterministic tests assert the actual zone ids/types and the `ignore_contents == false` option, not just label text.
- [ ] Harness or save inspection confirms generated zones remain saved/reopenable and do not crash or corrupt the camp layout.

Notes:
- Canonical contract lives at `doc/smart-zone-manager-v1-josef-playtest-followup-2026-04-26.md`.
- Josef checked the existing generated zones manually and narrowed this follow-up to `LOOT_MANUALS`, book/manual/gun-magazine clarity, and full-storage auto-consume zones.

---

## Basecamp medical consumable readiness v0

Status: GREENLIT / QUEUED FOLLOW-UP

Success state:
- [ ] Camp locker/service logic recognizes at least `bandages` and `adhesive_bandages` as bounded medical readiness supplies when stocking NPCs.
- [ ] NPCs can pick up a small reserve from the relevant Basecamp/locker storage path without hoarding all medical items.
- [ ] Existing carried bandages and adhesive bandages are preserved through locker refresh instead of being discarded as clutter.
- [ ] Non-medical loadout behavior, ammo/magazine readiness, and clothing/armor selection remain unchanged.
- [ ] Deterministic tests cover fresh pickup, carried-item preservation, cap/anti-hoarding behavior, and a negative case for unrelated drugs/items.
- [ ] If a live/harness proof is practical, one Basecamp/locker probe shows an NPC can acquire the intended medical consumable from camp storage; otherwise the packet states plainly why deterministic proof is sufficient for the first slice.

Notes:
- Canonical contract lives at `doc/basecamp-medical-consumable-readiness-v0-2026-04-26.md`.
- This is parked separately from the bandit live-signal correction.

---

## Basecamp locker armor ranking + blocker removal packet v0

Status: GREENLIT / QUEUED FOLLOW-UP

Success state:
- [ ] A generic helper scores candidate protective/full-body gear against currently worn blockers using body-part priority, protection/coverage, encumbrance, condition, and active locker policy.
- [ ] The helper is not item-id-specific and does not special-case `RM13 combat armor`.
- [ ] When a candidate is clearly superior, the locker path removes/drops the blocking worn items needed to equip it.
- [ ] When a candidate is not clearly superior or cannot be equipped, the locker path stops retrying the same failed swap and does not produce visible repeated wear spam.
- [ ] Existing superior-full-body and ballistic-maintenance tests are reused/extended, including positive and negative cases.
- [ ] Tests prove a clearly superior full-body/protective suit can displace worse blockers, while stronger current ballistic armor is preserved against worse candidates.
- [ ] At least one targeted regression covers the original symptom shape without depending on the exact RM13 item ID as the only proof.

Notes:
- Canonical contract lives at `doc/basecamp-locker-armor-ranking-blocker-removal-packet-v0-2026-04-26.md`.
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

Notes:
- Canonical contract lives at `doc/bandit-overmap-local-handoff-interaction-packet-v0-2026-04-21.md`.
- The current tree now closes this packet honestly on the playback/proof seam through `run_overmap_local_handoff_interaction_packet()` plus `render_overmap_local_handoff_interaction_packet( const handoff_packet_result &result )` in `src/bandit_playback.{h,cpp}`.
- Deterministic coverage in `tests/bandit_playback_test.cpp` now proves three bounded interaction slices directly: smoke posture narrows to a sharper mark, local danger can break a corridor probe hard enough to come home wounded, and a player-present follower-travel sighting degrades to rough search after contact loss instead of preserving exact-carrier certainty.
- Narrow deterministic validation passed on the current tree via `make -j4 tests`, `./tests/cata_test "[bandit][handoff]"`, `./tests/cata_test "[bandit][playback]"`, and `./tests/cata_test "[bandit]"`.

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

Notes:
- Canonical contract lives at `doc/bandit-elevated-light-and-z-level-visibility-packet-v0-2026-04-21.md`.
- The current tree now closes this packet honestly through vertical-aware smoke/light footing in `src/bandit_mark_generation.{h,cpp}` plus `run_elevated_light_z_level_visibility_packet()` and `render_elevated_light_z_level_visibility_packet( const proof_packet_result &result )` in `src/bandit_playback.{h,cpp}`.
- Deterministic coverage in `tests/bandit_mark_generation_test.cpp` and `tests/bandit_playback_test.cpp` now proves the bounded active contract directly: nearby cross-z light stays actionable without magical range creep, hidden ground light stays bounded while elevated exposed light can become actionable, a radio-tower fire carries shared zombie-horde pressure on the proof/playback seam, and vertical smoke keeps `vertical_range_bonus=0`. It does not prove live horde movement by light.
- Narrow deterministic validation passed on the current tree via `make -j4 tests`, `./tests/cata_test "[bandit][marks]"`, and `./tests/cata_test "[bandit][playback]"`.

---

## Bandit repeated-site revisit behavior packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded repeated-site follow-through exists on the current playback / evaluator footing, producing one more deliberate revisit / cautious-watch posture than plain early scout bookkeeping.
- [x] Deterministic coverage proves repeated same-site corroboration still does **not** unlock free `scavenge`, `steal`, or `raid` truth, and does not become immortal pressure.
- [x] Scenario `repeated_site_interest_stays_bounded` exposes the benchmark-facing long-horizon metrics reviewer-cleanly: `site_visit_count_500`, `site_revisit_count_500`, `cooldown_turn`, and `endless_pressure_flag`.
- [x] The honest `500`-turn proof shows the strengthened site cooling back out instead of regrowing forever.
- [x] The slice stays narrow: no site-type-sensitive branching, no settlement taxonomy pass, no broad visibility rewrite, and no z-level smuggling.

Notes:
- Canonical contract lives at `doc/bandit-repeated-site-revisit-behavior-packet-v0-2026-04-21.md`.
- This is the active follow-through on top of the already checkpointed repeated-site reinforcement seam and the already checkpointed benchmark suite.
- Josef explicitly liked the bounded revisit behavior plus metric-alignment follow-through, and explicitly rejected widening this packet into the broader site-type-sensitive branch.

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

Notes:
- Canonical contract lives at `doc/bandit-overmap-benchmark-suite-packet-v0-2026-04-21.md`.
- The current tree now closes honestly on the current playback/proof seam via `run_overmap_benchmark_suite_packet()` plus `render_overmap_benchmark_suite_packet( const benchmark_suite_result &result )` in `src/bandit_playback.{h,cpp}`.
- Deterministic coverage in `tests/bandit_playback_test.cpp` now proves the benchmark-suite contract, metric surface, empty-frontier packet, weather edge packet, and independence packet honestly.
- Narrow deterministic validation passed on the current tree via `make -j4 tests`, `./tests/cata_test "[bandit][playback]"`, and `./tests/cata_test "[bandit]"`.
- The benchmark suite stays closed, and the later elevated-light / z-level follow-through is now checkpointed too.

---

## Bandit long-range directional light proof packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded long-range directional-light proof packet exists on the current bandit scenario / playback seam.
- [x] Deterministic multi-turn proof up to `500` turns shows the hidden-side leakage case stays non-actionable while the visible-side leakage case becomes actionable under the same broader footing.
- [x] The matching deterministic zombie-horde corridor variant proves the same light can carry abstract horde pressure too instead of existing in isolated bandit-only theater; this is **not** live horde attraction until the live light-to-horde bridge exists.
- [x] Each scenario carries explicit goals and tuning metrics, and reviewer-readable output shows whether those benchmarks were met.
- [x] The slice stays bounded: no z-level expansion, no broad light-system rewrite, no handoff redesign, and no fresh world-sim jump.

Notes:
- Canonical contract lives at `doc/bandit-long-range-directional-light-proof-packet-v0-2026-04-21.md`.
- The landed packet now closes honestly on the current tree, and the current active lane moved on to the pressure-rewrite follow-up.

---

## Bandit overmap/local pressure rewrite packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded overmap/local pressure-rewrite proof packet exists on the current bandit scenario / playback footing.
- [x] Deterministic multi-turn proof shows a stalking or intercept posture can honestly cool, retreat, or reroute after local reality makes the original posture too dangerous.
- [x] Reviewer-readable output shows the pressure rewrite clearly enough to explain why the stale pursuit no longer holds.
- [x] Each scenario carries explicit goals plus scenario-specific benchmark hooks, and the later locked benchmark outcomes stay visible on the same report path.
- [x] The slice stays bounded: no broad handoff redesign, no tactical local combat AI expansion, and no fresh world-sim jump.

Notes:
- Canonical contract lives at `doc/bandit-overmap-local-pressure-rewrite-packet-v0-2026-04-21.md`.
- The current tree now carries the bounded pressure-rewrite packet in `src/bandit_playback.{h,cpp}` through the named `generated_local_loss_rewrites_corridor_to_withdrawal` and `generated_local_loss_reroutes_to_safer_detour` scenarios plus the first-class `run_overmap_local_pressure_rewrite_proof_packet()` / `render_overmap_local_pressure_rewrite_proof_packet( const proof_packet_result &result )` path.
- Deterministic coverage in `tests/bandit_playback_test.cpp` now proves the key bounded distinctions honestly: one stale corridor posture collapses to `hold / chill`, one parallel intercept case reroutes onto a safer detour, and the long horizon stays off the burned route instead of regrowing it by habit.
- Narrow deterministic validation passed via `make -j4 tests`, `./tests/cata_test "[bandit][playback]"`, and `./tests/cata_test "[bandit]"`.

---

## Bandit weather concealment refinement packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded weather-refinement packet exists on the current smoke/light mark-generation footing.
- [x] Deterministic coverage proves wind meaningfully fuzzes or de-precises smoke output instead of acting only as a token penalty.
- [x] Deterministic coverage proves portal-storm weather is handled explicitly for smoke and light instead of falling through as an unmodeled special case.
- [x] Reviewer-readable output explains how weather changed clue quality, for example reduced range, fuzzier origin, displaced/corridor-ish smoke read, or preserved bright-light legibility under dark storm conditions.
- [x] The slice stays bounded: no full plume physics, no global smoke sim, no sound-law rewrite, no z-level packet, and no broad visibility architecture rework.

Notes:
- Canonical contract lives at `doc/bandit-weather-concealment-refinement-packet-v0-2026-04-21.md`.
- The current tree now carries the bounded weather packet in `src/bandit_mark_generation.{h,cpp}` plus reviewer-readable smoke/light weather summaries, with matching scenario coverage in `src/bandit_playback.cpp`.
- Deterministic coverage in `tests/bandit_mark_generation_test.cpp` and `tests/bandit_playback_test.cpp` now proves the bounded weather distinctions honestly: windy smoke stays actionable but fuzzier, portal-storm smoke is harder to localize, exposed bright portal-storm light can stay legible while sheltered ordinary light stays bounded, and rain remains an explicit reducer.
- Narrow deterministic validation passed via `make -j4 tests`, `./tests/cata_test "[bandit][marks]"`, `./tests/cata_test "[bandit][playback]"`, and `./tests/cata_test "[bandit]"`.

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

Notes:
- Canonical contract lives at `doc/bandit-bounded-scout-explore-seam-v0-2026-04-21.md`.
- The current tree now carries the bounded evaluator packet in `src/bandit_dry_run.{h,cpp}`, using explicit `camp_input` footing for bounded explore instead of smuggling wandering out of failed routes.
- Deterministic coverage in `tests/bandit_dry_run_test.cpp` now proves the explicit-greenlight, blocked-route, and strong-real-lead splits reviewer-cleanly.
- `src/bandit_playback.cpp` now carries the named `bounded_explore_frontier_tripwire` scenario packet with explicit goals and tuning metrics, and `tests/bandit_playback_test.cpp` proves the same rule on the scenario seam.
- Narrow deterministic validation passed via `make -j4 tests`, `./tests/cata_test "[bandit][dry_run]"`, and `./tests/cata_test "[bandit][playback]"`.

---

## Bandit concealment seam v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded concealment adapter exists on the current light signal seam, weakening outward light legibility when exposure is poor instead of inventing a second fake visibility machine.
- [x] Deterministic coverage proves the key bounded distinctions honestly: daylight suppression, weather penalty, containment, and side-dependent leakage/suppression.
- [x] Reviewer-readable output shows why a light-born mark was reduced, blocked, or allowed instead of hiding the answer in debugger soup.
- [x] The slice stays bounded: no broad all-signals concealment rewrite, no new fog-sound law, no global smoke/world simulation, no tactical stealth doctrine, and no pursuit/handoff expansion.
- [x] If the concealment adapter starts looking computationally suspicious, the packet carries one small readable cost/probe angle instead of deferring perf truth to later folklore.

Notes:
- Canonical contract lives at `doc/bandit-concealment-seam-v0-2026-04-21.md`.
- This narrow promotion is now checkpointed on the current light seam: `src/bandit_mark_generation.{h,cpp}` carries the concealment reduction and reviewer-readable verdict summary, with deterministic proof in `tests/bandit_mark_generation_test.cpp` plus `tests/bandit_playback_test.cpp`.
- Narrow deterministic validation passed via `make -j4 tests` and `./tests/cata_test "[bandit]"`.

---

## Bandit scoring refinement seam v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded scoring-refinement adapter exists on the current bandit dry-run/evaluator seam, refining how existing camp ledger state plus existing marks become job choice.
- [x] The threat side first inspects and collapses usable existing threat/danger footing instead of inventing a fresh bespoke threat ontology.
- [x] Deterministic coverage proves the key opportunism split honestly: bandits avoid strong opponents, but pounce when zombie pressure or other distraction lowers effective target coherence.
- [x] Reviewer-readable output shows the refined choice breakdown clearly enough to explain why a target was avoided, deferred, or exploited.
- [x] The slice stays bounded: no new visibility signal family, no broad heatmap/memory rewrite, no tactical zombie simulation, no coalition strategy layer, and no fresh world-sim expansion.

Notes:
- Canonical contract lives at `doc/bandit-scoring-refinement-seam-v0-2026-04-21.md`.
- The current tree now carries the bounded scoring packet in `src/bandit_dry_run.{h,cpp}` plus the mark-to-evaluator bridge update in `src/bandit_mark_generation.cpp`, with deterministic proof in `tests/bandit_dry_run_test.cpp` and `tests/bandit_mark_generation_test.cpp`.
- The landed packet keeps the danger collapse small and inspectable: existing monster pressure plus target-coherence loss now reduce effective threat and add a bounded opportunism push on aggressive jobs instead of inventing a fresh threat cosmology.
- Narrow deterministic validation passed via `make -j4 tests` and `./tests/cata_test "[bandit]"`.

---

## Bandit moving-bounty memory seam v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded moving-bounty memory object exists for live `actor` or `corridor` style bounty, while structural bounty stays on site state instead of gaining chase memory.
- [x] Structural sites keep only cheap state such as harvested / recently-checked / false-lead / sticky-threat footing, with no stalking logic glued onto them.
- [x] Deterministic coverage proves the key bounded pursuit split honestly: live moving prey can be stalked briefly, but the memory collapses cleanly on lost contact, split, bad recheck, rising threat, or leash expiry instead of retrying forever.
- [x] Reviewer-readable output shows whether a moving lead was refreshed, narrowed, or dropped instead of hiding the memory state in debugger soup.
- [x] The slice stays computationally cheap: no per-turn tracking, no path-history scrapbook, no per-NPC biography graph, no endless retry loop, and no broad memory-palace world model.

Notes:
- Canonical contract lives at `doc/bandit-moving-bounty-memory-seam-v0-2026-04-21.md`.
- The current tree now carries the bounded moving-memory packet in `src/bandit_mark_generation.{h,cpp}`, keeping moving-carrier and corridor leads briefly stalkable while structural/site state stays cheap and non-chasing.
- Deterministic coverage in `tests/bandit_mark_generation_test.cpp` proves the bounded split honestly: moving prey persists briefly after raw signal cooling, structural sites do not gain chase memory, and stale moving contact drops reviewer-cleanly on leash expiry.
- `tests/bandit_playback_test.cpp` now keeps the corridor playback packet aligned with the bounded pursuit window instead of pretending the corridor mark evaporates immediately.
- Narrow deterministic validation passed via `make -j4 tests` and `./tests/cata_test "[bandit]"`.

---

## Bandit first 500-turn playback proof v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] At least one honest deterministic 500-turn bandit playback packet exists on the current abstract seams.
- [x] Reviewer-readable report output exposes long-horizon checkpoints and winner drift cleanly enough to inspect the 500-turn answers without debugger soup.
- [x] Deterministic coverage proves the chosen long-horizon scenarios stay bounded, including cooldown, peel-off, and repeated-site reinforcement behavior that does not magically harden into free raid truth.
- [x] The proof reuses the current mark-generation / playback / evaluator seams instead of smuggling in a broader overmap simulator or persistence rewrite.
- [x] The slice stays bounded: no new visibility adapter family, no live-harness-first theater, and no broad AI architecture jump.

Notes:
- Canonical contract lives at `doc/bandit-first-500-turn-playback-proof-v0-2026-04-20.md`.
- The current tree now has the 500-turn proof packet in `src/bandit_playback.{h,cpp}` with deterministic coverage in `tests/bandit_playback_test.cpp`.
- Narrow deterministic validation passed via `make -j4 tests` and `./tests/cata_test "[bandit]"`.

---

## Bandit repeated site activity reinforcement seam v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] A bounded repeated-site reinforcement adapter exists from mixed repeated smoke/light/traffic footing into modest site-mark confidence and bounty amplification.
- [x] Reinforced site marks feed the current bandit mark-generation / playback / evaluator seams reviewer-cleanly instead of staying hand-authored lore.
- [x] Deterministic coverage proves the key bounded distinctions: mixed repeated signals reinforce one site mark cleanly, weak repetition does not fake durable settlement truth, self-corroboration stays bounded, and strengthened site interest still does not unlock free extraction jobs.
- [x] Reviewer-readable report output exposes the reinforcement packet and resulting mark/lead path instead of hiding the bridge in debugger soup.
- [x] The slice stays bounded: no smoke/light/human-route rewrite, no broad concealment implementation, no settlement-signature mythology, and no first 500-turn proof smuggled in.

Notes:
- Canonical contract lives at `doc/bandit-repeated-site-activity-reinforcement-seam-v0-2026-04-20.md`.
- This was the next narrow visibility promotion after the smoke, light, and human / route bridge checkpoints, not permission to implement the whole visibility system at once.
- The current tree now has the bounded repeated-site seam in `src/bandit_mark_generation.{h,cpp}` and `src/bandit_playback.cpp`, with deterministic coverage in `tests/bandit_mark_generation_test.cpp` plus `tests/bandit_playback_test.cpp`.
- Narrow deterministic validation passed via `make -j4 tests` and `./tests/cata_test "[bandit]"`.

---

## Bandit human / route visibility mark seam v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] A bounded human / route adapter exists from direct sightings and repeated route-shaped activity, or equivalent deterministic route-visibility packets, into coarse overmap-readable moving-carrier, corridor, or bounded site signal state.
- [x] Generated human / route marks or leads feed the current bandit mark-generation / playback / evaluator seams reviewer-cleanly instead of staying hand-authored lore.
- [x] Deterministic coverage proves the key bounded distinctions: direct sighting versus repeated corridor activity, moving-carrier attachment versus site inflation, self-camp routine suppression, and at least one corroborated shared-route refresh case.
- [x] Reviewer-readable report output exposes the route packet and resulting mark/lead path instead of hiding the bridge in debugger soup.
- [x] The slice stays bounded: no light/smoke rewrite, no broad concealment implementation, no settlement-signature mythology, and no first 500-turn proof smuggled in.

Notes:
- Canonical contract lives at `doc/bandit-human-route-visibility-mark-seam-v0-2026-04-20.md`.
- This is the next narrow visibility promotion after the smoke and light bridge checkpoints, not permission to implement the whole visibility system at once.
- The current tree now has the bounded human / route seam in `src/bandit_mark_generation.{h,cpp}` and `src/bandit_playback.{h,cpp}`, with deterministic coverage in `tests/bandit_mark_generation_test.cpp` plus `tests/bandit_playback_test.cpp`.
- Narrow deterministic validation passed via `make -j4 tests` and `./tests/cata_test "[bandit]"`.

---

## Bandit light visibility mark seam v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] A bounded light-specific adapter exists from current local light and directional-exposure footing, or equivalent deterministic light packets, into coarse overmap-readable light signal state.
- [x] Generated light marks or light-born leads feed the current bandit mark-generation / playback / evaluator seams reviewer-cleanly instead of staying hand-authored lore.
- [x] Deterministic coverage proves the key bounded distinctions: daylight suppression, contained versus exposed night light, ordinary occupancy light versus searchlight-like threat light, and at least one side-dependent leakage case.
- [x] Reviewer-readable report output exposes the light packet and resulting mark/lead path instead of hiding the bridge in debugger soup.
- [x] The slice stays bounded: no smoke rewrite, no broad concealment implementation, no sound/horde expansion, and no first 500-turn proof smuggled in.

Notes:
- Canonical contract lives at `doc/bandit-light-visibility-mark-seam-v0-2026-04-20.md`.
- This is the next narrow visibility promotion after the smoke bridge checkpoint, not permission to implement the whole visibility system at once.
- The current tree now has the bounded light seam in `src/bandit_mark_generation.{h,cpp}` and `src/bandit_playback.{h,cpp}`, with deterministic coverage in `tests/bandit_mark_generation_test.cpp` plus `tests/bandit_playback_test.cpp`.
- Narrow deterministic validation passed via `make -j4 tests` and `./tests/cata_test "[bandit]"`.

---

## Bandit mark-generation + heatmap seam v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] A bounded overmap-side mark ledger and broad bounty/threat heat-pressure seam exist for deterministic bandit inputs.
- [x] Deterministic coverage proves mark creation, refresh, selective cooling, and sticky confirmed threat on named reference cases.
- [x] The existing evaluator / playback footing can consume generated mark output reviewer-cleanly instead of relying only on hand-authored leads.
- [x] The slice stays bounded: no bubble handoff, no broad visibility adapter, and no full hostile-world simulation are smuggled in.

Notes:
- Canonical contract lives at `doc/bandit-mark-generation-heatmap-seam-v0-2026-04-20.md`.
- The current tree now has the bounded writer-side seam in `src/bandit_mark_generation.{h,cpp}`, the playback bridge in `src/bandit_playback.{h,cpp}`, and deterministic coverage in `tests/bandit_mark_generation_test.cpp` plus `tests/bandit_playback_test.cpp`.
- Narrow deterministic validation passed via `make -j4 tests` and `./tests/cata_test "[bandit]"`.

---

## Bandit overmap-to-bubble pursuit handoff seam v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded pursuit / investigation handoff exists from abstract overmap group state into local play.
- [x] The return path preserves meaningful abstract consequences such as updated mark/threat knowledge, losses, panic, cargo, or retreat state instead of dropping them on the floor.
- [x] Entry payload and return packet stay explicit, small, and reviewer-readable.
- [x] The slice stays bounded: no full raid / ambush suite, no broad tactical AI rewrite, and no full per-bandit biography persistence are smuggled in.

Notes:
- Canonical contract lives at `doc/bandit-overmap-to-bubble-pursuit-handoff-seam-v0-2026-04-20.md`.
- The bounded handoff seam now lives in `src/bandit_pursuit_handoff.{h,cpp}` and stays on abstract group state plus explicit `entry_payload` / `return_packet` packets instead of pretending full local combat AI already exists.
- Deterministic coverage in `tests/bandit_pursuit_handoff_test.cpp` proves the bounded scout entry packet, explicit return consequences, moving-carrier shadow routing, and reviewer-readable report output.
- Narrow deterministic validation passed via `make -j4 tests`, `./tests/cata_test "[bandit][handoff]"`, and `./tests/cata_test "[bandit]"`.

---

## Locker lag-threshold probe v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One honest threshold packet exists for the real `CAMP_LOCKER` service path.
- [x] The packet distinguishes top-level item pressure from worker-count pressure instead of flattening them together.
- [x] The result can name an approximate fine / suspicious / bad range, or honestly report that no clear threshold was found within the tested bound.
- [x] If the threshold looks bad, the packet ends with a small cheap-first guardrail recommendation order instead of architecture opera, and if it does not, the packet says so plainly.

Notes:
- Canonical contract lives at `doc/locker-lag-threshold-probe-v0-2026-04-20.md`.
- This follow-up exists because `Locker clutter / perf guardrail probe v0` answered shape better than the sharper player-facing lag-threshold question.
- Current packet result: no clear knee was found through `20000` top-level locker items, and the `5000`-clutter worker sweep stayed around `1.0 ms` per worker across `1 / 5 / 10`.

---

## Bandit evaluator dry-run seam v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] A deterministic dry-run evaluator exists for controlled bandit camp inputs.
- [x] The evaluator always includes `hold / chill` and only emits outward candidates from real compatible leads or current hard state.
- [x] The first explanation surface shows leads considered, the full candidate board, per-candidate score inputs/final score, veto/soft-veto reasons, and winner versus `hold / chill`.
- [x] Narrow deterministic coverage exists for the first pure reasoning reference cases.
- [x] The slice stays bounded: no full autonomous bandit world behavior, no broad scenario playback suite, and no broad persistence architecture are smuggled in.

Notes:
- Canonical contract lives at `doc/bandit-evaluator-dry-run-seam-v0-2026-04-20.md`.
- This is the first promoted implementation slice from the parked bandit concept chain; broader bandit implementation still stays parked outside the explicitly greenlit v0 slices.

---

## Bandit scenario fixture + playback suite v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] Named deterministic bandit scenarios exist for the first reference cases.
- [x] The playback contract can inspect behavior at multiple checkpoints such as `tick 0`, `tick 5`, `tick 20`, and one longer horizon.
- [x] The scenario packet can answer whether camps stay idle, investigate smoke, stalk edges, peel off under pressure, or mis-upgrade whole regions from moving clues.
- [x] The suite stays bounded and does not turn into broad worldgen mutation or live-harness-first theater.

Notes:
- Canonical contract lives at `doc/bandit-scenario-fixture-playback-suite-v0-2026-04-20.md`.
- The landed playback seam is `src/bandit_playback.{h,cpp}` plus `tests/bandit_playback_test.cpp`, with fourteen stable named scenarios and checkpoint replay at `tick 0`, `tick 5`, `tick 20`, and `tick 100`.
- Narrow deterministic validation passed via `make -j4 tests` and `./tests/cata_test "[bandit]"`.

---

## Bandit perf + persistence budget probe v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] Repeatable cost measurements exist for the named bandit scenarios.
- [x] Obvious evaluator churn signals such as candidate-count growth, repeated scoring/path checks, or similar waste are visible instead of hidden.
- [x] Save-size growth has an honest first estimate tied to the actually persisted bandit state shape.
- [x] The packet can say whether the current design looks cheap enough, suspicious, or clearly too bloated before broader rollout.

Notes:
- Canonical contract lives at `doc/bandit-perf-persistence-budget-probe-v0-2026-04-20.md`.
- `src/bandit_playback.{h,cpp}` now provides `measure_scenario_budget()`, `measure_reference_suite_budget()`, `estimate_v0_persistence_budget()`, and `render_budget_report()` on top of the named playback suite, so the v0 measurement seam exists without smuggling in a broader optimization lane.
- `bandit_dry_run::evaluation_metrics` now exposes lead filtering, candidate generation, score/path checks, veto/no-path invalidations, and winner-comparison churn, while the first bounded persistence sample lands at about `512` payload bytes before serializer overhead and still reads cheap enough for the abstract v0 shape.
- Narrow deterministic validation passed via `make -j4 tests` and `./tests/cata_test "[bandit]"`.

---

## Locker clutter / perf guardrail probe v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One honest measurement packet exists for the real `CAMP_LOCKER` service path across a first bounded clutter sweep such as `50 / 100 / 200 / 500 / 1000` top-level items.
- [x] The packet distinguishes realistic worker-count pressure from item-hoard pressure instead of pretending twenty-to-fifty camp workers are the main threat.
- [x] The packet answers whether loaded magazines and ordinary container shapes mostly behave like one top-level locker item or create meaningful nested-content pain.
- [x] The packet can end with a usable verdict: fine for now, watch this, or land a guardrail now, plus the first cheap mitigation order if needed.

Notes:
- Canonical contract lives at `doc/locker-clutter-perf-guardrail-probe-v0-2026-04-20.md`.
- The current tree now meets the first bounded packet goal through `camp_locker_service_probe`, `basecamp::measure_camp_locker_service( npc & )`, and the deterministic locker-service probe coverage in `tests/faction_camp_test.cpp`.
- That coverage now includes top-level clutter sweeps, worker-count sweeps, the first junk-heavy / locker-candidate-heavy / ammo-magazine-container-heavy stock-shape comparison, and the nested-content question for loaded magazines and ordinary filled bags.
- Current honest verdict: `fine for now`. The observed service-path cost grows with top-level locker items and worker passes, while loaded magazines and ordinary filled bags still behave like one top-level locker item on this path.
- Probe bias should match likely play: item hoarding is common, while camp populations above about ten assigned NPCs are much less common.

---

## Controlled locker / basecamp follow-through packet

Status: CHECKPOINTED / PACKAGE 5 DONE FOR NOW

Success state:
- [x] **Package 1, harness zone-manager save-path polish** is landed with screenshots/artifacts from the current McWilliams harness path.
- [x] **Package 2, basecamp toolcall routing fix** is landed or honestly blocked, and the right discriminator is separated from the bad location-only heuristic.
- [x] **Package 3, locker outfit engine hardening** is now landed for the current required closeout slice: the weird board/log leak stays re-proved live on the rebuilt current tiles binary, the in-game-log plus `llm_intent.log` observability helper is exercised on that same live probe path, and the required deterministic/service-parity canon is closed through the outer one-piece civilian-garment seam (`abaya`) without reopening open-ended clothing-case collection.
- [x] **Package 5, basecamp carried-item dump lane** is landed: ordinary carried junk gets dumped during locker dressing, the kept carried lane is intentionally limited to `bandages`, `ammo`, and `magazines`, and curated locker stock is not polluted by the dump behavior.
- [x] The queue stayed controlled while Package 5 ran, and the next slice can now move forward cleanly as Package 4 instead of broadening into unrelated lanes.

Notes:
- Canonical package boundaries and acceptance bars live in `doc/locker-basecamp-followthrough-work-packages-2026-04-07.md`.
- The closeout run for the documented `bandages` acceptance item is `.userdata/dev-harness/harness_runs/20260419_133206/`: the live seeded packet shows exact `bandage`, Robbie visibly picks it up, and the rebuilt `camp locker:` artifact keeps `bandage`, `9x19mm JHP, reloaded`, and `Glock 9x19mm 15-round magazine` while dumping `small plastic bag` to cleanup.
- Package 4 is now reclosed separately in the `Locker Zone V1` ledger below, so this packet can stay checkpointed instead of pretending the whole locker/basecamp follow-through is still active.
- The ordinary harness footing for this packet should stay on `McWilliams` / `Zoraida Vick`, not drift back to the older default save.

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

Notes:
- Canonical implementation sketch lives in `doc/patrol-zone-v1-patch-plan-2026-04-06.md`.
- Intended order stayed: zone surface + 4-way clustering -> deterministic planner contract -> sticky roster / interrupt whitelist -> on-map hold-vs-loop -> live proof -> packet legibility close-out.
- The current packaged live proofs are `patrol.disconnected_live` -> `.userdata/dev-harness/harness_runs/20260406_230124/probe.report.json` and `patrol.connected_live` -> `.userdata/dev-harness/harness_runs/20260406_230552/probe.report.json`.
- Each current-binary patrol packet now includes readable staffing-pool and zone-topology crops, a tight `runtime_motion_compare.gif` blink helper, and a small `probe.patrol_summary.txt` explainer that states the shift roster, off-shift count, disconnected-vs-connected layout, and why gaps/holds are expected in that snapshot.
- Keep this lane closed unless later code or runtime evidence shows the deterministic contract or the packaged patrol packet has drifted out of truth.
- Watch for hallucinations, fake progress, and roadmap prose outrunning code/tests/live proof.

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

Notes:
- Canonical contract lives at `doc/smart-zone-manager-v1-aux-plan-2026-04-06.md`.
- Current live packet: `.userdata/dev-harness/harness_runs/20260409_140439/` via `tools/openclaw_harness/scenarios/smart_zone.live_probe_mcw_prepped.json` after the narrow prepared-save restage described in `TESTING.md`.
- Keep this focused on basecamp auto-layout helper behavior, not patrol/locker automation or smart-zone-manager soup.
- Keep this lane closed unless later code or runtime evidence disproves one of the bundled claims.

---

## Locker-capable harness restaging

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] A fixture or restaging path exists that contains a real `CAMP_LOCKER` zone and suitable locker-state/NPC-state for `locker.weather_wait`.
- [x] `locker.weather_wait` is no longer blocked on missing fixture shape.
- [x] A fresh packaged `locker.weather_wait` run reports **screen** / **tests** / **artifacts** separately on the repaired fixture path.
- [x] The result is described reviewer-cleanly as harness/fixture work on existing locker behavior, not as premature delivery of the later chat/ambient feature lanes.

Notes:
- This lane existed because the shipped `basecamp_dev_manual_2026-04-02` save fixture did not contain a `CAMP_LOCKER` zone even though the live dev save did.
- The repair is the captured harness-owned save fixture `tools/openclaw_harness/fixtures/saves/dev/locker_ready_dev_2026-04-06/`, plus the scenario update that restores the usual `basecamp_dev_manual_2026-04-02` profile snapshot while installing that locker-ready save.
- Latest packaged proof: `python3 tools/openclaw_harness/startup_harness.py probe locker.weather_wait` -> `.userdata/dev-harness/harness_runs/20260406_125056/probe.report.json` with `verdict: artifacts_matched` and separate screen/tests/artifacts reporting.
- The goal was not new locker gameplay; it was to make existing locker behavior probeable and regressible.
- Keep the later chat/ambient feature lanes clearly separate.

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

Notes:
- This lane is checkpointed because the requested footing now exists without pretending the remaining ambient/locker gaps are solved.
- The latest packaged chat proof is `python3 tools/openclaw_harness/startup_harness.py probe chat.nearby_npc_basic` on `dev-harness`, with recipient + prompt/response artifacts recorded at `.userdata/dev-harness/harness_runs/20260406_092352/probe.report.json`.
- The Josef packet now lives at `doc/josef-hackathon-runway-testing-packet-2026-04-06.md`.
- Harness screen audits now distinguish raw repo-HEAD drift from runtime-relevant drift, so docs/harness-only commits no longer falsely demote a runtime-compatible captured game window to `inconclusive_version_mismatch`.
- `ambient.weird_item_reaction` now tails the correct repo-level `config/llm_intent.log`; its latest packaged run at `.userdata/dev-harness/harness_runs/20260406_092532/probe.report.json` is honest about the remaining gap: `inconclusive_no_new_artifacts` on a runtime-compatible build, not a fake no-artifact result from watching `debug.log`.
- `locker.weather_wait` is now explicitly demoted back to blocked status until a locker-capable fixture/restaging path exists.
- Important reviewer-facing distinction: the packaged `chat.nearby_npc_basic` and `ambient.weird_item_reaction` scenarios are harness scaffolding only. They are **not** the real feature work for "chat interface over dialogue branches" or the "tiny ambient-trigger NPC model," and must not be described as partial completion of those feature lanes.

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

Notes:
- V2 is checkpointed because the targeted locker suite plus the current-binary live packet now cover the bundled exit state.
- If V2 work disproves any bundled V1 claim, reopen V1 first instead of pretending V2 can continue cleanly.
- No Josef-specific follow-up checks are currently needed for this closed slice.

---

## Locker Zone V1

Status: CHECKPOINTED / DONE FOR NOW

Bundled V1 completion tasks:
- [x] **Locker surface/control task** — `CAMP_LOCKER` works as a real Zone Manager zone on the fresh-save path, ordinary sorting does not steal from locker tiles, camp locker policy state exists, the player-facing locker policy menu/control exists, and the current surface is free of the reported zone-creation type-mismatch.
- [x] **Locker outfitting core task** — representative locker item classification, candidate gathering, score helpers, planning, equip/upgrade behavior, duplicate cleanup, and returning replaced managed gear all exist.
- [x] **Locker maintenance rhythm task** — wake-up dirty, policy-change dirty, new-gear dirty, lost-gear dirty, queue sequencing, and reservation behavior all exist.
- [x] **Locker V1 proof task** — deterministic coverage plus proportional runtime validation for the same downtime/service path are recorded in `TESTING.md`.
- [x] Any Josef-specific V1 follow-up checks are written down as non-blocking notes rather than treated as plan blockers.

Notes:
- V1 was reopened on 2026-04-07 because fresh-save manual testing contradicted the old surface/control close-out.
- That reopened follow-through is now honestly reclosed: `.userdata/dev-harness/harness_runs/20260419_141422/` shows the real McWilliams Zone Manager create/name/save/reopen packet for `Basecamp: Locker`, and the reported type-mismatch did not reproduce on that live seam.
- `dirty-trigger follow-through` was the final previously landed V1 chunk, not the name of the whole feature.
- Locker candidate scanning now uses sorted locker tiles so debug/state summaries stay deterministic enough for dirty-trigger tracking and tests.
- If later code or testing disproves any bundled V1 task, reopen that slice instead of pretending only the surface changed.

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

Notes:
- Canonical contract lives at `doc/locker-zone-v3-reopen-packet-v0-2026-04-21.md`.
- Locker Zone V3 now closes honestly on the current tree and hands the baton to the next camp-side packet instead of pretending one more polite missing row still exists.
- The first landed V3 lane is intentionally narrow: local temperature nudges shirt/vest-slot torso+arm outerwear toward warmer gear in cold weather and lighter gear in hot weather.
- The next landed V3 lane is still narrow: pants-slot legwear now distinguishes shorts-like coverage from full-length coverage so cold weather prefers full-length legwear and hot weather prefers shorts-like legwear.
- The newly landed seasonal slice stays bounded too: at moderate temperatures the same outerwear and legwear seams now still bias winter gear in winter and lighter gear in summer, on both the planning seam and the real locker service path.
- The newly landed weather-sensitive slice stays bounded too: rainy moderate-weather service now deliberately prefers rainproof outerwear over generic winter outerwear on both the planning seam and the real locker service path.
- The bounded per-NPC nuance is the worker-specific wardrobe-preservation layer already living on the same deterministic seam: the real locker service path now keeps one-piece suits, dresses, cheongsams, footed jumpsuits, waders, and similar specialized garments unless the replacement set preserves the worker's lost coverage or built-in footwear.
- Focused deterministic validation passed on the current tree via `make -j4 tests` and `./tests/cata_test "[camp][locker]"`.
- No Josef-specific follow-up is currently needed for this closed V3 packet; any later human-only checks should batch with a larger camp-side packet instead of becoming a tiny blocker.
- The `antarvasa` return-to-locker outcome is currently accepted as the honest one-item-per-slot pants policy: keep the best current pants item for comparison, and return extra pants-slot duplicates to the locker when a hot/cold swap lands.
- The slice stayed bounded: no Smart Zone Manager sprawl, no basecamp-automation rewrite, and no per-NPC fashion-chaos scripting.

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

Notes:
- Canonical packet now lives at `doc/basecamp-ai-capability-audit-readout-packet-v0-2026-04-21.md`.
- The packet now exists on the current tree and closes honestly: the current Basecamp AI surface is mostly deterministic craft-request plus board/job routing, while the still prompt-shaped layer is mainly snapshot/planner packaging rather than core spoken command interpretation.
- The audit stayed bounded: no fresh Basecamp implementation work, no prompt externalization rewrite, no Smart Zone Manager spillover, and no Locker Zone V3 reopen.
- This checkpoint now hands the baton to the greenlit live-probe packet instead of pretending the audit is still missing.

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

Notes:
- Canonical contract lives at `doc/live-bandit-basecamp-playtesting-feasibility-probe-v0-2026-04-21.md`.
- Fresh current-build startup proof now lives under `.userdata/dev/harness_runs/20260422_002122/`, with window title/build head `5af2fb80d8-dirty`, `version_matches_repo_head = true`, and `version_matches_runtime_paths = true`.
- The bounded live restage probe under `.userdata/dev-harness/harness_runs/20260422_002329/` now proves one real intentional bandit setup seam on the current tree: named NPC debug spawn via `}` -> `s` -> `p`, filter `bandit`, then confirm, producing hostile `Stefany Billings, Bandit` live on screen.
- The current honest verdict is narrow rather than theatrical: bounded current-build bandit + Basecamp live playtesting is practical now, but reviewer-clean packaged overmap-bandit scenario/control footing is still absent from the harness tree.

---

## Live bandit + Basecamp playtest packaging/helper packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One bounded helper or first-class harness scenario exists for current-build live bandit + Basecamp restage on real existing Basecamp footing.
- [x] The helper packages the already-proved named-bandit restage seam instead of relying on remembered debug-menu choreography alone.
- [x] One fresh live run proves the helper can reach the intended setup repeatably and leaves reviewer-readable artifacts.
- [x] The packet says plainly what remains manual or ugly, if anything, instead of laundering it into magic.
- [x] The slice stays bounded and does not widen into fresh mechanics, zoning-mechanics reopen, encounter/readability judgment, or another generic feasibility lap.

Notes:
- Canonical contract lives at `doc/live-bandit-basecamp-playtest-packaging-helper-packet-v0-2026-04-22.md`.
- The landed first-class helper is `tools/openclaw_harness/scenarios/bandit.basecamp_named_spawn_mcw.json`, packaging the named-NPC spawn seam on reused McWilliams/Basecamp footing instead of relying on remembered chat choreography.
- Fresh current-build proof now lives under `.userdata/dev-harness/harness_runs/20260422_132353/`, with `window_title = Cataclysm: Dark Days Ahead - 7ab535f0c7`, `version_matches_repo_head = true`, and `version_matches_runtime_paths = true`.
- Reviewer-readable artifacts from that run include `filter_bandit_template.after.{png,screen_text.txt}` showing the filtered `bandit` menu entries and `post_spawn_settle.after.{png,screen_text.txt}` showing `Joshua Wilkes, Bandit gets angry!`.
- Remaining rough edges stay explicit rather than hidden: the helper still uses the debug named-NPC spawn surface, still depends on the captured McWilliams fixture, the generic artifact verdict is screen-first rather than debug-log-smart, and probe cleanup still leaves a live game window to close.
- Validation stayed proportionate to the slice: forced fresh tiles rebuild via `make -B -j4 TILES=1 cataclysm-tiles`, then the packaged live probe via `python3 tools/openclaw_harness/startup_harness.py probe bandit.basecamp_named_spawn_mcw`, with no new deterministic tests because no gameplay code changed.

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

Notes:
- Canonical contract lives at `doc/bandit-basecamp-first-pass-encounter-readability-packet-v0-2026-04-22.md`.
- Fresh current-build proof lives under `.userdata/dev-harness/harness_runs/20260422_144921/` and uses the bounded live readability helper `tools/openclaw_harness/scenarios/bandit.basecamp_first_pass_readability_mcw.json`.
- The immediate read is weak and slightly puppet-show-ish: the spawn mostly lands as one more purple nearby-NPC name plus old right-panel clutter, so the player does not get a strong first-pass spatial read on why this person is the threat.
- The encounter is still mechanically real and dangerous, not fake theater: within eight turns the same run records `Heath Griffith, Bandit gets angry!`, taunts, safe-mode survivor ping, gunfire, and the deaths of Katharina Leach and Robbie Knox.
- The honest close-out verdict is therefore narrow and useful: real pressure exists, but first-pass readability is weak/confusing enough that the right next step is the playtest-kit surface-cleanup packet rather than another setup/readability feasibility loop.

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

Notes:
- Canonical contract lives at `doc/bandit-basecamp-playtest-kit-packet-v0-2026-04-22.md`.
- The closed `v0` packet now packages the thin repeatable fast-reload footing honestly instead of pretending it should keep growing.
- `python3 tools/openclaw_harness/startup_harness.py repeatability bandit.basecamp_named_spawn_mcw` is now the bounded repeatability/reporting seam for this helper path, writing `repeatability.report.json`, `repeatability.raw_runs.json`, and `repeatability.summary.txt` under the summary run directory.
- Fresh live evidence under `.userdata/dev-harness/harness_runs/20260422_151547/` ran the packaged helper three times on the same McWilliams footing, recorded stable filtered-bandit menu proof on all three runs, auto-terminated the launched game each time, and exposed the still-useful rough edge honestly: the right-panel anger line only OCR-matched on one run, so later discussion can talk about capture sensitivity instead of pretending the generic artifact verdict answered everything.
- The purpose-built fast-reload pack is now the thin alias pair `tools/openclaw_harness/fixtures/saves/live-debug/bandit_basecamp_playtest_kit_v0_2026-04-22/` plus `tools/openclaw_harness/fixtures/profiles/live-debug/bandit_basecamp_playtest_kit_v0_2026-04-22/`, both reusing the captured McWilliams live-debug footing through manifest-only `source_fixture` / `source_snapshot` indirection instead of another giant copied save blob.
- The first named scenario family on that pack is now:
  - `tools/openclaw_harness/scenarios/bandit.basecamp_playtestkit_restage_mcw.json`
  - `tools/openclaw_harness/scenarios/bandit.basecamp_playtestkit_readability_mcw.json`
- Fresh live proof on the new pack aliases lives under `.userdata/dev-harness/harness_runs/20260422_152650/` and `.userdata/dev-harness/harness_runs/20260422_152819/`, showing that the fast-reload alias footing reaches both the base hostile-restage seam and the short-horizon readability capture path while still auto-cleaning the launched game.
- The packet now says the rough edges plainly inside the aux doc: captured-save footing, named-NPC debug spawn dependency, screen-first evidence instead of `debug.log` mythology, and mixed immediate anger OCR.
- The packet also stops where it should: one thin alias pack plus two named scenarios, with richer prepared fixtures handed to `v1` and broader scenario surgery handed to `v2`.
- Non-goal reminder: no fresh bandit mechanics, no zoning reopen, no generic custom-map framework, and no infinite scenario sandbox.

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

Notes:
- Canonical contract lives at `doc/bandit-basecamp-playtest-kit-packet-v1-2026-04-22.md`.
- The current tree now closes this packet honestly on top of the closed thin `v0` footing instead of leaving the method row half-open.
- The landed fixture pair stays `bandit_basecamp_prepared_base_v1_2026-04-22` and `bandit_basecamp_clairvoyance_v1_2026-04-22`, with the clairvoyance pack now reusing the prepared-base alias instead of carrying its own copied save payload.
- `tools/openclaw_harness/startup_harness.py` now resolves fixture-manifest `save_transforms` and applies the bounded `player_mutations` restage helper during fixture install; `startup.result.json` / `probe.report.json` surface the applied transform under `fixture_install.applied_save_transforms`.
- Fresh current-build closeout proof lives under `.userdata/dev-harness/harness_runs/20260422_172658/`, and the installed/post-load save still serializes both debug clairvoyance mutations in `traits`, `mutations`, and `cached_mutations`.
- Narrow validation for the closeout slice passed via `python3 -m py_compile tools/openclaw_harness/startup_harness.py`, `python3 tools/openclaw_harness/startup_harness.py install-fixture bandit_basecamp_clairvoyance_v1_2026-04-22 --profile andi-v1-check --fixture-profile live-debug --replace`, and `python3 tools/openclaw_harness/startup_harness.py probe bandit.basecamp_clairvoyance_audit_mcw`.
- Audit honesty note still stands: the run folders contain screenshots and load-state metadata, but the generic harness artifact verdict for these screen-first audits still comes back `inconclusive_no_new_artifacts`.

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

Notes:
- Canonical contract lives at `doc/bandit-live-world-control-playtest-restage-packet-v0-2026-04-22.md`.
- This packet absorbs the useful remaining helper/tooling work from `Bandit + Basecamp playtest kit packet v2` instead of deleting it, but helper breadth no longer counts as the headline product by itself.
- The first bounded live owner seam now exists in `src/bandit_live_world.{h,cpp}` plus `overmap_global_state.bandit_live_world`: tracked `bandit_camp`, `bandit_work_camp`, `bandit_cabin`, `mx_looters`, and `mx_bandits_block` spawns claim into one saveable owner ledger at `map::place_npc` time with explicit site/member/spawn-tile records.
- Narrow validation for the owner-ledger slice passed earlier via touched-object compile of `obj/{bandit_live_world,mapgen,overmapbuffer,savegame}.o` plus a standalone filtered Catch binary for `tests/bandit_live_world_test.cpp`.
- The current tree now adds the first bounded control seam on top of that ledger: `bandit_live_world::plan_site_dispatch(...)` / `apply_dispatch_plan(...)` build one real scout dispatch from owned members, and `overmap_npc_move()` can hand those selected NPCs a normal `NPC_MISSION_TRAVELLING` overmap route toward a nearby player target instead of leaving the owner as post-hoc bookkeeping only.
- That dispatch seam now derives outgoing capacity from the live remaining roster instead of folklore counts: site-backed camps keep one member home by rule, micro-sites can still commit their full tiny roster, and `update_member_state(...)` shrinks both site headcount and per-spawn-tile headcount when deaths/missing results land.
- The owner ledger now also persists one active outing summary (`active_group_id`, `active_target_id`, `active_member_ids`) and can apply a bounded handoff return packet back onto those exact owned members after save/load without folklore-reconstructing which bodies went.
- The current tree now also has a first bounded live aftermath observer: `overmap_npc_move()` watches active owned outing members for real `local_contact`, `dead`, and bounded home-return observations, and `bandit_live_world::resolve_active_group_aftermath(...)` converts fully resolved observations back into the existing exact-member return-packet seam instead of leaving the saved outing summary inert.
- Fresh narrow validation for the reserve/writeback plus live-aftermath hook slice passed via touched-object compile of `obj/tiles/bandit_live_world.o`, touched-object compile of `obj/tiles/do_turn.o`, exact current-build compile of `tests/bandit_live_world_test.cpp`, and `git diff --check`.
- The nearby-restage helper surface has now widened in one bounded, reviewer-visible way: `tools/openclaw_harness/startup_harness.py` supports `player_mutations`, `player_near_overmap_special`, and the new `seed_overmap_special_near_player` manifest transform, the current nearby fixture alias is `bandit_live_world_nearby_camp_v0_2026-04-22`, and the paired scenario is `tools/openclaw_harness/scenarios/bandit.live_world_nearby_camp_mcw.json`.
- `tools/openclaw_harness/startup_harness.py` also now has a distinct `handoff` mode that leaves a packaged setup alive and writes `handoff.report.json` / `handoff.pid` instead of always auto-cleaning on success.
- Fresh live proof now closes the calm same-site follow-through question too: fixture `tmp_bandit_live_world_post_writeback_snapshot_2026-04-23` starts with the exact scout already written back home and no active outing, while continuation probe `.userdata/dev-harness/harness_runs/20260423_082832/` advances `1860` turns on the original basecamp footing and re-arms the same owned site with `active_group_id = overmap_special:bandit_camp@140,51,0#dispatch`, `active_target_id = player@140,41,0`, `active_member_ids = [4]`, and member `4` back in `state = outbound` with `last_writeback_summary = dispatch scout toward player@140,41,0`.
- The moved-player McWilliams variant was replaced with an honest seeded-site helper: fixture install now keeps the player/basecamp footing at `om_pos [140, 41, 0]` and copies a real `bandit_camp` footprint onto `target_abs_omt [140, 51, 0]`, about `10 OMT` away in the same overmap.
- Fresh narrow validation for that replacement passed via `python3 -m py_compile tools/openclaw_harness/startup_harness.py`, `python3 tools/openclaw_harness/startup_harness.py install-fixture bandit_live_world_nearby_camp_v0_2026-04-22 --profile andi-nearby-seed-check --fixture-profile live-debug --replace`, and direct placement audit showing the added `bandit_camp` footprint at `(140..141, 51..52)` in `o.0.0.zzip`.
- Fresh current-build nearby-restage proof now lives under `.userdata/dev-harness/harness_runs/20260422_205630/` and `.userdata/dev-harness/harness_runs/20260422_205658/`; the probe run auto-cleaned with `cleanup.status = terminated`, the handoff run stayed alive with `cleanup.status = deferred_handoff`, and grep over those run dirs no longer finds the old `game::validate_camps()` / `invalid map position` load error.
- Fresh current-build corrected-anchor ownership proof now also lives under `.userdata/dev-harness/harness_runs/20260422_224132/`: immediate save on the corrected nearby bootstrap serialized owned site `overmap_special:bandit_camp@140,51,0` with `headcount: 14`, the full nearby footprint, and 14 explicit claimed members in `dimension_data.gsav`.
- The restage seam is now honest enough to stop being the blocker; the open product bar has moved forward to real overmap/bubble interaction and local writeback on this nearby seeded setup.
- Fresh reviewer-clean perf evidence now exists on the same nearby basecamp footing via `.userdata/dev-harness/harness_runs/20260423_004225/`, `.userdata/dev-harness/harness_runs/20260423_004253/`, `.userdata/dev-harness/harness_runs/20260423_004349/`, and `.userdata/dev-harness/harness_runs/20260423_012819/`: single-turn `566.692 ms`, wait-120 `233.168 ms`, cadence-1860 `234.460 ms` avg with `236.622 ms` max batch-turn cost (`1.009x` spike ratio), and stress-4200 `234.065 ms` avg with `238.327 ms` max batch-turn cost (`1.018x` spike ratio).
- Fresh current-build end-to-end writeback proof now also exists on the real nearby setup via the copied-save continuation chain `.userdata/dev-harness/harness_runs/20260423_042618/` -> `20260423_054050/` -> `20260423_055255/`: the copied `7200`-turn save pinned the honest stuck `local_contact` seam, the first moved-away continuation with only `1860` turns still left the outing active, and the second moved-away continuation with `3600` turns resolved member `4` back to `state = at_home` with `last_writeback_summary = return withdrawn from player@140,41,0` while clearing `active_group_id`, `active_target_id`, and `active_member_ids`.
- Fresh current-build dirty later-world disturbance proof now also exists on the same nearby owned setup via `.userdata/dev-harness/harness_runs/20260423_194416/`: raw-local-contact continuation `tmp.bandit_owned_site2_raw_local_contact_kill_probe` killed the exact scout, advanced `10` turns, and saved the owned site with `headcount = 13`, member `4` as `state = missing` with `last_writeback_summary = return broken from player@140,41,0 (missing)`, home `spawn_tile [3371,1230,0]` at `headcount = 0`, and the next active outing already rotated to `active_member_ids = [5]` instead of snapping back to the old full roster.
- That closes the packet honestly on current build: the nearby owned-site seam now has real bootstrap, dispatch, live contact, exact-member writeback, calm same-site re-dispatch, dirty loss/missing follow-through, manual handoff support, and the reviewer-clean perf packet without widening into a bigger hostile-human empire.

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

Notes:
- Canonical contract lives at `doc/multi-site-hostile-owner-scheduler-packet-v0-2026-04-22.md`.
- First deterministic proof lives in `tests/bandit_live_world_test.cpp`: `bandit live world keeps several hostile sites independent across save and writeback` builds three claimed hostile sites (`bandit_camp`, `bandit_work_camp`, and `mx_bandits_block`), gives all three independent active outings/marks/pressure, round-trips through JSON save/load, applies one site's loss/writeback, and asserts the other active sites stay untouched.
- Earlier deterministic validation passed via `make -j4 TILES=1 SOUND=1 LOCALIZE=0 LINTJSON=0 ASTYLE=0 TESTS=0 obj/tiles/bandit_live_world.o`, `./tests/cata_test "[bandit][live_world]"`, and `./tests/cata_test "[bandit][live_world][multi_site]"` (`65 assertions in 1 test case`).
- Fresh current-build live scheduler proof now lives at `.userdata/dev-harness/harness_runs/20260424_003005/`: disposable scenario `tmp.bandit_multi_site_two_site_dispatch_probe_1860` starts from two claimed nearby hostile owners, disables safe mode, advances `1860` turns across the `30_minutes` scheduler cadence, and saves two independent active player-pressure records at once.
- That saved world has `overmap_special:bandit_camp@140,51,0` at `headcount = 14` with member `4` `state = outbound`, `active_group_id = overmap_special:bandit_camp@140,51,0#dispatch`, `active_target_id = player@140,43,0`, and remembered mark `player@140,43,0`; separately, `overmap_special:bandit_camp@140,44,0` stays at `headcount = 7` with member `18` `state = local_contact`, `active_group_id = overmap_special:bandit_camp@140,44,0#dispatch`, the same player target, and its own remembered mark/pressure.
- The scheduler-side behavior change is intentionally bounded: `steer_live_bandit_dispatch_toward_player()` now counts existing player pressure and permits at most `2` simultaneous player-pressure outings rather than returning after the first valid site; that proves overlap without teaching the sites a coalition brain.
- Fresh narrow validation for the final live proof passed via `make -j4 TILES=1 SOUND=1 LOCALIZE=0 LINTJSON=0 ASTYLE=0 TESTS=0 obj/tiles/do_turn.o cataclysm-tiles`, `python3 -m py_compile tools/openclaw_harness/startup_harness.py`, `python3 tools/openclaw_harness/startup_harness.py list-scenarios`, `git diff --check`, and the copied-save inspection from `.userdata/dev-harness/harness_runs/20260424_003005/saved_world/McWilliams`.
- This closes the packet honestly enough to move the active lane to `Hostile site profile layer packet v0`; the point was small independent hostile ownership, not reopening the whole hostile-human architecture debate.

---

## Hostile site profile layer packet v0

Status: CLOSED / CHECKPOINTED

Success state:
- [x] One shared hostile owner/cadence/persistence substrate exists with explicit profile rules instead of hardcoding everything to current bandit-camp assumptions.
- [x] At minimum one camp-style profile and one smaller hostile-site profile can coexist on that substrate reviewer-cleanly.
- [x] Dispatch, threat posture, return-clock, and writeback differences are profile-driven and readable rather than site-kind spaghetti.
- [x] The multi-site scheduler can consume those profiles without regressing the already-honest bandit live-owner footing.
- [x] The packet stays bounded: no giant faction-AI framework, no singleton stalker implementation, and no broad diplomacy/social-horror widening.

Notes:
- Canonical contract lives at `doc/hostile-site-profile-layer-packet-v0-2026-04-22.md`.
- This packet sits directly behind `Multi-site hostile owner scheduler packet v0` and is the bridge from one-family bandit machinery to a reusable hostile-site substrate.
- Current implementation lives in `src/bandit_live_world.{h,cpp}` as explicit `hostile_site_profile` state/rules; the side-by-side deterministic proof lives in `tests/bandit_live_world_test.cpp` as `bandit live world dispatch rules are driven by hostile site profile`.
- Fresh validation: `make -j4 tests`, `./tests/cata_test "[bandit][live_world][profile]"`, `./tests/cata_test "[bandit][live_world]"`, and focused `git diff --check` passed on the current dirty tree.
- `Writhing stalker` stays out of this slice on purpose; the goal here is the profile layer itself, not the first singleton proof.

---

## Cannibal camp attack-not-extort correction v0

Status: CHECKPOINTED / DONE FOR NOW.

- [x] Cannibal-camp local pressure never opens the bandit shakedown/pay-demand surface.
- [x] Favorable cannibal local contact becomes attack-to-kill / lethal ambush pressure.
- [x] Bad cannibal local conditions can still hold off, stalk, probe, or abort instead of becoming magically suicidal.
- [x] Bandit shakedown/pay/fight behavior still passes unchanged.

Canonical contract lives at `doc/cannibal-camp-attack-not-extort-correction-v0-2026-04-24.md`.

Notes:
- Current implementation lives in `src/bandit_live_world.cpp`: cannibal-camp local gate decisions bypass `open_shakedown`, favorable contact becomes `attack_now` / `combat_forward`, report text includes `profile=cannibal_camp`, and `build_shakedown_surface(...)` has a cannibal-profile guard.
- Fresh validation passed via `make -j4 tests`, `./tests/cata_test "[bandit][live_world][cannibal]"`, `./tests/cata_test "[bandit][live_world][shakedown]"`, and `git diff --check`.

## Cannibal camp first hostile-profile adopter packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One honest cannibal-camp hostile profile runs on the shared hostile-site substrate instead of faking reuse through bandit-only assumptions.
- [x] Cannibal-camp cadence, roster, pressure, dispatch, and writeback rules are explicit enough that later behavior can be reasoned about without folklore.
- [x] Cannibal-camp owned sites can coexist with bandit-owned sites without coalition nonsense or accidental shared-state corruption.
- [x] If the current tree lacks a real cannibal-camp site family, the packet first lands one **rare dedicated cannibal-camp mapgen/spawn anchor** instead of attaching the new profile to vapor.
- [x] That first anchor is allowed to derive from current bandit-camp structure, but its names, loadouts, and environmental dressing are explicit enough that review can tell it is the cannibal variant rather than a random runtime mutation.
- [x] The slice stays bounded: no writhing-stalker singleton behavior, no giant lore/diplomacy expansion, no every-hostile-family-at-once detour, and no silent default bandit-camp -> cannibal-camp conversion rule.

Notes:
- Canonical contract lives at `doc/cannibal-camp-first-hostile-profile-adopter-packet-v0-2026-04-22.md`.
- The current tree now closes this packet honestly: `src/bandit_live_world.{h,cpp}` carries explicit `cannibal_camp` site/profile ownership, cannibal NPC templates can claim the live-world substrate, and `tests/bandit_live_world_test.cpp` proves a cannibal camp and bandit camp coexist across dispatch/writeback serialization without shared-state mush.
- The dedicated rare anchor now exists through `cannibal_camp` overmap terrain/special/mapgen/palette data, `cannibal_camp` faction ownership, cannibal NPC templates/classes, and bounded cannibal-camp item groups using existing cannibal food/weapons/remains footing.
- Fresh validation passed via JSON parsing for the new data files, `make -j4 tests`, `./tests/cata_test "[bandit][live_world][cannibal]"`, `./tests/cata_test "[bandit][live_world]"`, and `git diff --check`.
- `Writhing stalker` remains parked one step longer and should be revisited later as the first singleton adopter after the profile layer exists.

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

Notes:
- Canonical contract lives at `doc/bandit-approach-stand-off-attack-gate-packet-v0-2026-04-22.md`.
- This first bandit-robbery packet now closes behind the already-frozen hostile-site stack; the next active packet is the player-present shakedown pay-or-fight surface.
- `src/bandit_live_world.{h,cpp}` carries the deterministic local gate surface: `local_gate_input`, `local_gate_decision`, `local_gate_posture`, `choose_local_gate_posture(...)`, and `render_local_gate_report(...)`.
- Fresh deterministic proof in `tests/bandit_live_world_test.cpp` covers all six v0 postures from an active owned outing: `stalk`, `hold_off`, `probe`, `open_shakedown`, `attack_now`, and `abort`, with reviewer-readable report coverage for the rolling-travel `attack_now` / `combat_forward` branch.
- Fresh live Basecamp stand-off proof lives under `.userdata/dev-harness/harness_runs/20260424_035742/`: the current-build probe advances the claimed nearby-owned-site/Basecamp save across dispatch, and saved-world inspection shows member `4` outbound with `last_writeback_summary = "dispatch hold_off toward player@140,41,0 via goal@140,43,0"`, so the group holds readable stand-off instead of collapsing onto the player/Basecamp tile.
- Fresh live rolling-travel proof lives under `.userdata/dev-harness/harness_runs/20260424_034924/`: the current-build probe advances the claimed nearby-owned-site road-travel save across dispatch, and saved-world inspection shows member `4` outbound with `last_writeback_summary = "dispatch attack_now toward player@140,42,0 via goal@140,42,0"`; the matching NPC save has `goalx/goaly/goalz = 140/42/0` and an overmap path beginning at `[140,42,0]`, so the same local gate chooses attack-forward pressure on the harnessed travel seam without opening the pay-or-fight UI.
- Fresh validation passed via `make -j4 TILES=1 SOUND=0 LOCALIZE=0 LINTJSON=0 ASTYLE=0 TESTS=0 obj/tiles/do_turn.o obj/tiles/version.o cataclysm-tiles`, `./tests/cata_test "[bandit][live_world][approach_gate]"`, `python3 tools/openclaw_harness/startup_harness.py probe tmp.bandit_rolling_travel_attack_gate_probe_1860`, `python3 tools/openclaw_harness/startup_harness.py probe tmp.bandit_basecamp_standoff_gate_probe_1860`, and `git diff --check -- src/do_turn.cpp src/bandit_live_world.cpp tests/bandit_live_world_test.cpp tools/openclaw_harness/scenarios/tmp.bandit_rolling_travel_attack_gate_probe_1860.json`.
- Radio warfare and `Writhing stalker` stay parked for later.

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

Notes:
- Canonical contract lives at `doc/bandit-shakedown-pay-or-fight-surface-packet-v0-2026-04-22.md`.
- This packet sits directly behind `Bandit approach / stand-off / attack-gate packet v0`.
- The deterministic surface helper exists on the owned-outing seam: `build_shakedown_surface(...)` consumes an `open_shakedown` gate decision and explicit reachable-goods pool, then reports a robbery bark, painful demanded toll, `pay` and `fight` availability, Basecamp/off-base pool reach, and rolling-travel bypass.
- Current live Basecamp-side proof exists under `.userdata/dev-harness/harness_runs/20260424_053502/` for the `fight` branch and `.userdata/dev-harness/harness_runs/20260424_054046/` for the `pay` branch.  The menu opens from real local contact with `Bandit shakedown`, `Reachable goods: 45134`, `Demanded toll: 15797`, `p Pay the demanded goods`, and `f Fight`; fighting reports `You refuse the shakedown.  The bandits come at you.`, while paying reports `You surrender goods worth about 18390 to the bandits.` and saves member `4` back `at_home` with `last_writeback_summary = "shakedown_surface paid toll=18390 demanded=15797 reachable=45134"`.
- Current live off-base reach proof exists under `.userdata/dev-harness/harness_runs/20260424_070845/` using `tmp.bandit_offbase_shakedown_fight_probe` on non-road/non-Basecamp footing at `player@140,54,0`.  The current-build artifact reports `basecamp_inventory=no`, `vehicle_inventory=yes`, `reachable_goods=3211`, `demanded_toll=1124`, explicit `pay` / `fight`, and `pool includes player, companion, and current vehicle goods only`, so the surface excludes remote Basecamp goods outside Basecamp/camp footing.

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

Notes:
- Canonical contract lives at `doc/bandit-aftermath-renegotiation-writeback-packet-v0-2026-04-22.md`.
- This packet sits directly behind `Bandit shakedown pay-or-fight surface packet v0`.
- The specific wanted beat is frozen here: if bandits kill a Basecamp defender and the local threat read drops, they may reopen once from a stronger position with a higher toll — and the player still gets a fresh choice to pay or fight again.
- Current deterministic layer now persists shakedown aftermath on `site_record`, records paid/fought outcomes, abstract loot, defender/bandit losses, anger/caution, and a one-use reopen latch; `[bandit][live_world][shakedown]` proves the raised-demand surface, direct cooled-demand report surface, and active-return-packet bandit-loss cooling seam.
- Live proof is deliberately split by evidence class: current-build shakedown runs reach the first fight; artificial continuation `.userdata/dev-harness/harness_runs/20260424_142054/` proves the defender observer can persist `fight_defender_loss`, `shakedown_defender_losses=2`, `shakedown_reopen_available=true`, and a missing active member without claiming a natural defender-kill trigger; isolated copied-save proof `.userdata/dev-harness/harness_runs/20260424_143107/` proves exact active member `4` loss writes back as `fight_bandit_loss`, clears the active outing, increments `shakedown_bandit_losses=1` / `shakedown_caution=1`, and collapses remembered pressure.
- The natural live defender-kill trigger remains a known harness/playthrough gap for the later extortion audit packet, not a reason to keep this aftermath packet open or to build a broad harness empire here.

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

Notes:
- Canonical contract lives at `doc/bandit-extortion-at-camp-restage-handoff-packet-v0-2026-04-22.md`.
- Named scenario: `tools/openclaw_harness/scenarios/bandit.extortion_at_camp_standoff_mcw.json`.
- Reviewer probe command: `python3 tools/openclaw_harness/startup_harness.py probe bandit.extortion_at_camp_standoff_mcw`.
- Manual handoff command: `python3 tools/openclaw_harness/startup_harness.py handoff bandit.extortion_at_camp_standoff_mcw`.
- Probe evidence: `.userdata/dev-harness/harness_runs/20260424_145617/` (run before the scenario was renamed from `bandit.extortion_at_camp_shakedown_mcw`) advanced from the honest claimed nearby-owned-site Basecamp fixture and logged `local_gate site=overmap_special:bandit_camp@140,51,0 active_group=overmap_special:bandit_camp@140,51,0#dispatch target=player@140,41,0 posture=hold_off ... basecamp_or_camp=yes`, with `live_dispatch_goal=140,43,0`.
- Handoff evidence: `.userdata/dev-harness/harness_runs/20260424_153309/` ran the renamed scenario in `mode = handoff`, wrote `handoff.report.json`, left `cleanup.status = deferred_handoff` with live pid `79412`, and captured the same controlled-site stand-off log line plus `live_dispatch_goal=140,43,0`.  The OCR screen is messy save/temperature text, so the decisive evidence class is the handoff report plus artifact log, not spatial readability.
- Caveat: both probe and handoff reports carry `inconclusive_version_mismatch` because the visible runtime binary did not match the later repo-head/runtime-path diff.  This packet is closed for setup/handoff repeatability; a future Josef-facing playthrough packet should rebuild/rerun if strict current-source runtime freshness matters.

---

## Bandit extortion playthrough audit + harness-skill packet v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] One named audit/playthrough surface exists for the full Basecamp extortion chain rather than only for the first setup moment.
- [x] The packaged flow can cover first demand, fight branch, defender-loss reopen, raised-price second demand, and second `pay` versus `fight` reconsideration clearly enough for review.
- [x] Reports separate screen, tests, and artifacts/logs cleanly enough that later debugging does not devolve into folklore.
- [x] The relevant harness skill/docs are updated so another agent can discover the named paths and run them without archaeological guessing.
- [x] The slice stays bounded: no fake total-automation empire and no feature-redesign side quest hiding inside audit packaging.

Notes:
- Canonical contract lives at `doc/bandit-extortion-playthrough-audit-harness-skill-packet-v0-2026-04-22.md`.
- Named scenario surface now consists of `bandit.extortion_at_camp_standoff_mcw`, `bandit.extortion_first_demand_fight_mcw`, `bandit.extortion_first_demand_pay_mcw`, and `bandit.extortion_reopened_demand_mcw`.
- Current-runtime reopened-demand proof is stable scenario run `.userdata/dev-harness/harness_runs/20260424_174913/`: rebuilt runtime `9e3c14260b-dirty`, `version_matches_repo_head=true`, `version_matches_runtime_paths=true`, screen-visible raised `Demanded toll: 22116`, and artifact `shakedown_surface ... pay_option=yes fight_option=yes ... demanded_toll=22116`.
- The natural redispatch-from-no-active-group probe `.userdata/dev-harness/harness_runs/20260424_170908/` stayed artifact-silent after 6000 turns, so the closed audit path is explicitly tiered: setup stand-off, first-demand pay/fight, deterministic/captured defender-loss state, and controlled reopen-local-contact second demand.  That keeps the packet teachable without pretending the whole combat chain is one fully automated macro.

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

Notes:
- This follow-through lane was already greenlit; it is now closed again instead of parked half-finished.
- Landed order: artifact proof cleanup -> upstream deterministic cleanup -> structured `next=` token follow-through.
- No Josef-specific follow-up checks are currently needed for this closed slice.

---

## LLM-side board snapshot path

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] Routing proof exists on the real `handle_heard_camp_request` path.
- [x] Structured/internal `show_board` emits the richer handoff snapshot.
- [x] Spoken `show me the board` stays on the concise spoken bark path.
- [x] Deterministic evidence for that split is recorded.

Notes:
- Retained here so the closed slice stays visible without drifting back into active work.


---

## Organic bulletin-board speech polish

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] Bulletin-board / camp-job requests can be triggered through natural player-facing phrasing instead of exposed machine wording.
- [x] Ordinary spoken answers no longer expose `job=<id>` / `show_board` / `show_job` style routing tokens.
- [x] Internal routing/debug structure can still exist where needed without leaking into normal in-world speech.
- [x] The visible answer tone sounds rough, practical, and in-world, like poor survivors making it work for another day while the dead and worse roam outside.

Notes:
- Canonical contract lives at `doc/organic-bulletin-board-speech-2026-04-09.md`.
- This lane is now checkpointed instead of active.
- Deterministic cleanup on the current tree keeps ordinary spoken board/job replies free of request ids, and board-status parsing now accepts `what needs making`, `what needs doing`, `got any craft work`, and `show me what needs doing`.
- Fresh narrow proof passed on the current tree via `make -j4 tests` and `./tests/cata_test "[camp][basecamp_ai]"`.
- Proportional live proof exists on the rebuilt current tiles binary in `.userdata/dev-harness/harness_runs/20260419_154244/`: the real camp-assignment seam plus `what needs making` produced `Board's got 1 live and 1 old - 1 x bandages.` with no visible request-id glue.
- The same live packet still lets Robbie chime in as ordinary follower crosstalk on the McWilliams fixture; keep that separate from this closed machine-speech cleanup unless it becomes a fresh visible seam.

---

## Combat-oriented locker policy

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] Future locker behavior strongly supports sensible common guard/combat gear: gloves, belts, masks, holsters, and the usual practical clothing/loadout pieces.
- [x] A bulletin-board / locker-surface bulletproof toggle exists and meaningfully shifts outfit preference toward ballistic gear.
- [x] Ballistic vest and plate handling becomes explicit enough to replace damaged (`XX`) ballistic components sensibly.
- [x] Clearly superior full-body battle/protective suits are preferred when appropriate instead of being split into worse piecemeal junk.
- [x] Future deterministic tests lean more toward combat/guard outfit behavior and less toward endlessly widening exotic garment edge-case law.

Notes:
- Canonical contract lives at `doc/locker-combat-oriented-policy-2026-04-09.md`.
- This lane is now checkpointed instead of active.
- The opening 2026-04-19 audit found the first honest combat-policy seam, and the current tree now has landed narrow slices for explicit `gloves` / `mask` / `belt` / `holster` locker footing, a persisted `Prefer bulletproof gear` locker-policy toggle that shifts body-armor and helmet scoring toward stronger ballistic protection, explicit ablative-plate-aware ballistic vest scoring/replacement behavior, missing-shirt filler suppression under protective full-body suits, and the direct-current comparisons where a superior full-body suit can now also displace weaker currently worn shirts, vests, and body armor while still keeping stronger current ballistic armor.
- The ballistic-maintenance slice is now covered by focused deterministic checks for loaded vs empty vest scoring, damaged insert scoring, same-type healthy-plate upgrades, and the new positive/negative full-body-suit-vs-current-body-armor tradeoffs.
- The final closure audit found one real remaining proof gap, namely end-to-end service evidence for the newly explicit combat-support slots.
- That gap is now closed by `camp_locker_service_equips_common_combat_support_slots`, and focused deterministic validation passed on the current tree via `make -j4 tests`, `./tests/cata_test "camp_locker_service_equips_common_combat_support_slots"`, and `./tests/cata_test "[camp][locker]"`.
- This future direction preserved the current weird-garment safety work instead of replacing it.

---

## Bandit concept formalization follow-through

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] A follow-through packet exists that turns the loose remaining bandit logic into three explicit doc slices: bounty source/harvesting/stockpile rules, cadence/distance/fallback rules, and cross-layer interactions/worked scenarios.
- [x] Those three slices are further decomposed into narrow single-question micro-items so Andi can freeze one law at a time instead of hiding several assumptions inside one paragraph.
- [x] The packet makes the no-passive-decay footing explicit: structural bounty changes via harvesting/exploitation, moving bounty via current activity and collection, threat via real recheck, and camp stockpile via explicit consumption instead of background decay math.
- [x] Each micro-item includes a clear question plus a concrete answer shape, and the packet as a whole includes starter numbers/tables, clear scope/non-goals, and enough worked examples that later implementation planning does not have to rediscover the control law from scratch.
- [x] The result remains conceptualization/backlog work only and does not silently greenlight bandit implementation.

Notes:
- Canonical contract lives at `doc/bandit-concept-formalization-followthrough-2026-04-19.md`.
- This lane is now honestly checkpointed instead of active.
- The follow-through is complete as a 3-package / 31-micro-item docs packet: micro-item 31 (`Invariants and non-goals packet`) is now landed on top of the already-closed starter-number and worked-scenario layers, so later implementation planning has both sanity checks and explicit red lines.
- No bandit implementation is greenlit here; keep this as conceptualization/packaging work only unless later canon explicitly reopens it.

---

## Plan status summary command

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] A user can request a plan readout and see compact numbered `active`, `greenlit`, and `parked` summaries derived from current `Plan.md` canon.
- [x] The greenlit readout preserves execution order, with active first and any bottom-of-stack entries simply appearing last in that numbered list.
- [x] If canon headings are contradictory or missing enough structure to classify cleanly, the command warns instead of inventing certainty.
- [x] The output stays short and Discord-friendly rather than dumping whole roadmap prose.

Notes:
- Canonical contract lives at `doc/plan-status-summary-command-2026-04-20.md`.
- The landed first-pass command seam is `tools/plan_status_summary.py`.
- The intended readout surface is `/plan active`, `/plan greenlit`, `/plan parked`, with optional compact combined `/plan`; the current script accepts those slash-style tokens directly.
- Source of truth stays `Plan.md`; the command does not invent state from chat memory or agent narration.
- Greenlit ordering stays active first, then any queued greenlit items, with bottom-of-stack entries appearing last instead of as a separate printed class.
- Thin or contradictory canon now warns instead of pretending certainty, and the command still keeps explicit fallback behavior when canon is too thin to classify cleanly.

---

## Bandit smoke visibility mark seam v0

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] A bounded smoke-specific adapter exists from current fire/smoke/wind footing, or equivalent deterministic smoke packets, into coarse overmap-readable smoke signal state for bandit logic.
- [x] Generated smoke marks or smoke-born leads feed the current bandit mark-generation / playback / evaluator seams instead of relying on hand-authored smoke lore.
- [x] Deterministic coverage proves the bounded long-range rule honestly: sustained clear-weather smoke stays meaningfully long-range, while weak/brief or degraded-condition smoke stays shorter and fuzzier.
- [x] Reviewer-readable report output exposes the smoke packet and resulting mark/lead path instead of hiding the bridge in debugger soup.
- [x] The slice stays bounded: no light/searchlight adapter, no broad visibility/concealment implementation, no global offscreen smoke sim, and no first 500-turn proof smuggled in.

Notes:
- Canonical contract lives at `doc/bandit-smoke-visibility-mark-seam-v0-2026-04-20.md`.
- This is the first checkpointed visibility promotion from the parked visibility/recon packet, aimed at future 500-turn proof without pretending that proof already exists.
- The current tree now has the bounded smoke seam in `src/bandit_mark_generation.{h,cpp}` and `src/bandit_playback.{h,cpp}`, with deterministic coverage in `tests/bandit_mark_generation_test.cpp` plus `tests/bandit_playback_test.cpp`.
- The key product rule stays frozen here: on clear days, sustained smoke should remain legible from several OMT away and should usually create a bounded `worth scoping out` lead before it implies anything sharper.
- Narrow deterministic validation passed via `make -j4 tests` and `./tests/cata_test "[bandit]"`.

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

Notes:
- Broad synthesis paper / anchor doc: `doc/bandit-overmap-ai-concept-2026-04-19.md`.
- Current scoring sub-item: `doc/bandit-bounty-threat-scoring-guidance-2026-04-19.md`.
- Current heatmap sub-item: `doc/bandit-mark-generation-and-heatmap-model-2026-04-19.md`.
- Current handoff sub-item: `doc/bandit-overmap-to-bubble-handoff-seam-2026-04-19.md`.
- Current visibility sub-item: `doc/bandit-player-basecamp-visibility-and-concealment-2026-04-19.md`.
- Supporting recon note for the visibility item: `doc/bandit-visibility-physical-systems-recon-2026-04-19.md`.
- The cleanup/follow-through packet for the remaining control-law gaps lives at `doc/bandit-concept-formalization-followthrough-2026-04-19.md` and is now checkpointed docs/spec substrate rather than an active lane.
- The broad concept still stays parked as substrate outside the explicitly promoted v0 slices, with smoke now being the next active narrow bridge.

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

Notes:
- Canonical concept-bank doc: `doc/arsenic-old-lace-social-threat-and-agentic-world-concept-bank-2026-04-22.md`.
- This is preserved as parked substrate only, not as permission to widen the current execution stack.
- The strongest near-promotable seeds currently look like alarm states via natural-language yelling, bandits exploiting readable routines, radio-driven information warfare, writhing-stalker pressure, and a first social-camouflage slice.
- The broader long-range families, including agentic NPC goals and interdimensional-traveler motive play, stay buried here until Josef explicitly promotes one bounded packet.

---

## Plan/Aux pipeline helper

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] A small helper can take a proposed item/greenlight and print the contract back for verification before canon files are changed.
- [x] The helper can collect corrections and then classify the item cleanly as active, parked, or bottom-of-stack.
- [x] The helper can update the relevant canon files consistently (`Plan.md`, `TODO.md`, `SUCCESS.md`, `TESTING.md` when needed, plus the auxiliary doc).
- [x] The helper reduces manual file carpentry for already-understood greenlights without bypassing the frozen workflow.
- [x] The helper can optionally generate the Andi handoff packet from the same classified contract.

Notes:
- Canonical contract lives at `doc/plan-aux-pipeline-helper-2026-04-09.md`.
- The suspicion-first tooling audit is now closed, and the current bounded helper path landed at `tools/plan_aux_pipeline_helper.py`.
- Current helper shape now covers spec validation, review-packet preview, explicit correction merge, reviewer-visible snippet emission, optional downstream `andi.handoff.md` output, and bounded in-place patching of known existing canon headings / active-lane anchors.
- Narrow validation passed on a sample spec via `python3 -m py_compile tools/plan_aux_pipeline_helper.py`, `schema`, `show`, `emit`, emitted `andi.handoff.md` review, and `apply` on a temp repo copy.
- Keep this lane closed unless future evidence shows the handoff output or bounded patch path lying about what the canon actually says.
