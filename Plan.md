# Plan

Canonical roadmap for Cataclysm-AOL.

This file answers one question: **what should the agent advance next?**
It is not a changelog, not a graveyard of crossed-off substeps, and not a place to preserve every historical detour.
Rewrite it as reality changes.

## File roles

- **Plan.md** - canonical roadmap and current delivery target
- **SUCCESS.md** - success-state ledger / crossed-off exit criteria for roadmap items
- **TODO.md** - short execution queue for the current target only
- **TESTING.md** - current validation policy, latest relevant evidence, and pending probes
- **TechnicalTome.md** - durable mechanic notes, not daily state tracking
- **COMMIT_POLICY.md** - checkpoint rules to prevent repo soup

If these files disagree, **Plan.md wins** and the other files should be repaired.

## Working rules for agents

- Do **not** mechanically grab the first unchecked-looking thing from some list.
- Follow the current delivery target below and move it to its **next real state**.
- Josef being unavailable for playtesting is **not** a blocker by itself.
- When a target is waiting on Josef, move to the next best unblocked target.
- If no good unblocked target remains, send Josef a short parked-options note so he can greenlight the next lane; do not just keep revalidating the old packet.
- During the current debug-proof finish stack, failed agent-side proof does **not** close or park implemented code. After the attempt budget, move implemented-but-unproven items to Josef's playtest package and continue the next greenlit debug note.
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

Repo policy remains unchanged: `/Users/josefhorvath/Schanigarten/Cataclysm-AOL` on `dev` is the normal worktree and `josihosi/Cataclysm-AOL` is the real project/release repo. `josihosi/C-AOL-mirror` is green-dot-only.

Detailed contracts, closure evidence, and older checkpoint history belong in `doc/*.md`, `SUCCESS.md`, and git history. Keep this file short enough that the active stack is visible without archaeology.

---

## Closed lane - Basecamp locker armor ranking + blocker removal packet v0

**Status:** CLOSED / CHECKPOINTED

Generic protective/full-body armor ranking and blocker clearing are landed for the camp locker path without special-casing `RM13 combat armor`. Deterministic proof covers superior full-body candidates displacing worse blockers, damaged candidates being rejected without repeated requeue/equip churn, stronger ballistic armor being preserved while a compatible full-body suit is added, and `[camp][locker]` readiness regression coverage.

Canonical contract lives at `doc/basecamp-locker-armor-ranking-blocker-removal-packet-v0-2026-04-26.md`.

---

## Closed lane - Basecamp job spam debounce + locker/patrol exceptions packet v0

**Status:** CLOSED / CHECKPOINTED

Stable-cause camp job chatter debounce landed for camp activity completion, camp request blocked/missing-tool barks, and no-progress request barks. Locker and patrol paths now use typed `[camp][locker]` / `[camp][patrol]` reports with repeated-state compression, preserving first occurrence and changed state visibility without a global message-log rewrite.

Canonical contract lives at `doc/basecamp-job-spam-debounce-exceptions-packet-v0-2026-04-26.md`.

---

## Completed lane - C-AOL debug-proof finish stack v0

**Status:** CLOSED / CHECKPOINTED

Josef explicitly reopened the current C-AOL debug-proof notes on 2026-04-27. The stack has now reached an honest agent-side boundary without leaving failed proof in parked/review-only posture.

- Bandit live signal/site-bootstrap product-proof work reached its honest agent-side boundary: synthetic loaded-map fire/light reaches the running live path and the real player-lit fire bridge is listed in Josef's playtest package as implemented-but-unproven; do not loop on it again without a fresh reopen/material blocker change.
- Smart Zone Manager live layout separation correction reached its honest agent-side boundary: the planner now keeps intended-separate generated zones on separate reserved tiles with deterministic geometry/separation coverage, the failed clean live/UI macro is packaged for Josef as implemented-but-unproven, and the 2026-04-28 current-runtime rerun only added startup/load red evidence (`Dunn has no characters to load`) rather than a fresh product-proof reopen.
- Bandit local standoff / scout return live correction is now fixed/proven on the current live product path: deterministic coverage asserts the pre-local-contact scout timeout, rebuilt current-runtime run `.userdata/dev-harness/harness_runs/20260427_154309/` used the real `wait_action` path, logged `local_gate ... posture=hold_off ... standoff_distance=5 ... live_dispatch_goal=140,46,0`, then logged `scout_sortie: linger limit reached -> return_home`, `scout_sortie: home footprint observed ... pos=(140,51,0)`, and `scout_report: returned -> pressure refreshed`.
- Attempt rule for this stack was observed: attempt 3 happened only after Frau Knackal consultation and a material code-path/instrumentation change.
- **GitHub normal-download release packet v0** should stay held until Schani/Josef decide whether the greenlit harness trust audit/proof-freeze package or release posture is next.

Canonical contract lives at `doc/caol-debug-proof-finish-stack-v0-2026-04-27.md`.

---

## Closed lane - Cannibal camp night-raid behavior packet v0

**Status:** CLOSED / CHECKPOINTED

Josef's 2026-04-28 cannibal-camp slice is checkpointed. Cannibal camps stay hostile-site-shaped, with the landed proof boundary covering pack-size pressure, smoke/light scout-stalk interpretation, darkness/concealment attack windows, sight-exposure hold-off, no bandit shakedown surface, and saved active-group persistence. The optional labeled bandit-control contrast can be reopened later if Josef wants more pay/fight comparison; it is not a closure blocker.

Canonical references:
- imagination source: `doc/cannibal-camp-night-raid-imagination-source-of-truth-2026-04-28.md`
- code audit: `doc/cannibal-camp-night-raid-code-audit-2026-04-28.md`
- contract: `doc/cannibal-camp-night-raid-behavior-packet-v0-2026-04-28.md`
- live matrix: `doc/cannibal-camp-night-raid-live-playtest-matrix-v0-2026-04-28.md`

---

## Closed lane - Bandit camp-map risk/reward dispatch planning packet v0

**Status:** CLOSED / CHECKPOINTED SCOPED LIVE PRODUCT GREEN

The Schani/Josef-greenlit bandit camp-map ecology slice is closed as a scoped live/product checkpoint. Deterministic/code support and the accepted live matrix cover remembered leads, variable roster sizing, no-opening/high-threat holds, dogpile blocking, shakedown/toll-control guardrails, and optional empty-camp sanity for this checkpoint.

Canonical references:
- product source: `doc/bandit-camp-map-ecology-source-of-truth-2026-04-28.md`
- implementation map: `doc/bandit-camp-map-ecology-implementation-map-2026-04-28.md`
- contract: `doc/bandit-camp-map-risk-reward-dispatch-planning-packet-v0-2026-04-28.md`
- Andi lane: `doc/bandit-camp-map-risk-reward-dispatch-andi-lane-v0-2026-04-28.md`
- live matrix: `doc/bandit-live-product-playtest-matrix-v0-2026-04-28.md`

Preserved caveats: the old Multidrop fuel/writeback path remains non-credit evidence, and the wood-source-zone fuel repair is also implemented-but-unproven/Josef-package after the apply-wielded firestarter boundary failed to open `Light where?`; same-fixture repeatability is not second-fixture anti-bias proof; no-opening proof is not opening-present escalation proof. Detailed run IDs and validation gates live in `SUCCESS.md`, `TESTING.md`, and the matrix doc.

---

## Checkpointed lane - Smart Zone Manager harness-audit retry packet v0

**Status:** CHECKPOINTED / JOSEF PLAYTEST PACKAGE / IMPLEMENTED-BUT-UNPROVEN BOUNDARY REACHED

Josef reopened the Smart Zone proof with a sharper live observable: the Zone Manager list shows relative coordinates beside each zone, such as `2E` for two tiles east of the player. That proof remains desirable, but the bounded agent-side retry budget is now exhausted without honest Zone Manager entry proof.

Imagination source lives at `doc/smart-zone-manager-harness-audit-retry-imagination-source-of-truth-2026-04-28.md`.
Canonical contract lives at `doc/smart-zone-manager-harness-audit-retry-packet-v0-2026-04-28.md`.

Final retry boundary: the current runtime rebuild/probes reached clean startup/load on `5f17cc7901-dirty`, but did not prove Smart Zone generation or Zone Manager coordinate labels. Guarded UI-entry runs stop before any add-zone/filter/generation credit because OCR never sees `Zones manager`. The final action-dispatch instrumentation run `.userdata/smart-zone-ui-entry-current-runtime-20260429c/harness_runs/20260429_005345/` shows the default printable `Y` delivery reaches gameplay as `raw_action="action_menu" action_id="action_menu"`, not `ACTION_ZONES`; no `invoke_zone_manager` trace is emitted and OCR still shows ordinary gameplay/actions UI. Earlier guarded runs `.userdata/smart-zone-audit-live-20260429e/harness_runs/20260429_002148/`, `.userdata/smart-zone-audit-live-20260429f/harness_runs/20260429_004203/`, and `.userdata/smart-zone-ui-entry-current-runtime-20260429b/harness_runs/20260429_005059/` are the same UI-entry/key-delivery blocker class.

Verdict: deterministic Smart Zone geometry remains support only; the live/UI coordinate-label proof is implemented-but-unproven agent-side and is recorded in `/Users/josefhorvath/.openclaw/workspace/runtime/josef-playtest-package.md`. Do not run another Smart Zone harness retry unless Josef/Schani explicitly reopens it with a materially repaired UI-entry/key-delivery primitive.

---

## Completed lane - Generic clean-code boundary review packet v0

**Status:** CLOSED / CHECKPOINTED REPORT-ONLY BOUNDARY REVIEW

The report-only clean-code boundary review reached its boundary: `doc/generic-clean-code-boundary-review-report-v0-2026-04-29.md` exists, the completed Smart Zone checkpoint TODO drift was retired in `0bbe92a368`, and no queued follow-up is currently promoted. Keep the gated `OPENCLAW_HARNESS_UI_TRACE` dispatch hook as queued/watchlist cleanup unless Schani/Josef explicitly asks to remove it. Do not reopen Smart Zone live proof without a materially repaired UI-entry/key-delivery primitive or Josef manual evidence.

Imagination source lives at `doc/generic-clean-code-boundary-review-imagination-source-of-truth-2026-04-28.md`.
Canonical contract lives at `doc/generic-clean-code-boundary-review-packet-v0-2026-04-28.md`.

---

## Held lane - C-AOL harness trust audit + proof-freeze v0

**Status:** HELD / CHECKPOINTED PROCESS FREEZE

Josef requested this proof-standard lane on 2026-04-27 after load-and-close harness runs risked being treated as feature proof. The checkpointed process boundary is now preserved in the proof-freeze docs and `SUCCESS.md`: startup/load is never feature proof; feature proof requires clean startup, green step-local rows, no yellow/blocked wait ledger, and claim-scoped artifacts. Metadata-only/debug primitives remain scoped process evidence unless the product claim itself is proven.

Canonical references:
- contract: `doc/c-aol-harness-trust-audit-and-proof-freeze-packet-v0-2026-04-27.md`
- inventory: `doc/c-aol-harness-trust-audit-inventory-v0-2026-04-27.md`
- proof-freeze matrix: `doc/c-aol-harness-proof-freeze-matrix-v0-2026-04-28.md`

Current product-facing consequences: the normal player-action brazier deploy primitive is green only as a scoped deploy gate; Josef's repaired fuel/writeback path using an oversized in-world wood source plus a broad source-firewood zone is implemented-but-unproven/Josef-package after the final action-boundary failure; Smart Zone live layout remains implemented-but-unproven/Josef-package; roof-fire proof is downstream of real player-lit fire rather than a separate decision latch.

---

## Active lane - C-AOL actual playtest verification stack v0

**Status:** ACTIVE / GREENLIT ACTUAL PLAYTEST STACK

Josef explicitly greenlit actual playtests now, using the same step-by-step metadata/screenshot validation that the harness trust audit is establishing. This is not a release lane and not a license to wander around a save; it is a small ordered verification stack for live product claims that deterministic tests or load-only harness runs could not honestly close.

Canonical contract lives at `doc/c-aol-actual-playtest-verification-stack-v0-2026-04-27.md`.
Bandit live product playtest matrix lives at `doc/bandit-live-product-playtest-matrix-v0-2026-04-28.md`.
Cannibal confidence-push matrix lives at `doc/cannibal-camp-confidence-push-live-playtest-matrix-v0-2026-04-29.md`.

Greenlit order:

1. **Fuel normal-map entry primitive packet v0 — COMPLETE / GREEN NORMAL-MAP ENTRY GATE.** Current-runtime probe `.userdata/dev-harness/harness_runs/20260429_140645/` (`python3 tools/openclaw_harness/startup_harness.py probe bandit.live_world_nearby_camp_source_zone_normal_map_entry_mcw`) is green for the primitive: feature-path classification, 5/5 green step-local ledger, load-finalized artifact match, charged wielded `lighter` with 100 remaining charge, saved `f_brazier` plus real `log` items east of the player, saved `SOURCE_FIREWOOD` zone covering the target/source area, and guarded screenshot `normal_map_entry_gate_before_activation.png` with OCR fallback including `Wield:` and standalone `YOU`. The red raw-JSON/item-info blocker remains preserved for prior bad runs, and no activation/targeting/fire/lighter action key is sent by this primitive. This is not fire proof; it is only the eye/keyboard gate that lets the existing source-zone fire proof resume. Imagination source lives at `doc/fuel-normal-map-entry-primitive-imagination-source-of-truth-2026-04-29.md`; canonical contract lives at `doc/fuel-normal-map-entry-primitive-packet-v0-2026-04-29.md`.
2. **Fuel writeback repair via wood source zone packet v0 — implemented-but-unproven / Josef playtest package, broken old fixture retired.** Josef explicitly diagnosed the old fuel/source-zone proof surface as a broken save/fixture, so `fuel_writeback_source_zone_v0_2026-04-29` must not be used for more fire/lighter proof. A clean replacement fixture now exists: `fuel_source_zone_clean_normal_map_v0_2026-04-29`, captured from `.userdata/dev-harness/harness_runs/20260429_140645/saved_world/McWilliams` after green normal-map entry and before any activation/targeting/fire/lighter key. Clean-gate probe `.userdata/dev-harness/harness_runs/20260429_143149/` (`bandit.live_world_nearby_camp_source_zone_clean_normal_map_entry_mcw`) is feature-path green with 5/5 green step-local ledger, load-finalized artifact match, saved wielded `lighter` charge 100, saved `f_brazier`, real saved `log`, saved `SOURCE_FIREWOOD`, and first screenshot `normal_map_entry_gate_before_activation.png`; OCR fallback matched `Wield:` and `YOU`, with no raw `pocket_type` / `contents` / `specific energy` guard. Prior action diagnostic `.userdata/dev-harness/harness_runs/20260429_142257/` remains non-proof: after normal apply-wielded `A`, `open_start_fire_targeting_prompt.after.png` / OCR did not show `Light where?`, so it aborted as `blocked_apply_wielded_firestarter_did_not_open_targeting`. No inventory-selector coverage, normal ignition, mtime, `fd_fire`, smoke, or bandit signal proof is credited. If fuel proof is reopened, use only the clean fixture or a fresher clean fixture that first proves normal map UI; if a clean fixture opens to raw JSON/item-info soup, classify harness/game-code load bug and stop for review. Manual package entry is `/Users/josefhorvath/.openclaw/workspace/runtime/josef-playtest-package.md#2026-04-29--fuel-writeback-source-zone-repair-v0`. Imagination source lives at `doc/fuel-writeback-repair-via-wood-source-zone-imagination-source-of-truth-2026-04-29.md`; canonical contract lives at `doc/fuel-writeback-repair-via-wood-source-zone-packet-v0-2026-04-29.md`.
3. **Smart Zone Manager harness-audit retry packet v0 — implemented-but-unproven/Josef package.** The bounded current-runtime retry exhausted its agent-side attempt budget at a UI-entry/key-delivery blocker: dispatch trace after default `Y` records `action_menu`, not `zones`, and no `Zones manager` OCR/trace proof exists. Do not rerun unless a materially repaired UI-entry primitive or Josef manual evidence reopens it.
4. **Cannibal camp confidence-push live playtest packet v0 — complete / confidence uplift green.** The compact live matrix now covers wandering day pressure, night mistake/contact, reload brain, different-footing exposed-sight repeat, and bandit contrast control. New current-runtime `e778902cac` evidence: reload/create-save feature proof `.userdata/dev-harness/harness_runs/20260429_021849/`, paired no-fixture reload saved-state support `.userdata/dev-harness/harness_runs/20260429_021929/`, and different-footing exposed-sight repeat `.userdata/dev-harness/harness_runs/20260429_022021/`. Earlier current-runtime rows remain credited for bandit contrast `.userdata/dev-harness/harness_runs/20260429_012915/`, day smoke/pressure `.userdata/dev-harness/harness_runs/20260429_013310/`, and night/contact pack-forward `.userdata/dev-harness/harness_runs/20260429_014900/`. This stays confidence uplift for already-finished behavior, not redesign or a reopened failure. Imagination source lives at `doc/cannibal-camp-confidence-push-live-playtest-imagination-source-of-truth-2026-04-28.md`; canonical contract lives at `doc/cannibal-camp-confidence-push-live-playtest-packet-v0-2026-04-28.md`; completed matrix lives at `doc/cannibal-camp-confidence-push-live-playtest-matrix-v0-2026-04-29.md`.
5. **Player-lit fire and bandit signal verification packet v0 — blocked behind Josef/manual fuel proof.** The fuel slice above did not prove normal ignition or saved `fd_fire`/smoke/light writeback agent-side. Continue only if Josef supplies manual evidence or Schani/Josef/Frau reopen a materially repaired primitive; reopened proof must include visible normal-map/fire-or-no-fire screenshot facts, player message/OCR, save mtime, and saved `fd_fire`/`fd_smoke`, then proceed into bounded wait/time passage and bandit signal response. Do not credit debug map-editor fields or synthetic loaded-map fire as player-lit fire.
6. **Roof-fire horde detection proof packet v0 — greenlit but blocked behind real player-lit fire.** Josef/Schani greenlit this later item: debug may stage the horde/distance, but not the fire. Only after real player-lit fire is green, prove roof/elevated player position, real player-created roof fire/light/smoke, bounded time passage, and horde before/after detection or response metadata; a proven no-detect/no-response outcome is acceptable.
7. **Bandit empty-camp retirement audit-mode packet v0 — deterministic proof green.** The active-site retirement rule now requires the conjunction: no home/inside live members or spawn-tile home headcount and no active dispatch/outbound/local-contact pressure. Tests keep a site active when either side remains populated and retire only the fully empty site; canonical contract lives at `doc/bandit-empty-camp-retirement-audit-mode-packet-v0-2026-04-28.md`.
8. **Bandit camp-map risk/reward dispatch planning packet v0 — complete / scoped live-product green.** See the closed lane above for canonical references and preserved caveats.
9. **Generic clean-code boundary review packet v0 — completed report-only boundary.** Report exists, completed-checkpoint TODO drift was retired, and no queued follow-up is promoted; keep only the dispatch trace hook removal as queue/watchlist cleanup unless Schani/Josef promotes it.
10. **C-AOL live AI performance audit packet v0 — complete / green enough for current playtest scale.** The live performance matrix now has green one-site, two-site, pre-staged three-site, and pre-staged four-site rows through the real bounded-wait live path. Four-site current-runtime evidence: `.userdata/dev-harness/harness_runs/20260429_041936/` (`performance.four_site_prestaged_active_wait_30m`) reached feature-path classification with 7/7 green step ledger, green 30m wait ledger, preflight-proven distinct active non-player-target jobs, and same-run `sites=4 active_sites=4 active_job_mix=camp_style:stalk=1,cannibal_camp:stalk=3 signals=0` perf counters. Natural three/four-site player-pressure dispatch remains a cap/watchlist behavior question, not a performance-load failure. Imagination source lives at `doc/c-aol-live-ai-performance-imagination-source-of-truth-2026-04-28.md`; canonical contract lives at `doc/c-aol-live-ai-performance-audit-packet-v0-2026-04-28.md`; completed matrix lives at `doc/c-aol-live-ai-performance-matrix-v0-2026-04-29.md`.
Non-goals: no Lacapult, no release publishing, no broad debug-note reopening, no blind Smart Zone live-probe loop outside `doc/smart-zone-manager-harness-audit-retry-packet-v0-2026-04-28.md`, no contaminated old McWilliams Smart Zone macro as closure proof, no informal Andi-only nudges as substitutes for canon plan items, and no feature closure from load-and-close or deterministic tests alone.

---

## Recently closed correction checkpoints

**Status:** CHECKPOINTED / CLOSED

Do not reopen these by drift:
- **Bandit live-wiring audit + visible-light horde bridge correction v0** — loaded-map visible fire/light -> horde signal bridge proof, not player-lit fire proof.
- **Bandit local sight-avoid + scout return cadence packet v0** — reclosed for the 2026-04-27 product gap: current-runtime live proof now covers five-OMT hold-off standoff plus scout timeout, home-footprint observation, and returned pressure-refresh writeback on the McWilliams/Basecamp product path.
- **Smart Zone Manager v1 Josef playtest corrections** — implemented-but-unproven live: deterministic geometry/separation proof is green and the clean live/UI macro failure is in Josef's playtest package; do not loop on it again without a fresh reopen or materially repaired harness primitive.
- **Basecamp medical consumable readiness v0** — deterministic camp/locker proof for bounded bandage-family readiness, including carried-stock cap behavior.
- **Basecamp locker armor ranking + blocker removal packet v0** — generic protective/full-body armor comparison and blocker clearing proof; no RM13 special case.

---

## Checkpointed packet index

**Status:** CHECKPOINTED / INDEX

Use the auxiliary docs below when a later discussion needs the canonical contract or the honest closed verdict, not as permission to reopen the lane automatically.

### Camp / Basecamp packets

- `doc/bandit-extortion-playthrough-audit-harness-skill-packet-v0-2026-04-22.md` (checkpointed / done for now)
- `doc/bandit-extortion-at-camp-restage-handoff-packet-v0-2026-04-22.md` (checkpointed / done for now)
- `doc/bandit-aftermath-renegotiation-writeback-packet-v0-2026-04-22.md` (checkpointed / done for now)
- `doc/bandit-shakedown-pay-or-fight-surface-packet-v0-2026-04-22.md` (checkpointed / done for now)
- `doc/bandit-approach-stand-off-attack-gate-packet-v0-2026-04-22.md` (checkpointed / done for now)
- `doc/bandit-live-world-control-playtest-restage-packet-v0-2026-04-22.md` (checkpointed / done for now)
- `doc/bandit-basecamp-playtest-kit-packet-v1-2026-04-22.md` (checkpointed / done for now)
- `doc/bandit-basecamp-playtest-kit-packet-v2-2026-04-22.md` (folded into later active lane / supporting only)
- `doc/bandit-basecamp-playtest-kit-packet-v0-2026-04-22.md`
- `doc/bandit-basecamp-first-pass-encounter-readability-packet-v0-2026-04-22.md`
- `doc/live-bandit-basecamp-playtest-packaging-helper-packet-v0-2026-04-22.md`
- `doc/locker-zone-v3-reopen-packet-v0-2026-04-21.md`
- `doc/basecamp-ai-capability-audit-readout-packet-v0-2026-04-21.md`
- `doc/live-bandit-basecamp-playtesting-feasibility-probe-v0-2026-04-21.md`

### Bandit readiness and follow-through packets

- `doc/bandit-overmap-local-handoff-interaction-packet-v0-2026-04-21.md`
- `doc/bandit-elevated-light-and-z-level-visibility-packet-v0-2026-04-21.md`
- `doc/bandit-overmap-benchmark-suite-packet-v0-2026-04-21.md`
- `doc/bandit-weather-concealment-refinement-packet-v0-2026-04-21.md`
- `doc/bandit-overmap-local-pressure-rewrite-packet-v0-2026-04-21.md`
- `doc/bandit-long-range-directional-light-proof-packet-v0-2026-04-21.md`
- `doc/bandit-bounded-scout-explore-seam-v0-2026-04-21.md`
- `doc/bandit-repeated-site-revisit-behavior-packet-v0-2026-04-21.md`
- `doc/bandit-scoring-refinement-seam-v0-2026-04-21.md`
- `doc/bandit-moving-bounty-memory-seam-v0-2026-04-21.md`

### Earlier bandit substrate docs still worth keeping straight

- `doc/bandit-concept-formalization-followthrough-2026-04-19.md`
- `doc/bandit-player-basecamp-visibility-and-concealment-2026-04-19.md`
- `doc/bandit-overmap-ai-concept-2026-04-19.md`

---

## Parked concept chain - Bandit overmap AI

**Status:** PARKED / COHERENT SUBSTRATE

This larger concept stays parked as a planning substrate.
Several bounded `v0` slices were promoted, implemented, and checkpointed, but that does **not** silently greenlight the remaining broader concept work.

Current parked-chain anchor:
- `doc/bandit-overmap-ai-concept-2026-04-19.md`

Still-parked concept docs:
- `doc/bandit-mark-generation-and-heatmap-model-2026-04-19.md`
- `doc/bandit-overmap-to-bubble-handoff-seam-2026-04-19.md`
- `doc/bandit-player-basecamp-visibility-and-concealment-2026-04-19.md`

Promoted slices that are now checkpointed closed live in the packet index above.
If this chain is revisited later, the next discussion should be about one new bounded promotion or one real contradiction in current canon, not about spawning another fog bank of feeder docs.

---

## Future feature lanes - parked far back

**Status:** PARKED / FAR BACK

These lanes are **not part of the current camp-handling or bandit queue**.
Keep them visibly separate so adjacent tooling or observability work does not get mistaken for partial feature delivery.

- Chat interface over in-game dialogue branches
- Tiny ambient-trigger NPC model
- Arsenic-and-Old-Lace social threat and agentic-world concept bank
  - anchor doc: `doc/arsenic-old-lace-social-threat-and-agentic-world-concept-bank-2026-04-22.md`
  - preserve this as a far-back parked concept bank, not a disguised implementation queue
  - the strongest near-promotable seeds currently include alarm states via natural-language yelling, bandits exploiting readable camp routines, radio information warfare, writhing-stalker pressure, and social camouflage / hidden-psychopath survivor play
  - broader far-out families worth keeping include agentic NPC goals, interdimensional-traveler motives, conspiracy pressure, and other weird-world social-horror systems
  - do **not** let this bank outrun honest playtesting of the current bandit and Basecamp zoning footing; unpack one bounded item at a time only after the present substrate is reviewer-clean enough to deserve more load

Do not reopen them by drift.
They stay buried until Josef explicitly promotes them.

---

## Documentation discipline

If the structure starts bloating again, apply this rule:
- `Plan.md` should be readable in a minute
- `TODO.md` should show only the current execution queue
- `TESTING.md` should show only current policy, latest relevant evidence, and pending probes
- `COMMIT_POLICY.md` should stop the dirty tree from becoming a lifestyle

If a sentence exists only to remember that something used to be true, it probably belongs in git history, not here.
