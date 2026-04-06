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

## Patrol Zone v1

Status: ACTIVE

Success state:
- [x] A Zone Manager patrol zone exists and patrol squares are grouped by 4-way connected clusters.
- [x] Patrol exists as a real camp job with its own priority surface.
- [x] Deterministic planner coverage exists for the reference staffing/topology cases.
- [x] The active patrol roster is chosen at shift boundaries from NPCs with patrol priority > 0.
- [x] On-shift patrol is sticky against routine chores.
- [x] Urgent disruption can break patrol, and reserve backfill works without full-roster reshuffle.
- [x] On-map behavior distinguishes hold-positions vs fixed-loop patrol in the intended simple v1 way.
- [x] Proportional live proof is recorded with separate screen/tests/artifacts reporting.
- [ ] The player-legibility bar is met: guard behavior, uncovered posts, connected-vs-disconnected behavior, and reserve/off-shift state are understandable enough to read in play.
- [x] The result stays explainable as simple v1 patrol rather than quietly turning into smart-zone-manager soup.

Notes:
- Canonical implementation sketch lives in `doc/patrol-zone-v1-patch-plan-2026-04-06.md`.
- Intended order: zone surface + 4-way clustering -> deterministic planner contract -> sticky roster / interrupt whitelist -> on-map hold-vs-loop -> live proof.
- The current packaged live proofs are `patrol.disconnected_live` -> `.userdata/dev-harness/harness_runs/20260406_221126/probe.report.json` and `patrol.connected_live` -> `.userdata/dev-harness/harness_runs/20260406_221548/probe.report.json`.
- The current-binary packet now includes readable staffing-pool and zone-topology companion crops plus a tight `runtime_motion_compare.gif` blink helper, so loop-vs-hold posture no longer depends on artifact logs alone to read.
- The remaining honest gap is the broader player-legibility bar: audit whether uncovered posts and off-shift / reserve state are understandable enough from the improved packet, not just whether loop vs hold can be seen.
- The interrupt whitelist should be nailed down early so the feature does not quietly rot into fake patrol.
- Watch for hallucinations, fake progress, and roadmap prose outrunning code/tests/live proof.

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
- [x] **Locker surface/control task** — `CAMP_LOCKER` exists as a Zone Manager zone, camp locker policy state exists, the player-facing locker policy menu/control exists, and the policy survives save/load.
- [x] **Locker outfitting core task** — representative locker item classification, candidate gathering, score helpers, planning, equip/upgrade behavior, duplicate cleanup, and returning replaced managed gear all exist.
- [x] **Locker maintenance rhythm task** — wake-up dirty, policy-change dirty, new-gear dirty, lost-gear dirty, queue sequencing, and reservation behavior all exist.
- [x] **Locker V1 proof task** — deterministic coverage plus proportional runtime validation for the same downtime/service path are recorded in `TESTING.md`.
- [x] Any Josef-specific V1 follow-up checks are written down as non-blocking notes rather than treated as plan blockers.

Notes:
- V1 is only considered done because the whole bundled task set above is checked.
- `dirty-trigger follow-through` was the final V1 chunk landed, not the name of the whole feature.
- Locker candidate scanning now uses sorted locker tiles so debug/state summaries stay deterministic enough for dirty-trigger tracking and tests.
- If later code or testing disproves any bundled V1 task, reopen V1 immediately.

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
- V3 is not the current delivery target. After the stabilization/harness checkpoint, the repo is waiting for Josef to greenlight the next lane instead of quietly drifting back into locker nuance.
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
