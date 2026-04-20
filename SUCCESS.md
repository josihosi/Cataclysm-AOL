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

## Bandit evaluator dry-run seam v0

Status: ACTIVE / GREENLIT

Success state:
- [ ] A deterministic dry-run evaluator exists for controlled bandit camp inputs.
- [ ] The evaluator always includes `hold / chill` and only emits outward candidates from real compatible leads or current hard state.
- [ ] The first explanation surface shows leads considered, the full candidate board, per-candidate score inputs/final score, veto/soft-veto reasons, and winner versus `hold / chill`.
- [ ] Narrow deterministic coverage exists for the first pure reasoning reference cases.
- [ ] The slice stays bounded: no full autonomous bandit world behavior, no broad scenario playback suite, and no broad persistence architecture are smuggled in.

Notes:
- Canonical contract lives at `doc/bandit-evaluator-dry-run-seam-v0-2026-04-20.md`.
- This is the first promoted implementation slice from the parked bandit concept chain; broader bandit implementation still stays parked outside the explicitly greenlit v0 slices.

---

## Bandit scenario fixture + playback suite v0

Status: GREENLIT / BACKLOG

Success state:
- [ ] Named deterministic bandit scenarios exist for the first reference cases.
- [ ] The playback contract can inspect behavior at multiple checkpoints such as `tick 0`, `tick 5`, `tick 20`, and one longer horizon.
- [ ] The scenario packet can answer whether camps stay idle, investigate smoke, stalk edges, peel off under pressure, or mis-upgrade whole regions from moving clues.
- [ ] The suite stays bounded and does not turn into broad worldgen mutation or live-harness-first theater.

Notes:
- Canonical contract lives at `doc/bandit-scenario-fixture-playback-suite-v0-2026-04-20.md`.
- This queue stays behind the active evaluator seam even if the fixtures sound adjacent.

---

## Bandit perf + persistence budget probe v0

Status: GREENLIT / BACKLOG

Success state:
- [ ] Repeatable cost measurements exist for the named bandit scenarios.
- [ ] Obvious evaluator churn signals such as candidate-count growth, repeated scoring/path checks, or similar waste are visible instead of hidden.
- [ ] Save-size growth has an honest first estimate tied to the actually persisted bandit state shape.
- [ ] The packet can say whether the current design looks cheap enough, suspicious, or clearly too bloated before broader rollout.

Notes:
- Canonical contract lives at `doc/bandit-perf-persistence-budget-probe-v0-2026-04-20.md`.
- This queue stays behind the evaluator seam and playback suite, not beside them.

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
- [x] The result is described reviewer-cleanly as harness/fixture work on existing locker behavior, not as early hackathon feature work.

Notes:
- This lane existed because the shipped `basecamp_dev_manual_2026-04-02` save fixture did not contain a `CAMP_LOCKER` zone even though the live dev save did.
- The repair is the captured harness-owned save fixture `tools/openclaw_harness/fixtures/saves/dev/locker_ready_dev_2026-04-06/`, plus the scenario update that restores the usual `basecamp_dev_manual_2026-04-02` profile snapshot while installing that locker-ready save.
- Latest packaged proof: `python3 tools/openclaw_harness/startup_harness.py probe locker.weather_wait` -> `.userdata/dev-harness/harness_runs/20260406_125056/probe.report.json` with `verdict: artifacts_matched` and separate screen/tests/artifacts reporting.
- The goal was not new locker gameplay; it was to make existing locker behavior probeable and regressible.
- Keep the hackathon-reserved chat/ambient feature lanes clearly separate.

---

## Hackathon runway — stabilization + harness

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
- Important reviewer-facing distinction: the packaged `chat.nearby_npc_basic` and `ambient.weird_item_reaction` scenarios are harness scaffolding only. They are **not** the hackathon feature work for "chat interface over dialogue branches" or the "tiny ambient-trigger NPC model," and must not be described as partial completion of those feature lanes.

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

Status: PARKED / PARTIALLY LANDED

Success state:
- [ ] Seasonal dressing / winter-vs-summer wardrobe logic exists.
- [ ] Weather-sensitive wardrobe choices (coats / blankets / shorts / similar) are handled deliberately rather than by V1/V2 shortcuts.
- [ ] Per-NPC overrides / nuance exist without undoing the simpler V1/V2 deterministic spine.
- [x] Deterministic coverage exists for the V3 behavior that is actually implemented.
- [x] Proportional runtime validation for the currently implemented V3 behavior is recorded in `TESTING.md`.
- [ ] Any Josef-specific follow-up checks are written down as non-blocking notes rather than treated as plan blockers.

Notes:
- V3 is not the current delivery target. The active controlled follow-through packet is now moving through Package 5, so keep V3 parked unless Josef explicitly reopens locker nuance.
- The first landed V3 lane is intentionally narrow: local temperature nudges shirt/vest-slot torso+arm outerwear toward warmer gear in cold weather and lighter gear in hot weather.
- The next landed V3 lane is still narrow: pants-slot legwear now distinguishes shorts-like coverage from full-length coverage so cold weather prefers full-length legwear and hot weather prefers shorts-like legwear.
- Outerwear and the currently landed legwear lane now both have proportional runtime proof on the recorded current-binary / current-save path.
- The `antarvasa` return-to-locker outcome is currently accepted as the honest one-item-per-slot pants policy: keep the best current pants item for comparison, and return extra pants-slot duplicates to the locker when a hot/cold swap lands.
- If Josef explicitly reopens locker work, resume from the remaining unchecked V3 rows instead of pretending the whole lane is already done.
- Do not quietly let V3 nuance undo the simpler V1/V2 deterministic spine.

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
- Thin or contradictory canon now warns instead of pretending certainty, with the current Hackathon parked heading already exercising the thin-canon fallback path.

---

## Bandit overmap AI concept chain

Status: PARKED / CONCEPT CHAIN

Success state:
- [ ] The parked concept packet defines the broad bandit overmap actor model, signal/memory role, threat shape, and overmap-to-bubble intent cleanly enough that later planning no longer has to rediscover the premise from scratch.
- [ ] The broad packet explicitly states that bounty and threat do not passively decay: structural bounty changes through harvesting/exploitation, moving bounty through fresh activity and collection, threat through real recheck, and camp stockpile through explicit consumption.
- [ ] Deterministic bounty/threat scoring guidance exists with explicit camp-ledger inputs, map-mark fields, harvest/collection/recheck rules, and job-scoring logic.
- [ ] Overmap mark-generation and heatmap guidance exists with explicit overmap-only mark creation, recheck/harvest rules, and threat/bounty field update logic on the shared cadence family without passive decay.
- [ ] Bidirectional overmap-to-bubble handoff guidance exists with explicit entry modes, return-path state, collapse-back rules, and reuse of existing pursuit/noise-routing footing where possible.
- [ ] Supporting physical-systems recon exists so the visibility/concealment slice is grounded in existing smoke, light, sound, and weather hooks instead of made-up parallel physics.
- [ ] Player/basecamp visibility and concealment guidance exists with explicit signal sources, environmental filters, bounty/threat interpretation outputs, repetition/reinforcement rules, and player/basecamp exposure-reduction levers.
- [ ] The visibility item explicitly allows fog/mist to suppress sight/light legibility without requiring new fog-based sound dampening rules unless later evidence says otherwise.
- [ ] The broad packet explicitly prevents suspicion spirals by making camp knowledge sparse and camp-owned, ground bounty coarse/finite/non-regenerating, moving bounty actor-driven, and checked/false-lead/harvested memory able to damp repeated interest.
- [ ] The broad packet explicitly resolves basecamp fairness asymmetry by allowing sustained offscreen pressure and stalking, keeping decisive full camp assault player-present only for current scope, and not requiring attack presignaling as the fairness mechanism.
- [ ] The packet explicitly names the current overmap-NPC persistence/travel/companion substrate as reusable footing for stalking and intercept pressure, without pretending the existing need-driven random-NPC policy is already the finished hostile model.
- [ ] The broad packet explicitly resolves handoff identity continuity by making the overmap group itself persistent, carrying only a small anchored-individual slice directly across handoffs, and treating the rest of the group as fungible mission strength.
- [ ] The broad packet explicitly treats smoke/mark destinations as provisional mission leads whose goals can be continued, diverted, shadowed, or aborted by local observations instead of sacred tile commitments.
- [ ] The broad packet explicitly resolves city opportunism under zombie pressure by allowing occasional risky opportunism without requiring direct bandit-versus-zombie tactical simulation, while keeping repeat attractiveness bounded by depleting bounty and sticky threat memory.
- [ ] The broad packet explicitly keeps threat marks sticky enough that bands do not cheaply remote-rewrite a scary area as safe again until they return close enough to genuinely reassess it.
- [ ] The broad packet explicitly resolves multi-camp dogpile behavior by keeping camps mostly independent in v1, allowing occasional overlap without turning it into routine coalition swarming.
- [ ] The broad packet explicitly uses territoriality, distance burden, depletion, sticky threat, and fresh active-pressure penalties to damp repeated multi-camp convergence on the same target region.
- [ ] The whole bandit concept packet becomes coherent enough that it can be reconsidered for promotion from parked concept chain into greenlit backlog without hidden open seams.

Notes:
- Broad synthesis paper / anchor doc: `doc/bandit-overmap-ai-concept-2026-04-19.md`.
- Current scoring sub-item: `doc/bandit-bounty-threat-scoring-guidance-2026-04-19.md`.
- Current heatmap sub-item: `doc/bandit-mark-generation-and-heatmap-model-2026-04-19.md`.
- Current handoff sub-item: `doc/bandit-overmap-to-bubble-handoff-seam-2026-04-19.md`.
- Current visibility sub-item: `doc/bandit-player-basecamp-visibility-and-concealment-2026-04-19.md`.
- Supporting recon note for the visibility item: `doc/bandit-visibility-physical-systems-recon-2026-04-19.md`.
- The cleanup/follow-through packet for the remaining control-law gaps lives at `doc/bandit-concept-formalization-followthrough-2026-04-19.md` and is greenlit doc/spec work only, not code greenlight.
- This broad packet remains parked concept substrate outside the explicitly greenlit v0 evaluator / fixture / perf slices.

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
