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
- **doc/work-ledger.md** - compact receipt book for asks, state transitions, evidence links, and owners
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

`doc/work-ledger.md` is now the compact receipt book for meaningful asks, state changes, evidence links, owners, supersessions, held lanes, and red/non-credit proof. Use it before trimming active docs.

Detailed contracts, closure evidence, and older checkpoint history belong in `doc/*.md`, `doc/work-ledger.md`, `SUCCESS.md`, and git history. Keep this file short enough that the active stack is visible without archaeology.

---

## Recent closed lane — CAOL-WRITHING-STALKER-PATTERN-TESTS-v0

**Status:** CLOSED / CHECKPOINTED GREEN V0

Schani promoted the queued writhing-stalker behavior-pattern follow-up into the next active lane after Josef clarified the desired rhythm: fair dread, not hidden-cheat goblin nonsense; repeated attacks with breathing room while healthy; withdrawal once badly injured.

Contract: `doc/writhing-stalker-behavior-pattern-minimap-packet-v0-2026-04-30.md`.

Imagination source: `doc/writhing-stalker-behavior-pattern-imagination-source-of-truth-2026-04-30.md`.

Proof: `doc/writhing-stalker-behavior-pattern-proof-v0-2026-04-30.md`.

Result: `tests/writhing_stalker_test.cpp` now has a compact deterministic behavior-pattern helper and trace rows covering no-magic/no-evidence, weak evidence decay, cover-route preference, light/focus counterplay, zombie-pressure guardrails, vulnerable-player strike windows, cooldown anti-spam, healthy repeated strikes, badly-injured retreat, and jitter/stuckness smell checks. No gameplay tuning/source behavior changed.

Evidence: `make -j4 tests/writhing_stalker_test.o tests LINTJSON=0 ASTYLE=0 && ./tests/cata_test "[writhing_stalker][ai]"` passed (`97 assertions in 8 test cases`); `./tests/cata_test "[writhing_stalker]"` passed (`129 assertions in 10 test cases`); trace extraction for `writhing_stalker_pattern_helper_traces_repeated_strikes_then_injured_retreat` passed and shows `shadow -> strike -> cooldown -> cooldown -> shadow -> strike -> withdraw` with retreat at `hp=50`.

Boundary: deterministic pattern proof is closed for v0. The live seam was not rerun because this slice changed tests only; existing live `monster::plan()` receipts remain the unchanged v0 game-path evidence. Do **not** reopen roof-horde, old writhing-stalker v0 closure, Smart Zone, old fire proof lanes, the multi-camp signal gauntlet, or this pattern packet without explicit Schani/Josef promotion.

---

## Recently closed lane — CAOL-WRITHING-STALKER-LIVE-FUN-SCENARIOS-v0

**Status:** CLOSED / CHECKPOINTED GREEN V0 LIVE FUN-SCENARIO PACKET

Josef explicitly greenlit live writhing-stalker fun scenarios on 2026-04-30: “YESSSS DO IT”. This lane follows the closed deterministic `CAOL-WRITHING-STALKER-PATTERN-TESTS-v0` and asks whether the stalker creates fair dread in live-shaped scenes instead of only passing tidy evaluator traces.

Contract: `doc/writhing-stalker-live-fun-scenarios-packet-v0-2026-04-30.md`.

Imagination source: `doc/writhing-stalker-live-fun-scenarios-imagination-source-of-truth-2026-04-30.md`.

Prior deterministic proof: `doc/writhing-stalker-behavior-pattern-proof-v0-2026-04-30.md`.

Closure proof: `doc/writhing-stalker-live-fun-scenario-proof-v0-2026-04-30.md`.

Green v0 result: staged-but-live McWilliams scenarios prove campfire/light counterplay, alley predator shadow routing, zombie distraction without magic, door/light escape, wounded-predator retreat, repeated strike/cooldown rhythm, and no omniscient target acquisition. Credited runs: `writhing_stalker.live_campfire_counterplay_mcw` -> `.userdata/dev-harness/harness_runs/20260430_233129/`; `writhing_stalker.live_alley_predator_mcw` -> `.userdata/dev-harness/harness_runs/20260430_233156/`; `writhing_stalker.live_zombie_distraction_mcw` -> `.userdata/dev-harness/harness_runs/20260430_233521/`; `writhing_stalker.live_zombie_distraction_no_magic_guard_mcw` -> `.userdata/dev-harness/harness_runs/20260430_233335/`; `writhing_stalker.live_door_light_escape_mcw` -> `.userdata/dev-harness/harness_runs/20260430_233405/`; `writhing_stalker.live_wounded_predator_mcw` -> `.userdata/dev-harness/harness_runs/20260430_233434/`.

Boundary: do **not** reopen monster flavor/stat/spawn footing, deterministic pattern closure, roof-horde, Smart Zone, old fire proof lanes, the multi-camp gauntlet, or these live fun rows without explicit Schani/Josef promotion. Remaining stricter fully-natural/manual discovery is future-only, not a blocker for v0.

Prior closed active lane: `CAOL-WRITHING-STALKER-PATTERN-TESTS-v0` — see `doc/writhing-stalker-behavior-pattern-proof-v0-2026-04-30.md`, `doc/work-ledger.md`, `SUCCESS.md`, and `TESTING.md`.

---

## Last closed active lane — CAOL-ZOMBIE-RIDER-0.3-v0

**Status:** CLOSED / CHECKPOINTED GREEN V0 INITIAL DEV

Josef greenlit preparing release `0.3` zombie rider for Andi, including playtests and a small map-AI funness benchmarking suite. The v0 initial-dev packet is now closed green.

Contract: `doc/zombie-rider-0.3-initial-dev-packet-v0-2026-04-30.md`.

Imagination source: `doc/zombie-rider-0.3-imagination-source-of-truth-2026-04-30.md`.

Raw intake / exact flavor text: `doc/zombie-rider-raw-intake-2026-04-30.md`.

Map-AI / funness benchmark suite: `doc/zombie-rider-map-ai-funness-benchmark-suite-v0-2026-04-30.md`.

Closure readout: `doc/zombie-rider-0.3-closure-readout-v0-2026-05-01.md`.

Result: endpoint zombie rider initial dev is green for v0: exact flavor text preserved, mature-world `GROUP_ZOMBIE` gate at `730 days`, large-body `SMALL_PASSAGE` counterplay, live-consumed local bow/withdraw/cover behavior, bounded overmap light interest and memory decay, capped rider convergence/band pressure, and five staged-but-live funness rows covering open-field terror, wounded disengagement, cover/LOS escape, lit-camp band circle-harass, and matched no-camp-light control.

Evidence: monster/evolution footing `d50715f00e`; local combat AI `4343dbdad1`; overmap light attraction `d2ffbd54c3`; convergence/band checkpoint `ce05ef44ec`; live rows `zombie_rider.live_open_field_pressure_mcw` -> `.userdata/dev-harness/harness_runs/20260501_013055/`, `zombie_rider.live_wounded_disengagement_mcw` -> `.userdata/dev-harness/harness_runs/20260501_014613/`, `zombie_rider.live_cover_escape_mcw` -> `.userdata/dev-harness/harness_runs/20260501_021656/`, `zombie_rider.live_camp_light_band_mcw` -> `.userdata/dev-harness/harness_runs/20260501_030432/`, and `zombie_rider.live_no_camp_light_control_mcw` -> `.userdata/dev-harness/harness_runs/20260501_032016/`.

Boundary: staged-but-live v0 does not claim natural random discovery, full siege/navigation behavior, year-one availability, unavoidable camp deletion, infinite rider accumulation, or broader release packaging. Do not reopen this lane, writhing-stalker lanes, bandit/horde proof lanes, Smart Zone, old fire proof lanes, release packaging, or held parked concepts by drift.

---

## Recently closed lane — CAOL-FLESH-RAPTOR-CIRCLING-SKIRMISHER-v0

**Status:** CLOSED / CHECKPOINTED GREEN V0 / PREDATOR VARIETY PACKAGE

Josef greenlit changing predator behavior variety after the zombie-rider lane, starting with flesh raptors as visible circling skirmishers instead of one-note stab-and-flee annoyance.

Contract: `doc/flesh-raptor-circling-skirmisher-packet-v0-2026-05-01.md`.

Imagination source: `doc/predator-behavior-variety-imagination-source-of-truth-2026-05-01.md`.

Result: flesh raptors now use a raptor-only live `monster::plan()` seam for readable 4–6 tile orbit/flank pressure, under-occupied arc preference, bounded swoop cadence, corridor/blocked-terrain fallback, and jitter guardrails without globally rewriting every `HIT_AND_RUN` monster or increasing equipment-destruction tedium.

Evidence: focused `[flesh_raptor]` tests cover variant footing, orbit/flank scoring, under-occupied arc preference, fallback, cadence/hysteresis, live plan consumption for `mon_spawn_raptor`, and non-raptor `HIT_AND_RUN` preservation. Credited staged-but-live rows are `flesh_raptor.live_open_field_skirmisher_mcw` -> `.userdata/dev-harness/harness_runs/20260501_052709/`, `flesh_raptor.live_crowded_arc_skirmisher_mcw` -> `.userdata/dev-harness/harness_runs/20260501_053414/`, `flesh_raptor.live_blocked_corridor_skirmisher_mcw` -> `.userdata/dev-harness/harness_runs/20260501_054807/`, and `flesh_raptor.live_equipment_frustration_comparison_mcw` -> `.userdata/dev-harness/harness_runs/20260501_062300/`. The final comparison row proves current orbit/swoop/melee debug metrics plus screenshot/OCR player-facing `flesh-raptor` / `impales` / `cut` / `bleeding` evidence from message history.

Closure review: Frau accepted v0 for agent-side close with staged-but-live caveats. Josef taste/playtest is optional/future, not a blocker; if Josef later says the raptor is still annoying, that is a taste/tuning follow-up, not proof that v0 failed.

Boundary: staged-but-live McWilliams rows are not natural random discovery. Equipment-damage tuning was not changed; equipment damage remains an observational frustration metric. Do not reopen this lane, remove every attack-and-retreat enemy, redesign eigenspectres, create a global pack-AI rewrite, or tune raptors by simply raising damage/equipment destruction unless Schani/Josef promote a follow-up.

---

## Recently closed lane — CAOL-WRITHING-STALKER-ZOMBIE-SHADOW-PREDATOR-v0

**Status:** CLOSED / CHECKPOINTED GREEN V0 / PREDATOR VARIETY PACKAGE

Josef greenlit changing both flesh raptors and the writhing stalker away from overused stab-and-flee behavior, with deterministic little-map tests, playtests, and fun metrics. Flesh raptor circling is closed green v0, and the stalker zombie-shadow predator shift is now closed green v0 for the scoped local-evidence package.

Shared imagination source: `doc/predator-behavior-variety-imagination-source-of-truth-2026-05-01.md`.

Contract: `doc/writhing-stalker-zombie-shadow-predator-packet-v0-2026-05-01.md`.

Result: the stalker now has explicit confidence components for evidence/interest, zombie pressure, quiet-side/cutoff opportunity, and counterpressure from light/focus/open exposure. The live shadow destination consumes the quiet-side cutoff scorer, so ordinary zombies pressing one side can make the stalker choose a shadow/cutoff tile where the zombies are not, without granting omniscient target acquisition.

Evidence: focused `[writhing_stalker]` tests prove pressure-side scoring, ambiguous split-pressure restraint, retreat-route cutoff bias, pressure gating, overmap-interest helper admission, light/focus suppression, and preservation of old no-omniscience/cooldown/injured-retreat constraints. `writhing_stalker.live_quiet_side_zombie_pressure_mcw` -> `.userdata/dev-harness/harness_runs/20260501_071548/` proves the local-evidence-only live path from east/front zombie pressure (`pressure_x=3`) to a west/quiet-side cutoff tile (`quiet_x=-1`, `chosen_rel_x=-1`, `chosen_rel_y=-4`) with `overmap_interest=no`; proof note: `doc/writhing-stalker-zombie-shadow-live-quiet-side-proof-v0-2026-05-01.md`. `writhing_stalker.live_escape_side_zombie_retreat_mcw` -> `.userdata/dev-harness/harness_runs/20260501_071940/` proves the local-evidence-only retreat row from north/front zombie pressure (`pressure_y=-3`) to a south/escape-side cutoff tile (`quiet_y=1`, `chosen_rel_y=4`) with `overmap_interest=no`; proof note: `doc/writhing-stalker-zombie-shadow-live-escape-side-proof-v0-2026-05-01.md`.

Closure caveat: v0 proves local-evidence zombie-shadow behavior through staged-but-live McWilliams rows. Live overmap-interest wiring/logging remains unclaimed unless a later packet promotes it; Josef taste/playtest is optional future feedback, not a v0 blocker.

Boundary: do not remove all attack-and-retreat enemies, do not fold eigenspectres into this package, do not make zombie proximity a magical target buff, and do not give the stalker omniscient backstab magic. The anti-redundancy refactor must preserve this approved zombie-shadow behavior shape.

---

## Closed recent lane — CAOL-WRITHING-STALKER-BEHAVIOR-SEAM-REDUCTION-v0

**Status:** CLOSED / CHECKPOINTED GREEN V0 / ANTI-REDUNDANCY PACKAGE

Josef greenlit targeted anti-redundancy packages after Schani's `master...dev` redundancy audit. The first cleanup package was the writhing-stalker behavior seam reduction, preserving the closed green zombie-shadow predator behavior.

Imagination source: `doc/anti-redundancy-packaging-imagination-source-of-truth-2026-05-01.md`.

Contract: `doc/writhing-stalker-behavior-seam-reduction-packet-v0-2026-05-01.md`.

Closure checkpoint: seam inventory is recorded in the contract doc, the live planner seam-consumption test proves `monster::plan()` reaches quiet-side cutoff behavior, and `src/monmove.cpp` now routes writhing stalker / zombie rider / flesh raptor target-response planning through a single named `targeted_live_plan_adapter` dispatch before the generic hostile/flee destination fallback. No behavior-tree or special-attack seam honestly owns this destination-planning response today; product-specific stalker judgment remains explicit in the writhing-stalker evaluator and quiet-side scorer.

Evidence: `git diff --check`; `make -j4 tests/writhing_stalker_test.o tests src/writhing_stalker_ai.o LINTJSON=0 ASTYLE=0 && ./tests/cata_test "[writhing_stalker]"` -> `All tests passed (192 assertions in 15 test cases)`; adapter spillover guard `./tests/cata_test "[zombie_rider],[flesh_raptor]"` -> `All tests passed (231 assertions in 21 test cases)`.

Boundary preserved: this was cleanup/refactor, not tuning. Closed writhing-stalker v0, zombie-shadow proof rows, bandit/horde/camp claims, flesh raptors, and eigenspectres remain closed unless explicitly promoted.

---

## Active lane — CAOL-CAMP-LOCKER-EQUIPMENT-API-REDUCTION-v0

**Status:** ACTIVE / GREENLIT / ANTI-REDUNDANCY PACKAGE / FIRST COVERAGE REFACTOR GREEN

Shared imagination source: `doc/anti-redundancy-packaging-imagination-source-of-truth-2026-05-01.md`.

Contract: `doc/camp-locker-equipment-api-reduction-packet-v0-2026-05-01.md`.

Goal: keep camp locker policy, but trim duplicate item/clothing/ammo/reload/zone ontology in favor of existing item, wearability, body coverage, reload, and zone APIs. Camp-specific policy stays explicit: enabled slots, bulletproof/weather-sensitive preference, readiness supplies, and camp-storage boundaries.

Current checkpoint: first narrow code step replaces camp-locker-local body/sub-body coverage loops with existing item coverage APIs while preserving the camp-specific slot policy. Second narrow code step makes camp locker item gathering respect the existing `NO_NPC_PICKUP` zone API before treating a `CAMP_LOCKER` tile as candidate stock. Focused camp-locker gate is green. Next honest step is to continue the audit/refactor pass on remaining local ontology: classification/scoring edges, carried cleanup, and ammo/magazine readiness.

Boundary: cleanup/refactor only. Do not redesign basecamp missions, Smart Zones, general NPC equipment selection, or outfit tuning by drift.

---

## Greenlit backlog — targeted anti-redundancy packages

**Status:** GREENLIT / BACKLOG / AFTER ACTIVE CAMP-LOCKER LANE

These are queued cleanup/refactor contracts with targeted tests, not current `TODO.md` work.

Shared imagination source: `doc/anti-redundancy-packaging-imagination-source-of-truth-2026-05-01.md`.

1. `CAOL-BANDIT-SIGNAL-ADAPTER-REDUCTION-v0` — keep bandit interpretation custom, but make smoke/light/fire observation visibly adapter-shaped over local fields/time/weather plus the existing horde signal seam. Contract: `doc/bandit-signal-adapter-reduction-packet-v0-2026-05-01.md`.

Boundary: these packages do not reopen closed writhing-stalker, bandit, horde, Smart Zone, or camp claims by drift. The larger bandit live-world roster/mission-state reduction remains future audit territory, not part of this targeted package set.

---

## Held / parked lanes that must not disappear

**Status:** PARKED / HELD VISIBILITY ONLY

These are visible here only because they affect future selection. Do not treat them as active without explicit Schani/Josef promotion.

- `CAOL-GITHUB-RELEASE-v0` — **HELD / NOT PROMOTED**. Normal GitHub-download release work remains held until Schani/Josef explicitly promote it. Contract: `doc/github-normal-download-release-packet-v0-2026-04-25.md`.
- `CAOL-BANDIT-OVERMAP-AI-CONCEPT-v0` — **PARKED / COHERENT SUBSTRATE**. Parent concept: `doc/bandit-overmap-ai-concept-2026-04-19.md`; bounded promoted slices need their own IDs/contracts.
- `CAOL-AOL-SOCIAL-THREAT-BANK-v0` — **PARKED / FAR BACK**. Anchor: `doc/arsenic-old-lace-social-threat-and-agentic-world-concept-bank-2026-04-22.md`. Writhing stalker has now been promoted out of this bank into its own active packet; the rest remains parked.

---

## Recent closed receipts

Use `doc/work-ledger.md` and the linked aux docs for exact evidence, caveats, and supersessions. Do not reopen these by drift.

- `CAOL-ROOF-HORDE-NICE-FIRE-v0` — closed/checkpointed green v0; focused nice roof-fire horde playtest using source player-created roof-fire chain `.userdata/dev-harness/harness_runs/20260429_172847/`, named scenario `bandit.roof_fire_horde_nice_roof_fire_mcw`, and green run `.userdata/dev-harness/harness_runs/20260430_191556/`. Proof reaches live roof-fire horde signal plus saved horde retarget/destination, `last_processed=5267242`, and `moves=8400`; caveats: `tracking_intensity=0` and horde-specific timing `not instrumented`. Proof doc `doc/roof-fire-horde-nice-roof-fire-proof-v0-2026-04-30.md`.
- `CAOL-WRITHING-STALKER-v0` — closed/checkpointed green v0; first playable rare singleton zombie-adjacent predator with deterministic interest/latch/approach/opportunity/withdraw substrate, live monster-plan seam, exposed-retreat proof, shadow/strike/cooldown proof, no-omniscience negative control, and mixed-hostile metrics/tuning readout. Caveat preserved: horde presence/setup is proven in `.userdata/dev-harness/harness_runs/20260430_181748/`, but direct horde movement/retarget cost is not instrumented and is future-only unless explicitly promoted.
- `CAOL-BANDIT-STRUCT-BOUNTY-v0` — closed/checkpointed green v0; closure readout `doc/bandit-structural-bounty-phase-7-closure-readout-2026-04-30.md`; green run `.userdata/dev-harness/harness_runs/20260430_115157/`; non-credit run `.userdata/dev-harness/harness_runs/20260430_114106/`.
- `CAOL-SZM-LIVE-LABEL-v0` — closed green live coordinate-label proof; run `.userdata/smart-zone-audit-live-20260429e/harness_runs/20260429_225644/`.
- `CAOL-CANNIBAL-NIGHT-RAID-v0` — closed cannibal camp night-raid behavior checkpoint; contract `doc/cannibal-camp-night-raid-behavior-packet-v0-2026-04-28.md`.
- `CAOL-CANNIBAL-CONFIDENCE-PUSH-v0` — closed confidence-uplift proof; matrix `doc/cannibal-camp-confidence-push-live-playtest-matrix-v0-2026-04-29.md`.
- `CAOL-BANDIT-CAMP-MAP-RISK-REWARD-v0` — closed scoped live/product checkpoint; matrix `doc/bandit-live-product-playtest-matrix-v0-2026-04-28.md`.
- `CAOL-HARNESS-PROOF-FREEZE-v0` — closed/checkpointed process rule; proof-freeze matrix `doc/c-aol-harness-proof-freeze-matrix-v0-2026-04-28.md`.
- `CAOL-REAL-FIRE-SIGNAL-v0` and `CAOL-ROOF-HORDE-SPLIT-v0` — closed actual-playtest proof bundles for real-fire signal response and split-run roof-fire horde response.
- `CAOL-BANDIT-LIVE-SIGNAL-SITE-BOOTSTRAP-v0` — superseded partial lane; real player-fire signal response is closed by `CAOL-REAL-FIRE-SIGNAL-v0`, while remaining range/decay/scoring questions are future-only unless explicitly promoted.

---

## Documentation discipline

If the structure starts bloating again, apply this rule:
- `Plan.md` should be readable in a minute
- `TODO.md` should show only the current execution queue
- `TESTING.md` should show only current policy, latest relevant evidence, and pending probes
- `COMMIT_POLICY.md` should stop the dirty tree from becoming a lifestyle

If a sentence exists only to remember that something used to be true, it probably belongs in git history, not here.
