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

The active greenlit repo lane is now **Multi-site hostile owner scheduler packet v0**.

Current honest state:
- the authoritative active contract now lives at `doc/multi-site-hostile-owner-scheduler-packet-v0-2026-04-22.md`
- the latest closed lane is now `Bandit live-world control + playtest restage packet v0` at `doc/bandit-live-world-control-playtest-restage-packet-v0-2026-04-22.md`
- that lane now closes honestly on current build: dirty disturbance probe `.userdata/dev-harness/harness_runs/20260423_194416/` resumes from raw local contact, kills the exact scout, and saves the nearby owned site with `headcount = 13`, member `4` as `state = missing`, home spawn-tile `[3371,1230,0]` at `headcount = 0`, and `active_member_ids = [5]` instead of stale-roster reset
- the live-world packet therefore now has owned-site bootstrap, real dispatch, live contact, exact-member writeback, calm same-site re-dispatch, dirty loss/missing follow-through, manual handoff support, and the reviewer-clean perf packet on the honest nearby footing
- the earlier latest closed lane remains `Bandit + Basecamp playtest kit packet v1` at `doc/bandit-basecamp-playtest-kit-packet-v1-2026-04-22.md`
- the earlier `Bandit + Basecamp playtest kit packet v0` remains checkpointed at `doc/bandit-basecamp-playtest-kit-packet-v0-2026-04-22.md`
- the useful harness/helper work from `Bandit + Basecamp playtest kit packet v2` was **not** wasted; it now sits behind the closed live-world lane as bounded support instead of masquerading as the product
- the next honest delivery target is no longer one-site happy-path continuity, but proving `2-3` hostile owners can coexist with independent state, honest save/load, and no accidental coalition brain
- `Locker Zone V3` stays checkpointed closed at `doc/locker-zone-v3-reopen-packet-v0-2026-04-21.md`; this hostile-site stack should reuse current camp footing, not reopen zoning mechanics by drift
- the current bandit playtesting-readiness train stays checkpointed closed for now, including the handoff interaction packet, elevated-light / z-level visibility packet, benchmark suite, weather refinement, pressure rewrite, long-range directional light, bounded scout/explore, scoring refinement, moving-bounty memory, repeated-site revisit follow-through, and the now-closed live-world control lane
- `Bandit z-level visibility proof packet v0` does not come back as a vague side branch; the bounded home for that work was `doc/bandit-elevated-light-and-z-level-visibility-packet-v0-2026-04-21.md`, and that packet stays closed unless new evidence says it lied

This lane is now about proving small independent hostile ownership on top of the newly honest single-site live-world footing.

---

## Active lane - Multi-site hostile owner scheduler packet v0

**Status:** ACTIVE / GREENLIT

Prove the live hostile-owner seam can run a **small set of independent hostile sites** at once instead of one bandit camp plus folklore. Each owner should keep its own anchor/home site, roster, active outing/contact state, remembered pressure, and cadence/writeback state without collapsing into coalition mush.

What this item should do:
- extend the current live owner seam from one-site-at-a-time proof to `2-3` simultaneous hostile sites on one world
- keep owner state independent: anchor, headcount, active outing/contact state, remembered pressure/marks, and persisted writeback
- preserve the current product rule that camps keep a home presence while losses continuously shrink later outings
- damp silly dogpile convergence through existing territoriality / active-pressure footing rather than magical coalition coordination

Non-goals:
- coalition brain or faction grand strategy
- dozens of hostile families at once
- full social-threat/extortion implementation
- whole-world omniscient map ownership dumps

Canonical contract lives at `doc/multi-site-hostile-owner-scheduler-packet-v0-2026-04-22.md`.

---

## Greenlit next

The next greenlit hostile-site stack is now frozen behind `Multi-site hostile owner scheduler packet v0`.
Keep the order explicit and do **not** silently merge these packets together just because they smell related.

### 1. Hostile site profile layer packet v0

**Status:** GREENLIT

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

### 2. Cannibal camp first hostile-profile adopter packet v0

**Status:** GREENLIT

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
If this stack lands cleanly, the stalker should be revisited later as the **first singleton adopter after the profile layer exists**, not crammed into the cannibal-camp proof.

### Then, once that hostile-site stack is honest, the next bandit/basecamp pressure lane should be the full player-present robbery chain.

Radio warfare, `writhing stalker`, and the broader Arsenic-and-Old-Lace social-threat garnish stay parked for later.
The next bandit-specific menu should land in this order.

### 3. Bandit approach / stand-off / attack-gate packet v0

**Status:** GREENLIT

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

### 4. Bandit shakedown pay-or-fight surface packet v0

**Status:** GREENLIT

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

### 5. Bandit aftermath / renegotiation writeback packet v0

**Status:** GREENLIT

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

### 6. Bandit extortion-at-camp restage + handoff packet v0

**Status:** GREENLIT

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

### 7. Bandit extortion playthrough audit + harness-skill packet v0

**Status:** GREENLIT

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

- `doc/bandit-live-world-control-playtest-restage-packet-v0-2026-04-22.md` (active)
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
