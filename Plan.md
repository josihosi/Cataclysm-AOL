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

The active greenlit repo lane is now **Bandit live-world control + playtest restage packet v0**.

Current honest state:
- the authoritative active contract now lives at `doc/bandit-live-world-control-playtest-restage-packet-v0-2026-04-22.md`
- the latest closed lane is still `Bandit + Basecamp playtest kit packet v1` at `doc/bandit-basecamp-playtest-kit-packet-v1-2026-04-22.md`
- the earlier `Bandit + Basecamp playtest kit packet v0` remains checkpointed at `doc/bandit-basecamp-playtest-kit-packet-v0-2026-04-22.md`
- the open useful harness/helper work from `Bandit + Basecamp playtest kit packet v2` is **not** killed; it is folded forward into the new active lane as supporting tooling instead of pretending helper breadth alone is the product
- the first live ownership substrate is now real: `src/bandit_live_world.{h,cpp}` plus `overmap_global_state.bandit_live_world` claim tracked `bandit_camp` / `bandit_work_camp` / `bandit_cabin` / `mx_looters` / `mx_bandits_block` spawns at `map::place_npc` time and persist explicit site/member/spawn-tile state through save/load
- the first bounded world-side control seam is now also real: `overmap_npc_move()` can ask `bandit_live_world::plan_site_dispatch(...)` / `apply_dispatch_plan(...)` for a nearby owned site, mark selected real members outbound, and hand those actual NPCs a normal overmap travel route toward the nearby player target instead of leaving them as disconnected `place_npc` islands
- that dispatch seam now derives outgoing capacity from the live remaining roster instead of folklore counts: site-backed camps keep one member home by rule, micro-sites can still commit their full tiny roster, and member-state writeback can shrink site/spawn-tile headcount for later outings
- the real missing value has therefore narrowed again: current live bandit content now has one owner/headcount ledger plus one population-coupled first dispatch seam, but the system still does not yet restage, survive messy local contact, write back aftermath, and report perf end to end
- this lane therefore now has to tighten population-coupled outings / writeback, add one bounded near-player restage seam, and produce one reviewer-clean perf readout on the same setup
- the restage seam must support two honest modes: reviewer probe/capture may clean up afterwards, but manual playtest handoff must leave the game/session running instead of auto-terminating as soon as the setup gets interesting
- the already-landed `save_transforms` / `player_mutations` helper surface and named hostile-contact preset from the old `v2` lane remain useful substrate here rather than wasted work
- `Locker Zone V3` stays checkpointed closed at `doc/locker-zone-v3-reopen-packet-v0-2026-04-21.md`; this live packet should reuse current camp footing, not reopen zoning mechanics by drift
- the current bandit playtesting-readiness train stays checkpointed closed for now, including the handoff interaction packet, elevated-light / z-level visibility packet, benchmark suite, weather refinement, pressure rewrite, long-range directional light, bounded scout/explore, scoring refinement, moving-bounty memory, and repeated-site revisit follow-through
- `Bandit z-level visibility proof packet v0` does not come back as a vague side branch; the bounded home for that work was `doc/bandit-elevated-light-and-z-level-visibility-packet-v0-2026-04-21.md`, and that packet stays closed unless new evidence says it lied

This lane is now about making the live bandit system exist in the actual game and making it restageable enough to playtest nearby and measure honestly.

---

## Active lane - Bandit live-world control + playtest restage packet v0

**Status:** ACTIVE / GREENLIT

Connect the real current bandit spawn families tied to this lane to one live saveable owner/control path, track explicit per-site and per-spawn-tile bandit headcounts, and make the resulting setup deliberately restageable near the player/basecamp so Schani and Josef can actually playtest the real system and measure performance on it.

Dispatch pressure should derive from the site's live remaining population, so losses shrink later outings and site-backed camps do **not** send the whole bloody camp off on adventures at once.

The restage seam is part of the product, not a side garnish: it must support reviewer capture/probe work **and** a manual playtest handoff mode that leaves the session alive after setup instead of auto-terminating it.
Useful open helper work from `Bandit + Basecamp playtest kit packet v2` survives here as supporting tooling, but helper breadth alone no longer counts as the headline product.

Canonical contract lives at `doc/bandit-live-world-control-playtest-restage-packet-v0-2026-04-22.md`.

---

## Greenlit next

No later greenlit repo lane is queued behind `Bandit live-world control + playtest restage packet v0` right now.
If this packet closes before new canon lands, prepare a short parked-options packet instead of drifting into speculative hostile-human empire.
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
