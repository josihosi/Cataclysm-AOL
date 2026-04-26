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

The active lane is now **Bandit live signal + site bootstrap correction v0**. Josef greenlit the C-AOL debug-note correction stack on 2026-04-26 and explicitly told Schani to point Andi back at C-AOL, reason deeply, remove hollow code, and map tests to real game implementation before more implementation lands. The audit is now checkpointed at `doc/test-vs-game-implementation-audit-report-2026-04-26.md`; it names this correction as the first implementation target.

Repo policy remains the load-bearing full-history project: `/Users/josefhorvath/Schanigarten/Cataclysm-AOL` on `dev` is the normal worktree, `josihosi/Cataclysm-AOL` is the real project/release repo, and `josihosi/C-AOL-mirror` is only a non-fork full-history contribution-graph mirror with no planning, issue, release, or local-work authority. The earlier standalone snapshot cutover note is historical/superseded; do not reopen destructive GitHub migration, release/tag repair, or repo-role surgery from this debug-correction stack without fresh explicit clearance.

Active scope:
- implement `Bandit live signal + site bootstrap correction v0` from `doc/bandit-live-signal-site-bootstrap-correction-v0-2026-04-26.md`
- preserve the audit finding: smoke/light/weather mark adapters, generated mark ledgers, authored playback frames, and most scout tuning are not live gameplay until live producers and consumers are wired
- register bounded hostile-site ownership without relying solely on the player stepping onto a camp tile and causing NPC placement
- wire real fire/smoke/light observations into bounded live bandit signal candidates under named weather/time conditions
- make live dispatch consume or explicitly reject those candidates with reviewer-readable range/cadence/hold-chill reasons
- keep deterministic tests as adapter regressions, then add source-hook plus live/harness/log proof before claiming gameplay

Non-goals:
- no implementation of every greenlit package inside this correction packet
- no deletion of useful deterministic tests merely because they are deterministic; relabel them honestly instead
- no claiming gameplay behavior until the live producer/consumer path and evidence class are named
- no release publishing, signing, repo-role surgery, Lacapult work, or user-data mutation in this correction lane

Greenlit implementation stack, in current order:
1. **Bandit live signal + site bootstrap correction v0** — ACTIVE; 40 OMT system envelope; real fire/smoke/light/weather source hooks; abstract site bootstrap; lazy materialization; live dispatch/mark consumption.
2. **Bandit live-wiring audit + visible-light horde bridge correction v0** — visible fire/light to real horde signal path, or honest deferral if not implemented.
3. **Bandit local sight-avoid + scout return cadence packet v0** — exposed scouts seek cover/break sight, finite sortie window, return-home/writeback cadence.
4. **Smart Zone Manager v1 Josef playtest corrections** — add `LOOT_MANUALS`, keep book/gun-magazine zone distinction clear, add full-storage `AUTO_EAT` / `AUTO_DRINK` with `ignore_contents=false`.
5. **Basecamp medical consumable readiness v0** — bounded `bandages` / `adhesive_bandages` pickup/preservation from camp storage.
6. **Basecamp locker armor ranking + blocker removal packet v0** — generic full-body/protective armor comparison and blocker clearing, not RM13-specific.
7. **Basecamp job spam debounce + locker/patrol exceptions packet v0** — compress repeated camp-job chatter while preserving typed locker/patrol state changes.

Held but still valid:
- **GitHub normal-download release packet v0** is paused behind Josef's newly greenlit debug-correction stack. Its CI-green footing remains useful, but Andi should not publish a release while the active order is to remove hollow code and test-vs-game gaps first. Canonical release contract remains `doc/github-normal-download-release-packet-v0-2026-04-25.md`.

Current honest state:
- an earlier closed lane is `Bandit approach / stand-off / attack-gate packet v0` at `doc/bandit-approach-stand-off-attack-gate-packet-v0-2026-04-22.md`
- the earlier closed lane is now `Cannibal camp first hostile-profile adopter packet v0` at `doc/cannibal-camp-first-hostile-profile-adopter-packet-v0-2026-04-22.md`
- that lane now closes honestly as substrate/content proof: `src/bandit_live_world.{h,cpp}` carries explicit `cannibal_camp` site/profile ownership, cannibal NPC templates can claim the live-world substrate, dedicated cannibal-camp overmap/mapgen/faction/NPC data gives the profile a real rare anchor, and `tests/bandit_live_world_test.cpp` proves a cannibal camp and bandit camp coexist through dispatch/writeback serialization without shared-state mush
- latest closed correction is `Cannibal camp attack-not-extort correction v0` at `doc/cannibal-camp-attack-not-extort-correction-v0-2026-04-24.md`; cannibal camps now bypass the bandit `open_shakedown` / pay-demand surface and favorable cannibal contact becomes attack-to-kill pressure while bad conditions can still hold off, stalk, probe, or abort
- the earlier closed lane is now `Hostile site profile layer packet v0` at `doc/hostile-site-profile-layer-packet-v0-2026-04-22.md`
- that lane closes honestly on current build: `src/bandit_live_world.{h,cpp}` carries explicit `hostile_site_profile` state/rules, camp-style and small-hostile-site profiles now diverge in reserve capacity, threat/posture bias, return-clock lean, and writeback pressure, and `tests/bandit_live_world_test.cpp` proves a camp and roadblock can dispatch side-by-side on the same substrate without regressing `[bandit][live_world]`
- the earlier closed lane is `Multi-site hostile owner scheduler packet v0` at `doc/multi-site-hostile-owner-scheduler-packet-v0-2026-04-22.md`
- that lane now closes honestly on current build: live probe `.userdata/dev-harness/harness_runs/20260424_003005/` starts from two claimed nearby hostile owners, advances across the `30_minutes` scheduler cadence, and saves two independent active player-pressure records at once: `overmap_special:bandit_camp@140,51,0` with member `4` outbound, and `overmap_special:bandit_camp@140,44,0` with member `18` in `local_contact`, both targeting `player@140,43,0` while keeping separate anchors, rosters, headcounts, remembered marks, and home presence
- the earlier closed lane remains `Bandit live-world control + playtest restage packet v0` at `doc/bandit-live-world-control-playtest-restage-packet-v0-2026-04-22.md`
- that lane closes honestly on current build: dirty disturbance probe `.userdata/dev-harness/harness_runs/20260423_194416/` resumes from raw local contact, kills the exact scout, and saves the nearby owned site with `headcount = 13`, member `4` as `state = missing`, home spawn-tile `[3371,1230,0]` at `headcount = 0`, and `active_member_ids = [5]` instead of stale-roster reset
- the live-world packet therefore has owned-site bootstrap, real dispatch, live contact, exact-member writeback, calm same-site re-dispatch, dirty loss/missing follow-through, manual handoff support, and the reviewer-clean perf packet on the honest nearby footing
- the earlier latest closed lane remains `Bandit + Basecamp playtest kit packet v1` at `doc/bandit-basecamp-playtest-kit-packet-v1-2026-04-22.md`
- the earlier `Bandit + Basecamp playtest kit packet v0` remains checkpointed at `doc/bandit-basecamp-playtest-kit-packet-v0-2026-04-22.md`
- the useful harness/helper work from `Bandit + Basecamp playtest kit packet v2` was **not** wasted; it now sits behind the closed live-world lane as bounded support instead of masquerading as the product
- the latest closed lane is now `Bandit aftermath / renegotiation writeback packet v0` at `doc/bandit-aftermath-renegotiation-writeback-packet-v0-2026-04-22.md`
- that lane closes honestly on current build: shakedown aftermath state now persists paid/fought outcomes, loot, anger, caution, defender-loss reopen, and bandit-loss cooling; deterministic proof covers the raised one-use reopen and `pay`/`fight` reconsideration surface, artificial live continuation `.userdata/dev-harness/harness_runs/20260424_142054/` proves the defender-observer/reopen state can persist, and isolated exact-member copied-save proof `.userdata/dev-harness/harness_runs/20260424_143107/` saves member `4` missing with `last_shakedown_outcome=fight_bandit_loss`, `shakedown_bandit_losses=1`, `shakedown_caution=1`, and collapsed pressure
- the earlier closed lane is now `Bandit shakedown pay-or-fight surface packet v0` at `doc/bandit-shakedown-pay-or-fight-surface-packet-v0-2026-04-22.md`
- that lane closes honestly on current build: the `open_shakedown` branch now opens a blunt player-present robbery demand with explicit `pay` / `fight`, Basecamp-side live fight and pay/writeback proof, and off-base live reach proof `.userdata/dev-harness/harness_runs/20260424_070845/` showing `basecamp_inventory=no`, `vehicle_inventory=yes`, and only carried/current-vehicle goods outside Basecamp/camp footing
- `Locker Zone V3` stays checkpointed closed at `doc/locker-zone-v3-reopen-packet-v0-2026-04-21.md`; this hostile-site stack should reuse current camp footing, not reopen zoning mechanics by drift
- MacOS release packaging now has a bounded portability guardrail: `build-data/osx/bundle_portable_dependencies.sh` runs from the `app` make target and fails future DMG packaging if `cataclysm` / `cataclysm-tiles` still link against local package-manager dylib paths such as the broken `v0.2.0` `/opt/local/lib/libfreetype.6.dylib` and `/opt/local/lib/libz.1.dylib` shape
- the current bandit playtesting-readiness train stays checkpointed closed for now, including the handoff interaction packet, elevated-light / z-level visibility packet, benchmark suite, weather refinement, pressure rewrite, long-range directional light, bounded scout/explore, scoring refinement, moving-bounty memory, repeated-site revisit follow-through, and the now-closed live-world control lane
- `Bandit z-level visibility proof packet v0` does not come back as a vague side branch; the bounded home for that work was `doc/bandit-elevated-light-and-z-level-visibility-packet-v0-2026-04-21.md`, and that packet stays closed unless new evidence says it lied

The extortion-at-camp restage/handoff packet is done-for-now: `bandit.extortion_at_camp_standoff_mcw` provides the named probe/handoff path, attracts the real controlled site `overmap_special:bandit_camp@140,51,0` through live dispatch/local-gate, and handoff run `.userdata/dev-harness/harness_runs/20260424_153309/` leaves the game alive with `cleanup.status = deferred_handoff` at camp-adjacent `hold_off` pressure.

The latest closed lane is now `Bandit extortion playthrough audit + harness-skill packet v0` at `doc/bandit-extortion-playthrough-audit-harness-skill-packet-v0-2026-04-22.md`: named scenario surfaces cover the stand-off setup, first Basecamp demand pay/fight fork, fight-forward message, pay/writeback path, and a controlled defender-loss reopen tier with raised second demand (`22116`) plus `pay` / `fight` still available.  The natural redispatch-from-no-active-group probe stayed artifact-silent after 6000 turns on current runtime, so that natural trigger is not claimed closed; the audit packet closes as a tiered teachable harness path rather than a fake one-button combat opera.

---

## Greenlit follow-up - Smart Zone Manager v1 Josef playtest corrections

**Status:** GREENLIT / QUEUED DEBUG FOLLOW-UP

Canonical contract: `doc/smart-zone-manager-v1-josef-playtest-followup-2026-04-26.md`.

Josef later checked the generated zones in Zone Manager and found the existing zone layout broadly OK. The greenlit correction is now narrow: add `LOOT_MANUALS` on/near the books cluster, keep `LOOT_BOOKS` versus gun `LOOT_MAGAZINES` clear, and add `AUTO_EAT` / `AUTO_DRINK` across the full Basecamp storage zone with the "Ignore items in this area when sorting?" / `ignore_contents` option set to **NO / false** so the Basecamp storage footprint still sorts normally.

## Active correction - Bandit live signal + site bootstrap correction v0

**Status:** ACTIVE / GREENLIT NOW

Canonical contract: `doc/bandit-live-signal-site-bootstrap-correction-v0-2026-04-26.md`.

Josef's 2026-04-26 fire-at-camp test exposed a live-chain break: a remote bandit camp and a real visible fire do not matter if the running save has `bandit_live_world.sites = []`, the dispatcher only sees registered sites, and the live candidate gate hard-stops at `distance <= 10`. This packet freezes the corrected range matrix and packages the fix: the overall overmap AI/system envelope is **40 OMT**, smoke remains about **15 OMT**, ordinary bounty visibility remains around **10 OMT**, confident threat around **6 OMT**, hard/searchlight threat around **8 OMT**, and exceptional elevated light stays adapter-bounded inside the 40 OMT envelope. The implementation should register abstract hostile sites/rosters without player proximity, materialize concrete bandits lazily, wire real fire/smoke/light observations into live marks/leads, split signal observation/decay cadence from dispatch cadence, and add reviewer-readable instrumentation. Do not eagerly spawn every bandit globally, and do not claim deterministic adapter/playback proof is live gameplay until the live source hook exists.

## Greenlit correction - Bandit live-wiring audit + visible-light horde bridge

**Status:** GREENLIT / QUEUED CORRECTION

Canonical contract: `doc/bandit-live-wiring-audit-and-light-horde-bridge-correction-v0-2026-04-26.md`.

Josef caught a real evidence gap: the bandit proof packets implemented light and shared horde-pressure behavior on deterministic mark-generation/playback seams, but the live game does not currently wire visible fire/light into `overmap_buffer.signal_hordes(...)` / `horde_map`. Current live horde attraction remains sound/JSON-effect driven unless a real bridge is added. Do not claim roof/basecamp light attracts overmap hordes in live gameplay until this bridge exists and has live/harness proof.

## Greenlit follow-up - Bandit local sight-avoid + scout return cadence packet v0

**Status:** GREENLIT / QUEUED FOLLOW-UP

Canonical contract: `doc/bandit-local-sight-avoid-and-scout-return-cadence-packet-v0-2026-04-26.md`.

Josef's 2026-04-26 nearby-camp playtest found a credible local-stalking gap: a dispatched scout can stand around in the reality bubble for too long, and the previously discussed creepy behavior where a seen bandit slips back out of player/Basecamp sight is not implemented yet. Queue this as one bounded local-stalking correction: stalking/hold-off bandits should use non-magical current/recent exposure heuristics to move toward cover or broken line of sight, scouts should eventually return home and write back what they learned, and any later larger dispatch must come from explicit camp re-evaluation rather than automatic spawn cheating. Keep it queued behind the active live-signal/site-bootstrap package and higher-priority bandit live-wiring packages unless Schani/Josef explicitly reorders the stack.

## Greenlit follow-up - Basecamp medical consumable readiness v0

**Status:** GREENLIT / QUEUED FOLLOW-UP

Canonical contract: `doc/basecamp-medical-consumable-readiness-v0-2026-04-26.md`.

Josef's locker/bandage report is a separate Basecamp readiness node, not part of the bandit signal chain. Current locker service keeps or equips loadout-style items and preserves some already-carried bandages, but it does not fetch fresh `bandages` / `adhesive_bandages` from `CAMP_LOCKER` as medical readiness stock. Queue this as one bounded medical-consumable slice: recognize obvious first-aid consumables, let NPCs take a small sensible reserve, preserve existing carried medical supplies, and prove the behavior without turning the camp locker into a full pharmacy/autodoc system.

## Greenlit follow-up - Basecamp locker armor ranking + blocker removal packet v0

**Status:** GREENLIT / QUEUED FOLLOW-UP

Canonical contract: `doc/basecamp-locker-armor-ranking-blocker-removal-packet-v0-2026-04-26.md`.

Josef confirmed the RM13/jeans spam is only one symptom of a generic locker policy gap: the camp locker must evaluate whether a candidate full-body or high-coverage protective item is better than currently worn blockers using an explainable metric, then remove/drop the blocking items when the candidate clearly wins. This must not be RM13-specific. The comparison should consider body-part priority, protection/coverage, encumbrance, condition, and active locker policy, preserving stronger current armor against worse candidates while stopping repeated failed equip/wear-message spam.

## Greenlit follow-up - Basecamp job spam debounce + locker/patrol exceptions packet v0

**Status:** GREENLIT / QUEUED FOLLOW-UP

Canonical contract: `doc/basecamp-job-spam-debounce-exceptions-packet-v0-2026-04-26.md`.

Josef greenlit a bounded spam fix for repeated Basecamp job chatter: repeated completion, missing-tool, or no-progress messages should debounce by stable cause so the message log stays readable. Locker-zone and patrol-zone work need typed exceptions: real gear/readiness failures, guard assignment changes, interruptions, or reserve/backfill changes should still surface once with reason, while routine repeats are compressed.

## Recently closed lane - Hostile site profile layer packet v0

**Status:** CLOSED / CHECKPOINTED

Once multiple hostile owners can coexist, generalize that scheduler into one bounded hostile-site substrate with explicit profiles so camp-style and non-camp hostile sites can share the machinery without becoming the same creature in a fake moustache.

What this item should do:
- define one shared hostile owner/cadence/persistence substrate with explicit per-profile rules
- support at minimum a camp-style profile and a smaller hostile-site profile without hardcoding everything to current bandit-camp assumptions
- keep dispatch, threat posture, return-clock, and writeback differences profile-driven and reviewer-readable
- preserve the same bounded live-world/writeback contract instead of reopening broad hostile-human architecture arguments

Non-goals:
- a giant generic faction-AI framework
- writhing-stalker singleton behavior in this same packet
- broad diplomacy/social-horror systems
- magical stealth or sight-avoidance perfection

Canonical contract lives at `doc/hostile-site-profile-layer-packet-v0-2026-04-22.md`.

---

## Recently closed lane - Cannibal camp first hostile-profile adopter packet v0

**Status:** CLOSED / CHECKPOINTED

This is the next greenlit hostile-site stack after the now-closed `Hostile site profile layer packet v0`.
Keep the order explicit and do **not** silently merge this packet with later stalker or social-threat work just because they smell related.

Use cannibal camp as the first non-bandit adopter of the shared hostile-site profile layer so the new substrate proves itself on a second hostile family **without** jumping straight into singleton stalker weirdness.

What this item should do:
- land one honest cannibal-camp hostile profile on top of the shared hostile-site substrate
- keep its cadence, roster, pressure, dispatch, and writeback rules explicit rather than smuggled through bandit names
- coexist with bandit-owned sites without coalition nonsense or silent bandit-specific assumptions
- if the current tree lacks a real cannibal-camp site family, first add one **rare dedicated cannibal-camp mapgen/spawn anchor** instead of pretending the profile can attach to vapor
- the intended first anchor may derive from current bandit-camp footing, but it should read as a bloodier cannibal variant with explicit names/loadouts/theme dressing rather than a runtime random bandit-camp mutation

Non-goals:
- writhing-stalker singleton behavior in the same packet
- broad hidden-psychopath / social-camouflage systems
- faction politics or giant cannibal lore expansion
- widening the packet into every hostile-human family at once
- silent default runtime conversion of ordinary bandit camps into cannibal camps

Canonical contract lives at `doc/cannibal-camp-first-hostile-profile-adopter-packet-v0-2026-04-22.md`.

`Writhing stalker` stays parked one step longer.
If this stack needs future work, keep it as a new explicit packet instead of silently reopening v0.

---

## Latest closed correction - Cannibal camp attack-not-extort correction v0

**Status:** CLOSED / CHECKPOINTED

Josef clarified the intended cannibal behavior: cannibals do **not** extort; they attack to kill.  The closed cannibal adopter packet proves the profile/content substrate, but the local encounter surface must now diverge from bandit robbery instead of allowing cannibal outings to reuse the `open_shakedown` / pay-demand surface.

What this item should do:
- make `cannibal_camp` local pressure bypass the bandit shakedown/extortion surface
- bias favorable cannibal local contact toward attack-to-kill / lethal ambush behavior rather than pay-or-fight robbery
- still allow hold-off, stalk, probe, or abort when the local gate says conditions are bad
- prove bandit camps can still extort while cannibal camps do not

Non-goals:
- broad cannibal lore, diplomacy, capture, or cooking-the-player systems
- rewriting bandit shakedown behavior
- reopening the already-closed cannibal-camp anchor/profile proof

Canonical contract lives at `doc/cannibal-camp-attack-not-extort-correction-v0-2026-04-24.md`.

Current tree closes this correction honestly: `choose_local_gate_posture(...)` now routes `cannibal_camp` favorable contact to `attack_now` / `combat_forward` instead of `open_shakedown`, the local-gate report names `profile=cannibal_camp`, and `build_shakedown_surface(...)` has a cannibal-profile guard against accidental robbery-surface reuse.  Deterministic proof in `tests/bandit_live_world_test.cpp` covers cannibal no-extort / attack, cautious probe, exposed hold-off, overwhelming abort, and unchanged bandit shakedown/pay/fight behavior.

---

## Recently closed lane - Bandit approach / stand-off / attack-gate packet v0

**Status:** CLOSED / CHECKPOINTED

Before bandits can rob the player or Basecamp honestly, they need one explicit local pressure law that decides whether a real dispatched group stalks, holds off, probes, opens a shakedown, attacks directly, or aborts.
The point is to stop the robbery chain from feeling psychic, abrupt, or decorative.

What this item should do:
- land one bounded local approach / stand-off / attack-gate layer on top of the live bandit control seam
- make the local posture explicit and reviewer-readable, using a small set such as `stalk`, `hold_off`, `probe`, `open_shakedown`, `attack_now`, or `abort`
- base that gate primarily on dispatch strength versus local threat/opportunity rather than folklore aggression
- preserve readable stand-off pressure around Basecamp/player scenes without magical omniscience or instant tile collapse
- allow convoy / vehicle / rolling-travel contexts to skip the polite shakedown posture when they honestly read as moving ambush opportunities

Non-goals:
- the pay-or-fight trading window itself
- loot valuation or exact surrender-pool law
- giant stealth doctrine / magical perfect sight avoidance
- radio warfare, stalker pressure, or broader social-horror systems
- full local combat AI rewrite

Canonical contract lives at `doc/bandit-approach-stand-off-attack-gate-packet-v0-2026-04-22.md`.

## Recently closed lane - Bandit shakedown pay-or-fight surface packet v0

**Status:** CLOSED / CHECKPOINTED

Once the approach law can honestly decide that a scene is a shakedown instead of a hot ambush, make the actual player-present robbery surface real: bandits try to rob you, you can pay, or you can fight.
Keep it hard, readable, and bounded.

What this item should do:
- land one honest bootstrap from the approach/gate layer into a player-present shakedown scene
- start the encounter with a short readable stick-up bark/intent surface instead of a generic hostile aggro flip
- expose one deliberately blunt first fork: `pay` or `fight`, and keep that combat option available whenever this shakedown surface is invoked
- use a trading-style surrender surface with a painful demand drawn from the honest reachable goods pool
- let Basecamp-side scenes expose player + relevant nearby NPC + reachable Basecamp inventory, while off-base scenes expose only currently carried / vehicle-carried goods
- allow surrendered goods to collapse into abstract bandit bounty/writeback rather than requiring perfect long-tail cargo theater

Non-goals:
- branching diplomacy theater or a fake debt economy
- magical remote inventory access outside the honest current scene
- convoy contexts that should honestly just open with violence
- radio warfare, stalker pressure, hostage simulation, or reputation opera

Canonical contract lives at `doc/bandit-shakedown-pay-or-fight-surface-packet-v0-2026-04-22.md`.

## Recently closed lane - Bandit aftermath / renegotiation writeback packet v0

**Status:** CLOSED / CHECKPOINTED

A robbery scene is fake if the world forgets it immediately.
This packet makes the outcome stick: losses, wounds, taken goods, anger, caution, and the specific wanted beat where a killed Basecamp defender can let bandits reopen from a stronger position with a higher demand.

What this item should do:
- land one honest aftermath/writeback layer for player-present bandit shakedown scenes
- write back losses, wounds, loot, failed extraction, anger, and caution into later bandit and target state
- let partial or interrupted fights rewrite the next pressure pass instead of forcing every scene to resolve as one terminal event
- allow one bounded renegotiation reopen when defender strength materially drops, especially if a Basecamp NPC dies and the bandits now read the scene as richer/safer prey
- make that reopened demand explicitly harsher rather than replaying the same toll as if nothing changed
- give the player one real reconsideration fork under the raised demand: pay the higher price or fight again; combat should remain an option both times
- preserve both directions of truth: bandit losses or panic should also cool later pressure instead of only ratcheting cruelty upward

Non-goals:
- infinite renegotiation loops or endless haggling AI
- broad diplomacy/reputation/morale opera
- multi-camp retaliatory coalition logic
- radio warfare, stalker pressure, or broader social-horror systems

Canonical contract lives at `doc/bandit-aftermath-renegotiation-writeback-packet-v0-2026-04-22.md`.

## Recently closed lane - Bandit extortion-at-camp restage + handoff packet v0

**Status:** CLOSED / CHECKPOINTED

Once the robbery chain is real, the harness should be able to attract a real controlled bandit group toward Basecamp and leave the scene alive at the interesting point instead of making Andi rediscover a one-off ritual every time.
This packet is the bounded setup seam for that.

What this item should do:
- land one named restage/handoff packet for the Basecamp extortion chain
- let the harness intentionally attract a **real controlled bandit group** toward current Basecamp/player footing through the live owner/dispatch path
- provide one reviewer probe/capture command and one manual handoff command for that same path
- leave the handoff session alive at a useful extortion-at-camp moment such as approach, stand-off, or opening shakedown state
- keep the setup tied to honest current world footing rather than moved-player/basecamp hacks that break `game::validate_camps()`
- keep the reports explicit enough that review can tell which setup mode ran and what scene state was reached

Non-goals:
- generic scenario/world-authoring empire
- fake debug-spawn shortcuts that bypass the real controlled-bandit path
- helper polish masquerading as the robbery chain itself
- radio warfare, stalker pressure, or broader social-horror feature work

Canonical contract lives at `doc/bandit-extortion-at-camp-restage-handoff-packet-v0-2026-04-22.md`.

## Recently closed lane - Bandit extortion playthrough audit + harness-skill packet v0

**Status:** CLOSED / CHECKPOINTED

Even with a real extortion scene and a real restage point, the job stays half-baked if only one operator remembers the magic sequence.
This packet packages the full playthrough surface and teaches the harness/skill about it so Andi can deliberately run the whole chain.

What this item should do:
- land one named audit/playthrough packet for the full Basecamp extortion chain
- cover at minimum: first demand, `pay`/`fight`, one fight-forward branch, one defender-loss reopen with higher demand, and the second `pay`/`fight` reconsideration fork
- keep screen proof, deterministic proof, and artifact/report proof clearly separated instead of flattening them into one soup verdict
- update the usable harness-facing docs/skill so later agents can discover and run the path without archaeological guessing
- keep the packet focused on making the extortion chain playable/testable/teachable, not on reopening earlier feature-design arguments

Non-goals:
- inventing new robbery mechanics by stealth while packaging the audit
- giant fully scripted automation that fakes every combat outcome
- operator-only secret macro lore
- radio warfare, stalker pressure, or broader social-horror packaging

Canonical contract lives at `doc/bandit-extortion-playthrough-audit-harness-skill-packet-v0-2026-04-22.md`.
---

## Checkpointed packet index

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
